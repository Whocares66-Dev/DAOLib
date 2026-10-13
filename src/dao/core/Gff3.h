// A reader of GFF version 3.2, the toolset's templates: creatures (.utc),
// items (.uti), placeables (.utp), triggers (.utt), areas (.are) and the
// player's character (.bic), over bytes. Unlike GFF4 (dao/core/Gff4.h) a
// file names its own fields.
//
//   const auto file = dao::core::gff3::File::Parse(bytes);
//   const auto appearance = file->Root()->Child("Appearance Data")->Get<std::uint32_t>("Appearance_Type");
//
// Every read is bounds-checked: a field that is missing, of another type or
// out of the buffer gives nothing, never garbage. A File keeps a view of the
// bytes it was given, which must outlive it, and a Struct points at its File,
// which must stay where it is while the Struct is used.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace dao::core::gff3
{

enum class Type : std::uint32_t
{
    Byte = 0,
    Char = 1,
    Word = 2,
    Short = 3,
    Dword = 4,
    Int = 5,
    Dword64 = 6,
    Int64 = 7,
    Float = 8,
    Double = 9,
    String = 10,
    ResRef = 11,
    LocString = 12,
    Void = 13,
    Struct = 14,
    List = 15,
    Orientation = 16,
    Vector = 17,
    StrRef = 18,
};

// A field of a struct: its label, its type, and its value or where its value is.
struct Field
{
    std::string label;
    Type type = Type::Byte;
    std::uint32_t data = 0;
};

// A localised string: its string reference, and any strings the field holds by language id.
struct LocString
{
    std::uint32_t strref = 0xFFFFFFFF;
    std::vector<std::pair<std::uint32_t, std::string>> strings;
};

class File;

// A struct in the file: its id and its fields.
class Struct
{
  public:
    Struct(const File &file, std::uint32_t index);

    std::uint32_t Id() const
    {
        return id_;
    }
    const std::vector<Field> &Fields() const
    {
        return fields_;
    }
    // Returns the field of a label, or null.
    const Field *Find(std::string_view label) const;

    // Returns a number field as the type it has: std::uint8_t for Byte, std::int8_t for
    // Char, ..., std::uint64_t for Dword64, float, double. Nothing for a field of another type.
    template <class T> std::optional<T> Get(std::string_view label) const;
    // Returns a String field's bytes, as the file has them (8-bit text).
    std::optional<std::string> String(std::string_view label) const;
    std::optional<std::string> ResRef(std::string_view label) const;
    std::optional<LocString> Loc(std::string_view label) const;
    std::optional<std::uint32_t> StrRef(std::string_view label) const;
    // Returns a Vector's three floats or an Orientation's four; none for a field of another type.
    std::vector<float> Floats(std::string_view label) const;
    // Returns a Void field's bytes.
    std::optional<std::span<const std::byte>> Void(std::string_view label) const;
    std::optional<Struct> Child(std::string_view label) const;
    // Returns a List field's structs; none for a field of another type or a list out of the file.
    std::vector<Struct> List(std::string_view label) const;

  private:
    // Where a field's value is in the field data, if `size` bytes of it are there.
    std::optional<std::size_t> Data(std::string_view label, Type type, std::size_t size) const;

    const File *file_;
    std::uint32_t id_ = 0;
    std::vector<Field> fields_;
};

class File
{
  public:
    // Returns the file in `bytes`; nothing if they are no GFF of version 3.2 or 3.3, or its
    // sections lie outside them.
    static std::optional<File> Parse(std::span<const std::byte> bytes);

    // Returns the file's type, its first four letters without trailing blanks ("UTC").
    const std::string &FileType() const
    {
        return type_;
    }
    const std::string &Version() const
    {
        return version_;
    }
    std::optional<Struct> Root() const;

    // -- for Struct
    struct Header
    {
        std::uint32_t structs = 0, structCount = 0;
        std::uint32_t fields = 0, fieldCount = 0;
        std::uint32_t labels = 0, labelCount = 0;
        std::uint32_t data = 0, dataSize = 0;
        std::uint32_t indices = 0, indicesSize = 0;
        std::uint32_t lists = 0, listsSize = 0;
    };
    std::span<const std::byte> Bytes() const
    {
        return bytes_;
    }
    const Header &Sections() const
    {
        return header_;
    }

  private:
    std::span<const std::byte> bytes_;
    std::string type_;
    std::string version_;
    Header header_;
};

namespace detail
{
template <class T> constexpr Type TypeOf() = delete;
template <> constexpr Type TypeOf<std::uint8_t>()
{
    return Type::Byte;
}
template <> constexpr Type TypeOf<std::int8_t>()
{
    return Type::Char;
}
template <> constexpr Type TypeOf<std::uint16_t>()
{
    return Type::Word;
}
template <> constexpr Type TypeOf<std::int16_t>()
{
    return Type::Short;
}
template <> constexpr Type TypeOf<std::uint32_t>()
{
    return Type::Dword;
}
template <> constexpr Type TypeOf<std::int32_t>()
{
    return Type::Int;
}
template <> constexpr Type TypeOf<std::uint64_t>()
{
    return Type::Dword64;
}
template <> constexpr Type TypeOf<std::int64_t>()
{
    return Type::Int64;
}
template <> constexpr Type TypeOf<float>()
{
    return Type::Float;
}
template <> constexpr Type TypeOf<double>()
{
    return Type::Double;
}

// A value of up to 32 bits as the field holds it in place.
std::optional<std::uint32_t> Inline(const Field *field, Type type);
// A value of 64 bits from the field data.
std::optional<std::uint64_t> Wide(const File &file, const Field *field, Type type);
} // namespace detail

template <class T> std::optional<T> Struct::Get(std::string_view label) const
{
    constexpr Type type = detail::TypeOf<T>();
    T value{};
    if constexpr (sizeof(T) == 8)
    {
        const auto bits = detail::Wide(*file_, Find(label), type);
        if (!bits)
        {
            return std::nullopt;
        }
        static_assert(sizeof(value) == sizeof(*bits));
        std::memcpy(&value, &*bits, sizeof value);
    }
    else
    {
        const auto bits = detail::Inline(Find(label), type);
        if (!bits)
        {
            return std::nullopt;
        }
        if constexpr (std::is_same_v<T, float>)
        {
            std::memcpy(&value, &*bits, sizeof value);
        }
        else
        {
            value = static_cast<T>(*bits);
        }
    }
    return value;
}

} // namespace dao::core::gff3
