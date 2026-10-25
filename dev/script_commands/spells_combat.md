# Script command handlers: spells_combat (DAOParagonEngine #27)

Read 2026-10-18 in the GOG exe (`game\DAOrigins.exe`, base 0x400000) by lib-agent-2's subagent, with `tools\daoexe.py --disasm` and scratch call/xref scans; no Ghidra, no game. "Checked" = read in the disassembly; "inferred" = a reading of purpose. Shared idioms (checked): `CVirtualMachine::StackPopObject` 0x00A2F6D0, `StackPopInteger` 0x00A2F3C0, `StackPopFloat` 0x00A2F410, `StackPushInteger` 0x00A2D650, `StackPushFloat` 0x00A2D690; a failed pop returns -639, a failed push -638. `CClientExoApp::GetCreatureById` 0x005D58C0 (null for anything not a creature), `GetObjectById` 0x005D5BE0. "The party" is `[[app+0xD0]+0x58]` (`CCCreature::GetParty` 0x006A2CC0, also `0x005D57C0`); `[[app+0xD0]+0x5C]` is the controlled creature's id (`0x005D5CA0`; `0x005D5CC0` returns that creature).

### GetWeaponStyle (354)
- **Handler:** 0x00668A20, the shared object getter (switch on the command number; 354 = 0x162 at 0x00668B20 → 0x00668B9D) → `0x006A3C30` (CCCreature: `[+0xC60]` combat data, `+0x5C`). Checked.
- **Does:** returns the combat data's style int, set by 0x007C3600 (called from the creature at 0x006B8DD9) when its combat animation set changes. Checked.
- **Mapping (checked):** set `OW.anb` (one weapon, id 0) → 1, `WS.anb` (weapon and shield, id 1) → 1, `DW.anb` (id 3) → 2, `TW.anb` (id 2) → 3, any other set → 0. The records are static 0x00C38C40/7C/B8/F4, built at 0x00AD3000–0x00AD3200. This matches script.ldf: 0 none, 1 single (with or without shield), 2 dual, 3 two-handed. Bow, staff and unarmed give 0 (inferred: theirs are other sets).
- **Edge cases:** invalid object or non-creature → 0 (checked: falls through every cast). A creature without combat data (`+0xC60` null) → 0 (checked). No dead or mode check.

