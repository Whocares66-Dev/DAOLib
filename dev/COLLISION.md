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
- **No margin on the rays** (read 2026-10-22 by lib-agent-2 with a subagent): `NxShapeDesc`'s constructor leaves `skinWidth` at −1, the scene's default (0x0082B0F0, 0x0081F40E) [C, read by the subagent, not re-checked]. No call changing the scene's skin width was found [I: not searched exhaustively], and a PhysX 2.x raycast does not inflate shapes by it [I: SDK behaviour]. 0x0067C530 is a closest-shape ray with group mask 0x24 (placeables and static) through the scene's slot 4 (0x0082DEB0) [C]. The only tolerance the exe adds is the 0.1 m "hit within 0.1 m of the target" (COMBAT.md:124). Which mask `LineOfSight` 0x0067C3F0 passes was not read.

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
  - The 4th field is 0–15 in lak302d; read to be a sound material. (read 2026-10-23 by paragon-agent, checked by lib-agent-2; "A hit's sound material" below).

### What the shapes are (lak302d.opf, checked)

- Every actor is static (`hasBody="false"`) and in group 5. It has box, capsule, sphere and triangle-mesh shapes (917 box, 283 capsule, 29 sphere, 49 mesh). The `.opf` has no `NxMaterialDesc`, and every `materialIndex` is 0.
- **The room geometry is not one big triangle mesh in lak302d.** The modular interior pieces (`tvi_smwall_01_0.GOB`, `tvi_pil_09_0`, `tvi_lrgstair_0`…) each carry shapes named by role:
  - `ColW*` (walkable, bits0 0x82002020): a floor strip (box 4 × 0.57 × 0.03).
  - `ColNW*` (not walkable, bits0 0x81002020): a wall slab (box 4 × 0.08 × 2.75).
  - `ColWmesh` / `ColNWmesh`: triangle meshes.
  - Props (`prp_*.GOB`) carry `Box*`, `Capsule*` and `Sphere*` shapes, or meshes for rubble piles.
- **Each actor's place:** `globalPose` is a 3×3 matrix (rows), then the translation. Each shape's `localPose` is the same form. A box's `dimensions` are half-extents (checked 2026-10-22, section 2). A capsule is NxCapsule radius and height.
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

### A hit's sound material, and the world ray's record (read 2026-10-23 by paragon-agent, checked by lib-agent-2)

GOG 1.05 addresses. [C] read in the code, [S] seen in play, [I] inferred.

