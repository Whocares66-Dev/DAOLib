# Script command handlers: threatsystem (DAOParagonEngine #27)

Read 2026-10-18 in the GOG exe (`game\DAOrigins.exe`, base 0x400000) by lib-agent-2's subagent, with `tools\daoexe.py --disasm` and scratch call/xref scans; no Ghidra, no game. "Checked" = read in the disassembly; "inferred" = a reading of purpose. Shared idioms (checked): `CVirtualMachine::StackPopObject` 0x00A2F6D0, `StackPopInteger` 0x00A2F3C0, `StackPopFloat` 0x00A2F410, `StackPushInteger` 0x00A2D650, `StackPushFloat` 0x00A2D690; a failed pop returns -639, a failed push -638. `CClientExoApp::GetCreatureById` 0x005D58C0 (null for anything not a creature), `GetObjectById` 0x005D5BE0. "The party" is `[[app+0xD0]+0x58]` (`CCCreature::GetParty` 0x006A2CC0, also `0x005D57C0`); `[[app+0xD0]+0x5C]` is the controlled creature's id (`0x005D5CA0`; `0x005D5CC0` returns that creature).

## Threat table: summary

- **Storage (checked):** `CCCreature+0xC60` points to the creature's combat data; the threat table is an MSVC8 `std::vector` at combat+0xC (begin +0x10, end +0x14, cap +0x18) of 12-byte entries `{+0 object id, +4 float threat, +8 float}`. The out-of-range sentinel is a static entry at 0x00C0CA74 = `{0xFFFFFFFF, 0.0, 0.0}`. The combat data's destructor (0x007C38D0) frees it via 0x00741EC0.
- **Value reported:** every reader returns `threat(+4) + (+8)`, and the sort key is the same sum. +8 is the engine's proximity bonus, rewritten every frame by 0x00741AC0 (below); `resize`'s default {-1, 0, 0} is the only other writer (0x00670BA0 is `vector::resize(n, {-1,0,0})`, not "ClearThreatTable" as `dev/script_backends.csv` names it).
- **Order (checked):** after each `UpdateThreatTable` the vector is `std::sort`ed (0x00741920: introsort, insertion sort 0x00741610 under 32) **descending** by `+4 + +8`; index 0 is the most hated. The sort runs after each `UpdateThreatTable` and every frame after the decay.
- **Cap (checked; the result is odd, probably an off-by-one):** let n be the size before the call. New enemy, n < 5: appended. n == 5: added only if `fChange >= entry[4].sum`, then no erase, so the table reaches **6**. n >= 6: appended unconditionally, sorted, then `erase(n-1)` (0x00741840) drops the entry at index n-1, i.e. the 6th-ranked, keeping the 7th (lowest, possibly the newcomer). Existing enemy updated at n == 6: sorted, then `erase(5)` drops the 6th-ranked, so the size goes back to 5.
- **The engine decays the table and adds a proximity bonus every frame** (see "The engine's per-frame update" below). The scripts' writers are `UpdateThreatTable`, `ClearThreatTable` and `ClearEnemyThreat`. Damage, perception and the death clear are in the scripts (`ai_threat_h.nss`, see "Where threat comes from"). Not saved: no GFF label or field reads it.

### UpdateThreatTable (204)
- **Handler:** 0x00659EE0 (pops oCreature, oEnemy, fThreatChange) → `ScriptBackend::ForScript_UpdateThreatTable` 0x00741D80 on combat+0xC. Checked.
- **Does:** finds oEnemy (0x00740D70, linear, last match). If found, `threat = max(threat + change, 0)`. If not, it adds `{oEnemy, max(change, 0), 0}` under the cap rules above. Then it sorts descending and, if the size before the call was over 5, erases index (size − 1). Checked.
- **Edge cases:** oEnemy is not validated (OBJECT_INVALID or a placeable becomes an entry; checked). Entries at 0 stay. A negative change for an absent enemy adds a 0 entry. Non-creature oCreature → no-op (checked). Dead creatures are not checked.

### ClearThreatTable (202)
- **Handler:** 0x00673030 → `vector::resize(0, {-1,0,0})` 0x00670BA0 on combat+0xC. Checked.
- **Does:** empties the table. Checked.
- **Edge cases:** non-creature → no-op. No event. `ClearEnemyThreat` (203, 0x00659F80 → 0x00741900) is the per-enemy erase, not sorted afterwards (checked).

### GetThreatTableSize (201)
- **Handler:** 0x0066CBA0, inline `(end − begin)/12` of combat+0xC. Checked.
- **Does:** returns the entry count, 0 to 6 (see the cap). Checked.
- **Edge cases:** non-creature or invalid → 0 (checked).

### GetThreatValueByIndex (199)
- **Handler:** 0x0065A0A0 (object, int) → `ScriptBackend::ForScript_GetThreatValueByIndex` 0x00740D10. Checked.
- **Does:** returns `entry[i].threat + entry[i].+8`. Index 0 is the highest, since the table is kept sorted. Checked.
- **Edge cases:** i past the end or negative (unsigned compare) → **0.0** (checked). Empty table, non-creature or invalid → 0.0 (checked).

