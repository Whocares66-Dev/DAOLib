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

// How many floats a field of a type holds; 0 for a type that is not floats.
std::size_t FloatsIn(std::uint16_t type)
{
    switch (static_cast<Type>(type))
    {
    case Type::F32:
        return 1;
    case Type::Vector3:
        return 3;
    case Type::Vector4:
    case Type::Quaternion:
    case Type::Color:
        return 4;
    case Type::Matrix:
        return 16;
    default:
        return 0;
    }
}

// A generic field's one element: its type and flags, and where its value is. The struct holds
// the element's type and flags, then the value, or with the field's reference flag an offset to
// it (an effect's parameter in a level layout, DAOLib #154; an animation's keys, DAOLib #157).
struct Element
{
    std::uint16_t type = 0;
    std::uint16_t flags = 0;
    std::size_t at = 0;
};

std::optional<Element> GenericElement(const File &file, std::size_t slot, const Field &field)
{
    const auto word = file.U32(slot);
    if (!word)
    {
        return std::nullopt;
    }
    Element element{.type = static_cast<std::uint16_t>(*word & 0xFFFF),
                    .flags = static_cast<std::uint16_t>(*word >> 16),
                    .at = slot + 4};
    if ((field.flags & kReference) != 0)
    {
        const auto offset = file.U32(element.at);
        if (!offset || *offset == kNull)
        {
            return std::nullopt;
        }
        element.at = file.DataOffset() + *offset;
    }
    return element;
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
                const std::size_t f = *offset + (std::size_t{12} * j);
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
        text[i] = static_cast<wchar_t>(ReadAt<std::uint16_t>(bytes_, p + 4 + (std::size_t{2} * i)).value_or(0));
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
    if (field == nullptr || (field->flags & (kList | kStruct)) != 0)
    {
        return std::nullopt;
    }
    // A generic field holding one value of the type: a campaign's variable,
    // its value's type in the element (singleplayer.cif's ScriptVarTable).
    if (field->type == static_cast<std::uint16_t>(Type::Generic))
    {
        const std::optional<Element> element = GenericElement(*file_, at_ + field->offset, *field);
        if (!element || element->type != static_cast<std::uint16_t>(type) || (element->flags & (kList | kStruct)) != 0)
        {
            return std::nullopt;
        }
        return element->at;
    }
    if (field->type != static_cast<std::uint16_t>(type))
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
    if (field != nullptr && field->type == static_cast<std::uint16_t>(Type::Generic))
    {
        const auto slot = Slot(label, Type::String);
        return slot ? file_->StringAt(*slot) : std::nullopt;
    }
    if (field == nullptr || field->type != static_cast<std::uint16_t>(Type::String) ||
        (field->flags & (kList | kStruct)) != 0)
    {
        return std::nullopt;
    }
    return file_->StringAt(at_ + field->offset);
}

std::vector<float> Struct::Floats(std::uint32_t label) const
{
    std::vector<float> floats;
    const Field *field = Shape().Find(label);
    if (field == nullptr || (field->flags & (kList | kStruct)) != 0)
    {
        return floats;
    }
    std::uint16_t type = field->type;
    std::optional<std::size_t> slot;
    if (type == static_cast<std::uint16_t>(Type::Generic))
    {
        const std::optional<Element> element = GenericElement(*file_, at_ + field->offset, *field);
        if (!element || (element->flags & (kList | kStruct)) != 0)
        {
            return floats;
        }
        type = element->type;
        slot = element->at;
    }
    else
    {
        slot = Slot(label, static_cast<Type>(type));
    }
    const std::size_t count = FloatsIn(type);
    for (std::size_t i = 0; slot && i < count; ++i)
    {
        const auto value = ReadAt<float>(file_->Bytes(), *slot + (i * sizeof(float)));
        if (!value)
        {
            floats.clear();
            break;
        }
        floats.push_back(*value);
    }
    return floats;
}

