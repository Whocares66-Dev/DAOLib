// The running DAOrigins.exe: where it is, whether it is the build every
// address was read from, and an address of ours turned into one of its.
#pragma once

#include "dao/core/Fingerprint.h"
#include "dao/core/Relocate.h"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace dao
{

namespace Module
{

// The base the process loaded the exe at.
std::uintptr_t Base();

// Its path on disk.
std::filesystem::path Path();

// A preferred-base address from dao/Addresses.h, relocated to Base().
inline std::uintptr_t Address(std::uintptr_t preferredVa)
{
    return core::Relocate(preferredVa, Base());
}

// The fingerprint of the loaded image, read from its headers in memory.
std::optional<core::Fingerprint> Fingerprint();

// Whether that fingerprint is core::kExpected: the one build this library
// knows. Every hook and every read of the game is conditional on this.
bool IsExpectedExecutable();

} // namespace Module

// A pointer into the game at a preferred-base address.
template <class T> T *At(std::uintptr_t preferredVa)
{
    return reinterpret_cast<T *>(Module::Address(preferredVa));
}

} // namespace dao
