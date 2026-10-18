# Script commands, group "locations": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not.

Shared facts (checked):
- **The structure.** `CScriptLocation`, 0x30 bytes (`dao/Structures.h`, master's `dev/LIBRARY.md`): +0x00 position (3 floats, +0x0C pad), +0x10 orientation (3 floats, +0x1C pad), +0x20 area id, +0x24..+0x2F pad. The constructor 0x007ABD00 zeroes both vectors and sets the area to 0xFFFFFFFF. The orientation is a **direction vector, not a quaternion**: nothing here reads or writes a fourth component (the pads get whatever 16-byte copy carries them, unread).
- **Facing ↔ orientation.** `Vector::FromAngle` (0x004EAE30) gives (sin(−a·k), cos(−a·k), 0) with k = 0.017453279 (double at 0x00AF14C0, a slightly imprecise π/180); every caller here then multiplies y by −1.0 (0x00AF0E48). So **facing a° → orientation (−sin a, −cos a, 0)**: 0° is −Y, 90° is −X. The way back (`GetFacingFromLocation`): normalize (0x00451A30), negate y, `Vector::ToAngle` (0x004EE570) = atan2(−x, y) × 57.295826 (0x00AF0E50), so facing = atan2(−ox, −oy) in degrees, range (−180, 180]. The two are inverses (a 359° facing reads back as −1°).
- **Normalize** 0x00451A30(out, in): if |v|² ≤ 1e-6 (0x00AE7094) it gives a static (1, 0, 0) (0x00C1E870, set once); otherwise v/|v|.
- **Pop and push.** A location pops with `StackPopEngineStructure(1)` (0x00A2F650), which hands the handler a deep copy (`CopyGameDefinedStructure` 0x00674AF0 → copy constructor 0x0064DD90); the handler frees it. A setter edits **its copy** and pushes it back (`StackPushEngineStructure` 0x00A2D920 copies again), so `SetLocation*` return a new location and never change the argument variable. Failed pops return −639, failed pushes −638.
- **A null location.** A null cell copies to a null pointer (0x00674B6B). `RSADD` makes a default location, but an array element past the end reads as null (master's `dev/NCS.md`). Only `IsLocationValid` checks for null; every other handler dereferences it without a check, so a null location faults there (no check: checked; the fault: inferred).

### Location (454)
- **Handler:** 0x0065ACE0 (not shared). It calls `CScriptLocation::CScriptLocation` (0x007ABD00) and `Vector::FromAngle` (0x004EAE30). (checked)
- **Does:** pops oArea, vPosition, fAngle, in that order. It builds a new location: area id = oArea raw, position = vPosition, orientation = (−sin a, −cos a, 0) from fAngle in degrees (shared facts). Pushes it, then frees its own. (checked)
- **State:** none; nothing is looked up. (checked)
- **Edge cases:** **oArea is not checked**: any id, OBJECT_INVALID or a creature's, is stored as given (`IsLocationValid` is where it is judged). Any angle works (sin/cos wrap it). (checked)

### IsLocationValid (455)
- **Handler:** 0x0065AE20 (not shared). It calls `CClientExoApp::GetAreaById` (0x005D5960) and `Vector::Magnitude` (0x00451990). (checked)
- **Does:** returns FALSE when the area id is not an area **or** when the position is exactly (0,0,0) **and** the orientation's magnitude is exactly 0.0 (0x00B190A4). Otherwise TRUE. (checked)
- **Edge cases:** a null location → FALSE (checked). The compares are exact float equality. script.ldf says invalid means "an invalid area object, an empty position vector and an empty orientation vector". The engine treats the area alone as enough: a location at (5,5,0) with no area is invalid. A location in a valid area at the origin with a zero orientation is invalid too, but one with any non-zero orientation is valid (checked). An area that is unloaded but still has its id → depends on `GetAreaById` (inferred: null, so FALSE).

### SetLocation (458)
- **Handler:** 0x0065B3B0 (not shared). It calls `CClientExoApp::GetObjectById` (0x005D5BE0), `GetAreaById` (0x005D5960), the object's vtable slot +0xF8 (`CCObject::SetLocation` 0x00696F90; `CCCreature`'s 0x006B6BC0), `CCObject::SetPosition` (0x00695540) and `CCObject::SetOrientation` (0x00694CE0). (checked)
- **Does:** pops oObject, then the location. If the object's area (+0x254) differs from the location's area and that area exists, it calls slot +0xF8(area, &position, 0): out of the old area (slot +0xFC), `SetPosition(pos, force 1)`, +0x254 = the area, into the new area (0x0068BF90). Then, always, `SetPosition(pos, 0)` (+0x120; skipped if equal, else +0x1C4 = 0 and slot +0x7C "moved" with the old position) and `SetOrientation` (+0x130 = the raw vector, then slot +0x80). (checked)
- **Not a jump.** No command is queued and none is cleared; it happens at once, inside the script. There is no `GetSafeLocation` snap, no walkability test and no pathfinding: the object is placed exactly at the vector (checked for the handler; slots +0x7C/+0x80 read only in outline). For a creature, slot +0x7C (0x006B5EF0) moves its physics body to the new spot (inferred). script.ldf's own warning says the same ("inherently unsafe"; use `JumpToLocation`).
- **Creature across areas** (0x006B6BC0, checked in outline): on its first entry to an area (+0xC54 = 0) it queues EVENT_TYPE_SPAWN (3) to itself 10 ms later (`AIMaster::AddEventDeltaTime` 0x0064ACE0) when 0x00698490(3) answers 1, sets +0xC54 = 1, and moves each object in its list +0xB9C (ids, through `GetGameObjectById`) along with it (inferred: its attached or summoned objects).
- **Edge cases:** an invalid object → nothing (checked). Any object type is accepted (`GetObjectById`). **A location whose area is not an area** (−1, a creature's id) does not change the area, but the position and orientation are still applied in the object's current area (checked). The orientation is not normalized here (checked). Returns 0; void.

### SetLocationPosition (460)
- **Handler:** 0x0065B580 (not shared). (checked)
- **Does:** pops the location, then vPosition; writes the 16 bytes at +0x00 (x, y, z and the pad); pushes the copy. (checked)
- **Edge cases:** no check on the vector. A null location faults on the write (inferred). script.ldf's "invalid location on failure" never happens: it always pushes the edited copy. (checked)

### GetAreaFromLocation (461)
- **Handler:** 0x00650F90 (not shared). (checked)
- **Does:** pops the location and pushes its +0x20 as an object id, unchecked. (checked)
- **Edge cases:** whatever `Location`/`SetLocationArea` stored comes back as is: OBJECT_INVALID for a default location, or a non-area id. An area since unloaded still gives its id. A null location faults (inferred). (checked)

### SetLocationArea (462)
- **Handler:** 0x00651000 (not shared). (checked)
- **Does:** pops the location, then oArea; writes oArea's raw id at +0x20; pushes the copy. (checked)
- **Edge cases:** no type check: any id, OBJECT_INVALID included, is stored. Never pushes an "invalid location" on failure, contrary to script.ldf. A null location faults (inferred). (checked)

### GetFacingFromLocation (463)
- **Handler:** 0x0065B630 (not shared). It calls the normalize 0x00451A30 and `Vector::ToAngle` (0x004EE570). (checked)
- **Does:** reads the orientation (+0x10), normalizes it, negates y and pushes atan2(−x, y) in degrees: facing = atan2(−ox, −oy)·180/π, in (−180, 180]. z is ignored. (checked)
- **Edge cases:** a zero (or |v|² ≤ 1e-6) orientation, as in a default location, becomes (1,0,0), giving **−90.0** (the vector: checked; the number: computed). Not an "absolute" 0..360 angle as script.ldf's wording may suggest: negative facings come back. A null location faults (inferred).

### SetLocationFacing (658)
- **Handler:** 0x0065B6F0 (not shared). It calls `Vector::FromAngle` (0x004EAE30). (checked)
- **Does:** pops the location, then fAngle (degrees); sets the orientation to (−sin a, −cos a, 0), as `Location` does; pushes the copy. (checked)
- **Edge cases:** any angle. A previous orientation's z is lost (set to 0). A null location faults (inferred). (checked)

### GetOrientationFromLocation (464)
- **Handler:** 0x0065B7B0 (not shared). (checked)
- **Does:** pushes the orientation (+0x10, +0x14, +0x18) as a vector, raw: not normalized. (checked)
- **Edge cases:** a default location gives (0,0,0). A location from `GetLocation` gives the object's +0x130 as it is. A null location faults (inferred). (checked)

### SetLocationOrientation (465)
- **Handler:** 0x0065B860 (not shared). It calls the normalize 0x00451A30. (checked)
- **Does:** pops the location, then vOrientation; stores **the normalized** vector at +0x10 (z kept); pushes the copy. (checked)
- **Edge cases:** a zero or tiny vector (|v|² ≤ 1e-6) is stored as (1,0,0), not as zero, so `IsLocationValid` will not see it as empty and `GetFacingFromLocation` gives −90. A null location faults (inferred). (checked)

## Not read
- `CCObject::SetLocation`'s area add (0x0068BF90), 0x00687980 and slots +0xFC, +0x104 and +0x154, and the creature override's tail (0x006A2A40, 0x006B3070, and the player's room lookup 0x0067B3A0/0x0067F5C0): only in outline.
- What 0x00698490(3) asks before the SPAWN event, and what the list at +0xB9C holds.
- `CCObject::SetOrientation`'s slot +0x80 (0x006955C0 for objects, 0x006A0190 for creatures), so whether a creature's model turns at once.
- `GetLocation` (457), `GetPositionFromLocation` (459), `IsLocationSafe`, `GetSafeLocation`: not in this group's list.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Shared facts as in that file: a location is 0x30 bytes (position +0x00, orientation +0x10, area id +0x20), popped as a deep copy (0x00A2F650) and freed after; failed pops return -639, failed pushes -638.

### IsLocationSafe (456)
- **Handler:** 0x0065AF40 (own handler); `CCArea::IsLocationSafe` 0x0073DE70 (checked).
- **Described in:** `dev/LOAD_EVENTS.md` / `dev/TODO.md` name the area-level checks only; the command is not described elsewhere.
- **Does:** takes the area by the location's area id (`GetAreaById` 0x005D5960), and asks it whether the location's position is safe (walkable and free; tested with a radius from the float at 0x00AE7104 and the area's grid from `app+0x118`, inferred), pushing 1 or 0 (checked).
- **Edge cases:** if the area id matches no area (including the default -1 of an unset location) it pushes 0 without failing (checked). Nothing else on the location is validated. A null location pops fine but then faults on dereference (as in the group notes).

### GetSafeLocation (659)
- **Handler:** 0x0065B0D0 (own handler); `CCArea::GetSafeLocation` 0x0073E370 (checked).
- **Described in:** `dev/LOAD_EVENTS.md` (walks square rings outward by half a grid cell; the party's spot snapped within 20.0 m) and `dev/PARTY.md`.
- **Does:** searches the location's area for a safe spot near the location's position with a radius of 20.0 (0x00AE72FC) and a second float of 0, reachable, on the ground (arguments 1, 1, 0 pushed), writes the found position into the copy and pushes the copy (checked).
- **Edge cases:** if no spot is found (the call returns 0) the position keeps its input values, since the copy is only overwritten on success: the location comes back unchanged (checked). If the area id matches no area the search is skipped and the copy comes back unchanged too (checked). Orientation and area id are never changed (checked: only the position is written by the search's out argument; the copy path above 0x0065B205 is a 16-byte restore of the position block).

### GetLocation (457)
- **Handler:** 0x0065B2A0 (own handler) (checked).
- **Described in:** `dev/LIBRARY.md` and `dev/PROBE.md` (the probe shadows it and finds the answer agrees with the library's own read); the object-to-location copy is not described elsewhere.
- **Does:** pops an object id, allocates a location (constructor 0x007ABD00: zero vectors, area -1) and, if the object exists, copies its position block (+0x120, 16 bytes), orientation block (+0x130) and area id (+0x254), then pushes it (checked).
- **Edge cases:** any object type works (it uses `GetObjectById` 0x005D5BE0, not the creature lookup). An invalid or unknown id gives the default location, position (0,0,0), area -1, with no failure (checked). Failed pop -639, failed push -638 (checked).

### GetPositionFromLocation (459)
- **Handler:** 0x0065B4D0 (own handler) (checked).
- **Described in:** `dev/LIBRARY.md` and `dev/PROBE.md` (checked in play 2026-09-30 through `Invoke`); no group note.
- **Does:** pops a location, copies its x, y, z into a 16-byte vector and pushes it with `StackPushVector` 0x00A2D6D0 (checked).
- **Edge cases:** no area or validity check: an unset location gives (0,0,0), the position it was constructed with (checked). Failed pop -639, failed push -638 (checked). A null location (array element past the end) is dereferenced unchecked (inferred from the group notes).

## Not read
- `CCArea::IsLocationSafe` 0x0073DE70's tests and `GetSafeLocation`'s flags beyond `dev/LOAD_EVENTS.md`.
