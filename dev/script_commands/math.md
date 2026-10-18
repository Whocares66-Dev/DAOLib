# Script commands, group "math": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm` and capstone scans, for DAOParagonEngine #27. Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not.

Shared facts (checked):
- Every handler is `__thiscall (commandId, paramCount)`, `ret 8`, on `CVirtualMachineCommands`; −639 (0xFFFFFD81) on a failed pop, −638 on a failed push, else 0 (master's `dev/script_commands/commands.md`). Arguments pop in the declared order.
- Floats are 32-bit; the x87 work is done at its default precision and stored back to float before the push. The CRT routines called are MSVC 2005's `_CIsin`, `_CItan`, `_CIasin`, `_CIsqrt`, `_CIatan2` (0x008E8990, 0x008E6340, 0x008E8AC0, 0x008E6240, 0x008E893A; the address library calls the first three `ScriptBackend::ForScript_sin`/`_tan`/`_asin`, but they are the CRT's intrinsics). Out-of-domain CRT results are the CRT's: NaN (the x87 indefinite), errno EDOM (inferred from the CRT, not read).
- **Angles in the vector commands are degrees**, converted with k = 0.017453279 (double 0x00AF14C0, a slightly short π/180) and back with 57.295826 (0x00AF0E50). The trig commands `sin`/`tan` take radians as script.ldf says; `asin` does not (below).
- **Two angle conventions.** `AngleToVector`/`VectorToAngle` use 0° = +Y, counter-clockwise (90° = −X). A location's or object's facing (master's `script_commands/locations.md`, and the posorient group) mirrors Y: 0° = −Y. So `AngleToVector(GetFacing(o))` is the object's direction mirrored in Y, not its orientation (the conventions: checked; that use: inferred).
- **Normalize** 0x00451A30: |v|² ≤ 1e-6 gives (1, 0, 0), else v/|v| (locations.md).
- **The RNG.** `Random` draws from one engine-wide generator: the object at `[0x00C2BDF8]+8` (getter 0x004C0030), next value 0x004E7050. It is a four-component combined Tausworthe (L'Ecuyer's LFSR113 form), (k, q, s) = (31, 6, 20), (29, 2, 12), (28, 13, 7), (25, 3, 15), output the XOR of the four 32-bit states, so a full 32-bit value. Its constructor 0x004E7510 sets fixed initial states (0x1745D12D, 0x249248ED, 0x333332F9, 0x383EF9FA), then **seeds once from entropy**: 128 bytes of uninitialised stack, `QueryPerformanceCounter` and `GetSystemInfo` folded to 16 bytes (0x004E7490, 0x004E7250) and split into the four seeds (0x004E70F0; a seed too small for its component keeps the fixed state). Nothing reseeds it (0x004E70F0 has that one caller), so a run's sequence cannot be reproduced and saves do not hold it. Other engine code draws from it too (0x00672006, 0x00A41380), so a script's draws interleave with the engine's. (checked)
- `RandomFloat` (767, not this group) is a different generator: MSVC's `rand` LCG (×0x343FD + 0x269EC3, bits 16–30) on its own static state 0x00C349AC, initial 0x83171015, written by nothing else, times 1/32767 (0x00AF7EB0): [0.0, 1.0] inclusive, and **the same sequence every launch**. (checked)

### Random (25)
- **Handler:** 0x0064EAB0 (not shared). It calls 0x004C0030 (the generator) and 0x004E7050 (next value). (checked)
- **Does:** pops n. n = 0 → pushes 0 without drawing. Otherwise draws r (u32) and pushes `r mod n` as an **unsigned** division (`div`), negated when n < 0. (checked)
- **Edge cases:** n > 0 → [0, n−1], with the usual small modulo bias (2³² is not a multiple of n). n = 1 → always 0 (one draw still taken). **n < 0 is garbage**: the divisor is 2³²−|n|, so the result is about −r, any 32-bit value of either sign, not script.ldf's "0 on error". Only n = 0 returns 0. (checked)

### abs (26)
- **Handler:** 0x0064EB80 (not shared). (checked)
- **Does:** pops an int, pushes `(x ^ (x>>31)) − (x>>31)`. (checked)
- **Edge cases:** abs(−2147483648) = −2147483648 (wraps). Otherwise exact. (checked)

### fabs (27)
- **Handler:** 0x0064EBD0 (not shared). (checked)
- **Does:** pops a float, pushes x87 `fabs` of it: clears the sign bit. (checked)
- **Edge cases:** −0.0 → +0.0; NaN stays NaN (sign cleared); ±inf → +inf. No error path, as script.ldf says. (checked)

### sin (29)
- **Handler:** 0x0064EC90 (not shared). It calls `_CIsin` 0x008E8990. (checked)
- **Does:** pops a float (radians) and pushes sin(x), rounded to float. (checked)
- **Edge cases:** no range reduction of its own; the CRT's handles any finite x (inferred); ±inf or NaN → NaN (inferred: CRT). (checked apart from that)

### tan (30)
- **Handler:** 0x0064ECF0 (not shared). It calls `_CItan` 0x008E6340. (checked)
- **Does:** pops a float (radians), pushes tan(x) as a float. (checked)
- **Edge cases:** near π/2 + kπ the result is large but finite (π/2 is not exact in float), not an error; ±inf/NaN → NaN (inferred: CRT). (checked apart from that)

### asin (32)
- **Handler:** 0x0064EDB0 (not shared). It calls `_CIasin` 0x008E8AC0. (checked)
- **Does:** pops x, **multiplies it by π/180** (k, 0x00AF14C0), rounds to float, and pushes `asin(x·k)` in **radians**. So asin(1.0) = asin(0.0174533) ≈ 0.0174542, not π/2. (checked)
- **Edge cases:** the domain is therefore |x| ≤ 57.2958, not [−1, 1]; outside it the CRT gives NaN (inferred: CRT). script.ldf ("the arcsine of an angle in radians") is wrong both ways. `acos` (31, 0x0064ED50) has no such multiply (checked; not this group). No vanilla script calls `asin` (grepped). (checked)

### sqrt (36)
- **Handler:** 0x0064EFA0 (not shared). It calls `_CIsqrt` 0x008E6240. (checked)
- **Does:** pops x; if x > 0 pushes sqrt(x), else pushes 0.0. (checked)
- **Edge cases:** the test is `fcom` against 0 with C0|C3: x < 0, x = ±0 and **NaN** all give 0.0 without calling sqrt, so there is no NaN from this command. +inf → +inf. Matches script.ldf's "0.0 on error". (checked)

### GetDistanceBetweenLocations (38)
- **Handler:** 0x00655B70 (not shared). It calls `_CIsqrt`. (checked)
- **Does:** pops lLocationA, then lLocationB (deep copies; master's `locations.md`). If their area ids (+0x20) are equal it pushes the **3D** distance √(dx²+dy²+dz²) of the positions (+0x00); otherwise **−1.0** (0x00AE2E00). Frees both copies. (checked)
- **Edge cases:** only the raw ids are compared, not validated: two default locations (area 0xFFFFFFFF) give their real distance, not −1. A null location faults on `[+0x20]` (no check: checked; the fault: inferred). If the second pop fails the first copy is leaked (it returns −639 without freeing). (checked)

### IsInTrigger (39)
- **Handler:** 0x0066DA90 (not shared; `ScriptCommand::INTRIGGER`). It calls `CClientExoApp::GetCreatureById` 0x005D58C0 and `std::vector<unsigned int>::Contains` 0x008663A0. (checked)
- **Does:** pops oObject, then oTrigger. Looks oObject up **as a creature** and answers whether oTrigger's id is in the creature's vector of trigger ids at **+0xBA8**. (checked)
- **State:** creature+0xBA8 (`std::vector<u32>`, begin +4, end +8) is kept by trigger code: 0x006368F0 and the routine at 0x00636A90 edit it beside the trigger's own list at trigger+0x184 (checked that they touch both; that these are the enter/exit bookkeeping: inferred). No geometry is tested here: it is membership as the trigger last recorded it. (checked)
- **Edge cases:** oObject not a creature (a placeable, an item) or invalid → FALSE. oTrigger invalid or not a trigger → FALSE (not in the list). A copy of this handler's body is at 0x006DA100, pushing to the VM at `[0x00C1D084]` instead (checked; whose table registers it: not read). (checked)

### IsVectorEmpty (40)
- **Handler:** 0x0064F010 (not shared). (checked)
- **Does:** pops a vector; TRUE only when x, y and z each compare **exactly** equal to 0.0 (`fcom`, C3/C2). (checked)
- **Edge cases:** −0.0 counts as zero; any NaN component → FALSE; 1e-30 → FALSE. No tolerance. (checked)

### Vector (41)
- **Handler:** 0x0064F0A0 (not shared). (checked)
- **Does:** pops x, y, z and pushes (x, y, z) as is. (checked)
- **Edge cases:** the defaults (0.0) are the compiler's, filled in at the call; the handler always pops three. No check on the values. (checked)

### GetVectorMagnitude (42)
- **Handler:** 0x0064F120 (not shared). It calls `_CIsqrt`. (checked)
- **Does:** pops v, pushes √(x²+y²+z²), the sum rounded to float before the root. (checked)
- **Edge cases:** the zero vector → 0.0; overflow (components ≳ 1.8e19) → +inf; NaN → NaN. (checked apart from the overflow figure: computed)

### GetVectorNormalize (43)
- **Handler:** 0x0064F1B0 (not shared). It calls the normalize 0x00451A30. (checked)
- **Does:** pops v. **It normalizes only when x, y and z are all non-zero**; if any component equals 0.0 exactly, it pushes (0, 0, 0). (checked: each `fcom … test ah,0x44 / jnp` jumps to the zero result on *equal*)
- **Edge cases:** so GetVectorNormalize(Vector(3, 4, 0)) is (0, 0, 0), not (0.6, 0.8, 0): almost certainly a bug (an `&&` where `||` was meant), but it is what runs. With all three non-zero but |v|² ≤ 1e-6 (e.g. (1e-4, 1e-4, 1e-4)) the normalize gives (1, 0, 0). A NaN component counts as non-zero. script.ldf's "empty vector on error" is the zero-component case. No vanilla script calls it (grepped). (checked)

### AngleToVector (44)
- **Handler:** 0x0064F260 (not shared). (checked)
- **Does:** pops fAngle in degrees, a' = −fAngle·k, `fsincos`, pushes **(sin a', cos a', 0) = (−sin a, cos a, 0)**, a unit vector. 0° → (0, 1, 0), 90° → (−1, 0, 0), 180° → (0, −1, 0). (checked)
- **Edge cases:** any angle works (wraps). Unlike `Location`/`SetLocationFacing` it does **not** negate y (shared facts). For |a'| ≥ 2⁶³ `fsincos` leaves its operand unreduced (x87; inferred), so absurd angles give garbage. (checked apart from that)

### VectorToAngle (45)
- **Handler:** 0x0064F310 (not shared). It calls the normalize 0x00451A30 and `Vector::ToAngle` 0x004EE570. (checked)
- **Does:** normalizes v, then pushes atan2(−x, y)·57.295826: degrees, 0° = +Y, counter-clockwise, range (−180, 180]. The inverse of `AngleToVector`. z only scales the normalization, so it does not change the angle. (checked)
- **Edge cases:** no y negation here, unlike `GetFacingFromLocation` (locations.md). The zero vector or |v|² ≤ 1e-6 → (1,0,0) → **−90.0**. A vertical (0, 0, z) → atan2(−0, 0) = −0.0 (computed). Negative angles come back, not 0..360. (checked)

### CheckLineOfSightObject (46)
- **Handler:** 0x00668DB0 (not shared). It calls `CClientExoApp::GetGameObjectById` 0x005D5840 (both), the objects' slot 0x18, `CCObject::Area` 0x00695180, `SmartData::Attribute::Get` 0x0053CC40 and `CCArea::LineOfSight` 0x0067C3F0. (checked)
- **Does:** pops oSource, oTarget. Takes the source's area; if the target's area id (+0x254) differs → FALSE. Else casts from source position + (0, 0, 1.8) to target position + (0, 0, 1.8) (0x00B0F264, eye height) through `CCArea::LineOfSight(from, to, 0, ignore −1, mask 0x24)`. (checked)
- **The ray** (0x0067C3F0, `ret 0x14`): the physics scene's closest-shape raycast with groups mask 0x24 (placeables 0x4, static geometry 0x20; creatures are outside it), master's `dev/COLLISION.md`. No hit → TRUE. A hit on static geometry → FALSE. A hit on an object: its id = ignore → TRUE; a placeable that `CCPlaceable::BlocksSight` (0x0067A670: base type +0x638 is 1, door, or 8) → FALSE; **any other placeable → TRUE**; a non-placeable object → FALSE. (checked)
- **Edge cases:** this is **not** perception's test (0x0067C530, which any hit fails; master's `dev/COMBAT.md`): here a crate or statue is seen through. Since only the closest hit is reported, a see-through placeable in front of a wall makes the whole line clear (inferred from the closest-hit query). No room check, unlike perception. Either object invalid or without an area → FALSE. A closed door as the **target** is hit by its own ray end and answers FALSE (inferred). Both objects can be any type. Expensive, as script.ldf warns (a PhysX raycast per call). (checked apart from what is marked)

### CheckLineOfSightVector (47)
- **Handler:** 0x00655CA0 (not shared). It calls `GetGameObjectById`, `CCObject::Area` and `CCArea::LineOfSight` 0x0067C3F0. (checked)
- **Does:** pops vSource, vTarget. Uses **OBJECT_SELF**'s (the VM's `[this+8]`) area, adds 1.8 to both z, and runs the same ray as `CheckLineOfSightObject` from source to target. (checked)
- **Edge cases:** the positions are taken as ground points (eye height added), not eye points. OBJECT_SELF invalid or not in an area (a module script) → FALSE, as script.ldf's remark says. Positions outside the area are not checked. Same see-through-placeable and closest-hit rules as above. (checked apart from the closest-hit consequence)

