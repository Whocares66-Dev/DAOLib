# Script command handlers: threatsystem (DAOParagonEngine #27)

Read 2026-10-18 in the GOG exe (`game\DAOrigins.exe`, base 0x400000) by lib-agent-2's subagent, with `tools\daoexe.py --disasm` and scratch call/xref scans; no Ghidra, no game. "Checked" = read in the disassembly; "inferred" = a reading of purpose. Shared idioms (checked): `CVirtualMachine::StackPopObject` 0x00A2F6D0, `StackPopInteger` 0x00A2F3C0, `StackPopFloat` 0x00A2F410, `StackPushInteger` 0x00A2D650, `StackPushFloat` 0x00A2D690; a failed pop returns -639, a failed push -638. `CClientExoApp::GetCreatureById` 0x005D58C0 (null for anything not a creature), `GetObjectById` 0x005D5BE0. "The party" is `[[app+0xD0]+0x58]` (`CCCreature::GetParty` 0x006A2CC0, also `0x005D57C0`); `[[app+0xD0]+0x5C]` is the controlled creature's id (`0x005D5CA0`; `0x005D5CC0` returns that creature).

## Threat table: summary

- **Storage (checked):** `CCCreature+0xC60` points to the creature's combat data; the threat table is an MSVC8 `std::vector` at combat+0xC (begin +0x10, end +0x14, cap +0x18) of 12-byte entries `{+0 object id, +4 float threat, +8 float}`. The out-of-range sentinel is a static entry at 0x00C0CA74 = `{0xFFFFFFFF, 0.0, 0.0}`. The combat data's destructor (0x007C38D0) frees it via 0x00741EC0.
- **Value reported:** every reader returns `threat(+4) + (+8)`, and the sort key is the same sum. Nothing writes +8 except `resize`'s default `{-1, 0, 0}` (0x00670BA0 is `vector::resize(n, {-1,0,0})`, not "ClearThreatTable" as `dev/script_backends.csv` names it), so +8 is always 0 (checked: the only callers of every table helper are the five script backends).
- **Order (checked):** after each `UpdateThreatTable` the vector is `std::sort`ed (0x00741920: introsort, insertion sort 0x00741610 under 32) **descending** by `+4 + +8`; index 0 is the most hated. The sort runs only in UpdateThreatTable.
- **Cap (checked; the result is odd, probably an off-by-one):** let n be the size before the call. New enemy, n < 5: appended. n == 5: added only if `fChange >= entry[4].sum`, then no erase, so the table reaches **6**. n >= 6: appended unconditionally, sorted, then `erase(n-1)` (0x00741840) drops the entry at index n-1, i.e. the 6th-ranked, keeping the 7th (lowest, possibly the newcomer). Existing enemy updated at n == 6: sorted, then `erase(5)` drops the 6th-ranked, so the size goes back to 5.
- **No decay, no engine writers (checked):** only `UpdateThreatTable`, `ClearThreatTable`, `ClearEnemyThreat` write it. Damage, perception and combat code never call the helpers. Decay, damage threat and the death clear are all in `ai_threat_h.nss`. Not saved: no GFF label or field reads it (no "Threat" strings outside the command names).

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