std::span<const std::byte> Struct::ListBytes(std::uint32_t label, Type element) const
{
    std::size_t size = 0;
    switch (element)
    {
    case Type::U8:
    case Type::S8:
        size = 1;
        break;
    case Type::U16:
    case Type::S16:
        size = 2;
        break;
    case Type::U32:
    case Type::S32:
    case Type::F32:
        size = 4;
        break;
    // A Vector3's element in a list takes 16 bytes, its fourth float junk: an effect's mesh spawn
    // volume's points (the 20 "data" structs' Vector3fList), whose last element is a point only
    // at that stride, read 2026-10-18 (DAOLib #181).
    case Type::Vector3:
    case Type::Vector4:
    case Type::Quaternion: // a saved effect's rotations (DAOLib #202)
        size = 16;
        break;
    default:
        return {};
    }
    const Field *field = Shape().Find(label);
    if (field == nullptr || field->type != static_cast<std::uint16_t>(element) || (field->flags & kList) == 0 ||
        (field->flags & kStruct) != 0)
    {
        return {};
    }
    const auto offset = file_->U32(at_ + field->offset);
    if (!offset || *offset == kNull)
    {
        return {};
    }
    const std::size_t list = file_->DataOffset() + *offset;
    const auto count = file_->U32(list);
    const std::span<const std::byte> bytes = file_->Bytes();
    if (!count || list + 4 > bytes.size() || *count > (bytes.size() - list - 4) / size)
    {
        return {};
    }
    return bytes.subspan(list + 4, *count * size);
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

// The text word is 0 where the field has no text of its own, as the game's files leave it and SetTlk
// writes it: the data's first bytes are the top struct, never a string. Read as an offset, it gave the
// top struct's bytes as a string, which an edit there changed (DAOLib #106's round trip, 2026-10-23).
std::optional<std::wstring> Struct::TlkText(std::uint32_t label) const
{
    const auto slot = Slot(label, Type::TlkString);
    if (!slot)
    {
        return std::nullopt;
    }
    if (file_->U32(*slot + 4) == std::optional<std::uint32_t>(0))
    {
        return std::wstring();
    }
    return file_->StringAt(*slot + 4);
}

std::optional<Struct> Struct::Child(std::uint32_t label) const
{
    const Field *field = Shape().Find(label);
    if (field != nullptr && field->type == static_cast<std::uint16_t>(Type::Generic) && (field->flags & kList) == 0)
    {
        const std::optional<Element> element = GenericElement(*file_, at_ + field->offset, *field);
        if (!element || (element->flags & kStruct) == 0 || (element->flags & kList) != 0 ||
            element->type >= file_->Templates().size())
        {
            return std::nullopt;
        }
        return Struct(*file_, element->type, element->at);
    }
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

namespace
{

// Where a list's element puts its struct: in place at `p`, or, for a list
// of references, at the offset `p` holds; nothing for a null reference.
std::optional<std::size_t> StructAt(const File &file, std::size_t p, bool reference)
{
    if (!reference)
    {
        return p;
    }
    const auto offset = file.U32(p);
    if (!offset || *offset == kNull)
    {
        return std::nullopt;
    }
    return file.DataOffset() + *offset;
}

// A generic list's structs: each element a type and flags, then the struct
// in place or its offset. Only the structs among them are given.
void GenericStructs(const File &file, std::size_t at, std::uint32_t count, bool reference, std::vector<Struct> &out)
{
    const auto &templates = file.Templates();
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const std::size_t element = at + 4 + (kGenericSize * i);
        const auto typeAndFlags = file.U32(element);
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
        if (const auto p = StructAt(file, element + 4, reference))
        {
            out.emplace_back(file, type, *p);
        }
    }
}

} // namespace

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
        GenericStructs(*file_, at, *count, reference, out);
        return out;
    }
    if ((field->flags & kStruct) == 0 || field->type >= templates.size())
    {
        return out;
    }
    const std::size_t size = reference ? 4 : templates[field->type].size;
    for (std::uint32_t i = 0; i < *count; ++i)
    {
        if (const auto p = StructAt(*file_, at + 4 + (size * i), reference))
        {
            out.emplace_back(*file_, field->type, *p);
        }
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
        const auto value = file_->U32(at + 4 + (std::size_t{4} * i));
        if (!value)
        {
            break;
        }
        out.push_back(*value);
    }
    return out;
}

// A list of strings is its count and then a string's slot an element, each the offset of the
// string's length and characters, as a string field's own slot is (tools/gff4.py reads them so).
std::vector<std::wstring> Struct::Strings(std::uint32_t label) const
{
    std::vector<std::wstring> out;
    const Field *field = Shape().Find(label);
    if (field == nullptr || (field->flags & kList) == 0 || (field->flags & kStruct) != 0 ||
        field->type != static_cast<std::uint16_t>(Type::String))
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
        auto text = file_->StringAt(at + 4 + (std::size_t{4} * i));
        if (!text)
        {
            break;
        }
        out.push_back(std::move(*text));
    }
    return out;
}

} // namespace dao::core::gff4

