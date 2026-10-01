// What identifies a build of DAOrigins.exe, read from its PE headers: the
// same bytes on disk and in memory, so one reader serves a check of a file
// and the loader's check of the running process.
//
// Which builds there are is not said here. The address library
// (extern/DAOAddressLibrary) has a file per build it knows, named by the
// link stamp and holding the rest of these numbers; dao::Module finds the
// one for the running exe, and a build with none is one nothing is applied
// to.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace dao::core
{

struct Fingerprint
{
    std::uint32_t timeDateStamp = 0; // IMAGE_FILE_HEADER::TimeDateStamp, the link time
    std::uint32_t sizeOfImage = 0;   // IMAGE_OPTIONAL_HEADER::SizeOfImage
    std::uint32_t entryPointRva = 0; // IMAGE_OPTIONAL_HEADER::AddressOfEntryPoint
    std::uint32_t imageBase = 0;     // IMAGE_OPTIONAL_HEADER::ImageBase, the preferred base
    bool is32Bit = false;            // PE32 with an x86 machine, which the game is

    friend bool operator==(const Fingerprint &, const Fingerprint &) = default;
};

// The fingerprint of a PE image from its first bytes: the DOS header, the
// NT signature, the file header and the optional header. Nothing past those
// is read, so a buffer of a few hundred bytes is enough, and a buffer that
// is not a PE image, or is cut short, gives nothing rather than garbage.
std::optional<Fingerprint> ReadFingerprint(std::span<const std::byte> image);

} // namespace dao::core
