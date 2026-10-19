# The walk grid's tests: paths, a blocked start or goal, spawning; the ground's height

What the engine does with the walk grid's cell kinds at each test (the end test, the straight walk, the coarse and fine searches), what `RequestPath` does with a start or goal on a blocked cell, and whether a creature put on a blocked spot is moved. Companion to `dev/PARAGON_GRID.md` (the grid's file and what a cell holds) and `dev/PLACEABLES.md` (a door's patch of it).

Both parts below are lib-agent-2's reading, 2026-10-18, for paragon-agent-2, in the GOG exe with its addresses. Their headings are shifted one level under the two titles. Not seen in play. Checked in the same pass, 2026-10-18: `RequestPath`'s radii, 2.0 for the goal and 5.0 for the start (0xAE1998 and 0xAE7308), and the order of its calls.

## Pathfinder ends: blocked start, blocked goal, walkable predicates (GOG exe, read by lib-agent-2, 2026-10-18)

Addresses GOG. "checked" = read in disassembly or data; "inferred" = reasoned from it. Note: 0x0073D6E0 is only a small "has a path/search in progress" test; the real RequestPath is CCObject::RequestPath 0x0073EA80.

### Short answers

1. **A blocked start is neither snapped by the pathfinder nor refused outright.** RequestPath (0x0073EA80) tests `MovementController::IsFree(start)`. If not free it first tries a randomised fan search within 5.0 m toward the goal (0x0073DF70); if that fails it takes `CCArea::NearestFree` (0x0073E7F0) with reach 5.0 and, if that finds a place, moves the creature there (SetPosition 0x00695540). If neither finds anything the start stays where it is and the search runs from the blocked cell (checked). A goal that is not free gets `NearestFree` with reach 2.0 back toward the start (the result is ignored: if none is found the goal stays) (checked).
2. **From a blocked start cell the search then fails**: the straight-walk test fails on a blocked centre cell, and the coarse level gives "no region" for a cell of kind >= 9 (code 8, "no mesh"; inferred for the multi-region block case, see below). But RequestPath has a last fallback that rescues the wolves' case: if the search answered anything but {1,2,3,4,6,7} and |start z - goal z| < 0.5, it asks the physics walk test (0x00685920, the mover's radius) for a straight line; if clear the straight line is the path, answer 1; otherwise answer 5 (checked).
3. **The walkable predicate is not one comparison.** IsFree (the end test): a cell blocks if kind >= 8 or kind == 7 (so kinds 0-6 free; stamps 5/6 never block with RequestPath's flags), and even then only if its centre is within max(r, one cell) of the point. Straight walk: kind >= 7 blocks (and stamps of the mover's own class). Coarse level: kinds 0-8 passable, 9+ not. Fine search: step from a cell of kind < 9 to a cell of kind < 8, kinds 5/6/7 only cost extra.
4. **The Wilds cell**: (927.14, 977.53) is in ost100d.arl (the Wilds; the cell's stored height -29/16 = -1.8125 m matches the wolves' z -1.81). Its cell is row 676, column 320, **kind 9** (byte 0x09, counters 0), the edge of a band of 9s one row above a row of 1s. Vanilla's own grid calls it blocked, and `IsFree` there answers not free (cell centre (927.33, 977.44) is 0.21 m from the point).

### 1. RequestPath's order (CCObject::RequestPath 0x0073EA80, checked)

Args (this = area, creature, goal*, start* or null); returns the answer code. Pseudo-code:

```
ctl = creature.Movement()                 // 0x006A2570; null -> return 8
if ctl.found (+0x9D) == 1: return 1
world = app+0x118; world.+0xC4 = (creature.+0xC90 != 0); world.+0xC8 = creature size (vslot 0x9C)
if ctl has a path in progress (0x0073D6E0: path+0 set, or path vector +0x18..+0x1C non-empty):
    start,goal,dir = ctl.+0x60, +0x30, +0x40            // ends kept; new ends only when idle
else:
    start = startArg ? *startArg : creature.+0x120
    goal  = *goalArg
    dir   = normalize(goal - start)                     // 0x00451A30
    n = area number (area+4 attribute, 0x0053CC40)
    // GOAL
    if !ctl.IsFree(goal, n, 0, 0):                      // 0x00A62A90
        area.NearestFree(goal, -dir, reach 2.0 (0x00AE1998), ctl, 0, &goal)   // result ignored
    // START
    if !ctl.IsFree(start, n, 0, 0):
        if area.FanSearch(start, dir, reach 5.0 (0x00AE7308), ctl, &start):   // 0x0073DF70, random
            ctl.+8 = 1
        else if area.NearestFree(start, dir, 5.0, ctl, 0, &start):
            creature.SetPosition(&start, 0)                                   // 0x00695540
    ctl.SetStart(n, start, creature.+0x130); ctl.SetGoal(n, goal, dir)        // 0x00A62990/0x00A629C0 only store
LiftObstacle(ctl)                                        // 0x00A627D0
LiftObstacle(Movement(object whose id is at ctl.+0x94))  // the walk-to target, if any
if ctl.Direct(0,0):                                      // 0x00A626F0 -> world 0x00A46710 -> 0x00A4BB80
    finish as found (answer 1)
else:
    r = ctl.Step(timeslice)                              // 0x00A62640 -> 0x00A473F0 -> 0x00A467A0 -> env request 0x00A4E1B0
    if r in {1,2,3,4,6,7}: pass through
    else if |start.z - goal.z| < 0.5 (0x00B1B7B4):
        if area.WalkLine(start, goal, radius=ctl.params+4, ...) == 1:   // 0x00685920, physics (CWalkHitInfo)
            straight path, answer 1
        else answer 5
creature.PlaceObstacle() and PlaceObstacle(target)       // 0x006A3BC0, put the stamps back, whatever the answer
```

