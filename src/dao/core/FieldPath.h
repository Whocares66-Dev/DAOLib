// A field of an engine object, as a path of offsets from the object: {0x7EC}
// is the value at object+0x7EC, {0xC60, 0x0} the value at offset 0 of what
// the pointer at object+0xC60 points to. Pure: the memory is read through a
// callable the caller supplies, so the same path serves the game (dao/Field.h,
// through a guarded or a plain read) and the tests (a fake memory).
//
// Paths come from tools/script_fields.py, which follows the script commands
// that read and write each field (dev/SCRIPT_COMMANDS.md, "Fields").
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace dao::core
{

template <class T, std::size_t Depth> struct FieldPath
{
    static_assert(Depth >= 1, "a field is at least one offset from its object");
    std::array<std::uint32_t, Depth> offsets;

    // The value, following each pointer on the way; nothing if a read fails
    // or a pointer on the way is null. `read(address, out, size)` copies
    // size bytes from address and says whether it could.
    template <class Reader> std::optional<T> Read(std::uintptr_t object, Reader &&read) const
    {
        std::uintptr_t at = object;
        for (std::size_t i = 0; i + 1 < Depth; ++i)
        {
            std::uint32_t next = 0;
            if (!read(at + offsets[i], &next, sizeof next) || next == 0)
            {
                return std::nullopt;
            }
            at = next;
        }
        T value{};
        if (!read(at + offsets[Depth - 1], &value, sizeof value))
        {
            return std::nullopt;
        }
        return value;
    }
};

} // namespace dao::core
