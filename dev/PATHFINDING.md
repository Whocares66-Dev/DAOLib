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
- **`NearestFree` (0x0073E7F0, `CCArea::NearestFree`), the lateral row** (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) [C, the x87 stack traced by hand for the divisor]: with step = 0.5 × the grid's cell size (0x0073DE50, the double 0xAE70F8 = 0.5), the first probe is `at + dir × step` with z from 0x0073D520, tested with IsLocationSafe (0x0073DE70) and creatureCheck 0; then for d = step, 2·step, … while d ≤ reach (d accumulates as a float sum), n = trunc((d + 0.01) / step) (the double 0xB1B7D8 = 0.01, truncation by 0x008E34F0 = cvttsd2si), and for k = −n … +n in that order the point is `at + dir × d + k × (dir.y, −dir.x)`, **k in metres, not scaled by the step or the cell** (0x0073E961–0x0073E988; the z term is multiplied by the double 0xADE210 = 0.0), z from 0x0073DE20 (the grid's cell height), tested with IsLocationSafe (creatureCheck 0). The first that passes is returned (1) with z = GroundHeight (0x00685430). So the row is 2n+1 points a metre apart, n = d / step = 2d / cell, in order from the −(dir.y, −dir.x) side to the other. If nothing passes: `GetSafeLocation(at, reach, ctl, reachable = the caller's flag argument, out, snapToGround 1, creatureCheck 0, minRadius 0)` (0x0073EA3E). With a null controller or out it answers 0 and writes nothing; with step ≤ 0 it writes `at` to out and answers 0. The row's unit is a metre, and n = d / (0.5 cell) (corrected 2026-10-22: it was noted as cells, n from distance / cell).
- `IsLocationSafe` (0x0073DE70) = `ctl.IsFree(pos)` AND one diagonal ray through 0x0067CFA0, pos + (−s, −s, 0) to pos + (s, s, 2s), both lifted 0.5 m, mask 0x2011134, the mover's own capsule excepted (IsLocationSafe below), optionally a 0x0073D780 check.
- `GetSafeLocation` (0x0073E370, args at, startRadius, ctl, reachable-flag, out, snap-to-ground flag, mask, maxRadius): null ctl/out -> 0; if `at` itself is safe -> returns it (1); else square rings outward from startRadius in steps of 0.5 * cell, each ring walked along its four sides cell by cell, first safe point returned (1; with the reachable flag also a 0x0067CFA0 test from `at`); runs out at maxRadius -> returns 0 and `out` is left alone (checked, except the exact ring corner order). Party placement uses maxRadius 20.0 (0x00AE72FC) as dev/LOAD_EVENTS.md says.
- **`FanSearch` (0x0073DF70)** (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) [C]; the jitter is `rand % 60 - 45`, not `rand % 90 - 45` (corrected 2026-10-22):
  - Arguments `(this = area, at, dir, reach, ctl, out)`. A null `ctl` answers 0. With step = 0.5 × cell ≤ 0 it copies `at` to out and answers 0. dir is flattened (z = 0) and made unit (0x00451A30). The first distance is d = min(step, reach), and d grows by step while d ≤ reach.
  - For each d: the angular increment is `inc = trunc(step × 360 / (8.0 × d))` degrees (the doubles 0xAF2658 = 360.0 and 0xB1B7C8 = 8.0; 0x0073E05F–0x0073E081), that is 45 × step / d, clamped to 5 … 120 (0x78). Then the seed (0x00C36C10, the CRT linear congruential generator, × 0x343FD + 0x269EC3) advances once, and `jitter = ((seed >> 16) & 0x7FFF) % 60 − 45` whole degrees (the divide by 60 is the 0x88888889 multiply at 0x0073E0CC; the 45 is 0x2D), so **−45 … +14**, not −45 … +44. The jitter is drawn once per distance, not per candidate.
  - The candidate angles are a = 0, inc, 2·inc, … while a < 180. For each a the candidate is `at + d × rotate(dir, a + jitter)` about the +z axis (degrees × 0.01745328 to radians, rotation 0x0053DFA0), with z = the ground cell height of `at` (0x0073DE20, taken once). It is accepted when IsLocationSafe(cand, ctl, 0) is nonzero **and** the ray 0x0067CFA0(at → cand, ctl, flag 1) passes; the answer is then 1 and out = cand with z = GroundHeight (0x00685430). When a > 5 the mirrored candidate `rotate(dir, jitter − a)` is tried next with the same two tests (the + side first, the − side second; no mirror for a = 0 or a = 5). After the last a, d += step; when d exceeds reach the answer is 0 and out is not written.
  - It uses the CRT-style rand (seed 0x00C36C10), so a port cannot match it exactly. The meaning of the controller flag byte it sets (+8) was not read.
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
  - any blocked cell and `wide` -> fail. Not wide: the sample's own (centre) cell blocked -> fail; otherwise the blocked neighbour is counted by its direction. The 9 counters are zeroed once (0x00A4BCD8) and accumulate over every sample of the line. After the last sample, and only if any neighbour was blocked, a ring test walks 12 steps over the clockwise ring 0,1,2,5,8,7,6,3 and fails when 4 consecutive directions have a count of 0 (0x00A4BE5E..0x00A4BEF3). A line whose blocked neighbours all lie on one side is refused; a line between walls on both sides passes. Samples run while t ≤ 1. A cell is trunc((v − padding − base)/cell + 0.5) (0x00A49410 for y, 0x00A49440 for x) [C, read 2026-10-21 by lib-agent-2].
  - Height step between consecutive sampled cells > 0.5 (cell heights are sixteenths of a metre; 0x00A49320 returns |dh|/16 and the test is 0.5, 0x00B59324) -> fail.
- So **a line starting in a blocked cell fails at once**: the start sample's centre cell is checked, kind >= 7 (or an enemy-class stamp) fails. It does not use bits 6-7 at all.

### 4. Coarse level and the search start (0x00A4E1B0 -> 0x00A4D8E0) (checked unless noted)

