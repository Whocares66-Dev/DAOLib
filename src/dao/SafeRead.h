// Reads the game's memory without crashing on a bad address: an access violation is caught and
// reported as false or nullopt. Use these to follow pointers to objects the game may free at any
// moment, for example from another thread. They do not protect against reading a value that
// another thread is halfway through writing.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace dao
{

// Copies `size` bytes from `address` to `out`.
// @return false if any byte could not be read; `out` is then left in an unspecified state
bool TryCopy(void *out, std::uintptr_t address, std::size_t size) noexcept;

// Reads a T from `address`. T must be trivially copyable.
// @return nullopt if any byte could not be read
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