namespace dao::core::gff4
{

namespace
{

// A whole-number type's width, 0 for a type that is none.
std::size_t WidthOf(std::uint16_t type)
{
    switch (static_cast<Type>(type))
    {
    case Type::U8:
    case Type::S8:
        return 1;
    case Type::U16:
    case Type::S16:
        return 2;
    case Type::U32:
    case Type::S32:
        return 4;
    case Type::U64:
    case Type::S64:
        return 8;
    default:
        return 0;
    }
}

} // namespace

Editor::Editor(const File &file)
    : file_(&file), data_(file.DataOffset()), bytes_(file.Bytes().begin(), file.Bytes().end())
{
}

std::optional<std::size_t> Editor::Slot(const Struct &in, std::uint32_t label, std::uint16_t &type) const
{
    const Field *field = in.Shape().Find(label);
    if (field == nullptr)
    {
        return std::nullopt;
    }
    type = field->type;
    const std::size_t at = in.Offset() + field->offset;
    if (at + 4 > bytes_.size())
    {
        return std::nullopt;
    }
    return at;
}

std::uint32_t Editor::Append(std::span<const std::byte> data)
{
    // Kept to four bytes, as the game's files keep their lists and strings.
    bytes_.resize((bytes_.size() + 3) & ~std::size_t{3});
    const std::size_t at = bytes_.size();
    bytes_.insert(bytes_.end(), data.begin(), data.end());
    return static_cast<std::uint32_t>(at - data_);
}

bool Editor::SetNumber(const Struct &in, std::uint32_t label, std::int64_t value)
{
    const Field *field = in.Shape().Find(label);
    std::uint16_t type = 0;
    const auto at = Slot(in, label, type);
    const std::size_t width = WidthOf(type);
    if (!at || width == 0 || (field->flags & (kList | kStruct | kReference)) != 0 || *at + width > bytes_.size())
    {
        return false;
    }
    std::memcpy(bytes_.data() + *at, &value, width); // little-endian: the low bytes are the narrower value
    return true;
}

bool Editor::SetFloats(const Struct &in, std::uint32_t label, std::span<const float> values)
{
    const Field *field = in.Shape().Find(label);
    std::uint16_t type = 0;
    const auto at = Slot(in, label, type);
    if (!at || FloatsIn(type) != values.size() || (field->flags & (kList | kStruct | kReference)) != 0 ||
        *at + values.size_bytes() > bytes_.size())
    {
        return false;
    }
    std::memcpy(bytes_.data() + *at, values.data(), values.size_bytes());
    return true;
}

std::uint32_t Editor::AppendString(std::wstring_view text)
{
    // Its count of UTF-16 units with the NUL, the units, the NUL; never written over an old string, which
    // another field may share.
    std::vector<std::byte> data(4 + ((text.size() + 1) * 2));
    const auto count = static_cast<std::uint32_t>(text.size() + 1);
    std::memcpy(data.data(), &count, 4);
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const auto unit = static_cast<std::uint16_t>(text[i]);
        std::memcpy(data.data() + 4 + (i * 2), &unit, 2);
    }
    return Append(data);
}

bool Editor::SetString(const Struct &in, std::uint32_t label, std::wstring_view text)
{
    const Field *field = in.Shape().Find(label);
    std::uint16_t type = 0;
    const auto at = Slot(in, label, type);
    if (!at || type != static_cast<std::uint16_t>(Type::String) || (field->flags & (kList | kStruct)) != 0)
    {
        return false;
    }
    const std::uint32_t offset = AppendString(text);
    std::memcpy(bytes_.data() + *at, &offset, 4);
    return true;
}

bool Editor::SetList(const Struct &in, std::uint32_t label, std::span<const std::byte> elements, std::uint32_t count,
                     Place place)
{
    const Field *field = in.Shape().Find(label);
    std::uint16_t type = 0;
    const auto at = Slot(in, label, type);
    if (!at || (field->flags & kList) == 0)
    {
        return false;
    }
    std::uint32_t old = 0;
    std::memcpy(&old, bytes_.data() + *at, 4);
    if (place == Place::InPlaceIfFits && old != kNull && data_ + old + 4 <= bytes_.size())
    {
        std::uint32_t oldCount = 0;
        std::memcpy(&oldCount, bytes_.data() + data_ + old, 4);
        const std::size_t element = count == 0 ? 0 : elements.size() / count;
        const std::size_t room = bytes_.size() - (data_ + old + 4);
        if (std::size_t{oldCount} * element >= elements.size() && room >= elements.size())
        {
            std::memcpy(bytes_.data() + data_ + old, &count, 4);
            std::memcpy(bytes_.data() + data_ + old + 4, elements.data(), elements.size());
            return true;
        }
    }
    std::vector<std::byte> data(4 + elements.size());
    std::memcpy(data.data(), &count, 4);
    std::memcpy(data.data() + 4, elements.data(), elements.size());
    const std::uint32_t offset = Append(data);
    std::memcpy(bytes_.data() + *at, &offset, 4);
    return true;
}