### SetAbilityResult (715)
- **Handler:** 0x0065E050 → `CCObject::GetCurrentCommand` 0x0073C5F0 → the cast record `0x006A0180` (creature+0xB54 holder, its +8 a lazily allocated 0x50-byte record, ctor 0x00696690). Checked.
- **Does:** copies the creature's current command's parameters into the record: +0 the command's int 0 (the ability), +4 the command's object 0 (the target) and +0x3C its object 1 (the item) (corrected 2026-10-24 by lib-agent-2: they were swapped; see below), +0x10..+0x18 its vector 0 (16 bytes copied, so +0x1C is uninitialized stack). Then it writes the arguments: +8 = nResistanceCheckResult, +0x40 = nProjectileTarget. Checked.
- **Readers:** the cast impact (0x006AF430) raises ABILITY_CAST_IMPACT with int 0/int 1 = record +0/+8 (`dev/ABILITIES.md`). The launch (0x006B6F60, command type 25) copies the caster's record into the projectile (+0x488 holder) and resets the caster's (0x00695420). Checked; the purpose of +0x40 there is inferred.
- **The two objects, and the record's defaults and reset** (read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2) [C]:
    - Object 1 is fetched at 0x0065E101 (`push 1`) and stored to +0x3C (0x0065E15E). Object 0 is fetched at 0x0065E10F and stored to +4 (0x0065E14A).
    - The impact 0x006AF430 agrees: it uses +4 as the target and passes +0x3C to 0x006A0DA0 as the item. This matches ABILITIES.md's "object 1 the item, object 2 the target".
  - **The record's defaults** [C]: its constructor 0x00696690 and reset 0x00695420 set +0 to 0, +4 to −1, +8 to 0x80000000, +0xC to 1, the vector to 0, +0x20 to an empty wstring, +0x3C to −1 and +0x40 to 0.
  - **Reset at every command's end** [C]: the shared end 0x00746AB8–0x00746ABF calls 0x006A0180, then the reset 0x00695420.
    - Added (read 2026-10-24, checked by lib-agent-2's subagent) [C unless marked]: the reset is inside the shared end's branch for a creature owner. Its only other caller is the launch 0x006B6F60 (0x006B72B0), for a command of type 25, after the record is copied to the projectile.
    - The end handlers that 0x00747480 puts in its +0x58 array (the starts are in +0x54) are either 0x00746900 itself or handlers that call it: 0x00746B30, 0x00746BA0 (UseAbility), 0x00746D60, 0x00746DD0, 0x00746E10, 0x00746E70, 0x00746E80, 0x00747250 (ExecuteEffect), 0x007472E0 (37, call at 0x0074731F), 0x00747330 (42, call at 0x00747354) and 0x00747360 (the deathblow) [I for every early exit of 0x00747360; for 0x00747250 settled to [C] 2026-10-24 by lib-agent-2: every path reaches EndShared 0x00746900, a null creature jumping to 0x007472CB, ANIMATION.md "The end"].
- **Edge cases:** pops object, int, then a third int only when given (arg count > 2), else +0x40 = 0 (checked). Not a creature, or no current command → returns **1** with nothing written (checked; positive, so not a script failure). No range check on either int.

### SetCreatureRank (766)
- **Handler:** 0x00664520 → creature attribute at `+0x4B0` (the template's `Rank`, `dev/PROPERTIES.md`), its vtable slot +0x54 (set) with nRank. Checked.
- **Does:** sets the rank attribute. Inferred: a SmartData attribute set, which notifies its observers. No engine recalculation follows in the handler (checked).
- **Edge cases:** non-creature or invalid → no-op (checked). No range check (checked). No dead check.

### GetCanDiePermanently (184)
- **Handler:** 0x006645A0 → `CCParty::IsInPartyPool` 0x006316F0. Checked.
- **Does:** returns 1 iff the creature's `+0xBD8` is 0 **and** it is not in the party pool. If there is no party (`app+0xD0` or its +0x58 null), the answer is just `+0xBD8 == 0`. Checked. `+0xBD8` is the template's `NoPermDeath` (inferred: the template load reads it next to `Combatant` +0xBD4).
- **Edge cases:** invalid object or non-creature → 0 (checked). Dead state is not consulted.

### SetLoadHint (322)
- **Handler:** 0x00660EE0 → GUI scene 14 (`load_town`, the loading screen; `app+0x158` slot +0x60) → `GdaStore::FindTable` 0x004DF580 on table n2DAReference (default 0xCE = 206 `loadhints`) → table slot +0x28 (row nLoadHint, column hash 0x406341BE = `StrRef`) → `ScriptBackend::ForScript_SetLoadHint` 0x00652730. Checked.
- **Does:** builds a localized-string value from the StrRef and stores it in the loading scene's attribute at scene+0xC0, then notifies its observers (slot +0x24). Checked; that the movie shows it is inferred.
- **Edge cases:** no loading scene, no such table, or no such row or column → nothing (checked). The row is looked up by the table's own lookup (by ID or index, not read). Any table with a `StrRef` column works.

### SetLoadImage (361)
- **Handler:** 0x0064E450. Checked.
- **Does:** **nothing.** It pops the string, frees it and returns 0. A stub in 1.05 (checked).
- **Edge cases:** only a failed pop returns -639.

### SetControllable (814)
- **Handler:** 0x00660FF0 → `CCCreature::ForScript_SetControllable` 0x006A5F40 (bool = nIsControllable != 0). Checked.
- **Does:** writes the byte `+0x7F9` (controllable). On false it also removes the creature from the party's selection (0x00639DB0 on the vector of ids at party+0x184, signalled through +0x194; if the selection had at most one id it re-picks via 0x00636CD0(1)). If the party then has no controlled creature (`[[app+0xD0]+0x5C] == -1`), 0x00636C00 makes the first party member in party+0x238 that is controllable and alive (slot 0x1D0 false) controlled, through 0x006368F0(id, 1, 1), or -1 if none. Checked; the slot meanings are inferred.
- **Edge cases:** non-creature or invalid → no-op (checked). True does not select it. No party → only the byte is written. `dev/COMBAT.md` calls 0x00639DB0 "ForScript_SetPartyPlotFlag", but it reads as deselect(id).

### SetPrimaryControlled (815)
- **Handler:** 0x00661050 → SetControllable(creature, 1) 0x006A5F40, then `0x006368F0(id, 0, 1)` on the party (makes it the controlled creature and clears the selection at party+0x184 first). Checked; "controlled" is inferred from its use.
- **Does:** makes the follower controllable and, if it is alive (vtable slot 0x1D0 answers 0), the controlled creature. Checked.
- **Edge cases:** no party → no-op (checked, before the lookup). **An invalid or non-creature object crashes:** the handler calls 0x006A5F40 with a null `this` before its null check, writing to address 0x7F9 (checked, 0x00661092–0x0066109D). A dead follower is made controllable but not controlled (checked).

### DEPRECATED_GetCreatureStrengthModifier (700)
- **Handler:** 0x0066A3B0, shared with 693, 699, 701 and 703 (jump table at 0x0066A578, byte index at 0x0066A590). Case 700 is 0x0066A490. Checked.
- **Does:** computes nothing. It returns **0** if the creature's stats attribute (`+0x7C4`) is present, else **-1**. Checked. (`dev/script_backends.csv` lists 0x006A8680/86F0/8770 against it; those are the other cases' backends.)
- **Edge cases:** invalid or non-creature → -1 (checked). Only the object is popped.

### CharGen_ClearAbilityList (320)
- **Handler:** 0x0066A0D0 → the creature's stats (`+0x7C4`) → `ScriptBackend::ForScript_CharGen_ClearAbilityList` 0x0071F160 (nAbilityType, default 0). Checked.
- **Does:** for each type in range, takes that type's list (0x007199F0(type)) and removes its first ability until the list is empty, through 0x0071EF00(id, type, 0, 1) (inferred: RemoveAbility's remover; it is also called from the Has/Add/RemoveAbility handler 0x00669AB0). Checked.
- **Range (checked):** 0 (ABILITY_TYPE_INVALID) → types 1, 2, 3 (talent, spell, skill); 1–3 → that one; **4 (ITEM) → nothing**, and 4 is skipped even under 0; any other value (negative or ≥ 4, compared unsigned) → nothing.
- **Edge cases:** non-creature → no-op (checked). A creature whose stats attribute is null would pass null to the backend (no check, checked). There is no chargen-mode check.