- 0x00A4E1B0 (env request): first the path's own continuation 0x00A4C4A0; on a fresh path it tries `Check(start, goal, 0, 0)` and on success writes the two-point path and returns 1. Otherwise 0x00A4D8E0 sets up and returns 4 (still searching, its state kept for the next call), 7 (set up, and the legs start in this same call: only when the start and goal are the same first-level node, 0x00A4DA94), 5 (no path) or 8 (no mesh), then slices (corrected 2026-10-20 by lib-agent-2: 7 had been read as still searching).
- 0x00A4D8E0: start and goal cells -> block (0x00A49C70, blocks of `sectorSize` cells) -> the cell's region in the block (0x00A4BA20). **A region of -1 for the start or the goal returns 8** (0x00A4DD6F, "no mesh"). 0x00A4BA20: the block's region count (0x00A49CC0): count <= 0 -> -1; count == 1 -> region 0 with NO check of the cell's kind; otherwise the flood 0x00A4B190, which starts with `kind(cell) < 9` (0x00A49C50) and returns -1 for kind >= 9 (inferred: that 0x00A49CC0's byte is the region count; the -1 path of the flood is read). So a blocked kind-9 start or goal in a multi-region block makes the search answer 8; in a single-region block it does not.
- The coarse level treats kinds 0..8 as passable: adjacent cells need both kinds < 9 and a height step <= 0.5 (0x00A49BF0, 0x00B59314). So kind 7 and 8 (door/placeable states) are routed through, which is how a shut door ends as "answer 6 + blocker" rather than "no path" (inferred from the door's behaviour in play, dev/PLACEABLES.md).
- Fine level (MapSearchEnvironment vtable 0x00B592E0: successors 0x00A461A0, cost 0x00A45AA0): the step test (0x00A455C0) needs the from-cell kind < 9, the to-cell kind < 8, and height step <= 0.5 (0x00B592B0). Eight neighbours; a diagonal is dropped when either orthogonal cell is kind 7. A wide mover's env (+4 flag; where it is set not read) drops cells with the creature-ring counter (bits 6-7, 0x00A49240) non-zero.
- Cost: base ~ dx + 1.5*dy form; if env+8 is 0 and the cell changed: +7.0 for a stamp of the mover's own class (kind 5 for class byte != 0, kind 6 otherwise), +3.0 when bits 6-7 are set (0x00A49240 != 0), +8.0 for kind 7, +1.0 for each of the four diagonal neighbours of kind >= 8 and +1.0 more if any diagonal-ish neighbour is kind 7, +2.0 when bits 4-5 are non-zero (constants 0x00B21118, 0x00ADE218, 0x00B1B7C8, 0x00C1CDE8, 0x00AE7108, read).

### 5. Cell kinds and counters, what the exe does with them (checked)

| field | meaning |
|---|---|
| kind 0-4 | walkable everywhere |
| kind 5 | a creature stamp of a ghost (`+0xC90`, set only by `SetCreatureIsGhost` or the save's `SavegameCreatureIsGhost` 16467); seen only by ghost movers (section 12) |
| kind 6 | a creature stamp of every other creature (party, hostile and neutral alike); seen by every non-ghost mover (section 12) |
| kind 7 | placeable/door state: not an end, blocks a straight walk and diagonals; the fine search steps onto it at +8 cost; the coarse level passes it |
| kind 8 | like 7 but the fine search cannot step onto it; IsFree blocks (>= 8) |
| kind 9+ | wall band / outside / off-grid: not passable at any level |
| bits 4-5 | placeable counter, written by 0x00A46E60 (a walk patch with +1/-1 via 0x00A490B0/0x00A49110, max 3). Read by 0x00A49160. Direct fails on a non-zero sample cell; the fine search costs +2.0; IsFree ignores unless flagB |
| bits 6-7 | creature crowding ring, written by 0x00A469C0 via 0x00A491A0/0x00A491F0 (max 3). Read by 0x00A49240 (returns 0 or 1). Fine search: +3.0, and a wide mover will not step there. Not read by Direct; IsFree reads it only with flagA. The fine search's ring term is skipped for a ghost mover (0x00A45B65) |

How a creature stamps, when, and who reads it: section 12.

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

### 7. The fine search's failure: the node it keeps (read 2026-10-19 by lib-agent-2, for DAOParagonEngine #96; checked unless said)

- **On a failed search,** the fine A* (0x00A555B0) builds its path through 0x00A54EF0 to the node it kept at `this+0x40`, and the leg still answers 3.
- **The kept node** starts as the start (0x00A555F1), with its h at `this+0x44`. Only a node newly pushed onto the open list can replace it (in 0x00A552E0, the compare at 0x00A55452), and only with a strictly smaller h, so the first of equal h wins. A node already open whose g is lowered (0x00A55480) does not.
- **h** is `Pathfinding::MapSearchEnvironment`'s slot +8 (vtable 0xB592E0, 0x00A45AA0), called as (node, goal, goal): the octile distance in cells, (max − min) + 1.5·min of the cell differences, a diagonal costing 1.5 (the double at 0x00B03BE8). The same function is a step's cost, called as (parent, neighbour, goal), with the cell's penalties added when the cell is not the goal: +7 for kind 5 or 6 in the mover-obstacle case, +8 for kind 7, and 3 times a count from 0x00A49240.
- **The neighbour order** (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) (0x00A461A0, `Pathfinding::MapSearchEnvironment` slot 2) [C]. The node word is `high 16 bits = column (x)`, `low 16 bits = row (y)` (the call `Grid::CellKind(row = low, column = high)`, 0x00A49070, whose first argument is compared with the row count at +0x24). The pushes are, in order: (x+1, y), (x−1, y), (x, y+1), (x, y−1), then (x−1, y−1), (x+1, y−1), (x−1, y+1), (x+1, y+1). Each neighbour needs the step test 0x00A455C0 and, when the environment's byte +0 is set, a zero count from 0x00A49240 at its cell. A diagonal is pushed only if both of its orthogonal neighbours were accepted and neither is of kind 7.
- **A turn costs 1.0** (read 2026-10-20 by lib-agent-2) [C]: the push (0x00A552E0) stores the step's direction as a 4-bit code, 3·dx + dy + 4, in the node's bits 26–29, and adds 1.0 to g when the code differs from the parent's, neither being 0 (the (−1, −1) step and the start; 0x00A5534D–0x00A55369). So g = the parent's g + the step's cost + the turn.
- **The packed node** [C]: g in bits 0–12 and h in bits 13–25, each stored as trunc(value × 2), clamped to 0xFFF, so both are truncated to half a cell at every step; f = 0.5 · (g field + h field) (0x00A536A0). The kept node on failure compares the untruncated h.
- **The open list** [C] is a binary heap, least f first; of equal f (within 0.001) the larger g first (0x00A53700); with no insertion counter, a full tie keeps the heap's shape order. Sift-up swaps a child up only while strictly better; sift-down takes the left child on a tie and swaps only while the child is strictly better. The first and second levels' generic heap (0x00A52350) orders the same way. A popped node is closed and never reopened; an open one is relaxed only by a lower g (0x00A533C0) [C].
- **The node states** (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) [C]: 0x00A54B30(node) answers 2 when the node has no record (bit 31 of its word clear; 0x00A54320), else `!bit30`. A pushed node's word gets bits 31 and 30 set (0x00A552E0: `(h | 0xFFFE0000) << 13`), so it answers 0 (open). The pop (0x00A55160) clears bit 30 of the popped node's word (`& 0xBFFFFFFF`), so it answers 1 (closed). The loop (0x00A555B0) relaxes on 0, pushes on 2, and does nothing on 1: **a closed node is never reopened by the fine search** (the "[I]" on the fine search's own closed test is read).
- **The relaxation** (0x00A55480) [C]: new g = the cost call (the environment's slot +8 with (parent, node, goal), 0x00A45AA0) + the parent's g field × 0.5. If the node's own g field × 0.5 is **not strictly greater** than it (the compare at 0x00A55513–0x00A5551C: equal or larger returns), nothing changes. Otherwise it stores the step's direction code, 3·dx + dy + 4 in bits 26 to 29 (0x00A55551–0x00A55570), and g = trunc(2 × new g), clamped to 0xFFF, in bits 0 to 12, leaves the h bits alone, and sifts the node up: 0x00A55280 finds its index by a linear scan of the heap's array and calls the sift-up 0x00A54B80 from there. **It adds no turn cost** (no +1.0 in the disassembly; the push's turn cost, 0x00A5534D, is not in the relaxation): it writes the direction code and truncates as the push does, and the only constant it adds is 0x00AE095C, the 2³² correction of an unsigned `fild`, at 0x00A554BD and 0x00A554FB. It does not touch the kept node (+0x40, +0x44).
- **The pop** (0x00A55160) [C]: decrements the heap count (+0x34), increments the expansion count (+0x38), clears the bit-30 flag, moves the last element to the root, and sifts down with 0x00A54CD0(0). The loop takes the root with 0x00A53850 first, ends with success when it equals the goal (after the pop), and after **each neighbour** asks the environment's slot +0x10 (the stopwatch against the limit) and stops expanding when the limit is below the time elapsed.
- The fine search's relaxation sifts with 0x00A54B80 from the node's found index; the generic first and second levels' relaxation is 0x00A533C0 (see the open list above). The fine A* is 0x00A555B0, the push of a new open node 0x00A552E0, the relaxation of an open one 0x00A55480, the heap's push 0x00A55220, its pop 0x00A55160, the node-state query 0x00A54B30.
- Not read: the rest of 0x00A45AA0's penalties, 0x00A54EF0 itself, the environment's second argument (+0x10), and the final swap test of the fine heap's sift-down (0x00A54CD0, read as the mirror of sift-up [I]).

### 8. The whole search's answers, and what DAOLib's `Pathfind` adds (from `src/dao/World.cpp`, seen in play 2026-09-30 to 2026-10-03)

- **The coarse search ends as found when it gives up** (0x00A4D1F0, read 2026-10-03, DAOLib #5): it plans a fine stretch between each two points of its coarse route, and two stretches in a row that fall short of their point end the whole search with answer 1. So a fine leg that fails (3, section 7) shows to RequestPath's caller as 1, "found", with a path that stops short.
- **The answers** RequestPath's search gives, a slice at a time:
  - 1: found, or gave up part way;
  - 4: still searching;
  - 7: set up, and the legs start in the same call (the setup's answer when the start and goal are one first-level node; section 4);
  - 5: no path;
  - 6: barred, a cell of kind 7 on the way. The engine notes the barred cell in the path (`kBarredAt` in World.cpp), for the creature's PathBlocker to look within 5 m of it. The game ends such a move with COMMAND_FAILED_PATH_ACTION_REQUIRED;
  - 8: no walk mesh there.
- **6 comes only near the door** (seen in play through Eluvian): from 1.2 m before a shut door, [4, 4, 6]. From 80 to 143 m off, the search gave up 53.38 m along, short of the door, with 1 and no 6.
- **Inferred, to check against a walk logged through Eluvian:** the end of `Pathfind`'s `partial` path is the kept node of the last failed fine leg (section 7).
- **DAOLib's own, not vanilla's:**
  - `Pathfind`'s budget of 40 ms in slices of 0.01 s, up to 200. The 0.01 s it passes each step is that call's time limit, the search object's +0xD0 (section 11), not a separate argument. A walker's own search gets 0.003 s, or 0.006 s by AI level (RequestPath 0x0073EDEA, section 11).
  - Its searching on from where a search gave up, up to six searches each at least 2 m nearer, where no placeable lies ahead. With it, 229.5 m from Ishal's courtyard found 276.4 m; without it, 244 m in the Korcari Wilds found nothing. How a click's own move reaches the end of so long a walk is not read.

### 9. Invisible walls: placeables that stamp kind 8 under a static coarse route (read 2026-10-20 by lib-agent-2, from DAOTurnBased's play logs; checked in the data, the searches reproduced in Python)

- **Invisible wall placeables stamp kind-8 cells into the live fine grid,** and kind 8 is never stepped onto (section 4). Their patches are `plc_invisowide_01_0_<appearance>.pwk`, 60 × 2 cells of 1/6 m: a wall 10 m long and 0.33 m deep. Kind 8 ends a search with 1, gave up part way; only kind 7, a shut door, gives 6.
- **The coarse level is baked and static:** it is not changed by those stamps, so its route can run through them, and the fine stretches then fall short (section 8).
- **`ran210ar_forest_spiders`** (layout `brc503d`), the forest road's long-standing "no path": ten `wmt_rand_blocker_wide` (appearance 163, tag `wmt_rand_encounter_blocker`) each stamp such a wall.
  - The level's grid is one region of 67,061 cells, and all 6,982 coarse edges are connected in the fine grid.
  - The coarse route's stretches 4 and 5 end in a pocket of 102 cells closed off by the walls and the rock, so two stretches in a row fall short: "found" (1) with "gave up 2", and a partial path of about 15 m to the kept node.
  - A path of about 40 m round the walls' ends exists on the stamped grid, which vanilla does not find, because it follows the coarse route. The route was reproduced here, not read from 0x00A4D1F0.
- **`urn200ar_ruined_temple`** (layout `lak401d`): the "World Map" `wmt_wow_generic` (appearance 159, "Area Transition, Invisible, Wide") stamps `plc_invisowide_01_0_159.pwk`, kind 8 in both its states, across the whole 6 m entrance corridor. No path either way, answer 1.
- These explain DAOTurnBased's DAOLIB_NEEDS item 6 cases. A port matching vanilla stamps the patches into the fine grid and leaves the coarse level as baked.

### 10. The second coarse level: an entrance's parent, and the search over it (read 2026-10-20 by lib-agent-2, DAOLib #215)

[C] read in the GOG exe's disassembly or checked over the game's layouts; [I] inferred. Node = `block << 16 | entrance` on both levels.

#### An entrance's parent [C]

- **The sector's offset** (0x00A49DC0(grid, block)) is the 16-bit word of the sector's dword at grid +0x14, plus 0x10000 when `block >= grid+0xC`. The loader sets the wrap index +0xC to 0x7FFFFFFF (0x00A4A79D), then to the first sector whose word is below the one before (0x00A4A7B0–0x00A4A819): the single wrap core's `SectorsOf` applies. The place (0x00A4A840), the entrance's end of edges (0x00A4A190, 0x00A4A1D0) and the edges (0x00A4AB80) all read from this base.
- **The parent byte** (0x00A49F00(grid, block, k)) is `memory[offset − k − 1]`, bounds-checked: −1 for block −1, or when the sector count is `columns × rows` (one level; 0x00A49F04–0x00A49F28, 0x00A49F5D). So each first-level sector of n entrances is preceded by exactly n bytes, entrance k's the k-th back; second-level sectors have none.
- **Checked over the 213 layouts the game takes:** the 204 with a second level hold 71,518 parent bytes, every one a valid index into its parent block's second-level entrances, and 0xFF (none to the engine) in none [C]; the 0xFF test is for a grid built at run time [I]. The 8 with one level (block size 16: arena2, brc201d, combat2, lak519d, orz703d, rmtst, rmtst01, rmtst03) have none, and den510c has no grid.
- **The parent block** (0x00A49D10(grid, block)) is `(col >> 1) + ((columns + 1) / 2) × (row >> 1)`, col = block % columns, row = block / columns, −1 with one level (0x00A49D34–0x00A49D63). It matches core's second level, which has `(columns + 1) / 2` columns.
- **In core:** `grid::Entrance::parent` (DAOLib #215), the parent block's entrance index, −1 on the second level or with one level.

#### The search over the second level [C unless marked]

- **Its environment** is `MapSuperAbstractionSearchEnvironment` (vtable 0xB592CC): slot 1 the successor (0x00A45D10), slot 2 the cost and h in one function (0x00A45670), slot 3 a stub returning false. The A* (0x00A53440) is the first level's, and found means the popped node is the goal. Setup (0x00A4D8E0) makes the start's and goal's super nodes from their parents, and skips the super search when both are the same node.
- **Successors** (0x00A45D10 → 0x00A4AD70): the entrance's stored edges in stored order, the direction (an edge byte's top 3 bits) mapping to the neighbouring parent block: 0 N −pc, 1 E +1, 2 S +pc, 3 W −1, 4 NE −pc+1, 5 SE +pc+1, 6 SW +pc−1, 7 NW −pc−1, with pc = (columns + 1) / 2 (the jump table at 0x00A4AF34). No filter, mask or bounds check.
- **Cost:** the octile distance `(max − min) + √2 · min` in cells (the double 0xB55040, not the fine level's 1.5) between the two entrances' places (0x00A49F70: side = 2 × blockCells, x = side × col16 / 16 + side × parent column, y likewise, col16 and row16 the place byte's nibbles). Unless b is the goal or the distance is 0, it adds 2 × side if b's cell has the placeable counter (bits 4–5), and 2 × side again if its kind is ≥ 8 or 6, or the creature ring (bits 6–7) is set.
- **h** is the same call with b the goal, so it takes no penalty, and the A* weights it: **f = g + 1.2 · h** (0x00A53394, the double 0xAFEBC0). The fine A* has no weight (section 7's f = g + h stands).
- **A route of more than 5 nodes** sets a mask (0x00A464D0): `mask[block] |= 1 << entrance` per route node. The first level's successor (0x00A45F20) then keeps a neighbour only if `mask[parent block] & (1 << parent entrance)`, confining the fine search to the corridor the super route passed through. With 5 nodes or fewer nothing is dropped.
- **A route of 26 nodes or more** plans the first level to its midpoint node, route[n / 2]: 0x00A4A080 looks for a first-level entrance under it in the child blocks TL, BL, TR, BR in that order (only the left two when the columns are odd and the block is in the last column), and finding none answers 5.
#### The start and goal, the setup's calls, the long route's rest, and the legs (read 2026-10-20 by lib-agent-2) [C unless marked]

- **A cell's node** is `block << 16 | region`, the block from 0x00A49C70 and the region from 0x00A4BA20: an entrance count of 0 is −1, of 1 region 0, else the flood. The region is the index of the entrance it returns. The flood (0x00A4B190) runs fresh on every call. It is a breadth-first search inside the block over the local index row·size + col, with a std::deque and a visited bitset. Neighbours are 4-connected, pushed in the order row+1, row−1, col−1, col+1. A step needs both cells' kind < 9 and a height difference ≤ 0.5 (0x00A49BF0, 0xB59314). A start of kind ≥ 9 gives −1 (0x00A49C50). At each popped cell the entrances are tested in index order, matching on their place byte. A match is returned if it has more than one edge (0x00A4A190 − 0x00A4A1D0), or exactly one edge whose target has more than one (0x00A4AB80); otherwise the search goes on. An empty queue gives −1.
- **No temporary node joins the start or goal.** 0x00A4A8E0 overwrites the place byte of the start's (and goal's) parent second-level entrance with the real cell, quantised to a sixteenth of the super block, for the one search, and 0x00A4A3B0 restores it. Its cost and h then measure from the real cells. On the first level 0x00A4A840 does the same with the start's and goal's own entrances (undone by 0x00A4A260), except on a route to the midpoint, where the start keeps its entrance's own place.
- **0x00A45CD0** is "has a second level": the sector count differs from columns × rows. **0x00A49D70**, the mask's size in words, is ((columns + 1) / 2) × ((rows + 1) / 2) with a second level, 0 without.
- **The setup (0x00A4D8E0) runs over calls:** the first call runs the super A*, answering 5 if it finds no route (no fallback to the first level alone) or 4. The next call takes the super route: the mask when it has more than 5 nodes, the first level's A* to the goal or, with 26 nodes or more, to the midpoint, and answers 4 (5 for an empty route).
- **The rest of a long route** is planned by 0x00A4DDA0 on the driver 0x00A4E1B0's 10th call, when the super route has 26 nodes or more: it drops the half route's last two nodes, masks the corridor from its tail to the goal, and runs the first level from the tail to the goal, joining the two. A shorter route is never re-planned.
- **The counter** (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) [C]: the driver 0x00A4E1B0 adds 1 to the path's counter at +0x24 **first thing on every call**, before the done test (0x00A4C4A0) and before the setup. So calls 1 and 2 are the setup's (while the path's active byte +0 is clear the driver tries the straight walk 0x00A4BB80 and then the setup 0x00A4D8E0, which answers 4 to the first call with the super route and, on the second, plans the first level and sets the byte), and the replan of the long route (0x00A4DDA0) is taken on the call where the counter equals 10, if the route vector (+0x18, +0x1C) has more than 25 nodes (`0x19 < count`), after the setup branch and before any other step. It is the 10th call of the driver, the 8th after the two of the setup. A search whose done flag the first test finds ends before it.
- **What a call after the setup does** [C]: it is **not always a leg.** 0x00A4C520 (called with the world and the follower) answers true when the done flag (+1) is set or when `(point count of the path, vector at world +0x74/+0x78, stride 0xC) − (the path's +0x44)` is **greater than 1**, and the driver then does a smoothing step (0x00A4C580 / 0x00A4CE40, or the window and corner passes 0x00A4BA70 / 0x00A4D110 / 0x00A4CEE0 / 0x00A4C5E0, by the conditions of section 11). Only when it answers false does the driver run a leg (0x00A4D1F0). +0x44 is the larger of itself and the follower's next index (+0x88). 0x00A4C580 answers true unless `(+0x30 − +0x44) < 3` and `+0x30 < point count − 1`. So the legs are paced by how far the follower has consumed the path, and the 10th call may fall on a smoothing step: the replan test comes first and does not depend on that.
- **A leg** (0x00A4D1F0, one a call) is one fine A* from where the last stopped to the next route entrance whose cell has a kind below 8, or to the goal at the last. On a sharp turn at that entrance (0x00A4C140: the squared difference of the two unit directions ≥ 1.0) it commits only part of the leg, about half the run of path cells inside the entrance's block, or a quarter when the angle there is obtuse (0x00A4BAA0), so the next leg can cut the corner; a straight leg commits all of it. After one failed leg the retry runs without the cells' penalties; two failed legs in a row end the search ("gave up").
- **Nothing coarse is built at run time.** 0x00A4A690 is a constructor over the layout's baked bytes (its caller is the loader 0x00A4AA80/0x00A4A9F0); it computes nothing. Entrances, edges, places, parent bytes and the super level are never recomputed at run time.
- **A door and the coarse levels:**
  - A placeable's stamp (0x0074DBA0 → 0x00A46CC0) writes only the low nibble of fine cells (0x00A49550).
  - Regions come from a flood over kind < 9, and a stamp writes only 7 or 8, so a door never splits or joins a region.
  - A stamp reaches the coarse search only through live reads of the destination entrance's cell b in the first-level cost 0x00A457E0 (and its super twin 0x00A45670): +1.0 if b's block does not have exactly one entrance (0x00A49CC0); +2 · sectorSize for the placeable counter (0x00A49160); another +2 · sectorSize (0x00A45A3E) for kind ≥ 8, kind 6, a creature ring, or, for a wide mover (env +4 = path +0x8D, at 0x00A4D947), any of b's 8 neighbours of kind ≥ 8.
  - The leg skips a route entrance of kind ≥ 8 as its goal.
  - Entrances are baked one per connected region of a block, inside the region rather than on the border. That held for 172 of 173 blocks on pre410ar_tower_level_1's layout; the placement rule itself is not recoverable.
- Not read: whether any level's odd columns make 0x00A4A080's two-candidate case happen, and what happens when the legs finish the half route before the 10th call. The smoothing steps and the corner tests are read in section 11.

### 11. Smoothing, the search's time budget, and the penalties' switch (read 2026-10-20 by lib-agent-2; C checked, I inferred)

- **Smoothing, three passes** [C]:
  - **Smooth** (0x00A4C790) grows a run of path points while the polyline's length equals the octile distance between its ends within 0.25 m (0xAE7FE8), then calls Collapse on it.
  - **Collapse** (0x00A4C2A0) tests the straight line between the run's ends with 0x00A4BB80 (flags 0, 0). If it is clear, it erases the interior and, where the gap is over 2 · `size` (the mover's size, world +0xC8), puts points back every `size`; if not, it halves the run and recurses on each half.
  - **The corner pass** (0x00A4C930, through 0x00A4D030 and 0x00A4D110) cuts corners; its fallback cut (a 0.5 m setback) is read from the code, not run.
  - Smooth runs inside each leg (0x00A4D1F0), over [0, n − 1] and then over [the old last, the new last]. Between legs the driver (0x00A4E1B0) picks a 10-point window slid along (0x00A4C4A0 and its helpers), a corner pass or a collapse of the tail, by the slice's parity and P +0x88 (the follower's next index). P +0x8C (a large mover near its goal) turns the between-leg passes off; the in-leg Smooth runs regardless. P +0x8D (wide) affects only which cells the fine search takes. The last leg appends the exact goal after its Smooth (0x00A4D888); its goal cell V[0] is never appended (cut 0). done does not end the search: the driver keeps running its passes until 0x00A4C4A0 sees the window and the corner pass at the end, so the goal is inside the window's Smooth (0x00A4CE40) and the tail's Collapse (0x00A4CEE0 → 0x00A4C5E0), unless P+0x8C is set.
  - There is no snapping or pass for points in a line. Cells become positions through 0x00A49B70, in 0x00A4C630.
- **The corner tests,** used by the leg alone (0x00A4D1F0: 0x00A4C140 at 0x00A4D576, 0x00A4BAA0 at 0x00A4D691, their only call sites; corrected 2026-10-22 by lib-agent-2, this said the setup) [C]: 0x00A4C140 is "sharp" when |u2 − u1|² ≥ 1.0 for the two unit directions, a turn of 60° or more; 0x00A4BAA0 is the integer test for an obtuse angle.
- **The commit** (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) (in the leg 0x00A4D1F0, after the fine A*) [C]: it applies when the fine search succeeded, the route has a next entrance (`index + 1 < route count`), the world's byte +0x8C is clear (the large mover near its goal), the route index is below the last, and the sharp test below answers "sharp".
  - The three points, in (column, row) order, are **the leg's start cell, the leg's end cell (the entrance it reached; the fine path's element 0), and the next route entrance's cell** (from `MapAbstraction::EntranceCell` of route entry index + 1). The fine path is stored from the goal back, so its element 0 is the leg's end and its last element the leg's start (that 0x00628590 is the path's last element is read from the use, not from the function).
  - 0x00A4C140 takes the three points as integers and returns 1 when `|u2 − u1|² < 1.0` (u1 the unit direction of the first to the second point, u2 of the second to the third; the float compared, at 0x00ADDC94, is 1.0), else 0. The leg commits part of the path when it returns **0** (the turn is 60° or more).
  - The run: starting at path index 1, it counts the following elements whose block (0x00A49C70) is the same as element 0's block, the last such index being `run`. The commit is `run >> 1`, or `run >> 2` when 0x00A4BAA0 (the same three points) answers true. The next leg starts at the path element with that index (stored at +0x28); when the test does not apply the commit is index 0, the whole path.
- **A leg that ends on a kind-7 cell answers 6** [C]: after the fine A* (0x00A4D6E7):
  - The leg tests the kind of V[0]: the cell where the fine path ends, which is the leg's goal or the kept node of a failed search.
  - If V[0] is kind 7, the leg clears the search's active byte and stores that cell's world position at P+0x84 (the barred position). It appends nothing and returns 6.
  - The driver and RequestPath pass 6 through (0x0073EE76).
  - A path that only crosses kind-7 cells inside a leg is not barred, so "a shut door ends as 6" holds only when the leg ends on the door's patch.
  - The walk command's move update (0x007A92F8, RequestPath's only direct caller) stops the creature (0x006A9430(0)) and fails the command with −9, PATH_ACTION_REQUIRED. It does not open the door.
  - At the command's completion (status −9, 0x00743EC3), `CCCreature::PathBlocker` 0x006A7E10 returns the first object of the creature's list +0x690 within 5 m of P+0x84 whose footprint test (0x0074DC40, not read) holds, else −1. The approach command then retries from the door's ring spot when the blocker is its own target (TURN_BASED.md:402).
- **The corner pass settles** [C for the test; I for the counts] when W.3C − W.44 > 5 or W.3C == W.30 (0x00A4BA70). Each step (0x00A4D110) advances W.3C by 5: after one call if it changed nothing, or after 11 calls if it keeps cutting. So the first window takes 2 to 22 driver calls, and a straight path about 2.
- **The time budget** [C]:
  - The search object at world +4 has a stopwatch at +0xCC (made at the world's construction, kept at world +0xE0 and copied in at each step by 0x00A467A0) and the limit in seconds at +0xD0, the step's one argument. RequestPath (from 0x0073EDEA) passes 0.003 × (AI level ≤ 3 ? 1 : 2) (0xB1B7E0 = 0.003).
  - Each A* call restarts the clock and stops when the limit is below the time elapsed, with no resume. The super and first levels then answer 5; a fine leg counts as failed (path +0x48 += 1), and two in a row answer 1 (section 8). There is no cap on a whole search's time, nodes or length.
  - So vanilla's give-up on a long route depends on the machine's speed. A port wanting vanilla's results without that can replace the clock with a fixed expansion count large enough never to trip, and keep the two-failed-legs rule [I].
- **The penalties' switch,** the fine environment's +8 [C]: it gates every penalty term of the step cost (0x00A45AA0); the base distance (so h) and the push's turn cost are not affected. With it clear, and b not the goal and the distance not 0: +7 for a kind-5 cell when the mover's stamps stop it, else +7 for kind 6 and +3 for each creature ring on the cell; +8 for kind 7; +1 for each of the four diagonal neighbours of kind 8 or more, and +1 once if any is kind 7; +2 for a placeable's counter on the cell. The leg sets the switch (0x00A4D3F3) when path +0x48 > 0, so a retry after a failed leg runs without penalties and without the wide filter, until a leg succeeds and +0x48 goes back to 0.
- Not read: the corner pass's fallback cut as run; the rounding of Collapse's point count (0x008E34F0); the append helper's length sum (0x00A4C630); the long-route tail at the 10th call (0x00A4DDA0); whether the 3 or 6 ms clock trips on a normal machine at 290 to 370 m; 0x0074DC40 (the blocker's footprint test) and what fills creature +0x690.

### 12. A creature's stamp: when, its footprint, who reads it (read 2026-10-21 by lib-agent-2 with two subagents, for DAOParagonEngine #130; C checked, I inferred)

**The footprint** [C, 0x00A469C0]:
- **The radius** is r = max(0.6 × PersonalSpace, 0.1). PersonalSpace is apr_base `PerSpace` ([[creature+0x7B0]+0x60]); the doubles are 0.6 at 0xB0F500 and 0.1 at 0xAE7D70.
  - It is set by 0x006A2A40 at creature set-up and AddToArea, and stored at params +4 by 0x00A62730.
  - A human (0.35) gets 0.21, a wolf or Mabari (0.45) 0.27, a bear (1.0) 0.6.
- **The core**: cells whose centre has d² ≤ r² + cell²/2. That is 0.41 m for a human and 0.45 m for a wolf: one to four cells of a 0.5 m grid.
  - A hard stamp writes kind 6, or kind 5 for a ghost (`sete al; add al,5`).
  - It skips cells of kind ≥ 7 and cells already 6. Kind 5 is overwritten.
- **The ring**: non-ghosts only. Cells outside the core within r + 1.0 m (r + 0.5 m for a soft stamp) get the bits 6–7 counter +1. The counter is capped at 3 in the writer (0x00A491A0); the stamper's own "≥ 3" skip never fires, since 0x00A49240 answers 0 or 1.
- **The record**: every cell the stamp touched is kept in a bit record (params +0x48, its size at +0x4C).
- **The lift** (0x00A627D0) replays the record at the recorded position, radius, class and soft flag:
  - core cells still 5 or 6 go back to **kind 1**, whatever the file had (0, 2, 3 or 4);
  - ring cells (and a soft core) get −1.
  - A cell another creature had already stamped was not recorded by the second creature. So lifting the first one clears it while the second still stands there [C for the rule, I for its effect in play].
- **Soft** = state +0xB20 is 1 (walk) or 2 (run) [C, 0x006A3BDA]. A soft stamp writes no kind: its core cells get the counter, and its band is 0.5 m. So **a walking creature blocks no straight walk**; it costs +3 and keeps wide movers off.

**When** [C unless marked]:
- **Gates** (`CCCreature::PlaceObstacle` 0x006A3BC0):
  - vslot 0x1D0 = 0x006A3660, IsDead (no stats object, or health ≤ 0), must answer 0;
  - +0x2C5 (active) must be set.
  - A failed gate returns without lifting.
- **Place always lifts first** (0x00A629F0 → 0x00A627D0), so the stamp follows the creature.
- **Every position change**: `CCCreature::SetPosition` 0x006B5EF0's tail lifts (0x006B609C) and places (0x006B60A3) while the creature is in an area. The walk moves through it, so a walker re-stamps each frame it moves (a soft stamp).
- **The end of a walk**: the stop 0x006A9430 (from the move update 0x007A9130) sets state 3 or 0 and places a hard stamp.
- **AddToArea** (0x006B3070) places when state 0 and active, and lifts when inactive.
- **SetActive** (0x006B5D90) places when on and lifts when off, except an active-party follower's deactivation, which returns early.
- **SetGhost** (0x006BBFC0) lifts, flips +0xC90 and places again.
- **RequestPath** lifts the mover and its walk-to target, then places both at the end (section 1).
- **The melee ring's spot** (0x006A8F30/0x006A8F81), the away-goal chooser 0x007A5450 ("The away goal" below), sub-action 16 (0x007A8F74) and the approach update (0x007A8DD8, 0x007A553C) also lift and place.
- **Death**: the health handler (0x006BC4B0, health falling to ≤ 0) calls slot 0x78 = 0x006B3620, which lifts. The exception is a creature whose appearance's DecayBehaviour is 2 (0x006A3A80: Broodmother, High Dragon): its corpse keeps the stamp until its position next changes, since the IsDead gate then refuses the new one.
  - A revive (0x006B3690) places it again.
  - So the dead do not block.
- **Destroy and RemoveFromArea leave a stale stamp** [C: RemoveFromArea 0x006B6E60 and 0x006B3070 read in full; the obstacle's destructors 0x00A63020, 0x00A62880, 0x00A62590 and 0x00A484C0 only free buffers].
  - RemoveFromArea runs ClearAllCommands (0x0073C930), then 0x006B3070 (0x006B6ED2).
  - A creature standing still (state 0) is re-placed there, so it leaves a hard kind-6 stamp.
  - A walking one has been put in state 3 by the clear, so it is skipped and leaves its soft stamp (counters).
  - The stamp goes only when the area's grid is freed (the area's destructor 0x00684760 → 0x0067C130), or when the creature is added to another area: the place's lift uses the record's own area.
  - So in an area that stays loaded, a destroyed living creature's cells stay stamped. That is a vanilla fault, not seen in play [I for its effect].
- **A command cancelled mid-walk** [C]: ClearAllCommands → the shared end 0x00746900 → 0x00A63AC0 (state 1 or 2 → 3). StopMovement 0x006A9430 is not called, so the stamp stays soft (counters only) until the creature next moves.
  - SetState 0x00A63800 does no stamp work.
  - Attack set-up and "movement cleared" also set state 0 without a place.
- **SetRadius 0x006A2A40 on a placed stamp** [C]: AddToArea and creature set-up (which `SetAppearance` reaches on a live creature) can run it while a stamp is placed. When the new radius needs a bigger record, the record is freed without a lift and refilled with 0xFF. The next lift then undoes every cell of the old circle, other creatures' kind-6 cells among them.
  - A smaller radius changes nothing: the lift uses the copy at +0x44.
  - The two other callers (0x0065B011, 0x0065B1B1) are stack temporaries, never placed.

**Who blocks whom** [C]:
- `+0xC90` is the ghost flag, written only by the constructor, `SetGhost` (from `SetObjectProperty` case 0x283, COMMAND_SETCREATUREISGHOST) and the save's loader.
  - The shipped scripts set it for the Fade's spirits, wisps, the Harrowing's spirits and Hespith (arl300pt_fade, bhm600cr_wisp, cir320cr_darkspawn_sp, den960cr_eerie_girl, orz550cr_hespith).
- **Everyone else is class 0 and stamps kind 6**. There is no test of team or faction: party members block each other, hostiles block the party, and the party blocks hostiles.
- Ghosts see only ghosts' kind 5.
- World +0xC4 (class) and +0xC8 (size) are written only by RequestPath, so other callers of the tests read the last requester's values.

**The readers** [C]:

| test | stamps |
|---|---|
| IsFree 0x00A46FC0 | ignores kinds 5 and 6, except with flagA = 1 (one caller, 0x007CB15E, a melee stand-point probe). flagB is the placeable counter |
| straight walk 0x00A4BB80 | a sample's 3×3 cell blocks on kind 6 (non-ghost mover) or kind 5 (ghost mover) when flagA = 0. `wide` there is the full PersonalSpace > cell (World +0xC8). RequestPath's Direct, Collapse, the corner pass and the driver pass (0, 0). flagA = 1 only in formation (0x00760766), 0x007A2D84 and the look-ahead 0x00A62C90 |
| fine search, cost 0x00A45AA0 | with the penalties on (env+8 = 0): +7 for the mover's own class kind, +3 for a ring (non-ghost movers only). A stamped cell is never refused |
| fine search, step | a wide mover drops ring cells. Wide (path +0x8D → env+4) is set in `Step` 0x00A62640 (0x00A626B2) when params +4 > cell, i.e. 0.6 × PS > 0.5, PS > 0.83: bears, drakes, bronto; not humans, wolves or Mabari. It is never cleared there |
| coarse levels (0x00A457E0, 0x00A45670) | only at the destination entrance's cell: +2 × sectorSize for kind 6 (any class) or a ring. The regions are baked; a stamp never changes them |

**While walking** [C by enumerating every direct caller of 0x00A4BB80, 0x00A46710, 0x00A46760, 0x00A46FC0, 0x00A62A90 and 0x00A626F0; I for indirect calls]:
- Nothing re-tests a walking creature's path against stamps.
- A blocked walker is caught only by the stuck counter (1000 ms, answer −4; TURN_BASED.md).
- The melee approach's look-ahead (0x00A62C90) uses flags (1, 1), so it ignores stamps.

**For a port** [I]:
1. Stamp at every position change, with lift-then-place.
2. Write counters only while walking or running, and a hard kind 6 once stopped.
3. Write no stamp for the dead or the inactive.
4. Lift the mover and its target during its own search.
5. Make the straight walk refuse kind 6, and the fine search add +7 and +3 for them.
6. Leave IsFree and the coarse regions alone.

**Not read**:
- Nothing left open from this reading.

### What a port should copy (summary)

1. Keep the grid's cell kinds as the file's 4-bit kinds, with the counters; implement `IsFree` as in section 2 (cells blocked at >= 8 / == 7, but only by centre distance <= max(r, cell)), not "the cell under the point is blocked".
2. Do not refuse a blocked end. Start: IsFree -> if not, a safe point within 5 m (the exe: a random fan toward the goal, else the first safe point of a forward/lateral scan, else rings by half a cell), moving the creature when the scan found one. Goal: if not free, a safe point within 2 m back toward the start; none -> keep the goal.
3. Straight walk first (it fails on a blocked centre cell); then the coarse/fine search with the "no region" answer 8 for a kind >= 9 end in a multi-region block; then, if the ends are within 0.5 m in height, a physics straight-line test with the mover's radius as the last resort (answer 1 if clear, else 5).
4. Lift the mover's own stamp (and the walk-to target's) before all of this; put it back after.

### Not read

- GetSafeLocation's ring walk order past the first two sides (NearestFree's lateral row is read, 2026-10-22).
- The meaning of controller byte +8, and 0x00A62F30 / 0x00A62B90 (what the found-path finish does).
- Where the environment's wide flag (env +4) is set; 0x00A49CC0's array layout (taken as region count); 0x00C4C0C0's value (a runtime sanity limit in IsFree).
- 0x00685920's body past the first calls (the physics walk test); 0x0073D780; 0x0067CFA0.
- Which creatures have `+0xC90 != 0` (the class byte) and what states `+0xB20` 1 and 2 are.
- That ost100d is the area the wolves were in (it fits the coordinates, grid extent and the height, not confirmed by area name).

## The away goal: `CommandMoveAwayFromObject` (read 2026-10-22 by lib-agent-2 with a subagent, for DAOParagonEngine #144)

- **The command** (561, 0x0066F430; `script_commands/commands.md`:145) makes a type-21 command with object 0 the target, float 0 = 0.5, float 1 = 0, float 4 = the distance and flag 5 = 1 [C]. A creature target runs sub-action 15 (`ApproachRanged`, 0x007AA200); a placeable takes sub-action 4 and ignores float 4.
- **The chooser** 0x007A6CA0 calls 0x007A5450 (ID 19843, no name in the address library) when float 4 is not 0 (0x007A6D63), and 0x007A31C0 (the plain approach goal) when it is [C]. Flag 5 keeps the goal from being chosen again on later updates [C]. With float 4 set, ApproachRanged's "already in range" exit is off [C].
- **0x007A5450** (handler, mover, centre = the target's position, distance, out) [C unless marked]:
  - The base direction is centre → mover in x and y. Only when the mover is within 1 mm of the centre is it a random angle.
  - The mover's own stamp is lifted while it searches and put back after (PATHFINDING.md, "Every position change").
  - Candidates are `centre + distance × Rz(base ± off)`: off starts at `rand % 20 − 10` degrees and grows by a step clamped to 10..90 degrees (from the mover's vtable +0x9C and the distance; what +0x9C is was not read [I]) while off < 171. +off is tried first, −off only when off > 5. The first candidate accepted wins.
  - **Every candidate is at the full distance from the target.** None is shortened or clamped to the last reachable cell.
  - **The tests are the grid's:** `IsFree` at the goal, `Direct(0, 1)` from the mover to it, then `IsFree` with flag 1. One that passes the first two but not the third is kept once as a fallback. No physics shape is asked.
  - With no candidate, it answers 0, and sub-action 15 fails with −4.
- **The walk ends** within float 2 = 0.5 m of the goal (0x007A9130), or by the stuck counter, a failed path or a timeout (TURN_BASED.md, "What ends an approach otherwise") [C]. The AI adds its moves with `AI_COMMAND_TIMER` (10 s; `ai_main_h_2.nss`:469, 557, 586, 856). `WR_AddCommand` (`wrappers_h.nss`:227–235) cuts a non-follower's first timed command in combat to 1.0 s (`CREATURE_HAS_TIMER_ATTACK`) [C].
  - **The Harrowing's wisps** walked about 3.4 m in vanilla on a 13 m `CommandMoveAwayFromObject` from the bear. The goal is 13 m from the bear, about 9.7 m from the wisp, so the walk was cut short, most likely by that 1 s timeout [I].

## The spawn: is a creature on a blocked spot moved? (lib-agent-2, 2026-10-18, GOG exe, addresses GOG's)

### Short answers

1. **Placed creatures from the .are are NOT moved by a safe-location search while the area loads.** They are added by the area's list step 0x0067F420, which calls the creature's AddToArea (CCCreature vtable +0xF8 = 0x006B6BC0) with the creature's own stored position (+0x120). AddToArea ends by calling SetPosition (vtable +0x7C = 0x006B5EF0, args (pos, 1)). SetPosition only runs the snap (0x006B1790) when the game mode is not Loading: it reads `[0xC327EC]+0x114` -> `+0x78` and skips the snap if it equals 9 (GameModes.h: Loading = 9), setting the physics body straight to the position. Area load runs in mode Loading, so placed creatures stand exactly where the .are puts them (checked in disassembly; "the mode is Loading during the area's object adds" is inferred from LOAD_EVENTS.md's order).
2. **CreateObject (0x00669550) does not call GetSafeLocation.** It makes the object through 0x00687830 -> factory 0x006861F0, which takes only x,y of the location, replaces z by the ground height from 0x00685430 (a physics ray down from z+3.0 over 100 m; the input z if nothing is hit), then calls AddToArea(area, &pos, 0). That AddToArea ends in SetPosition as above, now in a non-Loading mode, so the snap in 0x006B1790 is reachable (checked).
3. **That snap is gated by a physics test, not by the walk grid.** 0x006B1790(arg 0) first calls 0x006B1600: a query on the area's physics world (`[area+0x62C]`, vtable slot 0x34, mask 0x2011134) at the creature's position, with the creature's controller capsule moved there (`Physics::PhysicsScene::OverlapCapsule`; COLLISION.md section 5). Only if it answers true does it search: `GetSafeLocation(pos, 10.0, the creature's movement controller, reachable=0, out, snapToGround=1, creatureCheck=0, minRadius=0)`. If the creature is moving (controller 0xA630E0 true) it uses NearestFree (0x0073E7F0) instead. If nothing is found it keeps the position. True means the capsule overlaps a shape whose bits0 meets 0x2011134: static geometry, placeables, water, trees, terrain walls, and creatures' controllers, the mover's own excluded (checked 2026-10-22: the call returns its count of shapes > 0; COLLISION.md section 5). A cell the walk grid calls blocked but with no physics overlap is therefore NOT a trigger. (Corrected 2026-10-19 by lib-agent-2: the test gates only that extra safe-spot search. Its false branch jumps to the final controller setPosition at the ground's z, 0x006B1979–0x006B1997, so outside mode 9 the z snap always runs, for a move over 0.05 m: "What z a creature ends at" below.)
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

Order within a ring: left to right along the bottom (low y) and top at each x (bottom, then top), then the sides bottom to top (left, then right). The first success wins, so the nearest ring wins and ties go to the low-y row. The first ring is at half a cell (0.25 m for 0.5 m cells) and each ring adds half a cell; the ring's points are spaced half a cell too. That the success exit z-snap runs unconditionally is read at 0x73E75B (checked). The order checked in disassembly by lib-agent-2 on 2026-10-22, the coordinates too:

  - Each ring is centred on the caller's `pos` (argument 1), not the ground-snapped copy.
  - The x loop starts at `pos.x − r`. It tests `(x, pos.y − r)` (y loaded at 0x0073E56A) and then `(x, pos.y + r)` (0x0073E5C2), with x increasing.
  - The side loop tests `(pos.x − r, y)` (0x0073E666) and then `(pos.x + r, y)` (0x0073E6B5), with y increasing.
  - Nothing is random or tied to the creature's facing.
  - So a ring's first point is its low-x, low-y corner. With 0.5 m cells, the first point of the third ring is (−0.75, −0.75) from `pos`, 1.06 m away.

When nothing is found: the loop exits at 0x73E74C -> `xor eax,eax; ret 0x20` with `*out` NOT written (the callers pre-copy: 0x006C7664's caller keeps its spot, GETSAFELOCATION returns the input). The two earlier early exits (step <= 0 and the physics early-out at 0x73E48A) write `*out = p` and also return 0. (checked)

#### IsLocationSafe 0x0073DE70 `(pos, mc, creatureCheck)`

1. `mc->IsFree(pos, areaNo, 0, 0)` (0x00A62A90): forwards to the grid, `GridIsFree(pos, areaNo, r = personalSpace*0.5, includeStamp6 = 0, crowdCheck = 0)` = 0x00A46FC0 (checked). Its rule: the cells overlapping the box pos +- r (cell indices of the box's corners). For each cell with kind >= 8 or == 7 (6 too if includeStamp6; or a crowd count > 0 if crowdCheck; kind 9 is also what an off-grid cell reads as): the spot is blocked if that cell's centre is within r of pos (xy, 0x004EB010), or within one cell size (`grid+0x30`) of pos. A non-finite position (NaN/inf against the +inf at 0xC4C0C0) or an empty index range answers free. The cell's height is NOT used. Kinds 0-6 are free, including a creature's stamp (5, 6). (checked, except exactly how the box indices clamp)
2. **One diagonal ray** (read 2026-10-22 by lib-agent-2, for DAOParagonEngine #144; this said "from one point to another ... not decoded") [C 0x0073DEA1..0x0073DF06]:
   - s = `[[mc+0x10]+4]`, the movement params' +4 (max(0.6 × PerSpace, 0.1), :249, :306). The call is `0x0067CFA0(from = pos + (−s, −s, 0), to = pos + (+s, +s, 2s), mc, 0)`: one ray across the footprint's diagonal, rising 2s.
   - `0x0067CFA0` lifts both ends by 0.500001 (the double 0xB0DAA0) and casts the scene's `RaycastClosestSegment` (slot 4) with mask **0x2011134** for flag 0, or 0x2011124 for any other flag (0x0067CFF0..0x0067D03A). IsLocationSafe passes 0 itself (0x0073DEAE).
   - **It passes** when nothing is hit, or when the hit's flags byte has 0x10 [I: a creature's shape] and the hit's id equals `[[mc+0x10]]`, the params' +0. That is the mover's own id, which the snap writes there at 0x006B1839 (`mc` is `[creature+0xA40]`, 0x006A2570). Any other hit fails (0x0067D0AB..0x0067D0D7).
   - With no physics world or scene it returns false (0x0067CFD4, 0x0067CFE2), so the spot is not safe.
   - pos is the caller's point: the centre after GroundHeight when GetSafeLocation's snap flag is set, and a ring candidate at the walk grid's height (:397).
3. If creatureCheck: 0x0073D780, a 2-D segment against creatures' discs (corrected 2026-10-22 by lib-agent-2, this said "a box test against other objects ... not decoded"; read with a subagent):
   - **Not a box: a 2-D segment against creature discs** [C], `bool __thiscall CCArea::(x0, y0, x1, y1, mc, a6, a7, int* outBlocker, a9)`, `ret 0x24`, 1 = clear. IsLocationSafe passes the same diagonal as its ray, (pos − s) to (pos + s) in xy, with the four trailing arguments 0. z is not used.
   - **Early outs:** a segment shorter than 1e-6 on both axes answers 1 (0x0073D78C..0x0073D7E7). A null `mc` answers 0 (0x0073D802).
   - **Which objects:** the area's object ids at `area+0x694`, kept sorted by x by the add (0x00682450), are narrowed by a lower-bound search (0x0067CD00) to x within [min − 6.0, max + 6.0] (the double 0xB1B7C0). Each goes through `GetCreatureById` (0x005D58C0), so only creatures count. No physics, no grid crowd counts.
   - **Which creatures count:** those not dead (slot 0x1D0, 0x006A3660), visible (slot 0x138, 0x006A0E70; a stealthed creature is skipped [I for the meaning]) and active (the +0x70 interface's slot 0x6C, 0x00699FA0). Hostility, party and ghost status are not tested.
   - **Footprint:** each creature is a disc at (`+0x120`, `+0x124`) of radius `[[creature+0xA40]+0x10]+4`. The mover's own is s + 0.01 (0xAE7FF0). A disc within the sum of the two radii of the segment blocks [C for the radii; I for the exact segment-distance arithmetic].
   - **Excluded:** the mover's own id (`[[mc+0x10]]`) and its walk target (`[mc+0x94]`).
   - **The trailing arguments** [I for their meaning]: a6 limits contacts to within the sum of the radii of the segment's start; a7 adds the test 0x006A8D10; a8 receives −1 or the blocker's id; a9 adds a test against the walk target. With all four 0, the per-object verdict 0x0073D570 answers "blocks".
   - **The snap 0x006B1790 passes creatureCheck 0** [C 0x006B18B6..0x006B18E0]: `GetSafeLocation(&pos, 10.0, mc, reachable 0, &out, snap 1, creatureCheck 0, 0.0f)`, as :875 has it. The last argument 0.0 makes `at` itself tested first (0x0073E3C6..0x0073E3D0). By :410, the spawn snap and GETSAFELOCATION also pass 0, and the party arrival and the script's SETPOSITION pass 1 (not re-read here).

#### What "reachable" is
The extra call `0x0067CFA0(this, pos, candidate, mc, 1)` (flag 1 -> mask 0x2011124): a physics ray from the original position to the candidate (z lifted 0.5). It is a line of sight through the level's static shapes, not path reachability, and not the pathfinder.

### A creature's stamp

Section 12 has the stamp: its footprint, when it is placed and lifted, and who reads it. The ring is written for non-ghosts only, and a walking creature's stamp is counters only. There is no check against other creatures and no separation: three wolves on one point stamp once, the others find the cells already 6 (checked). IsFree ignores stamps by design, so stacked creatures remain a legal spot for each other.

### For the port

- Do not snap a creature at area load. Place it at the .are's position as given. (The engine does no search for it.)
- A blocked end of a path is not a spawn problem in the engine: `RequestPath` has its own order (IsFree at the destination, else NearestFree within 2 m back toward the start; already in TODO.md). The only GetSafeLocation uses are the ones listed above (script SETPOSITION safe: 5 m; GETSAFELOCATION and the party arrival: 20 m; physics-overlap snap on a runtime spawn: 10 m). None applies to an area's placed creatures.
- If a grid-blocked cell holds a placed creature, the engine just leaves it; a pathfinder that refuses paths from a blocked start differs from the engine, whose start is `SetStart` of the creature's own cell (see RequestPath in TODO.md: the mover's own obstacle is lifted, a straight walk tried, then slices). Whether the engine's search accepts a blocked start was not read here.

### Not read
- Meaning of 0x006493E0 (GetSafeLocation's early physics query that returns 0 with the unchanged spot).
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

#### The capsule's size (0x006B1ED0, checked; the sources read 2026-10-22 by lib-agent-2), all in metres

```
h = min(Height of the creature (virtual 0x1B0 with 0), 1.7)      // 0x0069EE10: apr_base Height, else the head node's, else 1.75
r = apr_base PerSpace ([[creature+0x7B0]+0x60], 0x006B1FD6)
if inActiveParty(creature) /*0x006A2CE0*/: h = max(h, 1.426); r = max(r, 0.35)   // floors
cr = max(0.6 * r, 0.1)                       // capsule radius,  stored creature+0x77C
ch = max(h - 2*cr - 0.2, cr + 0.1)           // cylinder height, stored creature+0x780
co = cr + ch/2 + 0.1                         // centre above the feet, stored creature+0x784
controller desc: up axis Z (2), slopeLimit = cos(pi/3)+1e-6 (0x008E6020 not decoded; cos assumed, 0.5), skinWidth 0.1, stepOffset 0.5
```

The 0.1 is the gap under the capsule: feet z = centre - co.

- **h** = `0x0069EE10` [C]: the creature's slot 0x1B0 (`0x006A1A30`) called with 0. That is `apr_base` `Height` (`[[creature+0x7B0]+0x64]`, EFFECTS.md:573) when the creature has a size record. Without one it is the height of the model's `head` node above the model, or 1.75 m with no model or node (COMBAT.md, "The attack's sight ray"). Then h = min(that, 1.7): the double 1.7 at 0x00B0F310 is compared, and the float at 0x00B0F30C is returned when it is not below 1.7.
- **r** = `apr_base` `PerSpace` (`[[creature+0x7B0]+0x60]`, 0x006B1FD6) [C].
- **The "big kind" is not a kind** [C]: `0x006A2CE0` is "in the active party". It returns `0x00631830(party, id) == 1`, with the party from `0x006A2CC0`, and `0x00631830` is `IsFollower` on the active party map `+0x234` (PARTY.md:13, :139). So every member of the active party gets h = max(h, 1.426) and r = max(r, 0.35) (the floats 0x00B0F280 and 0x00B0F284; the doubles compared are 0x00B0FEE0 and 0x00B0FED8). These are floors, so a bear's PerSpace of 1.0 stays 1.0.
- cr = max(0.6·r, 0.1) [C 0x006B2024..0x006B207A, stored at +0x77C]. `ch` (+0x780) and `co` (+0x784, `ch` × 0.5 at 0x006B20B7) are as the block says [C for the stores and the 0.5; the order of the x87 sums was not re-traced].

#### GroundHeight 0x00685430(this = area, pos, byte* hitOut, vec4* normalOut) -> z, checked

```
ray1: origin (x, y, z+3.0), dir (0,0,-1), length FLT_MAX, groups all, mask M
      M = 0x24 (Placeables|StaticGeometry) and the hit must not carry flag 0x01000000 (Nonwalkable)   if area+0x658->+0x7C == 1 (0x0067B4D0): the .arl's EnvAreaChunkIsChunk, below
      M = 0x2000000 (Walkable)                                                                         otherwise
if hit: z = hit.z; *hitOut = 1; *normalOut = hit.normal; return z
else:   ray2 from z+100 down with all hits; take the hit with the smallest |hit.z - z| whose flag 0x01000000 is clear (65505 start of the min); if none, return z
```

(The 0x0067B4D0 flag's meaning is not read; mask choice checked, who sets it not.)

##### The mask's switch: area+0x658 → +0x7C is the level's EnvAreaChunkIsChunk (3122) (lib-agent-2, read 2026-10-19)

###### Short answer (checked)
ChunkManager+0x7C is the area layout's "is chunked" flag: GFF4 field 3122 `EnvAreaChunkIsChunk` (bool, int8) of the area's layout file `<name>.arl` (root struct), stored as a dword 0/1. Rule a port can copy: `ground uses mask 0x24 (reject hits with flag 0x01000000) iff the area's .arl has EnvAreaChunkIsChunk != 0`, else mask 0x02000000 (Walkable). Nothing is computed and no data table is involved.

Core already exposes it: `src/dao/core/Level.cpp:407` `kAreaIsChunked = 3122`, read at :644; when nonzero it fills `out.chunks` (`Chunks{rows, columns, width, height}` from fields 3123/3124/3125/3126). So "chunked" = `layout.chunks.has_value()` (check the field name in Level.h).

###### Evidence
- Test 0x0067B4D0 (checked, daoexe --disasm): `mov eax,[ecx+0x658]; test; je -> 0; cmp dword [eax+0x7c],1; sete al`. Matches master dev/PATHFINDING.md:335.
- Writers of [reg+0x7C] in the layout load 0x005D1290 (esi = ChunkManager; checked by disasm of the whole function, only two stores): 
  - 0x005D168E `mov [esi+0x7c], 1`, 0x005D1A7B `mov [esi+0x7c], edi` (edi = 0).
  - The branch is `test bl,bl` at 0x005D1676 / `je 0x5D1A71`. bl is set at 0x005D1610-0x005D1617: a GFF4 bool read (call 0x4EBFC0, as the other bool reads) of field label 0xC32 = 3122 (`push 0xc32` at 0x005D15E7), `setne bl`.
  - True branch (0x7C=1) goes on to read 0xC33.. (3123 RowCount, 3124 ColCount, 3125 Width, 3126 Height; `[esi+0xA0]` takes the row count). False branch (0x7C=0) reads 0xBC8 = 3016 `EnvAreaRoomList`, i.e. the room-list layout. So the flag selects chunked (terrain grid) vs room-based levels. 
  - 3122 name from master dev/gff4_fields.csv: `3122,EnvAreaChunkIsChunk` (also 3123 RowCount, 3124 ColCount, 3125 Width, 3126 Height).
- File values (tools/gff4.py --dump of packages/core/env/<name>.arl, root fields; checked):
  - ost100d.arl (Wilds): IsChunk=1, 6 x 6 chunks, width 128.0.
  - lak100d / lak100c (Lake Calenhad exteriors): 1, 10 x 8, width 132.0.
  - lak101d (an interior): 0 (no chunk fields).
  - Scan of 95 .arl in env/ with prefixes brc, lak, ost, orz: 30 are 1, 65 are 0. The 1s: brc000d, brc100d, brc101d, brc300d, brc501d-505d, brc997d, brc999d, lak100c/d/n, lak200d, lak250d, lak300d, lak400d, lak403d, lak510d, lak515d, lak526d, orz100d, orz200d, orz300d, ost000a/c/t, ost100d, ost101d. All the rest of those prefixes are 0. (I did not get a Circle Tower or Redcliffe file: "cir*" and "red*" prefixes were not in my scan; lak101d stands as the interior example. Note ost000a/c/t, the Ostagar camp, are chunked too, so "chunked" is exterior terrain areas, not only the Wilds.)

###### Consequence for the port
Chunked areas (Wilds, Ostagar, Lake, Brecilian ...) use mask Placeables|StaticGeometry (0x24) minus Nonwalkable-flagged hits in GroundHeight; room-based areas use the Walkable shapes (0x02000000). This is consistent with the terrain shapes being 0x80002020 (static geometry, not Walkable).

###### Prior work
master dev/PATHFINDING.md:335 and its note "who sets it not read" is now settled; dev/COLLISION.md:116 (ChunkManager vtable 0x00AFFE24); dev/TODO.md:49 (0x005D1290 as the layout load). New here: the store sites, field 3122 as the source, and the file values.

###### Not read
Other consumers of +0x7C besides 0x0067B4D0 (no xref scan of `[reg+0x7c]` outside 0x005D1290 and the one test); Circle Tower / Redcliffe interior files; the chunk-grid behaviour itself; whether the .arl is the only source (the load reads the layout it is given at 0x005D1290; I took the .arl as the file Level.cpp reads).

#### Body tilt (0x006A7AE0 -> 0x006A7520), structure checked, maths inferred

```
// needs: game mode not 4 or 5 (0x005D5E70), creature.dirty (+0xB40, set by 0x0069EE60 on a position change; +0xB41 = "apply at once")
row = apr_base[creature.appearanceRow]                 // cache 0x00C0CB78; row id = word at [creature+0x7B0]+0x58
P, R, prec = row.TerrainFollowPitch, row.TerrainFollowRoll, row.TerrainFollowPrecision   // row +0xA8/+0xAC/+0xB0
if modelKind(creature)==1 (0x005CA710 GDA lookup, column 0xE991BD51): P = max(P, 60), R = ..., prec = 1   // corrected 2026-10-19: not a model kind but the current animation row's State column = 1 (deaths and sleeps), below
prec == 0:  one sample. n = ground normal at the creature (0x006954C0); target pitch/roll = the angles of n in the creature's heading frame
prec == 1:  two samples. GroundHeight at two points along the heading, +-(virtual 0x9C, a body length) from the centre,
            (calls at 0x006A7786 and 0x006A77D1; a miss gives 0 for pitch and roll, not the old tilt: corrected 2026-10-19, below);
            pitch from their height difference, roll from the normal (inferred)
pitch_t = clamp(pitch, -P, P);  roll_t = clamp(roll, -R, R)         // 0x006A78F4-0x006A79B0; stored creature+0xB30 / +0xB34
w_pitch = 180 + 540*min(|pitch_t - cur|/180, 1)  deg/s ; same for roll (+0xB38 / +0xB3C)   // 0x006A7A3D..: 180*(1-d)+720*d, inferred
rate = {mode(+0xB20)==2: 0.2, ==1: 0.8, ==0 and a root-motion clip (AnimBase+0xC0): 0.1, else 1.0}   // 0x006A7AF7-0x006A7B20
each frame: cur = the model's current pitch/roll (0x00A5CD00); move cur toward target by w * rate * dt, not past it; write back with 0x00A5CCC0
```

A port that wants the wolf to lean: P = TerrainFollowPitch, R = TerrainFollowRoll from apr_base (the columns are in the GDA), sample the ground at the body's front and back, clamp, slew. For humanoids both are 0, so there is no tilt: the capsule alone gives their feet's height.


##### Quadruped body tilt: the maths, instruction by instruction (read 2026-10-19, GOG exe, read-only) (lib-agent-2)

"checked" = read in disassembly or data (GOG exe, base 0x400000). "inferred" = reasoned from checked code, named.
Builds on lib-agent-2-ground-walk.md (now in master's dev/PATHFINDING.md around :360-400, the tilt section) and corrects it where marked CORRECTION. Prior notes: PATHFINDING.md (GroundHeight 0x00685430, mask switch on EnvAreaChunkIsChunk), AUDIO.md:1006 (AnimBase position setters 0x00A5CD50 / 0x00A5CDF0 and orientation 0x00A5CCC0), ANIMATION.md:455 / :1386 (column 0xE991BD51 = the animation table's `State`). The sibling projects hold nothing on the maths.

###### Short answers

1. **The target pitch and roll come from the ground NORMAL, never from a height difference.** For each sample: q = ShortestArc(Z=(0,0,1) -> n) * Rz(heading); pitch = elevation angle of the rotated forward axis (+Y), roll = the twist about that axis left after peeling the yaw and pitch off the left (decomposition q = Rz(yaw)·Rx(pitch)·Ry(roll)). Degrees throughout. **Positive pitch = nose up** (forward axis z > 0; so a rise ahead, normal leaning back, gives positive). **Positive roll = right-hand rotation about the forward axis = right side DOWN** (right axis z < 0; ground lower on the right gives positive). (checked: every helper below.)
2. **Precision 0** (TerrainFollowPrecision == 0): one sample, the normal cached at the creature (0x006954C0: GroundHeight at the creature's own position, normal out; (0,0,1) if no hit/no area). **Precision 1**: two samples, GroundHeight called at `pos + (0,0,H) + fwd*L` and `pos + (0,0,H) - fwd*L`, with `L` = the creature's personal space radius (virtual 0x9C -> [[creature+0x7B0]+0x60]) and `H` = its height (virtual 0x1B0(0) -> [[creature+0x7B0]+0x64]); each gives its own normal; pitch and roll are computed per normal exactly as above and then **averaged (x0.5)**. The returned heights are thrown away (`fstp st(0)` after both calls). If either ray misses (hit flag 0) both targets are 0. **Any other precision value: targets 0** (checked: 0x006A7650 `test eax,eax`, 0x006A76C1 `cmp eax,1; jne`, 0x006A78F4). **CORRECTION** of my earlier "pitch from the probes' height difference ... left as it was": it is the two normals averaged, and a miss sets 0.
3. **Clamp**: degrees, pitch to [-P, P], roll to [-R, R], P and R the table's TerrainFollowPitch/Roll. **CORRECTION**: the "60 / precision forced to 1" case is NOT a size class. It is when the creature's *current animation row's `State` column (0xE991BD51) == 1* (0x006956B0 returns AnimBase+0x13C, the current animation id; 0x005CA710 looks it up in the animation table: State). State 1 rows are the death/dying/dieloop/sleep animations (ghilana, Animations table, 62 rows: dieLoop_v2, dieloop, die, die_v2, db_c_*_die_v1, sleep, ...). So a **dead or sleeping creature gets P = max(P,60), R = max(R,60) and two probes**, which lays a corpse onto slopes even for a biped (checked code; the meaning from the table rows, inferred as "corpses lie on the ground").
4. **What is turned: the model's orientation only.** The tick builds `Rz(curYaw)·Rx(pitch)·Ry(roll)` and hands it to `AnimBase::SetOrientation` 0x00A5CCC0 (creature+0x154), which calls vtable slot +0x1C of the node at `[[AnimBase+0x18]+4]` and the sound emitter's orientation (+0x1B0). The creature object's own facing (+0x130), its position (+0x120) and its physics capsule are untouched (checked: 0x006A7520 and 0x006A7AE0 write only creature+0xB30/+0xB34/+0xB38/+0xB3C/+0xB40 and that call). So **visual only; the capsule stays upright**. The pivot is the model's origin: the position is set separately (AnimBase position setters, AUDIO.md:1006) to the creature's position, i.e. the feet; no translation is applied to compensate. Which scene node slot +0x1C belongs to (root or a "GOB" node): not read.
5. **Slew** (0x006A7AE0, arg dt in seconds, float): per axis, `cur += sign(target-cur) * min(w * k * dt, |target-cur|)`, with a snap to target when `|target-cur| < 1e-6` or when the "apply at once" flag (+0xB41) is set. `w` (deg/s) is fixed when the target is last computed: `w = 180*(1-t) + 720*t`, `t = min(|target - cur|/180, 1)`, computed per axis with the distance at that moment. `k`: 1.0; mode (+0xB20)==2: 0.2; ==1: 0.8; ==0 and 0x00695670 true (an object of the root-motion/animation holder with +0xC0 != 0): 0.1. (checked, below.)

###### Pseudo-code a port can copy

```
// Conventions: z up, model forward = +Y, right = +X. quaternion (x,y,z,w). Angles in degrees outside quats.
// All constants checked in the exe unless noted.

quat qAxisAngle(axis, rad)    = (axis*sin(rad/2), cos(rad/2))                  // 0x004CBC40, half = rad*0.5 (0x00AE70F8 = 0.5)
quat qmul(a,b)                = Hamilton a*b                                    // 0x004CB640 (x = a.x b.w + b.x a.w + a.y b.z - a.z b.y ...)
vec3 qForward(q)              = (2(xy - wz), 1 - 2(x²+z²), 2(yz + wx))          // 0x004CBD30 (rotated +Y)
vec3 qRight(q)                = (1 - 2(y²+z²), 2(xy + wz), 2(xz - wy))          // 0x004CBCE0 (rotated +X)
float yawOf(q)   = deg(atan2(-qForward(q).x, qForward(q).y))                    // 0x004EE310 (computes atan2(-2(xy-wz)... ) x 57.29582595825195 at 0x00AF0E50)
float pitchOf(q) = deg(atan2(f.z, sqrt(f.x² + f.y²))), f = qForward(q)           // 0x004EE370
float rollOf(q, after yaw and pitch peeled)                                      // 0x004EE420:
    q1 = qAxisAngle(Z, rad(-yawOf(q))) * q                                       //   left-multiply: removes the yaw
    q2 = qAxisAngle(X, rad(-pitchOf(q1))) * q1                                   //   removes the pitch
    return deg(atan2(-qRight(q2).z, qRight(q2).x))                               //   what is left is a twist about Y
quat eulerToQuat(yaw, pitch, roll) = qAxisAngle(Z,rad(yaw)) * qAxisAngle(X,rad(pitch)) * qAxisAngle(Y,rad(roll))   // 0x004EE200 (product order read at 0x004EE2E5-0x004EE2F9)
quat shortestArc(A,B) = normalise both; d = A.B; c = A x B; s = sqrt(2(1+d)); (c/s, s/2)   // 0x004EDF80 (OGRE getRotationTo; normalise 0x004EDE50 -> (1,0,0) for a zero vector)
                                                                                 // identity/opposite special cases at 0x004EE071-0x004EE0F3: irrelevant for ground normals
float heading(fwd)  = deg(atan2(-fwd.x, fwd.y))                                  // 0x004EE570, creature+0x130 is fwd (unit, xy plane)

// per sample (normal n):
sample(n, qYaw):  q = qmul(shortestArc((0,0,1), n), qYaw);  return (pitchOf(q), rollOf(q))

// target (0x006A7520). Runs only when creature.dirty (+0xB40) is set, game mode not 4 or 5 (0x005D5E70), an area exists; clears dirty at the end.
row = apr_base[ word at [[creature+0x7B0]+0x58] ]   // cache 0x00C0CB78; P,R,prec = +0xA8,+0xAC,+0xB0 (TerrainFollowPitch/Roll/Precision, in degrees, integer precision)
if animationRow(creature.currentAnim).State == 1:  P = max(P,60); R = max(R,60); prec = 1       // 0x006A75A5-0x006A75D3, 60.0 at 0x00C021D8
qYaw = qAxisAngle((0,0,1), rad(heading(creature.fwd)))                            // 0x006A7621-0x006A7647, deg->rad 0.017453279346227646 at 0x00AF14C0
pitch = 0; roll = 0
if prec == 0:
    n = groundNormalCache(creature)               // 0x006954C0: GroundHeight(area, &creature.pos, null, &normal), normal (0,0,1) if no hit; cache valid flag +0x1C4
    (pitch, roll) = sample(n, qYaw)
elif prec == 1:
    L = creature.personalSpace;  H = creature.height
    p1 = creature.pos + creature.fwd*L + (0,0,H);  p2 = creature.pos - creature.fwd*L + (0,0,H)
    n1 = (0,0,1); n2 = (0,0,1)
    if !GroundHeight(area, p1, &hit1, &n1) or !hit1: pitch = roll = 0 (goto clamp)  // 0x006A7786 / 0x006A778D
    if !GroundHeight(area, p2, &hit2, &n2) or !hit2: pitch = roll = 0 (goto clamp)  // 0x006A77D1 / 0x006A77D8
    (pa, ra) = sample(n1, qYaw);  (pb, rb) = sample(n2, qYaw)
    pitch = (pa + pb)*0.5;  roll = (ra + rb)*0.5                                   // 0x006A7899 fadd, 0x006A78CF * 0.5 (0x00AE70F8)
pitchT = clamp(pitch, -P, P);  rollT = clamp(roll, -R, R)                          // 0x006A78F4-0x006A79B0
creature.targetPitch = pitchT (+0xB30);  creature.targetRoll = rollT (+0xB34)      // 0x006A79BB / 0x006A79C1
if !creature.applyAtOnce (+0xB41):                                                 // 0x006A79B2 / 0x006A79C7
    q = AnimBase.GetOrientation();  cp = pitchOf(q);  cr = rollOf(q)                // 0x00A5CD00, current values read from the model itself
    t = min(|pitchT - cp| / 180, 1);  creature.wPitch (+0xB38) = 180*(1-t) + 720*t  // 0x006A7A35-0x006A7A74, 180 at 0x00AF7ED8 / 0x00C021E0, 720 at 0x00C021DC
    t = min(|rollT  - cr| / 180, 1);  creature.wRoll  (+0xB3C) = 180*(1-t) + 720*t  // 0x006A7A7A-0x006A7AC5
creature.dirty = 0                                                                 // 0x006A7ACB

// slew (0x006A7AE0, called each creature update, 0x006BD566; dt seconds)
tilt_target(creature)                                                              // the above
k = 1.0;  mode = creature[+0xB20]
if mode == 2: k = 0.2;  elif mode == 1: k = 0.8;  elif mode == 0 and hasRootMotionHolder(creature) /*0x00695670*/: k = 0.1   // 0x00C021E4/E8/EC
q = AnimBase.GetOrientation();  yaw = yawOf(q);  cp = pitchOf(q);  cr = rollOf(q)
newP = step(cp, creature.targetPitch, creature.wPitch)
newR = step(cr, creature.targetRoll,  creature.wRoll)
step(cur, tgt, w):  d = |tgt - cur|
                    if d < 1e-6 (0x00B0F2A4) or creature.applyAtOnce: return tgt
                    return cur + sign(tgt - cur) * min(w * k * dt, d)
if newP == cp and newR == cr: return                                               // 0x006A7CBE-0x006A7CE8
AnimBase.SetOrientation( eulerToQuat(yaw, newP, newR) )                            // 0x006A7CED-0x006A7D17 -> 0x004EE200 then 0x00A5CCC0
```

Note the yaw: the slew keeps the model's own current yaw (read back from its orientation); only pitch and roll are replaced.
Note the first-order picture for a hand check: with a unit normal n and heading unit vector f (xy), pitch ≈ -(n·f) rad and roll ≈ +(n·r) rad (r = right), small angles, signs as in answer 1 (inferred from the exact quaternion maths, not a separate read).

###### Evidence (addresses)

- 0x006A7520 (target): entry guards 0x006A7537-0x006A754E (`call 0x5D5E70` game mode, `cmp eax,5/4`, dirty byte +0xB40); row 0x006A755B-0x006A758E (`[creature+0x7B0]+0x58` word, cache 0x00C0CB78 via 0x004E1140; +0xA8/+0xAC/+0xB0 into locals); State test 0x006A7592-0x006A75D3 (0x006956B0 -> AnimBase+0x13C; 0x005CA710 -> GDA lookup col 0xE991BD51 on table 0xC1D090; `cmp eax,1`; max with 60.0 for both limits; precision := 1); area = virtual +0xA0 (0x00695CD0) at 0x006A75DB, none -> out; H = virtual +0x1B0 (0x006A1A30) with arg 0 -> [[creature+0x7B0]+0x64]; heading 0x006A761C (`call 0x4EE570` on creature+0x130) then `* 0.017453279346227646`, quaternion about 0x0057C4F0's (0,0,1) via 0x004CBC40 into the local at [esp+0x88]; prec 0 path 0x006A7654-0x006A76BC (0x006954C0 refresh, 0x004EDF80(out,(0,0,1),creature+0x1B0), 0x004CB640(out, qGround, qYaw), 0x004EE370 pitch, 0x004EE420 roll); prec 1 path 0x006A76CA-0x006A78ED (virtual +0x9C twice = [[creature+0x7B0]+0x60], 0x008D8B00 = vec*scalar, positions via `addps` with creature+0x120, the (0,0,H,?) vector at [S0+0x20], `call 0x685430` twice, hit flag at [S0+0x17]); average 0x006A7899-0x006A78E9; clamp 0x006A78F4-0x006A79B0; stores 0x006A79BB / 0x006A79C1; weights 0x006A7A35-0x006A7AC5; dirty clear 0x006A7ACB.
- 0x006A7AE0 (tick): 0x006A7AE6 calls the target routine first; k selection 0x006A7AED-0x006A7B26; model orientation 0x006A7B2A-0x006A7B7F (`creature+0x154`, 0x00A5CD00 reads it through `[[AnimBase+0x18]+4]` vtable +0x38; 0x004EE310 yaw, 0x004EE370 pitch, 0x004EE420 roll); pitch step 0x006A7B86-0x006A7C1E (epsilon 1e-6 at 0x00B0F2A4, `[esi+0xB38]*k*dt`, clamped to |delta|, sign by compare with target, +cur; `[esi+0xB41]` short-circuit); roll step 0x006A7C22-0x006A7CBA with +0xB3C and +0xB34; unchanged test 0x006A7CBE-0x006A7CE8; write 0x006A7CED-0x006A7D17.
- Helpers (all read in full): 0x004CBC40 (axis-angle -> quat, args: axis ptr, radians; ecx = out; half angle via 0.5 at 0x00AE70F8; fsincos), 0x004CB640 (Hamilton product), 0x004CBD30 / 0x004CBCE0 (rotated Y / X axes; constant 2.0 at 0x00AE7108), 0x004EE310 (atan2(-2(xy-wz)... sign: constant -2.0 at 0x00AF2660, so atan2(2(wz-xy), 1-2(x²+z²)) = atan2(-fwd.x, fwd.y)), 0x004EE370 (elevation), 0x004EE420 (peel yaw by left-multiplying a Z quat of -yaw, peel pitch by an X quat of -pitch, roll = atan2(-right.z, right.x) via the second 0x004CBCE0), 0x004EE200 (three quats from axes 0xC30260 = (0,1,0) roll, 0xC1E870 = (1,0,0) pitch, 0xC1EED0 = (0,0,1) yaw; angles are args 4, 3, 2; product (Z·X)·Y), 0x004EE570 (atan2(-x, y) of a vec), 0x004EDF80 + 0x004EDE50 (shortest arc with normalise), 0x008D8B00 (vec * scalar), 0x0057C4F0 (returns the static (0,0,1) at 0x00C1EED0). `fpatan` wrapper 0x008E893A. Degrees from radians: 57.29582595825195 at 0x00AF0E50.
- Constants: 60.0 0x00C021D8; 720.0 0x00C021DC; 180.0 0x00C021E0 (float) and 0x00AF7ED8 (double); 0.2/0.8/0.1 at 0x00C021E4/E8/EC; 1e-6 at 0x00B0F2A4 and 0x00AE09B0; deg->rad 0.017453279346227646 at 0x00AF14C0; read with the camconst tool.
- GroundHeight (cited, not redone): PATHFINDING.md ground section; it writes the normal only on a hit, so the (0,0,1) initialiser stays on a miss and the probe code tests the hit flag instead.

###### Checked / inferred

- Checked: every helper's maths; the prec 0 / 1 / other branches; the averaging; the clamp; the two weights; the slew; the Euler product order of 0x004EE200; the write to AnimBase orientation only; the two probe positions (pos +- fwd*personalSpace, raised by height); State==1 rule (the code path; the rows are from ghilana).
- Inferred: that the table's Pitch/Roll limits are degrees (compared with degree results and a 60.0 literal; the table's values were not looked up here); the sign statements in answer 1 (derived from the exact formulas above, not seen on screen); that 0x004EDF80's cross product has the standard sign (the special-case branches and w = s/2, c/s were read; the cross lines at 0x004EE142-0x004EE16E look standard); the small-angle first-order approximation.

###### Not read

- Which scene node slot +0x1C of `[[AnimBase+0x18]+4]` is (root of the model or another), and whether its parent adds a transform.
- Who sets the dirty flag (+0xB40, beyond 0x0069EE60 from the earlier note), "apply at once" (+0xB41) and the tilt mode (+0xB20); the creature update's frequency (the tick is called at 0x006BD566; dt there was not read, assumed seconds).
- The special branches of 0x004EDF80 for d near +-1 (0x004EE071-0x004EE0F3, uses 0x004D4EB0) and the exact cross-product lines.
- The actual TerrainFollowPitch/Roll values per appearance row (the table was not read in this pass; DAOTurnBased only uses Pitch as a "body kind").
- Whether the corpse (State 1) case matters for a port that has no death-animation rows; the dead creature's GroundHeight mask (the area flag at 0x0067B4D0) was not revisited.
- Nothing was run in the game.

##### The tilt's three inputs: probe distance, the "modelKind" test, the rate mode (read 2026-10-19, GOG exe, read-only) (lib-agent-2)

"checked" = read in disassembly or data. "inferred" = from control flow or a constant's meaning. Extends `%TEMP%\lib-agent-2-ground-walk.md` (tilt section, 0x006A7AE0 -> 0x006A7520). Prefix tiltb. Prior notes: dev/TURN_BASED.md:136 (PerSpace), dev/ANIMATION.md:472, :1140, :1386 and dev/script_commands/objects.md:154 (State column 0xE991BD51), dev/AUDIO.md:1263 and :1281 (+0xB20 as the movement state), dev/PATHFINDING.md:116 (soft stamps on +0xB20 in {1,2}), dev/PARTY.md:229/:233. The sibling projects hold nothing on the tilt itself.

###### Short answers

1. **Probe distance = the creature's own PersonalSpace, apr_base `PerSpace`.** The virtual at +0x9C is called on the creature itself (`this`, esi), CCCreature vtable 0x00B10484 slot 39 = 0x006A21A0 = `CCCreature::PersonalSpace` (ID 14451): `[creature+0x7B0] ? [[creature+0x7B0]+0x60] : 0.0`, the size record's PerSpace (checked, 0x006A21A0, 0x006B1FD6 reads the same field). It is read once per creature from apr_base's `PerSpace` column (dev/TURN_BASED.md:136; not re-read by me). Not a bounding box, not a constant. The two probe points are `pos + facing * PS` and `pos - facing * PS` (checked structure: `0x008D8B00` is vec*scalar, out = v * s; facing is the unit vector at creature+0x130, position creature+0x120). **Wolf: PerSpace 0.45 m, so probes at +-0.45 m, a 0.9 m baseline** (apr_base row "Wolf", ghilana: PerSpace 0.45, Height 1.29, TerrainFollowPitch 16, TerrainFollowRoll 4, Precision 1, ModelType W). Same 0.45 for Mabari/party dog/Wolf, Blight; Witherfang 0.60; Bear, Black 1.0; Great Bear 1.1; Bronto 1.5; Drake 1.4; Deer 0.6; Cat 0.24.
2. **"modelKind" is not a model or appearance field. It is the `State` column of the creature's current animation row**, and `== 1` means a dying / dead / downed animation. `0x006956B0` returns the animation set's current animation id (`[[creature+0x154]+0x13C]`), and `0x005CA710(id)` looks column `State` (hash 0xE991BD51) up in the animations table (table 0, anim_*.gda; the key 0 is the table, `[0x00C1D090]`, `0x00A2CCC0`), returning -1 if missing. State 1 rows (62 in the tables ghilana reads): every `die`/`dieloop` clip (die, die_v2, die_v3, dieloop, dieLoop_v2/v3, the db_* death-blow families, db_c_all_pos_die_1..5, sleep, ambdyingonfloor, ambwoundedfloor) and the talent "Charge Reaction"/"Overwhelm Reaction" enter+loop clips. So: **while a creature lies in a death/knocked-down loop, its limits become `Pitch = max(Pitch, 60)`, `Roll = max(Roll, 60)` and Precision = 1 (two probes)**, so a corpse lies along the slope with up to 60 degrees of tilt (and a biped's 0 limit becomes 60). Corrects `ground-walk.md`'s "a size class" guess.
3. **The rate mode +0xB20 is the MovementController's state, mirrored by address, not copied.** The `MovementController` is embedded in the creature at **creature+0xA30** (`lea ecx,[esi+0xA30]` at 0x006A2A31, 0x00745EBC, 0x007A93CA, 0x00763818), and its state dword is at controller+0xF0 = **creature+0xB20**. There is no store to creature+0xB20 anywhere: a whole-.text byte scan for `20 0B 00 00` finds 23 hits, all reads, and one setter writes the state: **`MovementController::SetState` 0x00A63800** (`if new != [+0xF0]: [+0xF0]=new; [anim ctrl +0x13C]=new via 0x00A69C90; then per state`), plus two resets to 0 (the constructor 0x00A60B50 at 0x00A60BF6, a reset 0x00A632D0 at 0x00A633AF). States: **0 idle, 1 walk, 2 run, 3 stopping, 4 turning in place** (checked from writers; walk/run naming inferred from AUDIO.md:1281 where 2 raises "walk/run" and 1/4 lowers it). Rates (floats checked): **mode 2 -> 0.2, mode 1 -> 0.8, mode 0 with a root-motion clip -> 0.1 (0x00C021EC), else (0 without, 3, 4) -> 1.0** (0x006A7AF7-0x006A7B26; the rate multiplies the slew speed, so a running creature tilts at a fifth of the idle rate). Writers listed below.

###### Pseudo-code a port can copy

```
// probes (two-sample case, Precision == 1)
PS      = apr_base[row].PerSpace                    // CCCreature::PersonalSpace 0x006A21A0; row = word at [[creature+0x7B0]+0x58]
front   = (pos.xy + facing.xy * PS)  with z from pos   // GroundHeight(area, point) 0x00685430 each; if either misses (hit flag 0) the tilt is left as it was
rear    = (pos.xy - facing.xy * PS)
// limits
P, R, prec = row.TerrainFollowPitch, row.TerrainFollowRoll, row.TerrainFollowPrecision
if AnimState(currentAnimId(creature)) == 1:        // dying/dead/downed clip: anim table's State column
    P = max(P, 60); R = max(R, 60); prec = 1
// rate
switch movementState (creature+0xB20):
    2: rate = 0.2;  1: rate = 0.8
    0: rate = rootMotionClipPlaying ? 0.1 : 1.0     // 0x00695670: [[this+0x70]->vfunc 0x60]() has +0xC0 != 0
    default (3, 4): rate = 1.0
```

###### Movement state: every writer (0x00A63800's callers, checked by a scan of `e8/e9 rel32`, argument read at the call)

State 0 = idle, 1 = walk, 2 = run, 3 = stopping, 4 = turn in place (SetState's jump table: 0 -> 0x00A6383F also calls 0x00742D30 and zeroes [+0xE0]/[+0xE4]; 1 and 4 -> 0x00A63878 clears [+0xF6]; 2 -> 0x00A63883 sets [+0xF6] = 1; 3 -> 0x00A63865 zeroes [+0xE4]).

| Writer (call site) | Function | Sets | Meaning |
|---|---|---|---|
| 0x00A639CF / 0x00A639DC | `SetMoving(bool run)` 0x00A639C0 (ID 33603) | 2 if run, else 1 | start a walk / run |
| 0x007A8E7E | sub-action 16 (drive) 0x007A8C20 (ID 19880), after `0xA638B0` (desired facing) | 1 or 2 by its `run` byte | a driven move |
| 0x007A93D0 | the move update 0x007A9130 (ID 19882), used by the melee and range approach and walk-to | 1 or 2: run = (sub-action bool 1 set, or creature+0xC60's first dword nonzero) and creature byte +0xB25 == 0 and `0xA62E00(2.0)` true (the path is long: 5+ legs, or farther than 2.0 m; inferred from its head) | a creature walks to a goal and runs when far |
| 0x00A6398D | `SetDesiredFacing` 0x00A638B0 (ID 33602), its end | 4 | turn in place (when not already moving and no root-motion clip, `cmp [[ctrl+0x70]+0x10]+0xC0`) |
| 0x00A63AD7 | stop request 0x00A63AC0 (ID 33605): called from 0x006A944E (0x006A9430, ID 14630), 0x00746A1D (0x00746900, the shared command end), 0x007A5403 (TurnToward 0x007A51F0) | state 1 or 2 -> **3**; state 4 -> **0** | a command ends or a turn finishes: stop |
| 0x00763829 | 0x00763810 (ID 18211) | 3 if the state is not 0 or 3 | stop now |
| 0x00A643E6, 0x00A644BC | the controller step 0x00A64350 (ID 33615) | 3 | arrival (remaining distance under a limit) / path ended |
| 0x00A64593, 0x00A645FD | the controller step 0x00A644E0 (ID 33616) | 0 | the stopping state (3) finished, or nothing to follow |
| 0x00A63AFE | `Cancel/Reset` 0x00A63AF0 (ID 33606) | 0 | hard stop (cancels the animation controller too); callers 0x006917AA (0x006913E0, ID 13952, creature setup on load; also 0x0069158C sets 0 there), 0x00692A47 (0x00692980), 0x006B3636 (0x006B3620, a CCCreature virtual, base +0x70 slot), 0x006C74C9 (0x006C73C0), 0x007F25B6 (0x007F2460) and 0x007F2AA4 (0x007F2800, the end of a conversation or a staged scene), 0x00A63D28 |
| 0x006A2A37 | creature function 0x006A2A20 (ID 14464): `Movement()->0xA62F30()` then | 0 | movement cleared |
| 0x00745EC4 | the melee attack's set-up 0x00745E10 (ID 17720) | 0 | an attack stops the walk |
| 0x00746E52 | 0x00746E10 (ID 17733), after SetDesiredFacing | 0 | a facing command (turn, then idle) |
| 0x00A60BF6, 0x00A633AF | constructor 0x00A60B50; reset 0x00A632D0 | 0 | initial / reset |

Consumers that treat the value: stamps ("soft" for 1 and 2, dev/PATHFINDING.md:116), the walk/run ramp for footsteps (0x006AD8F0, AUDIO.md:1281), the follow rule (PARTY.md:229/:233: the leader standing is 0 or 4), the equip overlay when moving (ANIMATION.md:49), and the tilt's rate.

###### Evidence

- Tilt routine: 0x006A7520 reads the row via `0x004E1140(cache 0x00C0CB78, word [[creature+0x7B0]+0x58])` and +0xA8/+0xAC/+0xB0 into stack slots (0x006A7570-0x006A758E); then `0x006956B0` and `0x005CA710` (0x006A7592-0x006A759D); `cmp eax,1` -> `P = max(P, 60.0)` (0x00C021D8), `R = max(R, 60.0)`, prec slot = 1 (0x006A75A5-0x006A75DB; the fcom/test ah,5 and ah,0x41 branches read as max). Precision 0 -> one sample (normal, 0x006A7654-0x006A76BC); Precision 1 -> the `[vtbl+0x9C]` call at 0x006A76CC (`mov edx,[eax+0x9C]; call edx` on `ecx=esi`), repeated at 0x006A76FC and 0x006A771E, each followed by `0x008D8B00`, an addps with the position at creature+0x120, and `0x00685430` at 0x006A7786 and 0x006A77D1.
- Slot 39: the vtable read from the exe: CCCreature vtable 0x00B10484, entry 39 = 0x006A21A0, ID 14451 `CCCreature::PersonalSpace`; body `mov eax,[ecx+0x7B0]; test; fld [eax+0x60]; ret / fldz`.
- State column: 0x005CA710 pushes 0xE991BD51 and the id into `0x00A2CCC0([0x00C1D090], 0, id, hash, &out)`; the hash is "State" (tools/gda.py column_hash; ANIMATION.md:455 and :1140 name it). ghilana `table anim where State=1` returns the 62 rows (die, dieloop, db_*_die/dieloop, pos_die_1..5, sleep, ambdyingonfloor, ambwoundedfloor, the two talent reaction pairs); the rows carry no Label in the base files.
- Embedded controller: `lea ecx,[esi+0xA30]` before `0xA63800` (0x006A2A31, 0x00745EBC, 0x007A93CA), `lea ecx,[ecx+0xA30]` at 0x00763818 right after reading `[creature+0xB20]`; SetState compares and stores `[ctrl+0xF0]` (0x00A63807, 0x00A63817). 0xA30 + 0xF0 = 0xB20. A direct scan of stores to `[reg+0xF0]` in 0x00A60000-0x00A70000 shows only 0x00A60BF6, 0x00A633AF, 0x00A63817 (writes), the rest are other fields.
- Tilt rate: 0x006A7AED loads `[esi+0xB20]`; `cmp 2` -> `[0x00C021E4]` = 0.2; `cmp 1` -> `[0x00C021E8]` = 0.8; 0 -> `0x00695670` and, if true, `[0x00C021EC]` = 0.1; the default is `fld1` (1.0) (floats read from the exe).

###### Not read

- The maths between the probes and the pitch (0x004EE2xx helpers) is the other reader's.
- What the target pitch does while a State-1 clip plays beyond the new 60/60 limits (the same routine, so the normal-from-two-probes values clamp to 60), and whether a dead creature's dirty flag (+0xB40) is set often enough for the tilt to run (it needs +0xB40; the animation wrapper sets it when the State changes to or from 1: ANIMATION.md:472).
- The exact meaning of `0xA62E00(2.0)` and of creature bytes +0xB24/+0xB25 (read only at the call sites above); the walk/run names of states 1 and 2 rest on the run flag's route and the audio ramp, not on a label.
- The anim-table rows 2, 3, 250, 254, 268-283 (State 1 by ANIMATION.md:1386) beyond ghilana's read of the core files; the add-in files with enciphered archives (anim_str, anim_gib, ...) may add rows.
- No game was run; nothing observed live.

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
- What sets creature+0xB20 (tilt rate mode), the dirty flag's setters beyond 0x0069EE60, and the meaning of 0x005CA710's column 0xE991BD51.
- The no-controller path of 0x006B6880 (0x006B6B1D: a sweep 0x00685920 then SetPosition) and the global switch at 0x00C353B5.
- The walk grid's height channel (interpolation between cells, 0x00A473A0): not read, not needed.
- A live check: nothing was run in the game; "z set exactly each frame" is the code's behaviour, not an observation. Where the animation layer might offset the model relative to the creature's position (root offset, scale) was not read.

## What z a creature ends at after a run-time jump / SetPosition / teleport (jumpz; lib-agent-2, 2026-10-19, GOG exe, addresses GOG's, read-only)

"checked" = read in disassembly or data this session. "inferred" = from control flow or a constant. "cited" = master's notes, file:line. Scratch: `%TEMP%\lib-agent-2-jumpz-calls.py` (scans E8/E9 rel32 callers).

### Short answers

1. **Vanilla never trusts a script's z at run time.** Every run-time move of a creature (SetPosition, CommandJumpToObject/Location, the party arrival) ends in `GroundHeight` 0x00685430 for the final z, in the safe-location paths *and* in the unsafe SetPosition path (through the physics snap). A z of 0 or 1 where the terrain is at 2.3..15.8 m is repaired. (checked)
2. **GroundHeight does recover a z below the ground.** Ray 1 starts at z+3.0 and runs down, unlimited length (the length argument is -1.0 at 0x00685513, `0xAE2E00`; "unlimited" inferred, PATHFINDING.md:154 says 100 m, :333 FLT_MAX). If the terrain is more than 3 m above the input z, ray 1 misses (or hits the terrain from below if the mesh is two-sided: not read, same answer). It then **falls through to ray 2 from z+100.0 (qword 0xAED110 = 100.0) down, all hits, mask 0x24, takes the hit whose |hit.z - z| is smallest (start value 65505.0 at 0xB07ECC), flag 0x01000000 clear**, so a creature at z=0 under terrain at 15.8 gets 15.8, and under terrain at 2.3 it gets 2.3 from ray 1. If both rays find nothing, the input z comes back. (checked: 0x00685547-0x006855B5 ray 1, 0x006855B5-0x0068571B ray 2, 0x0068571D hit exit, 0x00685770 miss exit.) Only a point with no terrain/placeable/static shape in 100 m above it keeps its bad z.
3. **No kill plane, no "fell through the world" check, no ray from +1000 found.** No string of that kind in the exe (`--strings fell|falling|KillZ|void` empty); GroundHeight's start heights are z+3.0 and z+100.0 only; GetSafeLocation uses GroundHeight and the grid's cell height, nothing else. (strings checked; "no other recovery anywhere" not exhaustive, see Not read.)
4. **Most plausible cause of Ostagar's followers at z 0..1 in a port:** a location with z = 0 (or an unset local float: `Rubber_GetHome` builds `Location(area, Vector(fX,fY,fZ), f)` from `GetLocalFloat`s that read 0.0 when `Rubber_SetHome` was never called, `sys_rubberband_h.nss:55-59`; or any script `Location(area, Vector(x,y,0), f)`; or a waypoint authored at z 0) handed to a port that keeps the given z. Vanilla's jump / safe SetPosition / arrival replace that z with GroundHeight(x,y) and would recover them. Vanilla keeps a raw z only in the cases listed under "When the raw z survives" below.

### Per path

#### A. `SetPosition(oCreature, vPos, bSafePosition)` = ScriptCommand::SETPOSITION 0x006554B0 (checked)

Only creatures (`[obj+0x14] == 6`) take the safe path; any other object (placeable, trigger, sound, etc.) always takes the plain path (0x0065568F).

```
SETPOSITION(obj, pos, bSafe):
  if !bSafe or obj.type != creature:
      obj.SetPositionBase(pos, force=0)                    // 0x00695540 at 0x00655698, the plain path (see D)
      return 0
  mc = creature.controller                                  // 0x006A2570
  if creature is in the party but not the controlled one (0x0069F140 true, 0x006A7D30 false)
     and |pos - controlled.pos| < 0.01 [0xAF7498]:
        0x006AA440(creature, controlled, &pos)              // party formation spot, see LOAD_EVENTS.md:128-133
  // the safe search; arguments read at 0x0065560D-0x00655634
  if GetSafeLocation(area, pos, maxRadius = 5.0 [0xAE7308], mc, reachable = 1, &out,
                     snapToGround = 1, creatureCheck = 1, minRadius = 0) != 1:
        return 0                                            // NOT MOVED. No fallback to the raw pos (0x00655639 jne 0x0065569D)
  (clear the creature's command/animation state: 0x007A0DC0, 0x007C1690, 0x007C1EA0, 0x00A65CF0)
  creature.SetPositionBase(out, force=0)                    // 0x00695540 at 0x0065567F; out.z = GroundHeight (below)
```

GetSafeLocation 0x0073E370 (cited exactly in PATHFINDING.md:164-198, re-read for z only):

- With snapToGround the **first thing** is `p.z = GroundHeight(p)` (0x0073D520 -> 0x00685430), so the centre test and its ray are done at the ground, not at the script's z. (cited :170, :193)
- The **ring candidates** (half a cell step, up to maxRadius) take their z **from the walk grid's cell height** (`test(c): c.z = grid height at (c.x,c.y)`, 0x0073DE20 -> 0x00A473A0, cited :171-200), so a ring candidate does not depend on the bad input z at all.
- On success **`out.z = GroundHeight(c)` always**, even for snapToGround=0 (cited :193, read at 0x0073E75B). So `out` has a ground z.
- On failure it returns 0 and the creature stays where it was (SETPOSITION), or the caller keeps its spot (arrival).
- `IsLocationSafe` uses the grid's `IsFree` (kind only, no height), a physics ray 0x0067CFA0 and, for creatureCheck=1, a creature-overlap box test 0x0073D780 (cited :200-205). None of them looks at the input z except the ray, which runs at the ground z after the snap.

#### B. `CommandJumpToObject` / `CommandJumpToLocation` (queued commands; execution 0x007A3D?? / 0x007A4100) (checked)

The script commands 0x0066FEF0 / 0x00670010 only build a command (JUMP_TO_LOCATION = type 10, PARTY.md:325): a location is stored as the command's three vectors (position copied verbatim, `0x00670093-0x0067010E`). The raw script z is in the command until it runs. The executors (callers of 0x006AA440 at 0x007A3E1C and 0x007A426A; the location executor starts at 0x007A4100, ret 8):

```
JumpToLocation.execute(creature, cmd):                      // 0x007A4100
  pos, facing, area = cmd.location
  fail (-1) if no creature / dead / area null
  creature.vtable[0xB0](..)                                  // clears whatever it was doing
  creature.controller.ownerArea = area
  if party member but not the controlled one and |pos - controlled.pos| < 0.01: 0x006AA440 (formation spot)
  if GetSafeLocation(area, pos, maxRadius = 20.0 [0xAE72FC], mc, reachable = 1, &out, snap = 1,
                     creatureCheck = 1, 0) != 1:
        controller reset (0x00A62F30); return -1             // command FAILS, creature unmoved
  (same state clear as A)
  if creature.area == area:  creature.SetPositionBase(out, 0)           // 0x00695540 at 0x007A42FE
  else:                      LiftObstacle; creature.AddToArea(area, out, 0)   // vtable +0xF8 at 0x007A4337
  place obstacle (0x006B3070), set facing (0x00694CE0)
  return 1
JumpToObject.execute (function containing 0x007A3E1C...0x007A3F3B):
  pos = target object's position (+0x120 of the target, or a formation spot when the target is the controlled creature);
  same GetSafeLocation call but maxRadius = 5.0 [0xAE7308] (push at 0x007A3F29), reachable 1, snap 1, creatureCheck 1;
  fail -> -1 (0x007A3EEF);  ok -> SetPositionBase(out, 0) (0x007A3F80) when same area, else the cross-area branch.
```

So the jump's final z is **GroundHeight(candidate)** and, unlike the plain SetPosition, a jump with no safe spot within 20 m (5 m for JumpToObject) fails and leaves the creature where it was. (checked, the radii by their `fld` loads)

`UT_LocalJump` (utility_h.nss:1148-1245) uses `SetPosition(oTarg, GetPosition(oDest), TRUE)` when nJumpImmediately (the default), `WR_AddCommand(CommandJumpToObject(oDest))` otherwise; `bJumpParty` does it for every party member. So the common script "teleport to a waypoint" is **path A (5 m) or JumpToObject (5 m)**, z from the waypoint's GetPosition.

#### C. `SetLocation` / `SETLOCATION*` (0x0065B3B0 etc.)

These (SETLOCATION, SETLOCATIONPOSITION, SETLOCATIONFACING...) are the setters of a `location` value (the script type), not a move of any object. Nothing in them calls GroundHeight or touches a creature; a location carries z exactly as the script put it, and the consumers above decide. (inferred from the names and from that none of them is in the callers of 0x00695540 or 0x006B1790, whose caller lists I scanned: 0x00695540 is called from 0x006327EC, 0x0065567F, 0x00655698, 0x0065B47E ... and 0x0065B47E is in the GETSAFELOCATION command, not SETLOCATION; the SETLOCATION bodies were not disassembled.)

#### D. The plain move: `CCObject::SetPosition` 0x00695540 (pos, force) -> vtable +0x7C (creature: 0x006B5EF0) (checked)

```
CCObject::SetPositionBase(pos, force):                      // 0x00695540
  if !force and pos == this.pos (0x004D4EF0): return
  old = this.pos; this.pos(+0x120) = pos         // the SCRIPT'S z is stored here, raw
  this.groundCacheValid(+0x1C4) = 0               // the cached ground height/normal is stale
  this.vtable[+0x7C](&old, 0)                     // CCCreature::SetPosition 0x006B5EF0

CCCreature::SetPosition(old, force):                         // 0x006B5EF0 (cited PATHFINDING.md:153, :241)
  if has capsule (+0x770) and !disabled (+0x420) and !inSnap (+0x774):
      capsulePos = capsule.GetPosition(); capsulePos.z -= +0x784 (centre offset)
      if distance(this.pos, capsulePos) > 0.05 [0xAF70B0]  or force:        // 0x006B5F51
          if game mode != Loading (9):  Snap(arg 0)                         // 0x006B1790
          else:                         capsule.SetPosition(x, y, this.z + +0x784)   // raw z!  0x006B5F87
  base set: orientation/room/obstacle bookkeeping (0x00697AE0, 0x00695D70, stamp 0x006A3BC0)

Snap(0)  // 0x006B1790
  g = GroundHeight via the cache: 0x006954C0 recomputes (+0x1C4 was cleared) -> +0x1C0 = GroundHeight(area, this.pos, 0, &normal)
  p = (this.x, this.y, g)                                    // 0x006B17C8-0x006B17D7: local z = +0x1C0
  if Overlap(p) (0x006B1600: physics query [area+0x62C]->vtable+0x34, mask 0x2011134, the controller's capsule at p, COLLISION.md section 5):
        found = GetSafeLocation(p, 10.0 [0xB0F298], mc, reachable=0, &p, snap=1, creatureCheck=0, 0)    // moving creature: NearestFree 0x0073E7F0
        if found: this.SetPositionBase(p, 0) with +0x774 set (recursion guard)
  capsule.SetPosition(p.x, p.y, p.z + +0x784)                // 0x006B1979-0x006B1997: ALWAYS, with the ground z
```

So after the plain SetPosition the physics capsule stands at **GroundHeight(x,y)**, whatever z the script gave, when the game is not in mode 9 and the capsule is more than 0.05 m from the new spot; the overlap test (0x006B1600) only decides whether a safe-spot search also runs, it does **not** gate the z snap (corrects my wording in PATHFINDING.md:156, which says the snap "is gated by the physics test": the *search* is gated, the final capsule placement at ground z is not, 0x006B1979 is reached from both branches). The creature object's own `+0x120` z keeps the script's raw z until the per-frame read-back: `0x006B2220` (from 0x006B6880 at 0x006B6B10) sets `this.z = capsuleCentreZ - +0x784` via 0x00695540 (checked 0x006B2220-0x006B230A), after which `this.z` is the ground's. If the capsule's flag at vtable +0x38 is false it instead keeps x,y from the capsule and its own z and calls Snap(0) again (0x006B22B3-0x006B22DF).

#### E. Area transition arrival (cited)

PATHFINDING.md:160-162 and LOAD_EVENTS.md:100-130: 0x006C73C0 -> `GetSafeLocation(spot, 20.0, mc, reachable=1, out, snap=1, creatureCheck=1, 0)` at 0x006C7664; success: z = GroundHeight; **failure: the spot is kept with its raw z**, then `AddToArea(area, spot, 0)` in Loading mode, where `SetPosition` does not snap (path D, mode 9). The spot is the waypoint's position plus a formation offset in x,y (z is the waypoint's).

### When the raw z survives (vanilla)

1. **Game mode 9 (Loading), plain move or arrival with no safe spot:** `0x006B5F76` skips the snap and the capsule is put at raw z + offset. A physics body under the terrain with a -9.81 downward move per frame (0x006B6880) finds no floor to land on (it only sweeps the shapes below it) and keeps falling; there is no catch (answer 3). Not seen; inferred.
2. **A creature with no capsule** (`+0x770 == 0`, not yet added): `0x006B5F06 je 0x006B5FB9` skips everything; `+0x120` stays raw until AddToArea creates the capsule. Placed-in-the-.are creatures are in this class and keep the .are's z by design (PATHFINDING.md:227-237).
3. **Plain SetPosition by less than 0.05 m** (the distance test): nothing changes, whatever the z.
4. **No collision under/over the point in 100 m:** GroundHeight returns the input z.
5. **Non-creature objects** (placeables etc.) via SetPosition: only `0x00695540` -> their own vtable +0x7C (0x0074BB70, no ground call, PATHFINDING.md:233): raw z kept.

### What to copy

```
// script SetPosition(obj, pos, safe)
if obj is creature and safe:
    out = GetSafeLocation(pos, 5.0, reachable, snapToGround, creatureCheck)   // z of out = GroundHeight(out.xy, from z+3 else z+100)
    if !out: return                          // the creature stays; do NOT place at the raw pos
    place(creature, out)
else:
    place(obj, pos)                          // creature: capsule z = GroundHeight(pos.xy) when mode != Loading and moved > 0.05 m
// CommandJumpTo*: same GetSafeLocation (20.0 for a location, 5.0 for an object), fail = command failed, creature unmoved
// GroundHeight(x, y, z): hit = ray(x,y,z+3) down, unlimited; if none: hits = all(ray(x,y,z+100) down), choose min |hit.z - z|; none: z
```

For the port's bug: take z = GroundHeight(x, y, inputZ) with that two-ray rule in whatever replaces SetPosition / the jump / the arrival, and treat a z of exactly 0 (an unset local float) as possible input; do not use the walk grid's cell height for the final z (its only use is a ring candidate's seed inside GetSafeLocation, which the exe then re-snaps).

### Not read

- The two-sidedness of PhysX triangle-mesh raycasts (whether ray 1 from below the terrain hits the terrain's underside): it does not change the answer.
- Ray 2's exact direction/length arguments (call at 0x006855F7 `[vt+0x20](list, start, dir, mask 0x24, 0, 0)`), taken from the shape of the loop; ray 1's length -1.0 meaning unlimited (inferred).
- The bodies of SETLOCATION*/SETLOCATIONAREA (0x0065B3B0, 0x0065B580, 0x00651000): assumed to edit a location value only.
- 0x006AA440's formation spot z, and which command class holds the JumpToObject executor (its entry is before 0x007A3DC0; the callers' tail was read, not its head and the cross-area branch at 0x007A3FB0-0x007A4100).
- Whether any script or the AI keeps a "fell off the world" watchdog in the .nss files for Ostagar (not grepped beyond SetPosition/CommandJumpTo*/Location(...Vector(...))); no engine-side one was found by strings.
- Whether Ostagar's followers were put at z 0..1 by Loading-mode placement (case 1 above) rather than by a script jump: nothing in the scripts for ost000a was found that builds a z=0 location; the Rubber_* home locals are the only z=0 builder found, and no Ostagar script uses them.
- The per-frame call conditions of 0x006B6880 for an idle creature (callers 0x006BBA39, 0x006BBC93 only checked as call sites).

## A creature that starts under the level's mesh: what the capsule controller and the snap do (read 2026-10-19, GOG exe, read-only; prefix ctlunder)

"checked" = read in disassembly. "inferred" = from control flow. Builds on master's dev/PATHFINDING.md walk section (the pseudo-code of 0x006B6880 / 0x006B2220, lines ~285-330) and :150-165 (the snap 0x006B1790, GroundHeight); only what is new is marked NEW.

### Short answers

1. **No depenetration, no ground search when the capsule is built or placed.** Creation (`CCCreature::CreatePhysicsController` 0x006B1ED0, tail 0x006B2156-0x006B21FD) and the physics-body setter (0x006B5EF0) do exactly one of two things (checked):
   - game mode is Loading (`[0xC327EC]+0x114 -> +0x78 == 9`): `controller.setPosition(x, y, z + creature+0x784)` (vtable slot +0x1C), the .are's z verbatim. Nothing else. A creature put at z 0..1 under a surface at 2.3 is built there.
   - any other mode: the snap `0x006B1790(1)`, which replaces z by `GroundHeight` and (flag 1 = skip the overlap test) always runs the safe-location search (below). NEW: at creation the flag is 1 (push 1 at 0x006B21BE), at SetPosition after a spawn it is 0.
   PhysX 2.8's NxController has no recovery of its own that the exe uses; there is no overlap-and-push-out call anywhere in these routines (checked: the only overlap query is 0x006B1600, used by the snap).
2. **The first move from under the terrain is recovered by the SNAP, not by the controller (NEW, checked flow).** Per frame (only while the creature is moving, see 3) 0x006B6880 moves the capsule by (dx, dy, -9.81), mask 0x2011134. Then 0x006B2220 reads it back:
   - controller reports "colliding down" (slot +0x38): creature z = capsule centre z - 0x784. Exact.
   - NOT colliding down (nothing within 9.81 m below, which is the under-the-terrain case if nothing lies lower): creature z is KEPT (x, y from the controller, z = old `creature+0x128`, 0x006B22B3-0x006B22D6) and then **`0x006B1790(0)`** runs. It sets z = `GroundHeight(creature x, y, z)` (0x006954C0 cache, invalidated by the SetPosition just made) and tests `0x006B1600` = `PhysicsScene::OverlapCapsule` (scene vtable 0x00B272D4 slot +0x34 = 0x0082EC20, address library ID 22526; checked) of the capsule centred at (x, y, groundZ + co) with mask 0x2011134:
     - no overlap: `controller.setPosition(x, y, groundZ + co)` (0x006B1979-0x006B1997). The creature itself is updated by the next frame's sync (which lands on the floor: colliding down, z exact).
     - overlap: `NearestFree` 0x0073E7F0 if the movement controller is active (0x00A630E0), else `GetSafeLocation` 0x0073E370 (10.0 m, snap to ground, reachable 0), the creature MOVED there with SetPosition (flag +0x774 = 1 around it), capsule size re-applied, then controller set to that spot's ground. If the search finds nothing the creature stays.
   So the engine's answer to "creature under the mesh": the next move frame lifts it to **GroundHeight(x, y, z)**, i.e. a surface found from z+3.0 downward (any Walkable-mask hit below z+3 in the column, nearest to the ray's start going down) and, if that ray hits nothing (every surface is more than 3 m above), the second ray's hit nearest in z to the input z (PATHFINDING.md:285 / GroundHeight pseudo-code). For z 0..1 and surfaces at 2.3 and 15.8: ray 1 starts at z+3 = 3..4, above the 2.3 surface, so it hits 2.3 first; the 15.8 surface is never chosen unless 2.3 is absent in that column.
3. **The controller moves only while the movement controller is active, not for an idle creature (NEW, checked).** Both calls of 0x006B6880 are inside the creature's per-frame update (0x006BB890, vtable slot; the second, 0x006BBC93, in the adjacent virtual 0x006BBB70). In 0x006BB890 the call at 0x006BBA39 sits behind `0x00A630E0(MovementController at creature+0xA30)` (0x006BB911: false jumps to 0x006BBB55, past the move) and then `0xA644E0` (the controller's update) true and `0x00695670` false. 0x00A630E0 = `[ctl+0x75] ? ([ctl+0x76] | (anim ctl at +0x70 && [+0x74] ? [anim+0x70] : 0)) : 0` (read 2026-10-22 by lib-agent-2 with a subagent):
   - **The objects** [C]: `ctl` is the `MovementController` (vtable 0x00B596C0, ctor 0x00A632D0) at creature+0xA30. `ctl+0x70` is a `MovementAnimController` (vtable 0x00B59AC0, ctor 0x00A69EC0), made lazily at 0x00A63A1A..0x00A63A4F.
   - **+0x76 is "the movement state is not 0"** [C]: `setne` of `ctl+0xF0` (creature+0xB20: 0 idle, 1 walk, 2 run, 3 stopping, 4 turning in place). Only the ctor (0x00A63326) and SetState 0x00A63800 (0x00A63833) write it. PATHFINDING.md's "Movement state: every writer" lists SetState's callers.
   - **+0x75 is an enable flag** [C]: the ctor sets it to 1 (0x00A63322). Binding the creature to a conversation or staged scene (0x007F2460, at 0x007F25BD) clears it, and the release (0x007F2800, at 0x007F2A9D) sets it again. No other store was found.
   - **+0x74 is only ever written 0** (0x00A6331F, 0x00A630D9, creature-side 0x006AA8E2) [C for the stores found; I that no store of 1 escaped the scan]. So the `[anim+0x70]` term (set at 0x00A69CAA from SetState, cleared at 0x00A69E1F when the movement clip's weight is ≤ 1e-6) does not count in practice.
   - **So 0x00A630E0 is true while the creature is not held by a scene and its movement state is not 0** [C]. That covers walking, running, stopping (state 3, until 0x00A644E0 sets 0) and turning in place. It is false when idle, when bound to a conversation or scene, and after a hard stop (0x00A63AF0) [I: moved by animation or a script with no state set].
   - So an idle creature gets no gravity move: it does not fall, and does not rise either. It stays under the mesh until something moves it. NOT read: the callers of 0x006BBB70 (it is a vtable entry; it takes dt, a target and an out vector, and calls 0x006B6880 with the target), so a script- or knockback-driven move may also pass.
4. **No kill plane, lowest-z clamp or stuck detector in the path (checked absence in 0x006B6880, 0x006B2220, 0x006B1790, 0x006B1600).** No string "fell"/"stuck"/"falling" in the exe. `0xA62E00(2.0)` (the stuck-looking call in PATHFINDING.md:528) is the walk-or-run predicate (path of 5+ legs, or longer than 2.0 m), not a stuck check (checked head). If the capsule falls (colliding down never true because nothing is below), the engine keeps z and snaps to GroundHeight each frame instead of letting z drop; z does not decrease frame by frame. If the column has NO Walkable surface at all, GroundHeight returns the input z and the creature stays where it is.
5. **One-sided meshes: not decided (PhysX is in `nxcharacter.dll`/`PhysXLoader`, a separate DLL; not read).** The exe imports `NxCreateControllerManager`/`NxReleaseControllerManager` (NxCharacter.dll), `NxCookTriangleMesh`/`NxInitCooking` (NxCooking.dll), PhysXLoader (3 functions). The exe cooks the level's meshes from the .opf data (NxTriangleMeshDesc, 40 bytes, "the same in every one seen", COLLISION.md:171); the flags of the desc were not decoded here (NX_MF_FLIPNORMALS / double-sided are not known). What the exe does show: it handles a controller hit UP (slot +0x40, 0x006B6A98-0x006B6B0C): restore the saved position, step offset 0, move again, step offset 0.5. That is for a head hit on a ceiling during a step-up; it does not repair a capsule under a surface.

### Pseudo-code (what a port should do for a creature that may be under the surface)

```
// each frame the creature is moving (movement controller active)
move capsule by (dx, dy, -9.81), mask 0x2011134, step 0.5
if colliding down:   creature.z = capsule.centre.z - co                     // as PATHFINDING.md:300
else:                creature.xy = capsule.xy; creature.z unchanged
                     g = GroundHeight(creature.xy, creature.z)               // ray from z+3 down, else nearest-in-z of all hits from z+100
                     if !OverlapCapsule(xy, g + co, mask 0x2011134):
                         capsule.setPosition(xy, g + co)                     // creature is lifted on the next frame's sync
                     else:
                         p = movingNow ? NearestFree(...) : GetSafeLocation(creature.pos, 10.0 m, snapToGround, reachable 0)
                         if p: creature.SetPosition(p)                       // then capsule to p's ground
// spawn in a non-Loading mode: z = GroundHeight(x, y, z) in the factory (0x006861F0), then the same snap with flag 1 (safe-location search always)
// spawn/load in Loading mode: z verbatim, no snap, no depenetration
```

For the Ostagar case: a port that wants the engine's result puts a creature at z 0..1 under surfaces at 2.3 and 15.8 onto **2.3** (the nearest surface above whose ray starts at z+3), by the GroundHeight ray (mask Walkable 0x2000000, or 0x24 with the Nonwalkable flag test where area+0x658->+0x7C == 1), not onto the 15.8 surface and not by falling. The engine does this at a spawn (non-Loading) at once and for a moving creature on its first move frame; an idle one it leaves wherever it is (the .are's z in Loading mode).

### Evidence (GOG addresses)

- 0x006B6880: gate `[esi+0x770]` null or `[0xC353B5]` set -> no-controller path 0x006B6B1D; `[ctl+0x18](1)` false (collision off) -> `SetPosition(target, 0)` 0x006B68BD. Displacement (target.xy - pos.xy, 0) at 0x006B68CA-0x006B68DE, then z = -9.81 (0x00B0F274) at 0x006B6A3B-0x006B6A47; mask 0x2011134 at 0x006B6A4B and 0x006B6A84 (move = slot +0x10, min dist 0.001 = 0x00B0F278, sharpness 1.0); `[ctl+0x40]` (up) at 0x006B6A98 -> restore via slot +0x1C, step offset 0 (slot +0x24), second move 0x006B6AF5, step offset 0.5 (0x00B0F28C); then 0x006B2220.
- Slope branch uses `0x008E6020(pi/3 = 1.0471976)` + 1e-6 (0x00AE09B0) against the ground normal z from 0x006954C0 (cos 60 deg; the call is read, "cos" inferred).
- 0x006B2220: skipped if `[esi+0x420] != 0` (unnamed flag) or no controller; centre z - `[+0x784]`; `(+0x780 - ctl.height)*0.5` when `[+0x780] != -1`; `ctl slot +0x38` (colliding down) false -> 0x006B22B3 branch (SetPosition(x, y, creature+0x128), then 0x006B1790(0) at 0x006B22DF).
- 0x006B1790(flag): reads creature +0x120/+0x124/+0x128, 0x006954C0, z = `[esi+0x1C0]` (0x006B17C8-0x006B17D7); flag 0 -> 0x006B1600 (call 0x006B17E4; false -> 0x006B1979); area `[vtable+0xA0]`; moving test 0x00A630E0 at 0x006B1838-0x006B1840 -> `NearestFree` 0x0073E7F0 (call 0x006B18A9, ecx = area) else `GetSafeLocation` 0x0073E370 with radius 10.0 (0x00B0F298) (call 0x006B18DE); success -> `SetPosition(out, 0)` 0x006B18FD with `[esi+0x774]` = 1 around it; sizes re-read from the controller 0x006B1902-0x006B1977; final `controller.setPosition(x, y, [esp+0x18] + [esi+0x784])` 0x006B1979-0x006B1997.
- 0x006B1600: area `[+0x62C]` physics world (0x00537D60(0)), `ctl slot +0x64` (radius) and `+0x6C` (height) read, centre = given pos (or the controller's position) with z + `[+0x784]` (0x006B16D4-0x006B16EA), height * 0.5 (0x00AE70F8) added/subtracted, mask 0x2011134 at 0x006B1725, `scene vtable +0x34` = 0x0082EC20 (`Physics::PhysicsScene::OverlapCapsule`, ID 22526) at 0x006B174D. Its result is the bool the snap tests; true = overlap (checked: `OverlapCapsule` returns its count > 0, COLLISION.md section 5).
- Creation 0x006B1ED0: desc up axis 2, slope/skin/step values at 0x006B20DC-0x006B212A (1e-6 at 0x00B0F2A4 added to the slope), `scene vtable +0xC` (CreateController, 0x00821B40) at 0x006B214A-0x006B2156 -> `creature+0x770`; then controller settings (slots +0x4C/+0x50/+0x54/+0x58/+0x5C: collision on, user data = creature, groups 4, 4, mask 0x10); then `[0xC327EC]+0x114 -> +0x78 == 9` ? direct setPosition(x, y, z + 0x784) at 0x006B21C9-0x006B21FD : `0x006B1790(1)` at 0x006B21C2. Callers of 0x006B1ED0 (scanned E8 calls): 0x0066C06D, 0x006B36BB, 0x006B5ECD (from 0x006B5E90, the physics on/off toggle: creates it when switched on), 0x006B6DC4, 0x006B754C, 0x006BC034, 0x007F2B00. Callers of 0x006B1790: 0x006B21C2, 0x006B22DF, 0x006B5F80 (SetPosition 0x006B5EF0, not Loading), 0x006BA882.
- Move gate: 0x006BB890 (`push ebp` at 0x006BB890; vtable data at file 0x710594, next slot 0x006BB BB70 at file 0x7105A0); 0x006BB911 `call 0xA630E0` / `je 0x6BBB55`; call 0x006BBA39.
- Cache: 0x006954C0 computes GroundHeight only when byte +0x1C4 is 0 and sets it; 0x00695540 (SetPosition) clears it (PATHFINDING.md:266).
- Imports (`daoexe.py --imports`): NxCharacter.dll {NxCreateControllerManager, NxReleaseControllerManager}; NxCooking.dll {NxCookClothMesh, NxCookConvexMesh, NxCookTriangleMesh, NxInitCooking, NxCloseCooking}; PhysXLoader.dll (3).
- A scratch scanner for E8/E9 callers: %TEMP%\lib-agent-2-ctlunder-calls.py.

### Prior notes used

PATHFINDING.md:150-165 (placed creatures keep the .are's z in Loading mode; CreateObject's GroundHeight; the snap and its overlap gate), :231-262 (ground z at load, the lazy cache), :285-330 (the controller's per-frame move and sync), :528 (0xA62E00), COLLISION.md:62-85 (mask bits), :171 (triangle mesh records). Sibling repos: nothing on a creature under the mesh (DAOTurnBased/Eluvian have no notes on it; not re-searched beyond the library's table: the physics rows 22675-22706 and 22526 exist, 0x006B1600/0x006B1790/0x006B6880 are unnamed).

### Not read

- PhysX 2.8's own behaviour (nxcharacter.dll / PhysXLoader): whether a controller that starts penetrating a mesh recovers, whether triangle meshes are one- or two-sided for the controller's sweeps and for scene raycasts, and whether the move reports "colliding down" when it lands on a mesh's back face. Without it the case "capsule under a surface with another shape lower down" is open: the capsule may land on that shape (colliding down, z exact there) and the snap would not run.
- The cooked meshes' NxTriangleMeshDesc flags (FLIPNORMALS, hardware); the exe's cooking call sites were not read.
- The callers of the vtable entry 0x006BBB70 (the second route into 0x006B6880), and the byte +0x420 of the creature (unnamed).
- GetSafeLocation's candidate order past PATHFINDING.md's reading; the tail of 0x006B1790 when the search succeeds (which z the controller gets: read as the new spot's ground, 0x006B1979 uses `[esp+0x18]`, not checked to be overwritten by the search).
- Nothing was run in the game; the Ostagar z of 2.3 / 15.8 is the paragon-agent's figure, not re-measured.
