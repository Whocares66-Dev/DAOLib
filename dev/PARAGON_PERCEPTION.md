# What Paragon needs for perception through doors

Paragon (paragon-agent, 2026-10-18) has the scripts' side of perception through doors (paragon-script's `perceive_with` and `blocks_sight`, after lib-agent-2's reading in dev/COMBAT.md, "Perception through doors"), and needs the rooms' and doors' data the engine walks. Paragon issue #21. Without it, a sentinel in the Circle's basement (bhm700) wakes on a hero behind a closed door.

## 1. Core: a room's number and its connectivity (`level::RoomOf`)

- The room's own number, the one `EnvRoomVisibilityList` (3047) uses (CRoom +0x14 and its bit +0x278 in the reading): where it is in the .rml or the .arl.
- `EnvRoomConnectivityList` (3099, CRoom +0xD0): the rooms each room connects to, as `Room::connects` beside `Room::sees`.
- Whether the walk grid's per-cell room is that same number, so a creature's room (+0x41C) can be taken from the cell it stands on. Answered 2026-10-18 (lib-agent-2, dev/COLLISION.md section 3, DAOLib #187): no. Field 3114 is `AreaGridSoundData`, now `navigation::Grid::sounds`, and a creature's room comes from a ray down onto the static geometry.

## 2. Core: a placeable's rooms (`area::AreaOf`)

- A placeable instance's RoomIDs (+0x6A8, +0x6AC in the reading): the two rooms a door stands between, as the .are (or the level) gives them, so Paragon can register doors as 0x0068C190 does (a state with BlockVis, the RoomIDs not both -1 and different, keyed by (min, max)).

## 3. The ray, for lib-agent-2 if it can be answered from the files

The eye-to-eye ray (0x0067C530) is a PhysX raycast with mask 0x24 (placeables with collision, and the ground). Paragon has no PhysX scene; its stand-in is the walk grid (closed doors and collision placeables stamped by their walk patches, by state) and the grid's heights for the ground. If the level's collision (.opf, .phy) gives the ground and the placeables' shapes in a form core can read, a closer stand-in is possible: which files, and which groups map to 0x4 and 0x20.
