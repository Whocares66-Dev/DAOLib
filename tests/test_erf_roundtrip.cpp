// Every archive of version 2.0 the GOG install has, its files read and written back by erf::Write
// (DAOLib #106's third step), byte for byte: the header with its date, the directory, the files
// and the gaps between them. Hidden, as it reads some 15 GB: run it by its tag, dao_core_tests
// "[install-real]", in the core-x64 build to take every archive, since the largest
// (texturepack.erf, 1.4 GB) is held three times over; a 32-bit run leaves out those past
// kMostIn32Bits and says how many.
//
// Seen in the first run (2026-10-24, 10,257 archives, the add-ins' enciphered 2.2 ones aside):
// every archive is Write's layout. Four of packages\core\data (designerareas, designercutscenes,
// designertriggers, fonts) end in 256 more bytes, ASCII hex digits, no MD5, SHA-1, SHA-256 or
// SHA-512 of the rest; what writes or reads them is unread, and Write makes none. One archive,
// PC_GAMER_TALE_modcore.erf, has no files: its header alone, which Write refuses to make.
#include "dao/core/Erf.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace archive = dao::core::erf;

namespace
{

constexpr std::uint64_t kMostIn32Bits = 384ULL << 20;
constexpr std::size_t kTail = 256;

std::vector<std::byte> ReadFile(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<std::byte> bytes(static_cast<std::size_t>(std::filesystem::file_size(path)));
    in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

std::uint32_t U32(std::span<const std::byte> bytes, std::size_t at)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + at, sizeof(value));
    return value;
}

// The day the header says the archive was built: its year less 1900, and its days since 1 January.
std::chrono::sys_days Built(std::span<const std::byte> bytes)
{
    const std::chrono::year year{static_cast<int>(U32(bytes, 20)) + 1900};
    return std::chrono::sys_days{year / std::chrono::January / 1} + std::chrono::days{U32(bytes, 24)};
}

bool HexDigits(std::span<const std::byte> bytes)
{
    return std::ranges::all_of(bytes, [](std::byte b) {
        const auto c = static_cast<char>(b);
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

bool IsArchiveName(const std::filesystem::path &path)
{
    std::wstring extension = path.extension().wstring();
    std::ranges::transform(extension, extension.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return extension == L".erf" || extension == L".rim";
}

} // namespace

TEST_CASE("on the GOG install, every archive of version 2.0 is written back as it was", "[.][install-real]")
{
    const std::filesystem::path game = L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins";
    if (!std::filesystem::exists(game / L"bin_ship"))
    {
        SKIP("no GOG install here");
    }
    std::size_t archives = 0;
    std::size_t tails = 0;
    std::size_t empty = 0;
    std::size_t tooLarge = 0;
    const std::uint64_t most = sizeof(void *) == 4 ? kMostIn32Bits : std::numeric_limits<std::uint64_t>::max();
    for (const auto &item : std::filesystem::recursive_directory_iterator(game))
    {
        if (!item.is_regular_file() || !IsArchiveName(item.path()))
        {
            continue;
        }
        const auto opened = archive::Archive::Open(item.path());
        if (!opened || opened->Head().version != archive::Version::V20)
        {
            continue;
        }
        if (item.file_size() > most)
        {
            ++tooLarge;
            continue;
        }
        INFO(item.path().string());
        const std::vector<std::byte> bytes = ReadFile(item.path());
        const std::vector<archive::Entry> entries = archive::Entries(bytes);
        ++archives;
        if (entries.empty())
        {
            CHECK(bytes.size() == archive::kHeaderSize);
            ++empty;
            continue;
        }
        std::vector<archive::Item> files;
        files.reserve(entries.size());
        for (const archive::Entry &entry : entries)
        {
            const auto *const first = bytes.data() + entry.offset;
            files.push_back({entry.name, std::vector<std::byte>(first, first + entry.size)});
        }
        const std::vector<std::byte> written = archive::Write(files, Built(bytes));
        REQUIRE(written.size() <= bytes.size());
        const auto [mine, theirs] = std::ranges::mismatch(written, std::span(bytes).first(written.size()));
        INFO("first difference at " << (mine - written.begin()));
        CHECK(mine == written.end());
        const std::span<const std::byte> tail = std::span(bytes).subspan(written.size());
        if (!tail.empty())
        {
            CHECK(tail.size() == kTail);
            CHECK(HexDigits(tail));
            ++tails;
        }
    }
    CHECK(archives > 0);
    WARN(archives << " archives written back, " << tails << " with 256 hex digits after their files, " << empty
                  << " with none; " << tooLarge << " too large for a 32-bit run");
}
