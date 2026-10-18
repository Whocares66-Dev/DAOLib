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
- **State:** reads the active party list and each member's +0xC18; writes nothing. (checked)
- **Edge cases:** **oCreature is only a gate**: it must be a valid creature (else 0, as above) but is otherwise ignored — any creature, party member or not, gives the same answer, the active party's. script.ldf's "the party of a creature" suggests otherwise. (checked) No party (no game) → 0. (checked) Ids in the list that are no longer creatures are skipped. (checked) Members in the pool but not the active party (+0x244) are not asked. (checked: only +0x234 is walked)

## Not read
- `0x0064BB40` itself (the reputation manager's hostility test); taken from `dev/COMBAT.md`.
- Whether a creature's +0xC18 set is cleared when it leaves the active party or dies (`ClearPerceptionList` 446 is another group's).
