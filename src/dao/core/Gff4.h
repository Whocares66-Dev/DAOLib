// A reader of GFF version 4, the game's own file format for its
// conversations, data tables, talk tables and saves, over bytes: what
// dao/Dialogue.h reads a conversation with while the engine plays it.
//
//   const auto file = dao::core::gff4::File::Parse(bytes);
//   for (const auto &line : file->Root().Structs(12002)) ...
//
// Read 2026-09-30, the same account of the format as tools/gff4.py, which
// was checked against the game's files, with xoreos's reader
// (src/aurora/gff4file.cpp) as the account of it. A file is a header ("GFF
// V4.0", the platform "PC  ", its type such as "CONV" and the type's
// version), a table of struct templates, and a data block the structs sit
// in. A template is a four-letter label, a size and its fields; a field is
// a numeric label (dev/gff4_fields.csv names them), a type, flags and an
// offset in the struct. The flags say a field is a list (its slot holds an
// offset to a count and the elements), a struct (its type is then the
// template's index) or a reference (its slot, or each element, holds an
// offset into the data instead of the value). A generic field's elements
// are 8 bytes, a type and flags then a value or an offset, so one list can
// hold structs of several templates, as a conversation's lines do.
//
// Every read is bounds-checked: a field that is missing, of another type
// or out of the buffer gives nothing, never garbage. A File keeps a view of
// the bytes it was given, which must outlive it, and a Struct points at
// its File, which must stay where it is while the Struct is used.
#pragma once

#include "dao/core/Bytes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::gff4
{

enum class Type : std::uint16_t
{
    U8 = 0,
    S8 = 1,
    U16 = 2,
    S16 = 3,
    U32 = 4,
    S32 = 5,
    U64 = 6,
    S64 = 7,
    F32 = 8,
    F64 = 9,
    Vector3 = 10,
    Vector4 = 12,
    Quaternion = 13,
    String = 14,
    Color = 15,
    Matrix = 16,
    TlkString = 17,
    Generic = 0xFFFF,
};

inline constexpr std::uint16_t kList = 0x8000;
inline constexpr std::uint16_t kStruct = 0x4000;
inline constexpr std::uint16_t kReference = 0x2000;

struct Field
{
    std::uint32_t label = 0;
    std::uint16_t type = 0; // a Type, or a template's index for a struct
    std::uint16_t flags = 0;
    std::uint32_t offset = 0;
};

struct Template
{
    std::string label; // four letters, trailing spaces and NULs dropped
    std::uint32_t size = 0;
    std::vector<Field> fields;

    const Field *Find(std::uint32_t wanted) const;
};

// A talk-table string as a field holds it: the string reference, and an
// offset to a string of its own that the game's files leave at 0.
struct TlkString
{
    std::uint32_t strref = 0xFFFFFFFF;
    std::uint32_t text = 0;
};

class File;

// A struct in the file: its template and where it sits.
class Struct
{
  public:
    Struct(const File &file, std::size_t index, std::size_t at) : file_(&file), template_(index), at_(at)
    {
    }

    const std::string &Label() const;
    const Template &Shape() const;

    // A scalar field: U8 to S64 and F32, F64 as the type the field has. Nothing
    // for a field of another type, even a wider one.
    template <class T> std::optional<T> Get(std::uint32_t label) const;
    std::optional<std::wstring> String(std::uint32_t label) const;
    std::optional<TlkString> Tlk(std::uint32_t label) const;

    // A struct field, held in place or by reference.
    std::optional<Struct> Child(std::uint32_t label) const;
    // A list of structs: of one template, in place or by reference, or a
    // generic list whose elements are structs of any template.
    std::vector<Struct> Structs(std::uint32_t label) const;
    // A list of U32 or S32 values.
    std::vector<std::uint32_t> Numbers(std::uint32_t label) const;

  private:
    std::optional<std::size_t> Slot(std::uint32_t label, Type type) const;

    const File *file_;
    std::size_t template_;
    std::size_t at_;
};

class File
{
  public:
    // The file in `bytes`, or nothing if it is not a GFF4 file this reads:
    // version 4.0 or 4.1 without shared strings, for the PC.
    static std::optional<File> Parse(std::span<const std::byte> bytes);

    std::string_view Type() const
    {
        return type_;
    }
    std::string_view TypeVersion() const
    {
        return typeVersion_;
    }
    const std::vector<Template> &Templates() const
    {
        return templates_;
    }
    // The top struct, of the first template at the start of the data.
    std::optional<Struct> Root() const;

    // -- for Struct
    std::span<const std::byte> Bytes() const
    {
        return bytes_;
    }
    std::size_t DataOffset() const
    {
        return data_;
    }
    std::optional<std::uint32_t> U32(std::size_t at) const;
    // The string whose data offset is at `at`: a count of UTF-16 units,
    // then the units, the last a NUL that is dropped.
    std::optional<std::wstring> StringAt(std::size_t at) const;

  private:
    std::span<const std::byte> bytes_;
    std::string type_;
    std::string typeVersion_;
    std::size_t data_ = 0;
    std::vector<Template> templates_;
};

namespace detail
{
template <class T> constexpr Type TypeOf() = delete;
template <> constexpr Type TypeOf<std::uint8_t>()
{
    return Type::U8;
}
template <> constexpr Type TypeOf<std::int8_t>()
{
    return Type::S8;
}
template <> constexpr Type TypeOf<std::uint16_t>()
{
    return Type::U16;
}
template <> constexpr Type TypeOf<std::int16_t>()
{
    return Type::S16;
}
template <> constexpr Type TypeOf<std::uint32_t>()
{
    return Type::U32;
}
template <> constexpr Type TypeOf<std::int32_t>()
{
    return Type::S32;
}
template <> constexpr Type TypeOf<std::uint64_t>()
{
    return Type::U64;
}
template <> constexpr Type TypeOf<std::int64_t>()
{
    return Type::S64;
}
template <> constexpr Type TypeOf<float>()
{
    return Type::F32;
}
template <> constexpr Type TypeOf<double>()
{
    return Type::F64;
}
} // namespace detail

template <class T> std::optional<T> Struct::Get(std::uint32_t label) const
{
    const auto slot = Slot(label, detail::TypeOf<T>());
    return slot ? ReadAt<T>(file_->Bytes(), *slot) : std::nullopt;
}

} // namespace dao::core::gff4
