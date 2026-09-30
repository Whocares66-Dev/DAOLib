// Whether `bytes` bytes at `address` lie inside [begin, end). Written once,
// here, because the obvious form -- `address >= begin && bytes <= end -
// address` -- passes every address above `end`: the subtraction wraps to a
// huge number. That form was in dao::TypeName until 2026-09-30, and a
// command object's garbage word above the image walked straight through it
// into a crash (tests/test_range.cpp has the case).
#pragma once

#include <cstddef>
#include <cstdint>

namespace dao::core
{

constexpr bool Contains(std::uintptr_t begin, std::uintptr_t end, std::uintptr_t address, std::size_t bytes)
{
    return begin <= end && address >= begin && address <= end && bytes <= end - address;
}

} // namespace dao::core
