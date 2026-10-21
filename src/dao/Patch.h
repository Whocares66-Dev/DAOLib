// Change a few of the game's bytes in place, a table in its data or an instruction's operand,
// using dao::Patch. Where Detour replaces a function, Patch changes what a function reads.
#pragma once

#include "dao/Module.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

// Notes on Patch: the plugin says what it expects to find, so a build whose bytes differ, or
// another plugin's patch of the same bytes, is refused rather than written over (DAOLib #220).
// The write is a memcpy under VirtualProtect, then FlushInstructionCache, as the loader's own
// fixes always did (src/fix/MouseRecords.cpp, the fast load's loading-screen step).

namespace dao
{

// Bytes of the game's exe written over in place, only where they are what the plugin expects.
//
//     // The order the engine tries a model's levels of detail in, for each level asked for.
//     constexpr std::array<std::int32_t, 16> kVanilla = {0, 1, 2, 3, 1, 0, 2, 3, 2, 1, 0, 3, 3, 2, 1, 0};
//     constexpr std::array<std::int32_t, 16> kFull = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
//     dao::Patch g_order;
//     g_order.Apply({dao::addr::kModelLodOrder, 0}, kVanilla, kFull);
//
// Apply from DAOLib_Load, before the game's code runs: the bytes are written one at a time, so a
// thread reading them meanwhile can see half the change. Later, only bytes no thread is reading.
// The destructor puts the bytes back, silently, if they are still the replacement.
class Patch
{
  public:
    Patch() = default;
    Patch(const Patch &) = delete;
    Patch &operator=(const Patch &) = delete;
    ~Patch();

    // Writes `replacement` at a place in the exe, if the bytes there are `expected`.
    // @param at an ID from dao/Addresses.h, and how many bytes past its address
    // @param expected the bytes as they are in the build, any trivially copyable value: an int, a
    // std::array of bytes or of ints
    // @param replacement what to write, of the same type
    // @return true if written. False, with nothing changed and a warning logged, if this patch is
    // already applied, the running build has no such ID, the bytes there are not `expected` (a
    // build that differs, or another plugin's patch), or the memory could not be written.
    template <class T> bool Apply(Module::Place at, const T &expected, const T &replacement)
    {
        static_assert(std::is_trivially_copyable_v<T>, "a patch's bytes are a trivially copyable value");
        return ApplyBytes(at, std::as_bytes(std::span(&expected, 1)), std::as_bytes(std::span(&replacement, 1)));
    }

    // Writes `replacement` at an address, if the bytes there are `expected`: memory with no ID,
    // such as another module's. Returns as Apply does.
    template <class T> bool ApplyAt(std::uintptr_t address, const T &expected, const T &replacement)
    {
        static_assert(std::is_trivially_copyable_v<T>, "a patch's bytes are a trivially copyable value");
        return ApplyBytesAt(address, std::as_bytes(std::span(&expected, 1)), std::as_bytes(std::span(&replacement, 1)));
    }

    // Puts back the bytes the patch wrote over.
    // @return true if they are back, or nothing was applied. False, with the bytes left and a
    // warning logged, if they are no longer the replacement: something else wrote them since.
    bool Revert();

    // Returns true while the replacement is in place.
    bool IsApplied() const
    {
        return address_ != 0;
    }

  private:
    bool ApplyBytes(Module::Place at, std::span<const std::byte> expected, std::span<const std::byte> replacement);
    bool ApplyBytesAt(std::uintptr_t address, std::span<const std::byte> expected,
                      std::span<const std::byte> replacement);

    std::uintptr_t address_ = 0; // where the replacement is, or 0
    std::vector<std::byte> original_;
    std::vector<std::byte> replacement_;
};

} // namespace dao
