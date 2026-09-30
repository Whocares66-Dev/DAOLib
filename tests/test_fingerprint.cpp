#include "dao/core/Fingerprint.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

using dao::core::Fingerprint;
using dao::core::kExpected;
using dao::core::ReadFingerprint;

namespace
{

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

} // namespace

TEST_CASE("a PE32 header reads back as the fingerprint it was made from")
{
    const auto image = Header(kExpected);
    const auto got = ReadFingerprint(image);
    REQUIRE(got.has_value());
    CHECK(*got == kExpected);
}

TEST_CASE("a header that is not x86 PE32 says so")
{
    CHECK_FALSE(ReadFingerprint(Header(kExpected, 0x8664, 0x20b))->is32Bit);
    CHECK_FALSE(ReadFingerprint(Header(kExpected, 0x14c, 0x20b))->is32Bit);
    CHECK_FALSE(*ReadFingerprint(Header(kExpected, 0x8664, 0x20b)) == kExpected);
}

TEST_CASE("bytes that are not a PE image give nothing")
{
    CHECK_FALSE(ReadFingerprint({}).has_value());
    std::vector<std::byte> junk(0x200, std::byte{0x41});
    CHECK_FALSE(ReadFingerprint(junk).has_value());

    auto noSignature = Header(kExpected);
    noSignature[0x80] = std::byte{0};
    CHECK_FALSE(ReadFingerprint(noSignature).has_value());

    auto cut = Header(kExpected);
    cut.resize(0x80 + 24 + 40); // ends inside the optional header, before SizeOfImage
    CHECK_FALSE(ReadFingerprint(cut).has_value());
}

TEST_CASE("the copy of the exe in game/, when there is one, is the expected build")
{
    // The test binary runs from build/<preset>/tests; the repo root is three up.
    const auto exe = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "DAOrigins.exe";
    if (!std::filesystem::exists(exe))
    {
        SKIP("no game/DAOrigins.exe in this checkout");
    }
    std::ifstream in(exe, std::ios::binary);
    std::vector<std::byte> head(0x1000);
    in.read(reinterpret_cast<char *>(head.data()), static_cast<std::streamsize>(head.size()));
    const auto got = ReadFingerprint(head);
    REQUIRE(got.has_value());
    CAPTURE(got->timeDateStamp, got->sizeOfImage, got->entryPointRva, got->imageBase, got->is32Bit);
    CHECK(*got == kExpected);
}
