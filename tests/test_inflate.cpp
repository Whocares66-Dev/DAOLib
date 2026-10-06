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
        x = ((x * 1103515245U) + 12345U) & 0x7FFFFFFFU;
        text.push_back(kLetters[(x >> 16U) & 15U]);
    }
    return text;
}

const std::vector<std::byte> kDynamic = Of(
    {0x78, 0xDA, 0x25, 0x4C, 0x4B, 0x0A, 0x00, 0x51, 0x08, 0x3A, 0xAB, 0x0B, 0xA1, 0x36, 0x09, 0xE5, 0xFD, 0x19, 0x1F,
     0xE3, 0x22, 0xCC, 0x9F, 0x97, 0xDD, 0xCD, 0xA6, 0xAF, 0x66, 0x68, 0x16, 0x37, 0xC7, 0x01, 0x84, 0x81, 0x1C, 0x8F,
     0xC4, 0x30, 0x62, 0xB2, 0x22, 0xC2, 0xE5, 0x2D, 0xA0, 0xEA, 0x49, 0xB8, 0xD3, 0x5A, 0xF4, 0x0E, 0x6E, 0xCB, 0x49,
     0x77, 0x11, 0x7F, 0xE9, 0xD4, 0x78, 0xF4, 0x5C, 0x62, 0xD6, 0xDF, 0xB3, 0x13, 0xF7, 0x03, 0xD2, 0x51, 0x31, 0xB6});

// zlib.compressobj(9) given b"first block, ", a Z_FULL_FLUSH, then
// b"then the last.": a block of the fixed codes that is not the last, the
// flush's empty stored block, then the last block. A movie of any size is
// several blocks; a reader that stopped at the first would lose the rest.
const std::vector<std::byte> kSeveral =
    Of({0x78, 0xDA, 0x4A, 0xCB, 0x2C, 0x2A, 0x2E, 0x51, 0x48, 0xCA, 0xC9, 0x4F, 0xCE, 0xD6,
        0x51, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x2B, 0xC9, 0x48, 0xCD, 0x53, 0x28, 0xC9,
        0x48, 0x55, 0xC8, 0x49, 0x2C, 0x2E, 0xD1, 0x03, 0x00, 0x8B, 0x41, 0x09, 0xB2});

// zlib.compress(b"a" * 600, 9): one literal, then matches of the longest
// length deflate has, 258, at distance 1; any long run in a movie is sent so.
const std::vector<std::byte> kLongest =
    Of({0x78, 0xDA, 0x4B, 0x4C, 0x1C, 0x05, 0xA3, 0x80, 0xFA, 0x00, 0x00, 0xEE, 0xBA, 0xE3, 0x59});

// zlib.compress(X + bytes(24600) + X, 9), X the twenty letters A to T: the
// second X is a match 24620 bytes back, in the farthest distance code (24577
// to 32768), which a movie larger than 24K comes to.
const std::vector<std::byte> kFar = Of(
    {0x78, 0xDA, 0xED, 0xDD, 0xC9, 0x01, 0x40, 0x30, 0x00, 0x00, 0x30, 0xAB, 0x55, 0xD1, 0xC3, 0x51, 0xC5, 0xFE, 0xB3,
     0x58, 0xC0, 0xD7, 0x2F, 0x59, 0x24, 0x61, 0x8C, 0xD3, 0xBC, 0xA4, 0x5C, 0xEA, 0xBA, 0xED, 0x47, 0x3B, 0xFB, 0x75,
     0x3F, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x9B, 0xF0, 0x71, 0x05, 0xBC, 0xB6, 0x30, 0x0B, 0xA5});

std::vector<std::byte> FarText()
{
    std::vector<std::byte> text;
    for (int round = 0; round < 2; ++round)
    {
        for (char c = 'A'; c <= 'T'; ++c)
        {
            text.push_back(static_cast<std::byte>(c));
        }
        if (round == 0)
        {
            text.insert(text.end(), 24600, std::byte{0});
        }
    }
    return text;
}

// Built by hand (zlib refuses it: "too many length or distance symbols"): a
// block of its own codes whose header says 286 length codes and 32 distance
// codes, two more than deflate has, then code lengths for all 318 of them (a
// code of 0 and 18, and 18's runs of zeros: 138, 138, 42). A reader that
// took the 32 would write the last two past its array of 286 + 30: damaged
// bytes in a movie must be refused, not written through.
const std::vector<std::byte> kTooManyDistances =
    Of({0x78, 0x9C, 0xED, 0x1F, 0x80, 0xE4, 0xFF, 0xFF, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});

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

TEST_CASE("a stream of several blocks inflates to its last", "[inflate]")
{
    REQUIRE((std::to_integer<int>(kSeveral[2]) & 1) == 0); // the first block is not the last
    const auto out = InflateZlib(kSeveral, 1024);
    REQUIRE(out);
    CHECK(Text(*out) == "first block, then the last.");
}

TEST_CASE("a match of the longest length inflates", "[inflate]")
{
    const auto out = InflateZlib(kLongest, 1024);
    REQUIRE(out);
    CHECK(Text(*out) == std::string(600, 'a'));
}

TEST_CASE("a match from the farthest distances inflates", "[inflate]")
{
    const auto out = InflateZlib(kFar, 32768);
    REQUIRE(out);
    CHECK(*out == FarText());
}

TEST_CASE("a block with more distance codes than deflate has is refused", "[inflate]")
{
    CHECK_FALSE(InflateZlib(kTooManyDistances, 1024));
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
