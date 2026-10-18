# Script commands, group "stats": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not.

Shared facts (checked unless marked):
- Handlers are `__thiscall (commandId, paramCount)`, `ret 8`; a failed pop returns -639, a failed push -638 (`dev/script_commands/commands.md`). Pops: `StackPopObject` 0x00A2F6D0, `StackPopInteger` 0x00A2F3C0; pushes: `StackPushInteger` 0x00A2D650, `StackPushFloat` 0x00A2D690. `GetObjectById` 0x005D5BE0 (any `CCObject`), `GetCreatureById` 0x005D58C0 (null for a non-creature).
- The property store, its four classes, their slots (total +0xC, base +0x14, modifier +0x18, current +0x24, SetBase +0x10, SetCurrent +0x2C), the clamps and the engine-link formulas are `dev/PROPERTIES.md`. Here: health is property 7, mana and stamina are **one** property, 8 (`Mana_Stamina`, DEPLETABLE, Min 0, Max 9999); health is Min 1, Max 9999.
- **Mana vs stamina by class: there is none.** 238/242, 239/243, 240/244 and 241/245 are each one handler (0x0066CC30, 0x0066CD00, 0x0066CDC0, 0x0066CE90) that never reads its command id: the "mana" and "stamina" commands are the same command on property 8 (checked).
- Each of these handlers reaches the property object directly: `stats = [creature+0x7C4]+0xC` (0x0053CC40), the vector at stats+0x190, entry 7 or 8 (0x00642E20), then the property's own slot. They do **not** go through `CCCreatureStats::Set*` (0x0071B230 and kin), so `0x00719A00`'s change notification (the bars, per PROPERTIES.md) is not called (checked: no such call).
- Float to int is `0x008E34F0` (the CRT's float-to-int, truncation toward zero: inferred `_ftol2`).
- The health commands go through `CCObject` virtual slots: +0x120 GetHealth, +0x124 SetHealth (`SetPlaceableHealth` 234), +0x128 GetMaxHealth, +0x12C SetMaxHealth (checked in the vtables of CCObject 0x00B0EE9C, CCCreature 0x00B10484, CCPlaceable 0x00B1C084, CCItem, CCTrigger, CCWayPoint, CCAreaOfEffectObject, CCSoundObject, CCVisualEffect).

### GetHealth (233)
- **Handler:** 0x0065A1E0 (not shared). Calls `GetObjectById`, then the object's slot +0x120 (checked).
- **Does:** pushes the object's current health as an int; -1 when the id is not an object (checked).
- **State:** creature (0x006AB4B0): property 7's CURRENT, truncated; 0 if the creature has no stats or no property 7 (checked). Placeable (0x00750F50): the int at +0x66C (checked).
- **Edge cases:** item, trigger, AoE, sound, visual effect: -1 (CCObject's 0x004E13D0). **A waypoint answers 1** (its slot is 0x0079B310, `mov eax,1`, the same folded body as an attribute's type getter) (checked). Areas and modules are not CCObjects found by `GetObjectById` (inferred), so -1.

### Deprecated_GetMaxHealth (235)
- **Handler:** 0x0065A2C0 (not shared). `GetObjectById`, then slot +0x128 (checked).
- **Does:** pushes the maximum health; -1 for an invalid object (checked).
- **State:** creature (0x006AB530): property 7's TOTAL (base + CON link + modifier, clamped 1..9999), truncated; 0 with no stats (checked). Placeable (0x00750F60): the int at +0x670 (checked).
- **Edge cases:** every other object type, waypoints included, answers -1 (0x004E13D0) (checked). `core_h.GetMaxHealth` uses it only for placeables and reads creatures with `GetCreatureProperty` (read in the scripts).

### SetMaxHealth (236)
- **Handler:** 0x0065A330 (not shared). Pops the object, then the int; `GetObjectById`, then slot +0x12C with the value **sign-extended from its low 16 bits** (`movsx … word`) (checked).
- **Does, placeable** (0x00750F70): stores the value at +0x670. The current health (+0x66C) is not touched or clamped (checked). `placeable_h` uses it at spawn for a placeable whose max is ≤ 1.1, from `placeable_types` `Health` (read in the scripts).
- **Does, creature** (0x006AB5B0): **sets the current health, not the maximum.** It converts the value to float and calls property 7's slot +0x2C, the depletable's SetCurrent (0x0079C8D0: clamp(value, +0x78, TOTAL)); the base and modifier are untouched. Then it writes 1 to the byte stats+0x1B0 (meaning not read). With no property 7 it only writes that byte; with no stats nothing (checked). This differs from script.ldf ("Set object's maximum health").
- **Edge cases:** every other type: no-op (0x00455850, `ret 4`) (checked). An invalid object does nothing. Values outside -32768..32767 wrap (checked). Returns 0.

### HasDeathEffect (326)
- **Handler:** 0x00668A20, shared with about fifteen object queries that return an int (`cmp` chain on the command id, then a jump table at 0x00668CE8/0x00668CFC); this command's case is 0x00668B85. Calls `GetObjectById`, the object's slot +0x10 (as a creature), and 0x0079F4A0 (checked).
- **Does:** pops the object and, if `paramCount` > 1, `bCheckForDeathEvent` (default -1 when absent: a compiled call always passes it). For a creature, 0x0079F4A0 on its effects holder (creature+0x70) with `bCheck > 0`: it takes the holder's slot 1 (0x0069A0D0: the pointer at holder+0xE8, i.e. creature+0x158) and its first dword as an effect; TRUE when that effect exists, its duration type (+0x28) is 6 (`EFFECT_DURATION_TYPE_DEATH`) and its invalid word (+0x48) is 0 (checked). That holder+0xE8 holds the creature's current death effect is inferred.
- **With bCheckForDeathEvent:** also requires the effect's second data array (+0x54), its list at +0xC (the one 0x00A5B890 writes), entry 3, to equal 1 (0x00A5A280) (checked). The type-7 apply handler 0x007A12E0 writes entry 3 of the **int** list (+4) of that array for an unsynced death (`dev/COMBAT.md`, "The death effect"), a different list (checked), so what sets the tested entry was not found.
- **Edge cases:** invalid object or non-creature: 0 (checked: `esi` is zeroed before the lookup and no other branch handles 326). Dead by health alone (no death effect) answers 0.

### GetCreatureAttribute (248)
- **Handler:** 0x0066D0A0 (own wrapper); it calls 0x0066CF60, the same pop-and-read helper as `GetCreatureProperty` (738, 0x0066D0E0), and pushes the float truncated to int (checked).
- **Does:** pops creature, property, value type (default 1 TOTAL in the helper too); value types 1–4 read TOTAL/BASE/CURRENT/MODIFIER through `CreatureProperties::GetTotal` 0x006435C0, `GetBase` 0x006436B0, `GetCurrent` 0x006437A0, `GetModifier` 0x00643890 (jump table 0x0066D090) (checked). Values per class: PROPERTIES.md.
- **Edge cases:** non-creature, creature with no stats, or a value type outside 1..4: -1 (the helper's default, -1.0 at 0x00AE2E00) (checked). CURRENT of a non-depletable: -1 (PROPERTIES.md). A property id not in the store: 0 (0x006436A1 `fldz` for TOTAL, checked; the others inferred). Ids 1000–1999 index a second per-creature list (stats+0x1A0), ids ≥ 2000 a **global** list at 0x00C3896C (checked in 0x006435C0). Truncation: 12.9 → 12, -0.5 → 0.

### GetCreaturePropertyType (739)
- **Handler:** 0x0066D120 (not shared). `GetCreatureById`, stats, then 0x0066C770 on stats+0x190 (checked).
- **Does:** finds the property object by id (the same three ranges as `GetTotal`) and returns its vtable slot 0: attribute 1 (0x0079B310), simple 2 (0x004CCAE0), depletable 3 (0x004CE240), derived 4 (0x00A11120) (checked). So it is the class the store built from `properties.gda`'s `Type`, not the table re-read.
- **Edge cases:** invalid or non-creature, no stats, or an id the store does not have: 0 (`PROPERTY_TYPE_INVALID`) (checked). The `oCreature` argument matters only for having a store; the answer is the same for every creature (inferred).

### ClearCreatureProperties (333)
- **Handler:** 0x0066BE50, shared with other `(object[, int])` void commands (328, 329, 758, …); case at 0x0066BF3D. Calls `GetObjectById`, slot +0x10 (as a creature), then 0x00795C10 on stats+0x190 (checked).
- **Does:** 0x00795C10 walks the main property list (ids < 1000) and, for each, sets value = max(Min, 0) clamped to Max through slot +0x10 (SetBase) then slot +0x2C (SetCurrent, a no-op for non-depletables) (checked). It is the same reset the template load does (PROPERTIES.md).
- **State:** bases and depletables' currents only. **Modifiers (+0x54) are kept** (checked: no call to the modifier slots). Ids ≥ 1000 are untouched. No event, no `0x00719A00` notification (checked).
- **Edge cases:** script.ldf says "Health 1, all else 0": the engine gives each property max(Min, 0), so also DamageScale (12) 1 and Healing_Effect_Factor (51) 100 (`properties.gda` Min). Non-creature or invalid: nothing (checked). The stats pointer is not null-checked before `+0x190` (checked; a creature always has stats, inferred).

### IsAbilityAvailable (610)
- **Handler:** 0x0065A690 (not shared). `GetCreatureById`, then 0x0069FE20 (the stats, but null unless stats+0x1B4 is set), then `CCCreatureStats` 0x0071BEF0(ability) (checked).
- **Does:** TRUE when all hold (checked): the ability's record exists (0x0062C730, the cached `ABI_base` row); the creature's level (property 15, read through 0x00630460) ≥ the record's +0x178 (`PrereqLevel`); the creature has the record's +0x168 (`PrereqAbility`) if non-zero (0x0071BEC0); the TOTAL of property +0x188 (`PrereqAttribute`, via 0x0071A230) ≥ +0x198 (`PrereqAttValue`) if the attribute is non-zero; and, for a talent (`AbilityType` 1) whose `GUIType` row in `guitypes` (table 111) has `CombatTraining` 1 (Archery, Dual Weapon, Weapon and Shield, Two Handed), the record's +0x1F8 ≤ the combat-training rank (4 if it has 100103, 3 for 100102, 2 for 100101, 1 for 100100, else 0). The record offsets' column names follow the load order in 0x0062C82C (inferred); +0x1F8's column was not found.
- **Edge cases:** it does not check that the creature already has the ability (checked). Invalid/non-creature or stats+0x1B4 null (no interface ability holder; inferred: creatures without the party's ability UI) → 0. Unknown ability → 0. **The push's failure is ignored** on the main path (returns 0 regardless, checked at 0x0065A709).

### GetAbilityCount (615)
- **Handler:** 0x00668A20 (shared, see HasDeathEffect), case at 0x00668BD4: creature → stats → 0x0071A2F0(nType, nGUIType) (checked).
- **Does:** the stats keep four ability vectors, 16 bytes apart: +0x150 talents (type 1), +0x160 spells (2), +0x170 skills (3), +0x180 item abilities (4) (0x007199D0, checked). nType 0 sums all four; nType 1–4 counts one; **any other nType counts the talent list** (0x007199D0's fallback) (checked). nGUIType 0 counts all; otherwise only abilities whose record (0x0062C730) has `GUIType` (+0x1B8) equal to it; an id with no record is not counted (checked).
- **Edge cases:** non-creature or invalid: 0 (checked). An omitted argument defaults to -1 in the engine (talent list, GUI type -1 matches none), not script.ldf's 0; a compiled call always passes both (inferred). `sys_chargen_h` calls it with GUIType 18 (Traits).

### RemoveAbility (254)
- **Handler:** 0x00669AB0, shared with `HasAbility` (252) and `AddAbility` (253), switching on the command id (case 254 at 0x00669B86). Calls `GetCreatureById`, the data-table int read 0x00A2CCC0 (`ABI_base`, column `AbilityType`, hash 0xE1A8F4EA), then stats 0x0071EF00(ability, type, 0, 3) (checked).
- **Does:** for an ability > 0, a creature, an `ABI_base` row, and `AbilityType` ≠ 4: 0x0071EF00 returns if the creature lacks it (0x0071BEC0), else removes its passive (0x0071B600: `Passive_ID` → table 0x75), its cooldown/GUI record (0x0071CA00 → 0x0071C7E0), erases it from its type's vector (`vector::remove` 0x004E9640), recomputes the party for a follower (0x00631830/0x00631F30), drops a reference on the GUI type's blend tree (`guitypes` `BlendTree`, a count map at stats+0x108; at 0 the creature unloads it, 0x006B5B30(2, …)), and takes it out of the interface's ability instances (stats+0x1B4, 0x0062CF60), then 0x0071E0D0 (checked in outline; the last unread).
- **Edge cases:** **an item ability (AbilityType 4) is silently not removed** (checked, as `AddAbility` refuses it). Ability ≤ 0, unknown id, invalid or non-creature: nothing. No event (inferred: 0x0071A450's event 0x49 is built only for type 4, checked at 0x0071A486). Returns 0.

### GetCreatureStamina (238) / GetCreatureMana (242)
- **Handler:** 0x0066CC30, both (checked).
- **Does:** pushes property 8's CURRENT (slot +0x24), truncated (checked).
- **Edge cases:** non-creature (a placeable too) or invalid: -1; no stats: -1; no property 8: 0 (checked). Same value for a warrior's "mana" and a mage's "stamina".

### SetCreatureStamina (239) / SetCreatureMana (243)
- **Handler:** 0x0066CD00, both (checked).
- **Does:** pops creature, int; property 8's SetCurrent (slot +0x2C, 0x0079C8D0): clamp(value, +0x78, TOTAL) (checked). Then the byte stats+0x1B0 = 1, on every path past the stats check (checked).
- **Edge cases:** above the max sets the max; below 0 sets the minimum (+0x78, 0 by default). No notification call. Non-creature or invalid: nothing, returns 0.

### GetCreatureMaxStamina (240) / GetCreatureMaxMana (244)
- **Handler:** 0x0066CDC0, both (checked).
- **Does:** pushes property 8's TOTAL (slot +0xC): base + max0(WIL)×5 + modifier, clamped 0..9999 (PROPERTIES.md), truncated (checked).
- **Edge cases:** as GetCreatureStamina: -1 invalid/non-creature/no stats, 0 with no property 8.

### SetCreatureMaxStamina (241) / SetCreatureMaxMana (245)
- **Handler:** 0x0066CE90, both (checked).
- **Does:** property 8's slot +0x10, **SetBase** (0x0079C860): the base becomes the value, so the resulting max is value + the WIL link + modifier, not the value; the current is re-clamped, so lowering it pulls the current down and raising it does not fill it (checked; clamps per PROPERTIES.md).
- **Edge cases:** writes stats+0x1B0 only when property 8 is missing (the success path returns before it) (checked). Non-creature or invalid: nothing. No script ships a call to either (searched the toolset's scripts).

## Not read
- What holder+0xE8 (creature+0x158) holds and when it is set; who writes the death effect's +0x54 list +0xC entry 3 that `HasDeathEffect(…, TRUE)` tests.
- The meaning of stats+0x1B0 and of stats+0x1B4 (inferred: the interface's ability holder).
- The ability record's +0x1F8 column; 0x00630460's list at creature+0x534 (inferred: property 15, Level).
- The body of 0x0071E0D0 and 0x0071C7E0 in the ability removal.
- `SetPlaceableHealth` (234) / placeable slot +0x124 (0x0074E8E0): not this group.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05). Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not. Shared pops/pushes (-639, -638, `GetCreatureById` 0x005D58C0, `GetObjectById` 0x005D5BE0): `dev/script_commands/commands.md`.

### SetPlaceableHealth (234)
- **Handler:** 0x0065A250 (not shared); `GetObjectById`, then the object's vtable slot +0x124 (SetHealth) with the value **sign-extended from its low 16 bits** (`movsx word`) and 1 (checked).
- **Described in:** `dev/script_commands/stats.md`, the shared-facts paragraph (slot +0x124 named there) and "SetMaxHealth"; `dev/PLACEABLES.md` for what a dead placeable does.
- **Does:** placeable (0x0074E8E0): stores the value at +0x66C with no clamp to the maximum; for a value <= 0 it first runs the death-side code (0x0069BA70(0) clears the byte +0x42C if the +0x678 attribute test is 0; then, if 0x0074D1B0 answers true, 0x0074DBA0 re-applies the state in +0x658 forced, and 0x0074D350(0)), then stores (checked as calls; their effect inferred: the dead state's animation). Creature (0x006A7DD0): the value as a float into `CCCreatureStats::SetCurrent` (0x0071B320) for property 7: the **current health**, clamped to the property's range, with the property-change notification (checked). It starts no death effect and no `KillCreature`.
- **Edge cases:** invalid id: nothing, returns 0 (checked). Item, trigger, waypoint and other types: CCObject's slot is a bare `ret 8` (0x00738C90), so nothing (checked). Values outside -32768..32767 wrap (checked). The name says placeable but a creature works (checked); script.ldf does not say so.

### IsDead (237)
- **Handler:** 0x0065A3A0 (not shared); `GetObjectById`, then 0x00695300 = the object's slot +0x120 (GetHealth) and "health <= 0" (checked). It does **not** use the creature's IsDead slot +0x1D0 (0x006A3660, which treats a creature with no stats as dead), although the effect is the same for a creature with stats (checked).
- **Described in:** `dev/COMBAT.md`, "Health and IsDead" (the slot version); `dev/script_commands/stats.md` for GetHealth's values per type.
- **Does:** pushes TRUE when GetHealth is 0 or below. No death effect or flag is consulted (checked).
- **Edge cases:** an invalid id pushes FALSE (the handler's `esi` is zeroed first). **An object whose GetHealth is -1 (item, trigger, AoE, sound, visual effect) answers TRUE**, a waypoint (GetHealth 1) FALSE, a placeable TRUE once +0x66C <= 0 (checked, derived from the GetHealth table in stats.md). A creature with no stats answers 0 health, so TRUE (checked in 0x006AB4B0's body per stats.md).

### SetDead (729)
- **Handler:** none. The command is in script.ldf (`void SetDead(object, int bDeadFlagValue)`) and the CSV has no row for it: the exe does not register it (`dev/SCRIPT_COMMANDS.md`, "Declared, not registered in this exe").
- **Described in:** `dev/SCRIPT_COMMANDS.md` (the count table).
- **Does:** nothing. A script's call reaches `ExecuteCommand` with no handler; the unregistered command 10 is shown there to answer -642, which fails the script (inferred for 729 from that, same path).
- **Edge cases:** no shipped script calls it (searched `toolset\extract\scripts`). script.ldf's "dead flag" has no field in the engine (COMBAT.md: "no separate flag").

### GetCreatureProperty (738)
- **Handler:** 0x0066D0E0 (not shared): the helper 0x0066CF60 (also used by `GetCreatureAttribute` 248) and `StackPushFloat` 0x00A2D690 (checked).
- **Described in:** `dev/PROPERTIES.md`, "GetCreatureProperty (738, `0x0066D0E0`)"; the by-type table in `dev/script_commands/stats.md`, "GetCreatureAttribute".
- **Does:** pops creature, property, value type, reads TOTAL/BASE/CURRENT/MODIFIER (types 1 to 4, jump table 0x0066D090) and **pushes the float itself** (no truncation, unlike 248) (checked).
- **Edge cases:** non-creature, no stats, or a value type outside 1..4: pushes -1.0 (0x00AE2E00; the helper's seed) (checked). All three arguments are popped unconditionally, so the ldf default for nValueType (TOTAL) is applied by the compiler, not the handler (checked). Property ids 1000 to 1999 and >= 2000: see stats.md (GetCreatureAttribute).

### SetCreatureProperty (740)
- **Handler:** 0x00669890 (not shared); pops creature, int, float, int in that order, all unconditionally (checked).
- **Described in:** `dev/PROPERTIES.md`, "SetCreatureProperty (740) and UpdateCreatureProperty (741)".
- **Does:** type 1 (TOTAL) and 2 (BASE) both call `CCCreatureStats::SetBase` 0x0071B230; 3 CURRENT `SetCurrent` 0x0071B320; 4 MODIFIER `SetModifier` 0x0071B280 (jump table 0x006699A4); each then calls the change notification 0x00719A00 (checked).
- **Edge cases:** a value type outside 1..4, a non-creature, a creature with no stats: nothing, returns 0 (checked). **The script.ldf default (BASE) is the compiler's; setting "TOTAL" sets the base**, so the total changes by the same amount only if no clamp or link intervenes (checked). A property id the store lacks: the stats' setters do nothing visible (inferred).

### UpdateCreatureProperty (741)
- **Handler:** 0x006699C0 (not shared); same pops as 740 (creature, property, float, value type) (checked).
- **Described in:** `dev/PROPERTIES.md`, same section as 740.
- **Does:** adds the float: type 3 (CURRENT) via 0x0071B370, type 4 (MODIFIER) via 0x0071B2D0, each with the notification (checked).
- **Edge cases:** types 1 (TOTAL) and 2 (BASE) and everything else: **nothing at all**, still returns 0 (checked). Non-creature or no stats: nothing. The current is clamped by the depletable's SetCurrent (PROPERTIES.md).

### HasAbility (252)
- **Handler:** 0x00669AB0, shared with `AddAbility` (253) and `RemoveAbility` (254); this command's case is 0xFC, at 0x00669C21 (checked).
- **Described in:** `dev/script_commands/stats.md`, "RemoveAbility" (the shared handler, its `ABI_base` AbilityType read).
- **Does:** pops creature and ability; reads `ABI_base` `AbilityType` (hash 0xE1A8F4EA, 0x00A2CCC0); if the ability is > 0, the creature exists and the row was read, pushes `CCCreatureStats::HasAbility` (0x0071BEC0: the ability id is in that type's vector, 0x007199D0) (checked).
- **Edge cases:** ability <= 0, unknown ability (no `ABI_base` row), invalid or non-creature: pushes 0 (checked). Item abilities (type 4) are answered like any other type (checked). Only for 252 does a failed push give -638.

### AddAbility (253)
- **Handler:** 0x00669AB0, case 0xFD at 0x00669BB1 (checked).
- **Described in:** `dev/script_commands/stats.md`, "RemoveAbility" (the refusal of type 4 is noted there).
- **Does:** pops creature, ability, and bSendNotification **only when the argument count is above 2** (default FALSE); for ability > 0 and a known row with `AbilityType` != 4, calls `CCCreatureStats::AddAbility` 0x0071EAC0(ability, type, -1). If bSendNotification and the creature is in the party pool (0x006316F0), it queues a notification on party+0x330 (0x006291C0, with the ability and type) (checked; the notification's shape inferred from `Party.md`'s queue).
- **Edge cases:** **an item ability (type 4) is silently not added.** A duplicate: 0x0071EAC0 sees `HasAbility` and returns early unless the type is 4 (checked). Unknown ability, ability <= 0, invalid or non-creature: nothing. Returns 0.

### GetCreatureMana (242)
- **Handler:** 0x0066CC30, the same handler as `GetCreatureStamina` (238) (checked).
- **Described in:** `dev/script_commands/stats.md`, "GetCreatureStamina (238) / GetCreatureMana (242)".
- **Does:** property 8's CURRENT, truncated to int. There is no separate mana: mage and warrior share property 8.
- **Edge cases:** as there: non-creature or no stats -1; no property 8: 0.

### SetCreatureMana (243)
- **Handler:** 0x0066CD00, shared with `SetCreatureStamina` (239) (checked).
- **Described in:** `dev/script_commands/stats.md`, "SetCreatureStamina (239) / SetCreatureMana (243)".
- **Does:** property 8's SetCurrent, clamped to [minimum, TOTAL]; no change notification (0x00719A00 is not called here).
- **Edge cases:** as there: invalid or non-creature, nothing; returns 0.

### GetCreatureMaxMana (244)
- **Handler:** 0x0066CDC0, shared with `GetCreatureMaxStamina` (240) (checked).
- **Described in:** `dev/script_commands/stats.md`, "GetCreatureMaxStamina (240) / GetCreatureMaxMana (244)".
- **Does:** property 8's TOTAL (base + WIL link + modifier, clamped 0..9999), truncated.
- **Edge cases:** as there: -1 for invalid, non-creature or no stats; 0 with no property 8.

### SetCreatureMaxMana (245)
- **Handler:** 0x0066CE90, shared with `SetCreatureMaxStamina` (241) (checked).
- **Described in:** `dev/script_commands/stats.md`, "SetCreatureMaxStamina (241) / SetCreatureMaxMana (245)".
- **Does:** property 8's SetBase: the maximum becomes value + the WIL link + modifier, not the value; the current is re-clamped down, never raised.
- **Edge cases:** as there: invalid or non-creature, nothing. script.ldf's "set the maximum" is only true when the WIL link and modifier are 0.

## Not read
- The body of the placeable death-side calls 0x0074D1B0, 0x0074DBA0 and 0x0074D350 in 0x0074E8E0 beyond their entry (what state change they make).
- The exact behaviour of `CCCreatureStats::AddAbility` 0x0071EAC0 past its first branches (passives, GUI records, party recompute).
- Whether property ids missing from the store are guarded in `SetBase`/`SetCurrent` (0x00642E20 for the id).
