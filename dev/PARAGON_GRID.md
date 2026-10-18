# For lib-agent, from paragon-agent: the walk grid, read

A note from `C:\project\DAOParagonEngine` (paragon-agent, 2026-10-17), for DAOLib's owner to keep, move or drop: what a level's walk grid holds, read from the files and the GOG exe, so its reader in core is mechanical. The user put the grid to me while you read the colour curve; the reader is yours (core), this is its spec. Addresses are GOG's.

## Where it is

`<level>.arl` (GFF4 `ARL`), `EnvAreaPathfindExport` (ARPA) / `AreaGridArea` (NAVI, template size 0x60). Fields by label (dev/gff4_fields.csv):

| Label | Field | Type | Meaning |
|---|---|---|---|
| 3086 | AreaGridNbColumns | int | columns (x) |
| 3087 | AreaGridNbRows | int | rows (y) |
| 3088 | AreaGridCellSize | float | metres a cell, 0.5 in lak303d and den000d |
| 3089 | AreaGridClearance | float | 2.1 in both; how it is used not read |
| 3090 | AreaGridBasePos | vector | the corner of cell (row 0, column 0) in the level's space; its z (9.83 in lak303d) is not the floor's |
| 3091 | AreaGridNormal | vector | (0, 0, 1) |
| 3092 | AreaGridData | uint8 list | one byte a cell, row by row: cell (row, column) is at row * columns + column |
| 3093 | AreaGridHeight | int16 list | one a cell, same order; the loader checks the field is GFF4 type 3 (0x00A48CAF) |
| 3094 | AreaGridAbstractSectorSize | int | 8 in lak303d, 11 in den000d |
| 3095 | AreaGridAbstractSectors | SECT list | per sector: AreaGridRoomNumberOfLinks, AreaGridLinks (an offset into the memory below) |
| 3096 | AreaGridAbstractMemory | uint8 list | the abstraction's graph, 4,252 bytes in lak303d; not read yet |
| 3114 | AreaGridSoundData | int8 list | not a room id (xoreos's name for it, AreaGridRoomID, misled this row): a cell's sound data, given to a creature on it at +0xB6C, probably the floor's sound material (inferred); lak302d: 5 almost everywhere, 15, 1, 0; -1 at the edges. A creature's room is the .rml room id of the static floor under it (dev/COLLISION.md, section 3; corrected 2026-10-18, DAOLib #187) |
| 3117 | AreaGridLightSubsetData8 | uint8 list | one a cell, 255 for none, else an index into the layout's `LvlLightSubsetList` (3800): a per-cell sun and light occlusion (xoreos's `AreaGridLink` misled this row; lak303d: all 255; 62 of 213 grids hold indices; DAOLib #187) |
| | AreaGridLinkID, AreaGridLinkRoomID | | empty and 0.25 in lak303d |
| | EnvStatPhys / EnvStatPhysData | uint8 list | a PhysX (NovodeX) cooked stream, "NXUSTREAM" after 8 bytes: the level's static collision, 403,551 bytes in lak303d |

The loader is 0x00A48B..: it takes each field by label (`push 0xC14` at 0x00A48B5A for the data, `push 0xC15` at 0x00A48C8D for the heights) and hands the arrays to the grid's set-up, 0x00A49470, which keeps them as they are: the cell bytes at the grid's +0x3C, the heights at +0x40, three more arrays at +0x44, +0x48 and +0x4C; columns at +0x20, rows at +0x24.

## A cell's byte (read in the exe)

- **Its kind is the low four bits**: 0x00A49070 (`__thiscall (row, column)`, `ret 8`) answers `byte & 0x0F`, and 9 for a cell off the grid. With what Addresses.h already says of the kinds (under 5 walkable, 5 and 6 a creature's stamp, 7 and up not walkable), the files' three values are: **1 walkable**, **9 not walkable** (a band along every wall and round every pillar and piece of furniture, the clearance already in it, and the kind off the grid), **10 not walkable** (outside the floor). lak303d: 14,784 of 1, 8,687 of 9, 22,963 of 10.
- **Bits 4 and 5 and bits 6 and 7 are two counters the engine keeps at run time**, 0 in the files: 0x00A49160 reads bits 4 and 5 (`(byte >> 4) & 3`); 0x00A491A0 raises bits 6 and 7 by one up to 3 and 0x00A491F0 lowers them by one down to 0. These are Addresses.h's "counts of what crowds it".

## A cell's height (read from the data)

**Sixteenths of a metre above z = 0** (cell size / 8 also fits, both 0.0625 at a cell of 0.5). Checked against the level's geometry under seven cells of lak303d (Paragon's `examples/floor`, every triangle of every placed model under a point): stored 3 under a surface at 0.186 m, 5 under 0.319 (the dais), 6 under 0.394, 8 under 0.492 (the dais), 14 under 0.959 (stairs up), -47 under -2.73 (stairs down), 0 under the floor at 0. The ratio is 0.058 to 0.068, the spread being the surfaces' slope across a cell. The value 84 (5.25 m) lies where the level's own geometry has only the floor: a walkway that is a placeable, not level art, I'd guess. The exe's own conversion was not found (no `movsx`/`fild word` over the heights near the grid's code); which of the two constants is the engine's is open.

## Not read yet

- The abstraction: 952 sectors for lak303d's 218 x 213 cells at a sector size of 8 (28 x 27 is 756, so a sector is not simply an 8 x 8 block, or there are more than one per block); each sector's `AreaGridLinks` is an offset into `AreaGridAbstractMemory` (values seen: 0, 4, 25, 3320, 3364, 4252 = its end). This is the hierarchical A*'s coarse graph (MapAbstractionSearchEnvironment).
- The three arrays at +0x44 to +0x4C (room ids, links, and one more).
- What AreaGridClearance and the base's z are for.

## What Paragon would call

`navigation::Grid GridOf(const gff4::File &arl)`: columns, rows, cell size, clearance, base, normal; the cell bytes and heights as arrays (or `Kind(row, column)`, `Walkable(row, column)` (kind under 5), `Height(row, column)` in metres); room ids per cell; the sectors as (links, offset) and the memory as bytes, as written, until the abstraction is read. Tests over lak303d's counts above and den000d (a terrain level: 518 x 387 at 0.5, sector size 11, 2,160 sectors) would hold it.
