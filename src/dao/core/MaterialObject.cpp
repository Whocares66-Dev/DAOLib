// dao/core/MaterialObject.h: a .mao's elements read one tag at a time (dao/core/XmlTags.h), and
// the exporter's dc:source comment. The elements are filed as MaterialObjectBuilderXML2 files them
// (render-19, 2026-11-01): at the closing tag, from the attributes of the opening one.
#include "dao/core/MaterialObject.h"

#include "dao/core/XmlTags.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <utility>

namespace dao::core::mao
{

namespace
{

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

// Whether two names are the same, ASCII letters' case aside. The engine compares element names
// without case (EndElement 0x005077F0: _wcsicmp and EqualsNoCase 0x004C0A50), and Clouds.mao and
// CloudMaterial.mao write <texture>, drawn with those textures (DAOLib #209, from
// DAOParagonEngine's level reader and elgara-agent's captures). The shipped files write "value"
// where the reader names Value (0x00508220's 0xAF44BC), so attributes are matched so too.
bool Is(std::string_view element, std::string_view name)
{
    return std::ranges::equal(element, name, [](char a, char b) {
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
        return lower(a) == lower(b);
    });
}

// A tag's attributes, as written.
struct Attributes
{
    std::vector<std::pair<std::string, std::string>> list;

    // Returns the first attribute of a name, or nullptr.
    const std::string *Find(std::string_view name) const
    {
        const auto found = std::ranges::find_if(list, [&](const auto &pair) { return Is(pair.first, name); });
        return found != list.end() ? &found->second : nullptr;
    }
};

// Reads a tag's quoted attributes: a name, spaces allowed about the `=`, either quote.
Attributes AttributesOf(std::string_view tag)
{
    Attributes out;
    std::size_t at = xml::ElementOf(tag).size();
    while (at < tag.size())
    {
        while (at < tag.size() && (xml::Space(tag[at]) || tag[at] == '/'))
        {
            ++at;
        }
        const std::size_t nameEnd = tag.find_first_of(" \t\r\n=/", at);
        if (nameEnd == std::string_view::npos)
        {
            break;
        }
        const std::string_view name = tag.substr(at, nameEnd - at);
        at = nameEnd;
        while (at < tag.size() && xml::Space(tag[at]))
        {
            ++at;
        }
        if (at >= tag.size() || tag[at] != '=')
        {
            continue;
        }
        ++at;
        while (at < tag.size() && xml::Space(tag[at]))
        {
            ++at;
        }
        if (at >= tag.size() || (tag[at] != '"' && tag[at] != '\''))
        {
            break;
        }
        const std::size_t close = tag.find(tag[at], at + 1);
        if (close == std::string_view::npos)
        {
            break;
        }
        out.list.emplace_back(std::string(name), xml::Unescape(tag.substr(at + 1, close - at - 1)));
        at = close + 1;
    }
    return out;
}

// A Value read as the engine reads it: a wide stringstream's `>> float`, one float after another
// (0x00508220: 0x00454300 per float, and 0x004D29D0 for the end). A read at the end of the text,
// or one that reaches it, sets the end; one that finds no number fails.
class FloatStream
{
  public:
    explicit FloatStream(std::string_view text) : m_text(text)
    {
    }

    // Reads the next float into out. @return false where there was none.
    bool Read(float &out)
    {
        if (m_failed)
        {
            return false;
        }
        while (m_at < m_text.size() && xml::Space(m_text[m_at]))
        {
            ++m_at;
        }
        std::size_t from = m_at;
        if (from < m_text.size() && m_text[from] == '+')
        {
            ++from;
        }
        float value = 0.0F;
        const char *begin = m_text.data() + from;
        const char *end = m_text.data() + m_text.size();
        const auto parsed = from < m_text.size() ? std::from_chars(begin, end, value)
                                                 : std::from_chars_result{begin, std::errc::invalid_argument};
        if (parsed.ec != std::errc())
        {
            m_failed = true;
            m_atEnd = m_atEnd || from >= m_text.size();
            return false;
        }
        m_at = static_cast<std::size_t>(parsed.ptr - m_text.data());
        m_atEnd = m_at >= m_text.size();
        out = value;
        return true;
    }

    // Whether a read has hit the end of the text, as a stream's eof().
    bool AtEnd() const
    {
        return m_atEnd;
    }

    // Whether a read found no number, as a stream's fail().
    bool Failed() const
    {
        return m_failed;
    }

