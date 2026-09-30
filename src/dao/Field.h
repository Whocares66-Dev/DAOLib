// A field of an engine object at a path of offsets (dao/core/FieldPath.h),
// read two ways:
//
//   Get     as the engine reads it: plain loads, pointers on the way
//           followed without a check, as the script commands that read the
//           field do. For code holding an object the engine gave it.
//   TryGet  every load guarded (dao/SafeRead.h): nothing where the engine
//           would have faulted. For the probe, which reads objects from
//           another thread, and for anything unsure what it holds.
#pragma once

#include "dao/SafeRead.h"
#include "dao/core/FieldPath.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace dao
{

template <class T, std::size_t Depth> struct Field
{
    core::FieldPath<T, Depth> path;

    T Get(const void *object) const
    {
        auto at = reinterpret_cast<std::uintptr_t>(object);
        for (std::size_t i = 0; i + 1 < Depth; ++i)
        {
            at = *reinterpret_cast<const std::uint32_t *>(at + path.offsets[i]);
        }
        return *reinterpret_cast<const T *>(at + path.offsets[Depth - 1]);
    }

    std::optional<T> TryGet(const void *object) const
    {
        return path.Read(
            reinterpret_cast<std::uintptr_t>(object),
            [](std::uintptr_t address, void *out, std::size_t size) { return TryCopy(out, address, size); });
    }
};

} // namespace dao