### EnableWeaponTrail (185)
- **Handler:** 0x0065E420 → `CCCreature::ForScript_EnableWeaponTrail` 0x006A01B0 (bool = bEnable != 0). Checked.
- **Does:** gets the creature's model through its +0x70 interface's slot +0x60. If there is one, it writes the byte `+0xB6D` = bEnable and tail-calls the model's slot +0x54(bEnable) (inferred: the trail's on/off). Checked.
- **Edge cases:** no model (not loaded or not drawn) → nothing at all, and the flag is not kept (checked). Non-creature → no-op.

## Not read
- The SmartData attribute's set (slot +0x54) behind SetCreatureRank, and whether any observer reacts to the rank.
- Which file the module load's `GroupMasterlist` GFF3 comes from (`[module+0x18]`).
- The `+0x4E8` object's slot +0x54 (the hostile-to-party flag's consumer) and the model's slot +0x54 behind EnableWeaponTrail.
- Whether `GdaTable` slot +0x28 takes nLoadHint as a row index or a row ID; what the loading movie does with the hint.
- 0x0071EF00's full removal (effects, quickbar) for CharGen_ClearAbilityList; 0x006368F0's whole body beyond the selection clear.
- `+0xBD8` as `NoPermDeath`: the template load's write was not read.
- What fills record +0x40 (nProjectileTarget) downstream in the projectile.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by lib-agent-2's subagent (b4b) in `game\DAOrigins.exe` (GOG, base 0x400000) with `tools\daoexe.py --disasm`. Shared facts as in the group's file: a failed pop returns -639, a failed push -638; `GetCreatureById` 0x005D58C0 (null for a non-creature), `GetObjectById` 0x005D5BE0. The combat data is `[creature+0xC60]`. "Checked" = read in the disassembly, "inferred" = not.