### GetThreatValueByObjectID (198)
- **Handler:** 0x0065A140 (object, object) → `ScriptBackend::ForScript_GetThreatValueByObjectID` 0x00740FA0. Checked.
- **Does:** linear search for oEnemy, returns its `threat + +8`. Checked.
- **Edge cases:** enemy not in the table → 0.0. Non-creature → 0.0. oEnemy is not validated, so OBJECT_INVALID finds an entry only if one was added with it (checked).

## The engine's per-frame update (read 2026-10-21 by lib-agent-2 with a subagent)

- **Who** [C]: `CCCreature::Update` (0x006BE4B0) calls 0x006AB620 at 0x006BE7EE. The gates are:
  - the creature is active (+0x2C5);
  - `Combatant` (+0xBD4) is 0;
  - byte +0xC94 is 0 [its meaning was not read];
  - the combat data's state (`[combat]`) is non-zero, which means in combat [I for states other than 1].
- **The rate** [C]: property 40, `PROPERTY_SIMPLE_THREAT_DECREASE_RATE`, its total (0x00642E20(0x28), slot 3). It is 0 if the property is missing. The scripts set it to `AI_THREAT_DISSOLVE` = −0.5 in `AS_SetCommonStats` (`sys_autoscale_h.nss`:400), and no script reads it.
- **The decay** [C, 0x00741AC0]:
  - Every entry's threat (+4) becomes max(threat + ms × 0.001 × rate, 0), where ms is `[[creature+0x2B8]+8]`, the frame's elapsed milliseconds. So threat falls by 0.5 per second.
  - Entries at 0 are kept.
- **The proximity bonus** [C, 0x00741B69..0x00741C92]:
  - Each entry's +8 becomes 100 × (60 − d) / 60, where d is the distance from the creature to that entry's creature ignoring the third axis.
  - It is 0 when d ≥ 60 or the entry is not a creature.
  - [I]: the third axis is height, since the world is Z-up, and the units are metres.
- **The sort** [C]: then the vector is sorted descending (0x00741920). Every reader and the sort take +4 + +8, so a script's threat value and the "most hated" include up to +100 for a near enemy.
- [I]: while the gates are closed (out of combat, or Combatant ≠ 0) neither the decay nor the bonus runs, and +8 keeps its last value.

## Where threat comes from (the scripts, read 2026-10-21 by lib-agent-2)

`AI_Threat_UpdateCreatureThreat` (`ai_threat_h.nss`:464–580) is the funnel for every gain or loss on a creature's table toward an enemy. A follower's table is never fed (rules_core tests `!IsFollower`).