## Not read
- The trigger routines 0x006368F0 and 0x00636A90 in full: when a creature's +0xBA8 is updated (on its move, each frame, on the trigger's own check) and whether it is cleared on an area change or a save load.
- The CRT's own domain-error paths (`_CIasin` beyond ±1, `_CItan` and `_CIsin` on inf), taken from the CRT's documented behaviour.
- `CCArea::LineOfSight`'s hit record (flag +0x54 bit 4, id +0x5C) as filled by the physics scene's slot 4, and what base type 8 is (`placeable_types.gda`).
- The other users of the shared RNG (0x00672006, 0x00A41380) and the holder object at `[0x00C2BDF8]` (built at 0x004C013D).
- `cos` (28), `acos` (31), `atan` (33), `log` (34), `pow` (35), `GetDistanceBetween` (37): not in this group's list.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe. Not seen in play. Shared facts of `math.md` hold: floats are rounded to 32 bits between steps; a failed pop is -639, a failed push -638; the CRT routines return NaN out of their domain (inferred from the CRT, not read).

### RandomFloat (767)
- **Handler:** 0x0064EB20 (not shared). `StackPushFloat`.
- **Described in:** `dev/script_commands/math.md` (the last bullet of the shared facts: the LCG, state 0x00C349AC, times 1/32767), `dev/TURN_BASED.md` (a command that ran only in combat).
- **Does:** `state = state * 0x343FD + 0x269EC3`; takes bits 16 to 30; pushes that integer times 1/32767 (double at 0x00AF7EB0). (checked)
- **Edge cases:** no arguments, so no pop; range [0.0, 1.0] inclusive (0x7FFF / 32767 = 1.0). The state is a plain global, so every script shares one sequence and it is the same on every launch (initial value in `math.md`). Unlike `Random` (25), not seeded from entropy. Only a failed push can fail it.

