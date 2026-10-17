// A level's walk grid out of the game's files, over bytes (DAOLib #156): the
// cells the pathfinder walks, their heights and rooms, and the hierarchical
// A*'s coarse levels over them, for a reimplementation that walks and finds
// paths as the game does (DAOParagonEngine).
//
// The grid is in the level's layout (<level>.arl, dao/core/Level.h), under
// EnvAreaPathfindExport / AreaGridArea (GFF4 "NAVI"); 212 of the 213 layouts
// the game takes have one (den510c has none). Read 2026-10-17 over all of them
// and in the GOG exe; dev/PARAGON_GRID.md has DAOParagonEngine's reading of
// the cells.
//
//   AreaGridData: one byte a cell, row by row. Its kind is the low four bits
//     (0x00A49070, 9 for a cell off the grid): 1 walkable, 9 and 10 not, the
//     only values in the files (4,664,956, 3,781,551 and 26,679,058 cells).
//     Under 5 is walkable and 5 and 6 a creature's stamp at run time
//     (dao/Addresses.h); the high four bits are two counters the engine keeps
//     at run time, 0 in the files.
//   AreaGridHeight: a signed 16-bit height a cell, in sixteenths of a metre
//     above z = 0 as DAOParagonEngine read it against the level's geometry;
//     the exe's own conversion was not found.
//   AreaGridRoomID: a signed byte a cell, its room's place in the level's
//     room list, -1 outside every room.
//   AreaGridAbstractSectors and AreaGridAbstractMemory: the coarse levels,
//     built at 0x00A4A690 (loader 0x00A4A9F0). The grid is cut into blocks of
//     AreaGridAbstractSectorSize cells a side, ceil(columns / size) by
//     ceil(rows / size), and those into blocks of 2 x 2; the sectors are the
//     first level's blocks row by row and then the second's (204 grids), or
//     the first's alone (the 8 with a size of 16). A sector is a 16-bit offset
//     into the memory, 0x10000 more from the first sector whose offset is less
//     than the one before (0x00A49DC0; none in the files), and a count of
//     entrances. At the offset, two bytes an entrance: where it is, and the end
//     of its edges, counted from the block's first edge, each entrance's
//     starting where the one before ended (0x00A4A190, 0x00A4A1D0); then one
//     byte an edge, the direction of the neighbouring block in its high three
//     bits and the entrance there in its low five (0x00A4AB80). A first-level
//     entrance's place is a cell of its block, column v % size and row v /
//     size (0x00A49E30); a second-level one's is in sixteenths of its 2 x 2
//     block, the column the high four bits and the row the low (0x00A49F70).
//     Every first-level entrance is on a walkable cell but 15 of arena2's (a
//     test arena), and every edge of either level names an entrance that is
//     there. The game's own node is
//     (block << 16) | entrance.
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/RayCast.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace dao::core::navigation
{

// A cell's kind: the low four bits of its byte.
inline constexpr std::uint8_t kWalkable = 1;
inline constexpr std::uint8_t kOffGrid = 9;     // what the engine answers for a cell off the grid
inline constexpr float kHeightUnit = 1.0F / 16; // metres a height step, read from the data

// An edge of the coarse graph: an entrance of a neighbouring block.
struct Edge
{
    std::uint32_t block = 0; // in the same level, row by row
    std::uint8_t entrance = 0;
};

// Where the coarse graph enters a block, and the entrances it reaches.
struct Entrance
{
    std::int32_t column = 0; // the grid's cell
    std::int32_t row = 0;
    std::vector<Edge> edges;
};

// One level of the coarse graph: its blocks row by row, each with its entrances.
struct Level
{
    std::int32_t blockCells = 0; // a block's side in cells
    std::int32_t columns = 0;    // blocks across
    std::int32_t rows = 0;
    std::vector<std::vector<Entrance>> blocks;
};

struct Grid
{
    std::int32_t columns = 0;
    std::int32_t rows = 0;
    float cellSize = 0;  // metres
    float clearance = 0; // as written; its use not read
    Point base{};        // cell (0, 0)'s corner in the level's space; its z is not the floor's
    Point normal{};
    std::vector<std::uint8_t> cells;   // a byte a cell, row by row
    std::vector<std::int16_t> heights; // a height a cell, in kHeightUnit
    std::vector<std::int8_t> rooms;    // a room a cell, -1 for none
    std::vector<std::uint8_t> links;   // AreaGridLink, a byte a cell: 255 in every file
    std::int32_t sectorSize = 0;
    std::vector<Level> abstraction;      // the first level, and the second where the file has one
    std::vector<std::uint8_t> collision; // EnvStatPhysData as written: a cooked PhysX stream

    // Returns a cell's kind, kOffGrid off the grid.
    [[nodiscard]] std::uint8_t Kind(std::int32_t row, std::int32_t column) const;
    // Returns whether a cell can be walked: its kind is under 5.
    [[nodiscard]] bool Walkable(std::int32_t row, std::int32_t column) const;
    // Returns a cell's height in metres above z = 0, or nothing off the grid.
    [[nodiscard]] std::optional<float> Height(std::int32_t row, std::int32_t column) const;
};

// Returns a level layout's walk grid, or nothing where it has none.
std::optional<Grid> GridOf(const gff4::File &layout);

} // namespace dao::core::navigation
