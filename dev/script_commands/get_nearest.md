# Script commands, group "get_nearest" (164, 165): the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent, b2b) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not. The other get_nearest commands (127, 433–436) are `dev/PARTY.md` "A search by tag finds inactive objects".

Shared facts (checked): both are cases of the shared getter 0x00668A20, which also answers `GetObjectActive` (141) and others. It pops the object (`StackPopObject` 0x00A2F6D0; failure -639), then **one or two more ints only when paramCount > 1 / > 2** (for other commands; 164 and 165 take none), looks the object up with `CClientExoApp::GetObjectById` (0x005D5BE0) and switches on the command number. The creature cases go through the object's vtable +0x10 (as-creature), then a jump table indexed by `command − 160` (byte table 0x00668CFC, targets 0x00668CE8): 164 → 0x00668B5C, 165 → 0x00668B69. The result is pushed with `StackPushInteger` 0x00A2D650 (failure -638). An invalid id, or an object that is not a creature, falls through every case and pushes 0.

The perceived set is creature+0xC18 (a `std::set` of ids), kept by the perception update; `dev/COMBAT.md` "Perception" says who gets in (hostile stealthed creatures do not), when it is updated (explore and combat game modes only), and the 60 m keep range.

### IsPerceivingHostiles (164)
- **Handler:** 0x00668A20 (shared; case at 0x00668B5C) → `CCCreature` 0x006AC230 (`__thiscall`, no arguments, returns bool in al). (checked)
- **Does:** walks the creature's perceived set (+0xC18) from its first node and calls 0x006A2B80(id) on each; returns TRUE at the first answering 1, FALSE when the set ends. 0x006A2B80 is `IsObjectHostile(creature, other)` through the reputation manager at app+0x1B0 (`0x0064BB40`, `dev/COMBAT.md` "Who"). (checked)
- **State:** reads creature+0xC18 only; nothing is written and no perception update is forced. (checked)
- **Edge cases:** invalid object or non-creature → 0, the script goes on. (checked) No death or activity test on the perceived: a hostile that died stays counted until the next perception update drops it (checked that 0x006AC230 tests nothing else; the drop is `dev/COMBAT.md`'s). With app+0x1B0 null, 0x006A2B80 answers 0, so FALSE (checked). In a conversation, cutscene or menu the set is not updated, so the answer is the one from the last explore/combat frame (inferred from `dev/COMBAT.md`'s pace). Stealthed hostiles are never in the set, so they do not count (inferred, same source).

