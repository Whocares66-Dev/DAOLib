// The running DAOrigins.exe: where it is, whether its build is one the
// address library has a file for, and an ID turned into an address in it.
//
// The game is sold in more than one build (extern/DAOAddressLibrary's
// dev/BUILDS.md), each with its functions and globals somewhere else. The
// library holds IDs (dao/Addresses.h), and here each is looked up in the
// file for the build that is running: daolib\addresses\daoaddresses-<link
// stamp>.bin, beside the loader. A build with no file is one the library
// does not know, and nothing is hooked or read in it. A new build of the
// game is a new file there, with neither the loader nor a plugin rebuilt.
//
// Logs, the probe's call chains and the crash logger write addresses as the
// GOG build's Ghidra listing shows them, whichever build is running, since
// that is the listing the names and the stories in the comments belong to:
// Preferred goes from a running address back to that. It needs the GOG
// build's file as well, which ships beside the others.
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

// The base the process loaded the exe at.
std::uintptr_t Base();

// Its path on disk.
std::filesystem::path Path();

// The fingerprint of the loaded image, read from its headers in memory.
std::optional<core::Fingerprint> Fingerprint();

// The address library's file for the running build, read once; null if
// there is none, or it is for another build than its name says. Every hook
// and every read of the game is conditional on there being one.
const daoaddr::Library *Library();
inline bool IsKnownExecutable()
{
    return Library() != nullptr;
}

// Where the library's files are looked for: daolib\addresses beside the
// loader, which for a plugin in daolib\plugins is the folder next to its
// own. The first of the candidates that holds the running build's file.
std::filesystem::path AddressFolder();

// An ID from dao/Addresses.h as an address in the running process. Zero
// where the running build has no such thing, or its build is not known: At
// gives a null pointer for it and Detour::Attach refuses it.
std::uintptr_t Address(AddressId id);

// What the address library notes about an ID in the running build: whether
// its code differs from the build it was read in, or it shares its address
// with other IDs. Nothing where the build lacks it.
std::optional<daoaddr::Entry> Find(AddressId id);

// The exe's code, its .text section, as runtime addresses [begin, end).
// Nothing if the section table cannot be read.
struct CodeRange
{
    std::uintptr_t begin = 0;
    std::uintptr_t end = 0;
    bool Contains(std::uintptr_t address) const
    {
        return address >= begin && address < end;
    }
};
std::optional<CodeRange> Code();

// A runtime address in the exe as the GOG build's listing shows it: what
// Ghidra, the logs and the last argument of a row in dao/Addresses.h
// write, whichever build is running. In another build it is the ID at or
// below the address, and as far past that ID's place in GOG's build; zero
// for an address nothing of GOG's corresponds to, or with no file for GOG's
// build to look in.
std::uint32_t Preferred(std::uintptr_t runtime);
// The same for an ID: where the listing has it. Zero if GOG's build lacks it.
std::uint32_t Preferred(AddressId id);

// The running build's own preferred-base address of a runtime address: what
// a listing of that build shows. Preferred's answer when GOG's is running.
inline std::uint32_t ImageAddress(std::uintptr_t runtime)
{
    return static_cast<std::uint32_t>(runtime - Base() + core::kPreferredBase);
}

} // namespace Module

// A pointer into the game at an ID from dao/Addresses.h.
template <class T> T *At(AddressId id)
{
    return reinterpret_cast<T *>(Module::Address(id));
}

} // namespace dao
