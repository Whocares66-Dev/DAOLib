// Notes on the archives:
//
// Version 2.0, read 2026-10-11 as tools/script_commands.py's erf_entries
// reads the game's own: "ERF V2.0" as sixteen bytes of UTF-16, the number of
// files as 32 bits, a year, a day and a word, and from byte 32 an entry of
// 72 bytes for each: the name as 32 UTF-16 units, NUL-padded, then where its
// bytes start and how many there are.
//
// Version 2.2, as xoreos reads it (src/aurora/erffile.cpp, readV22Header and
// readV22ResList) and as Eluvian's resources tool read the add-ins' names
// (2026-10-02): the same count at 16, then flags at 32, a module id at 36 and
// a password's MD5 at 40, and from byte 56 entries of 76 bytes, the name and
// offset as 2.0's, then the packed size before the size. The flags' bits 4 to
// 7 say how the files are enciphered (2, xoreos's kEncryptionBlowfishDAO:
// Blowfish under a numeric password whose MD5 the header holds; where the
// game gets it is unread) and bits 29 to 31 how they are compressed. Every
// one of the GOG install's 377 is enciphered and none compressed, counted
// 2026-10-12; their directories are not enciphered, so their names are read.
//
// The archives are large (modelmeshdata.erf is 425 MiB), so a reader takes
// the directory alone and then each file it wants by its place. The Archive
// is ElgaraShaders' (src/LevelFiles.h, 2026-10-11), moved here for every
// reader of the game's files (DAOLib #106, step 2): held open, a room of the
// Circle read in half a second where opening the archive for each reading
// took seven, on a thread whose reading gives way to everything; not held, a
// terrain level's five hundred archives do not run a process out of the 512
// files it may hold open.
#include "dao/core/Erf.h"

#include "dao/core/Bytes.h"
#include "dao/core/ReadAt.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <fstream>

