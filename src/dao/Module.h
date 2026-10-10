// The running DAOrigins.exe and the address library's file for its build. Use it to turn an ID
// from dao/Addresses.h into an address, to check that the build is a known one before hooking
// anything, and to turn a runtime address back into the GOG build's address for a log.
#pragma once

#include "dao/Addresses.h"
#include "dao/core/Fingerprint.h"
#include "dao/core/Relocate.h"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace dao
{

namespace Module
{

// Returns the address the exe is loaded at.
std::uintptr_t Base();

// Returns the full path of the exe on disk.
std::filesystem::path Path();

// Reads the fingerprint of the loaded exe from its headers in memory.
// @return nullopt if the headers cannot be read
std::optional<core::Fingerprint> Fingerprint();

// Returns the four header fields the address library uses to identify a build: link stamp,
// image size, entry point and base.
// @return nullopt if the headers cannot be read or the exe is not 32-bit
std::optional<daoaddr::BuildInfo> Build();

// Returns the address library's file for the running build. It is loaded on the first call.
// @return null if there is no file for this build, or the file's header names a different build
const daoaddr::Library *Library();

// Returns whether the address library has a file for the running build. Check it before hooking
// or reading anything in the game.
inline bool IsKnownExecutable()
{
    return Library() != nullptr;
}

// Returns the folder the running build's address file was found in.
// @return an empty path if none was found
std::filesystem::path AddressFolder();

// Returns the folder of the DLL this copy of the library is linked into: a plugin's own folder
// (daolib\plugins\<name>), or the loader's.
// @return an empty path if the DLL cannot be found
std::filesystem::path OwnFolder();

// Turns an ID from dao/Addresses.h into an address in the running process.
// @return 0 if the build is unknown or has no such ID; At() then returns null, and
// Detour::Attach refuses it
std::uintptr_t Address(AddressId id);

// Returns what the address library records about an ID in the running build: whether its code
// differs from the GOG build's, and whether other IDs share its address.
// @return nullopt if the build is unknown or has no such ID
std::optional<daoaddr::Entry> Find(AddressId id);

// The exe's .text section in runtime addresses, as the half-open range [begin, end).
struct CodeRange
{
    std::uintptr_t begin = 0;
    std::uintptr_t end = 0;
    // Returns whether `address` is in the range.
    bool Contains(std::uintptr_t address) const
    {
        return address >= begin && address < end;
    }
};

// Returns the exe's code section.
// @return nullopt if the section table cannot be read
std::optional<CodeRange> Code();

// Turns a runtime address in the exe into the matching address in the GOG build, the one that
// Ghidra, the logs and dao/Addresses.h use, whichever build is running.
// @return 0 if nothing in the GOG build matches the address, or the GOG build's address file is
// missing. In the GOG build, or a build the library does not know, it returns
// ImageAddress(runtime).
std::uint32_t Preferred(std::uintptr_t runtime);

// Returns an ID's address in the GOG build.
// @return 0 if the GOG build's address file is missing or has no such ID
std::uint32_t Preferred(AddressId id);

// A place in the exe's code: an ID, and how many bytes past the ID's address.
struct Place
{
    AddressId id{};
    std::uint32_t offset = 0; // bytes past the ID's address
};

// Finds the ID at or below a runtime address in the exe, and how far past it the address is.
// Given a hook's _ReturnAddress(), this names the calling function by an ID that is the same in
// every build:
//
//   const auto caller = dao::Module::Containing(reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
//   if (caller && caller->id == kTheScriptsAddCommand) ...
//
// @return nullopt if the build is unknown, or the address is outside the exe, before its first
// ID, or farther past the nearest ID than any function is long
//
// The offset is measured in the running build. It can differ from the GOG build's in a function
// the library marks as changed.
std::optional<Place> Containing(std::uintptr_t runtime);

// Turns a runtime address into the running build's own address, relative to the preferred base.
// In the GOG build this is the same as Preferred.
inline std::uint32_t ImageAddress(std::uintptr_t runtime)
{
    return static_cast<std::uint32_t>(runtime - Base() + core::kPreferredBase);
}

} // namespace Module

// Returns a pointer into the game for an ID from dao/Addresses.h, or null if the running build
// does not have it.
template <class T> T *At(AddressId id)
{
    return reinterpret_cast<T *>(Module::Address(id));
}

} // namespace dao
