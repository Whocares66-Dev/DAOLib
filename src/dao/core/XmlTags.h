// The game's XML files read a tag at a time: AddIns.xml and offers.xml
// (dao/core/AddIns.h), material objects (dao/core/MaterialObject.h) and
// material definitions (dao/core/MaterialDefinition.h). They are tools'
// output, flat and regular, so this reads tags and their quoted attributes
// rather than XML in general: XML's five entities in a value, no CDATA, a
// comment skipped whole even when it holds tags (the definitions keep old
// passes so, 2026-10-17).
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace dao::core::xml
{

// Returns text with XML's five entities replaced by their letters.
inline std::string Unescape(std::string_view text)
{
    static constexpr std::pair<std::string_view, char> kEntities[] = {
        {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
    std::string out;
    for (std::size_t i = 0; i < text.size();)
    {
        bool replaced = false;
        if (text[i] == '&')
        {
            for (const auto &[entity, letter] : kEntities)
            {
                if (text.substr(i).starts_with(entity))
                {
                    out += letter;
                    i += entity.size();
                    replaced = true;
                    break;
                }
            }
        }
        if (!replaced)
        {
            out += text[i++];
        }
    }
    return out;
}

inline bool Space(char letter)
{
    return letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r';
}

// Returns the value of `name="..."` in a tag's text, the name whole, spaces
// allowed about the `=` and either quote; nothing if it has none.
inline std::optional<std::string> Attribute(std::string_view tag, std::string_view name)
{
    std::size_t at = 0;
    while ((at = tag.find(name, at)) != std::string_view::npos)
    {
        const bool starts = at > 0 && Space(tag[at - 1]);
        std::size_t p = at + name.size();
        while (p < tag.size() && Space(tag[p]))
        {
            ++p;
        }
        if (starts && p < tag.size() && tag[p] == '=')
        {
            ++p;
            while (p < tag.size() && Space(tag[p]))
            {
                ++p;
            }
            if (p < tag.size() && (tag[p] == '"' || tag[p] == '\''))
            {
                const char quote = tag[p];
                const auto end = tag.find(quote, p + 1);
                if (end != std::string_view::npos)
                {
                    return Unescape(tag.substr(p + 1, end - p - 1));
                }
            }
        }
        at += name.size();
    }
    return std::nullopt;
}

// Returns a tag's element: its text up to the first space or slash.
inline std::string_view ElementOf(std::string_view tag)
{
    return tag.substr(0, tag.find_first_of(" \t\r\n/"));
}

// Calls take(tag) with each tag's text between its < and >, in order: an
// element's opening (ending in '/' if it closes itself) or its closing
// (starting with '/'). Comments, <?...?> and <!...> are left out. Stops at a
// tag with no end.
template <class Take> void ForEachTag(std::string_view text, Take &&take)
{
    for (std::size_t open = text.find('<'); open != std::string_view::npos; open = text.find('<', open + 1))
    {
        const std::string_view rest = text.substr(open + 1);
        if (rest.starts_with("!--"))
        {
            const std::size_t comment = text.find("-->", open);
            if (comment == std::string_view::npos)
            {
                return;
            }
            open = comment;
            continue;
        }
        const std::size_t close = text.find('>', open);
        if (close == std::string_view::npos)
        {
            return;
        }
        const std::string_view tag = text.substr(open + 1, close - open - 1);
        if (!tag.starts_with('?') && !tag.starts_with('!'))
        {
            take(tag);
        }
        open = close;
    }
}

} // namespace dao::core::xml
