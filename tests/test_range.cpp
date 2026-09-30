#include "dao/core/Range.h"

#include <catch2/catch_test_macros.hpp>

using dao::core::Contains;

namespace
{
// The exe's image, as the game has it.
constexpr std::uintptr_t kBegin = 0x00400000;
constexpr std::uintptr_t kEnd = 0x00400000 + 0x009ec0a2;
} // namespace

TEST_CASE("a range inside the image is inside")
{
    CHECK(Contains(kBegin, kEnd, 0x00b06648, 4));
    CHECK(Contains(kBegin, kEnd, kBegin, 4));
    CHECK(Contains(kBegin, kEnd, kEnd - 4, 4));
}

TEST_CASE("a range that runs past the end, or starts before the beginning, is not")
{
    CHECK_FALSE(Contains(kBegin, kEnd, kEnd - 3, 4));
    CHECK_FALSE(Contains(kBegin, kEnd, kBegin - 1, 4));
    CHECK_FALSE(Contains(kBegin, kEnd, 0, 4));
}

TEST_CASE("an address above the end is outside, whatever the size")
{
    // The crash of 2026-09-30: a word of 0x84c6cf93 read as a type
    // descriptor, and `bytes <= end - address` wrapped around and passed.
    CHECK_FALSE(Contains(kBegin, kEnd, 0x84c6cf93, 512));
    CHECK_FALSE(Contains(kBegin, kEnd, 0x84c6cf93, 1));
    CHECK_FALSE(Contains(kBegin, kEnd, 0xfffffff0, 4));
    CHECK_FALSE(Contains(kBegin, kEnd, kEnd + 1, 0));
}

static_assert(Contains(0x1000, 0x2000, 0x1ffc, 4));
static_assert(!Contains(0x1000, 0x2000, 0x3000, 4));
