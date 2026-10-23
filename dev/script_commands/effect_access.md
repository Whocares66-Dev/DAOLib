# Script commands: effect_access (GOG 1.05, read 2026-10-18 by lib-agent-2's subagent)

For DAOParagonEngine #27. Read in `game\DAOrigins.exe` with `tools\daoexe.py`; no Ghidra, no play. "Checked" means read in the disassembly; "inferred" means not followed to the end. Every handler pops its arguments with the VM's `StackPop*` and fails the script with 0xFFFFFD81 when a pop fails (missing or wrong-typed argument), and with 0xFFFFFD82 when its push of a result fails (checked in each one).

Shared facts used below (checked unless marked):
- An `effect` on the VM stack is a copy. `StackPopEngineStructure` (0x00A2F650) and `StackPushEngineStructure` (0x00A2D920) both copy through the VM's structure implementer, slot 0x20, and each handler deletes the copy it popped (vtable slot 0, `push 1`) before returning. So a `Set*` command never changes an effect already applied, or the one in the current event: it returns a changed copy, which matters only if the script then applies it (inferred from the copy calls and `dao/Structures.h`'s 0x7C-byte copy note; consistent with `SetIsCurrentEffectValid` existing).
- CGameEffect (0x7C bytes, constructor 0x00A652A0, checked): CDataArrays at +0 (vtable; +0x4 ints, +0x8 floats, +0xC flags, +0x10 objects, +0x14 strings, +0x18 vectors, each a pointer to a list made on first set); +0x20 id (from the global counter 0x00C1CF10, only when the type is non-zero); +0x24 type; +0x28 duration type; +0x2C duration ms; +0x30 elapsed ms; +0x34 animation; +0x38 priority; +0x3C flags; +0x40 creator (constructor: -1); +0x44 ability (0); +0x48 invalid word (0); +0x4C -1; +0x50 byte "started"; +0x54 a second embedded CDataArrays (constructor calls 0x00A59E10 on it; not the one the Get/SetEffect* commands use); +0x74, +0x78 -1. The copy constructor (0x00A65130) keeps the id, so copies share it.
- `CGameEffect::Prepare` (0x0065C760, ID 13143; `__thiscall`-free `ret 0x14`: effect, durType, duration, creator, ability), checked: duration type 1–4 or 6 stored at +0x28, anything else (0, 5, >6, negative) stored as 3 PERMANENT; for type 2 only, +0x2C = ftol(duration × [0x00AEBC00], read as ×1000 in `dao/Structures.h`), 0 allowed; creator through 0x0079F410 (an area-of-effect object, cast slot 0xC, is replaced by its +0x4AC, its own creator; else the id as given), stored at +0x40 by 0x00A65250; ability at +0x44.

