// dao/core/WalkPatch.h: a placeable's patches read as grids, and the engine's
// stamp of one into a level's grid (0x00A46CC0).
#include "dao/core/WalkPatch.h"

#include <cctype>
#include <cmath>
#include <optional>

namespace dao::core::navigation
{

namespace
{

// The fields' labels (dev/gff4_fields.csv).
constexpr std::uint32_t kRoomList = 3016;
constexpr std::uint32_t kGridArea = 3110;

// The turn from +y to a facing, about z: what the engine's quaternion is for a facing in the plane. 0x00A46CC0
// makes it with 0x004EDF80, the shortest arc from 0x004EDE20's (0, 1, 0) to the facing, both normalised, and
// 180 degrees about z where the facing is -y; 0x004CB6D0 turns each cell's point by it. A facing with a z would
// tilt the patch there; this takes x and y alone. A facing of no length is taken as +y (not read).
struct Turn
{
    float cos = 1;
    float sin = 0;
};

Turn TurnTo(const Point &facing)
{
    const float length = std::hypot(facing[0], facing[1]);
    if (length <= 0.0F)
    {
        return {};
    }
    // +y turned by the angle a is (-sin a, cos a): the facing.
    return {.cos = facing[1] / length, .sin = -facing[0] / length};
}

// Visits a patch's cells as 0x00A46CC0 does, column by column and each column's rows, with the level's cell under
// each.
template <class Visit>
void Each(const Grid &grid, const Grid &patch, const Point &position, const Point &facing, Visit visit)
{
    if (grid.columns <= 0 || grid.rows <= 0)
    {
        return;
    }
    const Turn turn = TurnTo(facing);
    for (std::int32_t column = 0; column < patch.columns; ++column)
    {
        for (std::int32_t row = 0; row < patch.rows; ++row)
        {
            const Point at = patch.Place(row, column);
            const float x = (turn.cos * at[0]) - (turn.sin * at[1]) + position[0];
            const float y = (turn.sin * at[0]) + (turn.cos * at[1]) + position[1];
            visit(grid.CellAt(x, y), patch.Kind(row, column));
        }
    }
}

} // namespace

std::string PatchName(std::string_view modelName, std::int32_t appearance)
{
    std::string out;
    for (const char c : modelName)
    {
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out + "_" + std::to_string(appearance) + ".pwk";
}

std::vector<Grid> PatchesOf(const gff4::File &pwk)
{
    std::vector<Grid> out;
    const std::optional<gff4::Struct> root = pwk.Root();
    if (!root)
    {
        return out;
    }
    for (const gff4::Struct &state : root->Structs(kRoomList))
    {
        const std::optional<gff4::Struct> area = state.Child(kGridArea);
        out.push_back(area ? GridOf(*area) : Grid{});
    }
    return out;
}

void Stamp(Grid &grid, const Grid &patch, const Point &position, const Point &facing)
{
    Each(grid, patch, position, facing, [&grid](const Cell &cell, std::uint8_t kind) {
        if (grid.Kind(cell.row, cell.column) < kBlocked)
        {
            grid.SetKind(cell.row, cell.column, kind);
        }
    });
}

void Unstamp(Grid &grid, const Grid &patch, const Point &position, const Point &facing)
{
    Each(grid, patch, position, facing, [&grid](const Cell &cell, std::uint8_t /*kind*/) {
        const std::uint8_t now = grid.Kind(cell.row, cell.column);
        if (now == kClosedDoor || now == kBlocked)
        {
            grid.SetKind(cell.row, cell.column, kWalkable);
        }
    });
}

std::vector<Cell> Footprint(const Grid &grid, const Grid &patch, const Point &position, const Point &facing)
{
    std::vector<Cell> out;
    Each(grid, patch, position, facing, [&out](const Cell &cell, std::uint8_t /*kind*/) { out.push_back(cell); });
    return out;
}

} // namespace dao::core::navigation
