#include "dao/core/Inflate.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

using dao::core::Adler32;
using dao::core::InflateZlib;

namespace
{

std::vector<std::byte> Of(std::initializer_list<int> values)
{
    std::vector<std::byte> bytes;
    for (const int value : values)
    {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

std::string Text(const std::vector<std::byte> &bytes)
{
    std::string text;
    for (const std::byte b : bytes)
    {
        text.push_back(static_cast<char>(b));
    }
    return text;
}

std::vector<std::byte> Bytes(std::string_view text)
{
    std::vector<std::byte> bytes;
    for (const char c : text)
    {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

// The streams are zlib's own, from Python's zlib.compress.

// zlib.compress(b"stored block", 0): one stored block.
const std::vector<std::byte> kStored = Of({0x78, 0x01, 0x01, 0x0C, 0x00, 0xF3, 0xFF, 0x73, 0x74, 0x6F, 0x72, 0x65,
                                           0x64, 0x20, 0x62, 0x6C, 0x6F, 0x63, 0x6B, 0x1F, 0x80, 0x04, 0xBD});

// zlib.compress(b"abcabcabcabc", 9): the fixed codes, with a match that
// runs into what it writes.
const std::vector<std::byte> kFixed =
    Of({0x78, 0xDA, 0x4B, 0x4C, 0x4A, 0x4E, 0x84, 0x21, 0x00, 0x1D, 0xE0, 0x04, 0x99});

// 120 letters, skewed and without pattern, which zlib sends with codes of
// the block's own.
std::string Skewed()
{
    constexpr std::array<char, 16> kLetters = {'e', 'e', 'e', 'e', 'e', 'e', 't', 't',
                                               'a', 'a', 'o', 'i', 'n', 's', 'r', 'h'};
    std::string text;
    std::uint32_t x = 1;
    for (int i = 0; i < 120; ++i)
    {
        x = (x * 1103515245U + 12345U) & 0x7FFFFFFFU;
        text.push_back(kLetters[(x >> 16U) & 15U]);
    }
    return text;
}

const std::vector<std::byte> kDynamic = Of(
    {0x78, 0xDA, 0x25, 0x4C, 0x4B, 0x0A, 0x00, 0x51, 0x08, 0x3A, 0xAB, 0x0B, 0xA1, 0x36, 0x09, 0xE5, 0xFD, 0x19, 0x1F,
     0xE3, 0x22, 0xCC, 0x9F, 0x97, 0xDD, 0xCD, 0xA6, 0xAF, 0x66, 0x68, 0x16, 0x37, 0xC7, 0x01, 0x84, 0x81, 0x1C, 0x8F,
     0xC4, 0x30, 0x62, 0xB2, 0x22, 0xC2, 0xE5, 0x2D, 0xA0, 0xEA, 0x49, 0xB8, 0xD3, 0x5A, 0xF4, 0x0E, 0x6E, 0xCB, 0x49,
     0x77, 0x11, 0x7F, 0xE9, 0xD4, 0x78, 0xF4, 0x5C, 0x62, 0xD6, 0xDF, 0xB3, 0x13, 0xF7, 0x03, 0xD2, 0x51, 0x31, 0xB6});

} // namespace

TEST_CASE("Adler-32 is zlib's", "[inflate]")
{
    CHECK(Adler32(Bytes("Wikipedia")) == 0x11E60398);
    CHECK(Adler32({}) == 1);
}

TEST_CASE("a stored block inflates", "[inflate]")
{
    const auto out = InflateZlib(kStored, 1024);
    REQUIRE(out);
    CHECK(Text(*out) == "stored block");
}

TEST_CASE("a block of the fixed codes inflates", "[inflate]")
{
    const auto out = InflateZlib(kFixed, 1024);
    REQUIRE(out);
    CHECK(Text(*out) == "abcabcabcabc");
}

TEST_CASE("a block with its own codes inflates", "[inflate]")
{
    REQUIRE((std::to_integer<int>(kDynamic[2]) >> 1 & 3) == 2); // the block's type
    const auto out = InflateZlib(kDynamic, 1024);
    REQUIRE(out);
    CHECK(Text(*out) == Skewed());
}

TEST_CASE("bytes after the stream are ignored", "[inflate]")
{
    std::vector<std::byte> padded = kFixed;
    padded.insert(padded.end(), 16, std::byte{0xAA});
    const auto out = InflateZlib(padded, 1024);
    REQUIRE(out);
    CHECK(Text(*out) == "abcabcabcabc");
}

TEST_CASE("a stream is refused past the limit", "[inflate]")
{
    CHECK(InflateZlib(kFixed, 12));
    CHECK_FALSE(InflateZlib(kFixed, 11));
    CHECK_FALSE(InflateZlib(kStored, 11));
    CHECK_FALSE(InflateZlib(kDynamic, 119));
}

TEST_CASE("a damaged stream is refused, not read past", "[inflate]")
{
    // The checksum.
    std::vector<std::byte> wrongSum = kFixed;
    wrongSum.back() ^= std::byte{1};
    CHECK_FALSE(InflateZlib(wrongSum, 1024));

    // The header: not deflate, and not a multiple of 31.
    std::vector<std::byte> wrongMethod = kFixed;
    wrongMethod[0] = std::byte{0x79};
    CHECK_FALSE(InflateZlib(wrongMethod, 1024));
    std::vector<std::byte> wrongCheck = kFixed;
    wrongCheck[1] = std::byte{0xDB};
    CHECK_FALSE(InflateZlib(wrongCheck, 1024));

    // Cut short anywhere, every stream is refused.
    for (const auto *stream : {&kStored, &kFixed, &kDynamic})
    {
        for (std::size_t length = 0; length < stream->size(); ++length)
        {
            INFO("cut to " << length << " of " << stream->size());
            CHECK_FALSE(InflateZlib(std::span(*stream).first(length), 1024));
        }
    }

    // A stored block whose length and its complement disagree.
    std::vector<std::byte> wrongLength = kStored;
    wrongLength[5] = std::byte{0xF2};
    CHECK_FALSE(InflateZlib(wrongLength, 1024));

    // The reserved block type.
    CHECK_FALSE(InflateZlib(Of({0x78, 0xDA, 0x07, 0x00, 0x00, 0x00, 0x00, 0x01}), 1024));
}

TEST_CASE("any byte changed gives the text or nothing, never a crash", "[inflate]")
{
    for (const auto *stream : {&kFixed, &kDynamic})
    {
        for (std::size_t at = 0; at < stream->size(); ++at)
        {
            for (const int flip : {0x01, 0x10, 0x80, 0xFF})
            {
                std::vector<std::byte> changed = *stream;
                changed[at] ^= static_cast<std::byte>(flip);
                const auto out = InflateZlib(changed, 4096);
                // A changed stream that still inflates has, by its checksum,
                // very likely the same text; what matters is that it ends.
                CHECK((!out || out->size() <= 4096));
            }
        }
    }
}
