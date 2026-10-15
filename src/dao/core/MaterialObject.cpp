// dao/core/MaterialObject.h: a .mao's elements read one tag at a time. The files are the Material
// Editor's own output, flat and regular, so this reads tags and their quoted attributes rather
// than XML in general: no entities, no CDATA, and a comment only as the exporter's dc:source.
#include "dao/core/MaterialObject.h"

#include <algorithm>
#include <array>

namespace dao::core::mao
{

namespace
{

// An attribute's value in a tag's text: `key="value"`, the key whole.
std::optional<std::string_view> Attribute(std::string_view tag, std::string_view key)
{
    for (std::size_t at = tag.find(key); at != std::string_view::npos; at = tag.find(key, at + 1))
    {
        const bool whole =
            at > 0 && (tag[at - 1] == ' ' || tag[at - 1] == '\t' || tag[at - 1] == '\r' || tag[at - 1] == '\n');
        const std::size_t quote = at + key.size();
        if (whole && tag.substr(quote, 2) == "=\"")
        {
            const std::size_t close = tag.find('"', quote + 2);
            if (close != std::string_view::npos)
            {
                return tag.substr(quote + 2, close - quote - 2);
            }
        }
    }
    return std::nullopt;
}

// The element's name: a tag's text up to the first space.
std::string_view ElementOf(std::string_view tag)
{
    return tag.substr(0, tag.find_first_of(" \t\r\n/"));
}

std::string Source(std::string_view text)
{
    constexpr std::string_view kSource = "dc:source=file:///";
    const std::size_t start = text.find(kSource);
    if (start == std::string_view::npos)
    {
        return {};
    }
    const std::size_t from = start + kSource.size();
    const std::size_t comment = text.find("-->", from);
    std::string_view path = text.substr(from, comment == std::string_view::npos ? 0 : comment - from);
    while (!path.empty() && (path.back() == ' ' || path.back() == '\t'))
    {
        path.remove_suffix(1);
    }
    return std::string(path);
}

// One element's tag read into the material object.
void Take(std::string_view tag, MaterialObject &out)
{
    constexpr std::array<std::string_view, 4> kNotParameters{"MaterialObject", "Material", "DefaultSemantic",
                                                             "Texture"};
    const std::string_view element = ElementOf(tag);
    const auto name = Attribute(tag, "Name");
    if (element == "MaterialObject")
    {
        out.name = std::string(name.value_or(""));
    }
    else if (element == "Material")
    {
        out.material = std::string(name.value_or(""));
    }
    else if (element == "DefaultSemantic")
    {
        out.defaultSemantic = std::string(name.value_or(""));
    }
    else if (element == "Texture")
    {
        out.textures.push_back({std::string(name.value_or("")), std::string(Attribute(tag, "ResName").value_or(""))});
    }
    else if (const auto value = Attribute(tag, "value");
             name && value && std::ranges::find(kNotParameters, element) == kNotParameters.end())
    {
        out.parameters.push_back({std::string(element), std::string(*name), std::string(*value)});
    }
}

} // namespace

const Texture *MaterialObject::TextureIn(std::string_view slot) const
{
    const auto found = std::ranges::find(textures, slot, &Texture::slot);
    return found != textures.end() ? &*found : nullptr;
}

const Parameter *MaterialObject::ParameterNamed(std::string_view wanted) const
{
    const auto found = std::ranges::find(parameters, wanted, &Parameter::name);
    return found != parameters.end() ? &*found : nullptr;
}

std::optional<MaterialObject> Read(std::string_view text)
{
    constexpr std::string_view kClose = "</MaterialObject>";
    const std::size_t end = text.rfind(kClose);
    if (end == std::string_view::npos)
    {
        return std::nullopt;
    }
    MaterialObject out;
    out.end = end;
    out.source = Source(text);
    bool seen = false;
    for (std::size_t open = text.find('<'); open != std::string_view::npos && open < end;
         open = text.find('<', open + 1))
    {
        const std::string_view rest = text.substr(open + 1);
        if (rest.starts_with("!--"))
        {
            const std::size_t comment = text.find("-->", open);
            if (comment == std::string_view::npos)
            {
                break;
            }
            open = comment;
            continue;
        }
        const std::size_t close = text.find('>', open);
        if (close == std::string_view::npos)
        {
            break;
        }
        const std::string_view tag = text.substr(open + 1, close - open - 1);
        if (!tag.starts_with('/') && !tag.starts_with('?'))
        {
            seen = seen || ElementOf(tag) == "MaterialObject";
            Take(tag, out);
        }
        open = close;
    }
    return seen ? std::optional(std::move(out)) : std::nullopt;
}

} // namespace dao::core::mao
