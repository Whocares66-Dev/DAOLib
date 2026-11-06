// A GFF4 file written from nothing, laid out byte for byte as the game's own writer lays one out
// (DAOLib #326): the saved hero (.chr, dao/core/Hero.h) first. Templates are added as the game
// adds them, then the structs are filled field by field; the data block's layout follows from the
// order of the calls, so a caller makes them in the game's order.
//
//   gff4::Writer out("CHAR", "V0.1");
//   const auto name = out.AddTemplate("ABLT", {{0x3D0FE, Type::U32}, {0x3D0FF, Type::S32}});
//   const gff4::StructAt root = out.Root("TOP ", {{0x3D0FD, Type::String}});
//   out.String(root, 0x3D0FD, L"EF_CPS_P01.mop");
//   const std::vector<std::byte> bytes = out.Save();
#pragma once

#include "dao/core/Gff4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::gff4
{

// A template's field, as it is added: its label, its type (or a template's index, with kStruct)
// and its flags.
struct FieldSpec
{
    std::uint32_t label = 0;
    std::uint16_t type = 0;
    std::uint16_t flags = 0;

    FieldSpec(std::uint32_t label, Type type, std::uint16_t flags = 0)
        : label(label), type(static_cast<std::uint16_t>(type)), flags(flags)
    {
    }
    // A field holding a struct of the template at `index`, or a list of them with kList.
    FieldSpec(std::uint32_t label, std::uint16_t index, std::uint16_t flags)
        : label(label), type(index), flags(static_cast<std::uint16_t>(flags | kStruct))
    {
    }
};

// A struct being written: where it is in the data block and its template.
struct StructAt
{
    std::size_t at = 0;
    std::uint16_t shape = 0;
};

class Writer
{
  public:
    // A file of the type and version given, four letters each ("CHAR", "V0.1"), for the PC.
    Writer(std::string_view type, std::string_view version);

    // Adds a template, or finds the same one added before, and returns its index. Index 0 is kept
    // for the root's, which Root adds.
    std::uint16_t AddTemplate(std::string_view label, const std::vector<FieldSpec> &fields);
    // Adds the root's template as index 0 and places the root struct at the data's start.
    // @return the root
    //
    // Call it once, after the templates its fields hold and before anything is written.
    StructAt Root(std::string_view label, const std::vector<FieldSpec> &fields);

    // Returns the struct a struct field holds in place.
    StructAt Child(const StructAt &in, std::uint32_t label) const;

    void U8(const StructAt &in, std::uint32_t label, std::uint8_t value);
    void U32(const StructAt &in, std::uint32_t label, std::uint32_t value);
    void S32(const StructAt &in, std::uint32_t label, std::int32_t value);
    void F32(const StructAt &in, std::uint32_t label, float value);
    void Vector4(const StructAt &in, std::uint32_t label, const std::array<float, 4> &value);
    // Writes a string, or one written before in the same file again; an empty string is none.
    void String(const StructAt &in, std::uint32_t label, std::wstring_view value);
    // Writes a list of strings, the list before its strings; an empty list is none, an empty
    // element none.
    void Strings(const StructAt &in, std::uint32_t label, const std::vector<std::wstring> &values);
    void Numbers(const StructAt &in, std::uint32_t label, std::span<const std::int32_t> values);
    void Floats(const StructAt &in, std::uint32_t label, std::span<const float> values);
    // Writes a list of structs of the field's template and returns its elements, to be filled
    // in order; no list for a count of 0.
    std::vector<StructAt> Structs(const StructAt &in, std::uint32_t label, std::size_t count);

    // Returns the file: its header, templates and data.
    std::vector<std::byte> Save() const;

  private:
    struct Laid
    {
        FieldSpec spec;
        std::uint32_t offset = 0;
    };
    struct Shape
    {
        std::string label;
        std::vector<Laid> fields; // in the order added
        std::uint32_t size = 0;
        std::uint32_t align = 1;
    };

    Shape Lay(std::string_view label, const std::vector<FieldSpec> &fields) const;
    const Laid &FieldOf(const StructAt &in, std::uint32_t label) const;
    std::size_t Alloc(std::size_t size, std::size_t align, bool list, std::size_t count);
    std::uint32_t StoreString(std::wstring_view value);
    std::size_t ScalarList(const StructAt &in, std::uint32_t label, std::size_t count);
    template <class T> void Put(std::size_t at, T value);

    std::string type_;
    std::string version_;
    std::vector<Shape> shapes_;
    std::vector<std::byte> data_;
    std::size_t maxAlign_ = 1;
    std::map<std::wstring, std::uint32_t, std::less<>> strings_;
};

} // namespace dao::core::gff4
