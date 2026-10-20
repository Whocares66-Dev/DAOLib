#include "dao/core/Gff4.h"
#include "dao/core/Grid.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

using Catch::Approx;
using dao::core::Point;
namespace gff4 = dao::core::gff4;
namespace navigation = dao::core::navigation;
using dao::test::Builder;
using dao::test::Bytes;
using dao::test::Floats;
using dao::test::Start;
using dao::test::Struct;

namespace
{

// A list of 16-bit numbers at the data's end: its data offset.
std::uint32_t Shorts(Builder &b, const std::vector<std::int16_t> &values)
{
    const std::size_t at = b.bytes.size();
    b.U32(at, static_cast<std::uint32_t>(values.size()));
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        const auto value = static_cast<std::uint16_t>(values[i]);
        b.U8(at + 4 + (2 * i), static_cast<std::uint8_t>(value & 0xFF));
        b.U8(at + 5 + (2 * i), static_cast<std::uint8_t>(value >> 8));
    }
    return static_cast<std::uint32_t>(at - b.data);
}

// A layout whose walk grid is laid out as lak303d.arl's (tools/gff4.py --templates): 3 columns by
// 2 rows of cells at (10, 20, 0), a sector size of 2, so a first level of 2 blocks by 1 and a
// second of 1. The first block's one entrance is at cell (1, 0) with an edge to the second block's
// entrance, at cell (2, 0), whose edge leads back; the second level's entrance is at 8 sixteenths
// of its 4-cell block across, cell (2, 0). Before each first-level block's records is its
// entrance's parent byte, 0: the second level's one entrance. The cells: 10, 1, 1 with a counter in
// its high bits, 9, 10, 10; heights 0, 16, -8, 0, 0, 0; sound data -1, 0, 0, 1, -1, -1.
std::vector<std::byte> LayoutFile()
{
    Builder b = Start("ARL ", {{"AREA", 96, {{3020, 1, gff4::kStruct, 0}}},
                               {"ARPA", 96, {{3110, 2, gff4::kStruct, 0}}},
                               {"NAVI",
                                96,
                                {{3090, 10, 0, 0},
                                 {3091, 10, 0, 16},
                                 {3086, 5, 0, 32},
                                 {3087, 5, 0, 36},
                                 {3088, 8, 0, 40},
                                 {3089, 8, 0, 48},
                                 {3092, 0, gff4::kList, 52},
                                 {3093, 3, gff4::kList, 56},
                                 {3114, 1, gff4::kList, 60},
                                 {3094, 5, 0, 72},
                                 {3095, 3, gff4::kList | gff4::kStruct, 76},
                                 {3096, 0, gff4::kList, 84}}},
                               {"SECT", 4, {{3116, 2, 0, 0}, {3115, 0, 0, 2}}}});
    const std::size_t root = Struct(b, 96);
    const std::size_t grid = root; // AREA's ARPA and its NAVI are in place at its start
    Floats(b, grid, {10, 20, 0});
    Floats(b, grid + 16, {0, 0, 1});
    b.U32(grid + 32, 3);
    b.U32(grid + 36, 2);
    dao::test::Float(b, grid + 40, 0.5F);
    dao::test::Float(b, grid + 48, 2.1F);
    b.U32(grid + 52, Bytes(b, {10, 1, 0x31, 9, 10, 10}));
    b.U32(grid + 56, Shorts(b, {0, 16, -8, 0, 0, 0}));
    b.U32(grid + 60, Bytes(b, {0xFF, 0, 0, 1, 0xFF, 0xFF}));
    b.U32(grid + 72, 2);
    const std::size_t sectors = b.bytes.size();
    b.U32(sectors, 3);
    const std::size_t first = Struct(b, 4);
    const std::size_t second = Struct(b, 4);
    const std::size_t super = Struct(b, 4);
    b.U32(grid + 76, static_cast<std::uint32_t>(sectors - b.data));
    b.U8(first, 1);
    b.U8(first + 2, 1);
    b.U8(second, 5);
    b.U8(second + 2, 1);
    b.U8(super, 8);
    b.U8(super + 2, 1);
    b.U32(grid + 84, Bytes(b, {0, 1, 1, 0x20, 0, 0, 1, 0x60, 0x80, 0}));
    return b.bytes;
}

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());
    std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

} // namespace

