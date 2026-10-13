#include "dao/core/Fingerprint.h"
#include "dao/core/Pe.h"

#include "dao/Addresses.h"

#include "daoaddr/CodeIndex.h"
#include "daoaddr/Library.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

using dao::core::Fingerprint;
using dao::core::ReadFingerprint;

namespace
{

// The GOG build's, as its headers have them; any numbers would do here.
constexpr Fingerprint kSample{
    .timeDateStamp = 0x4eb19d53,
    .sizeOfImage = 0x009ec0a2,
    .entryPointRva = 0x004e3024,
    .imageBase = 0x00400000,
    .is32Bit = true,
};

// A PE32 header with the given fields, laid out as the specification says
// and as the real exe is: DOS header, e_lfanew at 0x80, then the NT headers.
std::vector<std::byte> Header(const Fingerprint &f, std::uint16_t machine = 0x14c, std::uint16_t magic = 0x10b)
{
    std::vector<std::byte> image(0x200);
    auto put16 = [&](std::size_t at, std::uint16_t v) { std::memcpy(&image[at], &v, 2); };
    auto put32 = [&](std::size_t at, std::uint32_t v) { std::memcpy(&image[at], &v, 4); };
    put16(0, 0x5a4d);
    put32(0x3c, 0x80);
    const std::size_t nt = 0x80;
    put32(nt, 0x4550);
    put16(nt + 4, machine);
    put32(nt + 8, f.timeDateStamp);
    const std::size_t opt = nt + 24;
    put16(opt, magic);
    put32(opt + 16, f.entryPointRva);
    put32(opt + 28, f.imageBase);
    put32(opt + 56, f.sizeOfImage);
    return image;
}

std::optional<Fingerprint> FingerprintOfFile(const std::filesystem::path &exe)
{
    std::ifstream in(exe, std::ios::binary);
    std::vector<std::byte> head(0x1000);
    in.read(reinterpret_cast<char *>(head.data()), static_cast<std::streamsize>(head.size()));
    return ReadFingerprint(head);
}

daoaddr::BuildInfo BuildOf(const Fingerprint &exe)
{
    return daoaddr::BuildInfo{.linkStamp = exe.timeDateStamp,
                              .sizeOfImage = exe.sizeOfImage,
                              .entryPointRva = exe.entryPointRva,
                              .imageBase = exe.imageBase};
}

// Whether the address library, as this build packed it, has a file for the
// build an exe is: named by its four header fields, and saying the same.
bool LibraryKnows(const Fingerprint &exe)
{
    const auto library =
        daoaddr::Library::Load(std::filesystem::path(DAO_ADDRESS_DATA_DIR) / daoaddr::FileName(BuildOf(exe)));
    return exe.is32Bit && library && library->Build() == BuildOf(exe);
}

} // namespace

TEST_CASE("a PE32 header reads back as the fingerprint it was made from")
{
    const auto image = Header(kSample);
    const auto got = ReadFingerprint(image);
    REQUIRE(got.has_value());
    CHECK(*got == kSample);
}

TEST_CASE("a header that is not x86 PE32 says so")
{
    CHECK_FALSE(ReadFingerprint(Header(kSample, 0x8664, 0x20b))->is32Bit);
    CHECK_FALSE(ReadFingerprint(Header(kSample, 0x14c, 0x20b))->is32Bit);
    CHECK_FALSE(*ReadFingerprint(Header(kSample, 0x8664, 0x20b)) == kSample);
}

TEST_CASE("bytes that are not a PE image give nothing")
{
    CHECK_FALSE(ReadFingerprint({}).has_value());
    std::vector<std::byte> junk(0x200, std::byte{0x41});
    CHECK_FALSE(ReadFingerprint(junk).has_value());

    auto noSignature = Header(kSample);
    noSignature[0x80] = std::byte{0};
    CHECK_FALSE(ReadFingerprint(noSignature).has_value());

    auto cut = Header(kSample);
    cut.resize(0x80 + 24 + 40); // ends inside the optional header, before SizeOfImage
    CHECK_FALSE(ReadFingerprint(cut).has_value());
}

TEST_CASE("Steam's build has GOG's link stamp and a file of its own")
{
    // The stock Steam exe as installed (the address library's dev/STEAM.md):
    // SteamStub's headers, GOG's link stamp.
    auto steam = kSample;
    steam.sizeOfImage = 0x00a49000;
    steam.entryPointRva = 0x009ed2ee;
    REQUIRE(steam.timeDateStamp == kSample.timeDateStamp);
    CHECK(LibraryKnows(steam));
    CHECK(daoaddr::FileName(BuildOf(steam)) == "daoaddresses-4eb19d53-00a49000-009ed2ee-00400000.bin");
    CHECK(daoaddr::FileName(BuildOf(steam)) != daoaddr::FileName(BuildOf(kSample)));
    // EA's has a file by all four fields too.
    CHECK(LibraryKnows(Fingerprint{.timeDateStamp = 0x52164c37,
                                   .sizeOfImage = 0x0096b000,
                                   .entryPointRva = 0x00001000,
                                   .imageBase = 0x00400000,
                                   .is32Bit = true}));
    // Any one field off is another exe.
    auto entry = steam;
    entry.entryPointRva += 1;
    CHECK_FALSE(LibraryKnows(entry));
    auto base = steam;
    base.imageBase += 0x1000;
    CHECK_FALSE(LibraryKnows(base));
    // The game's code is GOG's, at GOG's addresses: the CRT's entry point,
    // which the stub jumps to once it has decrypted the code, among them.
    const auto load = [](const Fingerprint &exe) {
        return daoaddr::Library::Load(std::filesystem::path(DAO_ADDRESS_DATA_DIR) / daoaddr::FileName(BuildOf(exe)));
    };
    const auto gog = load(kSample);
    const auto stub = load(steam);
    REQUIRE(gog);
    REQUIRE(stub);
    CHECK(stub->Rva(dao::addr::kEntryPoint) == gog->Rva(dao::addr::kEntryPoint));
    CHECK(stub->Rva(dao::addr::kEntryPoint) != steam.entryPointRva);
    CHECK(stub->Rva(dao::addr::kCClientExoApp_SetPaused) == gog->Rva(dao::addr::kCClientExoApp_SetPaused));
}

