// The script VM's stack as memory, for looking at a command's arguments
// without popping them.
//
// Read 2026-09-30 from the VM's pops (dao/Addresses.h): the stack object
// at VM+0x1738 starts with its depth; a parallel array of tags (VM+0x1744)
// and of values (VM+0x1748), one word each, index 0 the bottom. An int, a
// float and an object are the value itself; a string, a resource, an
// engine structure and an array are a pointer to it. A command's arguments
// are on top, its first argument topmost (scripts push them last first).
#pragma once

#include <cstdint>
#include <optional>

namespace dao::core
{

// The tag of each kind of value (the pops compare them): 0x10 plus an
// engine structure's number, 0x40 and up for arrays (dao/core/ScriptArray.h).
enum class VmTag : std::int32_t
{
    Int = 3,
    Float = 4,
    String = 5,
    Object = 6,
    Structure = 0x10,
    Resource = 0x60,
};

struct VmStackView
{
    std::int32_t depth = 0;
    const std::int32_t *tags = nullptr;
    const std::uint32_t *values = nullptr;

    // The index of the n-th value from the top (0: the top, a command's
    // first argument), or nothing past the bottom.
    std::optional<std::int32_t> Index(std::int32_t n) const
    {
        if (tags == nullptr || values == nullptr || n < 0 || n >= depth)
        {
            return std::nullopt;
        }
        return depth - 1 - n;
    }

    std::optional<std::int32_t> Tag(std::int32_t n) const
    {
        const auto i = Index(n);
        return i ? std::optional{tags[*i]} : std::nullopt;
    }

    // The n-th value's word, if its tag is `tag`.
    std::optional<std::uint32_t> Word(std::int32_t n, std::int32_t tag) const
    {
        const auto i = Index(n);
        if (!i || tags[*i] != tag)
        {
            return std::nullopt;
        }
        return values[*i];
    }
};

} // namespace dao::core