### IsPartyPerceivingHostiles (165)
- **Handler:** 0x00668A20 (shared; case at 0x00668B69) → 0x006A2CC0 (the party: `[[app+0xD0]+0x58]`, 0 with no game) → party method 0x0062F9F0. (checked)
- **Does:** for each id in the active party (the vector at party+0x234, elements from +0x238 to +0x23C), `GetCreatureById` (0x005D58C0) and, if found, 0x006AC230 (`IsPerceivingHostiles`'s test above); TRUE at the first member that perceives a hostile, else FALSE. (checked)
  - The vector at party +0x234 is IsInParty's list, holding the states ACTIVE and LOCKED_ACTIVE (PARTY.md). The creature's own active byte (+0x2C5) is not tested (read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2) [C], so a member in that list but deactivated still counts, with whatever perceived set it last had [I].
- **State:** reads the active party list and each member's +0xC18; writes nothing. (checked)
- **Edge cases:** **oCreature is only a gate**: it must be a valid creature (else 0, as above) but is otherwise ignored — any creature, party member or not, gives the same answer, the active party's. script.ldf's "the party of a creature" suggests otherwise. (checked) No party (no game) → 0. (checked) Ids in the list that are no longer creatures are skipped. (checked) Members in the pool but not the active party (+0x244) are not asked. (checked: only +0x234 is walked)

## Not read
- `0x0064BB40` itself (the reputation manager's hostility test); taken from `dev/COMBAT.md`.
- Whether a creature's +0xC18 set is cleared when it leaves the active party or dies (`ClearPerceptionList` 446 is another group's).

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe. Not seen in play. A creature's perceived set is the `std::set` of ids at creature +0xC18, with a pending list at +0xC28 (`dev/COMBAT.md`, "Perception"). `GetCreatureById` is 0x005D58C0, `GetObjectById` 0x005D5BE0.

### UT_GetNearestObjectByTag (360)
- **Handler:** 0x0066DB20 (not shared; the engine's name is `COMMAND_GETSINGLENEARESTOBJECTBYTAG`). `GetObjectById`, `GetArea` 0x00695180, the area search `ScriptBackend::GetNearestObjects` 0x006849C0, `StackPushObject` 0x00A2D980.
- **Described in:** `dev/PARTY.md` ("A search by tag finds inactive objects").
- **Does:** pops oObject, sTag, nIncludeSelf (always); takes oObject's area and searches it for objects whose tag matches, with the type mask -1 (any), the group -1 (any), a result count of 1 and nIncludeSelf as the last argument (the argument meanings inferred from the pushes at 0x0066DBEE to 0x0066DC08); pushes the first of the result vector (the nearest to oObject) or OBJECT_INVALID if the result is empty. (checked apart from the argument meanings)
- **State:** none written; the result vector is freed.
- **Edge cases:** an invalid oObject, or one with no area, pushes **OBJECT_INVALID** (`edi` starts at -1) and the script goes on. A non-empty tag finds **inactive** objects too (`PARTY.md`); an empty tag does not. Only oObject's own area is searched. A failed pop is -639, a failed push -638.

### TriggerPerception (166)
- **Handler:** 0x0065A7B0 (not shared). `GetCreatureById`, a flag read 0x006A2CE0, the area's lookup 0x0067B430, then `CCCreature` 0x006B32A0.
- **Described in:** `dev/COMBAT.md` ("Perception"), which describes the update that fills the set; this command is not named there.
- **Does:** pops oPerceivingCreature, then oPerceivedCreature (an id, not looked up here). The perceiver must be a creature with an area, and the area must answer 0x0067B430(area, creature +0x150) non-null (the creature's room or tile; inferred). Then 0x006B32A0(perceived id, a flag, area, that result, &position) runs: it returns 0 for the creature itself, for one already in the set (0x008704D0), or when 0x006A2BB0 refuses; otherwise the perceived must exist as a creature (`GetCreatureById`), it is put in the set (0x006B3240) and the answer is **1**. The script gets that 1 or 0. (checked; 0x006A2BB0 and 0x006B3240 not read)
- **State:** the perceiver's +0xC18 set and the APPEAR event (below).
- **Edge cases:** an invalid perceiver or a non-creature gives 0 (the push is of `ebx`, zero). A perceived id that is not a creature gives 0. It does not run the line-of-sight and range test of the normal update (inferred from the call path: the test, if any, is 0x006A2BB0's).
- **It raises APPEAR** [C, read 2026-10-22 by paragon-agent-2's helper, checked by lib-agent-2, for DAOParagonEngine #169; 0x006B32A0 → 0x006B3240 → 0x006AFCF0]: 0x006B32A0(perceived id, flag, area, room, &position) returns 0 for the creature itself (`cmp esi, [this+4]`), for one already in the +0xC18 set (0x008704D0) and when `0x006A2BB0` refuses (COMBAT.md:135); else the perceived object must exist (`GetCreatureById` 0x005D58C0), then `0x006B3240(this, perceived, flag')` where `flag'` = `0x006A44D0(perceived, perceiver id)`: the perceived's stealth +0x7F0 is non-zero AND `0x006A2B80(perceiver id)` (the hostility test, the int 0 of the event) is non-zero. `0x006B3240` inserts the id into +0xC18 (`0x006B1120`) only when flag' is 0, then ALWAYS calls `0x006AFCF0(this, perceived, 0, 0)` (the APPEAR event, COMBAT.md:64), then `0x006A2CE0(this)`'s test sets the perceived's byte +0xB80 = 1. The command pushes 1. So a script-triggered perception announces `EVENT_TYPE_PERCEPTION_APPEAR` at once, as the update's newcomers do (COMBAT.md:60), and a stealthed hostile is announced with bStealthed = 1 but is not put in the set (COMBAT.md:59), still answering 1.

### IsPerceiving (170)
- **Handler:** 0x0066E040, shared with `IsInAOE` (336); the branch on command 0xAA (170) at 0x0066E08E.
- **Described in:** `dev/COMBAT.md` ("Perception": the set at +0xC18, `IsPerceiving`) and `aoe.md` (the shared handler).
- **Does:** pops oidA, then oidB; `GetCreatureById(oidA)`; for 170 it tests whether oidB is in the creature's set (0x008704D0 on +0xC18) and pushes that byte. (checked)
- **State:** read only.
- **Edge cases:** oidA not a creature, or invalid, pushes **0**. oidB is not looked up: any id not in the set, including an invalid or dead object's, is simply FALSE. It is one way: A's set holds B, not the reverse. It tells what the last perception update left, not what A could see now (`COMBAT.md`). A failed pop is -639.

### ClearPerceptionList (446)
- **Handler:** 0x0065A870 (not shared). `GetCreatureById`, then `CCCreature::ForScript_ClearPerceptionList` 0x006B8D20.
- **Described in:** `dev/COMBAT.md` ("Perception": the set and the pending list); the command itself is not named there.
- **Does:** pops oPerceiver; if it is a creature, empties its perceived set (+0xC18, via 0x006318E0) and its pending-appear list (+0xC28, via 0x005198D0). (checked)
- **State:** creature +0xC18 and +0xC28.
- **Edge cases:** an invalid or non-creature id does nothing and returns 0. **No DISAPPEAR event is raised** for the creatures dropped (the function raises none; checked), so a script that listens for it does not hear these. The next perception update refills the set from the creatures in range. A failed pop is -639.

### GetObjectsInShape (673) and FilterObjectsInShape (197) (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2) [C unless marked]

- **Handler:** 0x00670EB0 (`ScriptCommand::FilterOrGetObjectsInShape`), shared by both commands.
- **The arguments:**
  - It pops the array (197 only), the type mask, the shape, the location, fA, fB and fC.
  - fC defaults to 65505.0 (0x477FE100), and no shape reads it.
  - bExcludeDead is popped only when 673 has more than 6 arguments or 197 more than 7.
- **The objects:**
  - With no array, the location's area's list (GetAreaById, area +0x690).
  - With an array, the array's objects, whatever area each is in.
  - With neither, an empty array.
- **Each object is dropped when:**
  - it is not found;
  - it is inactive (+0x70 slot 0x6C, GetObjectActive; script_fields.csv:32);
  - `(1 << (byte[obj+0x14] − 1)) & mask` is 0 (0x00A581F0);
  - bExcludeDead is set and it is dead (`CCObject::ForScript_IsDead` 0x00695300: health, slot +0x120, ≤ 0);
  - the shape is neither 1 nor 2.
- **Personal space:** slot 39 (+0x9C) is `CCCreature::PersonalSpace` 0x006A21A0 (`[[+0x7B0]+0x60]`, apr_base PerSpace). Every other class has 0x0083A430, which returns 0.0 (a scan of every class with at least 40 slots).
- **Sphere (1), 0x004EB340(object position +0x120, space, the location's position, fA):** the ground (x, y) distance² ≤ (space + fA)², inclusive. z is not read.
- **Cone (2), 0x004EB3B0(object position, space, apex P, the location's orientation, fA, fB):**
  - It is refused unless 0 ≤ fA ≤ 180 (180.0 at 0x00AF14D0), so fA is the whole width.
  - The object must first pass the sphere test with fB: within fB + space of the apex, on the ground.
  - θ = `Vector::ToAngle`(orientation) (0x004EE570), and h = fA × 0.5 (0x00AE70F8).
  - The edges are Lft = P + fB·FromAngle(θ + h) and Rgt = P + fB·FromAngle(θ − h). `Vector::FromAngle` 0x004EAE30 is (−sin, cos, 0).
  - The object is in when it lies strictly between the two edges, both cross products > 0 as in the draft.
  - Otherwise it is in when its personal-space circle meets either edge segment (0x004EB240 with the foot 0x004EAF10): an end strictly inside the circle, or the foot on the segment at a ground distance² ≤ space².
  - So with no space, the apex and any point exactly on an edge are in.
- **The cone spell** (`_Ability_Scripts\Spells\spell_cone.nss`, from :75):
  - **The shape:** `GetObjectsInShape(OBJECT_TYPE_CREATURE, SHAPE_CONE, GetLocation(caster), aoe_param1, aoe_param2)`, with aoe_param2 = 5.0 when ≤ 0. The apex is the caster, the direction the caster's facing, and the caster is left out by the script.
  - **Dead creatures are hit:** bExcludeDead is left false (:104).
  - **The damage:** each target that passes CheckSpellResistance gets `SetIndividualImpactAOEEvent(..., 500)`, a delayed per-target event. Flame Blast first sends COMBO_EVENT_IGNITE.
  - **The look:** `EffectConeCasting(vfx_impact0)` on the caster for 1.5 s.

## Not read
- 0x006A2BB0 (the perception test TriggerPerception relies on), 0x0067B430's use of creature +0x150.