TEST_CASE("a walk grid has its cells' kinds, heights and sound data", "[grid]")
{
    const std::vector<std::byte> bytes = LayoutFile();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    const std::optional<navigation::Grid> grid = navigation::GridOf(*file);
    REQUIRE(grid.has_value());
    CHECK(grid->columns == 3);
    CHECK(grid->rows == 2);
    CHECK(grid->cellSize == 0.5F);
    CHECK(grid->base == Point{10, 20, 0});
    CHECK(grid->Kind(0, 0) == 10);
    CHECK(grid->Kind(0, 2) == navigation::kWalkable); // the counter in the high bits is no part of it
    CHECK(grid->Walkable(0, 1));
    CHECK(!grid->Walkable(1, 0));
    CHECK(grid->Kind(2, 0) == navigation::kOffGrid);
    CHECK(grid->Kind(0, -1) == navigation::kOffGrid);
    CHECK(grid->Height(0, 1) == Approx(1.0F));
    CHECK(grid->Height(0, 2) == Approx(-0.5F));
    CHECK(!grid->Height(0, 3).has_value());
    CHECK(grid->sounds == std::vector<std::int8_t>{-1, 0, 0, 1, -1, -1});
}

TEST_CASE("a walk grid's coarse levels have their entrances where the exe puts them, and their edges", "[grid]")
{
    const std::vector<std::byte> bytes = LayoutFile();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    const std::optional<navigation::Grid> grid = navigation::GridOf(*file);
    REQUIRE(grid.has_value());
    REQUIRE(grid->abstraction.size() == 2);

    const navigation::Level &first = grid->abstraction[0];
    CHECK(first.blockCells == 2);
    CHECK(first.columns == 2);
    CHECK(first.rows == 1);
    REQUIRE(first.blocks.size() == 2);
    REQUIRE(first.blocks[0].size() == 1);
    CHECK(first.blocks[0][0].column == 1);
    CHECK(first.blocks[0][0].row == 0);
    REQUIRE(first.blocks[0][0].edges.size() == 1);
    CHECK(first.blocks[0][0].edges[0].block == 1); // to the right
    CHECK(first.blocks[0][0].edges[0].entrance == 0);
    REQUIRE(first.blocks[1].size() == 1);
    CHECK(first.blocks[1][0].column == 2);
    REQUIRE(first.blocks[1][0].edges.size() == 1);
    CHECK(first.blocks[1][0].edges[0].block == 0); // to the left
    // Both under the second level's one block, its entrance 0 (DAOLib #215).
    CHECK(first.blocks[0][0].parent == 0);
    CHECK(first.blocks[1][0].parent == 0);

    const navigation::Level &second = grid->abstraction[1];
    CHECK(second.blockCells == 4);
    REQUIRE(second.blocks.size() == 1);
    REQUIRE(second.blocks[0].size() == 1);
    CHECK(second.blocks[0][0].column == 2); // 8 sixteenths of 4 cells
    CHECK(second.blocks[0][0].row == 0);
    CHECK(second.blocks[0][0].edges.empty());
    CHECK(second.blocks[0][0].parent == -1); // the second level has none
}

TEST_CASE("a layout with no grid has none", "[grid]")
{
    Builder b = Start("ARL ", {{"AREA", 4, {{3011, 5, 0, 0}}}});
    Struct(b, 4);
    const std::optional<gff4::File> file = gff4::File::Parse(b.bytes);
    REQUIRE(file.has_value());
    CHECK(!navigation::GridOf(*file).has_value());
}