- **Adjustments in the funnel:**
  - the hated race, class and gender ×2 each (`AI_Threat_GetHatedThreat`; the class test reads the hater's own class, a vanilla slip);
  - a positive change under 1 becomes 1;
  - a loss never takes the value below 1;
  - the stored value is capped at 1000.
- **The script sources:**
  - **Perception** (`EVENT_TYPE_PERCEPTION_APPEAR` → `AI_Threat_UpdateEnemyAppeared`, :318–380): 10, plus the chest armour's +5 (light) or +10 (medium and up) below Nightmare, plus `AI_THREAT_GENERATE_EXTRA_THREAT` (a local on the enemy), plus 5 below Nightmare. The weapon test compares the chest's type with bow and staff types, so the 5 is always added (a vanilla slip). A creature of AI package 10130 (coward) gives 1.
  - **Damage** (`EVENT_TYPE_DAMAGED`, `rules_core.nss`:306 → :240–278): damage / max health × 100, × 0.8 if the attacker is a rogue, × 2 with Threaten active.
  - **A hostile ability landing** (`EVENT_TYPE_CAST_AT`, `rules_core.nss`:610 → :281–296): `abi_base.Threat_Impact` × 10 (for example Taunt 10, Overpower 10, MONSTER_BEAR_OVERWHELM 5, Cone of Cold 4; most spells 0). Walking Bomb is ×2 on Hard and up.
  - **Being attacked** (`EVENT_TYPE_ATTACKED`, :649 → :407–421): raises the attacker to 1 only while it is under 1.
  - **The abilities' own scripts:**
    - Taunt: +300, +100 more with Frightening, and the switch timer reset (`talent_aoe_instant.nss`:173–184).
    - Disengage: −100 (:162–166).
    - Distraction: every entry down to 1 (`talent_singletarget.nss`:222–234).
    - Mind Blast: the caster down to 1 (`spell_aoe_instant.nss`:97–99).
    - A shapeshifted party member's attack: +300 (`monster_singletarget.nss`:588–594).
    - A hostile non-damaging AoE: +20 on Hard (`spell_aoe_duration.nss`:635–637).
    - The on-hit threat item property, id 4008: +5 directly (`sys_itemprops_h.nss`:222–224).
  - **Losses:**
    - Can't reach the target: −75 % (`ai_main_h_2.nss`:3235).
    - Root, sleep and the like: the creature's enemies' threat toward it to 0 (`AI_Threat_ClearEnemiesThreatToMe`, `effects_h.nss`).
    - Out of sight: its entry erased (`combat_h.nss`:1309, 1331).
    - Death: the dead creature's table cleared.
- **The target** [C, the scripts]:
  - **`AI_Threat_GetThreatTarget`** keeps a chosen target in the local `AI_THREAT_TARGET`. Each update sets it to `GetThreatEnemy(0)` (the highest stored + bonus) under these conditions:
    - when none is set;
    - when the switch timer has run out: 0 s at first, then +5 s per switch up to 25 s, then back to 0; always 0 for ranged attackers (`AI_Threat_ClearToSwitchTarget`);
    - when the current target is unseen, dead or dying.
  - **The wounded-target keep**: a creature at 10–20 % health is kept with a 50 % chance, and one under 10 % with a 90 % chance (`AI_Threat_KeepWoundedTarget`).
  - **Who uses it**: an enemy's default attack takes it (`AI_ExecuteDefaultAction` → `_AI_Condition_GetMostHatedEnemy`, `ai_main_h_2.nss`:910), and so do the tactics' `AI_TARGET_TYPE_MOST_HATED` and `MOST_HATED_ENEMY`. The light AI (`AI_DetermineCombatRound_Light`, :784–786) picks a random entry instead. Followers take their selected or nearest target, not threat.
- **An enemy can be hated and targeted without being perceived** (read 2026-10-21 by lib-agent-2):
  - **No perception test on threat** [C]: damage, a hostile ability landing, being attacked, and the abilities' own threat (for example a shapeshifted party member's Bear Slam, +300) all reach `AI_Threat_UpdateCreatureThreat`. That function tests only hostility and death, never `IsPerceiving`.
  - **The forced perception has no sight test** (corrected 2026-10-21 by lib-agent-2; the first version of this bullet said it had one) [C]:
    - `WR_TriggerPerception` (`sys_stealth_h.nss`:126–140; it also drops the perceived creature's stealth) is `TriggerPerception` (166, 0x0065A7B0 → 0x006B32A0).
    - It adds the other to the perceived set and raises APPEAR whenever 0x006A2BB0 passes: same area, not dead, active, and a 2D distance under 60 m (the double at 0x00B03BF8). No ray is cast.
    - So an ATTACKED or CAST_AT event between two creatures makes them perceive each other, a wall or not.
    - **What does add threat without perception** is damage alone (`EVENT_TYPE_DAMAGED`, which triggers no perception) and the abilities' own threat. In the Harrowing case, the bear held in the wisps' tables but never in their lists fits damage that came with no ATTACKED or CAST_AT event on them [I, not traced].
  - **The target stays unperceived** [C, `ai_threat_h.nss`:208–226]: `AI_Threat_GetThreatTarget` returns the stored `AI_THREAT_TARGET` even when it is not perceived. Its re-update's result is not read back. A switch re-picks `GetThreatEnemy(0)`, again without a perception test.
  - **Seen in play** (DAOParagonEngine, the Harrowing): two wisps that never perceived Mouse as the bear (a scarecrow placeable blocks the ray) held the bear in their tables within about 7 s and attacked it 70–96 % of the time.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by lib-agent-2's subagent (b4b) in `game\DAOrigins.exe` (GOG, base 0x400000) with `tools\daoexe.py --disasm`. Shared facts as in the group's file: a failed pop returns -639, a failed push -638; `GetCreatureById` 0x005D58C0; the threat table is the vector at combat data (`[creature+0xC60]`) +0xC of 12-byte entries `{id, threat, 0}`, sorted descending after each update. "Checked" = read in the disassembly, "inferred" = not.

### ClearEnemyThreat (203)
- **Handler:** 0x00659F80 (not shared). `GetCreatureById`, then the table's find 0x00740D70 and erase 0x00741840, through 0x00741900 on combat +0xC. (checked)
- **Described in:** `dev/script_commands/threatsystem.md`, "Threat table: summary" (its only writers are `UpdateThreatTable`, `ClearThreatTable` and this).
- **Does:** pops the creature and the enemy id, finds the enemy's entry and erases it, so the rest keep their order. (checked)
- **Edge cases:** a non-creature does nothing; an enemy with no entry does nothing (the find answers -1). The enemy is not validated, so an id of a dead or removed object still clears its entry. A creature without combat data would fault (no test, inferred). Returns 0.

### GetThreatEnemy (200)
- **Handler:** 0x0065A000 (not shared). `GetCreatureById`, then 0x00740CC0 on combat +0xC. (checked)
- **Described in:** `dev/script_commands/threatsystem.md`, "Threat table: summary" (index 0 is the most hated, the order a sort after each update leaves).
- **Does:** pops the creature and the index and pushes the entry's object id at that index. It does not push the threat value (`GetThreatValueByIndex` does). (checked)
- **Edge cases:** an index at or past the table's size (compared unsigned, so a negative index too), an empty or never-filled table, and a non-creature all push OBJECT_INVALID (-1, the default). (checked) Entries are not checked for validity: the id may be of a dead creature or any object that was passed to `UpdateThreatTable`. Push failure -638.

## Not read
- Whether a creature always has combat data (a creature without it would fault in both handlers).
