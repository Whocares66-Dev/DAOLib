// The archives (dao/core/Erf.h): version 2.2's directory beside 2.0's, and
// an archive on disk read by name. Version 2.0's directory over bytes is
// test_lightprobes.cpp's, where the levels first needed it.
#include "dao/core/Erf.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace archive = dao::core::erf;

namespace
{

struct File
{
    std::string name;
    std::string bytes;
};

void Put32(std::vector<std::byte> &out, std::uint32_t value)
{
    for (int i = 0; i < 4; ++i)
    {
        out.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFF));
    }
}

void PutName(std::vector<std::byte> &out, const std::string &name)
{
    for (std::size_t unit = 0; unit < 32; ++unit)
    {
        const char letter = unit < name.size() ? name[unit] : '\0';
        out.push_back(static_cast<std::byte>(letter));
        out.push_back(std::byte{0});
    }
}

// An archive as the game lays one out: header, directory, then the files.
std::vector<std::byte> Build(bool v22, const std::vector<File> &files, std::uint32_t flags = 0)
{
    std::vector<std::byte> out;
    for (const char letter : std::string(v22 ? "ERF V2.2" : "ERF V2.0"))
    {
        out.push_back(static_cast<std::byte>(letter));
        out.push_back(std::byte{0});
    }
    Put32(out, static_cast<std::uint32_t>(files.size()));
    Put32(out, 109); // the year since 1900
    Put32(out, 200); // the day
    Put32(out, 0xFFFFFFFF);
    if (v22)
    {
        Put32(out, flags);
        Put32(out, 0);               // the module
        out.resize(out.size() + 16); // the password's digest
    }
    const std::size_t header = v22 ? archive::kHeaderSize22 : archive::kHeaderSize;
    const std::size_t entry = v22 ? archive::kEntrySize22 : archive::kEntrySize;
    auto offset = static_cast<std::uint32_t>(header + (files.size() * entry));
    for (const File &file : files)
    {
        PutName(out, file.name);
        Put32(out, offset);
        const auto size = static_cast<std::uint32_t>(file.bytes.size());
        Put32(out, size);
        if (v22)
        {
            Put32(out, size);
        }
        offset += size;
    }
    for (const File &file : files)
    {
        for (const char c : file.bytes)
        {
            out.push_back(static_cast<std::byte>(c));
        }
    }
    return out;
}

// A file of the test's own, gone when the test is: the mutants run many
// copies of the tests at once, so no two share a name.
struct TempFile
{
    std::filesystem::path path;

    explicit TempFile(const std::vector<std::byte> &bytes)
    {
        std::random_device random;
        path = std::filesystem::temp_directory_path() /
               ("dao_archive" + std::to_string(random()) + "_" + std::to_string(random()) + ".erf");
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    ~TempFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    TempFile(const TempFile &) = delete;
    TempFile &operator=(const TempFile &) = delete;
};

std::string Text(const std::vector<std::byte> &bytes)
{
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

} // namespace

TEST_CASE("a version 2.2 header says its count and flags, and its directory's size", "[erf]")
{
    const auto bytes = Build(true, {{"a.uti", "x"}, {"b.plo", "yy"}}, (2U << 4) | (1U << 29));
    const auto header = archive::ReadHeader(bytes);
    REQUIRE(header);
    CHECK(header->version == archive::Version::V22);
    CHECK(header->count == 2);
    CHECK(header->Encryption() == 2);
    CHECK(header->Compression() == 1);
    CHECK(header->DirectorySize() == archive::kHeaderSize22 + (2 * archive::kEntrySize22));
    // EntryCount stays version 2.0's: its callers size a 2.0 directory with it.
    CHECK_FALSE(archive::EntryCount(bytes).has_value());
}

TEST_CASE("a version 2.0 header has no flags, and its directory is 2.0's size", "[erf]")
{
    const auto bytes = Build(false, {{"a.gda", "abc"}});
    const auto header = archive::ReadHeader(bytes);
    REQUIRE(header);
    CHECK(header->version == archive::Version::V20);
    CHECK(header->count == 1);
    CHECK(header->flags == 0);
    CHECK(header->DirectorySize() == archive::kHeaderSize + archive::kEntrySize);
    CHECK_FALSE(archive::ReadHeader(std::span(bytes).first(20)).has_value());
}

TEST_CASE("a version 2.2 directory has the packed size before the size", "[erf]")
{
    auto bytes = Build(true, {{"a.uti", "hello"}});
    // Packed 3, unpacked 5, as a compressed archive would have them.
    const std::size_t numbers = archive::kHeaderSize22 + 64;
    const std::uint32_t packed = 3;
    std::memcpy(bytes.data() + numbers + 4, &packed, 4);
    const auto entries = archive::Entries(bytes);
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].name == L"a.uti");
    CHECK(entries[0].offset == archive::kHeaderSize22 + archive::kEntrySize22);
    CHECK(entries[0].packed == 3);
    CHECK(entries[0].size == 5);
}

TEST_CASE("a version 2.0 entry's packed size is its size", "[erf]")
{
    const auto entries = archive::Entries(Build(false, {{"a.gda", "abcd"}}));
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].size == 4);
    CHECK(entries[0].packed == 4);
}

