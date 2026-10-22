# Script commands, group "abilities": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not. Shared idioms (pops, pushes, -639/-638, `GetCreatureById` 0x005D58C0): `dev/script_commands/commands.md`. `IsAbilityAvailable`, `GetAbilityCount` and `RemoveAbility` are in script.ldf's `stats` group (see `lib-agent-2-scg-stats.md`).

### CanUseConditionedAbility (819)
- **Handler:** 0x0066C1F0 (not shared). Pops creature, ability, conditions (all three, no count check); `GetCreatureById`, stats (`[creature+0x7C4]+0xC`), then `CCCreatureStats` 0x0071C310(ability, conditions, 0); pushes 1 if that returns 1, else 0 (checked).
- **Does (0x0071C310, checked):** mask = `ABI_base` `Conditions` (hash 0xB1CAEE1D) AND nConditions; mask 0 (no conditions, unknown ability, or the bits not asked) → TRUE. Then, by bit (the constants are `player_core`'s comment: MELEEWEAPON 0x1, SHIELD 0x2, RANGEDWEAPON 0x4, BEHINDTARGET 0x8, DUALWEAPONS 0x40, 2HWEAPON 0x80):
  - Weapon bits (mask & 0xC7), only when the creature has an equipment object (`[creature+0xBE0]`): main = slot 0's item, off = slot 1's (0x007F4EA0), each item's class at +0x6E0 (0x00752C70; 0 for none). RANGED: main class 6, and it decides alone. MELEE: main 2, off 2, or main empty. SHIELD: off 3. DUAL: main 2 and off 2. 2H: main 2 and its +0x440 passes 0x00A664C0 (inferred: two-handed); a 2H failure fails at once. Otherwise **only the last weapon bit tested decides** (each overwrites the same flag), so MELEE|SHIELD reduces to SHIELD (checked).
  - 0x100: `ABI_base` `Condition_AppGroup` AND the appearance's group (0x0071A210 on creature+0x7B0) must be non-zero.
  - 0x200/0x400/0x800 (class): property 27 `CurrentClass` (0x0071B010) 1 needs 0x800, 2 needs 0x200, 3 needs 0x400 set in the mask; any other class passes.
  - 0x10: `ABI_base` `Condition_Mode` (hash 0x07342EA2), if non-zero, must be in the stats' active modal abilities (0x0066DD60, checked: below).
- **State:** reads only. With a non-null third argument it would write a talk-table id for the reason (0x364D9 ranged, 0x364DA melee, 0x364DB shield, 0x4D082 dual, 0x5C674 2H, 0x5D8BE group, 0x5D8BF/0x5D8C0/0x5D8C1 class, 0x4BE85 mode); the command passes 0 (checked).
- **Edge cases:** BEHINDTARGET (0x8) is not tested by the engine (checked; `ability_h.Ability_CheckUseConditions` does it in the script). Invalid or non-creature: 0 (checked). No data-table manager, or stats with no owner (+0x5C): TRUE (checked). A creature without an equipment object passes every weapon bit (checked). Default nConditions 0xFFFFFFFF tests all of the ability's conditions. No shipped script calls it; `player_core` uses `GetConditionedAbilities` (816) with 0xC7.

### CanUseConditionedAbility's mode and group tests (read 2026-10-22 by paragon-agent-2's helper, checked by lib-agent-2, for DAOParagonEngine #169)
- **The mode bit (0x10)** [C, 0x0071C5BC-0x0071C5F7]: the value read is `abi_base` `Condition_Mode` (hash 0x07342EA2, via `0x00A2CCC0(table 1, ability, hash, &out)`); when the read succeeds and the value is non-zero, `0x0066DD60(stats, value)` is called, and a false result fails the test with reason 0x4BE85. `0x0066DD60` is `lea ecx, [ecx+0xF8]` then `std::vector<unsigned int>::Contains` (0x008663A0): the stats' +0xF8 vector of ability ids, the very one `IsModalAbilityActive` (726, 0x0066DD80) searches (ABILITIES.md:98). So the existing "(inferred: that modal ability is active)" becomes [C]: `Condition_Mode` is an ability id that must be in the stats' active-modal list. Condition_Mode 0 (no read or zero) passes.
- **The appearance group (0x100)** [C mechanism, 0x0071C4BA-0x0071C502]: the mask is the `abi_base` column with hash 0xC7E5BB24 (`Condition_AppGroup`, by the doc's naming; the hash was not matched to a name here: [I] for the name); the creature's group is 0x0071A210 on the appearance record `[[stats+0x5C]+0x7B0]` (null owner or null record: group 0): `movzx eax, word [rec+0x58]` (the appearance id) → the apr_base row lookup in the map at 0x00C0CB78 (`0x004E1140`) → `[row+0x9C]` (a bit mask, apr_base's column at that offset: `AppearanceRestrictionGroup` is the likely one, [I]). Passes when `mask & group` is non-zero; else fails with reason 0x5D8BE. So this test is not "passes": it is `Condition_AppGroup & row's group != 0`.
- Item class values at +0x6E0 are bitm_base `Type` (hash 0x12A02374, stored at load by 0x007547FF; ITEMS.md:517, checked), and the 2H test `0x00A664C0` is `EquippableSlots == 1` (ITEMS.md:517): the abilities.md "(inferred: two-handed)" and "Not read" for these are answered there. The main/off hand read is `0x007F4EA0(slot)` → `0x00761260({slot, set -1})` (set -1: the active set, ITEMS.md:517).

## Not read
- The names of `ABI_base`'s column hash 0xC7E5BB24 (taken as `Condition_AppGroup`) and of `apr_base`'s column at row +0x9C (`AppearanceRestrictionGroup` is likely).

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05). Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not. Pops/pushes and `GetCreatureById` 0x005D58C0: `dev/script_commands/commands.md`. The creature's stats are `creature+0x7C4` (`CCCreatureStats`).

### Engine_SetModalAbilityGUI (725)
- **Handler:** 0x00669E80 (not shared); pops creature, ability id, status (all unconditional) and calls `CCCreatureStats` 0x0071DA90(ability, status, 0) (checked).
- **Described in:** `dev/ABILITIES.md`, "IsModalAbilityActive and Engine_SetModalAbilityGUI share one list".
- **Does:** status 0 removes the ability id from the stats' +0xF8 vector, anything else appends it; sets the quickbar toggle (0x00719A40); may send event 0x88/0x89 on a real change. Called by the scripts' upkeep effect on apply and remove.
- **Edge cases:** invalid or non-creature: nothing, returns 0 (checked). 0x0071DA90 returns at once if the stats have no owner (stats+0x5C null) (checked). Any non-zero status counts as ON (checked). The ability id is not validated here (the `ABI_base` row is looked up inside, hash 0x70F463FA, and its use inferred).

### IsModalAbilityActive (726)
- **Handler:** 0x0066DD80 (not shared) (checked).
- **Described in:** `dev/ABILITIES.md`, same section (the +0xF8 vector).
- **Does:** `GetCreatureById`, then pushes TRUE when the ability id is in the stats' +0xF8 vector (`std::vector<unsigned>::Contains` 0x008663A0), else FALSE.
- **Edge cases:** invalid or non-creature: FALSE (`esi` zeroed first) (checked). It tests the list `Engine_SetModalAbilityGUI` and a save's load fill, not whether the effect is applied: a modal ability whose upkeep was removed by another route stays TRUE until 725 clears it (inferred from the writers in ABILITIES.md).

### SetCooldown (194)
- **Handler:** 0x0066A150 (not shared) (checked).
- **Described in:** `dev/ABILITIES.md`, "Cooldown" (set by the scripts); `src/dao/Abilities.h` (`dao::abilities::SetCooldown`) and `dev/LIBRARY.md`.
- **Does:** pops creature, ability id, float seconds and, **only when the argument count is 4 or more**, the item tag string (default empty); then `CCCreatureStats::SetCooldown` 0x0071DCE0(ability, seconds, tag) on the stats (checked). Remaining and total are both set to the seconds (Abilities.h).
- **Edge cases:** invalid or non-creature: nothing, returns 0 (checked). A failed pop gives -639. No range check: 0 or a negative time is stored as given (the record's own handling inferred). For an item ability (type 4) the record is found by ability and item tag (0x0071CA00; checked in `GetRemainingCooldown`'s lookup), otherwise the tag is ignored (inferred).

### GetRemainingCooldown (195)
- **Handler:** 0x0066A280 (not shared); stats 0x0071CE60(ability, tag) (checked).
- **Described in:** `dev/ABILITIES.md`, "Cooldown"; `src/dao/Abilities.h`.
- **Does:** pops creature, ability id and, **only when the argument count is 3 or more**, the item tag; looks the record up (0x0071CA00 then 0x0071C250) and pushes its remaining seconds as a float via `StackPushFloat` (checked).
- **Edge cases:** invalid or non-creature: pushes 0.0 (seeded `fldz`) (checked). No record for the ability (never set, or finished and dropped): 0.0 (0x0071CE85) (checked). A failed push returns -638.

## Not read
- The bodies of `CCCreatureStats::SetCooldown` 0x0071DCE0 and the record lookups 0x0071CA00/0x0071C250 (when a record is dropped, how the tag keys it).
- 0x0071DA90's later branches (the event 0x88/0x89 conditions).