### GetAttackTarget (522)
- **Handler:** 0x00655000, shared with `GetArea` (223); the case is `cmp edi,0x20A` at 0x00655061. It calls `GetObjectById`, then the object's cast slot +0x10 (creature).
- **Described in:** `dev/script_commands/objects.md`, "GetArea (223)" for the shared handler; its use in `dev/TURN_BASED.md` ("Commands that ran only in combat") and `dev/PARTY.md` ("The AI and the controlled creature").
- **Does:** pushes the object id at combat data +4. (checked)
- **Who writes it** (read 2026-10-21 by lib-agent-2, a sweep of .text; checked): [creature+0xC60]+4 is an object id, −1 for none, and no script command sets it.
  - The attack step 0x00742FD0 (0x00743038) writes the attack's target.
  - The UseAbility step 0x00743F50 (0x00744204) writes the ability's target (command object 0), or −1 for a ground target. A self-cast keeps the old value. It is written once the command's checks pass, before the approach and conjure.
  - The party's group-attack click (0x00712C05) and the cursor-target sender (0x00712DE0, 0x00712ED4) write the clicked target into each selected member.
  - −1 comes from the constructor (0x007C37FB), from leaving combat (0x006ABC1C), from `CCCreature::Update`'s check 0x007C3490 (target gone or dead), and from UseAbility's end (0x00746D32) for a creature with +0xC95 set that is out of combat.
  - A hit received, death, ClearAllCommands, an attack's end and an area change do not touch it. The handler 0x00655000 does not test for a dead target. Not read: Update's guard bytes +0xC94 and +0xBD4; whether a save keeps it.
- **Edge cases:** an invalid id, a non-creature (not a creature by the cast) and a creature with no combat data all push OBJECT_INVALID (-1, the default in esi). (checked) A creature whose +4 was never set pushes whatever it holds (-1 for none is inferred). Push failure -638.

### GetAttackImpactDamageEffect (695)
- **Handler:** 0x0065E180 (not shared). It calls `GetCreatureById` and the combat data's 0x007C3770 (a find by id in the list at combat +0x6C, then 0x004E9640 on combat +0x68). (checked)
- **Described in:** `dev/COMBAT.md`, "The attack command": "fetches the kept effect".
- **Does:** pops the attacker and the effect id, finds the copy `SetAttackResult` kept for that id, and pushes it as an engine structure (type 3). After the push the handler deletes its own copy (vtable slot 0 with 1). (checked)
- **Edge cases:** a non-creature attacker, or an id with no kept effect, pushes a null effect (esi 0). (checked) That the find also takes the entry out of the kept list, so a second call with the same id answers null, is inferred from the second call (0x004E9640) on the list; not confirmed. There is no null test on the combat data: a creature without one would fault (inferred). Push failure -638 leaves the copy undeleted.

