#include "dao/core/Fingerprint.h"

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

// Whether the address library, as this build packed it, has a file for the
// build an exe is: named by its link stamp, and saying the same of the rest.
bool LibraryKnows(const Fingerprint &exe)
{
    const auto library =
        daoaddr::Library::Load(std::filesystem::path(DAO_ADDRESS_DATA_DIR) / daoaddr::FileName(exe.timeDateStamp));
    return library && library->Build() == daoaddr::BuildInfo{.linkStamp = exe.timeDateStamp,
                                                             .sizeOfImage = exe.sizeOfImage,
                                                             .entryPointRva = exe.entryPointRva,
                                                             .imageBase = exe.imageBase};
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