### cos (28)
- **Handler:** 0x0064EC30 (not shared). The CRT routine 0x008E6020 (the cosine; name inferred, the family of `_CIsin` 0x008E8990).
- **Described in:** not in master's notes beyond the command table; `sin` (29) in `math.md` is the model.
- **Does:** pops a float and pushes cos(x) in **radians**, no degree conversion (checked: no multiply before the call), as script.ldf says.
- **Edge cases:** no range check of its own; infinity or NaN gives NaN (inferred from the CRT). A failed pop returns -639.

### acos (31)
- **Handler:** 0x0064ED50 (not shared). The CRT routine 0x008E5ED0 (the arc cosine; name inferred).
- **Described in:** `math.md`, `asin` section ("`acos` has no such multiply").
- **Does:** pops x and pushes acos(x) in radians. No conversion (checked), unlike `asin` and `atan`, which multiply the argument by pi/180 first.
- **Edge cases:** the domain is the true [-1, 1]; outside it the CRT's NaN (inferred). `acos` is the only inverse trig command that does what script.ldf says.

### atan (33)
- **Handler:** 0x0064EE20 (not shared). The CRT routine 0x008E9FD0 (the arc tangent; name inferred).
- **Described in:** `math.md`'s `asin` section is the sibling; `atan` itself is not described.
- **Does:** pops x, **multiplies it by pi/180** (k, double 0x00AF14C0, checked at 0x0064EE44), and pushes atan(x * k) in radians. So atan(1.0) is atan(0.0174533), about 0.0174524, not 0.7854.
- **Edge cases:** like `asin`, script.ldf is wrong about the argument. No domain limit; a very large x gives about 1.5708 as for any arctangent.

