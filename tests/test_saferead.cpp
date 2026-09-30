// dao::TryCopy and dao::TypeName against memory that is not there: the probe
// reads engine objects from another thread, and a read that faults must come
// back false or empty, never take the game down. These run in the test
// process, which is as good a place as any to fault.
#include "dao/SafeRead.h"
#include "dao/TypeName.h"

#include <Windows.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

namespace
{

// An address that was memory a moment ago and is not now: reserved, then
// released, so nothing is mapped there.
std::uintptr_t FreedPage()
{
    void *page = VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(page != nullptr);
    REQUIRE(VirtualFree(page, 0, MEM_RELEASE));
    return reinterpret_cast<std::uintptr_t>(page);
}

} // namespace

TEST_CASE("a copy from unmapped memory fails instead of faulting")
{
    std::array<std::byte, 16> small{};
    CHECK_FALSE(dao::TryCopy(small.data(), FreedPage(), small.size()));
    CHECK_FALSE(dao::TryRead<std::uint32_t>(FreedPage()).has_value());
    CHECK_FALSE(dao::TryRead<std::uint32_t>(0x10).has_value());
}

TEST_CASE("a large copy that runs off the end of memory fails instead of faulting")
{
    // Two pages, the second released: a 16 KiB copy from inside the first
    // walks into the hole, as the sampler's stack copy could.
    auto *base = static_cast<std::byte *>(VirtualAlloc(nullptr, 0x2000, MEM_RESERVE, PAGE_NOACCESS));
    REQUIRE(base != nullptr);
    REQUIRE(VirtualAlloc(base, 0x1000, MEM_COMMIT, PAGE_READWRITE) != nullptr);
    std::array<std::byte, 0x4000> big{};
    CHECK_FALSE(dao::TryCopy(big.data(), reinterpret_cast<std::uintptr_t>(base + 0x800), big.size()));
    CHECK(dao::TryCopy(big.data(), reinterpret_cast<std::uintptr_t>(base), 0x1000));
    VirtualFree(base, 0, MEM_RELEASE);
}

TEST_CASE("the type name of a pointer to nothing is empty")
{
    CHECK(dao::TypeName(reinterpret_cast<const void *>(FreedPage())).empty());
    CHECK(dao::TypeName(reinterpret_cast<const void *>(0x10)).empty());
    CHECK(dao::TypeName(nullptr).empty());
    const std::uint32_t notAnObject = 0x12345678;
    CHECK(dao::TypeName(&notAnObject).empty());
}