TEST_CASE("the address library has a file for the build that was read, and none for another")
{
    // What dao::Module asks at start-up, of the files the build packed.
    CHECK(LibraryKnows(kSample));
    auto other = kSample;
    other.timeDateStamp += 1;
    CHECK_FALSE(LibraryKnows(other));
    // The same link stamp is not enough: a file for an exe of another size
    // or entry point is for another exe.
    auto altered = kSample;
    altered.sizeOfImage += 0x1000;
    CHECK_FALSE(LibraryKnows(altered));
}

TEST_CASE("the copy of the exe in game/, when there is one, is a build the address library knows")
{
    // The test binary runs from build/<preset>/tests; the repo root is three up.
    const auto exe = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "DAOrigins.exe";
    if (!std::filesystem::exists(exe))
    {
        SKIP("no game/DAOrigins.exe in this checkout");
    }
    const auto got = FingerprintOfFile(exe);
    REQUIRE(got.has_value());
    CAPTURE(got->timeDateStamp, got->sizeOfImage, got->entryPointRva, got->imageBase, got->is32Bit);
    CHECK(got->is32Bit);
    CHECK(LibraryKnows(*got));
}

TEST_CASE("the EA app's exe in game/ea/, when there is one, is a build the address library knows")
{
    // As the EA app installs it: wrapped, its entry point the wrapper's.
    const auto exe = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "ea" / "DAOrigins.exe";
    if (!std::filesystem::exists(exe))
    {
        SKIP("no game/ea/DAOrigins.exe in this checkout");
    }
    const auto got = FingerprintOfFile(exe);
    REQUIRE(got.has_value());
    CAPTURE(got->timeDateStamp, got->sizeOfImage, got->entryPointRva, got->imageBase, got->is32Bit);
    CHECK(got->is32Bit);
    CHECK(LibraryKnows(*got));
}

// DAOLib #110: an exe whose headers no file is named for is looked up by its code, which Module
// reads from the exe on disk by the section's raw offset. The same steps over an image made here.
TEST_CASE("code read from a file by its section's raw offset is the build whose code it is")
{
    constexpr std::uint32_t kLength = 0x40;
    std::vector<std::byte> image(0x400 + kLength + 0x10); // sixteen bytes of padding after the code
    auto put32 = [&](std::size_t at, std::uint32_t v) { std::memcpy(&image[at], &v, 4); };
    put32(0x3c, 0x80);
    image[0] = std::byte{'M'};
    image[1] = std::byte{'Z'};
    put32(0x80, 0x4550);
    const std::uint16_t sections = 1;
    std::memcpy(&image[0x80 + 6], &sections, 2);
    const std::size_t table = 0x80 + 24;
    std::memcpy(&image[table], ".text", 5);
    put32(table + 8, kLength + 0x10); // this exe's own virtual size is longer than the build's
    put32(table + 12, 0x1000);
    put32(table + 16, 0x400);
    put32(table + 20, 0x400);
    for (std::size_t i = 0; i < kLength; ++i)
    {
        image[0x400 + i] = static_cast<std::byte>(i * 7);
    }
    const std::vector<std::byte> code(image.begin() + 0x400, image.begin() + 0x400 + kLength);
    const auto text = "DAOCODE 1\n00001000 " + std::string("00000040 ") + daoaddr::ToHex(daoaddr::Sha256(code)) +
                      " 4eb19d53 009ec0a2 004e3024 00400000 gog\n";
    const auto index = daoaddr::CodeIndex::Parse(text);
    REQUIRE(index.has_value());
    REQUIRE(index->Ranges().size() == 1);
    const auto range = index->Ranges().front();

    const auto offset = dao::core::FileOffsetOf(image, range.rva, range.length);
    REQUIRE(offset.has_value());
    const std::span<const std::byte> read(image.data() + *offset, range.length);
    const auto build = index->Match(range, read);
    REQUIRE(build.has_value());
    CHECK(daoaddr::FileName(*build) == "daoaddresses-4eb19d53-009ec0a2-004e3024-00400000.bin");

    // Another exe's code, or any byte of it changed, is no build's.
    image[0x400 + 5] = std::byte{0xCC};
    CHECK_FALSE(index->Match(range, std::span<const std::byte>(image.data() + *offset, range.length)).has_value());
}

TEST_CASE("the code index packed from the table names GOG's code")
{
    const auto index =
        daoaddr::CodeIndex::Load(std::filesystem::path(DAO_ADDRESS_DATA_DIR) / daoaddr::CodeIndexFileName());
    REQUIRE(index.has_value());
    CHECK_FALSE(index->Ranges().empty());
    CHECK(index->Ranges().front() == daoaddr::CodeRange{.rva = 0x1000, .length = 0x6d86aa});
}
