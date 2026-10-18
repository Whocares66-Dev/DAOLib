# Collision: the eye-to-eye ray, the shapes it meets, placeables' masks, and rooms


Read 2026-10-18 by lib-agent-2 (with a subagent) in the GOG exe (`tools\daoexe.py`, capstone scans) and vanilla's files (ghilana, `tools\gff4.py`), for DAOParagonEngine. Nothing was run in the game. **Checked** means seen in the code or the files. **Inferred** means a reading of purpose. Level used: the Circle tower basement `bhm700ar_tower_basement.are`, whose `Layout` is `lak302d` (12 rooms, `packages/core/env/lak302d.arl` and `.opf`), and `oth000d`.


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
- No code reading `.opf` was found (no string), so it is inferred to be the toolset's export. **For the port, reading the `.opf` XML is equivalent and easier.**
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
  - The visibility list is not used by that walk.
- **Example, lak302d_1101.rml:** visibility [1103, 1325], connectivity [1103]. `lak302d_1103`: visibility and connectivity both [1101, 1325, 1113].
- **The grid's per-cell byte (field 3114) is DA's `AreaGridSoundData`.** xoreos calls it `AreaGridRoomID`, and so does core's Grid.h.
  - In DA's own label set, 3110–3120 are `AreaGridArea`, `AreaGridSoundData` (3114), `AreaGridAbstractionSNumReg`, `AreaGridAbstractionSaddr`, `AreaGridLightSubsetData8` (3117), `AreaGridLightSubsetData16` (3118), `LightCanBeOccluded` and `AreaGridCellPadding` (3120). Grid.h already takes 3120's DA name.
  - The loader `0x00A48940` → `0x00A49470` keeps 3114 at grid+0x44. `0x00A49290` reads a cell's byte, and `0x00A46870` returns it with 3117's coarse value.
  - The byte goes to **creature+0xB6C** (`0x006AA5EB`, `0x006B608F`). The coarse value goes to creature+0x798 (`0x006B60E7`).
- **In lak302d the byte is no room index:**
  - −1 on 36,288 cells; 5 on 21,021 (14,360 walkable); 15 on 512; 1 on 260; 0 on 131.
  - Every creature, door and trigger position in the `.are` that was tried reads 5, whatever its room (1101, 1103, 1325, 1291).
  - 15 is past the 12-room list.
  - Its values match the actor records' 4th field (0–15), inferred to be the floor's sound material.
- **For the port:** take a creature's room by casting down from 2 m above its feet onto the static shapes (bits0 & 0x20) and reading the hit actor's room id from the `.opf`'s actor record. Test reachability by walking `EnvRoomConnectivityList` through passable doors keyed by the `.are`'s RoomID pairs.
