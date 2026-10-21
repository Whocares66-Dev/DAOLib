// dao::Patch against the test process's own memory: a write only over the bytes expected, a
// second patch of the same bytes refused, the bytes put back only while they are the patch's,
// and the protection of a read-only page kept (DAOLib #220).
#include "dao/Patch.h"

#include <Windows.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

namespace
{

using Row = std::array<std::int32_t, 4>;
constexpr Row kVanilla = {1, 0, 2, 3};
constexpr Row kFull = {0, 1, 2, 3};

std::uintptr_t At(const void *value)
{
    return reinterpret_cast<std::uintptr_t>(value);
}

} // namespace

TEST_CASE("A patch writes over the bytes expected and puts them back")
{
    Row row = kVanilla;
    {
        dao::Patch patch;
        REQUIRE(patch.ApplyAt(At(&row), kVanilla, kFull));
        CHECK(patch.IsApplied());
        CHECK(row == kFull);
        CHECK(patch.Revert());
        CHECK_FALSE(patch.IsApplied());
        CHECK(row == kVanilla);
        REQUIRE(patch.ApplyAt(At(&row), kVanilla, kFull));
    }
    CHECK(row == kVanilla); // the destructor reverts
}

TEST_CASE("A patch leaves bytes that are not as expected")
{
    Row row = {9, 9, 9, 9};
    dao::Patch patch;
    CHECK_FALSE(patch.ApplyAt(At(&row), kVanilla, kFull));
    CHECK_FALSE(patch.IsApplied());
    CHECK(row == Row{9, 9, 9, 9});
}

TEST_CASE("A second patch of the same bytes is refused, and the first is left")
{
    Row row = kVanilla;
    dao::Patch first;
    dao::Patch second;
    REQUIRE(first.ApplyAt(At(&row), kVanilla, kFull));
    CHECK_FALSE(second.ApplyAt(At(&row), kVanilla, Row{3, 2, 1, 0}));
    CHECK(row == kFull);
    CHECK_FALSE(first.ApplyAt(At(&row), kFull, kVanilla)); // already applied
    CHECK(row == kFull);
}

TEST_CASE("A patch whose bytes were written since leaves them")
{
    Row row = kVanilla;
    dao::Patch patch;
    REQUIRE(patch.ApplyAt(At(&row), kVanilla, kFull));
    row[0] = 7;
    CHECK_FALSE(patch.Revert());
    CHECK_FALSE(patch.IsApplied());
    CHECK(row == Row{7, 1, 2, 3});
}

TEST_CASE("A patch writes a read-only page and keeps it read-only")
{
    void *page = VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(page != nullptr);
    *static_cast<Row *>(page) = kVanilla;
    DWORD was = 0;
    REQUIRE(VirtualProtect(page, 0x1000, PAGE_READONLY, &was));
    dao::Patch patch;
    REQUIRE(patch.ApplyAt(At(page), kVanilla, kFull));
    CHECK(*static_cast<const Row *>(page) == kFull);
    MEMORY_BASIC_INFORMATION info{};
    REQUIRE(VirtualQuery(page, &info, sizeof info) != 0);
    CHECK(info.Protect == PAGE_READONLY);
    CHECK(patch.Revert());
    CHECK(*static_cast<const Row *>(page) == kVanilla);
    VirtualFree(page, 0, MEM_RELEASE);
}

TEST_CASE("A patch of memory that is not there is refused")
{
    void *page = VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(page != nullptr);
    const std::uintptr_t gone = At(page);
    REQUIRE(VirtualFree(page, 0, MEM_RELEASE));
    dao::Patch patch;
    CHECK_FALSE(patch.ApplyAt(gone, kVanilla, kFull));
}