Notes:
- IsFree radius: `ctl.params[+4] * 0.5` (0x00A62A90). The world's version is 0x00A46FC0 (below).
- The goal is settled only on first call (idle). The start is snapped only on first call too.
- `NearestFree` (0x0073E7F0, args at, dir, reach, ctl, 0, out; returns 1 and writes `out` only on success): dir flattened (z=0), unit; step = 0.5 * the grid's cell size (0x0073DE50 asks the world, 0x00A46980). First probe: `at + dir * step`. Then marches forward (distance from step up to `reach`) and at each distance scans a lateral row of -n..n cells (n from distance/cell), the first point that passes `IsLocationSafe` (0x0073DE70) wins - first by forward distance, then lateral order, not truly nearest. If nothing within reach it falls to `GetSafeLocation` (0x0073E370) with the same reach. Checked structurally, not every constant of the row scan.
- `IsLocationSafe` (0x0073DE70) = `ctl.IsFree(pos)` AND four corner probes through 0x0067CFA0 (ground / walk mesh at pos +-(radius) offsets), optionally a 0x0073D780 check.
- `GetSafeLocation` (0x0073E370, args at, startRadius, ctl, reachable-flag, out, snap-to-ground flag, mask, maxRadius): null ctl/out -> 0; if `at` itself is safe -> returns it (1); else square rings outward from startRadius in steps of 0.5 * cell, each ring walked along its four sides cell by cell, first safe point returned (1; with the reachable flag also a 0x0067CFA0 test from `at`); runs out at maxRadius -> returns 0 and `out` is left alone (checked, except the exact ring corner order). Party placement uses maxRadius 20.0 (0x00AE72FC) as dev/LOAD_EVENTS.md says.
- `FanSearch` (0x0073DF70): random: for distances from step to reach, angles jittered by `rand % 90 - 45` degrees around dir and swept by growing steps up to +-180 degrees, each candidate through IsLocationSafe + 0x0067CFA0; first success returned. It uses the CRT-style rand (seed 0x00C36C10), so a port cannot match it exactly; any "a safe point within 5 m toward the goal, then around" would be the same in kind. The meaning of the controller flag byte it sets (+8) was not read.
- A found `NearestFree` for the start MOVES THE CREATURE (SetPosition); a found FanSearch does not move it (only the search's start buffer changes).

### 2. The world's IsFree (0x00A46FC0), the end test (checked)

Called `world.IsFree(areaNumber, pos, r, flagA, flagB)`, from the controller with r = radius * 0.5, flagA = flagB = 0 (RequestPath and IsLocationSafe both pass 0, 0).

```
grid = world.areas[areaNumber]; if no grid / cols<=0 / rows<=0: return true      // no mesh -> free
box = [pos.x - r, pos.x + r] x [pos.y - r, pos.y + r]
(range sanity vs a global at 0x00C4C0C0: out of range -> free)
for each cell (row, col) in the index box (indices = floor((coord - base - 0.25)/cell + 0.5), clamped to >= 0 below, NOT clamped above, so off-grid cells give kind 9):
    k = CellKind(row, col)                  // 0x00A49070: byte & 0x0F, 9 off the grid
    blocked = k >= 8 || k == 7 || (flagA && k == 6) || (flagB && counter4_5(row,col) > 0)
    if !blocked: continue
    centre = CellCentre(row, col)           // 0x00A49B70: base + idx*cell + 0.25, xy only matters
    if dist_xy(centre, pos) <= r  -> return false          // 0x004EB010
    if dist_xy(centre, pos) <  cellSize -> return false
return true
```
So: a blocked cell counts only when its centre is within max(r, cell) of the point; kinds 5 and 6 (creature stamps) never block here; the runtime counters never block here (flagB = 0). The mover's size enters only as r. An off-grid cell's centre is the grid's base corner (0x00A49B70 returns early with the base), so a point far outside the grid is "free".

### 3. Straight-walk test (0x00A46760 -> 0x00A4BB80; Direct 0x00A626F0 calls 0x00A46710 with flags 0, 0) (checked)

`Check(start, goal, flagA, flagB)` on the pathfinding world's environment:
- Samples the segment every 0.5 * cell (first sample = the start itself); the sample's cell = (a49410(y), a49440(x)).
- `wide = (creature size at world+0xC8) > cellSize` (set by RequestPath from vslot 0x9C).
- Per sample: if flagB == 0 and the placeable counter (bits 4-5, 0x00A49160) of the sample cell is non-zero -> **fail**. Then for the 3x3 cells round it: blocked = kind >= 8, or kind == 7, or kind 6 when (mover class byte == 0 and flagA == 0), or kind 5 when (class byte != 0 and flagA == 0). (World+0xC4 = creature.+0xC90 != 0 is the class byte.)
  - any blocked cell and `wide` -> fail. Not wide: the sample's own (centre) cell blocked -> fail; otherwise blocked neighbours are tallied, and a ring check (around the 8 neighbours, 12 steps) fails if four consecutive neighbours are all unblocked while some are blocked (i.e. a wall-side is refused, a pocket is accepted).
  - Height step between consecutive sampled cells > 0.5 (cell heights are sixteenths of a metre; 0x00A49320 returns |dh|/16 and the test is 0.5, 0x00B59324) -> fail.
- So **a line starting in a blocked cell fails at once**: the start sample's centre cell is checked, kind >= 7 (or an enemy-class stamp) fails. It does not use bits 6-7 at all.

### 4. Coarse level and the search start (0x00A4E1B0 -> 0x00A4D8E0) (checked unless noted)

- 0x00A4E1B0 (env request): first the path's own continuation 0x00A4C4A0; on a fresh path it tries `Check(start, goal, 0, 0)` and on success writes the two-point path and returns 1. Otherwise 0x00A4D8E0 sets up and returns 7 (still searching) or an error code, then slices.
- 0x00A4D8E0: start and goal cells -> block (0x00A49C70, blocks of `sectorSize` cells) -> the cell's region in the block (0x00A4BA20). **A region of -1 for the start or the goal returns 8** (0x00A4DD6F, "no mesh"). 0x00A4BA20: the block's region count (0x00A49CC0): count <= 0 -> -1; count == 1 -> region 0 with NO check of the cell's kind; otherwise the flood 0x00A4B190, which starts with `kind(cell) < 9` (0x00A49C50) and returns -1 for kind >= 9 (inferred: that 0x00A49CC0's byte is the region count; the -1 path of the flood is read). So a blocked kind-9 start or goal in a multi-region block makes the search answer 8; in a single-region block it does not.
- The coarse level treats kinds 0..8 as passable: adjacent cells need both kinds < 9 and a height step <= 0.5 (0x00A49BF0, 0x00B59314). So kind 7 and 8 (door/placeable states) are routed through, which is how a shut door ends as "answer 6 + blocker" rather than "no path" (inferred from the door's behaviour in play, dev/PLACEABLES.md).
- Fine level (MapSearchEnvironment vtable 0x00B592E0: successors 0x00A461A0, cost 0x00A45AA0): the step test (0x00A455C0) needs the from-cell kind < 9, the to-cell kind < 8, and height step <= 0.5 (0x00B592B0). Eight neighbours; a diagonal is dropped when either orthogonal cell is kind 7. A wide mover's env (+4 flag; where it is set not read) drops cells with the creature-ring counter (bits 6-7, 0x00A49240) non-zero.
- Cost: base ~ dx + 1.5*dy form; if env+8 is 0 and the cell changed: +7.0 for a stamp of the mover's own class (kind 5 for class byte != 0, kind 6 otherwise), +3.0 when bits 6-7 are set (0x00A49240 != 0), +8.0 for kind 7, +1.0 for each of the four diagonal neighbours of kind >= 8 and +1.0 more if any diagonal-ish neighbour is kind 7, +2.0 when bits 4-5 are non-zero (constants 0x00B21118, 0x00ADE218, 0x00B1B7C8, 0x00C1CDE8, 0x00AE7108, read).

### 5. Cell kinds and counters, what the exe does with them (checked)

| field | meaning |
|---|---|
| kind 0-4 | walkable everywhere |
| kind 5 | a creature stamp, written for a creature with `+0xC90 != 0` (PlaceObstacle passes class = that); blocks/penalises movers of that same class only |
| kind 6 | a creature stamp, class byte 0; same, for movers with class byte 0 |
| kind 7 | placeable/door state: not an end, blocks a straight walk and diagonals; the fine search steps onto it at +8 cost; the coarse level passes it |
| kind 8 | like 7 but the fine search cannot step onto it; IsFree blocks (>= 8) |
| kind 9+ | wall band / outside / off-grid: not passable at any level |
| bits 4-5 | placeable counter, written by 0x00A46E60 (a walk patch with +1/-1 via 0x00A490B0/0x00A49110, max 3). Read by 0x00A49160. Direct fails on a non-zero sample cell; the fine search costs +2.0; IsFree ignores unless flagB |
| bits 6-7 | creature crowding ring, written by 0x00A469C0 via 0x00A491A0/0x00A491F0 (max 3). Read by 0x00A49240 (returns 0 or 1). Fine search: +3.0, and a wide mover will not step there. Not read by Direct or IsFree |

How a creature stamps (0x00A469C0 via CCCreature::PlaceObstacle 0x006A3BC0 -> controller 0x00A629F0): args (remove, pos, radius = params[+4], classFlag = creature.+0xC90 != 0, soft = creature.+0xB20 in {1,2}, record of cells). Only if the creature's vslot 0x1D0 answers 0 and its byte +0x2C5 is set. Cells whose centre is within the radius (the box is radius + one cell) become kind 6 (classFlag 0) or 5 (classFlag set), skipping cells that are kind >= 7 or already 6; a "soft" creature (state 1 or 2) does not change kinds, only counters. Cells in the band between the radius and radius + one cell (only for classFlag 0, and where the cell's squared distance is >= r^2 + cell^2/2) get the bits 6-7 counter +1 (not past 3). Every cell touched is recorded in a bit record. LiftObstacle (0x00A627D0) undoes exactly that record: stamped cells that are still 5 or 6 go back to **1**, ring cells' counters go down; it only does it if the controller's "placed" flag (+0x40 of the parameters) is set. So "the mover's own obstacle lifted" = unstamp its recorded cells, and the walk-to target's likewise, for the search; both are stamped again at the end of RequestPath whatever the answer.

