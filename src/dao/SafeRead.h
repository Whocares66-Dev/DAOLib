// Reading the game's memory from a thread that does not own it: the probe's
// sampler reads engine objects while the game runs on, and a pointer read
// a moment ago can be freed by the time it is followed. These reads catch
// the access violation (SEH) and report failure instead of crashing the
// game. They do nothing about a torn value, only about a fault.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace dao
{

// Copies `size` bytes from `address` to `out`. False, with `out` in an
// unspecified state, if any byte could not be read.
bool TryCopy(void *out, std::uintptr_t address, std::size_t size) noexcept;

template <class T> std::optional<T> TryRead(std::uintptr_t address) noexcept
{
    T value{};
    if (!TryCopy(&value, address, sizeof value))
    {
        return std::nullopt;
    }
    return value;
}

} // namespace dao
