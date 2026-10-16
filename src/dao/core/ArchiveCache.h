// Archives' directories kept on disk between builds of an install's index
// (dao/core/Install.h, Options::cache): each archive's directory as the index
// read it, with what says whether the archive has changed since. One file for
// all of them, since ten thousand small files would cost their own reads.
#pragma once

#include "dao/core/Erf.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dao::core::install
{

// One archive's directory, as kept: only an archive that opened, since a file that did not
// may have been one that could not be read just then.
struct KeptDirectory
{
    std::string path;         // the archive's, absolute, UTF-8
    std::uint64_t size = 0;   // its bytes when read
    std::int64_t written = 0; // its last-write time when read, in the file system's ticks
    erf::Header header;
    std::vector<std::pair<std::string, std::uint32_t>> files; // each name in lower case, and size

    bool operator==(const KeptDirectory &) const = default;
};

// The kept directories by their archives' absolute paths.
using KeptByPath = std::unordered_map<std::string, KeptDirectory>;

// Returns the bytes of a cache that holds `directories`.
std::vector<std::byte> WriteKept(std::span<const KeptDirectory> directories);

// Returns the directories a cache's bytes hold.
// @return nullopt for bytes of another version of the format, or that do not read
std::optional<std::vector<KeptDirectory>> ReadKept(std::span<const std::byte> bytes);

// Returns the directories kept in a cache folder: none for an empty path, or a folder whose
// file is missing, of another version, or does not read.
KeptByPath LoadKept(const std::filesystem::path &folder);

// Keeps directories in a cache folder, made if it is not there, in place of what it kept.
// Written beside the file and moved over it, so a build stopped half way, or another writing at
// the same time, leaves a whole file: the old one or a new one. A failure leaves the old one.
void SaveKept(const std::filesystem::path &folder, std::span<const KeptDirectory> keep);

} // namespace dao::core::install