// What the game's grids hold, counted over them.
namespace
{

struct Counts
{
    std::size_t grids = 0;
    std::size_t twoLevels = 0;
    std::size_t walkable = 0;
    std::size_t walls = 0;
    std::size_t outside = 0;
    std::size_t unwalkable = 0; // first-level entrances on a cell that cannot be walked
    std::size_t parents = 0;    // first-level entrances of two-level grids, each with a parent that is there
    std::size_t orphans = 0;    // such entrances whose parent is none, or not among its parent block's
};

// Checks one grid's lists and adds what it holds to the counts.
void Tally(const navigation::Grid &grid, Counts &counts)
{
    ++counts.grids;
    const auto cells = static_cast<std::size_t>(grid.columns) * static_cast<std::size_t>(grid.rows);
    CHECK(grid.cells.size() == cells);
    CHECK(grid.heights.size() == cells);
    CHECK(grid.sounds.size() == cells);
    // Seven grids hold no AreaGridLightSubsetData8 at all (rmtst, combat2, brc201d among them).
    CHECK((grid.lightSubsets.empty() || grid.lightSubsets.size() == cells));
    REQUIRE(!grid.abstraction.empty());
    counts.twoLevels += grid.abstraction.size() == 2 ? 1 : 0;
    const navigation::Level &first = grid.abstraction[0];
    for (std::size_t index = 0; index < first.blocks.size(); ++index)
    {
        for (const navigation::Entrance &entrance : first.blocks[index])
        {
            counts.unwalkable += grid.Walkable(entrance.row, entrance.column) ? 0 : 1;
            if (grid.abstraction.size() == 2)
            {
                // The parent block by 0x00A49D10's rule (DAOLib #215).
                const navigation::Level &second = grid.abstraction[1];
                const auto column = static_cast<std::int32_t>(index) % first.columns;
                const auto row = static_cast<std::int32_t>(index) / first.columns;
                const auto parent = static_cast<std::size_t>((column / 2) + (second.columns * (row / 2)));
                const bool there = entrance.parent >= 0 && parent < second.blocks.size() &&
                                   static_cast<std::size_t>(entrance.parent) < second.blocks[parent].size();
                (there ? counts.parents : counts.orphans) += 1;
            }
        }
    }
    for (const std::uint8_t cell : grid.cells)
    {
        counts.walkable += (cell & 0x0F) == 1 ? 1 : 0;
        counts.walls += (cell & 0x0F) == 9 ? 1 : 0;
        counts.outside += (cell & 0x0F) == 10 ? 1 : 0;
    }
}

} // namespace

TEST_CASE("the game's walk grids in game/levels, when there, have whole lists and entrances on walkable cells",
          "[grid]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "levels";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/levels in this checkout (ghilana's extract of *.arl writes them)");
    }
    Counts counts;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".arl")
        {
            continue;
        }
        const std::vector<std::byte> bytes = FromDisk(entry.path());
        const std::optional<gff4::File> file = gff4::File::Parse(bytes);
        INFO(entry.path().filename().string());
        REQUIRE(file.has_value());
        if (const std::optional<navigation::Grid> grid = navigation::GridOf(*file))
        {
            Tally(*grid, counts);
        }
    }
    CHECK(counts.grids > 0);
    // Every layout the game takes (DAOLib #156): the counts tools/gff4.py read in them.
    if (counts.grids == 212)
    {
        CHECK(counts.twoLevels == 204);
        CHECK(counts.walkable == 4664956);
        CHECK(counts.walls == 3781551);
        CHECK(counts.outside == 26679058);
        CHECK(counts.unwalkable == 15); // all arena2's, a test arena: its grid changed after its abstraction was made
        CHECK(counts.parents == 71518); // lib-agent-2's count, 2026-10-20 (DAOLib #215)
        CHECK(counts.orphans == 0);
    }
}
