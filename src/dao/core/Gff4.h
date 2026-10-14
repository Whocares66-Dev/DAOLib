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
    // A list of strings: a campaign's CampaignCIFPackagesList. Empty for a field that is no such
    // list; a string that does not read stops the list there.
    std::vector<std::wstring> Strings(std::uint32_t label) const;
    // The floats of a field that is made of them: one for an F32, three for
    // a Vector3, four for a Vector4, a Quaternion or a Color, sixteen for a
    // Matrix. None for a field of another type.
    std::vector<float> Floats(std::uint32_t label) const;
    // The elements of a list of U8, U16, U32 or F32 as the file has them,
    // little-endian and one after another: a mesh's vertices and indices
    // are lists of bytes and of 16-bit numbers a megabyte long. Empty for a
    // field that is no such list or runs out of the file.
    std::span<const std::byte> ListBytes(std::uint32_t label, Type element) const;

    // Where the struct is in the file's bytes.
    std::size_t Offset() const
    {
        return at_;
    }
    // Its template's index in the file's templates: a generic list's element names it.
    std::size_t Index() const
    {
        return template_;
    }

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

// Where a list's new elements go: over the old ones when they fit, or at the
// file's end. A list two fields share would change for both in place; the
// meshes' vertex and index buffers are each one field's.
enum class Place : std::uint8_t
{
    InPlaceIfFits,
    Append,
};

// A GFF4 file changed field by field: a copy of its bytes in which a value is
// written over its slot, and a string or a list that does not fit where it
// was is appended to the data and its slot pointed at it. Nothing already in
// the file moves, so every Struct of the File it was made from still points
// at its own struct, and a file nothing was set in is the same bytes.
class Editor
{
  public:
    explicit Editor(const File &file);

    // Writes a whole number over a scalar field, at the field's own width and
    // sign. False for a field that is no whole number, a reference, or none.
    bool SetNumber(const Struct &in, std::uint32_t label, std::int64_t value);
    // Writes the floats of a field made of them: one for an F32, three for a
    // Vector3, four for a Vector4, Quaternion or Color, sixteen for a Matrix.
    // False for another count or type.
    bool SetFloats(const Struct &in, std::uint32_t label, std::span<const float> values);
    // Appends a string and points the field at it.
    bool SetString(const Struct &in, std::uint32_t label, std::wstring_view text);
    // Sets a list field's elements: `count` of them, `elements` their bytes
    // as the list keeps them (a list of structs in place, its structs').
    bool SetList(const Struct &in, std::uint32_t label, std::span<const std::byte> elements, std::uint32_t count,
                 Place place);
    // Adds `count` elements after a list's own, `elements` their bytes as the
    // list keeps them (4 a reference, 8 a generic element). The whole list is
    // appended to the data and the field pointed at it; the old one is left.
    bool Extend(const Struct &in, std::uint32_t label, std::span<const std::byte> elements, std::uint32_t count);
    // Appends a copy of a struct, its references still pointing where its
    // own do. Returns the copy, for this Editor's Set calls: the File reads
    // nothing of it. Nothing for a struct that runs out of the bytes.
    std::optional<Struct> Add(const Struct &like);
    // Returns a struct's offset from the start of the data, which a
    // reference to it holds.
    std::uint32_t Reference(const Struct &to) const
    {
        return static_cast<std::uint32_t>(to.Offset() - data_);
    }

    const std::vector<std::byte> &Bytes() const
    {
        return bytes_;
    }

  private:
    std::optional<std::size_t> Slot(const Struct &in, std::uint32_t label, std::uint16_t &type) const;
    std::uint32_t Append(std::span<const std::byte> data);

    const File *file_;
    std::size_t data_;
    std::vector<std::byte> bytes_;
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