  private:
    std::string_view m_text;
    std::size_t m_at = 0;
    bool m_atEnd = false;
    bool m_failed = false;
};

// Fills out from a Value's floats, as many as there are; the rest stay zero.
template <std::size_t N> std::array<float, N> Floats(std::string_view value)
{
    std::array<float, N> out{};
    FloatStream stream(value);
    for (float &slot : out)
    {
        if (!stream.Read(slot))
        {
            break;
        }
    }
    return out;
}

// A Vector4fArray's Value: the vectors are read until the stream ends. The end test comes before
// the four reads, so a Value that ends in white space, or is empty, gives one more vector, zero
// where the reads found nothing (0x0050845E..0x00508512; read in the code, not run). A read that
// fails short of the end would never finish in the engine; here it ends the array.
Vector4Array Vector4s(std::string_view value)
{
    Vector4Array out;
    FloatStream stream(value);
    while (!stream.AtEnd())
    {
        Vector4 vector{};
        for (float &slot : vector)
        {
            stream.Read(slot);
        }
        out.push_back(vector);
        if (stream.Failed())
        {
            break;
        }
    }
    return out;
}

// _wtof and _wtoi of a Float's and a SoundType's Value (0x005087FD, 0x00508799): the number at the
// start, 0 where there is none.
float FloatOf(std::string_view value)
{
    return Floats<1>(value)[0];
}

int IntOf(const std::string &value)
{
    return static_cast<int>(std::strtol(value.c_str(), nullptr, 10));
}

// An element as the engine holds it between its opening tag and its close.
struct Open
{
    std::string element;
    Attributes attributes;
};

// The material object being built: what has been filed, and whether MaterialObject has opened.
struct Builder
{
    MaterialObject out;
    bool seen = false;

    // Files an element at its close (EndElement, 0x005077F0). Each needs a Name or ID; the kinds
    // with a value need that too (the flags at +0x20..+0x26).
    void File(const Open &open)
    {
        if (!seen)
        {
            return;
        }
        const std::string_view element = open.element;
        const std::string *name = open.attributes.Find("Name");
        name = name != nullptr ? name : open.attributes.Find("ID");
        if (name == nullptr)
        {
            return;
        }
        const std::string *value = open.attributes.Find("Value");
        if (Is(element, "Texture"))
        {
            // 0x005078A2: Texture needs its ResName too.
            if (const std::string *resource = open.attributes.Find("ResName"))
            {
                const std::string *requires_ = open.attributes.Find("RequiresID");
                out.textures.push_back({*name, *resource, requires_ != nullptr && Is(*requires_, "true")});
            }
        }
        else if (Is(element, "Material"))
        {
            out.material = *name; // 0x0050158F: the material goes to object+0x8
        }
        else if (Is(element, "DefaultSemantic"))
        {
            out.defaultSemantic = *name; // 0x00507A60; the object's +0x74 is applied by the caller
        }
        else if (Is(element, "Override"))
        {
            out.overrideName = *name; // 0x004FE930: object+0x38
        }
        else if (value != nullptr)
        {
            FileParameter(element, *name, *value);
        }
    }

    // The kinds with a Value (0x005078E0..0x00507B7A), at the closing tag.
    void FileParameter(std::string_view element, const std::string &name, const std::string &value)
    {
        if (Is(element, "Vector4f"))
        {
            out.parameters.push_back({name, Floats<4>(value)});
        }
        else if (Is(element, "Vector4fArray"))
        {
            out.parameters.push_back({name, Vector4s(value)});
        }
        else if (Is(element, "Matrix4x4f"))
        {
            out.parameters.push_back({name, Floats<16>(value)});
        }
        else if (Is(element, "Float"))
        {
            out.parameters.push_back({name, FloatOf(value)});
        }
        else if (Is(element, "SoundType"))
        {
            out.parameters.push_back({name, SoundType{IntOf(value)}});
        }
    }

    // An element opens (StartElement, 0x00507E00). The first MaterialObject makes the object and
    // reads its Name; a second is ignored with its attributes (0x00507E9F).
    void Opened(const Open &open)
    {
        if (!Is(open.element, "MaterialObject") || seen)
        {
            return;
        }
        seen = true;
        const std::string *name = open.attributes.Find("Name");
        name = name != nullptr ? name : open.attributes.Find("ID");
        out.name = name != nullptr ? *name : std::string();
    }
};

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
    Builder builder;
    builder.out.end = end;
    builder.out.source = Source(text);
    std::vector<Open> stack;
    xml::ForEachTag(text.substr(0, end), [&](std::string_view tag) {
        if (tag.starts_with('/'))
        {
            // The top of the stack is what closes, whatever the closing tag is called (0x00507730).
            if (!stack.empty())
            {
                builder.File(stack.back());
                stack.pop_back();
            }
            return;
        }
        Open open{std::string(xml::ElementOf(tag)), AttributesOf(tag)};
        builder.Opened(open);
        if (tag.ends_with('/'))
        {
            builder.File(open);
        }
        else
        {
            stack.push_back(std::move(open));
        }
    });
    return builder.seen ? std::optional(std::move(builder.out)) : std::nullopt;
}

} // namespace dao::core::mao
