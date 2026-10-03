// dao::TryCopy and dao::TypeName against memory that is not there: the probe
// reads engine objects from another thread, and a read that faults must come
// back false or empty, never take the game down. These run in the test
// process, which is as good a place as any to fault.
#include "dao/SafeRead.h"
#include "dao/TypeName.h"
#include "dao/core/Pages.h"

#include <Windows.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{

// An address that was memory a moment ago and is not now: reserved, then
// released, so nothing is mapped there.
std::uintptr_t FreedPage()
{
    void *page = VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(page != nullptr);
    const auto address = reinterpret_cast<std::uintptr_t>(page);
    REQUIRE(VirtualFree(page, 0, MEM_RELEASE));
    return address;
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
    std::vector<std::byte> big(0x4000);
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

namespace
{

// TryCopy over a guard page, the exception it lets through caught here: 0 if
// none came.
DWORD GuardCopyRaises(std::uintptr_t page)
{
    std::array<std::byte, 8> out{};
    DWORD code = 0;
    __try
    {
        dao::TryCopy(out.data(), page, out.size());
    }
    __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        return code;
    }
    return 0;
}

} // namespace

// The cause of DAOLib issue #9: TryCopy catches access violations only, and
// a guard page raises its own exception, which goes on past it.
TEST_CASE("a copy over a guard page is not caught by TryCopy")
{
    void *page = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE | PAGE_GUARD);
    REQUIRE(page != nullptr);
    CHECK(GuardCopyRaises(reinterpret_cast<std::uintptr_t>(page)) == STATUS_GUARD_PAGE_VIOLATION);
    VirtualFree(page, 0, MEM_RELEASE);
}

// What a stack's guard page looks like to VirtualQuery, which the crash and
// hang logs ask before they read where a stack word points (DAOLib issue
// #9): reading one raises STATUS_GUARD_PAGE_VIOLATION, which TryCopy does
// not catch, so the page must be known for one beforehand. Not read here.
TEST_CASE("a guard page is known for one before it is read")
{
    void *page = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE | PAGE_GUARD);
    REQUIRE(page != nullptr);
    MEMORY_BASIC_INFORMATION region{};
    REQUIRE(VirtualQuery(page, &region, sizeof region) != 0);
    CHECK((region.Protect & PAGE_GUARD) != 0);
    CHECK_FALSE(dao::core::IsReadablePage(region.State, region.Protect));
    VirtualFree(page, 0, MEM_RELEASE);

    void *plain = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(plain != nullptr);
    REQUIRE(VirtualQuery(plain, &region, sizeof region) != 0);
    CHECK(dao::core::IsReadablePage(region.State, region.Protect));
    VirtualFree(plain, 0, MEM_RELEASE);
}
