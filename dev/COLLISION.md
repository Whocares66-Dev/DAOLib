# Collision: the eye-to-eye ray, the shapes it meets, placeables' masks, and rooms

Read 2026-10-18 by lib-agent-2 (with a subagent) in the GOG exe (`tools\daoexe.py`, capstone scans) and vanilla's files (ghilana, `tools\gff4.py`), for DAOParagonEngine. Nothing was run in the game. **Checked** means seen in the code or the files. **Inferred** means a reading of purpose. Level used: the Circle tower basement `bhm700ar_tower_basement.are`, whose `Layout` is `lak302d` (12 rooms, `packages/core/env/lak302d.arl` and `.opf`), and `oth000d`.

The reader is `dao/core/Collision.h` (#189, #197): a level's static physics, from its `.opf` or from the `.arl`'s binary copy of it (its actors, shapes, meshes and records), and a placeable's `.phy` shapes with the mask the engine builds from them. Only 132 of the 213 levels have an `.opf`: the add-ins' 55 levels and 26 of core's (brc201d, den000d, den200d, hrt000d, lak100n, orz100d, orz800d, ost100d among them) have only the `.arl`. Of those 81, 73 have the binary stream, read since #197 (section 4), and 8 have none (brc201d, combat2, den510c, lak519d, orz703d, rmtst, rmtst01, rmtst03: no `EnvStatPhys` at all, or an empty one). In outdoor levels the actor records' room ids are not `.rml` rooms: oth000d's are -1 and values like 0x10000, 0x10001 … 0x50004, inferred to be terrain chunks.


## 1. Static collision and the ray's filter

### The ray: 0x0067C530 (checked)

- It calls `area+0x62C` (the physics world) → `0x00537D60` → the `Physics::PhysicsScene` at world+0xCC. Then it calls scene slot 4 (`0x0082DEB0`) with (hit, from, to, 0x24, 0).
- Slot 4 does three things:
  - It builds an `NxGroupsMask {0x24, 0, 0, 0}`.
  - `0x0082D900(1, &m, &m)` sets the NxScene's filter: `setFilterBool(true)` (slot 0xE0), `setFilterConstant0(m)` (0xE4) and `setFilterConstant1(m)` (0xE8). It reads each back first through slots 0xF0, 0xF4 and 0xF8.
  - `0x0082A150(0, 0, 0)` sets `setFilterOps(AND, AND, AND)` (slot 0xDC; 0x00837690 passes each op through unchanged).
- Then it calls NxScene slot 0x1C0 with (ray, shapesType 3 = all shapes, hit, groups 0xFFFFFFFF, maxDist = |to − from|, hint −1, &groupsMask, cache 0). That is a closest-shape raycast; the 5th argument 0 picks it over slot 0x1BC.
- 0x0067C530 answers clear only when nothing is hit. The eye points are each position + 1.8 m (`0x00B004D8` = 1.8, a double).
- **The test is `(shape.bits0 & 0x24) != 0`**, so a shape blocks when it has Placeables (0x4) or StaticGeometry (0x20).
  - With every op AND and both constants the ray's mask, PhysX's equation `(G0 op K0) op (G1 op K1) == true` reduces to that test.
  - That PhysX puts the shape's mask on one side and the ray's on the other is from its documentation (inferred). With these ops the order does not matter.
  - The shapes' NxCollisionGroup plays no part: statics are group 5, creatures group 4.

### Where the static shapes come from (checked)

- **The game loads the level's static physics from the `.arl`**, not from the `.opf`.
  - `PhysicsScene` slot 34 (`0x00822B10`) reads `EnvAreaPathfindExport/AreaGridArea/EnvStatPhys` (3744) → `EnvStatPhysData` (3745).
  - It hands that to slot 36 (`0x0082B990`), which calls `NXU::loadCollection("Static Physics", type 0 = binary, data, len)` (`0x008B6110`).
  - `NXU::instantiateCollection` (`0x008B5930`) then runs with the notify object `Physics::UserDataEncoder` (`0x00B27154`, built at `0x0082A2B0`).
- **The `.opf` is the same collection as XML** (NxuStream2), with the same framing:
  - u32 total length;
  - u32 stream length;
  - the stream;
  - u32 n, then n 16-byte actor records;
  - u32 m, then m 12-byte shape records.
- For lak302d both copies have 964 actors, 1,278 shapes and identical actor records (checked). The masks were compared in the XML only.
- No code reading `.opf` was found (no string), so it is inferred to be the toolset's export. Reading the `.opf` XML is equivalent where there is one; the binary stream (section 4) is the one every level with physics has.
- **The load changes no mask** (checked). For each actor, `NXU_notifyActor` (`UserDataEncoder` slot 6, `0x0082A6F0`) does this:
  - It parses the actor's `userProperties` ("a,b,c…", `sscanf "%d"` with ","), sets actor→userData = &actorRecords[a], and gives each shape in turn shape→userData = &shapeRecords[next].
  - Then it calls `shape->setGroup(5)` (NxShape slot 2, `push 5`).
  - It makes no setGroupsMask call, so bits0 is the file's.
- **Actor record (16 bytes):** (−1, 0, **room id**, small int).
  - The room id is the `.rml` room's id (1101 … 1325) and is read as the room (section 3).
  - The 4th field is 0–15 in lak302d; inferred to be a sound material.

### What the shapes are (lak302d.opf, checked)

- Every actor is static (`hasBody="false"`) and in group 5. It has box, capsule, sphere and triangle-mesh shapes (917 box, 283 capsule, 29 sphere, 49 mesh). The `.opf` has no `NxMaterialDesc`, and every `materialIndex` is 0.
- **The room geometry is not one big triangle mesh in lak302d.** The modular interior pieces (`tvi_smwall_01_0.GOB`, `tvi_pil_09_0`, `tvi_lrgstair_0`…) each carry shapes named by role:
  - `ColW*` (walkable, bits0 0x82002020): a floor strip (box 4 × 0.57 × 0.03).
  - `ColNW*` (not walkable, bits0 0x81002020): a wall slab (box 4 × 0.08 × 2.75).
  - `ColWmesh` / `ColNWmesh`: triangle meshes.
  - Props (`prp_*.GOB`) carry `Box*`, `Capsule*` and `Sphere*` shapes, or meshes for rubble piles.
- **Each actor's place:** `globalPose` is a 3×3 matrix (rows), then the translation. Each shape's `localPose` is the same form. A box's `dimensions` are half-extents (inferred). A capsule is NxCapsule radius and height.
- **oth000d.opf** (326 actors):
  - 28 terrain triangle-mesh shapes (`oth000d_14_0`) at 0x80002020; these block.
  - 23 terrain mesh shapes at 0x80010000 (TerrainWall alone); these do not block the ray.
  - 158 tree capsules at 0x01001020; these block.
  - 728 rock capsules at 0x81002020 and 43 `ColNWBox` boxes; these block.
- **bits0 decoded:** the shape reader `0x00589DB0` builds bits0 from a model `.phy`'s `MMHShapeCollisionMask*` flags (`0x00589F99`–`0x0058A20F`, checked by the fork).

| Bit | Flag (label) |
|---|---|
| 0x1 | Items (6065) |
| 0x2 | Creatures (6066), the `.phy` flag |
| 0x4 | Placeables (6067) |
| 0x8 | Triggers (6068) |
| 0x20 | StaticGeometry (6069) |
| 0x100 | Water (6277) |
| 0x10000 | TerrainWall (6295) |
| 0x01000000 | Nonwalkable (6070) |
| 0x02000000 | Walkable (6305) |
| 0x80000000 | added when any bit is set |
| 0x2000 | added for StaticGeometry when the shape is not `MMHShapeFadeable` (6252) |
| 0xFFFFFFFF | All (6063) |

- None (6064) is read and ignored. Effects (6178) and Waypoints (6179) are not read.
- Those decode every value in the two `.opf`s:
  - 0x81002020 = StaticGeometry, not fadeable, Nonwalkable.
  - 0x82002020 = the same, Walkable.
  - 0x81012020 adds TerrainWall.
  - 0x80002020 = static and not fadeable.
  - 0x80010000 = TerrainWall alone.
- Trees' 0x01001020 is set in code: SpeedTree collision capsules, `0x0053167B mov [edi+0xD0], 0x1001020`. Bit 0x1000 is inferred to be "tree".
- **Creatures** (checked by the fork): the creature load (`0x006B2160`–`0x006B21A0`) puts its controller (creature+0x770) in actor and shape group 4 and sets groupsMask {0x10, 0, 0, 0} (`0x00838120`, NxShape slot 0x8C). So creatures are outside 0x24.

**For the stand-in:** load every static shape with its bits0 and treat a shape as an occluder when `bits0 & 0x24` is non-zero. Add the placeables (section 2). The ray runs eye to eye (+1.8 m) over the segment's length, and any hit means not seen.

## 2. Placeables (checked, by a fork; one claim inferred)

- **The shapes are in the model's `.phy`** (GFF4 "PHY", in `modelhierarchies.erf`). The chain is `.utp` `Appearance` → `placeable_types.gda` (table 93) `ModelName` → `<model>.phy`. Examples:
  - Appearance 94 → `PLC_LrgWdDrR_01_0` (`bhm700ip_sidedoor`, BaseType 1 door).
  - 121 → `PLC_CrateLarge_01_0`.
  - 315 → `plc_tevstatu03_0`.
- **The tree:** node `GOB` → (`mshh`, doors) → `cobj` → `shap`.
  - `cobj` fields: `MMHNodeCollisionObjType` 1, `MMHNodeCollisionObjDensity` 10, `MMHCollisionGroup` (6245) 0.
  - `shap` fields: `MMHShapeType` (2 box, 3 capsule), `MMHShapePosition`, `MMHShapeRotation` (a quaternion), `MMHShapeName`, `MMHCollisionGroup` 0, `MMHShapeFadeable`, `MMHShapeAllowEmitterSpawn`, `MMHShapeVolume`, and the `MMHShapeCollisionMask*` flags (0/1 each).
  - The size is in `MMHShapeTypeStruct`: `boxs` has `MMHShapeBoxDim` (half-extents, inferred); `caps` has `MMHShapeRadius` and `MMHShapeHeight`.
- **Examples:**
  - The door's `Box21` has dims (1.358, 0.1255, 1.970) at (1.386, 0, 1.994).
  - The crate's `Box12` has dims (0.567, 0.390, 0.326) at z 0.326.
  - The statue's `Capsule07` has r 0.687, h 1.778.
  - All three have Placeables = 1 and Nonwalkable = 1, so bits0 = **0x81000004**.
- **placeable+0x6D8:**
  - It is −1 at creation (`0x0074EFA6`).
  - At model-ready (`0x0074F7A0`), if it is still −1, it becomes `PhysicsObject` slot 20 (`0x00839080` → `PhysicsActor` `0x0081E6C0`): the OR of bits0 over every shape of the model. That is 0x81000004 for these models.
  - A model with no `.phy` has no physics object and never blocks (inferred).
- **Doors only** (`0x0074D350`) switch masks with their state through `PhysicsObject` slot 22 → `PhysicsActor` slot 24 (`0x0081E5F0`), which writes {mask, 0, 0, 0} to every shape. A blocking state restores +0x6D8 (bit 0x4 included, so it blocks); any other state sets 1 (Items, so it does not).
- Every other placeable keeps its `.phy` mask in every state, so a crate or statue always blocks sight.
- Side note: the walk-patch stamp `0x0074DBA0` skips placeables whose +0x6D8 has bit 0x1, unless +0x638 == 1.

## 3. Rooms: the walk grid's byte is not the room (checked)

- **Creature +0x41C is the room object and +0x150 is its id**, both written by `0x00695D10` (`[+0x41C] = room`, `[+0x150] = room ? room+0x14 : −1`). It is called from `0x00695D70` and `0x0068C0FD`.
- **The room comes from the physics, not the grid.**
  - `0x00695D70` takes the object's position (+0x120) plus (0, 0, its slot 0x1B0(0)) and calls `0x0067B3A0` → `EC::ChunkManager` (area+0x658, vtable `0x00AFFE24`) slot 2 (`0x005D1E00`).
  - That slot casts a ray from (x, y, z + 2.0) along (0, 0, −1), unbounded (FLT_MAX), collecting all hits (`PhysicsScene` slot 8, `0x0082E830`) under groupsMask **0x20** (StaticGeometry).
  - The room is taken from the first hit (in report order) whose room id (hit+0x34) is not −1 and whose flags (hit+0x20) lack bit 2. The id is looked up with ChunkManager slot 4.
  - The hit's room id comes from `0x0081C560(actor)`: the actor's userData record +8. That is the `.opf`/`EnvStatPhys` actor record's **room id**, or, for an object-owned actor, owner→+0x258→+0x14.
  - So the room is the room of the static floor under the creature.
- **Room ids are one numbering throughout:**
  - the `.arl`'s `EnvAreaRoomList/EnvRoomID`;
  - the `.rml` file names (`lak302d_1101`);
  - the `.opf` actor records;
  - the `.are`'s placeables' `RoomID1`/`RoomID2`;
  - each room object's +0x14 (`0x0067B430` finds a room by comparing +0x14 to the id).
- **`EnvRoomVisibilityList` (3047) and `EnvRoomConnectivityList` (3099) hold those ids** (checked). Their reader `0x005D3B0A`/`0x005D3BBE` copies them as int32 into room+0xB0 (visibility) and room+0xD0 (connectivity).
  - The perception room walk `0x00687EC0` → `0x00686A70` walks **+0xD0, the connectivity list**, by id.
  - For each neighbour it asks `0x00685100` for the doors between the two rooms and `0x0074BCC0` whether each is passable.
  - The walk itself steps over the connectivity list, but every room it passes through must be in the origin's visibility list, and RoomsSeen takes its candidates from the visibility list (corrected 2026-10-20, "Rooms seen" below).
- **Example, lak302d_1101.rml:** visibility [1103, 1325], connectivity [1103]. `lak302d_1103`: visibility and connectivity both [1101, 1325, 1113].
- **The grid's per-cell byte (field 3114) is DA's `AreaGridSoundData`.** xoreos calls it `AreaGridRoomID`, as core's Grid.h did until DAOLib #187, which renamed its list `Grid::sounds`.
  - In DA's own label set, 3110–3120 are `AreaGridArea`, `AreaGridSoundData` (3114), `AreaGridAbstractionSNumReg`, `AreaGridAbstractionSaddr`, `AreaGridLightSubsetData8` (3117), `AreaGridLightSubsetData16` (3118), `LightCanBeOccluded` and `AreaGridCellPadding` (3120). Grid.h already takes 3120's DA name.
  - The loader `0x00A48940` → `0x00A49470` keeps 3114 at grid+0x44. `0x00A49290` reads a cell's byte, and `0x00A46870` returns it with 3117's light-subset index (`AreaGridLightSubsetData8`, one u8 a cell into the layout's `LvlLightSubsetList`, 255 for none).
  - The byte goes to **creature+0xB6C** (`0x006AA5EB`, `0x006B608F`). The light-subset index goes to creature+0xB68, through `0x0069E550` to the creature's visual; creature+0x798 (`0x006B60E7`) is a flag set at `0x006B5FFB`, not a value from the grid (corrected 2026-10-18 by lib-agent-2).
- **In lak302d the byte is no room index:**
  - −1 on 36,288 cells; 5 on 21,021 (14,360 walkable); 15 on 512; 1 on 260; 0 on 131.
  - Every creature, door and trigger position in the `.are` that was tried reads 5, whatever its room (1101, 1103, 1325, 1291).
  - 15 is past the 12-room list.
  - Its values match the actor records' 4th field (0–15), inferred to be the floor's sound material.
- **For the port:** take a creature's room by casting down from 2 m above its feet onto the static shapes (bits0 & 0x20) and reading the hit actor's room id from the `.opf`'s actor record. Test reachability by walking `EnvRoomConnectivityList` through passable doors keyed by the `.are`'s RoomID pairs.

### Rooms seen (read 2026-10-20 by lib-agent-2, DAOParagonEngine #108; C checked, I inferred)

- **RoomsSeen** (`0x006886F0`, `__thiscall` on the area, the party's rooms, `ret 4`) [C]:
  - It returns at once when the layout has no rooms (layout slot 1, +0x428) or the global `0x00C34BD4` (never written) is set, and clears area +0x608.
  - For each party room P: P is in view, and so is each id N of **P's visibility list** (`sees`, +0xB0) that passes the test `0x00687EC0(P, N)`. The layout's adjacency is not used.
  - Every room in view is added to the area's list of rooms seen (+0x460, saved as `SavegameAreaRoomsViewed` 16018; nothing removes an entry).
  - Then every room of the layout: in view, `SetSeen(1)` and `Show(true)`; in the seen list, `SetSeen(2)` and `Show(true)`; otherwise `SetSeen(0)` and `Show(false)` (slots 22 and 31, `0x00688937`–`0x0068898E`).
- **The test** `0x00687EC0` (`ret 8`) [C]:
  - If N's connectivity list (+0xD0) is empty: N passes when it is P or in P's visibility list (CRoom slot 21, `0x005D22E0`). No door is asked.
  - Otherwise a depth-first walk from P (`0x00686A70`, visited ids at area +0x470) over **connectivity** ids. Every room it passes through must be in P's visibility list. A step from R to X is blocked only when there are doors registered for the pair (`0x00685100`, a map at area +0x634 keyed by the two ids, min first) and every one of them blocks: `DoorBlocks` (`0x0074BCC0`) is the door active (+0x2C5), with a state controller (+0x654), whose current state's record (+0x658, `0x00A3BAC0`, stride 20) has `BlockVis` (+0x10) ≠ 0, read live. Distance and the camera play no part.
- **When it runs** [C]: the area's update `0x0068AF30` (while area +0x3FC is set) gathers the party members' rooms in order (`0x00635FD0`, each member's +0x150, −1 left out) into +0x61C. When that list differs from the last (+0x60C), or area +0x608 is set, it runs RoomsSeen. +0x608 is set by the area's constructor `0x00688CB0` (so the first update after a load rebuilds every room's state from the saved list), a door's registration `0x0068C190`, `SetPlaceableState` `0x0074F9D0` when the door's two room ids are not both −1, and the area's unload `0x0067C980` (which also sets every room in view and shown).
- **SetSeen** (slot 22, `0x005D2210` → `0x005D2150`) [C]: it keeps the state at room +0x100 and starts a fade of the room's **darkness level** (from +0x334 to +0x32C): 1.0 never seen, 0.7 seen before (`0x00B58750`), 0 in view, over 0.7 s (`0x00AFFFAC`), or at once while the game mode is Loading (9). RoomsSeen never asks for it at once.
- **The room's update** (slot 23, `0x005D2DF0`) advances the fade while the room is loaded (the curve `0x005CBC90` not read), forces the level to 0 when its lit byte (+0x2F4) is 0, and when the level changes stores it at +0x364 and calls **Show(level ≠ 1.0)** (`0x005D2800`) [C]. So a never-seen room ends hidden at 1.0, and a room coming into view is shown from its fade's first step.
- **Show** (slot 31, `0x005D2360`) touches only the room's own scene objects, the `.rml`'s placed models (+0x2A8; effect models such as the candles' among them) [C]:
  - It hands each the level through the scene object's slot 68 (`0x00546970`), as the tint (r, g, b, level) with the object's rgb, black (0, 0, 0) from its constructor; the material constant is `mml_vTintColor` (string `0x00AF8E28`, used at `0x0055A9DB`). In `ch1.psh` that is colour × (1 − d) after the lights and before the fog [C for ch1, I for the other materials that declare it].
  - It sets or clears the object's flag 1 (slots 39 and 40). Cleared, every node gets flag 0x200, and `0x0053AD40` disables the node's Umbra objects, so the visibility query does not return them and they are not drawn, their lights not gathered and their `UpdateOnlyWhenVisible` emitters not updated [C for the flags and Umbra, I for the draw side].
  - Seen before: drawn, tinted (0, 0, 0, 0.7), colour × 0.3 [C for the formula].
- **Outdoors** [C]: an outdoor cell's room is made in view (`0x005CF790`: seen 1), and its `.rml` holds empty visibility and connectivity lists (`ost100d_10_5_0.rml`), which is why every cell outdoors reads in view.
- **Creatures** [C]: nothing makes a creature in a never-seen room inactive; its AI, perception (by the rooms' visibility masks) and sounds go on. Its attached effects emit only while its room's level is below 0.7 (`0x006BD4D0`, its slot 77). Other readers of +0x100: the explore pick (`0x0070EB5B`, `seen == 0` for the hit point's room), `0x00764820` (skips unless in view), and a dead store at creature +0x798.
- **A never-seen room's creatures are not drawn at all** (seen in vanilla by elgara-agent's capture, `bhm700ar_tower_basement`, frames 14921–14922): three Sentinels in room 1325 (never seen, shown false, loaded) had no draw call in the main, depth (`skin_depth`, `static_depth`) or shadow (`pssmskinmesh`) passes, while a Sentinel in room 1103, in view, drew with `char2.vsh`, `ch2.psh`, `ch2armtnt.psh`. Culled, not drawn black. What culls them is not read [I: Umbra, by the room's disabled objects or its cell].
- **Seen in play** (eluvian-agent, 2026-10-20, Eluvian #164; the Elven Ruins, layout `brc200d`, rooms 1574 to 1580):
  - With the party standing still in 1574, opening `door_south` (1574–1579) turned 1579 from never seen to in view within 1.5 s; shutting it turned 1579 to seen before; opening it again, back in view.
  - Walking in, neighbours turned in view before anyone entered them (1578 and 1580 from 1579, 1576 from 1577), and rooms left behind went to seen before.
  - Every step fits the rooms' `.rml` sees and connects and the area's doors' RoomID1 and RoomID2 pairs as read above: 1574–1579, 1579–1576, 1577–1576, 1577–1578, 1580–1576 (locked), 1580–1575 (a mirror), 1579–1578 (a pressure plate). Inferred: a shut door's state has BlockVis 1.
  - A never-seen room's pixels are exactly (0, 0, 0), the interface drawn over them.
- **Not read:** the fade's curve and who calls slots 23 and 28; the lit byte's setter; the explore pick's branches; `0x00764820`'s object; the draw-side use of node flag 0x200; the entries Object+0x64 hands the tint to; the other materials' shaders; whether a hidden room's lamps and probes stop lighting other things. The 2026-10-01 case of a room behind a just-opened door staying unseen is not seen again (below): likely a door with no RoomIDs, or a room not in the party room's visibility list [I].

## 4. The `.arl`'s binary stream (#197)

Read 2026-10-19 from the files, not the exe: the stream was laid against the `.opf`'s XML of all 132 levels that have both, and `dao/core/Collision.cpp`'s reader gives the same actors, shapes, poses, masks, meshes and records from either. **Checked** means that comparison (below). The exe's loader (`NXU::loadCollection`, `0x008B6110`, type 0) was not read through; it is the generic NxuStream2 reader, and the block numbers below are what the files say. No PhysX source was consulted.

### The stream

`EnvStatPhysData` is framed as an `.opf` is (u32 file length, u32 stream length, the stream, the actor records, the shape records). The stream is `"NXUSTREAM"`, two zero bytes, then the SDK's version and NXU's as u32 (272 and 103, as the XML's `sdkVersion` and `nxuVersion`: 272 is 0x110, and 281 is 0x119), then blocks (checked).

A **block** is a 16-byte head, then its own fields, then the blocks it holds:

| | |
|---|---|
| u32 | 0x00ABCDEF |
| u32 | type |
| u32 | length: from the head's first byte to the block's last, so the head's 16 bytes are in it and the next block's head starts at the end |
| u32 | 0x01234567 |

The blocks nest by their lengths. A block's fields are fixed-size or strings; a **string** is a u32 length, then for a length over 0 that many characters and a NUL (an empty string is the four bytes of 0). The reader finds a block's children by their heads after its own fields, so it need not know all of the fields. Everything is little-endian, and a pose is the XML's twelve numbers rearranged as three rows of four, each row of the turn then that row's translation (checked: an actor turned by pi about z, translation (12, -76, 0): -1, 1.82e-6, 0, 12 / -1.82e-6, -1, 0, -76 / 0, 0, 1, 0).

### The blocks

The type numbers are NXU's classes' and move with the SDK's version: the SDK 281 ones are the 272 ones plus one from the shape description on, and plus seven for the collection, its description, the scene and the end.

| Block | SDK 272 | SDK 281 | Fields before its blocks |
|---|---|---|---|
| the stream | 0 | 0 | none; holds the collection |
| collection | 99 | 106 | "Physics", u32 0, SDK version, NXU version; holds the description, the meshes, the scene |
| SDK description | 98 | 105 | hwPageSize, hwPageMax, hwConvexMax |
| triangle mesh | 9 | 9 | id string, 40 bytes (userProperties, the empty material indices, the convex edge threshold 0.001 and the rest of NxTriangleMeshDesc, the same in every one seen); holds a simple mesh |
| simple mesh | 7 | 7 | u32 n, n points of three floats, u32 t, t triangles of three u32 (u16 when the u32 flags that end the block have bit 1, NX_MF_16_BIT_INDICES; no mesh seen has it), the flags |
| scene | 95 | 102 | 44 bytes; holds the actors (and two groups masks of the scene) |
| actor | 41 | 42 | id, userProperties (the XML's "a,b,c"), u32 hasBody, name, the pose, 16 bytes (0, 0, 0, 1 as floats), density (float), flags (u32), group (u32), 2 or 8 bytes; holds the shapes in order |
| box | 32 | 33 | three floats, the half extents; holds a shape description |
| sphere | 34 | 35 | radius; holds a shape description |
| capsule | 35 | 36 | radius, height, u32 flags; holds a shape description |
| triangle mesh shape | 38 | 39 | the mesh's id string (meshData), 8 bytes (mesh flags, paging mode); holds a shape description |
| shape description | 31 | 32 | userProperties, name, the local pose, 16 bytes (0, 0, 0, 1), u32 shape flags, u32 group, 16 bytes (material, density, mass, skin width); holds the groups mask, and in SDK 281 ends with 4 more bytes |
| groups mask | 17 | 17 | four u32, bits0 to bits3 |
| the end | 101 | 108 | none |

SDK 272 is the 132 levels with an `.opf`; SDK 281 is the 18 core levels without one that were looked at, a later PhysX 2.8. Box, sphere, capsule and mesh are all that were seen: no plane, convex or height field.

### The comparison (checked, `dao_core_tests "[collision-real]"`, the GOG install, 2026-10-19)

- All 205 levels with a stream (every `.arl` the index reads: core's 158 and the add-ins' 55, 8 of them with no physics) read to the end, with 113,613 actors, 175,019 shapes and 4,875,044 triangles. Each level's actors are as many as its actor records and its shapes as many as its shape records.
- For the 132 with an `.opf`, the two readings are the same in every field: ids, names, kinds, sizes, poses, groups, masks, records, rooms and meshes (points and triangles). The one difference in kind is the XML's rounding: it writes a number with nine decimals (0.00000182 for 1.82e-6, and halves rounded away from zero), which loses the small turns (the floats of about 325,000 groups of numbers differ this way) and is the same as printing the binary's float that way and reading it back. The binary stream is the exact one.
- 73 of the 81 levels without an `.opf` have shapes now; the other 8 have no physics.
