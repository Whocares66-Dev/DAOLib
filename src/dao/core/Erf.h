// The game's resource archives, ERF versions 2.0 and 2.2: an archive's
// directory over bytes, and an archive on disk with any file of it read by
// name. The .erf files of packages\core\data and the .rim files a level's
// rooms come in are version 2.0; the add-ins' .erf files are 2.2, whose
// files are enciphered (DAOLib #106).
//
//   const auto archive = dao::core::erf::Archive::Open(path);
//   const auto bytes = archive->Read(L"abi_base.gda");
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dao::core::erf
{

// Version 2.0's header and entry sizes.
inline constexpr std::size_t kHeaderSize = 32;
inline constexpr std::size_t kEntrySize = 72;
// Version 2.2's.
inline constexpr std::size_t kHeaderSize22 = 56;
inline constexpr std::size_t kEntrySize22 = 76;

// One file of an archive.
struct Entry
{
    std::wstring name; // as the archive has it, lower case for all the game's own
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    // How many bytes it takes in the archive: `size`, but in a compressed archive.
    std::uint32_t packed = 0;
};

enum class Version
{
    V20,
    V22,
};

// What an archive's header says.
struct Header
{
    Version version = Version::V20;
    std::uint32_t count = 0;
    std::uint32_t flags = 0; // version 2.2's; 0 for 2.0

    // How its files are enciphered: 0 not at all, 2 the game's Blowfish.
    std::uint32_t Encryption() const
    {
        return (flags >> 4) & 0xF;
    }
    // How its files are compressed: 0 not at all.
    std::uint32_t Compression() const
    {
        return (flags >> 29) & 0x7;
    }
    // How many bytes from the archive's first hold its header and directory.
    std::size_t DirectorySize() const;
};

// Returns what an archive's header says.
// @param start the archive's first bytes: kHeaderSize of them for version 2.0, kHeaderSize22
// for 2.2
// @return nullopt if the bytes are too few or not an ERF of version 2.0 or 2.2
std::optional<Header> ReadHeader(std::span<const std::byte> start);

// Returns how many files an archive of version 2.0 holds.
// @param header the archive's first kHeaderSize bytes or more
// @return nullopt if the bytes are too few or not an ERF of version 2.0; ReadHeader reads
// either version
std::optional<std::uint32_t> EntryCount(std::span<const std::byte> header);

// Returns the archive's files, in its order.
// @param start the archive's bytes from its first, at least through its directory
// (Header::DirectorySize)
// @return nothing if the bytes are not an ERF of version 2.0 or 2.2; the entries the bytes
// hold whole, if they stop short
std::vector<Entry> Entries(std::span<const std::byte> start);

// An archive on disk: its directory, read once, and any file of it by name.
class Archive
{
  public:
    // Returns the archive at a path, its directory read.
    // @param held keep the file open from the first read on, for an archive read from
    // often; reads of a held archive take turns, and its copies share the file
    // @return nullopt if the file cannot be read or is no ERF of version 2.0 or 2.2
    static std::optional<Archive> Open(const std::filesystem::path &path, bool held = false);

    const std::filesystem::path &Path() const
    {
        return path_;
    }
    const Header &Head() const
    {
        return header_;
    }
    // Returns the archive's files, in its order.
    const std::vector<Entry> &Entries() const
    {
        return entries_;
    }
    // Returns true if its files can be read: not enciphered, not compressed.
    bool Readable() const
    {
        return header_.Encryption() == 0 && header_.Compression() == 0;
    }

    // Returns the file of a name, without case; of two of one name, the first.
    // @return null if the archive has none
    const Entry *Find(std::wstring_view name) const;

    // Returns a file's bytes.
    // @return nullopt if the archive is not Readable, or the file runs past its end
    std::optional<std::vector<std::byte>> Read(const Entry &entry) const;
    std::optional<std::vector<std::byte>> Read(std::wstring_view name) const;

  private:
    std::filesystem::path path_;
    Header header_;
    std::vector<Entry> entries_;
    std::vector<std::pair<std::wstring, std::size_t>> byName_; // lower case, sorted
    struct Stream;
    std::shared_ptr<Stream> stream_; // the held file
};

} // namespace dao::core::erf