### GetClusterCenter (445)
- **Handler:** 0x00671980 (own). Calls `GetCreatureById` 0x005D58C0, `CCObject::Area` 0x00695180, the data-table reads 0x00A2CCC0 (int) / 0x00A2CC80 (float), an area spatial query (area 0x0067B430 → +0x278 → 0x00686F70), hostility 0x0064B900 on the application's +0x1B0, distance 0x004519D0/0x00451990, an RNG (0x004C0030 → 0x004E7050).
- **Does:** pops creator (object), nAbilityId, nClusterSize, nAllyFailChance, bReturnFirstMatch. Radius = `persistent.gda` (table 96) `Radius` at the ability's `AOE_Idx` when `AOE_Idx` ≠ 0, else the ability's `AOE_Param1`; doubled when `AOE_Type` == 1 (checked; column hashes matched by CRC). It also reads `ranges.gda` `PrimaryRange` at the ability's `Range` into a local that is never read (checked).
- Candidates: the area's spatial query around the creator's position (+0x120) with radius the float at the creator's stats (+0x7C4 value) +0x68 (read 2026-10-22; that the attribute is a perception range is inferred). An entry is kept when its query flags have bit 2 and not bit 0 (meaning inferred), and a stealthed one (+0x7F0 ≠ 0) only when within its +0x7F4 of the creator (checked).
  - [C] (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) The query is `0x00686F70(area+0x278, &query)` (the area's spatial query; area = `CCObject::Area` of the creator, via 0x0067B430): it clears four counters (area+0x5E0, 0x5E4, 0x5EC, 0x5F0) and, when the tree root at area+0x584 is set, walks it with 0x00685B50(root, results list at area+0x5A8, ..., query centre = query+0x10, squared radius = query+0x20 squared). The results list at area+0x5A8 is returned.
  - [C] The query record is built at 0x00671B8A-0x00671BA9: radius = the float at +0x68 of the record returned by `SmartData::Attribute::Get` (0x0053CC40) on the creator's +0x7C4; centre = the creator's position (+0x120); the record is built by 0x00559FF0. That the radius comes from that float is therefore read, not inferred. [I] what the attribute is (a perception range is only a guess from the value's use).
  - [C] The walk 0x00685B50 is a depth-first walk of a binary space tree: an inner node splits on one axis (x or y, picked by 0x0067A2D0); it visits the child on the query centre's side of the split first, then the other child only if the squared distance from the centre to the split does not exceed the squared radius. The order of the candidates is therefore the tree's traversal order (near side first), not sorted by distance. No sort sits between the call (0x00671BE2) and the candidate loop (0x00671C20), which reads the result array as 8-byte (object pointer, second dword) pairs, count from 0x00566060 and base from 0x0066DD20.
  - [C] A leaf entry is kept when its flag byte has bit 0 clear and bit 2 set, its object's +0x41C (the area-side record) is non-null, and 0x0067A3C0 on that record's +0x278 passes; then the full 3-D squared distance (0x0045B080: dx^2 + dy^2 + dz^2 of the object's +0x120 and the centre) is taken, and the entry is dropped when it exceeds the squared radius, else appended by 0x00681DC0. What bits 0 and 2 mean: [I].
- Clusters, checked: each candidate i seeds one if hostile to the creator (0x0064B900 on the creator's slot 0x10 answer) and not dead (slot 0x1D0). Every live candidate j (in order, the seed included) joins when within the radius of the seed and of every member already in (pairwise distances cached). Non-hostile members (neutrals, and the creator itself if near: inferred) count as allies. Score = enemies − allies; a cluster qualifies when enemies ≥ nClusterSize and score ≥ the best so far (so ties go to the later one); then it is rejected when `rand % 100 < allies × nAllyFailChance`.
- Returns a location: position = mean of the best cluster's hostile members' positions (allies left out), orientation 0, area = the creator's area (checked).
- **State:** reads only; consumes RNG draws.
- **Edge cases (checked):** bReturnFirstMatch is popped and never read: the whole set is always searched. **A best cluster seeded by candidate 0 is never returned** (`cmp [best],0; jle` at 0x00672043). Creator not a creature, not in an area, or app+0x1B0 null, or no qualifying cluster → the default location: position (0,0,0), area OBJECT_INVALID (constructor 0x007ABD00). nClusterSize ≤ 0 lets any hostile seed qualify. nAllyFailChance ≥ 100 rejects every cluster with an ally; 0 never rejects.

### Engine_ApplyEffectAtLocation (596)
- **Handler:** 0x0065C830 (own). Calls `GetAreaById` 0x005D5960, the creator check 0x0065C7D0, `CGameEffect::Prepare` 0x0065C760, `CDataArrays::SetVector` 0x00A5A9B0, and the area's effects holder (area +0xCC) 0x007A1A20 → the effects manager's 0x007A1920 (the same apply path as `Engine_ApplyEffectOnObject`).
- **Does (checked):** pops nDurationType, effect, location, fDuration, oCreator, and nAbilityId only when the argument count is ≥ 6 (else 0). Looks up the location's area (+0x20). oCreator OBJECT_INVALID → the area itself becomes the creator; otherwise the creator must be a CCObject whose area (slot 0xA0) is the location's area, or the area itself. Then `Prepare`, vector 0 = the location's position, vector 1 = its orientation (+0x10..+0x18), and the effect is handed to the area's holder, which takes ownership.
- **State:** the area's effects holder; through 0x007A1920 the native type handler for types 1–11 (an AoE, type 5, or a visual effect, type 4, at the vectors: inferred) or APPLY_EFFECT (33) on the area's script for scripted types (per dev/COMBAT.md).
- **Edge cases (checked):** no area at the location, or a creator in another area (or not an object) → the effect is deleted silently, script continues (returns 0). No effect-type or duration-type check here beyond Prepare's normalising. A null effect copy → nothing.

### Engine_ApplyEffectOnParty (148)
- **Handler:** 0x00672200 (own). Calls `Prepare`, `CGameEffect` copy constructor 0x00A65130, `GetObjectById` 0x005D5BE0, holder 0x007A1A20.
- **Does (checked):** pops nDurationType, effect, fDuration, oCreator, nAbilityId, bExcludeCreator (all six, no count check). Party object = `[app+0xD0]+0x58`; `Prepare` once on the popped effect; then for each id of the active party (party +0x234, the followers vector `IsFollower` reads, dev/PARTY.md): skip if no object, skip if id == oCreator and bExcludeCreator ≠ 0; else a new copy goes to the member's holder at +0x70.
- **State:** each active member's effects holder; all copies share one effect id.
- **Edge cases:** no party object → nothing (checked). No dead, area, or object-type check on members (checked); a dead member's acceptance is left to the apply path (inferred). The exclusion compares the creator id as passed, before Prepare's AoE redirect (checked). Members outside the active party (the pool) get nothing.

### IsEffectValid (595)
- **Handler:** 0x0065D290 (own).
- **Does (checked):** returns 1 when the effect copy is non-null, its type (+0x24) ≠ 0, and its invalid word (+0x48) ≠ 1; else 0.
- **State:** none.
- **Edge cases:** an effect marked invalid by `SetIsCurrentEffectValid(FALSE)` or by removal (0x007A0A90 sets +0x48 = 1) reads 0; an empty `effect` variable is a null handle, not a type-0 object (corrected 2026-10-23 by lib-agent-2; only `Effect()` makes a type-0 object: "The null effect handle" below, and GetEffectType gives −1 for it). Values of +0x48 other than 0/1 count as valid (checked; none seen written).

### RemoveEffectsByParameters (593)
- **Handler:** 0x00673940 (own). Builds a filter (a vector of {matcher, value, negate}, 0x00670B20/0x00670A80) and calls the holder's 0x007A0C90.
- **Does (checked):** pops oTarget, nType, nAbilityId, oCreator. Target via `GetGameObjectById` then its SEffectsHolder cast (slot 0x3C). Adds `type == nType` (+0x24, 0x00651810) when nType ≠ 0; `ability == nAbilityId` (+0x44, 0x006517F0) when nAbilityId > 0; `creator == oCreator` (+0x40, 0x006517D0) when oCreator ≠ -1. 0x007A0C90(filter, 0) then adds "duration type ≠ 6, ≠ 5, ≠ 4" (0x00695400 negated) and walks a copy of the holder's list (0x0079F5D0), ending each match through 0x007A0A90: skip if already invalid, set +0x48 = 1, then REMOVE_EFFECT (34) and removal (0x007A08E0; per dev/COMBAT.md).
- **State:** the target's effect list; the target's script runs REMOVE_EFFECT per effect, synchronously.
- **Edge cases:** all filters default → every effect except duration types 4 (equipped items), 5 and 6 (death). A negative nAbilityId means "any" (checked: `jle`). Invalid or holder-less target → nothing, returns 0. Filters are ANDed (inferred from the evaluator 0x00A64770's loop).

### RemoveAllEffects (594)
- **Handler:** 0x00673AB0 (own). Same holder walk 0x007A0C90.
- **Does (checked):** pops oTarget, bIgnoreInjuries, bDeath. With bDeath ≠ 0, keeps only matches where `effects.gda` `IgnoreDeath` of the effect's type is 0 (0x00651830 → 0x00A653A0 → 0x00A65260, column hash 0xB2B84870 = `IgnoreDeath`, table 136). With bIgnoreInjuries ≠ 0, excludes abilities 500001–500099 (+0x44 strictly between 500000 and 500100, 0x00651850 negated). Duration types 4, 5, 6 are excluded as above.
- **State:** as RemoveEffectsByParameters.
- **Edge cases:** even bIgnoreInjuries = FALSE, bDeath = FALSE leaves item (4), type-5 and death (6) effects (checked). Invalid target → nothing.

### RemoveEffectsByCreator (139)
- **Handler:** 0x00674690 (own). Calls `GetObjectById`, `GetAreaById`, the filter builder 0x006741E0, holder 0x007A0C90.
- **Does (checked):** pops oCreator, nAbilityID. Reads the creator's area id at +0x254, walks that area's object list (+0x690); for each object found, filter `creator == oCreator` plus `ability == nAbilityID` when ≠ 0, applied to the object's holder at +0x70 (no cast).
- **State:** effect lists of every object in the creator's area; REMOVE_EFFECT per removed effect.
- **Edge cases:** invalid creator → nothing. **A creator whose +0x254 names no area: the list pointer is null and 0x0067D390 reads [0+4], an access violation** (checked path; whether a reachable case exists is not checked). Effects the creator left on objects in other areas, and on the area's own holder (+0xCC, where `Engine_ApplyEffectAtLocation` puts them), are not removed (checked: only the +0x690 objects' +0x70). Negative nAbilityID is a filter (`≠ 0`), unlike 593. Duration types 4, 5, 6 kept. Objects in the list without a holder at +0x70 would be misread (inferred: the list holds CCObjects).

### SetEffectCreator (613)
- **Handler:** 0x0065C360 (shared with 609, 611, 621; jump table 0x0065C438/0x0065C44C, case at 0x0065C402); calls 0x00A65250.
- **Does (checked):** pops effect and an object (`StackPopObject`, this case only), stores the id raw at +0x40, pushes the copy.
- **State:** the returned copy only.
- **Edge cases:** no validation and no AoE-creator redirect (unlike Prepare, which `Engine_Apply*` run later and which overwrites +0x40 with its own creator argument: checked in Prepare). A null effect → no write, the null is pushed back. Other numbers in the shared range (610, 612, 614–620) push the effect unchanged.

### SetEffectAbilityID (621)
- **Handler:** 0x0065C360 (shared, case at 0x0065C3FD).
- **Does (checked):** pops effect and int; +0x44 = value; pushes the copy.
- **Edge cases:** no range check; `Prepare` overwrites +0x44 with its nAbilityId argument when the effect is applied (checked), so a value set here survives an apply only if passed again (inferred from the scripts' wrappers, not read).

### GetEffectFlags (605)
- **Handler:** 0x0065C270 (shared with 604, 607, 608, 612, 614, 620; table 0x0065C328/0x0065C348, case at 0x0065C2CB → 0x004F6A90).
- **Does (checked):** returns +0x3C.
- **Edge cases:** null effect → -1 (checked: `or eax,-1` default).

### SetEffectFlags (609)
- **Handler:** 0x0065C360 (shared, case at 0x0065C3F3).
- **Does (checked):** +0x3C = value, any int; pushes the copy. Null effect → unchanged.

### GetEffectAnimation (607)
- **Handler:** 0x0065C270 (shared, case at 0x0065C2D2 → 0x00A53690).
- **Does (checked):** returns +0x34. Null effect → -1. The death effect's native handler overwrites +0x34 with the death animation (dev/COMBAT.md).

### SetEffectAnimation (611)
- **Handler:** 0x0065C360 (shared, case at 0x0065C3F8).
- **Does (checked):** +0x34 = value; pushes the copy. Null effect → unchanged.

### GetEffectInteger (683)
- **Handler:** 0x00651270 (shared with 685, 687, 689; `if` chain on the number, case at 0x006512E5); `CDataArrays::GetInteger` 0x00A5A220.
- **Does (checked):** pops effect, then index; returns ints[index] from the list at +0x4.
- **Storage:** a growable std::vector per kind, no fixed count (the ldf says so too); an effect has only as many as were set.
- **Edge cases (checked):** null effect → -1 (as the ldf says); index out of range, negative (unsigned compare), or no list → 0, not -1.

### SetEffectInteger (684)
- **Handler:** 0x00651430 (shared with 686, 688, 690; case at 0x006514A3); `CDataArrays::SetInteger` 0x00A5B710.
- **Does (checked):** pops effect, index, value; creates the list if absent; if index ≥ size (unsigned) resizes to index+1, zero-filling (0x005AE960 → 0x004A66D0(n, 0): inferred std::vector resize); stores; pushes the copy.
- **Edge cases:** index −1 → resize(0), which empties the int list (inferred from the resize call), then the write lands on a static dummy (0x00C2A8D4, checked). Other negative indices → resize to ~4 G entries: an allocation failure (inferred: crash). Null effect → no write, null pushed back (checked).

### GetEffectFloat (685)
- **Handler:** 0x00651270 (case at 0x0065131F); `CDataArrays::GetFloat` 0x00A5A250, list at +0x8.
- **Edge cases (checked):** null effect → −1.0 (constant 0x00AE2E00); out of range → 0.0.

### SetEffectFloat (686)
- **Handler:** 0x00651430 (case at 0x006514E2); `CDataArrays::SetFloat` 0x00A5B7D0. Pops a float (`StackPopFloat` 0x00A2F410).
- **Does:** as SetEffectInteger on the float list (+0x8). Growth and negative-index behaviour the same shape (inferred: the setter not read beyond its address-library note).

### GetEffectObject (687)
- **Handler:** 0x00651270 (case at 0x0065136B); `CDataArrays::GetObject` 0x00A5A2B0, list at +0x10.
- **Edge cases (checked):** null effect or out of range → OBJECT_INVALID (-1). The id is returned as stored; whether the object still exists is not checked.

### SetEffectObject (688)
- **Handler:** 0x00651430 (case at 0x00651524); `CDataArrays::SetObject` 0x00A5B950. Pops `StackPopObject`.
- **Does:** as SetEffectInteger on the object list (+0x10); the id is stored unvalidated (checked: no lookup in the handler). Growth as above (inferred).

### GetEffectString (689)
- **Handler:** 0x00651270 (case at 0x0065139E); `CDataArrays::GetString` 0x00A5A2E0, list at +0x14 of 0x1C-byte std::wstrings.
- **Edge cases (checked):** null effect → empty string (the local is constructed empty and not assigned); out of range → a static empty string (0x00C4C12C).


## Second half's shared facts

Read 2026-10-18 by a subagent of lib-agent-2 in the GOG exe (`game\DAOrigins.exe`, base 0x400000) with `tools\daoexe.py --disasm`, the address library's names, `script.ldf` and the toolset's scripts. No Ghidra, no play. [C] = read in the disassembly; [I] = inferred. Builds on `dev/COMBAT.md` (effects), `dev/PROPERTIES.md`, `dev/ITEMS.md`, and `dev/EFFECTS.md` (on branch gh-181-vfx, commit 78fe841).

Common facts used below [C]:
- `CGameEffect` (0x7C bytes, ctor 0x00A652A0(type)): +0 the script's `CDataArrays` (ints, floats, objects, strings, vectors; `GetEffectInteger` and kin), +0x20 id, +0x24 type, +0x28 duration type, +0x2C duration (ms), +0x30 elapsed, +0x34 animation, +0x38 priority, +0x3C flags, +0x40 creator, +0x44 ability, +0x48 invalid word, +0x4C −1, +0x50 started byte, **+0x54 a second `CDataArrays`, the "engine" parameters**, +0x74 the visual-effect handle (−1 none), +0x78 −1. The ctor zeroes the rest, and for a type ≠ 0 takes the id from a global counter at 0x00C1CF10 (starts at 1, skips −1, post-incremented); a type-0 effect keeps id 0.
- `CDataArrays::Set*` (SetInteger 0x00A5B710, SetFloat 0x00A5B7D0, SetObject 0x00A5B950, SetString 0x00A5BA10, SetVector 0x00A5A9B0): make the list if absent; the index is compared **unsigned**; if index ≥ size it calls resize(index+1) (0x005AE960 → 0x004A66D0, fill 0), then stores; past the size it writes a static scratch word (0x00C2A8D4) instead [C]. So index −1 → resize(0) empties that list and the write is lost [I: resize semantics from the call shape]; index ≤ −2 → resize(≥ 2^31) [I: an allocation failure, a crash].
- An effect on the VM stack is popped as a copy (StackPopEngineStructure, type 3); a "modify and return" command pushes the copy back and deletes its own (vtable slot 0, flag 1) [C]. A failed pop returns −639 (0xFFFFFD81, the script fails); a failed push −638 [C].
- `CClientExoApp::GetObjectById` (0x005D5BE0) answers only `CCObject`s (an area's and the module's slot 0x18 return null); `GetGameObjectById` (0x005D5840) answers areas too [C].
- An object's effects owner is its slot 0x3C: `CCObject` and kin answer this+0x70, `CCArea` this+0xCC, `CCModule` null (0x004137B0) [C]. Its slot 8 gives the effect list (std::vector of `CGameEffect*` at +8/+0xC).

### SetEffectString (690)
- **Handler:** 0x00651430, shared with SetEffectInteger 684, SetEffectFloat 686, SetEffectObject 688 (if-chain on the command number); case at 0x00651551 (cmp 0x2B2). Calls `CDataArrays::SetString` 0x00A5BA10. [C]
- **Does:** pops effect, index, string (a std::wstring local); sets the **script** string list (the effect's +0 `CDataArrays`, +0x14 list), then pushes the effect back as the result. [C]
- **State:** only the popped copy; nothing in the world until the effect is applied. [C]
- **Edge cases:** a null/invalid effect: the string is still popped, nothing is set, the null effect is pushed (no script failure) [C]. Index rules as above (unsigned; −1 empties the string list) [C/I]. An unknown command number in this body only pushes the effect back [C].

### SetEffectEngineInteger (655)
- **Handler:** 0x00660650, shared with 727, 761, 762; two byte-indexed switches on (cmd − 655): pop at 0x006606CE, set at 0x0066076C. Calls `CDataArrays::SetInteger` on **effect+0x54**. [C]
- **Does:** pops effect, index, int; writes the engine int list (+0x54+4); pushes the effect. ldf: write-only from scripts. Known indices from the scripts: 0 `EFFECT_INTEGER_DISABLE_PHYSICS`, 1 `USE_INTERPOLATION_ANGLE`, 2 `EFFECT_INTEGER_VFX` (the type-4 apply handler 0x007A00A0 also writes engine int 2 = its row), `DONT_INTERPOLATE` [C for the scripts and 0x007A00F5; what each native handler reads not read].
- **State:** the popped copy only. [C]
- **Edge cases:** a null effect: value still popped, null pushed back, returns 0 [C]. Locals default to int 0 if the value pop is skipped (it is not, for valid calls) [C]. A failed value pop returns −639 and leaks the copy [C]. Index −1 / negative as in the common facts [C/I].

### SetEffectEngineFloat (761)
- **Handler:** 0x00660650 (as 655); pop case 0x006606EE, set case 0x0066077D → `CDataArrays::SetFloat` on effect+0x54. [C]
- **Does:** writes the engine float list (+0x54+8): e.g. `EFFECT_FLOAT_KNOCKBACK_DISTANCE`, `EFFECT_FLOAT_POTENCY` in the scripts; pushes the effect. [C]
- **State / Edge cases:** as 655 (default local 0.0) [C].

### SetEffectEngineObject (762)
- **Handler:** 0x00660650; pop case 0x0066070E (StackPopObject), set case 0x00660791 → `CDataArrays::SetObject` on effect+0x54. [C]
- **Does:** writes the engine object list (+0x54+0x10); pushes the effect. The id is stored as given: no check that the object exists [C].
- **State / Edge cases:** as 655 (default local OBJECT_INVALID 0xFFFFFFFF) [C].

### SetEffectEngineVector (727)
- **Handler:** 0x00660650; pop case 0x0066072E (StackPopVector), set case 0x006607A2 → `CDataArrays::SetVector` on effect+0x54 (16-byte element, movaps). [C]
- **Does:** writes the engine vector list (+0x54+0x18, {data,size,cap} of 16-byte elements); e.g. `EFFECT_VECTOR_ORIGIN` in the scripts; pushes the effect. [C]
- **State / Edge cases:** as 655 (default local (0,0,0,0)) [C].

### GetHasEffects (177)
- **Handler:** 0x00673D70, shared with GetEffects 712; case at 0x00673F50 (cmp 0xB1). Filter walk 0x00A64D10 → 0x00A64950 on the owner's effect list. [C]
- **Does:** pops object, nEffectType, nAbilityId; an ability of −1 becomes 0 ("any"). With 3 arguments the creator filter is −1 (any), duration type 0 (any), effect id −1 (any). Collects matching effects into a temporary array and pushes `size != 0`. [C]
- **Matching** (0x00A64950, ANDed): effect valid (+0x48 == 0; invalid ones are skipped since the "include invalid" argument is 0); type 0 or +0x24 == type; creator −1 or +0x40; ability 0 or +0x44; id −1 or +0x20; duration type 0 or +0x28. [C]
- **State:** reads only. [C]
- **Edge cases:** an unknown id or the module → 0 [C]. Areas are found (GetGameObjectById) and searched through their +0xCC owner [C]. Placeables, items, triggers, AOEs: their +0x70 owner [C]. Dead creatures are not special-cased [C]. Ability 0 also means "any", so an effect with ability 0 cannot be singled out [C].

### GetVisualEffectLocation (186)
- **Handler:** 0x006646B0 (own). Calls `CDataArrays::GetObject(0)`, `GetAreaById`, `GetVector`, `GetGameObjectById`. [C]
- **Does:** starts from a default `CScriptLocation` (0x007ABD00: position and orientation 0, area −1). If the effect is non-null and type 4: reads the effect's **script object 0**. If that is an area: location = (area, script vector 0 as position, vector 1 as orientation). Else if it is an object: the object's position (+0x120), orientation (+0x130) and area id (+0x254), taken only if the area id ≠ 0. Pushes the location. [C]
- **Where object 0 comes from:** the type-4 apply handler (0x007A00A0) writes script object 0 = the owner's id (owner slot 0x20) when it starts the effect [C]; a location-applied effect evidently gets the area and vectors 0/1 [I: that writer not read]. So it is "where the target is now", not where the effect was cast [C].
- **Edge cases:** a non-VFX effect, a null effect, an effect never applied (object 0 unset → −1), or a destroyed target → the default location (area −1) [C]. An object with area id 0 → default [C].

### GetEffectID (614)
- **Handler:** 0x0065C270, shared with GetEffectType 604, GetEffectFlags 605, GetEffectAnimation 607, GetEffectDurationType 608, GetEffectCreator 612, GetEffectAbilityID 620; byte switch on (cmd − 604), case at 0x0065C2C1. [C]
- **Does:** pops the effect, pushes its +0x20 as an int. [C]
- **State:** none. The id is the ctor's global counter (above); a VM copy keeps it (the copy ctor 0x00A65130 copies +0x20) [C], so ids match between `GetEffects` copies and the applied original, which RemoveItemVisualEffect relies on.
- **Edge cases:** a null effect → −1 [C]. A type-0 effect → 0 [C]. The counter's persistence across saves not read.

### GetEffectsFlags (192)
- **Handler:** 0x00668A20, shared with ~20 object getters (GetActive 141 and kin); case at 0x00668ADA (141 + 0x33). Calls 0x00A64740 on the object's +0x70. [C]
- **Does:** result = owner slot 0xC (base flags) OR the OR of `+0x3C` over every **valid** effect in the list (0x00A646E0, skips +0x48 ≠ 0). For a creature the base is its appearance record (+0x7B0)'s row +0xA0 in the table at 0x00C0CB78 (0x006A9000) [C], inferred `apr_base.DefaultEffectFlags` [I]. Every other class answers 0 (0x004137B0) [C]. Bits: `EFFECT_FLAG_DISABLE_MOVEMENT` 1, TURNING 2, COMBAT 4, TALENTS 8, SPELLS 16, SKILLS 32, ITEMS 64, INPUT 128 (ldf). [C]
- **State:** reads only. [C]
- **Edge cases:** unknown id, an area or the module (GetObjectById null) → 0 [C]. Dead creatures not special-cased [C].

### ApplyEffectVisualEffect (635)
- **Handler:** 0x0065D300 (own; `dev/EFFECTS.md`). Builds `CGameEffect(4)`, sets the creator through 0x0079F410, applies through 0x007A1A20 on target+0x70. [C]
- **Does:** pops creator, target, nVFXId, nDurationType, fDuration, nAbilityId (always 6 pops, no argc check). +0x28 = nDurationType **truncated to a byte**; script int 0 = row; +0x2C = (int64)(fDuration × 1000.0) truncated; creator via 0x0079F410 (an area-of-effect creator is replaced by its own +0x4AC, the AOE's creator); +0x44 = ability. No `CGameEffect::Prepare`, no object-type filter (unlike ApplyEffectOnObject 597). [C]
- **Apply:** 0x007A1A20 → manager 0x007A1920 → the native type-4 handler 0x007A00A0: refuses a second start (+0x74 ≠ −1 → 3); innate (5) on a creature whose death effect is in place → nothing; an instant (1) one is refused when the owner's +0x154 record's state +0x150 > 2 (0x007C4A60; state's meaning not read); else engine int 2 = row, handle → +0x74, +0x50 = 0, script object 0 = owner id. [C]
- **Edge cases:** target not a `CCObject` (unknown id, area, module) → the effect is deleted, nothing happens [C]. Any `CCObject` kind is accepted at this level (placeable, item, trigger, waypoint, AOE, projectile) [C]; whether the holder's slot 0x6C refuses some is not read. Creator unknown → stored as given [C]. Duration types other than 1/2/3/5 are stored as-is [C].

### AddItemVisualEffect (780)
- **Handler:** 0x0065D4A0 (own). Calls 0x0079F410, 0x007A1A20, `ScriptBackend::ForScript_AddItemVisualEffect` 0x00A64900. [C]
- **Does:** pops target, nVFXId. Makes `CGameEffect(4)`, +0x28 = 3 (permanent), script int 0 = row, creator = the target itself. Target must be an item (slot 0x14, `CCItem` answers this). If the item's owner +0x4A8 ≠ −1 (equipped: `dev/ITEMS.md`) it is applied at once through the item's +0x70 holder (the same type-4 path as 635). Otherwise it goes to the item's pending record (+0x158): appended to its list (+4); its +0 keeps the valid effect of highest priority (0x00A65380: +0x38, or 999 for duration type 6) if > 0. [C]
- **State:** the item's effect list or its pending list (+0x158). That the pending list is applied on equip is inferred [I].
- **Edge cases:** not an item (creature, placeable) or unknown id → the effect is deleted, nothing [C]. No check that the row exists [C]. A VFX's +0x38 is 0, so it never becomes the pending +0 entry [C].

### RemoveItemVisualEffect (782)
- **Handler:** 0x006747F0 (own). Filter `ScriptBackend::MakeEffectFilter` 0x006741E0 + 0x00670A80, removal 0x007A0C90 on item+0x70. [C]
- **Does:** pops target, nId. Target through 0x005D5870 (an item only, by the type byte +0x14). Builds a filter of: type == 4 (matcher 0x00651810 on +0x24) AND **effect id == nId** (matcher 0x006517B0 on +0x20) — nId is an effect id (as `GetItemVisualEffectsIDs` 781 returns), not a VFX row. Sets the owner's byte item+0x75 to 1 around the call. Removes through 0x007A0C90(filter, 0), which also excludes duration types 4, 5, 6 (matcher 0x00695400), then 0x007A0AF0 (REMOVE_EFFECT and the type-4 remove handler 0x007A0150, which stops the VFX handle) [C for the filter; the walk per `dev/COMBAT.md`].
- **State:** the item's effect list; its pending list (+0x158) is not touched [C], so an effect added while unequipped is not removed by this [I: if that list is what equip applies].
- **Edge cases:** not an item / unknown id → nothing [C]. An id that matches nothing → nothing [C]. Item effects with duration type 4 (from equipping) are never removed here [C].

### RemoveAllItemVisualEffects (783)
- **Handler:** 0x006748E0 (own). [C]
- **Does:** pops target (an item via 0x005D5870); filter type == 4 only; item+0x75 = 1; 0x007A0C90(filter, 0) on item+0x70; +0x75 = 0. Every type-4 effect of duration type 1/2/3 on the item goes. [C]
- **State / Edge cases:** as 782: non-items and unknown ids do nothing; duration types 4–6 kept; the pending list untouched [C].

### EffectImpact (834)
- **Handler:** 0x00662A00 (own). [C]
- **Does:** pops fDamage, oWeapon, nVfx, nAbi, nDamageType (always 5 pops). Makes `CGameEffect(1024)` (`EFFECT_TYPE_IMPACT`): script float 0 = damage, object 0 = weapon, int 0 = VFX, int 1 = ability, int 2 = damage type; duration type stays 0. Creator = the caller (OBJECT_SELF, `this`+8 of the command implementer) through 0x0079F410, only when that id resolves to a game object. Pushes it. [C]
- **State:** none: it is a value; `combat_h`'s `Combat_HandleAttackImpact` reads float 0, object 0, ints 0–2, and is not applied as such [C, scripts].
- **Edge cases:** the weapon is not checked [C]. Note int 2 is the damage type here, not a VFX; if applied, `0x0079FC10`'s "int 2 VFX" (COMBAT.md) would read it [I: whether that reads script or engine int 2 not checked].

### EffectModifyProperty (835)
- **Handler:** 0x00662BB0, shared with 847 (cmp 0x34F picks the type). [C]
- **Does:** pops nProperty0, fChange0, nProperty1, fChange1, nProperty2, fChange2 (always 6). `CGameEffect(1012)` (`EFFECT_TYPE_MODIFY_PROPERTY`). Script int 0/float 0 always written; int 1/float 1 only if nProperty1 > 0; int 2/float 2 only if nProperty1 > 0 **and** nProperty2 > 0. Creator = OBJECT_SELF as in 834. Pushes it. [C]
- **State:** none until applied; APPLY_EFFECT then runs `Effects_HandleApplyEffectModifyProperty`, which calls 854 [C, scripts].
- **Edge cases:** property 0 is stored even if 0 or out of range [C]. `EFFECT_MODIFY_PROPERTY_ATTRIBUTE_ALL` (−1000) is legal only in slot 0 (slots 1–2 need > 0) [C].

### EffectModifyPropertyHostile (847)
- **Handler:** 0x00662BB0 (as 835); the type branch at 0x00662C72. [C]
- **Does:** identical to 835 but type **1058** (`EFFECT_TYPE_DECREASE_PROPERTY`). The values are not negated or changed; "hostile" is only the type, which `effects_h` routes to the same handler [C, scripts]. [C]
- **State / Edge cases:** as 835 [C].

### ApplyEffectModifyProperty (854)
- **Handler:** 0x0066B3C0, shared with 855 (cmp 0x357 negates). Calls `CDataArrays::GetInteger/GetFloat` (0..2) and 0x0071B2D0 (add to the MODIFIER, `dev/PROPERTIES.md`). [C]
- **Does:** pops the effect; only types 1012 and 1058 proceed. The creature is **OBJECT_SELF** (the implementer's +8, cast by slot 0x10), not the effect's target; stats = creature+0x7C4. For p0, then p1 if ≠ 0, then p2 if p1 and p2 ≠ 0: property −1000 → the change to properties 1–6 (STR, DEX, WIL, MAG, CUN, CON) each; else to that property. Each call keeps the current (`GetCurrent`, slot 7 += value, then 0x00719A00 re-sets the current, which for 7/8 clamps and notifies). [C]
- **State:** the creature's property modifiers (+0x54 of each property object). [C]
- **Edge cases:** wrong effect type, null effect, OBJECT_SELF not a creature (a placeable's or the module's script), or no stats → nothing, returns 0 [C]. No check of +0x48 validity, of death, or of the effect being on that creature [C]. An unknown property id: behaviour of `CreatureProperties::GetCurrent` then not read. Applying twice adds twice [C].

### RemoveEffectModifyProperty (855)
- **Handler:** 0x0066B3C0 (as 854); negation at 0x0066B4BE. [C]
- **Does:** as 854 with fChange0..2 negated (fchs), so it subtracts what 854 added, on OBJECT_SELF. [C]
- **State:** the modifiers. A depletable whose total drops has its current clamped down; raising it back does not refill (`dev/PROPERTIES.md`) [C].
- **Edge cases:** as 854; nothing records that the effect was applied, so an unmatched remove subtracts anyway [C].

### GetItemVisualEffectsIDs (781)

- **Handler 0x00673BE0, `int[] GetItemVisualEffectsIDs(object oTarget)`** (script.ldf:6746, `COMMAND_GETITEMVFXS`; read 2026-10-23 by lib-agent-2 with a subagent) [C]:
  - **What it returns:** for an item (0x005D5870), the ids (+0x20) of the type-4 effects in the item's record at `+0x158` (its list at +4), as an array. A non-item or an item with no record gives an empty array. It changes nothing.
  - **The ids** are what `RemoveItemVisualEffect` (782) takes.
  - **Equipped items:** an equipped item's effect from 780 goes to the `+0x70` holder instead, so it may not be listed [I].
  - No shipped script calls it.

### SetPhysicsController (839)
- **Handler:** 0x0066BE50, shared with 296, 328, 329, 333, 335, 642, 643, 645, 676, 698, 758, 764, 805, 813 (backend list: `CCCreature::ForScript_SetItemDroppable`); byte switch on (cmd − 0x2FC), case at 0x0066C01D. [C]
- **Does:** pops object, bEnable (default 0 if only one argument). Needs a creature (slot 0x10). Sets a creature local int (key {0, 0xB} into the variable table at creature+0x2E0, `ForScript_SetLocalInt`) to !bEnable [C], inferred `PHYSICS_DISABLED` (var_creature; `den_functions_h` reads it) [I]. Enable: if creature+0x770 (the physics controller) is null, builds it (0x006B1ED0(−1, −1)); then 0x0069EBB0(1). Disable: 0x0069EDC0 destroys the controller (slots 0x48/0x44/0, then +0x770 = 0), then 0x0069EBB0(0). [C]
- **State:** +0x770 controller, +0x778 enabled flag (0x0069EBB0 writes it, then 0x0069EB60 sets the controller's slot 0x2C to 0 when enabled and creature byte +0x1AC is set, else 1) [C]; the local int [C].
- **The create flag** (read 2026-10-23 by lib-agent-2 with a subagent) [C]:
  - **The call:** 0x0066C046 is in command 839's handler (`SetPhysicsController`, 0x0066BE50). It calls `ForScript_SetLocalInt` (0x00703DB0) with key {0, 0xB}, value `!bEnable` and create 0.
  - **The finder:** 0x00703BF0 adds a record only on a miss with create set (0x00703C08).
  - **So 839 never creates `PHYSICS_DISABLED`:** it writes it only where the creature already has that variable, and builds or removes the controller either way.
  - **Engine writers that create it** (create 1): 0x007BC4DB and 0x007F457A, and the template loader 0x00704240.
- **Edge cases:** non-creatures (placeable, item) and unknown ids → nothing [C]. The build (0x006B1ED0) is skipped unless the owner's slot 0x6C allows, the local int is 0, the creature is alive (or dead with 0x006A3A80 == 2), 0x0079FD80(0) is false and the creature has a model (slot 0xA0) [C in outline]; so enabling a dead or modelless creature only sets the flag [I].

## Not read
- What the native type handlers (types 1–11, e.g. AoE type 5 on an area holder) do with an effect applied at a location; the inside of 0x007A1920 beyond dev/COMBAT.md.
- 0x0064B900 (hostility) and the creator's slot 0x10 it is fed; the spatial query 0x00686F70 and its entry flags; what creature stats +0x68 is.
- The RNG 0x004C0030/0x004E7050.
- `CDataArrays::SetFloat/SetObject/SetString` bodies (assumed SetInteger's shape) and the exact resize helper 0x004A66D0.
- The filter evaluator 0x00A64770 in full (AND-with-negate inferred) and 0x007A08E0 (removal itself).
- Whether `GetCurrentEffect` with no current effect pushes a null or a type-0 effect.
- None of this was seen in play.
- The native consumers of the engine parameters (+0x54) per effect type (which index each handler reads), and the writer of a location-applied VFX's object 0 and vectors.
- Whether the id counter 0x00C1CF10 is saved and restored with a game.
- What applies an unequipped item's pending visual effects (item+0x158) on equip; the item+0x75 byte's role.
- The effects holder's slot 0x6C (which object kinds take native effects); the instant-VFX state at +0x154/+0x150.
- `CreatureProperties::GetCurrent` / slot 7 with an unknown property id.
- The physics controller's class and its slot 0x2C; the {0, 0xB} variable key's form.
- GetItemVisualEffectsIDs (781) is read (above).

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not followed to the end. The shared facts of `dev/script_commands/effect_access.md` apply (an effect on the stack is a copy; `CGameEffect` is 0x7C bytes; failed pop -639, failed push -638). One more, checked: **"OBJECT_SELF resolves to a game object" is `CClientExoApp::GetGameObjectById` 0x005D5840** (a lookup in the table at `app+0xCC`; it answers null for an unknown id); the constructors below set the creator only then, and otherwise leave the constructor's -1.

### Engine_ApplyEffectOnObject (597)
- **Handler:** 0x0065CA20 (not shared). Pops nDurationType, the effect (structure 3), oTarget, fDuration, oCreator and, only when paramCount >= 6, nAbilityId (else 0).
- **Described in:** `dev/COMBAT.md` ("`Engine_ApplyEffectOnObject` pops ..."; the death effect and slot 0x168/0x16C paragraph) and `dev/script_commands/effect_access.md` (`CGameEffect::Prepare`).
- **Does:** `Prepare(effect, durationType, duration, creator, ability)`, then hands the effect to the target's holder at `target+0x70` (0x007A1A20) and always returns 0. For a creature-type-7 effect it first clears an immortal creature (COMBAT.md). (checked)
- **Edge cases:** **an object of any other kind is refused silently.** The target is found with 0x005D5BE0 and must have the type byte (+0x14) 4 (area), 6 (creature), 10 (placeable) or 12 (area of effect); items, triggers, stores, the module and a missing object get the effect deleted, nothing applied, and the script goes on with 0. A null effect copy returns 0 at once. The check is the only one: the duration type is normalised by `Prepare` (not here), and a dead target's acceptance is left to the apply path (`Effects_HandleApplyEffect` in the scripts). (checked)

### Effect (19)
- **Handler:** 0x0065C460 (not shared). Pops nType.
- **Described in:** `dev/script_commands/effect_access.md` (the `CGameEffect` constructor, 0x00A652A0, and its id counter); no note describes the command itself.
- **Does:** allocates 0x7C bytes, constructs `CGameEffect(nType)`, sets the creator to OBJECT_SELF when that is a live game object (0x00A58190 lookup, then 0x0079F410, the area-of-effect redirect), pushes it (copied) and deletes the local. (checked)
- **Edge cases:** no check of nType: `Effect(0)` (EFFECT_TYPE_INVALID, the default) is a type-0 effect that has **no id** (the counter is read only for a non-zero type) and reads as invalid in `IsEffectValid`; any number, native or script type, is accepted. Duration type 0, priority and the rest are the constructor's. If the allocation failed the constructor's `this` is null and the code goes on to push it (not checked there). (checked)

### EffectAreaOfEffect (634)
- **Handler:** 0x00662610 (not shared). Pops nId, the resource rScript and, only when paramCount > 2, nAoEVfx (default 0).
- **Described in:** `dev/TURN_BASED.md` (the gas trap uses it) and `dev/NCS.md` (which commands strip a script name's extension: this one was listed as not read).
- **Does:** builds `CGameEffect(5)` (the area-of-effect type): **script int 0 = nId**, **script string 0 = rScript with the first `.ncs` removed** (a first-occurrence `replace` of the 4-character constant 0x00B00A38 `.ncs` by the empty string 0x00ADEF14, `std::wstring` 0x004C2FC0; which constant is the pattern and which the replacement is inferred from the order they are built in), creator = OBJECT_SELF set with the plain store 0x00A65250 (no area-of-effect redirect, unlike `Prepare`) when OBJECT_SELF is a game object, and **engine int 2 (EFFECT_INTEGER_VFX, the +0x54 arrays) = nAoEVfx** when it is above 0. Pushes it. (checked apart from the one inference)
- **Edge cases:** the duration, and the area it makes, come from how it is applied (`Engine_ApplyEffectOnObject` or `...AtLocation` with `Prepare`, then the effect handler for type 5). Any `.ncs` inside the name goes, not only a trailing one; `.nss` or other extensions stay, and no stem cutting at the first `.` happens (unlike `HandleEvent`). The nId is not validated against `persistent.gda`. The script name is the AoE's own script (the AoE heartbeat, `dev/TURN_BASED.md`). (checked)

### RemoveEffect (598)
- **Handler:** 0x00674590 (not shared). Pops oTarget, then the effect (structure 3). Calls 0x005D5840 on the target, the object's slot 0x3C (its effects holder), `CGameEffect` matcher builder `ForScript_RemoveEffect` 0x006741E0, the holder's remover 0x007A0C90.
- **Described in:** `dev/COMBAT.md` ("REMOVE_EFFECT is sent on `RemoveEffect` and on expiry"; the 0x007A0C50 holder walk).
- **Does:** builds a matcher on the effect's **id (+0x20)** with function 0x006517B0 and removes what it matches from the target's holder, which sends EVENT_TYPE_REMOVE_EFFECT (34) per COMBAT.md; so it removes the applied effect, and any copies, that share the id with the one the script holds. Returns 0 (void). (checked up to the matcher; the matching itself inferred)
- **Edge cases:** **an invalid target faults.** The handler calls slot 0x3C on 0x005D5840's answer with no null test (0x00674612), so an unknown id is an access violation at address 0 in the script thread, not a quiet no-op. A holder that is null (the object keeps none) and a null effect do nothing. **An effect whose own duration type (+0x28) is 5 or 6 is never removed** (checked: both skip to the free), so effects of those duration types (6 is one `Prepare` lets through; 5 it turns into 3 on apply, so only a copy that was never applied can hold it) cannot be removed by this command. A type-0 effect (no id) matches id 0 (inferred). The popped copy is freed. (checked)

### GetEffectType (604)
- **Handler:** 0x0065C270, shared with 605, 607, 608, 612, 620 (and the others in `effect_access.md`); the case for 604 is the first jump-table entry (`mov eax,[ecx+0x24]`, 0x0065C2B7).
- **Described in:** `dev/COMBAT.md` (`Effects_HandleApplyEffect` switches on it) and `dev/PROBE.md` (checked in play against the library's accessor).
- **Does:** pops the effect and pushes its type word (+0x24) as an int. (checked)
- **Edge cases:** **a null effect gives -1** (default `eax = -1`), not 0, so a script that tests `GetEffectType(...) == EFFECT_TYPE_INVALID` (0) is not true of an empty value. The handler frees the copy. A type-0 effect from `Effect()` gives 0. (checked)

### GetEffectCreator (612)
- **Handler:** 0x0065C270 (shared), case at 0x0065C2D9 (`mov eax,[ecx+0x40]`); the push is `StackPushObject` 0x00A2D980, not the int one, since command number 0x264 is tested at 0x0065C2E2.
- **Described in:** `dev/script_commands/effect_access.md` (the creator field and `Prepare`'s area-of-effect redirect).
- **Does:** pushes the creator id (+0x40). The value is -1 for an effect that was never applied with a creator, else what `Prepare` stored: the creator, or for an area-of-effect creator its own creator (+0x4AC). (checked)
- **Edge cases:** null effect: -1, which is OBJECT_INVALID as an object. The id is not checked: a creator that has since been destroyed is returned all the same, so a script must test it. (checked)

### GetEffectDurationType (608)
- **Handler:** 0x0065C270 (shared), case at 0x0065C2BC (`mov eax,[ecx+0x28]`).
- **Described in:** `dev/script_commands/effect_access.md` (`Prepare`: duration type 1-4 or 6 kept, anything else 3 PERMANENT).
- **Does:** pushes the duration type (+0x28) as an int. (checked)
- **Edge cases:** null effect: -1. An effect not yet applied has whatever the constructor left (not read; probably 0), and after an application it is `Prepare`'s normalised value, so a script reads the normalised type, not the one it passed. (checked for the read; the constructor's value not read)

### GetEffectAbilityID (620)
- **Handler:** 0x0065C270 (shared), case at 0x0065C2C6 (`mov eax,[ecx+0x44]`).
- **Described in:** `dev/script_commands/effect_access.md` (`SetEffectAbilityID`; `Prepare` writes the ability at +0x44).
- **Does:** pushes the ability id (+0x44) as an int. (checked)
- **Edge cases:** null effect: -1. The constructor leaves 0, and applying overwrites the word with the ability passed to the apply command (default 0), so an effect read after application shows the apply's value. (checked)

### GetCurrentEffect (681)
- **Handler:** 0x006604D0 (not shared). Looks up OBJECT_SELF with 0x005D5840 and reads the event being run: a game object with a CCObject view (slot 0x18) -> `+0x2A8`; type byte 4 (area) -> through slot 8, `+0x3C4`; type 3 (module) -> through slot 0x1C, `+0x390`. If that event's type (+0x20) is 33 or 34 it takes its effect 0 (0x0069BDB0).
- **Described in:** `dev/COMBAT.md` ("`GetCurrentEffect` (681) and `SetIsCurrentEffectValid` (682) use the event the calling object is running"; that note gives the area's and the module's offsets the other way round: the code has area +0x3C4, module +0x390, as `dev/script_commands/events.md` has them).
- **Does:** pushes a copy of the effect the APPLY_EFFECT or REMOVE_EFFECT event carries. (checked)
- **Edge cases:** no OBJECT_SELF object, no event running, or an event of any other type: **a null effect is pushed** (the `edi = 0` default, pushed through structure 3; a null copies to null, per events.md), which `GetEffectType` reads as -1 and `IsEffectValid` as 0. This answers the open question in `effect_access.md`'s "Not read". It is a copy, so changing it does not touch the event's own effect. (checked; that the push of a null stays null is inferred)

### SetIsCurrentEffectValid (682)
- **Handler:** 0x00660580 (not shared). Pops nValid (the handler's own default is 0; scripts pass TRUE by default), finds the event the same way as `GetCurrentEffect`, takes the **effect inside the event itself** (0x0069BDB0, not a copy) and calls 0x00424030 with `(nValid == 0)`.
- **Described in:** `dev/COMBAT.md` ("`SetIsCurrentEffectValid(nValid)` writes `!nValid` into the effect's invalid word, +0x48").
- **Does:** writes `+0x48 = (nValid == 0)` on the live effect: any non-zero nValid makes it valid (0), 0 makes it invalid (1). `IsEffectValid` and the apply path read it afterwards. Returns 0. (checked)
- **Edge cases:** it changes the effect in the event in flight, unlike every `SetEffect*` command, which work on copies; the result outlives the script. No OBJECT_SELF, no event, an event whose type is not 33 or 34, or no effect: nothing happens. Only the value 0 invalidates. (checked)

### EffectDamage (833)
- **Handler:** 0x00662870 (not shared). Pops fValue, nDamageType, nFlags and nImpactVFX, always all four (no count test; each starts at 0).
- **Described in:** `dev/COMBAT.md` (`Combat_HandleAttackImpact` applies the damage; "`EffectDamage`, 0x00662950-0x00662974" for the impact VFX in script int 2) and the `effect_damage_h` paragraph.
- **Does:** builds `CGameEffect(0x3F5)` (1013, EFFECT_TYPE_DAMAGE, not native): **float 0 = fValue, int 0 = nDamageType, int 1 = nFlags, int 2 = nImpactVFX**, creator = OBJECT_SELF when it is a game object, and pushes it. (checked)
- **Edge cases:** no validation of any argument: a negative damage is stored as given (the script handler, not the engine, decides what it does), the damage type is not checked against the damage table, and the duration type stays 0 (the apply command sets it). The engine VFX int (EFFECT_INTEGER_VFX) stays unset, so the impact VFX is played by the scripts, not the engine (COMBAT.md). (checked)

## Not read
- The matching in 0x007A0C90 / 0x00670A80 beyond the matcher built (so exactly which effects `RemoveEffect` removes when copies differ in other fields), and what the holder does to the effect afterwards.
- The `CGameEffect` constructor's values for the duration type and priority (only the id and type words were read).
- Whether a null effect on the stack survives `StackPushEngineStructure` as null (assumed from `events.md`).
- `CGameEffect::Prepare`'s handling here beyond `effect_access.md`'s account.

## Read for DAOParagonEngine #169 (read 2026-10-22 by paragon-agent-2's helper)

- **An effect's flags are two stores.** `CGameEffect+0x3C` is the script-visible flags word (GetEffectFlags 605 returns it; GetEffectsFlags 192 ORs it over valid effects, 0x00A64740). The "spawned dead" mark of a SimulateDeath effect is NOT in that word: `CDataArrays::SetFlag` (0x00A5B890, called as 0x00A5B890(4, 1) on the effect, comment in COMBAT.md:534) makes the flag list at `CDataArrays+0xC` on first use (0x10 bytes) and writes the DWORD at index 4 of that list with 1, growing the vector to index+1 when short, and writing the static dummy 0x00C2A8D4 when out of range (0x00A5B8F4-0x00A5B938) [C]. The test `HasSimulateDeath(flagOnly=1)` (0x0079FD80) is a matcher list: type == 0x415 (0x00651810), and for flagOnly the matcher 0x0079F2E0 with value 4, and invalid word == 0 (0x006953E0) (list built at 0x0079FDC0-0x0079FE1A) [C; the body of 0x0079F2E0 is not read]. So a creature spawned dead (CREATURE_SPAWN_DEAD = 2, 0x006BF710) has an effect whose flags word `+0x3C` stays 0 and does not make GetEffectsFlags answer DISABLE_SPELLS. The mark is in a separate list on the effect, not a bit of its flags word. [C]
- **GetEffectsFlags base flags are non-zero for five appearances.** `apr_base.DefaultEffectFlags` on this install (core rows, add-in rows unreadable): ID 10 Broodmother 3, 66 Dragon High 3, 73 Broodmother Tentacle 1, 82 Archdemon (Wounded) 3, 89 Archdemon 3; every other row 0. 3 = DISABLE_MOVEMENT | DISABLE_TURNING. The exe reads it at 0x006A9000 from `[creature+0x7B0]` row +0xA0 of the table at 0x00C0CB78 (effect_access.md:174 [C]); that the column is DefaultEffectFlags is the note's [I] (the value pattern, 3 for the big monsters and 1 for the tentacle, fits DISABLE_MOVEMENT|TURNING). [I for the column, C for the read]
- **The death effect's apply removes exactly the effects that pass three ANDed matchers.** Handler 0x007A12E0, after the APPLY_EFFECT run (0x0079FE90) and if the effect is still valid (+0x48 == 0, 0x007A1378), builds ONE matcher list (0x007A137D-0x007A13D2): `0x006741E0(0x00695400, 5)`: duration type (+0x28) == 5 (INNATE); `0x00670A80` (a push_back of a 0xC-byte matcher {fn, value, expected byte}) with `0x00651810, 4`: type (+0x24) == 4 (EFFECT_TYPE_VISUAL_EFFECT); and `0x006953E0, 0`: invalid word (+0x48) == 0. The evaluator `0x00A64770` walks the list and returns false at the first matcher whose result equals its expected byte (0), true if none does: an AND [C: 0x00A64770-0x00A6482F]. The walk is `0x0079F5D0(holder, list, 0x007A0C50)`: each effect that passes is removed with REMOVE_EFFECT (0x007A0C50). So: innate AND visual-effect AND still valid. An effect already invalid is not removed. [C]
- **The events of APPLY_EFFECT and REMOVE_EFFECT carry no creator and no target.** 0x0079FE90 (APPLY_EFFECT, type 0x21, set at 0x007A005A) and 0x007A0840 (REMOVE_EFFECT, type 0x22, 0x007A08A6) allocate a CScriptEvent (0x0069BAC0), whose constructor sets creator (+0x24) and target (+0x28) to -1 (`or eax,-1` at 0x0069BAEE, stores 0x0069BB03/0x0069BB06), set the type, put the effect in slot 0 with `0x0069D120(0, effect)` and run it through the target's vtable slot 0x28 [C]. The effect's creator is not copied to the event. The event's creator is OBJECT_INVALID. Whether the runner (slot 0x28) fills the event's target before the script runs is not read [I]; scripts read the effect's creator with GetEffectCreator on the effect.
- **Event 0x1D (SetIndividualImpactAOEEvent) delivery.** aoe.md:35-38 has the command. The handler `CCObject::EventHandler` 0x00698A50, case 0x1D at 0x00698B2D, stores the event at `+0x2A8` and falls to 0x00698BCC, which reads the event's string 0 and runs that script on the object (0x00A5A2E0, 0x00A31E70) [C]. The handler tests neither switched-off event types nor listeners [C]. What the queue (0x0064ACE0) or `ProcessEvent` (0x0064A8C0) tests before calling the handler is not read [I]. A negative delay stays as aoe.md:38 has it ("passed as unsigned milliseconds ... wraps", inferred).
- **A landed script projectile's impact event has int 1 = -1, not 0.** 0x007E87C0 (EVENT_TYPE_ATTACK_IMPACT 29 from a projectile): `0x007E68A0` gives the attack record `0x00697220(0)` of the projectile's record list at `+0x488`; a new record is `{0, -1, -1}` (0x00697254-0x00697264). The event: type 0x1D (29) (0x007E8829), creator the projectile's id (0x53cc40 on `this+4`, 0x007E8835), object 0 = `[+0x4C4]` (the event receiver), int 0 = `[record+0]`, int 1 = `[record+8]`, int 2 = `[projectile+0x4B0]` (0x007E8838-0x007E886C), vector 0 the projectile's position (0x007E887C), vector 1 the orientation from 0x007E6D80, objects 1 and up from the hit list (0x007E88C1-0x007E8904); queued with `AddEventDeltaTime(0, 0, ...)` (0x007E8935), and a second pass skipped when the result `[record]` is 0, 3 or 5 (0x007E8942-0x007E894C) [C]. A script projectile (FireProjectile, 0x00661C70) writes none of the record, and `+0x4B0` starts 0 (0x007E853A), so the ints are [0, -1, 0]. [C; I that nothing in 0x00661C70 touches the record, that handler was not re-read for it]
- **Item visual effect on an unequipped item: pending list +0x158 applied on equip** stays [I] (effect_access.md:187, :246 open). A scan of the exe for `[reg+0x158]` was too noisy to find the equip reader; the equip path in ITEMS.md is where to look next.
- **The ghost.** The type-7 apply 0x007A12E0 has no ghost test (read whole), and the sub-action 51 handler 0x007AAC10 (the ExecuteEffect step that plays the die clip and raises DEATH, COMBAT.md:376-379) makes no ability or trait query in the calls of its first 400 instructions (effect-array getters, matcher builders, 0x007A0C90 removal, event construction). So the exe queues the death command for a ghost too, and sub-action 51 would raise DEATH as for any creature, on top of the script's DYING/DEATH sends for a ghost (`effect_death_h`, COMBAT.md:369). [I: the tail of sub-action 51 was not read]
- **The null effect handle.** RSADD of structure 0x13: `0x00A30700` -> 0x00A2D920 (`CVirtualMachine::StackPushEngineStructure`), which asks the game's structure handler at `vm+0x1968` (vtable slot 0x20, (type, arg)) for the object and stores the result on the structure stack; with no handler it returns 0 (0x00A2D923-0x00A2D973) [C]. What the handler returns for an effect (a type-0 effect or null) is not read [I]; effect_access.md:35 (an empty effect variable is type 0) and :284 (a null gives -1) disagree about which an empty variable is.
- **The factory returns null for effect and itemproperty** (read 2026-10-23 by lib-agent-2 with a subagent) [C]:
  - **The value:** the VM pushes a null pointer. RSADD 0x00A30700 → 0x00A2D920(type − 0x10, 0) → the structure factory slot 8 (0x00674AF0 on the script VM), which returns 0 for effect (3) and itemproperty (4) with argument 0 (0x00674C5B).
  - **Where a typed object comes from:** only `Effect()` makes a type-0 object.