TEST_CASE("an archive on disk reads any file by name, without case, either version", "[erf]")
{
    for (const bool v22 : {false, true})
    {
        const TempFile file(Build(v22, {{"abi_base.gda", "the table"}, {"Ogre.UTC", "an ogre"}}));
        const auto opened = archive::Archive::Open(file.path);
        REQUIRE(opened);
        CHECK(opened->Readable());
        CHECK(opened->Entries().size() == 2);
        REQUIRE(opened->Find(L"ABI_BASE.gda") != nullptr);
        CHECK(Text(opened->Read(L"abi_base.gda").value()) == "the table");
        CHECK(Text(opened->Read(L"ogre.utc").value()) == "an ogre");
        CHECK(opened->Find(L"none.utc") == nullptr);
        CHECK_FALSE(opened->Read(L"none.utc").has_value());
    }
}

TEST_CASE("of two files of one name, the archive's first is found", "[erf]")
{
    const TempFile file(Build(false, {{"z.txt", "z"}, {"a.txt", "first"}, {"A.TXT", "second"}}));
    const auto opened = archive::Archive::Open(file.path);
    REQUIRE(opened);
    CHECK(Text(opened->Read(L"a.txt").value()) == "first");
    // And the directory is in the archive's order, not the names'.
    CHECK(opened->Entries()[0].name == L"z.txt");
}

TEST_CASE("of many files of one name, the archive's first is found", "[erf]")
{
    // Past the 32 a sort leaves to an insertion sort, which keeps the order
    // of equal names as a sort need not.
    std::vector<File> files;
    files.reserve(200);
    for (int i = 0; i < 200; ++i)
    {
        files.push_back({i % 2 == 0 ? "same.txt" : "other" + std::to_string(i), std::to_string(i)});
    }
    const TempFile file(Build(false, files));
    const auto opened = archive::Archive::Open(file.path);
    REQUIRE(opened);
    CHECK(Text(opened->Read(L"same.txt").value()) == "0");
}

TEST_CASE("a held archive reads again from the file it keeps open", "[erf]")
{
    const TempFile file(Build(false, {{"one", "1111"}, {"two", "22"}}));
    const auto opened = archive::Archive::Open(file.path, true);
    REQUIRE(opened);
    for (int round = 0; round < 3; ++round)
    {
        CHECK(Text(opened->Read(L"two").value()) == "22");
        CHECK(Text(opened->Read(L"one").value()) == "1111");
    }
}

TEST_CASE("an enciphered archive lists its files and reads none", "[erf]")
{
    const TempFile file(Build(true, {{"secret.gda", "ciphered"}}, 2U << 4));
    const auto opened = archive::Archive::Open(file.path);
    REQUIRE(opened);
    CHECK_FALSE(opened->Readable());
    REQUIRE(opened->Find(L"secret.gda") != nullptr);
    CHECK_FALSE(opened->Read(L"secret.gda").has_value());
}

TEST_CASE("a file that runs past the archive's end is not read", "[erf]")
{
    auto bytes = Build(false, {{"cut", "0123456789"}});
    bytes.resize(bytes.size() - 4);
    const TempFile file(bytes);
    const auto opened = archive::Archive::Open(file.path);
    REQUIRE(opened);
    REQUIRE(opened->Find(L"cut") != nullptr);
    CHECK_FALSE(opened->Read(L"cut").has_value());
}

TEST_CASE("a file whose size is past the archive's is refused before its bytes are asked for", "[erf]")
{
    auto bytes = Build(false, {{"huge", "0123"}});
    // The entry's size, after its name and offset: near 4 GiB, which a
    // 32-bit test cannot allocate and must not be asked to.
    const std::size_t size = archive::kHeaderSize + 64 + 4;
    bytes[size] = bytes[size + 1] = bytes[size + 2] = std::byte{0xFF};
    bytes[size + 3] = std::byte{0xF0};
    const TempFile file(bytes);
    const auto opened = archive::Archive::Open(file.path);
    REQUIRE(opened);
    const archive::Entry *huge = opened->Find(L"huge");
    REQUIRE(huge != nullptr);
    CHECK(huge->size == 0xF0FFFFFFU);
    CHECK_FALSE(opened->Read(*huge).has_value());
}

TEST_CASE("an archive whose directory is cut short gives the entries it holds whole", "[erf]")
{
    auto bytes = Build(false, {{"one", ""}, {"two", ""}});
    bytes.resize(archive::kHeaderSize + archive::kEntrySize + 10);
    const TempFile file(bytes);
    const auto opened = archive::Archive::Open(file.path);
    REQUIRE(opened);
    CHECK(opened->Entries().size() == 1);
}

TEST_CASE("what is no archive, or no file, opens as none", "[erf]")
{
    const TempFile file(std::vector<std::byte>(100, std::byte{'X'}));
    CHECK_FALSE(archive::Archive::Open(file.path).has_value());
    CHECK_FALSE(archive::Archive::Open(file.path.string() + ".missing").has_value());
}
