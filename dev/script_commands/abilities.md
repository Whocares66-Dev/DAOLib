# Script commands, group "abilities": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not. Shared idioms (pops, pushes, -639/-638, `GetCreatureById` 0x005D58C0): `dev/script_commands/commands.md`. `IsAbilityAvailable`, `GetAbilityCount` and `RemoveAbility` are in script.ldf's `stats` group (see `lib-agent-2-scg-stats.md`).

### CanUseConditionedAbility (819)
- **Handler:** 0x0066C1F0 (not shared). Pops creature, ability, conditions (all three, no count check); `GetCreatureById`, stats (`[creature+0x7C4]+0xC`), then `CCCreatureStats` 0x0071C310(ability, conditions, 0); pushes 1 if that returns 1, else 0 (checked).
- **Does (0x0071C310, checked):** mask = `ABI_base` `Conditions` (hash 0xB1CAEE1D) AND nConditions; mask 0 (no conditions, unknown ability, or the bits not asked) → TRUE. Then, by bit (the constants are `player_core`'s comment: MELEEWEAPON 0x1, SHIELD 0x2, RANGEDWEAPON 0x4, BEHINDTARGET 0x8, DUALWEAPONS 0x40, 2HWEAPON 0x80):
  - Weapon bits (mask & 0xC7), only when the creature has an equipment object (`[creature+0xBE0]`): main = slot 0's item, off = slot 1's (0x007F4EA0), each item's class at +0x6E0 (0x00752C70; 0 for none). RANGED: main class 6, and it decides alone. MELEE: main 2, off 2, or main empty. SHIELD: off 3. DUAL: main 2 and off 2. 2H: main 2 and its +0x440 passes 0x00A664C0 (inferred: two-handed); a 2H failure fails at once. Otherwise **only the last weapon bit tested decides** (each overwrites the same flag), so MELEE|SHIELD reduces to SHIELD (checked).
  - 0x100: `ABI_base` `Condition_AppGroup` AND the appearance's group (0x0071A210 on creature+0x7B0) must be non-zero.
  - 0x200/0x400/0x800 (class): property 27 `CurrentClass` (0x0071B010) 1 needs 0x800, 2 needs 0x200, 3 needs 0x400 set in the mask; any other class passes.
  - 0x10: `ABI_base` `Condition_Mode` (hash 0x07342EA2), if non-zero, must pass 0x0066DD60 (inferred: that modal ability is active).
- **State:** reads only. With a non-null third argument it would write a talk-table id for the reason (0x364D9 ranged, 0x364DA melee, 0x364DB shield, 0x4D082 dual, 0x5C674 2H, 0x5D8BE group, 0x5D8BF/0x5D8C0/0x5D8C1 class, 0x4BE85 mode); the command passes 0 (checked).
- **Edge cases:** BEHINDTARGET (0x8) is not tested by the engine (checked; `ability_h.Ability_CheckUseConditions` does it in the script). Invalid or non-creature: 0 (checked). No data-table manager, or stats with no owner (+0x5C): TRUE (checked). A creature without an equipment object passes every weapon bit (checked). Default nConditions 0xFFFFFFFF tests all of the ability's conditions. No shipped script calls it; `player_core` uses `GetConditionedAbilities` (816) with 0xC7.

## Not read
- The item class values at +0x6E0 beyond what the tests imply (2 melee, 3 shield, 6 ranged), and 0x00A664C0.
- 0x0071A210 (the appearance group) and 0x0066DD60 (the mode test).
