// Notes on GFF3:
//
// Read as xoreos reads it (src/aurora/gff3file.cpp), written for Eluvian's
// ghilana 2026-10-12 and moved here (DAOLib #106): a header of the type and
// version as eight letters ("UTC V3.2"), then twelve 32-bit words, the offset
// and count of the structs, fields, labels, field data, field indices and
// list indices (the last three counted in bytes). A struct is 12 bytes: an id,
// then a field's index if it has one field, else a byte offset into the field
// indices, then its field count. A field is 12 bytes: its type, its label's
// index, and its value (a number of 32 bits or fewer, a struct's index, a
// byte offset into the list indices) or a byte offset into the field data. A
// label is 16 ASCII letters. In the field data, a String is a 32-bit length
// and its bytes, a ResRef a byte's length and its bytes (DAO's may be 255
// long, xoreos says), a LocString a 32-bit size, a string reference, a count,
// then each string's language id, length and bytes, a StrRef a size (4) and
// the reference. A list is a count of struct indices, then the indices.
//
// The game's templates open with their type and "V3.2" (counted over the GOG
// install's core archives: utc, uti, utp, utt, are, bic, lst; 2026-10-12).
// Their strings are 8-bit, as xoreos reads them, and are given as the bytes.
#include "dao/core/Gff3.h"

#include "dao/core/Bytes.h"

namespace dao::core::gff3
{

namespace
{

constexpr std::size_t kHeaderSize = 56;
constexpr std::size_t kStructSize = 12;
constexpr std::size_t kFieldSize = 12;
constexpr std::size_t kLabelSize = 16;
// An index that names nothing, for a read that runs out of the file.
constexpr std::uint32_t kNull = 0xFFFFFFFF;

std::string Letters(std::span<const std::byte> bytes, std::size_t at, std::size_t count)
{
    std::string text;
    for (std::size_t i = 0; i < count && at + i < bytes.size(); ++i)
    {
        const char c = static_cast<char>(bytes[at + i]);
        if (c == '\0')
        {
            break;
        }
        text += c;
    }
    while (!text.empty() && text.back() == ' ')
    {
        text.pop_back();
    }
    return text;
}

// A label as a view of the file's bytes, up to its NUL, trailing spaces left off: a struct's fields
// are made for every struct of a list, and a string each was most of a template's read (Paragon:
// 7.8 ms a creature template, DAOLib #207).
std::string_view LabelAt(std::span<const std::byte> bytes, std::size_t at, std::size_t count)
{
    if (at >= bytes.size())
    {
        return {};
    }
    const std::size_t most = std::min(count, bytes.size() - at);
    const std::string_view all(reinterpret_cast<const char *>(bytes.data() + at), most);
    std::string_view text = all.substr(0, all.find('\0'));
    while (!text.empty() && text.back() == ' ')
    {
        text.remove_suffix(1);
    }
    return text;
}

bool Inside(std::size_t size, std::uint64_t offset, std::uint64_t length)
{
    return offset <= size && length <= size - offset;
}

bool IsInline(Type type)
{
    switch (type)
    {
    case Type::Byte:
    case Type::Char:
    case Type::Word:
    case Type::Short:
    case Type::Dword:
    case Type::Int:
    case Type::Float:
        return true;
    default:
        return false;
    }
}

} // namespace

std::optional<File> File::Parse(std::span<const std::byte> bytes)
{
    if (bytes.size() < kHeaderSize)
    {
        return std::nullopt;
    }
    File file;
    file.bytes_ = bytes;
    file.type_ = Letters(bytes, 0, 4);
    file.version_ = Letters(bytes, 4, 4);
    if (file.version_ != "V3.2" && file.version_ != "V3.3")
    {
        return std::nullopt;
    }
    std::uint32_t words[12] = {};
    for (std::size_t i = 0; i < 12; ++i)
    {
        words[i] = ReadAt<std::uint32_t>(bytes, 8 + (4 * i)).value_or(0);
    }
    Header &h = file.header_;
    h = Header{words[0], words[1], words[2], words[3], words[4],  words[5],
               words[6], words[7], words[8], words[9], words[10], words[11]};
    const std::size_t size = bytes.size();
    if (h.structCount == 0 || !Inside(size, h.structs, std::uint64_t{h.structCount} * kStructSize) ||
        !Inside(size, h.fields, std::uint64_t{h.fieldCount} * kFieldSize) ||
        !Inside(size, h.labels, std::uint64_t{h.labelCount} * kLabelSize) || !Inside(size, h.data, h.dataSize) ||
        !Inside(size, h.indices, h.indicesSize) || !Inside(size, h.lists, h.listsSize))
    {
        return std::nullopt;
    }
    return file;
}

std::optional<Struct> File::Root() const
{
    return Struct(*this, 0);
}

Struct::Struct(const File &file, std::uint32_t index) : file_(&file), index_(index)
{
    const auto &h = file.Sections();
    const auto bytes = file.Bytes();
    if (index >= h.structCount)
    {
        return;
    }
    const std::size_t at = h.structs + (std::size_t{index} * kStructSize);
    id_ = ReadAt<std::uint32_t>(bytes, at).value_or(0);
    const std::uint32_t dataOrOffset = ReadAt<std::uint32_t>(bytes, at + 4).value_or(0);
    const std::uint32_t count = ReadAt<std::uint32_t>(bytes, at + 8).value_or(0);
    if (count > h.fieldCount)
    {
        return;
    }
    fields_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::uint32_t field = dataOrOffset;
        if (count > 1)
        {
            const std::size_t p = std::size_t{h.indices} + dataOrOffset + (std::size_t{4} * i);
            if (p + 4 > std::size_t{h.indices} + h.indicesSize)
            {
                break;
            }
            field = ReadAt<std::uint32_t>(bytes, p).value_or(kNull);
        }
        if (field >= h.fieldCount)
        {
            continue;
        }
        const std::size_t f = h.fields + (std::size_t{field} * kFieldSize);
        Field out;
        out.type = static_cast<Type>(ReadAt<std::uint32_t>(bytes, f).value_or(0));
        const std::uint32_t label = ReadAt<std::uint32_t>(bytes, f + 4).value_or(kNull);
        out.data = ReadAt<std::uint32_t>(bytes, f + 8).value_or(0);
        if (label < h.labelCount)
        {
            out.label = LabelAt(bytes, h.labels + (std::size_t{label} * kLabelSize), kLabelSize);
        }
        fields_.push_back(out);
    }
}