namespace dao::core::erf
{

namespace
{

constexpr std::size_t kNameUnits = 32;
constexpr std::size_t kMagicSize = 16;
constexpr std::size_t kFlags = 32;
constexpr std::array<char16_t, 8> kMagic20{u'E', u'R', u'F', u' ', u'V', u'2', u'.', u'0'};
constexpr std::array<char16_t, 8> kMagic22{u'E', u'R', u'F', u' ', u'V', u'2', u'.', u'2'};
// No archive of the game's has a directory near this (modelmeshdata.erf's
// 425 MiB hold some 20,000 files, 1.4 MiB of directory); a count past it is a
// damaged or foreign file, not a reason to ask for gigabytes.
constexpr std::size_t kMostDirectory = std::size_t{64} << 20;

bool Is(std::span<const std::byte> bytes, const std::array<char16_t, 8> &magic)
{
    return bytes.size() >= kMagicSize && std::memcmp(bytes.data(), magic.data(), kMagicSize) == 0;
}

std::wstring Lower(std::wstring_view text)
{
    std::wstring lower(text);
    for (wchar_t &letter : lower)
    {
        if (letter >= L'A' && letter <= L'Z')
        {
            letter = static_cast<wchar_t>(letter - L'A' + L'a');
        }
    }
    return lower;
}

} // namespace

std::size_t Header::DirectorySize() const
{
    return version == Version::V22 ? kHeaderSize22 + (std::size_t{count} * kEntrySize22)
                                   : kHeaderSize + (std::size_t{count} * kEntrySize);
}

std::optional<Header> ReadHeader(std::span<const std::byte> start)
{
    Header header;
    if (Is(start, kMagic20) && start.size() >= kHeaderSize)
    {
        header.version = Version::V20;
    }
    else if (Is(start, kMagic22) && start.size() >= kHeaderSize22)
    {
        header.version = Version::V22;
        header.flags = ReadAt<std::uint32_t>(start, kFlags).value_or(0);
    }
    else
    {
        return std::nullopt;
    }
    header.count = ReadAt<std::uint32_t>(start, kMagicSize).value_or(0);
    return header;
}

std::optional<std::uint32_t> EntryCount(std::span<const std::byte> header)
{
    if (header.size() < kHeaderSize || !Is(header, kMagic20))
    {
        return std::nullopt;
    }
    return ReadAt<std::uint32_t>(header, kMagicSize);
}

std::vector<Entry> Entries(std::span<const std::byte> start)
{
    std::vector<Entry> entries;
    const std::optional<Header> header = ReadHeader(start);
    if (!header)
    {
        return entries;
    }
    const bool v22 = header->version == Version::V22;
    const std::size_t first = v22 ? kHeaderSize22 : kHeaderSize;
    const std::size_t size = v22 ? kEntrySize22 : kEntrySize;
    const std::size_t whole = (start.size() - first) / size;
    entries.reserve(std::min<std::size_t>(header->count, whole));
    for (std::size_t i = 0; i < header->count && i < whole; ++i)
    {
        const std::size_t at = first + (i * size);
        Entry entry;
        for (std::size_t unit = 0; unit < kNameUnits; ++unit)
        {
            const char16_t letter = ReadAt<char16_t>(start, at + (unit * sizeof(char16_t))).value_or(0);
            if (letter == 0)
            {
                break;
            }
            entry.name.push_back(static_cast<wchar_t>(letter));
        }
        const std::size_t numbers = at + (kNameUnits * sizeof(char16_t));
        entry.offset = ReadAt<std::uint32_t>(start, numbers).value_or(0);
        if (v22)
        {
            entry.packed = ReadAt<std::uint32_t>(start, numbers + 4).value_or(0);
            entry.size = ReadAt<std::uint32_t>(start, numbers + 8).value_or(0);
        }
        else
        {
            entry.size = ReadAt<std::uint32_t>(start, numbers + 4).value_or(0);
            entry.packed = entry.size;
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

// A file handle opened for reading and shared with other readers, closed once.
// Read at offsets (ReadAt), so many threads read it with no lock; the lock
// guards only the first open.
namespace
{

HANDLE OpenForReading(const std::filesystem::path &path)
{
    return CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                       nullptr);
}

} // namespace

// A held archive's file, opened at its first read and shared by every thread after: a read at an
// offset shares no position, so no lock (DAOLib #210). An open that fails is tried again at the next
// read, as the stream before it was; of two threads opening at once, the second closes its own.
struct Archive::Stream
{
    std::atomic<HANDLE> handle{INVALID_HANDLE_VALUE};

    Stream() = default;
    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
    ~Stream()
    {
        if (HANDLE open = handle.load(); open != INVALID_HANDLE_VALUE)
        {
            CloseHandle(open);
        }
    }

    HANDLE Get(const std::filesystem::path &path)
    {
        HANDLE open = handle.load(std::memory_order_acquire);
        if (open != INVALID_HANDLE_VALUE)
        {
            return open;
        }
        HANDLE mine = OpenForReading(path);
        if (mine == INVALID_HANDLE_VALUE)
        {
            return mine;
        }
        if (handle.compare_exchange_strong(open, mine, std::memory_order_acq_rel))
        {
            return mine;
        }
        CloseHandle(mine);
        return open;
    }
};

std::optional<Archive> Archive::Open(const std::filesystem::path &path, bool held)
{
    std::ifstream file(path, std::ios::binary);
    std::vector<std::byte> start(kHeaderSize22);
    file.read(reinterpret_cast<char *>(start.data()), static_cast<std::streamsize>(start.size()));
    start.resize(static_cast<std::size_t>(std::max<std::streamsize>(file.gcount(), 0)));
    const std::optional<Header> header = ReadHeader(start);
    if (!header)
    {
        return std::nullopt;
    }
    std::error_code unknown;
    const std::uintmax_t fileSize = std::filesystem::file_size(path, unknown);
    // The directory, or as much of it as the file holds.
    const std::size_t wanted = std::min(header->DirectorySize(), kMostDirectory);
    if (wanted > start.size())
    {
        const std::size_t had = start.size();
        start.resize(wanted);
        file.clear();
        file.read(reinterpret_cast<char *>(start.data() + had), static_cast<std::streamsize>(wanted - had));
        start.resize(had + static_cast<std::size_t>(std::max<std::streamsize>(file.gcount(), 0)));
    }
    Archive archive;
    archive.path_ = path;
    archive.header_ = *header;
    archive.size_ = unknown ? 0 : fileSize;
    if (held)
    {
        archive.stream_ = std::make_shared<Stream>(); // its file opened at the first read
    }
    archive.entries_ = erf::Entries(start);
    archive.byName_.reserve(archive.entries_.size());
    for (std::size_t i = 0; i < archive.entries_.size(); ++i)
    {
        archive.byName_.emplace_back(Lower(archive.entries_[i].name), i);
    }
    // In the names' order, and of two files of one name the first.
    std::ranges::stable_sort(archive.byName_, {}, &std::pair<std::wstring, std::size_t>::first);
    return archive;
}

const Entry *Archive::Find(std::wstring_view name) const
{
    const std::wstring wanted = Lower(name);
    const auto found = std::ranges::lower_bound(byName_, wanted, {}, &std::pair<std::wstring, std::size_t>::first);
    return found != byName_.end() && found->first == wanted ? &entries_[found->second] : nullptr;
}

std::optional<std::vector<std::byte>> Archive::Read(const Entry &entry) const
{
    // A file the archive does not hold whole is refused before its bytes are
    // asked for: a damaged directory's size can be near 4 GiB, which a
    // 32-bit plugin cannot have (review of #106 step 2, 2026-10-12).
    if (!Readable() || std::uint64_t{entry.offset} + entry.size > size_)
    {
        return std::nullopt;
    }
    std::vector<std::byte> bytes(entry.size);
    Stream own;
    HANDLE handle = (stream_ != nullptr ? *stream_ : own).Get(path_);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }
    if (ReadAt(handle, bytes.data(), entry.size, entry.offset) != entry.size)
    {
        return std::nullopt;
    }
    return bytes;
}

std::optional<std::vector<std::byte>> Archive::Read(std::wstring_view name) const
{
    const Entry *entry = Find(name);
    if (entry == nullptr)
    {
        return std::nullopt;
    }
    return Read(*entry);
}

} // namespace dao::core::erf
