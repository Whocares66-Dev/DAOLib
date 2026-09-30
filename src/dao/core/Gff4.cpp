#include "dao/core/Gff4.h"

#include <algorithm>
#include <cstring>

namespace dao::core::gff4
{

namespace
{

constexpr std::uint32_t kNull = 0xFFFFFFFF;
constexpr std::size_t kGenericSize = 8;

bool Is(std::span<const std::byte> bytes, std::size_t at, std::string_view text)
{
    return bytes.size() >= at + text.size() && std::memcmp(bytes.data() + at, text.data(), text.size()) == 0;
}

std::string Label(std::span<const std::byte> bytes, std::size_t at)
{
    std::string label(reinterpret_cast<const char *>(bytes.data() + at), 4);
    while (!label.empty() && (label.back() == ' ' || label.back() == '\0'))
    {
        label.pop_back();
    }
    return label;
}

} // namespace

const Field *Template::Find(std::uint32_t wanted) const
{
    for (const Field &field : fields)
    {
        if (field.label == wanted)
        {
            return &field;
        }
    }
    return nullptr;
}

std::optional<File> File::Parse(std::span<const std::byte> bytes)
{
    if (!Is(bytes, 0, "GFF ") || !(Is(bytes, 4, "V4.0") || Is(bytes, 4, "V4.1")) || !Is(bytes, 8, "PC  ") ||
        bytes.size() < 24)
    {
        return std::nullopt;
    }
    File file;
    file.bytes_ = bytes;
    file.type_ = Label(bytes, 12);
    file.typeVersion_ = Label(bytes, 16);
    const auto count = ReadAt<std::uint32_t>(bytes, 20);
    std::size_t at = 24;
    if (Is(bytes, 4, "V4.1"))
    {
        // Shared strings: none of the game's files use them, and they are
        // not read (tools/gff4.py refuses them too).
        const auto shared = ReadAt<std::uint32_t>(bytes, 24);
        if (!shared || *shared != 0)
        {
            return std::nullopt;
        }
        at = 32;
    }
    const auto data = ReadAt<std::uint32_t>(bytes, at);
    if (!count || !data || *data > bytes.size() || *count > bytes.size() / 16)
    {
        return std::nullopt;
    }
    file.data_ = *data;
    at += 4;
    for (std::uint32_t i = 0; i < *count; ++i, at += 16)
    {
        const auto fields = ReadAt<std::uint32_t>(bytes, at + 4);
        const auto offset = ReadAt<std::uint32_t>(bytes, at + 8);
        const auto size = ReadAt<std::uint32_t>(bytes, at + 12);
        if (!fields || !offset || !size)
        {
            return std::nullopt;
        }
        Template shape;
        shape.label = Label(bytes, at);
        shape.size = *size;
        if (*offset != kNull)
        {
            if (*fields > bytes.size() / 12)
            {
                return std::nullopt;
            }
            for (std::uint32_t j = 0; j < *fields; ++j)
            {
                const std::size_t f = *offset + std::size_t{12} * j;
                const auto label = ReadAt<std::uint32_t>(bytes, f);
                const auto typeAndFlags = ReadAt<std::uint32_t>(bytes, f + 4);
                const auto fieldOffset = ReadAt<std::uint32_t>(bytes, f + 8);
                if (!label || !typeAndFlags || !fieldOffset)
                {
                    return std::nullopt;
                }
                shape.fields.push_back({*label, static_cast<std::uint16_t>(*typeAndFlags & 0xFFFF),
                                        static_cast<std::uint16_t>(*typeAndFlags >> 16), *fieldOffset});
            }
        }
        file.templates_.push_back(std::move(shape));
    }
    return file;
}

std::optional<Struct> File::Root() const
{
    if (templates_.empty())
    {
        return std::nullopt;
    }
    return Struct(*this, 0, data_);
}

std::optional<std::uint32_t> File::U32(std::size_t at) const
{
    return ReadAt<std::uint32_t>(bytes_, at);
}

std::optional<std::wstring> File::StringAt(std::size_t at) const
{
    const auto offset = U32(at);
    if (!offset)
    {
        return std::nullopt;
    }
    if (*offset == kNull)
    {
        return std::wstring();
    }
    const std::size_t p = data_ + *offset;
    const auto count = U32(p);
    if (!count || *count > (bytes_.size() - std::min(bytes_.size(), p + 4)) / 2)
    {
        return std::nullopt;
    }
    std::wstring text(*count, L'\0');
    for (std::uint32_t i = 0; i < *count; ++i)
    {
        text[i] = static_cast<wchar_t>(ReadAt<std::uint16_t>(bytes_, p + 4 + std::size_t{2} * i).value_or(0));
    }
    while (!text.empty() && text.back() == L'\0')
    {
        text.pop_back();
    }
    return text;
}

const std::string &Struct::Label() const
{
    return Shape().label;
}

const Template &Struct::Shape() const
{
    return file_->Templates()[template_];
}

std::optional<std::size_t> Struct::Slot(std::uint32_t label, Type type) const
{
    const Field *field = Shape().Find(label);
    if (field == nullptr || field->type != static_cast<std::uint16_t>(type) || (field->flags & (kList | kStruct)) != 0)
    {
        return std::nullopt;
    }
    std::size_t at = at_ + field->offset;
    if ((field->flags & kReference) != 0)
    {
        const auto offset = file_->U32(at);
        if (!offset || *offset == kNull)
        {
            return std::nullopt;
        }
        at = file_->DataOffset() + *offset;
    }
    return at;
}

std::optional<std::wstring> Struct::String(std::uint32_t label) const
{
    // A string's slot always holds an offset, flagged a reference or not.
    const Field *field = Shape().Find(label);
    if (field == nullptr || field->type != static_cast<std::uint16_t>(Type::String) ||
        (field->flags & (kList | kStruct)) != 0)
    {
        return std::nullopt;
    }
    return file_->StringAt(at_ + field->offset);
}

std::optional<TlkString> Struct::Tlk(std::uint32_t label) const
{
    const auto slot = Slot(label, Type::TlkString);
    if (!slot)
    {
        return std::nullopt;
    }
    const auto strref = file_->U32(*slot);
    const auto text = file_->U32(*slot + 4);
    if (!strref || !text)
    {
        return std::nullopt;
    }
    return TlkString{*strref, *text};
}

std::optional<Struct> Struct::Child(std::uint32_t label) const
{
    const Field *field = Shape().Find(label);
    if (field == nullptr || (field->flags & kStruct) == 0 || (field->flags & kList) != 0 ||
        field->type >= file_->Templates().size())
    {
        return std::nullopt;
    }
    std::size_t at = at_ + field->offset;
    if ((field->flags & kReference) != 0)
    {
        const auto offset = file_->U32(at);
        if (!offset || *offset == kNull)
        {
            return std::nullopt;
        }
        at = file_->DataOffset() + *offset;
    }
    return Struct(*file_, field->type, at);
}

std::vector<Struct> Struct::Structs(std::uint32_t label) const
{
    std::vector<Struct> out;
    const Field *field = Shape().Find(label);
    if (field == nullptr || (field->flags & kList) == 0)
    {
        return out;
    }
    const auto list = file_->U32(at_ + field->offset);
    if (!list || *list == kNull)
    {
        return out;
    }
    const std::size_t at = file_->DataOffset() + *list;
    const auto count = file_->U32(at);
    if (!count || *count > file_->Bytes().size())
    {
        return out;
    }
    const auto &templates = file_->Templates();
    const bool reference = (field->flags & kReference) != 0;
    if (field->type == static_cast<std::uint16_t>(Type::Generic) && (field->flags & kStruct) == 0)
    {
        // Each element: a type and flags, then the struct in place or its
        // offset. Only the structs among them are given.
        for (std::uint32_t i = 0; i < *count; ++i)
        {
            const std::size_t element = at + 4 + kGenericSize * i;
            const auto typeAndFlags = file_->U32(element);
            if (!typeAndFlags)
            {
                break;
            }
            const auto type = static_cast<std::uint16_t>(*typeAndFlags & 0xFFFF);
            const auto flags = static_cast<std::uint16_t>(*typeAndFlags >> 16);
            if ((flags & kStruct) == 0 || type >= templates.size())
            {
                continue;
            }
            std::size_t p = element + 4;
            if (reference)
            {
                const auto offset = file_->U32(p);
                if (!offset || *offset == kNull)
                {
                    continue;
                }
                p = file_->DataOffset() + *offset;
            }
            out.emplace_back(*file_, type, p);
        }
        return out;
    }
    if ((field->flags & kStruct) == 0 || field->type >= templates.size())
    {
        return out;
    }
    const std::size_t size = reference ? 4 : templates[field->type].size;
    for (std::uint32_t i = 0; i < *count; ++i)
    {
        std::size_t p = at + 4 + size * i;
        if (reference)
        {
            const auto offset = file_->U32(p);
            if (!offset || *offset == kNull)
            {
                continue;
            }
            p = file_->DataOffset() + *offset;
        }
        out.emplace_back(*file_, field->type, p);
    }
    return out;
}

std::vector<std::uint32_t> Struct::Numbers(std::uint32_t label) const
{
    std::vector<std::uint32_t> out;
    const Field *field = Shape().Find(label);
    if (field == nullptr || (field->flags & kList) == 0 || (field->flags & (kStruct | kReference)) != 0 ||
        (field->type != static_cast<std::uint16_t>(Type::U32) && field->type != static_cast<std::uint16_t>(Type::S32)))
    {
        return out;
    }
    const auto list = file_->U32(at_ + field->offset);
    if (!list || *list == kNull)
    {
        return out;
    }
    const std::size_t at = file_->DataOffset() + *list;
    const auto count = file_->U32(at);
    if (!count || *count > file_->Bytes().size() / 4)
    {
        return out;
    }
    for (std::uint32_t i = 0; i < *count; ++i)
    {
        const auto value = file_->U32(at + 4 + std::size_t{4} * i);
        if (!value)
        {
            break;
        }
        out.push_back(*value);
    }
    return out;
}

} // namespace dao::core::gff4
