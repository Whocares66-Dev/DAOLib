// A script array (`int[]`, `object[]`, `effect[]`, ...) as the VM holds
// it: a tag saying what it holds, and a payload shared between copies.
//
// Read 2026-09-30. On the VM's stack an array's type is its tag, 0x40 to
// 0x44 for int, float, string, object and resource arrays and 0x50 plus
// the engine structure's number for arrays of those (GetArraySize,
// 0x006608F0, accepts exactly these; the array's constructor, size and
// destructor each switch on them in the same groups). GetTeam makes an
// empty object[] with 0x43. The value is 8 bytes: the tag, then a pointer
// to the payload. The payload is 8 bytes too, a pointer to a std::vector
// (dao/core/Msvc8.h) and a pointer to a reference count: the copy
// constructor (0x00A2D260) copies both and adds one, and the destructor
// frees the vector when it takes the count from 1. Every element is 4
// bytes (the size is (last - first) / 4 for every tag, and the string
// vector's copy moves 4-byte elements): the value for int, float and
// object, a pointer for strings and structures.
#pragma once

#include "dao/core/Msvc8.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace dao::core
{

enum class ArrayTag : std::int32_t
{
    Int = 0x40,
    Float = 0x41,
    String = 0x42,
    Object = 0x43,
    Resource = 0x44,
    Structure = 0x50, // plus the engine structure's number, to 0x59
};

constexpr bool IsArrayTag(std::int32_t tag)
{
    return (tag >= 0x40 && tag <= 0x44) || (tag >= 0x50 && tag <= 0x59);
}

// The engine structure an array of structures holds, or -1.
constexpr std::int32_t ArrayStructure(std::int32_t tag)
{
    return tag >= 0x50 && tag <= 0x59 ? tag - 0x50 : -1;
}

struct ArrayRep
{
    struct Payload
    {
        VectorRep<std::uint32_t> *elements;
        std::int32_t *references;
    };

    std::int32_t tag = static_cast<std::int32_t>(ArrayTag::Int);
    Payload *payload = nullptr;

    std::span<const std::uint32_t> Elements() const
    {
        return payload == nullptr || payload->elements == nullptr ? std::span<const std::uint32_t>{}
                                                                  : payload->elements->View();
    }
};
static_assert(sizeof(ArrayRep) == 8, "the engine allocates 8 bytes for an array");

} // namespace dao::core
