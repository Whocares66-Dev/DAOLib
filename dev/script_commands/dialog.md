# Script commands, group "dialog" (438, 444): the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent, b2b) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not. Pops and pushes as `dev/script_commands/Party.md` says (-639 a failed pop, -638 a failed push).

### GetPCSpeaker (438)
- **Handler:** 0x00661870 (`ScriptCommand::GETPCSPEAKER`, not shared). Calls 0x005D5CA0. Pops nothing. (checked)
- **Does:** if the implementer's `[this+4]` is 1 (inferred: the server's `CVirtualMachineCommands`, as `dev/ITEMS.md` reads the same test), pushes `[[app+0xD0]+0x5C]`, the controlled creature (not the party leader at party+0x1E8, `dev/PARTY.md`: corrected 2026-10-18 by lib-agent-2); otherwise pushes OBJECT_INVALID. With no game (app+0xD0 null) 0x005D5CA0 answers OBJECT_INVALID. (checked)
- **State:** reads only the controlled creature's id; **no conversation state is read**. (checked)
- **Edge cases:** script.ldf says "the PC (if any) involved in the dialog that ran this script"; the engine answers the controlled creature whether or not a conversation is running or which one ran the script, so it is never OBJECT_INVALID in a game, and a follower the player controls is returned rather than the hero. (checked) One shipped script uses it (`bdn210pt_shield_puzzle.nss`, comparing it with the PC). (checked by grep)

### HasConversation (444)
- **Handler:** 0x0065DB60 (`ScriptCommand::HASCONVERSATION`, not shared). Calls `GetObjectById` (0x005D5BE0) and 0x00A29F60 (the address library's `ScriptBackend::ForScript_HasConversation`; in fact a resref's "not empty" test, `cmp byte [ecx+0x21],0; setne al`). (checked)
- **Does:** pushes TRUE when the object's conversation resref (CCObject+0x3D8) has a first character, else FALSE. (checked)
- **State:** CCObject+0x3D8 is a resref (name bytes from +0x21, up to 32: 0x00A29F70) built empty by `CCObject`'s constructor (0x0069997B, in 0x00699670 which writes the vftable 0x00B0EE9C) and loaded from the GFF field `DialogResRef` (string 0x00B0FC5C) by the creature load (0x006C0B45); a placeable copies it (0x00751B7D). (checked) `BeginConversation` with no file argument falls back to it (0x00697722; inferred).
- **Edge cases:** invalid id, or an id that is not a `CCObject` (`GetObjectById`'s vtable +0x18 cast answers null: areas, the module, inferred) → FALSE, script goes on. (checked) Works for any object kind with the field (creature, placeable, item, trigger...); no type check. (checked) Only non-emptiness is tested: a resref naming a missing `.dlg` still answers TRUE. (checked) Nothing about the conversation being available (plot conditions, the object's state) is consulted. (checked)

## Not read
- Who writes `CVirtualMachineCommands+4` (it is a 0x10-byte object, ctor at 0x005DBE0C, which leaves +4 to someone else); the "server implementer" reading is inferred.
- Which script command sets a conversation at run time (writing +0x3D8 other than the load and the copy), and whether placeables and triggers load `DialogResRef` through the same field.