bool Editor::Extend(const Struct &in, std::uint32_t label, std::span<const std::byte> elements, std::uint32_t count)
{
    const Field *field = in.Shape().Find(label);
    std::uint16_t type = 0;
    const auto at = Slot(in, label, type);
    if (!at || (field->flags & kList) == 0 || count == 0 || elements.size() % count != 0)
    {
        return false;
    }
    const std::size_t element = elements.size() / count;
    std::uint32_t old = 0;
    std::memcpy(&old, bytes_.data() + *at, 4);
    std::uint32_t oldCount = 0;
    if (old != kNull)
    {
        if (data_ + old + 4 > bytes_.size())
        {
            return false;
        }
        std::memcpy(&oldCount, bytes_.data() + data_ + old, 4);
        if (std::size_t{oldCount} * element > bytes_.size() - (data_ + old + 4))
        {
            return false;
        }
    }
    const std::size_t kept = std::size_t{oldCount} * element;
    std::vector<std::byte> data(4 + kept + elements.size());
    const std::uint32_t total = oldCount + count;
    std::memcpy(data.data(), &total, 4);
    if (kept != 0)
    {
        std::memcpy(data.data() + 4, bytes_.data() + data_ + old + 4, kept);
    }
    std::memcpy(data.data() + 4 + kept, elements.data(), elements.size());
    const std::uint32_t offset = Append(data);
    std::memcpy(bytes_.data() + *at, &offset, 4);
    return true;
}

std::optional<std::vector<Struct>> Editor::CopyList(const Struct &in, std::uint32_t label, std::span<const Struct> like)
{
    const Field *field = in.Shape().Find(label);
    std::uint16_t type = 0;
    const auto at = Slot(in, label, type);
    if (!at || (field->flags & (kList | kStruct | kReference)) != (kList | kStruct) ||
        type >= file_->Templates().size())
    {
        return std::nullopt;
    }
    const std::size_t size = file_->Templates()[type].size;
    std::vector<std::byte> data(4 + (like.size() * size));
    const auto count = static_cast<std::uint32_t>(like.size());
    std::memcpy(data.data(), &count, 4);
    for (std::size_t i = 0; i < like.size(); ++i)
    {
        const Struct &one = like[i];
        if (one.Index() != type || one.Offset() + size > bytes_.size())
        {
            return std::nullopt;
        }
        std::memcpy(data.data() + 4 + (i * size), bytes_.data() + one.Offset(), size);
    }
    const std::uint32_t offset = Append(data);
    std::memcpy(bytes_.data() + *at, &offset, 4);
    std::vector<Struct> copies;
    copies.reserve(like.size());
    for (std::size_t i = 0; i < like.size(); ++i)
    {
        copies.emplace_back(*file_, type, data_ + offset + 4 + (i * size));
    }
    return copies;
}

std::optional<Struct> Editor::Add(const Struct &like)
{
    const std::size_t size = like.Shape().size;
    if (like.Offset() < data_ || like.Offset() + size > bytes_.size())
    {
        return std::nullopt;
    }
    const std::vector<std::byte> copy(bytes_.begin() + static_cast<std::ptrdiff_t>(like.Offset()),
                                      bytes_.begin() + static_cast<std::ptrdiff_t>(like.Offset() + size));
    const std::uint32_t offset = Append(copy);
    return Struct(*file_, like.Index(), data_ + offset);
}

void Editor::Blank(std::size_t at, std::size_t templateIndex, int depth)
{
    const Template &shape = file_->Templates()[templateIndex];
    if (depth > 8 || at + shape.size > bytes_.size())
    {
        return;
    }
    for (const Field &field : shape.fields)
    {
        const std::size_t slot = at + field.offset;
        if (slot + 4 > bytes_.size())
        {
            continue;
        }
        const bool inPlaceStruct = (field.flags & kStruct) != 0 && (field.flags & (kList | kReference)) == 0;
        if (inPlaceStruct)
        {
            if (field.type < file_->Templates().size())
            {
                Blank(slot, field.type, depth + 1);
            }
        }
        else if ((field.flags & (kList | kReference)) != 0 || field.type == static_cast<std::uint16_t>(Type::String) ||
                 field.type == static_cast<std::uint16_t>(Type::TlkString))
        {
            std::memcpy(bytes_.data() + slot, &kNull, 4); // a talk-table string's text word stays 0
        }
    }
}

