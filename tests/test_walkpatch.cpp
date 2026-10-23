#include "dao/core/Gff4.h"
#include "dao/core/Grid.h"
#include "dao/core/WalkPatch.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
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

// A level's grid of 4 by 4 walkable cells a metre a side from (0, 0), each cell's point at its middle.
navigation::Grid Level()
{
    navigation::Grid grid;
    grid.columns = 4;
    grid.rows = 4;
    grid.cellSize = 1;
    grid.padding = 0.5F;
    grid.cells.assign(16, navigation::kWalkable);
    return grid;
}

// A patch of cells a metre a side, its cell (0, 0)'s point at (0.5, 0.5) from the placeable, as the game's
// patches have a padding of 0: its kinds row by row.
navigation::Grid Patch(std::int32_t columns, std::int32_t rows, std::vector<std::uint8_t> kinds, float size = 1)
{
    navigation::Grid patch;
    patch.columns = columns;
    patch.rows = rows;
    patch.cellSize = size;
    patch.base = {0.5F, 0.5F, 0};
    patch.cells = std::move(kinds);
    return patch;
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

TEST_CASE("a grid's cell has its point at the corner plus the padding, and a point its cell", "[walkpatch]")
{
    navigation::Grid grid = Level();
    grid.base = {10, 20, 3};
    grid.heights.assign(16, 16);
    const Point at = grid.Place(1, 2);
    CHECK(at[0] == Approx(12.5F));
    CHECK(at[1] == Approx(21.5F));
    CHECK(at[2] == Approx(1.0F)); // its height alone, not the base's z
    CHECK(grid.CellAt(12.99F, 21.01F) == navigation::Cell{.row = 1, .column = 2});
    CHECK(grid.CellAt(5, 20.5F) == navigation::Cell{.row = 0, .column = 0}); // a negative index is 0
    CHECK(grid.CellAt(19, 40) == navigation::Cell{.row = 20, .column = 9});  // past the grid stays past it
}

TEST_CASE("setting a cell's kind keeps its counters and leaves cells off the grid", "[walkpatch]")
{
    navigation::Grid grid = Level();
    grid.cells[5] = 0x31;
    grid.SetKind(1, 1, navigation::kBlocked);
    CHECK(grid.cells[5] == 0x38);
    grid.SetKind(4, 0, navigation::kBlocked);
    grid.SetKind(0, -1, navigation::kBlocked);
    CHECK(std::ranges::count(grid.cells, navigation::kBlocked) == 0);
}

TEST_CASE("a stamp writes the patch's kinds where the placeable stands, facing +y", "[walkpatch]")
{
    navigation::Grid grid = Level();
    navigation::Stamp(grid, Patch(2, 1, {8, 7}), {1, 1, 0}, {0, 1, 0});
    CHECK(grid.Kind(1, 1) == 8);
    CHECK(grid.Kind(1, 2) == 7);
    CHECK(std::ranges::count(grid.cells, navigation::kWalkable) == 14);
}

TEST_CASE("a stamp is turned from +y to the placeable's facing", "[walkpatch]")
{
    const navigation::Grid patch = Patch(2, 1, {8, 7});
    SECTION("facing +x, a quarter turn clockwise")
    {
        navigation::Grid grid = Level();
        navigation::Stamp(grid, patch, {1, 3, 0}, {1, 0, 0});
        CHECK(grid.Kind(2, 1) == 8); // (0.5, 0.5) to (0.5, -0.5)
        CHECK(grid.Kind(1, 1) == 7); // (1.5, 0.5) to (0.5, -1.5)
    }
    SECTION("facing -x, a quarter turn the other way, the facing's length no matter")
    {
        navigation::Grid grid = Level();
        navigation::Stamp(grid, patch, {3, 1, 0}, {-2, 0, 0});
        CHECK(grid.Kind(1, 2) == 8);
        CHECK(grid.Kind(2, 2) == 7);
    }
    SECTION("facing -y, half a turn about z, not a mirror")
    {
        navigation::Grid grid = Level();
        navigation::Stamp(grid, patch, {3, 3, 0}, {0, -1, 0});
        CHECK(grid.Kind(2, 2) == 8);
        CHECK(grid.Kind(2, 1) == 7);
    }
}

TEST_CASE("a stamp writes only where the level's cell is under 8", "[walkpatch]")
{
    navigation::Grid grid = Level();
    grid.SetKind(0, 0, 9); // a wall
    grid.SetKind(0, 1, 8); // another placeable's
    grid.SetKind(0, 2, 5); // a creature's stamp
    grid.SetKind(0, 3, 7); // a closed door's
    navigation::Stamp(grid, Patch(4, 1, {7, 7, 8, 1}), {0, -0.5F, 0}, {0, 1, 0});
    CHECK(grid.Kind(0, 0) == 9);
    CHECK(grid.Kind(0, 1) == 8);
    CHECK(grid.Kind(0, 2) == 8);
    CHECK(grid.Kind(0, 3) == 1);
}

TEST_CASE("a patch's finer cells over one of the level's: an 8 holds it, else the last visited", "[walkpatch]")
{
    navigation::Grid grid = Level();
    navigation::Stamp(grid, Patch(2, 1, {8, 1}, 0.25F), {0, 0, 0}, {0, 1, 0});
    CHECK(grid.Kind(0, 0) == 8);
    navigation::Stamp(grid, Patch(2, 1, {7, 1}, 0.25F), {1, 0, 0}, {0, 1, 0});
    CHECK(grid.Kind(0, 1) == 1);
}

TEST_CASE("taking a patch out sets the 7s and 8s under it to 1, whatever the patch's cells", "[walkpatch]")
{
    navigation::Grid grid = Level();
    grid.SetKind(1, 0, 7);
    grid.SetKind(1, 1, 8);
    grid.SetKind(1, 2, 9);
    grid.SetKind(1, 3, 5);
    grid.SetKind(2, 1, 8); // not under the patch
    navigation::Unstamp(grid, Patch(4, 1, {1, 1, 8, 8}), {0, 0.5F, 0}, {0, 1, 0});
    CHECK(grid.Kind(1, 0) == 1);
    CHECK(grid.Kind(1, 1) == 1);
    CHECK(grid.Kind(1, 2) == 9);
    CHECK(grid.Kind(1, 3) == 5);
    CHECK(grid.Kind(2, 1) == 8);
}

TEST_CASE("a footprint is the level's cells in the engine's order, column by column", "[walkpatch]")
{
    const std::vector<navigation::Cell> cells =
        navigation::Footprint(Level(), Patch(2, 2, {1, 1, 1, 1}), {0, 0, 0}, {0, 1, 0});
    CHECK(cells ==
          std::vector<navigation::Cell>{
              {.row = 0, .column = 0}, {.row = 1, .column = 0}, {.row = 0, .column = 1}, {.row = 1, .column = 1}});
}

TEST_CASE("a patch file's states are its room list's grids, in order", "[walkpatch]")
{
    Builder b = Start("PWK ", {{"PPG", 4, {{3016, 1, gff4::kList | gff4::kStruct, 0}}},
                               {"", 32, {{3110, 2, gff4::kStruct, 0}}},
                               {"NAVI",
                                32,
                                {{3086, 5, 0, 0},
                                 {3087, 5, 0, 4},
                                 {3088, 8, 0, 8},
                                 {3120, 8, 0, 12},
                                 {3090, 10, 0, 16},
                                 {3092, 0, gff4::kList, 28}}}});
    const std::size_t root = Struct(b, 4);
    const std::size_t list = b.bytes.size();
    b.U32(list, 2);
    const std::size_t first = Struct(b, 32);
    const std::size_t second = Struct(b, 32);
    b.U32(root, static_cast<std::uint32_t>(list - b.data));
    b.U32(first, 2);
    b.U32(first + 4, 1);
    dao::test::Float(b, first + 8, 1.0F / 6);
    dao::test::Float(b, first + 12, 0);
    Floats(b, first + 16, {0.25F, -0.5F, 0});
    b.U32(first + 28, Bytes(b, {7, 7}));
    b.U32(second, 1);
    b.U32(second + 4, 2);
    dao::test::Float(b, second + 8, 1.0F / 6);
    dao::test::Float(b, second + 12, 0);
    b.U32(second + 28, Bytes(b, {8, 8}));
    const std::optional<gff4::File> file = gff4::File::Parse(b.bytes);
    REQUIRE(file.has_value());
    const std::vector<navigation::Grid> states = navigation::PatchesOf(*file);
    REQUIRE(states.size() == 2);
    CHECK(states[0].columns == 2);
    CHECK(states[0].rows == 1);
    CHECK(states[0].padding == 0.0F);
    CHECK(states[0].base == Point{0.25F, -0.5F, 0});
    CHECK(states[0].Kind(0, 1) == navigation::kClosedDoor);
    CHECK(states[1].rows == 2);
    CHECK(states[1].Kind(1, 0) == navigation::kBlocked);
}

TEST_CASE("a patch file is named for the placeable type's model and row", "[walkpatch]")
{
    CHECK(navigation::PatchName("PLC_SmFerDrB_01_0", 1) == "plc_smferdrb_01_0_1.pwk");
}

// What the game's patches hold, counted over them.
namespace
{

struct Counts
{
    std::size_t files = 0;
    std::size_t states = 0;
    std::map<std::size_t, std::size_t> statesPerFile;
    std::map<std::uint8_t, std::size_t> kinds;
};

void Tally(const std::vector<navigation::Grid> &states, Counts &counts)
{
    ++counts.files;
    counts.states += states.size();
    ++counts.statesPerFile[states.size()];
    for (const navigation::Grid &state : states)
    {
        CHECK(state.cells.size() == static_cast<std::size_t>(state.columns) * static_cast<std::size_t>(state.rows));
        for (const std::uint8_t cell : state.cells)
        {
            ++counts.kinds[cell];
        }
    }
}

// The small Ferelden door's patches stamped and swapped in a level's grid of 0.5 m cells from (-5, -5).
void CheckDoor(const std::vector<navigation::Grid> &door)
{
    REQUIRE(door.size() == 6);
    CHECK(door[0].columns == 10);
    CHECK(door[0].rows == 2);
    CHECK(door[0].cellSize == Approx(1.0F / 6));
    CHECK(door[0].padding == 0.0F);
    CHECK(std::ranges::count(door[0].cells, navigation::kClosedDoor) == 20); // Closed_Unlocked
    CHECK(std::ranges::count(door[1].cells, navigation::kBlocked) == 20);    // Closed_Locked
    CHECK(door[2].columns == 2);                                             // Open: a strip along -y
    CHECK(door[2].rows == 10);
    CHECK(std::ranges::count(door[2].cells, navigation::kBlocked) == 20);

    navigation::Grid grid;
    grid.columns = 20;
    grid.rows = 20;
    grid.cellSize = 0.5F;
    grid.padding = 0.25F;
    grid.base = {-5, -5, 0};
    grid.cells.assign(400, navigation::kWalkable);
    navigation::Stamp(grid, door[0], {0, 0, 0}, {0, 1, 0});
    CHECK(std::ranges::count(grid.cells, navigation::kClosedDoor) == 8); // columns 10 to 13, rows 9 and 10
    CHECK(grid.Kind(9, 10) == navigation::kClosedDoor);
    CHECK(grid.Kind(10, 13) == navigation::kClosedDoor);
    navigation::Unstamp(grid, door[0], {0, 0, 0}, {0, 1, 0});
    navigation::Stamp(grid, door[2], {0, 0, 0}, {0, 1, 0});
    CHECK(std::ranges::count(grid.cells, navigation::kClosedDoor) == 0);
    CHECK(grid.Kind(10, 11) == navigation::kWalkable);               // where it stood closed
    CHECK(std::ranges::count(grid.cells, navigation::kBlocked) > 0); // and the open door's strip
    CHECK(grid.Kind(6, 9) == navigation::kBlocked);                  // (-0.1, -1.6)
}

} // namespace

TEST_CASE("the game's patches in game/patches, when there, read whole, and the small door's states",
          "[walkpatch][folder]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "patches";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/patches in this checkout (ghilana's extract of *.pwk writes them)");
    }
    Counts counts;
    std::optional<std::vector<navigation::Grid>> door;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".pwk")
        {
            continue;
        }
        const std::vector<std::byte> bytes = FromDisk(entry.path());
        const std::optional<gff4::File> file = gff4::File::Parse(bytes);
        INFO(entry.path().filename().string());
        REQUIRE(file.has_value());
        const std::vector<navigation::Grid> states = navigation::PatchesOf(*file);
        CHECK(!states.empty());
        Tally(states, counts);
        if (entry.path().filename() == "plc_smferdrb_01_0_1.pwk")
        {
            door = states;
        }
    }
    CHECK(counts.files > 0);
    // Every patch the game takes from core (DAOLib #176): the counts tools/gff4.py read in them.
    if (counts.files == 522)
    {
        CHECK(counts.states == 1100);
        CHECK(counts.statesPerFile ==
              std::map<std::size_t, std::size_t>{{1, 217}, {2, 141}, {3, 113}, {4, 22}, {6, 29}});
        CHECK(counts.kinds == std::map<std::uint8_t, std::size_t>{{1, 130735}, {7, 1597}, {8, 126753}});
    }
    if (door)
    {
        CheckDoor(*door);
    }
}
