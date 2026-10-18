// A placeable's walk-grid patches out of the game's files, and a patch
// stamped into a level's walk grid and taken out of it as the engine does
// when a placeable changes state (DAOLib #176): a closed door blocks the grid
// and an open one does not, for a reimplementation (DAOParagonEngine).
//
// A placeable's patches are <ModelName>_<Appearance>.pwk, its placeable_types
// row's ModelName and the row's number ("%ls_%ld.pwk", read at load by
// 0x007500E0), in pathfindingpatches.erf: GFF4 "PWK " V1.0, whose root's
// EnvAreaRoomList (3016) holds one struct a state, each with an AreaGridArea
// (3110) laid out as a level's grid (dao/core/Grid.h). The loader stores the
// list's n-th grid at the placeable's +0x690[n], and the stamp's caller
// 0x0074DBA0 takes the patch of state s as +0x690[s], doing nothing for a
// state past the count at +0x694 (lib-agent-2's reading, 2026-10-18, and
// ours of the loader). So the file's order is the states' order as the
// engine uses it; that this is the state controller's own order (Closed_
// Unlocked, Closed_Locked, Open, ... for statecnt_door) is inferred from the
// door's shapes, not read. The file holds nothing else: no rectangle or use
// spot.
//
// Read 2026-10-18 over all 522 the game takes, every one in core's archive
// (78 more in the add-ins' enciphered archives were not read): 1,100
// states, 217 files of 1, 141 of 2, 113 of 3, 22 of 4 and 29 of 6. A patch's
// cells are a sixth of a metre (1,074 states) in the placeable's own space,
// its padding 0, so a cell's point is base + index / 6; the other 26 are
// empty, 0 by 0 cells of 0.5 m. Cells are 1 (130,735), 7 (1,597) or 8
// (126,753); 7 only in state 0 of 28 of the 29 six-state files, the doors'
// Closed_Unlocked, which a locked door's state 1 has as 8s. Rooms are all 0,
// links 255. The largest is plc_destcavewall_0_3.pwk's, 208 by 136.
//
// The stamp is 0x00A46CC0 (its one caller 0x0074DBA0, with the patch of the
// state, the placeable's position at +0x120 and facing at +0x130). For each
// of the patch's cells, column by column and each column's rows in turn, the
// cell's point is turned from +y to the facing, moved to the position, and
// the level's cell there found (Grid::Place, Grid::CellAt). Stamping writes
// the patch's kind there where the level's is under 8; taking out writes 1
// where it is 7 or 8, whatever the patch's cell. Cells off the level's grid
// read 9 and are left. Read 2026-10-18 in the GOG exe.
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/Grid.h"
#include "dao/core/RayCast.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::navigation
{

inline constexpr std::uint8_t kClosedDoor = 7; // a closed door that is not locked
inline constexpr std::uint8_t kBlocked = 8;    // a placeable's cells; a stamp writes only under this

// Returns a placeable's patch file's name: its placeable_types row's ModelName and the row's number.
std::string PatchName(std::string_view modelName, std::int32_t appearance);

// Returns a placeable's patches, indexed by state as the engine indexes them; none for a file that is no patch
// file. A state past the end has no patch, and the engine stamps nothing for it.
std::vector<Grid> PatchesOf(const gff4::File &pwk);

// Stamps a state's patch into a level's grid for a placeable at `position` facing `facing`, as the engine does
// when the placeable enters the state.
// @param facing the placeable's facing; its x and y alone are used.
void Stamp(Grid &grid, const Grid &patch, const Point &position, const Point &facing);

// Takes a state's patch out of a level's grid, as the engine does when the placeable leaves the state: a 7 or an
// 8 under any of the patch's cells is 1 again.
void Unstamp(Grid &grid, const Grid &patch, const Point &position, const Point &facing);

// Returns the level's cell under each of a patch's cells, in the order the engine visits them.
std::vector<Cell> Footprint(const Grid &grid, const Grid &patch, const Point &position, const Point &facing);

} // namespace dao::core::navigation