std::optional<Struct> Editor::New(std::size_t templateIndex)
{
    if (templateIndex >= file_->Templates().size())
    {
        return std::nullopt;
    }
    const std::vector<std::byte> zero(file_->Templates()[templateIndex].size);
    const std::uint32_t offset = Append(zero);
    Blank(data_ + offset, templateIndex, 0);
    return Struct(*file_, templateIndex, data_ + offset);
}

bool Editor::SetChild(const Struct &in, std::uint32_t label, const Struct *to)
{
    const Field *field = in.Shape().Find(label);
    std::uint16_t type = 0;
    const auto at = Slot(in, label, type);
    if (!at || (field->flags & (kList | kStruct | kReference)) != (kStruct | kReference))
    {
        return false;
    }
    std::uint32_t offset = kNull;
    if (to != nullptr)
    {
        if (to->Index() != type || to->Offset() < data_ || to->Offset() >= bytes_.size())
        {
            return false;
        }
        offset = static_cast<std::uint32_t>(to->Offset() - data_);
    }
    std::memcpy(bytes_.data() + *at, &offset, 4);
    return true;
}

bool Editor::SetStructs(const Struct &in, std::uint32_t label, std::span<const Struct> elements)
{
    const Field *field = in.Shape().Find(label);
    std::uint16_t type = 0;
    const auto at = Slot(in, label, type);
    if (!at || (field->flags & (kList | kReference)) != (kList | kReference))
    {
        return false;
    }
    const bool generic = type == static_cast<std::uint16_t>(Type::Generic) && (field->flags & kStruct) == 0;
    const bool typed = (field->flags & kStruct) != 0 && type < file_->Templates().size();
    if (!generic && !typed)
    {
        return false;
    }
    const std::size_t each = generic ? kGenericSize : 4;
    std::vector<std::byte> data(4 + (elements.size() * each));
    const auto count = static_cast<std::uint32_t>(elements.size());
    std::memcpy(data.data(), &count, 4);
    for (std::size_t i = 0; i < elements.size(); ++i)
    {
        const Struct &one = elements[i];
        if ((typed && one.Index() != type) || one.Offset() < data_ || one.Offset() >= bytes_.size())
        {
            return false;
        }
        const auto offset = static_cast<std::uint32_t>(one.Offset() - data_);
        std::byte *to = data.data() + 4 + (i * each);
        if (generic)
        {
            // The element's template and the struct flag; the reference is the field's.
            const std::uint32_t word = static_cast<std::uint32_t>(one.Index()) | (std::uint32_t{kStruct} << 16);
            std::memcpy(to, &word, 4);
            to += 4;
        }
        std::memcpy(to, &offset, 4);
    }
    const std::uint32_t list = Append(data);
    std::memcpy(bytes_.data() + *at, &list, 4);
    return true;
}

bool Editor::SetTlk(const Struct &in, std::uint32_t label, std::uint32_t strref, std::optional<std::wstring_view> text)
{
    const Field *field = in.Shape().Find(label);
    std::uint16_t type = 0;
    const auto at = Slot(in, label, type);
    if (!at || type != static_cast<std::uint16_t>(Type::TlkString) ||
        (field->flags & (kList | kStruct | kReference)) != 0 || *at + 8 > bytes_.size())
    {
        return false;
    }
    std::memcpy(bytes_.data() + *at, &strref, 4);
    if (text)
    {
        const std::uint32_t offset = text->empty() ? 0 : AppendString(*text);
        std::memcpy(bytes_.data() + *at + 4, &offset, 4);
    }
    return true;
}

bool Editor::SetStrings(const Struct &in, std::uint32_t label, std::span<const std::wstring> texts)
{
    const Field *field = in.Shape().Find(label);
    std::uint16_t type = 0;
    const auto at = Slot(in, label, type);
    if (!at || type != static_cast<std::uint16_t>(Type::String) || (field->flags & (kList | kStruct)) != kList)
    {
        return false;
    }
    std::vector<std::byte> data(4 + (texts.size() * 4));
    const auto count = static_cast<std::uint32_t>(texts.size());
    std::memcpy(data.data(), &count, 4);
    for (std::size_t i = 0; i < texts.size(); ++i)
    {
        // Each string is appended before the list is, so the list's own offset comes last.
        const std::uint32_t offset = AppendString(texts[i]);
        std::memcpy(data.data() + 4 + (i * 4), &offset, 4);
    }
    const std::uint32_t list = Append(data);
    std::memcpy(bytes_.data() + *at, &list, 4);
    return true;
}

void Editor::Truncate(std::size_t size)
{
    if (size >= data_ && size <= bytes_.size())
    {
        bytes_.resize(size);
    }
}

} // namespace dao::core::gff4
