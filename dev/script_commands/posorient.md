# Script commands, group "posorient": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not.

Shared facts (checked):
- **Conventions** are master's `dev/script_commands/locations.md` "Facing ↔ orientation": `Vector::FromAngle` 0x004EAE30 = (sin(−a·k), cos(−a·k), 0), `Vector::ToAngle` 0x004EE570 = atan2(−x, y)·57.2958, normalize 0x00451A30 (|v|² ≤ 1e-6 → (1,0,0)). Position is object +0x120, orientation +0x130 (a raw direction vector), `CCObject::SetOrientation` 0x00694CE0 writes it.
- **Two angle conventions, both degrees.** Script facing (bMirrorYAxis TRUE, script.ldf's `DIRECTION_*`): orientation (−sin a, −cos a, 0); 0° = −Y = DIRECTION_SOUTH, 90° = −X = WEST, 180° = +Y = NORTH, 270° = +X = EAST, clockwise seen from +Z. Engine angle (FromAngle/ToAngle, mirror FALSE): (−sin a, cos a, 0); 0° = +Y, 90° = −X, counter-clockwise. Script facing = 180 − engine angle.
- **The facing apply, 0x00655170(obj, &dir)** (used by SetFacing, SetFacingPosition, SetFacingObject; not by SetOrientation):
  - Not active (obj+0x70's slot 0x6C, 0x00699FA0: +0x2C5 set and +0x421 clear, master's `dev/PARTY.md`): `CCObject::SetOrientation(dir)` at once, the vector raw (z kept).
  - Active creature (slot 0x10 cast): `MovementController::SetDesiredFacing` (0x00A638B0) on creature+0xA30. **Not instant**: it stores normalize(x, y, 0) as the desired facing (+0x90) and, unless already within 1° (`Vector::AngleBetween` ≤ 0.999999°) or the controller is busy (+0xF0 ≠ 0, or its owner's +0xC0 set), sets controller state 4 (0x00A63800), inferred the turn played over frames. A dir with |v|² ≤ 1e-6 instead clears the desired facing to (0,0,0): no turn. A command's end clears it too (master's `commands.md`, end handler 0x00746E10), so a queued command ending can cut the turn short (inferred).
  - Active non-creature: `ToAngle(dir)` then slot 0x78 (0x00699EB0, `CCObject::SetFacing(float)`), which writes (−sin θ, cos θ, 0): instant, z dropped, xy normalized. A zero xy gives θ = 0 → (0, 1, 0).
- None of these queues a command; all act inside the script. Each returns 0, −639 on a failed pop, −638 on a failed push.

### SetFacing (18)
- **Handler:** 0x006551D0 (not shared). It calls `Vector::FromAngle`, `CClientExoApp::GetObjectById` (0x005D5BE0) and the facing apply 0x00655170. (checked)
- **Does:** pops oTarget, fFacing, then bMirrorYAxis if more than 2 arguments (default 1). dir = FromAngle(fFacing); if bMirrorYAxis **== 1** y is multiplied by −1.0 (0x00AF0E48). Then the facing apply. (checked)
- **Units:** degrees; any value (sin/cos wrap it). TRUE → script facing (0° south, clockwise); FALSE → engine angle (0° north, counter-clockwise), i.e. the facing mirrored across the X axis: SetFacing(a, FALSE) faces as SetFacing(180 − a). (checked; the compass names from script.ldf's `DIRECTION_*`)
- **Edge cases:** only exactly 1 mirrors: bMirrorYAxis = 2 or −1 acts as FALSE (checked). An invalid object → nothing (checked). An active creature turns over time; an inactive one or a placeable snaps (shared facts). The orientation's z becomes 0. (checked)

### GetOrientation (656)
- **Handler:** 0x00655290 (not shared). It calls `CClientExoApp::GetGameObjectById` (0x005D5840) and the CCObject cast (slot 0x18; it calls it three times, redundantly). (checked)
- **Does:** pops oTarget and pushes its +0x130 as a vector, raw (not normalized; z as stored). (checked)
- **Edge cases:** an invalid id, an area or the module (no CCObject cast) → (0,0,0), as script.ldf says. A push failure → −638. (checked)

### SetOrientation (657)
- **Handler:** 0x00655340 (not shared). It calls `GetGameObjectById`, the CCObject cast, the normalize 0x00451A30 and `CCObject::SetOrientation` (0x00694CE0). (checked)
- **Does:** pops oTarget, then vOrientation; writes **normalize(v)** (3D, z kept) straight through `CCObject::SetOrientation`. (checked)
- **Instant for every object**, an active creature too: it bypasses the facing apply and the movement controller (checked). Whether the creature's model follows at once is slot +0x80's (not read; see locations.md's "Not read").
- **Edge cases:** a zero or tiny vector (|v|² ≤ 1e-6) is stored as (1, 0, 0): facing +X, east, which is 270° (−90°) in both conventions (checked for the vector). A non-zero z tilts the stored orientation; nothing flattens it. An invalid object or a non-CCObject → nothing. (checked)

### SetFacingPosition (23)
- **Handler:** 0x006556B0 (not shared). It calls `GetGameObjectById`, the CCObject cast, the normalize and the facing apply 0x00655170. (checked)
- **Does:** pops oObject, then vPosition; dir = normalize(vPosition − obj+0x120), 3D; then the facing apply. (checked)
- **Edge cases:**
  - vPosition equal to (or within 1e-3 m of) the object's own position: the normalize gives (1,0,0), so it faces +X (east). Not a no-op. (checked)
  - A point straight above or below: dir (0,0,±1). An inactive object stores that vertical vector as its orientation; an active creature's controller normalizes (0,0,0) to (1,0,0) and turns east; an active placeable gets (0,1,0), north. (checked, from the three paths)
  - A height difference otherwise: an inactive object keeps the tilted z; the active paths drop it. (checked)
  - An invalid object or a non-CCObject → nothing. (checked)

### SetFacingObject (24)
- **Handler:** 0x00655790 (not shared). It calls `GetGameObjectById` twice, the CCObject cast on both, the normalize and the facing apply 0x00655170. (checked)
- **Does:** pops oObject, oTarget, then bInvert if more than 2 arguments (default 1). dir = normalize(target+0x120 − obj+0x120), 3D. If bInvert **== 0**, dir is negated (all three components). Then the facing apply to oObject. (checked)
- **bInvert:** TRUE (any non-zero, the default) faces **toward** the target; FALSE faces **away**. The name reads backwards; the engine is as stated (checked). The one vanilla FALSE call is the ballista in `cli220ip_ballista_top.nss`.
- **Edge cases:** either object invalid or not a CCObject → nothing (checked). The two at the same spot: (1,0,0), east, or west with bInvert FALSE (checked). Height difference: as SetFacingPosition. oTarget == oObject: the same spot, so east. (checked)

### GetAngleBetweenObjects (161)
- **Handler:** 0x00655910 (not shared). It calls `CClientExoApp::GetObjectById` (0x005D5BE0) twice, the normalize and `Vector::SignedAngleBetween` (0x004D4D10 → 0x004D4CC0 → 0x004D4B80). (checked)
- **Does:** pops oObjectA, oObjectB. b = normalize(A.pos − B.pos) (from B to A); s = SignedAngleBetween(A's +0x130, b) in degrees; pushes s + 180.0 (0x00AF7ED8). (checked)
- **SignedAngleBetween:** θ = acos(dot) of the 3D vectors, dot clamped (|dot| ≥ 1 → 0 or π), A's orientation not normalized; negated when the xy cross (a.x·b.y − a.y·b.x) < 0. (checked)
- **Range and sense:** relative to A's facing, **[0, 360]**, counter-clockwise seen from +Z: B ahead → 0 or 360 (exactly ahead gives 180 + 180 = 360, rounding may give ~0), B to A's left → 90, behind → 180, right → 270. script.ldf's description matches (worked from the code; not run).
- **Edge cases:** either object invalid → 0.0, which reads as "ahead" (checked). Same spot: b = (1,0,0), so the answer depends only on A's facing (checked). A height difference shrinks |dot| and so pulls the answer toward 90/270 (inferred from the 3D dot). A's orientation of zero length (default) → dot 0 → 90 or 270 (inferred). No CCObject cast is made; `GetObjectById` decides what is accepted (inferred: game objects only).

## Not read
- `MovementController`'s per-frame turn (state 4): its rate, and what +0xF0 ≠ 0 (moving) does with the desired facing later.
- 0x00A69C90 (called from the state setter on +0x70) and the owner's +0xC0 flag.
- `CCObject::SetOrientation`'s slot +0x80 (whether a creature's model and physics turn at once).
- `GetFacing` (17), not in this group: presumably the inverse with the same bMirrorYAxis rule.
