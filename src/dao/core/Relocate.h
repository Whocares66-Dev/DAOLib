// Addresses in dao/Addresses.h are virtual addresses as Ghidra shows them:
// the exe's preferred base, 0x00400000, plus the offset. The exe has no
// ASLR flag and loads there in practice, but it does carry a .reloc section,
// so the loader can put it elsewhere, and every address goes through here
// with the base the process actually got.
#pragma once

#include <cstdint>

namespace dao::core
{

inline constexpr std::uintptr_t kPreferredBase = 0x00400000;

constexpr std::uintptr_t Relocate(std::uintptr_t preferredVa, std::uintptr_t actualBase)
{
    return preferredVa - kPreferredBase + actualBase;
}

// The relative virtual address of a preferred-base address: what Ghidra's
// ImportNames.java and the PE headers speak.
constexpr std::uint32_t Rva(std::uintptr_t preferredVa)
{
    return static_cast<std::uint32_t>(preferredVa - kPreferredBase);
}

} // namespace dao::core
