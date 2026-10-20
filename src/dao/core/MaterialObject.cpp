// dao/core/MaterialObject.h: a .mao's elements read one tag at a time (dao/core/XmlTags.h), and
// the exporter's dc:source comment.
#include "dao/core/MaterialObject.h"

#include "dao/core/XmlTags.h"

#include <algorithm>
#include <array>

namespace dao::core::mao
{

namespace
{

using xml::Attribute;
using xml::ElementOf;

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

// Whether two element names are the same, ASCII letters' case aside: Clouds.mao and
// CloudMaterial.mao write <texture>, and the game draws their clouds with those textures
// (DAOLib #209, from DAOParagonEngine's level reader and elgara-agent's captures).
bool Is(std::string_view element, std::string_view name)
{
    return std::ranges::equal(element, name, [](char a, char b) {
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
        return lower(a) == lower(b);
    });
}

// One element's tag read into the material object.
void Take(std::string_view tag, MaterialObject &out)
{
    constexpr std::array<std::string_view, 4> kNotParameters{"MaterialObject", "Material", "DefaultSemantic",
                                                             "Texture"};
    const std::string_view element = ElementOf(tag);
    const auto name = Attribute(tag, "Name");
    if (Is(element, "MaterialObject"))
    {
        out.name = name.value_or("");
    }
    else if (Is(element, "Material"))
    {
        out.material = name.value_or("");
    }
    else if (Is(element, "DefaultSemantic"))
    {
        out.defaultSemantic = name.value_or("");
    }
    else if (Is(element, "Texture"))
    {
        out.textures.push_back({name.value_or(""), Attribute(tag, "ResName").value_or("")});
    }
    else if (const auto value = Attribute(tag, "value");
             name && value &&
             std::ranges::none_of(kNotParameters, [&](std::string_view other) { return Is(element, other); }))
    {
        out.parameters.push_back({std::string(element), *name, *value});
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
    xml::ForEachTag(text.substr(0, end), [&](std::string_view tag) {
        if (!tag.starts_with('/'))
        {
            seen = seen || Is(ElementOf(tag), "MaterialObject");
            Take(tag, out);
        }
    });
    return seen ? std::optional(std::move(out)) : std::nullopt;
}

} // namespace dao::core::mao