### SetAttackResult (691)
- **Handler:** 0x0065DE40 (not shared). It calls `GetCreatureById`, the current command (0x0073C5F0: `[[creature+0x2DC]]`), `CDataArrays::GetObject` (0x00A5A2B0) and the hand records 0x006A0160 / 0x006A0170 (creature +0xB54, index 0 and 1). (checked)
- **Described in:** `dev/COMBAT.md`, "The attack command" (what each hand's record gets).
- **Does:** pops the attacker, then (int result, effect) twice. For each hand it stores the result at +0, the current command's object 0 at +4 (the target), and, unless the result is 3 (miss) and the effect is non-null with its +0x24 non-zero, the effect's id (+0x20) at +8 and a 0x7C-byte copy of the effect kept in the combat data (0x007C3840). (checked)
- **Edge cases:** a non-creature attacker, or an attacker with no current command, does nothing but still frees both popped effects. (checked) A command whose object 0 is missing (the read answers -1) also stops it before either hand is written, so neither hand gets its result. (checked) A miss leaves that hand's effect id as it was. The popped effect copies are always freed. Returns 0. No test of the combat data (inferred fault if null).

### SetCombatState (525)
- **Handler:** 0x0065DD60 (not shared). It calls `GetCreatureById`, then `CCCreature::SetCombatState` 0x006AE0A0. (checked)
- **Described in:** `dev/ANIMATION.md`, "The combat state (checked)".
- **Does:** pops the creature and the int and passes the int unchanged. The native stores it at combat +0 only when it differs from the stored one; a change runs the enter (1) or leave (any other value) routine and queues the draw or sheathe. (checked)
- **Edge cases:** a non-creature does nothing. Any int is accepted and stored: only 1 counts as "in combat" for the routines, so 2 stores 2 and runs the leave routine. (checked) Returns 0.

### GetCombatState (526)
- **Handler:** 0x0065DDD0 (not shared). `GetCreatureById`, then `[[creature+0xC60]]`. (checked)
- **Described in:** `dev/SCRIPT_COMMANDS.md`, "Fields" (combat state at combat +0); written by `SetCombatState` (`dev/ANIMATION.md`).
- **Does:** pushes the int at combat data +0. (checked)
- **Edge cases:** a non-creature pushes 0. A creature with no combat data would fault (no null test, inferred). Push failure -638.

### GetWeaponAttackRating (693)
- **Handler:** 0x0066A3B0, shared with 699, 700, 701 and 703; the case is index 0 of the jump table at 0x0066A578 (id - 0x2B5), body 0x0066A424. (checked)
- **Described in:** `dev/ITEMS.md`, "The four weapon commands (one implementation)"; and its "The weapon style, and GetWeaponStrengthModifier" (read 2026-10-18 by lib-agent-2), which found 693/703 push the default -0.1 for an invalid attacker.
- **Does:** pops the attacker and the hand flag, takes the hand's weapon and pushes the float of `bitm_base` `CombatRating` (0x006A8680). Exactly 1 selects the main hand, any other value the off hand. (checked)
- **Edge cases:** a non-creature attacker pushes -0.1 (the float at 0x00B0839C). A creature with no weapon in that hand pushes 0.0 (the backend starts at 0 and the item lookup fails). (checked) Failed pops -639, a failed push -638.

### GetCreatureRank (183)
- **Handler:** 0x0066BB30 (not shared). `GetCreatureById`, then 0x0053CC40 on creature +0x4B0 (an attribute: its int is at +0xC, so the field is +0x4BC). (checked)
- **Described in:** `dev/COMBAT.md`, "A placed creature's load" (Rank is read from the template at 0x006C088F and nothing derives from it).
- **Does:** pushes the creature's Rank int (RANK_* from the template).
- **Edge cases:** a non-creature pushes 0. No range check on the value. (checked) Push failure -638.

### GetCombatantType (763)
- **Handler:** 0x00664640 (not shared). `GetCreatureById`, then creature +0xBD4. (checked)
- **Described in:** `dev/PROPERTIES.md`, "Load from a template, and from a save" (the template's `Combatant`); `dev/ITEMS.md` ("0x006A88F0, step by step": 1, non-combatant, is never scaled).
- **Does:** pushes the template's `Combatant` int (CREATURE_TYPE_*).
- **Edge cases:** a non-creature pushes 0, the same as a plain combatant. (checked) In play it answered 0 only (`dev/LIBRARY.md`).

### ShowDeathScreen (771)
- **Handler:** 0x00660D60 (not shared). It calls the party (0x005D57C0), the party's 0x0062EBA0, then the game mode controller (`[app+0x114]`) 0x006414A0 (2, 0, 0). (checked)
- **Described in:** `dev/COMBAT.md`, "GM_DEAD" (it sets `CapturedOnDeath` and `SetGameMode(2)`).
- **Does:** pops an int; 1 passes true as "captured". It always sets the game mode to dead (2) after that. (checked)
- **Edge cases:** any value but 1 means not captured. There is no check that the party is dead or that the mode is not already 2. Returns 0; a failed pop -639.

### SetDeathHint (321)
- **Handler:** 0x00660DC0 (not shared). (checked)
- **Described in:** `dev/COMBAT.md`, "GM_DEAD" (step 5: `GUIDeath.DeathHint` comes from `SetDeathHint`).
- **Does:** pops the hint row, then, only if given (param count of at least 2), the table: it defaults to 205 (0xCD). It asks the GUI manager (`[app+0x158]`, slot +0x60) for scene 0x13, `GUIDeath`, then the GDA table with that id: it reads the row's string column (hash 0x406341BE) and sets that text on the scene's DeathHint field through the scene's object at +0xC0, slot +0x54. (checked up to the lookup; the column's name is inferred)
- **Edge cases:** if scene 0x13 does not exist, the table does not, or the row has no such string, nothing happens (no script failure). Returns 0.

### GetWeaponBaseDamage (699)
- **Handler:** 0x0066A3B0, shared; the body is at 0x0066A455. (checked)
- **Described in:** `dev/ITEMS.md`, "The four weapon commands (one implementation)".
- **Does:** pushes the **int** of `bitm_base` `BaseDamage` for the hand's weapon (0x006A86F0), 1 selecting the main hand.
- **Edge cases:** a non-creature attacker pushes -1 (an int, not the float default of its siblings). A creature with no weapon, or a base item with no such column, pushes 0. (checked)

### GetWeaponStrengthModifier (701)
- **Handler:** 0x0066A3B0, shared; the case is at 0x0066A4B6. (checked)
- **Described in:** `dev/ITEMS.md`, "The four weapon commands (one implementation)"; and its "The weapon style, and GetWeaponStrengthModifier" (read 2026-10-18 by lib-agent-2), which found it never reaches the column.
- **Does:** always pushes the default -0.1.
- **Edge cases:** the case pops only the weapon hand flag and then looks up the creature `-1` (always null), so its `StrengthModifier` backend (0x006A8770) is never called. (checked) The attacker is popped but unused. This differs from script.ldf's description. Failed pops -639.

### GetWeaponArmorPenetration (703)
- **Handler:** 0x0066A3B0, shared; the case is at 0x0066A4EC. (checked)
- **Described in:** `dev/ITEMS.md`, "The four weapon commands (one implementation)"; and its "The weapon style, and GetWeaponStrengthModifier" (read 2026-10-18 by lib-agent-2).
- **Does:** pops the attacker, then the target, then the hand flag. The result is `bitm_base` `ArmourPenetration` of the hand's weapon, from 0x006A87E0 (the target is passed). (checked)
- **Edge cases:** a non-creature attacker **or** a non-creature target pushes -0.1. A missing weapon pushes 0.0. (checked)

### SpawnBodyBag (672)
- **Handler:** 0x0065E3A0 (not shared). `GetCreatureById`, then `CCCreature::SpawnBodyBag` 0x006A91E0. (checked)
- **Described in:** `dev/COMBAT.md`, "The body bag".
- **Does:** pops the creature and, if given (param count over 1), bForce; calls the native with `bForce != 0`. (checked)
- **Edge cases:** a non-creature does nothing. Default `bForce` is 0, so a living creature, one without loot and no bag yet, or a pooled creature gets no bag (the native's tests, COMBAT.md). The handler returns 0.

### SetCreaturesGlobalMaxTimeBeforeDecay (702)
- **Handler:** 0x00665630 (not shared). The case is `cmp [esp+8],0x2BE` at 0x00665655. (checked)
- **Described in:** `dev/COMBAT.md`, "The body bag": the corpse timer is counted against `[0x00C021C4]`, 60000 ms by default.
- **Does:** pops an int and stores it at `[0x00C021C4]`.
- **Edge cases:** no range check: 0 or a negative value is stored as is (a decay at the next check is inferred). The value is a global, not per creature. It is the engine's static, so a save does not keep it (inferred). Returns 0.

### SetBodybagDecayDelay (698)
- **Handler:** 0x0066BE50, shared with 296, 328, 329, 333, 335, 642, 643, 645, 676, 758, 764, 805, 813 and 839; the case is `cmp ebx,0x2BA` at 0x0066C125. (checked)
- **Described in:** `dev/COMBAT.md`, "The body bag" (it calls the corpse's decay delay, 0x0069F360, on the bag's creature).
- **Does:** pops the object and, if given, the delay in ms (otherwise 0). The object must pass the placeable cast (slot +0x24) with `IsBodyBag` (+0x600) set. It then gets the creature at +0x664 and calls 0x0069F360(delay). (checked)
- **Edge cases:** an invalid object, a placeable that is not a body bag, a bag whose creature is gone, and any non-placeable do nothing. The delay is not range-checked. With the argument omitted the delay is 0. Returns 0. (checked)

## Not read
- Who writes combat data +4 (the target `GetAttackTarget` reads), and when it is cleared.
- `0x004E9640` and `0x007C3840`: whether a fetch removes the kept effect, and how long a kept effect lives.
- `0x006A0160`/`0x006A0170` (hand records) for a creature with no combat data.
- The bodies of `0x0062EBA0` and `0x006414A0` (`ShowDeathScreen`), and the table lookup and scene call at the end of `SetDeathHint`.
