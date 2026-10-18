# Script command handlers: spells_combat (DAOParagonEngine #27)

Read 2026-10-18 in the GOG exe (`game\DAOrigins.exe`, base 0x400000) by lib-agent-2's subagent, with `tools\daoexe.py --disasm` and scratch call/xref scans; no Ghidra, no game. "Checked" = read in the disassembly; "inferred" = a reading of purpose. Shared idioms (checked): `CVirtualMachine::StackPopObject` 0x00A2F6D0, `StackPopInteger` 0x00A2F3C0, `StackPopFloat` 0x00A2F410, `StackPushInteger` 0x00A2D650, `StackPushFloat` 0x00A2D690; a failed pop returns -639, a failed push -638. `CClientExoApp::GetCreatureById` 0x005D58C0 (null for anything not a creature), `GetObjectById` 0x005D5BE0. "The party" is `[[app+0xD0]+0x58]` (`CCCreature::GetParty` 0x006A2CC0, also `0x005D57C0`); `[[app+0xD0]+0x5C]` is the controlled creature's id (`0x005D5CA0`; `0x005D5CC0` returns that creature).

### GetWeaponStyle (354)
- **Handler:** 0x00668A20, the shared object getter (switch on the command number; 354 = 0x162 at 0x00668B20 → 0x00668B9D) → `0x006A3C30` (CCCreature: `[+0xC60]` combat data, `+0x5C`). Checked.
- **Does:** returns the combat data's style int, set by 0x007C3600 (called from the creature at 0x006B8DD9) when its combat animation set changes. Checked.
- **Mapping (checked):** set `OW.anb` (one weapon, id 0) → 1, `WS.anb` (weapon and shield, id 1) → 1, `DW.anb` (id 3) → 2, `TW.anb` (id 2) → 3, any other set → 0. The records are static 0x00C38C40/7C/B8/F4, built at 0x00AD3000–0x00AD3200. This matches script.ldf: 0 none, 1 single (with or without shield), 2 dual, 3 two-handed. Bow, staff and unarmed give 0 (inferred: theirs are other sets).
- **Edge cases:** invalid object or non-creature → 0 (checked: falls through every cast). A creature without combat data (`+0xC60` null) → 0 (checked). No dead or mode check.

### SetAbilityResult (715)
- **Handler:** 0x0065E050 → `CCObject::GetCurrentCommand` 0x0073C5F0 → the cast record `0x006A0180` (creature+0xB54 holder, its +8 a lazily allocated 0x50-byte record, ctor 0x00696690). Checked.
- **Does:** copies the creature's current command's parameters into the record: +0 the command's int 0 (the ability), +4 its object 1, +0x3C its object 0, +0x10..+0x18 its vector 0 (16 bytes copied, so +0x1C is uninitialized stack). Then it writes the arguments: +8 = nResistanceCheckResult, +0x40 = nProjectileTarget. Checked.
- **Readers:** the cast impact (0x006AF430) raises ABILITY_CAST_IMPACT with int 0/int 1 = record +0/+8 (`dev/ABILITIES.md`). The launch (0x006B6F60, command type 25) copies the caster's record into the projectile (+0x488 holder) and resets the caster's (0x00695420). Checked; the purpose of +0x40 there is inferred.
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
