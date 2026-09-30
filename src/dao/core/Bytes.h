// Bounds-checked little-endian reads out of a byte buffer: how core/ reads
// PE headers and code without touching the process. A read that would run
// past the end gives nothing rather than garbage.
#pragma once

#include <cstddef>
#include <cstring>
#include <optional>
#include <span>

namespace dao::core
{

template <class T> std::optional<T> ReadAt(std::span<const std::byte> bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < sizeof(T))
    {
        return std::nullopt;
    }
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

} // namespace dao::core