### log (34)
- **Handler:** 0x0064EE90 (not shared). The CRT routine 0x008E33E0 (the natural log; name inferred).
- **Described in:** not in master's notes beyond the command table.
- **Does:** pops x. If x is greater than 0 it pushes ln(x); **otherwise it pushes 0.0** (the result starts at 0.0 and the call is skipped when `x <= 0`: `fcom` then `test ah, 0x41` at 0x0064EEBB to 0x0064EEC4, checked).
- **Edge cases:** log(0) and log of a negative give **0.0**, not -inf or NaN. A NaN input also skips the call (an unordered compare sets the same status bits; inferred from the flag test). script.ldf does not say so.

### pow (35)
- **Handler:** 0x0064EF00 (not shared). The CRT routine 0x008E8C50 (pow; name inferred).
- **Described in:** not in master's notes beyond the command table.
- **Does:** pops fValue, then fExponent; pushes pow(fValue, fExponent), with one special case: **when fValue == 0 and fExponent < 0 it pushes 0.0** instead of infinity (0x0064EF3A to 0x0064EF5A: base compared to zero with `fucom`, exponent with `fcom`, checked).
- **Edge cases:** pow(0, 0) and pow(0, positive) go to the CRT (1.0 and 0.0; inferred). A NaN base goes to the CRT. A negative base with a fractional exponent gives the CRT's NaN (inferred).

### GetDistanceBetween (37)
- **Handler:** 0x00655A50 (not shared; the engine's name is `COMMAND_GETDISTANCEBETWEENOBJECTS`). `GetGameObjectById` 0x005D5840 twice, `Vector::Magnitude` 0x00451990.
- **Described in:** `dev/script_backends.csv` (the backend name) and `dev/SCRIPT_COMMANDS.md` (the name difference); no note says what it does.
- **Does:** pops oObjectA and oObjectB; each must pass the `CCObject` cast (vtable slot +0x18, as in `GetPosition`); pushes the length of (B.position - A.position), positions at +0x120. **3-D, z included**, in metres. (checked; that the magnitude is the Euclidean length is inferred from its name)
- **Edge cases:** either id invalid, or an object failing the cast, pushes **-1.0** (the result starts at -1.0, 0x00AE2E00, checked). No area test: two objects in different areas give the plain difference of their coordinates. The same object twice gives 0.0. Inactive objects still answer.

## Not read
- The CRT routines 0x008E6020, 0x008E5ED0, 0x008E9FD0, 0x008E33E0 and 0x008E8C50 inside (named by their use only). `Vector::Magnitude` was not read.