const Field *Struct::Find(std::string_view label) const
{
    for (const Field &field : fields_)
    {
        if (field.label == label)
        {
            return &field;
        }
    }
    return nullptr;
}

std::optional<std::size_t> Struct::Data(std::string_view label, Type type, std::size_t size) const
{
    const Field *field = Find(label);
    if (field == nullptr || field->type != type)
    {
        return std::nullopt;
    }
    const auto &h = file_->Sections();
    const std::size_t at = std::size_t{h.data} + field->data;
    const std::size_t end = std::size_t{h.data} + h.dataSize;
    if (at > end || size > end - at)
    {
        return std::nullopt;
    }
    return at;
}

std::optional<std::uint32_t> detail::Inline(const Field *field, Type type)
{
    if (field == nullptr || field->type != type || !IsInline(type))
    {
        return std::nullopt;
    }
    return field->data;
}

std::optional<std::uint64_t> detail::Wide(const File &file, const Field *field, Type type)
{
    if (field == nullptr || field->type != type)
    {
        return std::nullopt;
    }
    const auto &h = file.Sections();
    const std::size_t at = std::size_t{h.data} + field->data;
    if (at + 8 > std::size_t{h.data} + h.dataSize)
    {
        return std::nullopt;
    }
    return ReadAt<std::uint64_t>(file.Bytes(), at);
}

std::optional<std::string> Struct::String(std::string_view label) const
{
    const auto at = Data(label, Type::String, 4);
    if (!at)
    {
        return std::nullopt;
    }
    const std::uint32_t length = ReadAt<std::uint32_t>(file_->Bytes(), *at).value_or(0);
    const auto &h = file_->Sections();
    if (*at + 4 + std::size_t{length} > std::size_t{h.data} + h.dataSize)
    {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char *>(file_->Bytes().data() + *at + 4), length);
}

std::optional<std::string> Struct::ResRef(std::string_view label) const
{
    const auto at = Data(label, Type::ResRef, 1);
    if (!at)
    {
        return std::nullopt;
    }
    const auto length = static_cast<std::size_t>(file_->Bytes()[*at]);
    const auto &h = file_->Sections();
    if (*at + 1 + length > std::size_t{h.data} + h.dataSize)
    {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char *>(file_->Bytes().data() + *at + 1), length);
}