- RaycastClosestSegment's result +0x38 is `0x0081C4F0(actor)` [C].
- 0x0081C4F0 answers by actor kind [C]: a creature controller's actor (kind 4 by its slot 0x58) gives 0; an actor whose userData record's owner (`u+4`) is null gives `[u+0xC]`, the level .opf's actor record's fourth word, core's `ActorRecord::sound` (its sibling `GetActorRoomId` 0x0081C560 gives `[u+8]`, the room, which confirms the layout {first, second, room, sound}); a placeable's shape (an actor with an owner) asks the owner's slot 0x48, then that object's slot 4, then 0x005429C0, which gives the owner's `+0x2F0` when it is not negative, else 0. What sets `+0x2F0` was not read.
- The world ray (0x00648C60) copies it to its own record's +0x28 [C], and the projectile's wall impact reads it there as a row of sound_geometry_mat_types (table 156) [C]; see AUDIO.md, "A projectile's wall impact".
- Over the install, in Paragon: stone gives the arrow's `impact_materials/metal` event, Hollow_wood its `wood` event [S].
- It comes from the collision file, not the walk grid [C].
- The world ray's record (0x00648C60):
  - +0x20 and +0x24 are swapped from the physics record's; +0x24 is the shape's groups mask [C]. It also sets its +0x18 to (not bit 24 of the hit's +0x20), the Nonwalkable bit cleared [C, 0x00648D4B..0x00648D6A].
  - +0x28 is the physics record's +0x38, the sound material above [C].

## 2. Placeables (checked, by a fork; one claim inferred)

- **The shapes are in the model's `.phy`** (GFF4 "PHY", in `modelhierarchies.erf`). The chain is `.utp` `Appearance` → `placeable_types.gda` (table 93) `ModelName` → `<model>.phy`. Examples:
  - Appearance 94 → `PLC_LrgWdDrR_01_0` (`bhm700ip_sidedoor`, BaseType 1 door).
  - 121 → `PLC_CrateLarge_01_0`.
  - 315 → `plc_tevstatu03_0`.
- **The tree:** node `GOB` → (`mshh`, doors) → `cobj` → `shap`.
  - `cobj` fields: `MMHNodeCollisionObjType` 1, `MMHNodeCollisionObjDensity` 10, `MMHCollisionGroup` (6245) 0.
  - `shap` fields: `MMHShapeType` (2 box, 3 capsule), `MMHShapePosition`, `MMHShapeRotation` (a quaternion), `MMHShapeName`, `MMHCollisionGroup` 0, `MMHShapeFadeable`, `MMHShapeAllowEmitterSpawn`, `MMHShapeVolume`, and the `MMHShapeCollisionMask*` flags (0/1 each).
  - The size is in `MMHShapeTypeStruct`: `boxs` has `MMHShapeBoxDim` (half-extents, checked 2026-10-22); `caps` has `MMHShapeRadius` and `MMHShapeHeight`.
    - **Half-extents, read 2026-10-22 by lib-agent-2 with a subagent** [C]: the loader reads `MMHShapeBoxDim` (field 6071, push at 0x0058A321) into the box prototype (+0xE0) and copies the three floats as they are (0x00837A40) into `NxBoxShapeDesc.dimensions` (+0x6C..+0x74, 0x0081F4A5..0x0081F4D6), with no factor; PhysX takes them as half-extents. The file's `MMHShapeVolume` is the plain product of the three numbers (`prp_4wcart01_0.phy`: 0.826 × 0.2247 × 0.5170 = 0.0959868), not 8× it, so it is no evidence for full dimensions. The boxes' centres (z 0.5166 for a z of 0.517) put them on the ground as half-extents.
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
  - **A miss clears the room; no last room is kept** (read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2) [C]: 0x00695D70 passes the ray's answer to SetRoom 0x00695D10 untested (0x00695DCD). With no area it passes 0. 0x0067B3A0 also answers 0 when area +0x658 is null. SetRoom stores room 0 and id −1 for a null room, and skips the store only when the same room and id are already set (0x00695D23–0x00695D31). So an object over no static geometry has no room, and the perception update then exits at once (COMBAT.md, +0x41C null).
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
  - Its values match the actor records' 4th field (0–15), read to be the floor's sound material. (read 2026-10-23 by paragon-agent, checked by lib-agent-2; "A hit's sound material" below).
- **For the port:** take a creature's room by casting down from 2 m above its feet onto the static shapes (bits0 & 0x20) and reading the hit actor's room id from the `.opf`'s actor record. Test reachability by walking `EnvRoomConnectivityList` through passable doors keyed by the `.are`'s RoomID pairs.

### Rooms seen (read 2026-10-20 by lib-agent-2, DAOParagonEngine #108; C checked, I inferred)

- **RoomsSeen** (`0x006886F0`, `__thiscall` on the area, the party's rooms, `ret 4`) [C]:
  - It returns at once when the layout has no rooms (layout slot 1, +0x428) or the global `0x00C34BD4` (never written) is set, and clears area +0x608.
  - For each party room P: P is in view, and so is each id N of **P's visibility list** (`sees`, +0xB0) that passes the test `0x00687EC0(P, N)`. The layout's adjacency is not used.
  - Every room in view is added to the area's list of rooms seen (+0x460, saved as `SavegameAreaRoomsViewed` 16018; nothing removes an entry).
  - Then every room of the layout: in view, `SetSeen(1)` and `Show(true)`; in the seen list, `SetSeen(2)` and `Show(true)`; otherwise `SetSeen(0)` and `Show(false)` (slots 22 and 31, `0x00688937`–`0x0068898E`).
- **The test** `0x00687EC0` (`ret 8`) [C]:
  - If N's connectivity list (+0xD0) is empty: N passes when it is P or in P's visibility list (CRoom slot 21, `0x005D22E0`; for a chunk the slot is 0x00722AE0, always true). No door is asked. A false answer falls through to the walk below and does not fail the test (corrected 2026-10-24 by lib-agent-2).
  - Otherwise a depth-first walk from P (`0x00686A70`, visited ids at area +0x470) over **connectivity** ids. Every room it passes through must be in P's visibility list. A step from R to X is blocked only when there are doors registered for the pair (`0x00685100`, a map at area +0x634 keyed by the two ids, min first) and every one of them blocks: `DoorBlocks` (`0x0074BCC0`) is the door active (+0x2C5), with a state controller (+0x654), whose current state's record (+0x658, `0x00A3BAC0`, stride 20) has `BlockVis` (+0x10) ≠ 0, read live. Distance and the camera play no part.
- **When it runs** [C]: the area's update `0x0068AF30` (while area +0x3FC is set) gathers the party members' rooms in order (`0x00635FD0`, each member's +0x150, −1 left out) into +0x61C. When that list differs from the last (+0x60C), or area +0x608 is set, it runs RoomsSeen. +0x608 is set by the area's constructor `0x00688CB0` (so the first update after a load rebuilds every room's state from the saved list), a door's registration `0x0068C190`, `SetPlaceableState` `0x0074F9D0` when the door's two room ids are not both −1, and the area's unload `0x0067C980` (which also sets every room in view and shown).
- **SetSeen** (slot 22, `0x005D2210` → `0x005D2150`) [C]: it keeps the state at room +0x100 and starts a fade of the room's **darkness level** (from +0x334 to +0x32C): 1.0 never seen, 0.7 seen before (`0x00B58750`), 0 in view, over 0.7 s (`0x00AFFFAC`), or at once while the game mode is Loading (9). RoomsSeen never asks for it at once.
- **The room's update** (slot 23, `0x005D2DF0`) advances the fade while the room is loaded (the curve `0x005CBC90` is a sine ease, read 2026-10-22: CAMERA.md, "The roof fade"), forces the level to 0 when its lit byte (+0x2F4) is 0, and when the level changes stores it at +0x364 and calls **Show(level ≠ 1.0)** (`0x005D2800`) [C]. So a never-seen room ends hidden at 1.0, and a room coming into view is shown from its fade's first step.
- **Show** (slot 31, `0x005D2360`) touches only the room's own scene objects, the `.rml`'s placed models (+0x2A8; effect models such as the candles' among them) [C]:
  - It hands each the level through the scene object's slot 68 (`0x00546970`), as the tint (r, g, b, level) with the object's rgb, black (0, 0, 0) from its constructor; the material constant is `mml_vTintColor` (string `0x00AF8E28`, used at `0x0055A9DB`). In `ch1.psh` that is colour × (1 − d) after the lights and before the fog [C for ch1, I for the other materials that declare it].
  - It sets or clears the object's flag 1 (slots 39 and 40). Cleared, every node gets flag 0x200, and `0x0053AD40` disables the node's Umbra objects, so the visibility query does not return them and they are not drawn, their lights not gathered and their `UpdateOnlyWhenVisible` emitters not updated [C for the flags and Umbra, I for the draw side].
  - Seen before: drawn, tinted (0, 0, 0, 0.7), colour × 0.3 [C for the formula].
- **Outdoors** [C]: an outdoor cell's room is made in view (`0x005CF790`: seen 1), and its `.rml` holds empty visibility and connectivity lists (`ost100d_10_5_0.rml`), which is why every cell outdoors reads in view.
  - A chunked area's cells are `EC::CExteriorChunk`s, made by the exterior load 0x005C8E50 with all-ones room masks. Their slot 21 (+0x54) is 0x00722AE0, always true, so the test `0x00687EC0` passes between any two chunks; COMBAT.md, "Perception through doors", has the masks (read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2) [C].
- **Creatures** [C]: nothing makes a creature in a never-seen room inactive; its AI, perception (by the rooms' visibility masks) and sounds go on. Its attached effects emit only while its room's level is below 0.7 (`0x006BD4D0`, its slot 77). Other readers of +0x100: the explore pick (`0x0070EB5B`, `seen == 0` for the hit point's room), `0x00764820` (skips unless in view), and a dead store at creature +0x798.
- **A never-seen room's creatures are not drawn at all** (seen in vanilla by elgara-agent's capture, `bhm700ar_tower_basement`, frames 14921–14922): three Sentinels in room 1325 (never seen, shown false, loaded) had no draw call in the main, depth (`skin_depth`, `static_depth`) or shadow (`pssmskinmesh`) passes, while a Sentinel in room 1103, in view, drew with `char2.vsh`, `ch2.psh`, `ch2armtnt.psh`. Culled, not drawn black. What culls them is not read [I: Umbra, by the room's disabled objects or its cell].
- **Seen in play** (eluvian-agent, 2026-10-20, Eluvian #164; the Elven Ruins, layout `brc200d`, rooms 1574 to 1580):
  - With the party standing still in 1574, opening `door_south` (1574–1579) turned 1579 from never seen to in view within 1.5 s; shutting it turned 1579 to seen before; opening it again, back in view.
  - Walking in, neighbours turned in view before anyone entered them (1578 and 1580 from 1579, 1576 from 1577), and rooms left behind went to seen before.
  - Every step fits the rooms' `.rml` sees and connects and the area's doors' RoomID1 and RoomID2 pairs as read above: 1574–1579, 1579–1576, 1577–1576, 1577–1578, 1580–1576 (locked), 1580–1575 (a mirror), 1579–1578 (a pressure plate). A shut door's state has BlockVis 1, checked in the data: `statecnt_door.gda` gives 1 for Closed_Unlocked and Closed_Locked, 0 for Open, Dead, Open_2 and Dead_2; every state controller's table has its own BlockVis column (`statecnt_door_secret`, `statecnt_bridge` and the rest), which DoorBlocks (0x0074BCC0) reads through the current state's record.
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

## 5. The creature's overlap test: 0x006B1600 (read 2026-10-22 by lib-agent with a subagent, DAOLib #233, for DAOParagonEngine #144)

Read in the GOG exe. [C] = read in the disassembly, [I] = inferred. Builds on PATHFINDING.md:885-891 and :976 (the snap 0x006B1790 and its call) and COLLISION.md section 1 (the ray's filter, the bits0 table).

### What it is

`bool __thiscall Creature::OverlapsPhysics(const float4* footPos /* or null */)`, `ret 4` [C 0x006B176E]. It returns false (al = 0) at once when the creature has no controller (`[this+0x770] == 0`), no area (`this->vtable+0xA0` returns 0), no physics world (`area+0x62C`) or no scene [C 0x006B1634-0x006B1667, 0x006B1771].

### 1. The call at 0x006B174D

- `ecx` is **not an NxScene**. It is the `Physics::PhysicsScene` wrapper: `0x00537D60(0)` is `mov eax,[ecx+0xCC]` [C], the same object COLLISION.md section 1 reaches for the ray (`Physics::PhysicsScene`, whose NxScene is at `[PhysicsScene+0x24]` [C 0x0082EDA8]).
- Its vtable slot 0x34 (index 13) is `0x0082EC20`, `Physics::PhysicsScene::OverlapCapsule` (address library ID 22526) [C: the vtable word is what the call reads; the name is the library's, and the function body matches it]. Its sibling `0x0082F0D0` is `OverlapBox` (ID 22527), and the ray's slot 4 is `RaycastClosestSegment` (`0x0082DEB0`, ID 22520).
- `OverlapCapsule` does the PhysX call: `mov edx,[NxScene.vtable+0x1D0]` at `0x0082EDAD`, called at `0x0082EDD5` with eight pushed arguments in the order of PhysX 2.8's `NxScene::overlapCapsuleShapes(const NxCapsule& worldCapsule, NxShapesType shapeType, NxU32 nbShapes, NxShape** shapes, NxUserEntityReport<NxShape*>* callback, NxU32 activeGroups, const NxGroupsMask* groupsMask, bool accurateCollision)` [C for the eight arguments; I for the SDK name]. Evidence the slot is the capsule's and not the OBB's: the object passed is 28 bytes (7 floats, radius last) [C 0x0082ED08-0x0082ED5D], and `OverlapBox` calls slot 0x1CC [C 0x0082F43D], the slot before it, which is the SDK's order (overlapOBBShapes, then overlapCapsuleShapes). Confidence: high. The NxScene's code is PhysX's (not followed into it).
- Arguments of that call [C]: `shapeType = 3` (NX_ALL_SHAPES: static and dynamic, `push 3` at 0x0082EDCB); `nbShapes` = the capacity of the report's vector (`[esp+0x14]`, from `0x0082ED87-0x0082EDA4`); `shapes = 0`; `callback` = a `PhysicsEntityReport<NxShape*>` (vtable 0x00B2711C, ctor `0x00827690`) that collects the shapes into a vector; `activeGroups = -1` (`push -1` at 0x0082EDBA, so **no shape group index is excluded**); `groupsMask` = a local NxGroupsMask `{bits0 = 0x2011134, 0, 0, 0}` [C 0x0082ED0C, 0x0082ED17-0x0082ED2A zero the rest]; `accurateCollision = 1` (`push 1` at 0x0082EDB3).
- Before the call `OverlapCapsule` does what the ray does [C 0x0082ED69-0x0082ED82]: `0x0082D900(1, &m, &m)` (`PushFilter`: `setFilterBool(true)`, both filter constants = the mask) and `0x0082A150(0,0,0)` (`setFilterOps(AND, AND, AND)`). So a shape is a hit when `(shape.bits0 & 0x2011134) != 0` (same reduction as COLLISION.md:19) [I for PhysX's equation, as there].

### 2. The arguments the creature function pushes (0x006B171B-0x006B174A)

Order the callee reads them: `OverlapCapsule(this = scene, p0*, p1*, radius, mask, 0, outVector*)`, `ret 0x18` [C 0x0082F0C9]. The creature function passes:

| Callee arg | Value | Where |
|---|---|---|
| p0 (`[ebp+8]`) | pointer to the capsule's **bottom** point (x, y, centreZ - h/2) | `lea ecx,[esp+0x44]` 0x006B1737, z by `fsubr` 0x006B1733 |
| p1 (`[ebp+0xC]`) | pointer to the **top** point (x, y, centreZ + h/2) | `lea edx` 0x006B172F, z `fadd st(1)` 0x006B171C |
| radius (`[ebp+0x10]`) | float `r`, written into the pushed slot by `fstp [esp]` | 0x006B173F-0x006B1743 |
| mask (`[ebp+0x14]`) | `0x2011134` | `push 0x2011134` 0x006B1725 |
| `[ebp+0x18]` | 0 | `push 0` 0x006B1723 (its use not found; the callee copies it to a local at 0x0082EC6A) |
| exclude (`[ebp+0x1C]`) | a `std::vector` of ids holding the creature's own (an exclusion list: below) | `push ecx` 0x006B171B; destroyed after the call with `0x0046C760` |

`OverlapCapsule` copies p0/p1 to the 28-byte **NxCapsule {p0 = bottom (3 floats), p1 = top (3 floats), radius}** with `0x00837A40` (a plain 3-float copy) and passes it unchanged to the PhysX call [C 0x0082ECA4-0x0082ED5D]. It does not shrink or grow the capsule before the query.

### 3. How the shape is built

- `r` = `controller->vtable+0x64` = `PhysicsControllerCapsule::GetRadius` (ID 22684, `0x00837E50`); `h` = `vtable+0x6C` = `GetHeight` (ID 22686, `0x00837F00`) [C 0x006B166D-0x006B1683; slots read from the vtable 0x00B2766C]. The controller is `creature+0x770` (PATHFINDING.md:489).
- Centre: with the argument given, `(x, y, z + [this+0x784])` of the 16-byte vector it points to (`movaps xmm0,[eax]`, `fld [edi+0x784]`, `fadd [esp+0x2c]` [C 0x006B16D4-0x006B16E6]). So the argument is the **feet** position and `+0x784` the centre height above the feet. With no argument, the centre is the controller's own position from its vtable `+0x20` (`0x00838380`, the PhysX position, which is already the centre), with **no** `+0x784` added [C 0x006B16EC-0x006B16FD]. The snap calls it with an argument (`0x006B17E4`, PATHFINDING.md:975).
- `0x00AE70F8` is the double **0.5** [C: read as 0.5 from the file]; `h * 0.5` (`fmul qword [0xAE70F8]` at 0x006B170B) is added to and subtracted from the centre z to make p1 and p0.
- So the shape is a **vertical capsule**: a segment from (x, y, c - h/2) to (x, y, c + h/2) with radius r. With PATHFINDING.md:530-531 (creature+0x780 is the cylinder height `ch = max(h_body - 2cr - 0.2, cr + 0.1)`, centre `co = cr + ch/2 + 0.1` above the feet), and `GetHeight` being PhysX's capsule controller height (the cylinder between the sphere centres), the capsule's lowest point is 0.1 m above the feet and its top is `ch + 2cr + 0.1` above them [I: that GetHeight returns the cylinder height, as the controller's setter is given `ch`; not re-read]. So a walkable floor (bits0 includes 0x20) does not touch it at the feet's height.
- It is the same capsule as the creature's controller shape (same radius, height and centre), moved to the position tested.

### 4. The return value

- `OverlapCapsule` returns `count > 0` (`setg bl`, `mov al,bl` [C 0x0082F06A-0x0082F0B2]). `count` increments once per shape the report collected whose type is not 6 (NX_SHAPE_MESH), and for a mesh shape only if a further per-shape call (shape vtable +0xAC with args `(ptr, 3, ptr)`, unidentified) answers true [C 0x0082EF16-0x0082EF3C]. So the mesh shapes (the level's triangle meshes) get a second, exact test and boxes, capsules and spheres count on the PhysX answer [I for the meaning of the second call].
- **true = the capsule overlaps something in the mask**; the creature function returns that `al` unchanged [C 0x006B1753-0x006B175A]. This is PATHFINDING.md:976's "true = overlap" reading, now confirmed by the callee's `count > 0`.
- **The last argument is an exclusion list holding the mover's own id** (corrected 2026-10-22, read 2026-10-22 by lib-agent-2; this said it was an output list and that nothing was excluded):
  - **The list** [C]: before the call, `0x006B1600` takes its own id (`0x0053CC40` on `this+4`, 0x006B16AF..0x006B16BB) and push_backs it (`0x006F1340`, a `std::vector<uint32>` push_back) into the vector at `[esp+0x44]` (0x006B16C8). That is the vector passed as OverlapCapsule's last argument (0x006B171B).
  - **The report** [C]: `OverlapCapsule` (0x0082EC20) hands that pointer to the report's constructor `0x00827690` (0x0082EC94..0x0082EC9F), which keeps it at report `+0x14`. The report's own result vector is at `+4` (0x0040D610).
  - **The filter** [C]: for each shape, the report's callback `0x008276B0` takes a key through its slot 2 (`0x00820FC0` → shape slot +4 → `Physics::GetActorObjectId` 0x0081C970, ID 22199). It walks the list at `+0x14` (0x00827701..0x00827746). A shape whose key is in the list is skipped (0x00827755). Every other shape is appended to `+4` (0x0082777D..0x008277A3). Nothing dedupes the result.
  - **The controller's key is the creature's id** [C for the calls, I for the link]: after building the controller, the creature passes itself to the controller's slot +0x50 (0x006B2172, its user object). `GetActorObjectId` returns `[[x+4]+4]`'s id (0x0081C98E) for an actor of kind 4, and `[[x+4]]` otherwise.
  - **So the mover never overlaps itself**, wherever its controller still is [C for the exclusion, I that its controller's actor reports kind 4 with the creature as its object]. Other creatures' controllers (0x10) still count.
- What the snap does with it (PATHFINDING.md:885, :975) [C there]: false jumps straight to the final `capsule.SetPosition(p.x, p.y, p.z + 0x784)` at `0x006B1979`; true runs the safe-spot search first (`NearestFree` 0x0073E7F0 for a moving creature, else `GetSafeLocation` 0x0073E370, radius 10.0) and re-positions with `SetPosition(out, 0)` if it finds one. With the flag argument 1 the test is skipped and the search always runs (PATHFINDING.md:939).

### 5. The mask 0x2011134

It is a `NxGroupsMask.bits0` test against each shape's own `bits0` (the filter above, `activeGroups = -1`), **not** a set of shape group indices: group indices (statics 5, creatures 4, COLLISION.md:22, :41, :86) play no part, exactly as in the ray [C]. The bits, in COLLISION.md's table (:62-76):

| Bit | Flag | Shapes that carry it |
|---|---|---|
| 0x4 | Placeables | placeables' `.phy` shapes |
| 0x10 | (creatures' own mask, COLLISION.md:86; the `.phy` flag 0x2 "Creatures" is a different bit) | creature controllers' shapes (`groupsMask {0x10,0,0,0}`, 0x006B2160-0x006B21A0) |
| 0x20 | StaticGeometry | the level's static shapes (e.g. 0x81002020) |
| 0x100 | Water | water volumes |
| 0x1000 | trees (inferred "tree", COLLISION.md:84) | SpeedTree collision capsules, 0x01001020 (`0x0053167B`) |
| 0x10000 | TerrainWall | terrain walls (e.g. 0x80010000, 0x81012020) |
| 0x02000000 | Walkable | walkable statics (0x82002020) |

(0x2011134 = 0x2000000 + 0x10000 + 0x1000 + 0x100 + 0x20 + 0x10 + 0x4. Not in it: 0x1 Items, 0x2 Creatures-in-.phy, 0x8 Triggers, 0x01000000 Nonwalkable on its own, 0x80000000, 0x2000.) A level static with bits0 0x81002020 (Nonwalkable) still matches through 0x20.

It is the **same mask the creature's movement uses**: `0x006B6880` pushes 0x2011134 for the controller's move at 0x006B6A4B and 0x006B6A84 (PATHFINDING.md:972), so this test asks "would the controller's capsule be in something its own movement collides with". The ray's mask 0x24 (COLLISION.md:19) is the subset {placeables, static}.

### Not read / caveats

- The creature's own controller does not count: its id is in the exclusion list (above). Other creatures' controllers (0x10) standing there do make it true [C for the mask; not seen in play].
- The per-mesh call (shape vtable +0xAC) and what the report's key is were not identified.
- The PhysX side (`overlapCapsuleShapes` in the NxScene's implementation, and whether `accurateCollision` changes capsule-vs-box results) was not followed.

## 6. The capsule sweep: `Physics::PhysicsScene::SweepCapsule` 0x0082F600 (ID 22528; read 2026-10-22 by lib-agent with a subagent, DAOLib #238, for DAOParagonEngine #151)

Read in the GOG exe. [C] = read in the disassembly, [I] = inferred. Same wrapper object as section 5 (`Physics::PhysicsScene`, vtable 0x00B272D4, NxScene at `[this+0x24]` [C 0x0082F7E5]). Its slot 15 (+0x3C) is `SweepCapsule`, slot 16 (+0x40) is `SweepBox` 0x0082FA20 (ID 22529), next to OverlapCapsule (slot 13, +0x34) [C: the vtable words].

### 1. Signature

`bool __thiscall PhysicsScene::SweepCapsule(Vec& out, const float4* p0, const float4* p1, float radius, uint32 mask, const std::vector<uint32>* exclude)`, `ret 0x18` [C 0x0082FA1C]; six stack args, `ecx` = the wrapper.

| Arg | Read as | Evidence |
|---|---|---|
| `[ebp+8]` out | pointer to a `std::vector` of 0x40-byte hit records, filled by push_back (`0x00539F50`) | 0x0082F632, 0x0082F99E; the return value counts it as `(end-begin)>>6` |
| `[ebp+0xC]` p0 | start point, 16-byte aligned float4 (`movaps`), the capsule's place before the move | 0x0082F635, 0x0082F682 |
| `[ebp+0x10]` p1 | end point (not a vector): the motion is `p1 - p0` (`subps`) | 0x0082F63A, 0x0082F7DF-0x0082F7F2 |
| `[ebp+0x14]` radius | float | `fld`/`fstp` into the capsule's last float, 0x0082F76A-0x0082F76D |
| `[ebp+0x18]` mask | 32 bits, becomes `NxGroupsMask.bits0` (bits1-3 zeroed) and is copied into every hit record at +0x20 | 0x0082F632 area: 0x0082F6EE-0x0082F705, 0x0082F71E-0x0082F737, 0x0082F94F |
| `[ebp+0x1C]` exclude | the exclusion vector kept by the report (`0x008278C0` stores it at report +0x14), as in OverlapCapsule (section 5); 0 = nothing excluded | 0x0082F670 |

Returns `true` if the out vector holds at least one record (`seta bl` after the loop, 0x0082F9D4-0x0082F9DB, `mov al,bl`), else false. The scene lock, report and filter are the same as OverlapCapsule's: `0x004C4D00(this+4,0)` / `0x004C4D50(-1)` / `0x004CB2B0(-1)` before, `0x004CB300` after [C]; `0x00819550` (an engine singleton) vtable +8 `(1)` before and +0xC after [C, meaning unknown].

### 2. The PhysX call

At 0x0082F83A: `NxScene vtable +0x1E0` (slot 120; the overlap capsule is +0x1D0) [C]. Nine pushed arguments in the order of PhysX 2.8's `NxScene::linearCapsuleSweep(const NxCapsule& worldCapsule, const NxVec3& motion, NxU32 flags, void* userData, NxU32 nbShapeDescriptors, NxSweepQueryHit* shapes, NxUserEntityReport<NxSweepQueryHit>* callback, NxU32 activeGroups, const NxGroupsMask* groupsMask)` [C for the nine; I for the SDK name]. The values [C 0x0082F7E8-0x0082F83A]:

- `worldCapsule`: a 28-byte local at the stack, built below.
- `motion`: `p1 - p0` copied (`0x00837A40`, a plain 3-float copy) into a temporary; the w lane is dropped.
- `flags = 0xB` (`push 0xb`, 0x0082F80C) = NX_SF_STATICS (1) | NX_SF_DYNAMICS (2) | NX_SF_ALL_HITS (8) [I for the SDK's bit values]. Not async.
- `userData = 0`, `shapes = 0`.
- `nbShapeDescriptors` = the report's result vector's size in 0x30-byte elements, computed at 0x0082F7B6-0x0082F7D9 after the report was reset by `0x008277D0(0)` (0x0082F774); in practice 0 [I: not run; the result goes by the callback].
- `callback` = a `PhysicsEntityReport<NxSweepQueryHit>` (vtable 0x00B27144, ctor `0x008278C0`, ID in the address library's table), which holds the exclude list at +0x14 and collects the hits as 0x30-byte `NxSweepQueryHit`s in its vector.
- `activeGroups = -1` (`push -1`): no group index excluded.
- `groupsMask` = a local `{bits0 = mask, 0, 0, 0}` (0x0082F705, 0x0082F71E-0x0082F737).

The filter before it is the ray's and OverlapCapsule's (section 5): `0x0082D900(1, &m, &m)` (PushFilter: `setFilterBool(true)`, both constants = the mask) and `0x0082A150(0,0,0)` (setFilterOps AND, AND, AND) [C 0x0082F79C-0x0082F7B1]. So a shape is hit when `(shape.bits0 & mask) != 0` [I for PhysX's equation]. The mask is the caller's; group indices play no part.

### 3. The swept shape

Built at 0x0082F680-0x0082F76D [C], on the stack at the NxCapsule {p0, p1, radius}:

- capsule `p0` = the start point (x, y, z);
- capsule `p1` = start + (0, 0, **0.1**) (`0x00AE7BD8` read as the float 0.1 from the file; added with `addps` at 0x0082F6AE);
- radius = arg `[ebp+0x14]`.

So the "sphere" is a capsule 0.1 m tall in z, its lower sphere's centre at the start, not a point-sized sphere; it is moved along `p1 - p0` as a whole. (The camera's 0.25 m sphere is thus a capsule with segment 0.1 m.)

### 4. What it reports

The result is not a fraction or a distance. The PhysX hit's `t` ([hit+0]) is **not** read [C: the loop at 0x0082F883-0x0082F9AD touches only [hit+4], [hit+0x18], [hit+0x24] of each 0x30-byte hit]. For each hit the wrapper builds a 0x40-byte record (default vtable 0x00AF7820, the same default shape as the ray's record at 0x00648DEB) and pushes it:

| Record offset | Content | Where |
|---|---|---|
| +0x00 | vtable 0x00AF7820 | 0x0082F89A |
| +0x04 | flags: `|=1` point, `|=2` normal, `|=4`, `|=0x10`, and 8 if the hit shape's object (`0x0081C4C0`, then its vtable +0x48) exists | 0x0082F90F-0x0082F978 |
| +0x08..+0x10 | **hit point** = `NxSweepQueryHit.point` ([hit+0x18]) | 0x0082F913-0x0082F943 |
| +0x14..+0x1C | **hit normal** = `NxSweepQueryHit.normal` ([hit+0x24]) | 0x0082F93F-0x0082F967 |
| +0x20 | the **query's mask** (arg +0x18), not the shape's own bits | 0x0082F94F-0x0082F96B |
| +0x24 | the object's value from vtable +0x48 (0 if none) | 0x0082F90B |
| +0x28 | `Physics::GetActorObjectId(shape)` `0x0081C970` | 0x0082F973-0x0082F97C |
| +0x34, +0x38 | `0x0081C560(shape)`, `0x0081C4F0(shape)`, default -1 (meaning not read) | 0x0082F984-0x0082F997 |

[hit+4] is the hit `NxShape*` and its vtable slot +4 gives the owner passed on; hits whose shape has no owner (`[[hit+4]+4] == 0`) are skipped [C 0x0082F883-0x0082F891, 0x0082F9AA]. The records come in the order the report collected them; the wrapper **does not sort**, and does not carry t or the swept-capsule position. The caller works out the distance from the hit point (below). Excluded ids are dropped by the report's callback as in section 5 [I: the same class of report; its sweep callback was not read].

**"The hit point plus the normal times r" is not what SweepCapsule returns** [C]: it returns point and normal separately, and the point is PhysX's contact point on the hit shape's surface. The camera's wrapper `0x006490B0` (below) does the sum: `out = point + normal * radius` (0x0064931F-0x00649364: `[esp+0x74]` + `[esp+0x70..0x78]*r`, stored with `movaps [edi]`) [C for the arithmetic; I that `[esp+0x74]`/`[esp+0x64]` are the copied point and normal (4d4580 copies of record +8 and +0x14)]. That sum is the sphere's centre at the touch exactly for a contact on the sphere cap with the normal pointing out of the hit surface; for a contact on the capsule's 0.1 m side it is off by up to 0.1 m in z [I].

### 5. A shape that overlaps at the start

The wrapper does nothing about it [C]: no overlap test, no `t == 0` check, no special record; whatever the NxScene returns in the callback is converted. What PhysX 2.8 does for an initial overlap (a hit with t = 0 and some point, or no hit) was not followed [not read; PhysX's own code]. The camera's pull-in starts at the pivot, by the followed creature, which its own controller's shape there does not count against (mask 0x1024 has no 0x10) (a held start is tested again every frame, section 6).

### 6. Callers

No direct call (the 32-bit E8 scan of .text finds none): the only way in is the vtable, slot 15 [C: scanned .text for `call rel32` to 0x0082F600 and the vtable words]. A call of `[reg+0x3C]` right after a scene lookup was found at only one place:

- **`0x006490B0`**, the explore camera's wrapper (ID 12710, `ret 0x18`): `bool __thiscall(float4* outPos, const float4* p0, const float4* p1, float r, uint32 mask, int mode)`. It gets the scene from `World::CurrentScene` `0x00648B30` (`app +0x10C`'s module -> area `+0x62C` -> `0x00537D60(0)`) [C 0x00648B30-0x00648B58], and calls slot +0x3C at 0x00649133 with `(vec, p0, p1, r, mask, exclude = 0)` [C 0x00649118-0x00649133]. It then takes the nearest (mode 0) or the farthest (mode 1) record by the distance from the hit point to **p0** (`0x004519D0`, `sqrt`; first record seeded from index 0, then a loop from index 1 with `fcomp`) [C 0x00649138-0x0064929F]; the empty vector, or NaN in the point or normal, returns false (0x006492F6, 0x00649310). Out: `*outPos = point + normal * r` (above), and a flag byte at `out+0x18`: `!((record+0x20) >> 24 & 1)`, i.e. bit 0x01000000 of the query mask clear [C 0x0064936C-0x0064937B; with the camera's mask 0x1024 it is always 1].
- **The camera's callers** (`dev/CAMERA.md`, "Collision: a pull-in", whose 0x006490B0 is this): `0x006C3710` calls it at `0x006C3774` and `0x006C35A0` (the release) at `0x006C36AC`; `0x006C3180` (mode 3) was not followed to a call here.
  - **radius** `0.25` (`0x00AFF3E8` read as the float 0.25 from the file), so the "0.25 m sphere" is this capsule 0.1 m tall (above); **mask `0x1024`** (placeables 0x4, static 0x20, trees 0x1000) [C 0x006C3758, 0x006C3693]; **mode 0** (nearest to p0); **exclude 0** (nothing excluded: the leader's own capsule, if it is in the mask, is not removed; the mask 0x1024 does not carry the creatures' 0x10, so it is not hit [I]).
  - **Pull-in** (`0x006C3774`): the arguments are `(out, p0 = the pivot from 0x006C2950, p1 = the orbit's place at [ctl+0x80])`, so the sweep runs from the pivot out to the orbit's place and the hit kept (mode 0) is the first surface in the way going out [C, the pushes 0x006C3756-0x006C3772; corrected 2026-10-22 by lib-agent from the camera helper's reading for DAOParagonEngine, checked: `0x006C2950` is the followed object's `+0x120` (or the behaviour's `+0x50`) plus the behaviour's offset `+0x10` (0x006C297B-0x006C2996), the pivot; and `SceneCamera::Update` (0x006C2C20) writes `[+0x80]` as that same point plus `0x004D4990`'s spherical offset (`fsincos`, 0x006C2FCA-0x006C2FDE), the orbit's place; this said the other way]. On a hit it calls `0x006C32C0(&out, r)` with the centre `point + normal * 0.25`; there `|hit - pivot|` and the pull are worked out as CAMERA.md says (`want = min(|hit - raw|, ...)`).
  - **Release** (`0x006C36AC`): `p0 = ctl+0x70` (the current place), `p1 = pivot + dir * (the new pull)` (`0x008D8B00`, then `[esi+0x80] +`, 0x006C3656-0x006C3677); the same 0.25 / 0x1024 / mode 0; a hit zeroes the speed (`[esi+0x38] = 0`, the pull is held), no hit lets the pull release [C for the arguments; I for the reading].
- Only one call into the wrapper family from the rest of the exe was found by the scan for `+0x3C` calls near a scene lookup; a call of the slot through a scene pointer kept in another object (not through `World::CurrentScene` or `0x00537D60`'s callers) would not be found by that scan.

### Not read

- The report's sweep callback (vtable 0x00B27144 slot 2, `0x00820FE0`) and how it applies the exclude list for `NxSweepQueryHit`.
- PhysX's `linearCapsuleSweep` itself: the order of hits, `t` units, the initial-overlap case, and the meaning of NX_SF_ALL_HITS.
- `0x00819550`'s vtable +8 / +0xC calls around the sweep, and what `0x0081C560` / `0x0081C4F0` / the object's vtable +0x48 return.
