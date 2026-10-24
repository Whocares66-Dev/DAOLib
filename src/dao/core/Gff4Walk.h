// Every value of a GFF4 file in order, handed to a visitor: each struct with its fields in its
// template's order, structs depth first, a list's elements one after another (DAOLib #106). What
// a dump of a file (tools/files' JSON), or a second reader's comparison (tools/second_reader),
// walks it with: one walk of core's reader, each a visitor of its own.
//
//   struct Count : dao::core::gff4::Visitor { ... };
//   dao::core::gff4::Walk(*file->Root(), count);
#pragma once

#include "dao/core/Gff4.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace dao::core::gff4
{

// A value the walk gives: none, a whole number (signed, or unsigned past the signed range), a
// float of either width, a string, or a talk-table string's id.
struct StrRef
{
    std::uint32_t id = 0;
    bool operator==(const StrRef &) const = default;
};
using Leaf = std::variant<std::monostate, std::int64_t, std::uint64_t, double, std::wstring, StrRef>;

class Visitor
{
  public:
    virtual ~Visitor() = default;
    // A struct begins: its template's label ("TLK", "CONV"); its fields follow, then EndStruct.
    virtual void BeginStruct(std::string_view label) = 0;
    virtual void EndStruct() = 0;
    // A field of the struct begun last, by its label; its value follows.
    virtual void Field(std::uint32_t label) = 0;
    // A list begins, of `count` elements; they follow, then EndList. A vector's or a matrix's
    // floats come as a list of floats, a generic list as its elements that are structs.
    virtual void BeginList(std::size_t count) = 0;
    virtual void EndList() = 0;
    virtual void Value(const Leaf &leaf) = 0;
    // Past the depth the walk goes to: what is there is not walked.
    virtual void TooDeep() = 0;
};

// Walks a struct and all it holds, to `depth` structs below it.
void Walk(const Struct &top, Visitor &visitor, int depth = 64);

} // namespace dao::core::gff4