std::optional<LocString> Struct::Loc(std::string_view label) const
{
    const auto at = Data(label, Type::LocString, 12);
    if (!at)
    {
        return std::nullopt;
    }
    const auto bytes = file_->Bytes();
    const auto &h = file_->Sections();
    const std::size_t end = std::size_t{h.data} + h.dataSize;
    LocString out;
    out.strref = ReadAt<std::uint32_t>(bytes, *at + 4).value_or(kNull);
    const std::uint32_t count = ReadAt<std::uint32_t>(bytes, *at + 8).value_or(0);
    std::size_t p = *at + 12;
    for (std::uint32_t i = 0; i < count && p + 8 <= end; ++i)
    {
        const std::uint32_t id = ReadAt<std::uint32_t>(bytes, p).value_or(0);
        const std::uint32_t length = ReadAt<std::uint32_t>(bytes, p + 4).value_or(0);
        if (p + 8 + std::size_t{length} > end)
        {
            break;
        }
        out.strings.emplace_back(id, std::string(reinterpret_cast<const char *>(bytes.data() + p + 8), length));
        p += 8 + std::size_t{length};
    }
    return out;
}

std::optional<std::uint32_t> Struct::StrRef(std::string_view label) const
{
    const auto at = Data(label, Type::StrRef, 8);
    if (!at)
    {
        return std::nullopt;
    }
    return ReadAt<std::uint32_t>(file_->Bytes(), *at + 4);
}

std::vector<float> Struct::Floats(std::string_view label) const
{
    std::vector<float> out;
    const Field *field = Find(label);
    if (field == nullptr || (field->type != Type::Vector && field->type != Type::Orientation))
    {
        return out;
    }
    const std::size_t count = field->type == Type::Vector ? 3 : 4;
    const auto at = Data(label, field->type, 4 * count);
    for (std::size_t i = 0; at && i < count; ++i)
    {
        out.push_back(ReadAt<float>(file_->Bytes(), *at + (4 * i)).value_or(0.0F));
    }
    return out;
}

std::optional<std::span<const std::byte>> Struct::Void(std::string_view label) const
{
    const auto at = Data(label, Type::Void, 4);
    if (!at)
    {
        return std::nullopt;
    }
    const std::uint32_t length = ReadAt<std::uint32_t>(file_->Bytes(), *at).value_or(0);
    const auto &h = file_->Sections();
    if (*at + 4 + std::size_t{length} > std::size_t{h.data} + h.dataSize)
    {
        return std::nullopt;
    }
    return file_->Bytes().subspan(*at + 4, length);
}

std::optional<Struct> Struct::Child(std::string_view label) const
{
    const Field *field = Find(label);
    if (field == nullptr || field->type != Type::Struct || field->data >= file_->Sections().structCount)
    {
        return std::nullopt;
    }
    return Struct(*file_, field->data);
}

std::optional<std::pair<std::size_t, std::uint32_t>> Struct::Entries(std::string_view label) const
{
    const Field *field = Find(label);
    if (field == nullptr || field->type != Type::List)
    {
        return std::nullopt;
    }
    const auto &h = file_->Sections();
    const std::size_t p = std::size_t{h.lists} + field->data;
    const std::size_t end = std::size_t{h.lists} + h.listsSize;
    if (p + 4 > end)
    {
        return std::nullopt;
    }
    const std::uint32_t count = ReadAt<std::uint32_t>(file_->Bytes(), p).value_or(0);
    if (count > (end - p - 4) / 4)
    {
        return std::nullopt;
    }
    return std::pair{p + 4, count};
}

std::vector<Struct> Struct::List(std::string_view label) const
{
    std::vector<Struct> out;
    const auto entries = Entries(label);
    if (!entries)
    {
        return out;
    }
    const auto [at, count] = *entries;
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const std::uint32_t index = ReadAt<std::uint32_t>(file_->Bytes(), at + (std::size_t{4} * i)).value_or(kNull);
        if (index < file_->Sections().structCount)
        {
            out.emplace_back(*file_, index);
        }
    }
    return out;
}

std::size_t Struct::ListSize(std::string_view label) const
{
    const auto entries = Entries(label);
    return entries ? entries->second : 0;
}

// One entry read alone (DAOLib #207): a path into a list (Paragon's VarTable[7]/Name, four fields an
// entry) built the whole list again for each field it read, 0.55 s for 96 creatures' templates.
std::optional<Struct> Struct::ListItem(std::string_view label, std::size_t index) const
{
    const auto entries = Entries(label);
    if (!entries || index >= entries->second)
    {
        return std::nullopt;
    }
    const std::uint32_t item = ReadAt<std::uint32_t>(file_->Bytes(), entries->first + (4 * index)).value_or(kNull);
    if (item >= file_->Sections().structCount)
    {
        return std::nullopt;
    }
    return Struct(*file_, item);
}

} // namespace dao::core::gff3
