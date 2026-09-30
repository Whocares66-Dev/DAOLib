// What identifies a build of DAOrigins.exe, read from its PE headers: the
// same bytes on disk and in memory, so one reader serves tools/daoexe.py's
// check of the copy in game\ and the loader's check of the running process.
// Every address in dao/Addresses.h was read from kExpected, and nothing
// here is applied to any other build.
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

// The GOG Ultimate Edition's bin_ship\DAOrigins.exe, 1.05.13263.0, linked
// 2011-11-02, SHA-256 7e719cac...859c. The only build this library knows.
inline constexpr Fingerprint kExpected{
    .timeDateStamp = 0x4eb19d53,
    .sizeOfImage = 0x009ec0a2,
    .entryPointRva = 0x004e3024,
    .imageBase = 0x00400000,
    .is32Bit = true,
};

// The fingerprint of a PE image from its first bytes: the DOS header, the
// NT signature, the file header and the optional header. Nothing past those
// is read, so a buffer of a few hundred bytes is enough, and a buffer that
// is not a PE image, or is cut short, gives nothing rather than garbage.
std::optional<Fingerprint> ReadFingerprint(std::span<const std::byte> image);

} // namespace dao::core
