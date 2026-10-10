// Reads a field of an engine object by following a path of offsets from the object
// (dao/core/FieldPath.h). The generated field descriptors in dao/generated are all of this type:
// read one with Get, or with TryGet when you are not sure the object is valid.
#pragma once

#include "dao/SafeRead.h"
#include "dao/core/FieldPath.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace dao
{

// A field of type T, reached from an object through a path of Depth offsets.
template <class T, std::size_t Depth> struct Field
{
    core::FieldPath<T, Depth> path; // the offsets; each one but the last leads to a pointer

    // Reads the field with plain loads, the way the engine's own code does, and does not check
    // the pointers it follows. Use it on an object the engine has just handed you. A bad object
    // crashes the game.
    T Get(const void *object) const
    {
        auto at = reinterpret_cast<std::uintptr_t>(object);
        for (std::size_t i = 0; i + 1 < Depth; ++i)
        {
            at = *reinterpret_cast<const std::uint32_t *>(at + path.offsets[i]);
        }
        return *reinterpret_cast<const T *>(at + path.offsets[Depth - 1]);
    }

    // Reads the field with every load guarded (dao/SafeRead.h), so a bad pointer returns nullopt
    // instead of crashing. Use it off the game thread, or on an object you are not sure of.
    // @return the value, or nullopt if a plain read would have faulted
    std::optional<T> TryGet(const void *object) const
    {
        return path.Read(
            reinterpret_cast<std::uintptr_t>(object),
            [](std::uintptr_t address, void *out, std::size_t size) { return TryCopy(out, address, size); });
    }
};

} // namespace dao
