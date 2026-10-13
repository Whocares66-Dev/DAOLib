// Notes on AddIns.xml, read 2026-10-12 on the GOG install's: an <AddInItem>
// an add-in, its attributes UID (in lower case its folder under Addins\),
// Name, ExtendedModuleUID (the campaign it extends: "Single Player",
// "DAO_PRC_EP_1" for Awakening's, or none), Priority, Enabled, State, Format,
// BioWare, RequiresAuthorization, then a <Title> whose DefaultText is its
// English name; 39 items on the Ultimate Edition. offers.xml is the same with
// <OfferItem> and no Enabled. Neither is parsed as XML in full: the item
// tags' attributes and the title's are what is read, with XML's five
// entities. Written for Eluvian's ghilana and moved here (DAOLib #106).
#include "dao/core/AddIns.h"

#include <charconv>
#include <utility>

namespace dao::core
{

namespace
{

std::string Unescape(std::string_view text)
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

// The value of `name="..."` in a tag's text; nothing if it has none.
std::optional<std::string> Attribute(std::string_view tag, std::string_view name)
{
    std::size_t at = 0;
    while ((at = tag.find(name, at)) != std::string_view::npos)
    {
        const bool starts =
            at > 0 && (tag[at - 1] == ' ' || tag[at - 1] == '\t' || tag[at - 1] == '\n' || tag[at - 1] == '\r');
        std::size_t p = at + name.size();
        while (p < tag.size() && tag[p] == ' ')
        {
            ++p;
        }
        if (starts && p < tag.size() && tag[p] == '=')
        {
            ++p;
            while (p < tag.size() && tag[p] == ' ')
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

std::optional<int> Number(const std::optional<std::string> &text)
{
    if (!text)
    {
        return std::nullopt;
    }
    int value = 0;
    const auto *end = text->data() + text->size();
    const auto [at, error] = std::from_chars(text->data(), end, value);
    if (error != std::errc() || at != end)
    {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::vector<AddIn> ReadAddIns(std::string_view xml, std::string_view tag)
{
    std::vector<AddIn> items;
    const std::string open = "<" + std::string(tag);
    std::size_t at = 0;
    while ((at = xml.find(open, at)) != std::string_view::npos)
    {
        const std::size_t after = at + open.size();
        if (after >= xml.size() ||
            (xml[after] != ' ' && xml[after] != '>' && xml[after] != '\t' && xml[after] != '\r' && xml[after] != '\n'))
        {
            at = after;
            continue;
        }
        const auto close = xml.find('>', after);
        if (close == std::string_view::npos)
        {
            break;
        }
        const std::string_view head = xml.substr(at, close - at);
        AddIn item;
        item.uid = Attribute(head, "UID").value_or("");
        item.name = Attribute(head, "Name").value_or("");
        item.extends = Attribute(head, "ExtendedModuleUID").value_or("");
        item.priority = Number(Attribute(head, "Priority"));
        item.state = Number(Attribute(head, "State"));
        if (const auto enabled = Number(Attribute(head, "Enabled")))
        {
            item.enabled = *enabled != 0;
        }
        item.bioware = Number(Attribute(head, "BioWare")).value_or(0) != 0;
        // Its title: the first <Title ...> before the item ends.
        const auto end = xml.find("</" + std::string(tag), close);
        const auto title = xml.find("<Title", close);
        if (title != std::string_view::npos && (end == std::string_view::npos || title < end))
        {
            const auto titleClose = xml.find('>', title);
            if (titleClose != std::string_view::npos)
            {
                item.title = Attribute(xml.substr(title, titleClose - title), "DefaultText").value_or("");
            }
        }
        if (!item.uid.empty())
        {
            items.push_back(std::move(item));
        }
        at = close;
    }
    return items;
}

} // namespace dao::core
