# Script command handlers: reaction (DAOParagonEngine #27)

Read 2026-10-18 in the GOG exe (`game\DAOrigins.exe`, base 0x400000) by lib-agent-2's subagent, with `tools\daoexe.py --disasm` and scratch call/xref scans; no Ghidra, no game. "Checked" = read in the disassembly; "inferred" = a reading of purpose. Shared idioms (checked): `CVirtualMachine::StackPopObject` 0x00A2F6D0, `StackPopInteger` 0x00A2F3C0, `StackPopFloat` 0x00A2F410, `StackPushInteger` 0x00A2D650, `StackPushFloat` 0x00A2D690; a failed pop returns -639, a failed push -638. `CClientExoApp::GetCreatureById` 0x005D58C0 (null for anything not a creature), `GetObjectById` 0x005D5BE0. "The party" is `[[app+0xD0]+0x58]` (`CCCreature::GetParty` 0x006A2CC0, also `0x005D57C0`); `[[app+0xD0]+0x5C]` is the controlled creature's id (`0x005D5CA0`; `0x005D5CC0` returns that creature).

## Group hostility: summary

- **Storage (checked):** the reputation manager at `app+0x1B0` (handlers do nothing if it is null). `[mgr+0]` points to a vector (begin +4, end +8) of pointers to 8-byte records, sorted by and binary-searched (0x0064B830) on `+0 group id`, compared **unsigned**. Each record's `+4` points to a sorted vector of group ids (find 0x0064B5B0, insert 0x0064BC00, erase 0x0064BB10). A hostile pair (a, b), a != b, is stored once: the larger id's record holds the smaller id. Hostility is symmetric.
- **Defaults (checked plus inferred):** the matrix is empty, so every pair is non-hostile and a group is never hostile to itself. The module load (0x00605430 → 0x0064C100) clears it and reads GFF3 lists `GroupMasterlist`/`GroupID`/`GroupList`/`GroupHostile` from the module's data. The shipped campaign is a GFF4 `.cif` without them (inferred: none shipped). `module_core.nss` MODULE_LOAD then sets PC↔HOSTILE and FRIENDLY↔HOSTILE (GROUP_PC 0, HOSTILE 1, FRIENDLY 2, NEUTRAL 3). Saves keep it: 0x0064C310/0x0064BD20, GFF4 fields 16450 `SavegameGroupList`, 16451 `SavegameGroupID`, 16452 `SavegameGroupHostiles`.
- **IsObjectHostile (0x0064B900, checked):** both are looked up with GetCreatureById, so a non-creature on either side gives 0. A creature's group is `+0x3C0`, its reaction override flag `+0x3B8` and value `+0x3BC`. If **either** has `+0x3B8 == 1` **and the groups differ**, the answer is `(A.+0x3BC == 1 || B.+0x3BC == 1)`. An override of 0 thus beats a hostile matrix entry, unless the other side's override is 1. Otherwise (no override, or the same group, even with overrides) the answer is the matrix. There is no party special case: party members are just GROUP_PC by template.
- **Change notification (checked):** a real change from `SetGroupHostility` or a creature's `SetGroupId` puts ids on the perceiver's pending list `+0xC28`. The next perception update raises APPEAR with int 2 = 1 for each (COMBAT.md's "hostility changed"), so this is what fills that list. No DISAPPEAR, and no immediate event. Only the controlled creature's area is walked for SetGroupHostility. Pending APPEARs are raised only while perception runs (explore or combat mode).

### GetGroupHostility (466)
- **Handler:** 0x0065B920 → `ScriptBackend::ForScript_GetGroupHostility` 0x0064B9A0 on `app+0x1B0`. Checked.
- **Does:** returns 1 if the pair is in the matrix, else 0. Reaction overrides are ignored. Checked.
- **Edge cases:** a == b → 0 (checked). Any int is accepted; unknown groups → 0. A null manager → 0.

### SetGroupHostility (467)
- **Handler:** 0x0065B9A0 (a, b, bHostile) → `ScriptBackend::ForScript_SetGroupHostility` 0x0064C060. Checked.
- **Does:** if a == b, or the current value already equals bHostile, nothing. Otherwise bHostile == 1 inserts the smaller id into the larger id's record (creating the record, 0x0064BF00), and any other value erases it. Then 0x0064B750(a, b) runs. Checked.
- **Notification (checked):** for every creature in the controlled creature's area (`area+0x680`): if its group is a, 0x006B8470(b); if b, 0x006B8470(a). That call queues each creature it perceives (`+0xC18`) of the other group onto its pending list `+0xC28`, giving APPEAR with int 2 = 1 next update. If the party's group (party+0x1E4) is the other group, it also refreshes the creature's hostile-to-party flag (`+0x4E8` object slot +0x54, with `0x0064B9E0`: matrix or override; inferred to be the UI's hostility indicator).
- **Edge cases:** bHostile values other than 0 or 1 erase (= FALSE), but only when they differ from the current 0 or 1. Creatures in other areas are not told (checked). No manager → no-op.

### SetGroupId (470)
- **Handler:** 0x0065BB00 → object vtable slot +0x170. The base `CCObject::ForScript_SetGroupId` 0x00699FF0 writes `+0x3C0` only. CCCreature overrides it with 0x006B8450. Checked.
- **Does (creature):** if the group is unchanged, nothing. Otherwise it writes `+0x3C0` and calls 0x006B8250(old, new). That refreshes the hostile-to-party flag (`+0x4E8` slot +0x54 with matrix(partyGroup, new)). It queues, onto the creature's own pending list, each creature it perceives whose matrix hostility to old and new differs. Then, for each creature in its area that perceives it (its `+0xC18` set contains the id) with differing old/new hostility, it queues this id onto that creature's pending list. Checked.
- **Edge cases:** placeables, triggers and other objects take the plain write, with no notify (checked). Invalid object → no-op. Overrides are not consulted in the change test (it uses `GetGroupHostility`).

### SetReactionOverride (471)
- **Handler:** 0x0065BB70. Checked.
- **Does:** `+0x3B8 = 1`, `+0x3BC = bHostile` (raw int) on any object found by `GetObjectById`. **No notification and no APPEAR**, and the party flag is not refreshed. Checked.
- **Edge cases:** IsObjectHostile tests `+0x3BC == 1` exactly, so values other than 1 mean non-hostile. Overrides have no effect between members of the same group, and none on non-creatures in IsObjectHostile (checked).

### ResetReaction (669)
- **Handler:** 0x0065BBE0. Checked.
- **Does:** `+0x3B8 = 0`, `+0x3BC = 0` on any object. No notification. Checked.
- **Edge cases:** invalid object → no-op.