The mover's size: IsFree's r (radius * 0.5); the straight-walk test's `wide` flag (size > cell size) turning any blocked neighbour into failure; the fine search's wide-mover exclusion of crowd cells. Clearance (AreaGridClearance 2.1) was not seen in any of these.

### 6. The Wilds cell (checked)

`ost100d.arl` (core), 1027 x 1027 cells at 0.5 m, base (767.0797, 639.1931, 44.2039), AreaGridData kinds: 10: 852,768, 1: 137,375, 9: 64,586, counters all 0 in the file. Index: col = floor((927.14 - 767.0797)/0.5) = 320, row = floor((977.53 - 639.1931)/0.5) = 676 (the +0.25 offset in the exe's index formula is the cell-centre offset, grid +0x38 = 0.25 from the constructor 0x00A48F4B / 0x00AFF3E8; nothing else writes it). **Cell (row 676, col 320): byte 0x09, kind 9, height -29 (sixteenths) = -1.8125 m** - the wolves' z -1.81. Neighbourhood (kinds, rows 673..679, cols 314..326):

```
673  1 1 1 1 1 9 9 9 9 10 10 10 10
674  1 1 1 1 1 9 9 9 9  9 10 10 10
675  1 1 1 1 1 9 9 9 9  9  9  9  9
676  1 1 1 1 1 1 [9] 9 9 9  1  1  1     <- col 320 is the bracketed one
677  1 1 1 1 1 1 1 1 1 1  1  1  1
```
A blocked pocket edge: two cells (columns 319 and 320... col 319 is 1; 320..323 are 9) and the clear floor one row south (row 677 all 1). Under the exe's IsFree the point is not free (centre (927.33, 977.44) is 0.21 m away, under a cell); one row south it would be. The cell holds the wolves' z, so they stood on a cell the level's own grid marks as a wall band (the clearance band), not a cell a stamp made. Straight-walk from it fails (centre kind 9). The nearest free place by the exe's own search is NearestFree/FanSearch (above), not a fixed 2 m snap: reach 5.0 m for a start, 2.0 m back toward the start for a goal.

### What a port should copy (summary)

1. Keep the grid's cell kinds as the file's 4-bit kinds, with the counters; implement `IsFree` as in section 2 (cells blocked at >= 8 / == 7, but only by centre distance <= max(r, cell)), not "the cell under the point is blocked".
2. Do not refuse a blocked end. Start: IsFree -> if not, a safe point within 5 m (the exe: a random fan toward the goal, else the first safe point of a forward/lateral scan, else rings by half a cell), moving the creature when the scan found one. Goal: if not free, a safe point within 2 m back toward the start; none -> keep the goal.
3. Straight walk first (it fails on a blocked centre cell); then the coarse/fine search with the "no region" answer 8 for a kind >= 9 end in a multi-region block; then, if the ends are within 0.5 m in height, a physics straight-line test with the mover's radius as the last resort (answer 1 if clear, else 5).
4. Lift the mover's own stamp (and the walk-to target's) before all of this; put it back after.

### Not read

- The exact lateral-row scan inside NearestFree (0x0073E8E1..0x0073EA4C) beyond its structure, and GetSafeLocation's ring walk order past the first two sides.
- 0x0073DF70's remaining tail after 0x0073E2E6 (angle sweep termination), the meaning of controller byte +8, and 0x00A62F30 / 0x00A62B90 (what the found-path finish does).
- Where the environment's wide flag (env +4) is set; 0x00A49CC0's array layout (taken as region count); 0x00C4C0C0's value (a runtime sanity limit in IsFree).
- 0x00685920's body past the first calls (the physics walk test); 0x0073D780; 0x0067CFA0.
- Which creatures have `+0xC90 != 0` (the class byte) and what states `+0xB20` 1 and 2 are.
- That ost100d is the area the wolves were in (it fits the coordinates, grid extent and the height, not confirmed by area name).

## The spawn: is a creature on a blocked spot moved? (lib-agent-2, 2026-10-18, GOG exe, addresses GOG's)

### Short answers

