# Script commands, group "players" (500, 513): the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent, b2b) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not. Pops and pushes as `dev/script_commands/Party.md` says (-639 a failed pop, -638 a failed push).

### IsHero (500)
- **Handler:** 0x00660A20 (`ScriptCommand::ISHERO`, not shared). Calls `GetObjectById` (0x005D5BE0) and the object's vtable +0x10 (as-creature). (checked)
- **Does:** pushes TRUE when the creature's byte +0x7D4 is exactly 1, else FALSE. (checked)
- **State:** creature+0x7D4 is the template's/save's `CharType` byte: the creature load reads GFF field `CharType` (string 0x00B10710) and stores it through the setter 0x0069E480 (`mov [ecx+0x7D4], al`) at 0x006C0A41. (checked) Other writers: the constructor sets 0xFF (0x006B419A) and a reset sets 0 (0x006B453E); 0x0069D910, the hero maker, sets 1 on the creature it builds (0x0069DA05; below); `GUICharGen::HandleMessage_CharGenStartAppearanceSceneMessage` sets 1 on the chargen creature (0x0073A076); 0x007662A0 copies one creature's value to another and then sets 0 (0x007663DA, 0x007663F5; inferred: a copy of the hero made for a scene). (checked for the writes)
- **Where the byte comes from** (read 2026-10-29 by lib-agent-2 with a subagent; a scan of `.text` for every store to +0x7D4: the three writers above and the setter's five callers 0x0069DA05, 0x006C0A41, 0x0073A076, 0x007663DA and 0x007663F5) [C unless marked]:
  - **The template's load** reads `CharType` with the current value as its default (0x006C0A16), so a template without it leaves the constructor's 0xFF. Of 2,486 readable `.utc`, only `aluviandarkstar.utc` has it (= 1); `default_player.utc` has none. 112 enciphered add-in templates were not searched. So a creature spawned from a template (a script's CreateObject) is not a hero unless the template has `CharType` 1; a mod's can [I].
  - **The hero** is built from `default_player` by 0x0069D910 (constructor 0x006B9ED0, size 0xD20), which sets 1 in code (`push 1; call 0x0069E480` at 0x0069DA05). It is called from chargen's end (0x0069DFB8, in 0x0069DF10) and from 0x0069DD16 (in 0x0069DA70, which has no direct caller).
  - **The save does not keep `CharType`:** neither the creature's writer (0x006B2470; `dev/SAVE_WRITER.md`) nor `gff4_fields.csv` has it. So a loaded hero gets its 1 only by a re-make through 0x0069D910's path [I: which path the save load takes was not read].
  - **Core:** `Gff3Schema.h` gives the `.utc`'s `CharType()`; `Savegame` has no such field, as the save has none.
- **Edge cases:** invalid id or non-creature → FALSE, script goes on. (checked) Any CharType other than 1 (0, 0xFF, other values) → FALSE. (checked) It is the stored flag, not "the controlled creature" or "the party leader": a follower being controlled is not the hero, and a copy of the hero keeps 1 only if not reset (inferred from the writers above).

### DoAutoSave (513)
- **Handler:** 0x00660B20 (`ScriptCommand::AUTOSAVE`, not shared). Pops nothing, always returns 0. (checked)
- **Does:** if the options' `AutoSave` setting is on (`ClientOptions` [0x00C32938]+0xC8, a SmartData bool read through 0x00838210, `mov al,[ecx+0xC]`; registered under the name "AutoSave" with default 1 at 0x005E8E46–0x005E8E5A), builds a `SaveGameMessage` (vftable 0x00B00098, ctor 0x005D4110) with +8 = -1 (slot: the handler chooses), +0xC = 1 (kind: autosave) and an empty name (L"" at 0x00ADEF14 into +0x10), and sends it straight to `SaveLoadProxy` ([app+0x150]) with `Messages::Send` (0x004D20C0), then destroys it (0x00735140). (checked; the slot/kind reading of +8/+0xC from `dev/SAVES.md`'s quicksave, inferred)
- **State:** what the proxy does with it (the may-save check `0x006CB740`, the autosave slot from proxy+0x7C, the slot folder `AutoSave_<n>`, the list rebuild that stalls with many saves) is `dev/SAVES.md` "Asking for a save or a load" and `dev/CRASHES.md`. (pointer)
- **The slot and the gates** (read 2026-10-23 by lib-agent-2 with a subagent) [C]:
  - **The slot:** the message carries slot −1 and kind 1. The proxy (0x006D1C60) refreshes the list and computes the slot (0x006D15D0 → 0x006D1160 → 0x006D0A50), reading it from proxy `+0x7C`.
    - It takes the first free slot of 1–4. With all four taken, it takes the oldest autosave (the list at `+0x134` is sorted newest first by the `.das` time).
    - So the slots rotate per character, in `Characters\<char>\Saves\AutoSave_<1-4>\` (`savegame.das`, `.das.met`, `screen.dds`).
  - **Gates:** it needs a game (`app+0x14C`) and the AutoSave option. It is not gated by the proxy's `+0x5C` (manual and quick saves only).
  - **0x006CB740:** false with no mode controller, or while a switch to loading (9) is queued (the vector at ctrl+0xA0 [I for what it is]). With the current mode loading, it is false in the loading mode's state 2 (a game load) or 3 (saving). It is true in every other mode, combat, dialogue and cutscene included: the exe refuses none of them.
- **Edge cases:** with AutoSave off in the options nothing is sent and nothing is reported to the script. (checked) It does not go through the game mode controller's `SaveGameMessage` handler (0x00640140) but to the proxy directly; whether the save is refused in combat, a conversation or a cutscene is the proxy's check, not read here. (checked for the destination; the refusal inferred) The save itself is written later by the loading game mode's worker (`dev/SAVES.md`), so the script continues before anything is on disk. (inferred)

## Not read
- `SaveLoadProxy::HandleMessage_SaveGameMessage` (0x006D1C60)'s autosave branch beyond `dev/SAVES.md`, and `0x006CB740`'s per-mode answer.
- Which of 0x0069D910's two callers the save load takes (0x0069DA70's own callers; probably a vtable).

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05). Not seen in play. "(checked)" = read in the disassembly; "(inferred)" = not. The application is the global 0x00C327EC; `[app+0xD0]` is the `CCPlayer`, whose +0x58 is the party and +0x5C the controlled creature's id (`dev/PARTY.md`). Push helpers: `StackPushObject` 0x00A2D980 (a failed push returns -638).

### GetHero (502)
- **Handler:** 0x00660AA0 (no arguments, not shared) (checked).
- **Described in:** `dev/CHARGEN.md` (the hero is the engine's player creature, `GetHero()` is every chargen event's object 0); `dev/script_commands/players.md`, "IsHero" for the `CharType` flag.
- **Does:** takes the party (0x005D57C0) and pushes its attribute at +0x1F8 (0x0062F390 = `lea +0x1F8` then the SmartData read 0x0053CC40), the hero's object id.
- **Edge cases:** no party (no game yet): pushes OBJECT_INVALID (-1) (`or esi,-1`) (checked). A party whose +0x1F8 is unset answers whatever the attribute holds, inferred -1 until the hero is made. It is the party's recorded hero, not the creature with `CharType` 1 that `IsHero` tests, nor the leader (`GetPartyLeader`, +0x1E8) nor the controlled one (inferred; the writer of +0x1F8 was not read).

### GetMainControlled (168)
- **Handler:** 0x00660AE0 (no arguments, not shared; the CSV names it `COMMAND_GETCONTROLLED`) (checked).
- **Described in:** `dev/PARTY.md`, the control and selection section (`CCPlayer`+0x5C, "main controlled"); `dev/LIBRARY.md` (`dao::party::Controlled`).
- **Does:** pushes the id at `[app+0xD0]+0x5C` (0x005D5CA0), the first of the selection: the creature the player moves and the party update follows.
- **Edge cases:** no `CCPlayer` or its +0x58 (party) null: OBJECT_INVALID (-1) (checked). The constructor sets +0x5C to -1 (`dev/PARTY.md`), so before anyone is controlled it is -1. With several selected it answers one id, while `IsControlled` is TRUE for every selected member.

### GetParty (83)
- **Handler:** 0x00651610 (not shared) (checked).
- **Described in:** nowhere in master's notes (script.ldf only: "Returns the object for the party").
- **Does:** pops one object argument and **ignores it**, then pushes OBJECT_INVALID (-1) with `StackPushObject`. It is a stub: it neither reads the party nor the creature (checked: no call between the pop and the push).
- **Edge cases:** always -1, whatever the creature, party or module state; only a failed pop (-639) or a failed push (-638) differs. No shipped script is known to rely on it (inferred; `GetPartyLeader` 624 and `GetPartyPoolList` are the working ones).

## Not read
- Who writes the party's +0x1F8 attribute (the hero) and when; whether `GetHero` is valid between `CCPlayer` creation and the first creature.
