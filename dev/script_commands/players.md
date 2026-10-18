# Script commands, group "players" (500, 513): the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent, b2b) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not. Pops and pushes as `dev/script_commands/Party.md` says (-639 a failed pop, -638 a failed push).

### IsHero (500)
- **Handler:** 0x00660A20 (`ScriptCommand::ISHERO`, not shared). Calls `GetObjectById` (0x005D5BE0) and the object's vtable +0x10 (as-creature). (checked)
- **Does:** pushes TRUE when the creature's byte +0x7D4 is exactly 1, else FALSE. (checked)
- **State:** creature+0x7D4 is the template's/save's `CharType` byte: the creature load reads GFF field `CharType` (string 0x00B10710) and stores it through the setter 0x0069E480 (`mov [ecx+0x7D4], al`) at 0x006C0A41. (checked) Other writers: the constructor sets 0xFF (0x006B419A) and a reset sets 0 (0x006B453E); 0x0069D910 sets 1 on a creature it creates (inferred: the player character); `GUICharGen::HandleMessage_CharGenStartAppearanceSceneMessage` sets 1 on the chargen creature (0x0073A076); 0x007662A0 copies one creature's value to another and then sets 0 (0x007663DA, 0x007663F5; inferred: a copy of the hero made for a scene). (checked for the writes)
- **Edge cases:** invalid id or non-creature → FALSE, script goes on. (checked) Any CharType other than 1 (0, 0xFF, other values) → FALSE. (checked) It is the stored flag, not "the controlled creature" or "the party leader": a follower being controlled is not the hero, and a copy of the hero keeps 1 only if not reset (inferred from the writers above).

### DoAutoSave (513)
- **Handler:** 0x00660B20 (`ScriptCommand::AUTOSAVE`, not shared). Pops nothing, always returns 0. (checked)
- **Does:** if the options' `AutoSave` setting is on (`ClientOptions` [0x00C32938]+0xC8, a SmartData bool read through 0x00838210, `mov al,[ecx+0xC]`; registered under the name "AutoSave" with default 1 at 0x005E8E46–0x005E8E5A), builds a `SaveGameMessage` (vftable 0x00B00098, ctor 0x005D4110) with +8 = -1 (slot: the handler chooses), +0xC = 1 (kind: autosave) and an empty name (L"" at 0x00ADEF14 into +0x10), and sends it straight to `SaveLoadProxy` ([app+0x150]) with `Messages::Send` (0x004D20C0), then destroys it (0x00735140). (checked; the slot/kind reading of +8/+0xC from `dev/SAVES.md`'s quicksave, inferred)
- **State:** what the proxy does with it (the may-save check `0x006CB740`, the autosave slot from proxy+0x7C, the slot folder `AutoSave_<n>`, the list rebuild that stalls with many saves) is `dev/SAVES.md` "Asking for a save or a load" and `dev/CRASHES.md`. (pointer)
- **Edge cases:** with AutoSave off in the options nothing is sent and nothing is reported to the script. (checked) It does not go through the game mode controller's `SaveGameMessage` handler (0x00640140) but to the proxy directly; whether the save is refused in combat, a conversation or a cutscene is the proxy's check, not read here. (checked for the destination; the refusal inferred) The save itself is written later by the loading game mode's worker (`dev/SAVES.md`), so the script continues before anything is on disk. (inferred)

## Not read
- `SaveLoadProxy::HandleMessage_SaveGameMessage` (0x006D1C60)'s autosave branch beyond `dev/SAVES.md`, and `0x006CB740`'s per-mode answer.
- What 0x0069D910 is (a creature made with `CharType` 1).