1. **Placed creatures from the .are are NOT moved by a safe-location search while the area loads.** They are added by the area's list step 0x0067F420, which calls the creature's AddToArea (CCCreature vtable +0xF8 = 0x006B6BC0) with the creature's own stored position (+0x120). AddToArea ends by calling SetPosition (vtable +0x7C = 0x006B5EF0, args (pos, 1)). SetPosition only runs the snap (0x006B1790) when the game mode is not Loading: it reads `[0xC327EC]+0x114` -> `+0x78` and skips the snap if it equals 9 (GameModes.h: Loading = 9), setting the physics body straight to the position. Area load runs in mode Loading, so placed creatures stand exactly where the .are puts them (checked in disassembly; "the mode is Loading during the area's object adds" is inferred from LOAD_EVENTS.md's order).
2. **CreateObject (0x00669550) does not call GetSafeLocation.** It makes the object through 0x00687830 -> factory 0x006861F0, which takes only x,y of the location, replaces z by the ground height from 0x00685430 (a physics ray down from z+3.0 over 100 m; the input z if nothing is hit), then calls AddToArea(area, &pos, 0). That AddToArea ends in SetPosition as above, now in a non-Loading mode, so the snap in 0x006B1790 is reachable (checked).
3. **That snap is gated by a physics test, not by the walk grid.** 0x006B1790(arg 0) first calls 0x006B1600: a query on the area's physics world (`[area+0x62C]`, vtable slot 0x34, mask 0x2011134) at the creature's position, with a box/capsule from the body's size. Only if it answers true does it search: `GetSafeLocation(pos, 10.0, the creature's movement controller, reachable=0, out, snapToGround=1, creatureCheck=0, minRadius=0)`. If the creature is moving (controller 0xA630E0 true) it uses NearestFree (0x0073E7F0) instead. If nothing is found it keeps the position. The test's meaning (true = overlaps static physics) is inferred from the control flow (`test al; je skip`), not from a name. A cell the walk grid calls blocked but with no physics overlap is therefore NOT a trigger.
4. **GetSafeLocation's walkable test is the grid's IsFree with the creature's half personal space, plus a short ray.** The grid is consulted by cell kind only (kind >= 8 or == 7 blocks; 5 and 6 do not), so a creature standing on another's stamp is not blocked. No height-difference test in it. "Reachable" = a physics ray from the ORIGINAL position to the candidate is clear, not a path search.
5. **Stacked creatures are not pushed apart anywhere in this path.** A creature's stamp (PlaceObstacle) skips cells already kind 6 or >= 7, so stacking just re-stamps nothing; nothing separates them. I found no separation step.
6. **Wolves at (927.14, 977.53, -1.81): not a placed spot in pre200ar_korcari_wilds.are.** None of its 98 creatures is within 15 m of it (placed wolves are at e.g. (912.5,966.8,-0.89), (945.0,978.9,-0.19), (904.9,979.1,-0.46), (924.5,955.7,-1.59), (905.3,958.2,-1.53); the single float scan of the file finds no 927.14 or 977.53 pair). So the stack is not the placed point; it is where something led them (a path end or script jump), and the engine never separates stacked creatures.

### The party's arrival is different (for contrast)

`0x006C73C0` (arrival placement) calls GetSafeLocation at 0x006C7664: `(spot, 20.0 [0xAE72FC], controller, reachable=1, out, snap=1, creatureCheck=1, min=0)`; on failure the spot is kept; then AddToArea(area, spot, 0) is called, whose SetPosition does not snap in Loading mode. ScriptCommand::SETPOSITION with bSafePosition (0x00655634): `(pos, 5.0 [0xAE7308], controller, reachable=1, out, snap=1, creatureCheck=1, min=0)`. ScriptCommand::GETSAFELOCATION (0x0065B1F8): `(pos, 20.0, a default temporary controller made by 0xA62730, reachable=1, out, snap=1, creatureCheck=0, min=0)`; on 0 it returns the input location.

### CCArea::GetSafeLocation 0x0073E370 exactly

Signature (`__thiscall`, `ret 0x20`, this = CCArea; the CCArea+4 attribute gives the area number via 0x0053CC40): `GetSafeLocation(const Vec4 *pos, float maxRadius, MovementController *mc, bool reachable, Vec4 *out, bool snapToGround, bool creatureCheck, float minRadius)`. The third argument is the movement controller (the creature's `+0xA40`, from 0x006A2570), not the creature. Checked from callers' pushes.

```
if (!mc or !out) return 0
p = *pos
if (snapToGround) p.z = GroundHeight(p)          // 0x73D520 -> 0x685430 (physics ray, above)
if (minRadius == 0 and IsLocationSafe(p, mc, creatureCheck)) goto found(p)
if (reachable and the area's [0xC327EC]+0x10C object exists):
    r = max(mc.personalSpace /*[[mc+0x10]+4]*/, 0.15)  // double 0.15 at 0xB1B7D0, 0xADEEA8
    if (0x006493E0(...mask 0x2011124..., r+...) )   // a physics query near p; meaning not read
        *out = p; return 0                           // note: returns 0 with the unchanged spot
step = gridCellSize(area) * 0.5                      // 0x73DE50: 0xA46980 -> grid +0x30; 0.5 at 0xAE70F8; the area's fallback 0xAE7104=0.5
if (step <= 0) { *out = p; return 0 }
r = (minRadius > 0) ? minRadius : step
while (r <= maxRadius):                              // float compare; r += step at the end
    // square ring at Chebyshev distance r around pos (x,y), side coords from (pos.x-r..pos.x+r)
    for x = pos.x - r; x <= pos.x + r; x += step:    // accumulating float sum
        test (x, pos.y - r)                          // bottom
        test (x, pos.y + r)                          // top
    for y = pos.y - r + step; y <= pos.y + r - step; y += step:
        test (pos.x - r, y)                          // left
        test (pos.x + r, y)                          // right
    r += step
return 0, *out unchanged? (see below)

test(c): c.z = grid height at (c.x,c.y)             // 0x73DE20 -> 0xA473A0 -> cell height
         if IsLocationSafe(c, mc, creatureCheck) and (!reachable or Ray(pos -> c, mc)) -> found(c)
found(c): *out = c with z = GroundHeight(c) (0x685430)  // always, even when snapToGround was false; return 1
```

Order within a ring: left to right along the bottom (low y) and top at each x (bottom, then top), then the sides bottom to top (left, then right). The first success wins, so the nearest ring wins and ties go to the low-y row. The first ring is at half a cell (0.25 m for 0.5 m cells) and each ring adds half a cell; the ring's points are spaced half a cell too. Checked in disassembly for the loop shape (0x73E512-0x73E746); that the success exit z-snap runs unconditionally is read at 0x73E75B (checked).

When nothing is found: the loop exits at 0x73E74C -> `xor eax,eax; ret 0x20` with `*out` NOT written (the callers pre-copy: 0x006C7664's caller keeps its spot, GETSAFELOCATION returns the input). The two earlier early exits (step <= 0 and the physics early-out at 0x73E48A) write `*out = p` and also return 0. (checked)

#### IsLocationSafe 0x0073DE70 `(pos, mc, creatureCheck)`

1. `mc->IsFree(pos, areaNo, 0, 0)` (0x00A62A90): forwards to the grid, `GridIsFree(pos, areaNo, r = personalSpace*0.5, includeStamp6 = 0, crowdCheck = 0)` = 0x00A46FC0 (checked). Its rule: the cells overlapping the box pos +- r (cell indices of the box's corners). For each cell with kind >= 8 or == 7 (6 too if includeStamp6; or a crowd count > 0 if crowdCheck; kind 9 is also what an off-grid cell reads as): the spot is blocked if that cell's centre is within r of pos (xy, 0x004EB010), or within one cell size (`grid+0x30`) of pos. A non-finite position (NaN/inf against the +inf at 0xC4C0C0) or an empty index range answers free. The cell's height is NOT used. Kinds 0-6 are free, including a creature's stamp (5, 6). (checked, except exactly how the box indices clamp)
2. A physics ray 0x0067CFA0 from one point to another around pos (points derived from +-personal space in xy and a z span, both lifted 0.5 m: double 0xB0DAA0, mask 0x2011134 when its flag is 0), ignoring the mover's own body; it passes if nothing is hit, or the hit is the mover itself. The exact two points were not decoded (diagonal through the creature's footprint, inferred).
3. If creatureCheck: 0x0073D780 (a box test against other objects with the mover's personal space and its id from `[mc+0x10]`); not decoded further. The spawn snap and GETSAFELOCATION pass creatureCheck = 0; the party arrival and SetPosition pass 1.

#### What "reachable" is
The extra call `0x0067CFA0(this, pos, candidate, mc, 1)` (flag 1 -> mask 0x2011124): a physics ray from the original position to the candidate (z lifted 0.5). It is a line of sight through the level's static shapes, not path reachability, and not the pathfinder.

### A creature's stamp

`CCCreature::PlaceObstacle` 0x006A3BC0 -> controller 0x00A629F0 -> grid 0x00A469C0 (the cells in the circle of the creature's size, radius^2 * 0.5 as a test, cells found by corner indices). Called from SetPosition's tail (0x006B60A3, every position change), from 0x006B3070 (AddToArea's helper, 0x006B3206 / 0x006B36A4), and from several others. For each cell it skips kind >= 7 and kind 6, and where the crowd count is already 3; it records the cell's index in a bit list and writes kind 6 (kind 5 if the flag at +0xC90 is non-zero) (`sete al; add al,5`), and raises the counters (0x00A491A0) of the cells within about a metre. LiftObstacle undoes exactly the recorded cells. There is no check against other creatures and no separation: three wolves on one point stamp once, the others find the cells already 6 (checked). IsFree ignores stamps by design, so stacked creatures remain a legal spot for each other.

### For the port

- Do not snap a creature at area load. Place it at the .are's position as given. (The engine does no search for it.)
- A blocked end of a path is not a spawn problem in the engine: `RequestPath` has its own order (IsFree at the destination, else NearestFree within 2 m back toward the start; already in TODO.md). The only GetSafeLocation uses are the ones listed above (script SETPOSITION safe: 5 m; GETSAFELOCATION and the party arrival: 20 m; physics-overlap snap on a runtime spawn: 10 m). None applies to an area's placed creatures.
- If a grid-blocked cell holds a placed creature, the engine just leaves it; a pathfinder that refuses paths from a blocked start differs from the engine, whose start is `SetStart` of the creature's own cell (see RequestPath in TODO.md: the mover's own obstacle is lifted, a straight walk tried, then slices). Whether the engine's search accepts a blocked start was not read here.

### Not read
- Meaning of 0x006B1600's physics method (assumed true = overlap) and of 0x006493E0 (GetSafeLocation's early physics query that returns 0 with the unchanged spot).
- The two ray points inside IsLocationSafe and the body of 0x0073D780.
- Where the game mode is Loading exactly during the object adds (I rely on `[0xC327EC]+0x114 -> +0x78 == 9` and LOAD_EVENTS.md's order), and 0x006B3070's other effects.
- Where the stacked wolves' point comes from (not in the .are's creature list; scripts of pre200tr_wolf_fight use no CreateObject/SetPosition); whether the engine's pathfinder accepts a blocked start.
- The `[ebp+0x1C]` snap flag is passed but the found-exit snaps regardless (read); whether the early ground snap at the top is the only effect of the flag.
- The GFF3 .are's other lists (placeables, stages) were not checked for the stack.

## Ground z at load: what z a placed creature or placeable gets (lib-agent-2, 2026-10-19, GOG exe, addresses GOG's)

### Short answers

1. **A creature placed in an area's .are keeps the .are's z exactly (checked in disassembly).** The Position (XPosition/YPosition/ZPosition) is read verbatim by the common object loader `0x00698ED0` (called through vtable +0x88: CCCreature `0x006BE980`, CCPlaceable `0x0074FE30`; both end in the same loader) and stored at +0x120 by `0x00695540`, which calls SetPosition on an object that has no physics body yet. The area's creature list step `0x0067F420` then calls AddToArea (vtable +0xF8, `0x006B6BC0`) with that +0x120. AddToArea ends in SetPosition `0x006B5EF0`, which in game mode 9 (Loading) skips the snap `0x006B1790` and sets the physics body to (x, y, z + [obj+0x784]). No GroundHeight and no GetSafeLocation. Confirms PATHFINDING.md:153.
2. **A creature restored from a save is the same (checked for the loader, inferred for the mode).** LOAD_EVENTS.md:103 says a saved object is restored through vtable +0x88; that is the same `0x006BE980` -> `0x00698ED0` that reads the saved Position verbatim, and the area's list step `0x0067F420` adds it the same way (the creature loader `0x00683820` puts both kinds on the +0x250 list). So the saved z is kept as is. That the mode is 9 for a save's area adds is inferred from the loading mode's state machine running them (LOAD_EVENTS.md:97).
3. **The party's arrival is the one load-time snap (checked, PATHFINDING.md:162, LOAD_EVENTS.md:125).** `0x006C73C0` -> GetSafeLocation `0x0073E370` (20.0 m, reachable, snapToGround=1). Its z part: at the top `p.z = GroundHeight(p)` (0x73D520 -> 0x685430); at the found exit `*out.z = GroundHeight(c)` always (PATHFINDING.md:193). Then AddToArea in Loading mode, no further snap. If the search fails the spot (with its unsnapped z) is kept.
4. **Placeables are never snapped at load (checked).** Their SetPosition (vtable +0x7C = `0x0074BB70`) is the base one (`0x00697AE0` + `0x00695D70`), without any ground call; their AddToArea `0x00750B10` has none either. A placeable placed from the .are stands at the .are's z.
5. **At CreateObject the factory snaps placeables too, not only creatures (checked).** The factory `0x006861F0(type, area, pos)` takes type 6 (creature, object 0xD20) or 0xA (placeable, object 0x6F0, init `0x0074BB30`); both branches reach the same tail `0x00686301`, which takes x and y of the given location, calls `0x00685430` (GroundHeight) for z, and calls AddToArea(area, &pos, 0). Other types are not made here. So a script `CreateObject` of a placeable gets z = ground at (x, y) (from z+3.0 down 100 m; the input z if the ray hits nothing).
6. **Other GroundHeight callers (found by scanning E8 calls, not read):** `0x0068581C`, `0x0069551A` (CCObject lazy cache below), `0x006A5E5D`, `0x006A7786`, `0x006A77D1` (creature code), `0x00752803` (a placeable function reached from `0x006B9954`, the code after a creature's `0x006B98E0`-area routine: it copies the creature's transform to a placeable and puts it on the ground; inferred to be the death/corpse placeable), `0x0073D529`, `0x0073E31D/E348/E3BD/E766/E794/E7C2/E896/EA57` (GetSafeLocation family), `0x007A595D`, `0x007A5E62`, `0x007A9A6C`.
7. **The designers' z already sits on the ground (checked with the files).** Ostagar's camp (`pre100ar_kings_camp.are`, Layout `ost000t`, `ost000t.opf`'s 54,674 terrain-mesh triangles): of 96 placed creatures, 79 are within 5 cm of the mesh below them, and of the 83 within 25 cm, 71 are within 1 cm (7 are 1-5 cm below the surface, 2 are 5-10 cm below, 2 are 10-25 cm above). The 13 beyond 25 cm are on non-mesh shapes (box/capsule bridges and tower floors: z up to 61 m "above" the mesh underneath), which my mesh-only test cannot see. Placeables: 25 of 44 within 5 cm, 38 of 44 within 25 cm; the rest on gates, bridges, floors. Slopes are mild here (steepest creature 0.34 m of rise per metre, its z 17.5 cm above the mesh; most are below 0.16). So the port should keep the .are's z for a placed object and not re-snap it; if it must snap, do it against the physics mesh (a ray), not the walk grid's heights.

### A hint for the port about slopes while moving (new, inferred)

`CCObject` `0x006954C0` is a lazy ground cache: if byte +0x1C4 is 0 it sets the normal at +0x1B0 to (0,0,1) and +0x1C0 = z, and if the object has an area (vtable +0xA0) it calls GroundHeight(area, &pos(+0x120), 0, &normal(+0x1B0)) and stores the ray's height into +0x1C0, then sets +0x1C4 = 1. `0x00695540` (set position) clears +0x1C4 to 0, so the cache is recomputed after every position change. The creature's per-frame update (code at `0x006A3470-0x006A35BB`, argument dt) calls `0x006954C0` twice and reads +0x1C0 as the creature's height. So the engine puts a moving creature's z on the physics mesh (a ray), with the surface normal kept at +0x1B0 (probably what TerrainFollowPitch/Roll in ANIMATION.md:557/606 consume; not read). A walk grid's sixteenth-of-a-metre heights are not what the engine uses for z.

### Pseudo-code a port can copy

```
// area load, for each creature and placeable of the .are (or of the save)
obj.pos = file.Position            // verbatim, z included
obj.AddToArea(area, obj.pos)       // game mode Loading: no snap, no safe-location search
                                   // physics body z = pos.z + body offset (+0x784 for a creature)

// script CreateObject (creature or placeable)
pos.z = GroundHeight(area, pos.x, pos.y, pos.z)   // ray down from z+3.0, 100 m long; pos.z if no hit
obj.AddToArea(area, pos)           // not Loading: a creature may then be moved by 0x006B1790 if it overlaps static physics

// party arrival: GetSafeLocation(spot, 20 m, reachable, snapToGround) as PATHFINDING.md:164-198

// each frame, a moving creature: z = GroundHeight(area, pos) (cached, invalidated by any position change)
```

### Evidence (all GOG)

- Position fields: strings "XPosition" 0xB0DB9C, "YPosition" 0xB0DB90, "ZPosition" 0xB0DB84. The generic object read `0x0069AAD0` (reads X/Y/Z through `0x00A370C0`, calls `0x00695540`; reached by vtable data, scanned) stores the values untouched. `0x00698ED0` reads the vector (field id 4, default the object's own +0x120) through `0x005FF7A0` and calls `0x00695540` at `0x00698F38`; the orientation follows (field 5). CCCreature's main vtable `0x00B10484`: +0x7C `0x006B5EF0`, +0x88 `0x006BE980`, +0xA0 `0x00695CD0`, +0xF8 `0x006B6BC0`. CCPlaceable's `0x00B1C084`: +0x7C `0x0074BB70`, +0x88 `0x0074FE30`, +0xF8 `0x00750B10`.
- The loaders: placeables `0x00683380` fills the list at area+0x240, creatures `0x00683820` (new object 0xD20, constructor `0x006B9ED0`, load `0x006ABDC0`) the list at area+0x250; `0x0067F420` walks the +0x240 list first, then the +0x250 list, each: a readiness call, then `[obj vtable +0xF8](area, &obj+0x120, 0)` (disassembly at `0x0067F49B..0x0067F4AD` and `0x0067F552..0x0067F570`).
- `0x006B5EF0`: gate `mov eax,[0xC327EC]; mov edx,[eax+0x114]; cmp [edx+0x78],9; je skip` at `0x006B5F67-0x006B5F7A`; else `call 0x6B1790`. The skip path writes (x, y, z + [esi+0x784]) to the physics body (vtable +0x1C) at `0x006B5F87-0x006B5FB7`.
- Factory `0x006861F0`: `cmp eax,6` / `cmp eax,0xA` at `0x00686224-0x0068622C`; common tail `0x00686301`; GroundHeight call `0x0068633D`; AddToArea via `[vtable+0xF8]` at `0x00686370`.
- Ground cache `0x006954C0`, set-position `0x00695540` (clears `[esi+0x1C4]` at `0x006955A0`), snap `0x006B1790` reads `[esi+0x1C0]` at `0x006B17D1`.
- Data check: scratch `%TEMP%\lib-agent-2-groundload-cmp.py` (a GFF3 reader for the .are, the .opf's XML triangle meshes placed by actor and shape poses, a downward ray from z+3.0 over all mesh shapes; the highest hit below z+3.0). Files extracted by ghilana: `pre100ar_kings_camp.are` (patch003.erf), `ost000t.opf`. The Wilds (`ost100d`) has no `.opf` (COLLISION.md:5), so it could not be compared; Ostagar's camp was used.

### Not read

- That the game mode is exactly 9 during the object adds of a save's area (relied on LOAD_EVENTS.md's order).
- What `0x00685430`'s third argument (the normal out) and its fourth/"nothing hit" path do beyond what PATHFINDING.md says; what consumes the normal at +0x1B0 (TerrainFollow*).
- The creature per-frame update around `0x006A3470` beyond its use of `0x006954C0`/+0x1C0; whether the final z of a moving creature is that cache or the physics body.
- The other GroundHeight callers listed in point 6 (what they are); `0x0069AAD0`'s caller (only reached through data tables).
- The 13 creatures and 6 placeables on non-mesh shapes (boxes, capsules): not compared; the Wilds; an area with steep stairs (Ostagar's camp has only mild slopes).

## How a walking creature gets its z, and the body tilt (read 2026-10-19, GOG exe, read-only) (lib-agent-2)

"checked" = read in disassembly or data. "inferred" = from control flow or a constant's meaning, not a name.

### Short answers

1. **A moving creature's z is not the walk grid's, and not a per-step GroundHeight ray. It is a PhysX character controller.** Every creature has a capsule controller (`Physics::PhysicsControllerCapsule`, vtable 0x00B2766C, over `NxController`) at `creature+0x770`, made by `CCCreature::CreatePhysicsController` 0x006B1ED0 (checked). Each frame the creature update (0x006BB910 region, call at 0x006BBA39) calls **0x006B6880(dt, targetPos)**, which moves the capsule by `(target.xy - pos.xy, z = -9.81)` through `NxController::move` with mask 0x2011134, then **0x006B2220 reads the capsule back into the creature**: `z = capsuleCenterZ - centerOffset`. The z is set exactly (no lerp, no smoothing) (checked).
2. The -9.81 is a displacement per move, not a velocity times dt: it is "fall up to 9.81 m this frame". The capsule lands on whatever floor its sweep meets within 9.81 m below, and climbs up steps of up to **0.5 m** (the controller's stepOffset), slope limit **cos 60 degrees** (inferred, see below), skin width 0.1. So feet follow stairs and slopes exactly each frame, because the contact is the capsule's, not a sampled height. A port that sets z from a 0.5 m cell's height will always differ from this.
3. **GroundHeight (0x00685430) is not the walking z.** It is used for spawn (CreateObject), GetSafeLocation, the snap 0x006B1790, a cached ground height and normal at the creature (`creature+0x1C0` z, `+0x1B0` vec4 normal, refreshed by 0x006954C0), and the root-motion (gob clip) / script-moved path, where z is taken from that cached ground height (0x006A3487 region). It is a physics ray: origin (x, y, z+3.0), straight down, **unlimited length** (FLT_MAX), group mask 0x2000000 (Walkable); if that misses, a second ray from z+100 collects all hits and takes the hit nearest in z to the input z (inferred from the loop, checked shape). The input z is returned if nothing is hit (cited, PATHFINDING.md:154).
4. **The walk grid's cell height is not read by any of this** (checked: 0x006B6880 and 0x006B2220 never touch the grid). It only supplies z for GetSafeLocation's candidate cells (0x0073DE20 -> 0x00A473A0, cited PATHFINDING.md:171-200), and even there the result is re-snapped by GroundHeight. Whether the grid interpolates between cells: not read, and it does not matter for walking.
5. **No foot IK.** `IKDNode` (vtable 0x00B0D9B4, 2 slots: destructor + one) is not inverse kinematics: its constructors/destructors sit in area code (0x0067A5C0, 0x0067B830, 0x0067E3F0, the CCArea region) and it is a two-pointer container (fields +4/+8 or +0xC/+0x10 zeroed). Refutes the "probably the physics" guess in ANIMATION.md:606 (checked: the code, inferred: what the name stands for). Confirms "no foot IK" (no per-foot ray or bone solve found in the movement or animation code I read; not an exhaustive scan of every animation system).
6. **Body tilt exists, for quadrupeds, and it is not a controller thing.** `TerrainFollowPitch/Roll/Precision` (apr_base row +0xA8/+0xAC/+0xB0) are consumed by a per-frame creature routine **0x006A7AE0** (from the creature update at 0x006BD566) which calls **0x006A7520** to compute a target pitch and roll from the ground, clamps them to +-TerrainFollowPitch and +-TerrainFollowRoll, and slews the model toward them (checked structure; formulas inferred). A biped's pitch limit is 0, so its target is 0 (inferred from the clamp; it never tilts).

### Pseudo-code a port can copy

#### Per-frame z (0x006B6880 + 0x006B2220), checked

```
// creature has a capsule: radius cr, cylinder height ch, centre offset co  (below)
step(creature, target /* new x,y,(z unchanged) from the movement controller or root motion */, dt):
    if capsule.collisionEnabled == false:  creature.SetPosition(target)          // 0x006B68BD: no ground follow
    saved = capsule.position                                                    // GetDebugPosition 0x00838380 (centre)
    disp  = (target.x - creature.x, target.y - creature.y, -9.81)               // NOT times dt
    n     = groundNormalAt(creature)                                            // 0x006954C0 -> GroundHeight's 3rd out
    if n.z <= cos(60deg) + 1e-6:                                                // steeper than 60 deg: slide
        slide = min(slide + dt*5.0, 20.0); if slide > 0.01: disp.xy += downhill(n) * slide * dt   // inferred direction
    else:
        slide *= clamp(1 - 3*dt, 0.1, 0.9)                                      // decays
    flags = capsule.move(disp, mask=0x2011134, minDist=0.001, sharpness=1.0)    // NxController::move, slot +0x10
    if flags & UP:                                                              // head hit while stepping up
        capsule.setPosition(saved); capsule.stepOffset = 0.0
        capsule.move(disp, mask=0x2011134, 0.001, 1.0); capsule.stepOffset = 0.5
    p = capsule.position; p.z -= co                                             // 0x006B2220
    if creature.capsuleHeight != -1: p.z += (creature.capsuleHeight - capsule.height) * 0.5
    if capsule.collidingDown:  if p != creature.pos: creature.SetPosition(p)    // exact
    else:                      creature.SetPosition(p.x, p.y, creature.z)       // airborne: z kept, then snap 0x006B1790(0)
```

The mask 0x2011134 = Walkable 0x2000000 | TerrainWall 0x10000 | tree 0x1000 | Water 0x100 | StaticGeometry 0x20 | 0x10 (creatures' own bit, so creatures block each other) | Placeables 0x4 (bit names from COLLISION.md:62-84, checked). Creature capsules sit in shape group 4, groupsMask 0x10 (cited COLLISION.md:85).

#### The capsule's size (0x006B1ED0, values checked, flow inferred), all in metres

```
h = min(Height of the creature (virtual 0x1B0), 1.7)             // 0x0069EE10; default 1.7
r = movementController.personalSpace ([[creature+0x7B0]+0x60])
if bigKind(creature) /*0x006A2CE0*/: h = max(h, 1.426); r = max(r, 0.35)
cr = max(0.6 * r, 0.1)                       // capsule radius,  stored creature+0x77C
ch = max(h - 2*cr - 0.2, cr + 0.1)           // cylinder height, stored creature+0x780
co = cr + ch/2 + 0.1                         // centre above the feet, stored creature+0x784
controller desc: up axis Z (2), slopeLimit = cos(pi/3)+1e-6 (0x008E6020 not decoded; cos assumed, 0.5), skinWidth 0.1, stepOffset 0.5
```

The 0.1 is the gap under the capsule: feet z = centre - co.

#### GroundHeight 0x00685430(this = area, pos, byte* hitOut, vec4* normalOut) -> z, checked

```
ray1: origin (x, y, z+3.0), dir (0,0,-1), length FLT_MAX, groups all, mask M
      M = 0x24 (Placeables|StaticGeometry) and the hit must not carry flag 0x01000000 (Nonwalkable)   if area+0x658->+0x7C == 1 (0x0067B4D0)
      M = 0x2000000 (Walkable)                                                                         otherwise
if hit: z = hit.z; *hitOut = 1; *normalOut = hit.normal; return z
else:   ray2 from z+100 down with all hits; take the hit with the smallest |hit.z - z| whose flag 0x01000000 is clear (65505 start of the min); if none, return z
```

(The 0x0067B4D0 flag's meaning is not read; mask choice checked, who sets it not.)

#### Body tilt (0x006A7AE0 -> 0x006A7520), structure checked, maths inferred

```
// needs: game mode not 4 or 5 (0x005D5E70), creature.dirty (+0xB40, set by 0x0069EE60 on a position change; +0xB41 = "apply at once")
row = apr_base[creature.appearanceRow]                 // cache 0x00C0CB78; row id = word at [creature+0x7B0]+0x58
P, R, prec = row.TerrainFollowPitch, row.TerrainFollowRoll, row.TerrainFollowPrecision   // row +0xA8/+0xAC/+0xB0
if modelKind(creature)==1 (0x005CA710 GDA lookup, column 0xE991BD51): P = max(P, 60), R = ..., prec = 1   // a size class that always uses two probes (inferred)
prec == 0:  one sample. n = ground normal at the creature (0x006954C0); target pitch/roll = the angles of n in the creature's heading frame
prec == 1:  two samples. GroundHeight at two points along the heading, +-(virtual 0x9C, a body length) from the centre,
            (calls at 0x006A7786 and 0x006A77D1; if either misses the ray, the tilt is left as it was);
            pitch from their height difference, roll from the normal (inferred)
pitch_t = clamp(pitch, -P, P);  roll_t = clamp(roll, -R, R)         // 0x006A78F4-0x006A79B0; stored creature+0xB30 / +0xB34
w_pitch = 180 + 540*min(|pitch_t - cur|/180, 1)  deg/s ; same for roll (+0xB38 / +0xB3C)   // 0x006A7A3D..: 180*(1-d)+720*d, inferred
rate = {mode(+0xB20)==2: 0.2, ==1: 0.8, ==0 and a root-motion clip (AnimBase+0xC0): 0.1, else 1.0}   // 0x006A7AF7-0x006A7B20
each frame: cur = the model's current pitch/roll (0x00A5CD00); move cur toward target by w * rate * dt, not past it; write back with 0x00A5CCC0
```

A port that wants the wolf to lean: P = TerrainFollowPitch, R = TerrainFollowRoll from apr_base (the columns are in the GDA), sample the ground at the body's front and back, clamp, slew. For humanoids both are 0, so there is no tilt: the capsule alone gives their feet's height.

### Evidence

- Capsule use: `0x006B6880` reads `creature+0x770`; `[ctrl]+0x18` = "collision enabled" (byte +0xF, 0x00837D20); +0x10 = Move 0x00838240 (`NxController::move`, stores the sides/up/down flags at +0xD/+0xE/+0xC, 0x008382D4-0x008382EB); +0x20 = position getter 0x00838380; +0x1C = SetPosition 0x00838310; +0x24 = SetStepOffset 0x00837DD0; +0x38/+0x3C/+0x40 = colliding down/sides/up (0x00838210/20/30). Names from the address library's Physics rows (22675-22706). Displacement z literal -9.81 at 0x00B0F274, mask 0x2011134 at 0x006B6A4B/0x006B6A84, 0.001 at 0x00B0F278, 1.0, stepOffset 0.5 at 0x00B0F28C (0x006B6B05).
- Slide: 20.0 at 0x00AE72FC, 0.01 at 0x00AF7498, 5.0 at 0x00B02CD0, 3.0 at 0x00AF0C38, clamp 0.9/0.1 at 0x00B0E1F8/0x00AF91D0/0x00AE7D70.
- Sync: 0x006B2220 (z minus +0x784, plus (+0x780 - height)*0.5 at 0x006B2290-0x006B229E, 0.5 at 0x00AE70F8); airborne branch builds (ctrl.x, ctrl.y, creature+0x128) at 0x006B22B3-0x006B22D6 then 0x006B1790(0); grounded branch compares with 0x004D4EF0 and calls SetPosition(p, 0).
- Capsule dims: 0x006B1FD2-0x006B20C3; constants 1.426 (0x00B0FEE0), 0.35 (0x00B0FED8), 0.6 (0x00B0F500), 0.1 (0x00AE7D70/0x00B0F290), 0.2 (0x00AF7608), 1.7 (0x00B0F310). Desc fields 0x006B20DC-0x006B212A (Z up = 2, +0x24 slope, +0x28 0.1, +0x2C 0.5). Radius/height applied by SetRadius/SetHeight slots +0x64..+0x70 in 0x006B1908-0x006B1974; the dimension of the intended values is in +0x77C/+0x780.
- GroundHeight: origin z+3.0 (double 0x00AF0C38), -1.0 at 0x00AE2E00, FLT_MAX 0x00AF0D18 in the scene wrapper 0x0082E620 (`raycastClosestShape` call at 0x0082E71D: shapesType 3, maxDist FLT_MAX, groupsMask from the 5th argument), 100.0 at 0x00AED110 for ray 2; scene vtable 0x00B272D4 slot +0x1C = RaycastClosestRay 0x0082E620, +0x20 = RaycastAllRay 0x0082E830.
- Tilt: loader of the apr_base row 0x00748370 (hash columns 0xD11FCE15 / 0x9B8A2CBE / 0xB6D228CD = TerrainFollowPitch / Roll / Precision, computed with tools/gda.py's column_hash; no other code holds those hashes). Consumer: 0x006A7570 reads row+0xA8/+0xAC/+0xB0 into locals, 0x006A79BB/0x006A79C1 store the clamped result at creature+0xB30/+0xB34, 0x006A7A74 and 0x006A7AC5 the rates at +0xB38/+0xB3C, 0x006A7ACB clears +0xB40. Tick 0x006A7AE0 (reads +0xB30/+0xB34, applies via 0x00A5CCC0). Constants 60.0 (0x00C021D8), 720.0 (0x00C021DC), 180.0 (0x00C021E0 and 0x00AF7ED8), rates 0.2/0.8/0.1 (0x00C021E4/E8/EC).
- IKDNode: refs 0x0067A5CA, 0x0067B842, 0x0067E402 are its destructors in CCArea's address range (0x0067A5C0 etc.).
- Prior notes used: PATHFINDING.md:61-63 and 150-225 (CreateObject, GetSafeLocation, 0x006B1790/0x006B1600, mask 0x2011134), COLLISION.md:62-85 (group bits, creatures in group 4), ANIMATION.md:562-565 (MovementController, 0x006BB910 region, 0x006B6880 named "position integrated"), :606 (corrected here), :1063. The sibling projects hold nothing on creature z or tilt: DAOTurnBased only reads TerrainFollowPitch from the appearance table as "body kind" (Bar.cpp:723, Portraits.cpp:154); Eluvian and the address library have the Physics rows and no notes.

### What this means for the port

- A walking/standing/pushed creature's z should come from a **capsule-versus-static-shapes sweep** (radius max(0.6 r, 0.1), centre offset co, step 0.5, fall up to 9.81 per frame, mask above) or, if the port has the level's physics shapes (core's `Collision`), from a downward ray with the same mask at the creature's xy. Sampling the 0.5 m grid's height will clip on stairs and slopes; the grid is only a path/occupancy structure.
- For a stationary creature the engine still runs the same move every frame it is updated (the displacement is nonzero in z), so a pushed or knocked-back creature settles onto the floor in one frame rather than falling.
- For tilt: only creatures with TerrainFollowPitch > 0 (quadrupeds: wolves, dogs, spiders and the like per DAOTurnBased's use of the column) need it; clamp to the row's two limits and slew at 180 to 720 deg/s.

### Not read

- The sign and direction of the slide (downhill vector) and whether `0x008E6020` is cos or tan (assumed cos 60 degrees by the use as a normal.z threshold).
- The exact tilt mathematics (the quaternion/euler helpers 0x004EE200/0x004EE310/0x004EE370/0x004EE420/0x004EDF80/0x004CB640 not decoded): target pitch/roll from a normal in the heading frame versus from two probes is read from the control flow only; the front/back probe distance (virtual 0x9C) and the second probe pair at 0x006A77E3-0x006A7845 not decoded.
- What sets creature+0xB20 (tilt rate mode), the dirty flag's setters beyond 0x0069EE60, and the meaning of 0x005CA710's column 0xE991BD51 and of 0x006A2CE0 (the "big kind" flag, 0x00631830 == 1).
- The no-controller path of 0x006B6880 (0x006B6B1D: a sweep 0x00685920 then SetPosition) and the global switch at 0x00C353B5.
- The walk grid's height channel (interpolation between cells, 0x00A473A0): not read, not needed.
- A live check: nothing was run in the game; "z set exactly each frame" is the code's behaviour, not an observation. Where the animation layer might offset the model relative to the creature's position (root offset, scale) was not read.
