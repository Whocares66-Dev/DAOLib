// The directory of one of the game's archives, ERF version 2.0, over bytes:
// the .erf files of packages\core\data and the .rim files a level's rooms
// come in are both this. Read 2026-10-11, the same account as
// tools/script_commands.py's erf_entries, which reads the game's own.
//
// The file starts with "ERF V2.0" as sixteen bytes of UTF-16, then the
// number of files it holds as 32 bits, and from byte 32 an entry of 72 bytes
// for each: the file's name as 32 UTF-16 units, NUL-padded, then where its
// bytes start in the archive and how many there are. The archives are large
// (modelmeshdata.erf is 425 MiB), so a reader takes the directory alone and
// then each file it wants by its place.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace dao::core::erf
{

inline constexpr std::size_t kHeaderSize = 32;
inline constexpr std::size_t kEntrySize = 72;

// One file of an archive.
struct Entry
{
    std::wstring name; // as the archive has it, lower case for all the game's own
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

// Returns how many files the archive holds.
// @param header the archive's first kHeaderSize bytes or more
// @return nullopt if the bytes are too few or not an ERF of version 2.0
std::optional<std::uint32_t> EntryCount(std::span<const std::byte> header);

// Returns the archive's files.
// @param start the archive's bytes from its first, at least through its
// directory: kHeaderSize + EntryCount * kEntrySize of them
// @return nothing if the bytes are not an ERF of version 2.0; the entries
// the bytes hold whole, if they stop short
std::vector<Entry> Entries(std::span<const std::byte> start);

} // namespace dao::core::erf
