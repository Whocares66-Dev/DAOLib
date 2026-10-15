// A GFF3 file changed field by field (DAOLib #123). Gff3.cpp's notes have
// the layout; the sections are kept apart here and laid out again in the
// order the game's templates have them: header, structs, fields, labels,
// field data, field indices, list indices.
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

std::vector<std::byte> Section(std::span<const std::byte> bytes, std::uint32_t at, std::size_t size)
{
    const auto from = bytes.begin() + static_cast<std::ptrdiff_t>(at);
    return {from, from + static_cast<std::ptrdiff_t>(size)};
}

template <class T> void Put(std::vector<std::byte> &out, std::size_t at, T value)
{
    std::memcpy(out.data() + at, &value, sizeof(T));
}

template <class T> void Push(std::vector<std::byte> &out, T value)
{
    out.resize(out.size() + sizeof(T));
    Put(out, out.size() - sizeof(T), value);
}

// A number as a field of 32 bits or fewer keeps it: at its type's width, then widened to 32 bits.
std::optional<std::uint32_t> Narrowed(Type type, std::int64_t value)
{
    switch (type)
    {
    case Type::Byte:
        return static_cast<std::uint8_t>(value);
    case Type::Char:
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value)));
    case Type::Word:
        return static_cast<std::uint16_t>(value);
    case Type::Short:
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
    case Type::Dword:
    case Type::Int:
        return static_cast<std::uint32_t>(value);
    default:
        return std::nullopt;
    }
}

} // namespace

Editor::Editor(const File &file)
{
    const auto bytes = file.Bytes();
    const auto &h = file.Sections();
    std::memcpy(head_.data(), bytes.data(), head_.size());
    structs_ = Section(bytes, h.structs, std::size_t{h.structCount} * kStructSize);
    fields_ = Section(bytes, h.fields, std::size_t{h.fieldCount} * kFieldSize);
    labels_ = Section(bytes, h.labels, std::size_t{h.labelCount} * kLabelSize);
    data_ = Section(bytes, h.data, h.dataSize);
    indices_ = Section(bytes, h.indices, h.indicesSize);
    lists_ = Section(bytes, h.lists, h.listsSize);
}

std::optional<std::size_t> Editor::FieldOf(const Struct &in, std::string_view label, Type &type) const
{
    const std::size_t at = std::size_t{in.Index()} * kStructSize;
    const auto first = ReadAt<std::uint32_t>(structs_, at + 4);
    const auto count = ReadAt<std::uint32_t>(structs_, at + 8);
    if (!first || !count)
    {
        return std::nullopt;
    }
    for (std::uint32_t i = 0; i < *count; ++i)
    {
        const auto field = *count == 1 ? first : ReadAt<std::uint32_t>(indices_, *first + (std::size_t{4} * i));
        const std::size_t f = std::size_t{field.value_or(0xFFFFFFFF)} * kFieldSize;
        const auto name = ReadAt<std::uint32_t>(fields_, f + 4);
        if (!field || f + kFieldSize > fields_.size() || !name || (*name + 1) * kLabelSize > labels_.size())
        {
            continue;
        }
        std::string text;
        for (std::size_t c = 0; c < kLabelSize; ++c)
        {
            const auto letter = static_cast<char>(labels_[(*name * kLabelSize) + c]);
            if (letter == '\0')
            {
                break;
            }
            text += letter;
        }
        if (text == label)
        {
            type = static_cast<Type>(ReadAt<std::uint32_t>(fields_, f).value_or(0));
            return f;
        }
    }
    return std::nullopt;
}

std::uint32_t Editor::AppendData(std::span<const std::byte> bytes)
{
    const auto at = static_cast<std::uint32_t>(data_.size());
    data_.insert(data_.end(), bytes.begin(), bytes.end());
    return at;
}

bool Editor::SetNumber(const Struct &in, std::string_view label, std::int64_t value)
{
    Type type = Type::Byte;
    const auto field = FieldOf(in, label, type);
    if (!field)
    {
        return false;
    }
    if (type == Type::Dword64 || type == Type::Int64)
    {
        const std::uint32_t at = ReadAt<std::uint32_t>(fields_, *field + 8).value_or(0);
        if (std::size_t{at} + 8 > data_.size())
        {
            return false;
        }
        Put(data_, at, value);
        return true;
    }
    const auto narrowed = Narrowed(type, value);
    if (narrowed)
    {
        Put(fields_, *field + 8, *narrowed);
    }
    return narrowed.has_value();
}

bool Editor::SetFloat(const Struct &in, std::string_view label, double value)
{
    Type type = Type::Byte;
    const auto field = FieldOf(in, label, type);
    if (field && type == Type::Float)
    {
        Put(fields_, *field + 8, static_cast<float>(value));
        return true;
    }
    if (field && type == Type::Double)
    {
        const std::uint32_t at = ReadAt<std::uint32_t>(fields_, *field + 8).value_or(0);
        if (std::size_t{at} + 8 <= data_.size())
        {
            Put(data_, at, value);
            return true;
        }
    }
    return false;
}

bool Editor::SetString(const Struct &in, std::string_view label, std::string_view text)
{
    Type type = Type::Byte;
    const auto field = FieldOf(in, label, type);
    if (!field || (type != Type::String && type != Type::ResRef) || (type == Type::ResRef && text.size() > 255))
    {
        return false;
    }
    std::vector<std::byte> bytes;
    if (type == Type::String)
    {
        Push(bytes, static_cast<std::uint32_t>(text.size()));
    }
    else
    {
        Push(bytes, static_cast<std::uint8_t>(text.size()));
    }
    for (const char c : text)
    {
        Push(bytes, c);
    }
    Put(fields_, *field + 8, AppendData(bytes));
    return true;
}

bool Editor::SetLoc(const Struct &in, std::string_view label, const LocString &value)
{
    Type type = Type::Byte;
    const auto field = FieldOf(in, label, type);
    if (!field || type != Type::LocString)
    {
        return false;
    }
    // Its size after the size, its string reference, its count, then each string's language, length and text.
    std::vector<std::byte> bytes(4);
    Push(bytes, value.strref);
    Push(bytes, static_cast<std::uint32_t>(value.strings.size()));
    for (const auto &[language, text] : value.strings)
    {
        Push(bytes, language);
        Push(bytes, static_cast<std::uint32_t>(text.size()));
        for (const char c : text)
        {
            Push(bytes, c);
        }
    }
    Put(bytes, 0, static_cast<std::uint32_t>(bytes.size() - 4));
    Put(fields_, *field + 8, AppendData(bytes));
    return true;
}

std::vector<std::byte> Editor::Bytes() const
{
    std::vector<std::byte> out(kHeaderSize);
    std::memcpy(out.data(), head_.data(), head_.size());
    const std::array<const std::vector<std::byte> *, 6> sections{&structs_, &fields_,  &labels_,
                                                                 &data_,    &indices_, &lists_};
    const std::array<std::size_t, 6> counts{structs_.size() / kStructSize,
                                            fields_.size() / kFieldSize,
                                            labels_.size() / kLabelSize,
                                            data_.size(),
                                            indices_.size(),
                                            lists_.size()};
    for (std::size_t i = 0; i < sections.size(); ++i)
    {
        Put(out, 8 + (8 * i), static_cast<std::uint32_t>(out.size()));
        Put(out, 12 + (8 * i), static_cast<std::uint32_t>(counts.at(i)));
        out.insert(out.end(), sections.at(i)->begin(), sections.at(i)->end());
    }
    return out;
}

} // namespace dao::core::gff3
