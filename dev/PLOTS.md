# Plots: a flag read and set, and a conversation's conditions and actions

How the engine reads and sets a plot's flags and runs a conversation line's conditions and actions. Read 2026-10-18 in the GOG exe by lib-agent-2 (corrected the same day: the script's `OBJECT_SELF`, `nCallScript`), two subagents in parallel and checked against each other, for DAOParagonEngine's conversations (its S4). The plot files themselves are `dao/core/Plot.h`'s.

## Stored and defined flags

A flag of 0 to 127 is stored, in the plot's four 32-bit words; one of 128 and up is defined, the plot's script's answer, decided by the number alone (`0x0075eb5a`). The toolset numbers the defined ones from 256, so no shipped plot has a flag between 128 and 255.

- `GetPlotFlag` (`0x0075ea60`) runs the plot's script with `EVENT_TYPE_GET_PLOT` (28), and for a stored flag ignores its answer.
- `SetPlotFlag` (`0x0075f7e0`) runs it with `EVENT_TYPE_SET_PLOT` (27) before storing the value, and the script cannot veto.
- The caller's `nCallScript` governs only the stored flags, 0 to 127: with it 0 neither runs the script. For a defined flag both run the plot's script whenever the plot has one, whatever `nCallScript` says; with no script a defined flag reads FALSE, and setting one stores nothing. Conversations always pass 1.
- **`WR_SetPlotFlag` asks before it sets, so a defined flag's script runs twice: `EVENT_TYPE_GET_PLOT`, then `EVENT_TYPE_SET_PLOT`** (read 2026-10-22 by lib-agent-2, for DAOParagonEngine #164) [C]:
  - `WR_SetPlotFlag` (`wrappers_h.nss`:86) first reads the current value with `GetPartyPlotFlag(oParty, strPlot, nFlag, nCallScript)` (for its log line), then calls `SetPartyPlotFlag`. For a defined flag the get runs the plot's script with GET_PLOT whatever `nCallScript` is (above), so the script's GET branch runs on every `WR_SetPlotFlag` of a defined flag.
  - The engine's `SetPlotFlag` (0x0075F7E0) runs the script with event 27 (SET_PLOT) for a defined flag (`cmp edi, 0x7f` at 0x0075F944, the run at 0x0075F9B7..0x0075F9C1) and stores nothing. The GET comes from the wrapper, not from the engine's set. What the engine passes as int 3 (the old value) for a defined flag was not read.
  - **Example** (corrected 2026-10-22 by lib-agent-2 from paragon-agent-2's reading; this named `DOOR_TO_THIRD` as a defined flag): the mage origin's `bhm000pt_main` has its `GET_PLOT` cases on defined flags such as `PLAYER_WEARING_MAGE_ROBE` (263), which is what this applies to. `DOOR_TO_THIRD` is flag 17 of `plt_bhm000pt_main.plo`, a stored flag [C, the .plo], so with `WR_SetPlotFlag`'s default `nCallScript` FALSE neither its get nor its set runs the plot script, and the `UT_Talk` in its GET case (`bhm000pt_main.nss`:251) is not reached that way. Its only setter, `bhm200ip_to_level_3.nss`, is not the stair's script either: the template `bhm200ip_to_level_3.utp` has `EventScript` `placeable_core` (every copy: the level 2 and level 1 rims and `designerplaceables.erf`), and the stair is locked (`PickLock` 10000) [C; the area's placed instance's script not checked]. What starts the door templar's conversation in vanilla was not read.

The event (`0x0075bea0`) is run at once on the plot's script, with the hero as `OBJECT_SELF` (`0x00697310` called on the hero's creature):

- creator: the hero; target: `OBJECT_INVALID` (its +0x28 left at -1);
- string 0: the plot's GUID; string 1: its TemplateResRef;
- int 1: the flag; int 2: the new value; int 3: the old value; int 4: whether it is defined; int 5: the plot's PlotEntryType;
- object 0: the conversation's owner, the only place the owner is.

The answer is `GetRunScriptReturnValue` not 0, a script that returns nothing (-1) counting as true.

## A tutorial plot's flag and the tutorial box (read 2026-10-22 by lib-agent-2 with a subagent, for DAOParagonEngine #162)

- **The engine adds the tutorial when the flag is set; no script does** [C]:
  - **The scripts don't.** `plot_core.nss` reads `IsTutorial` and `IsCodex` into locals and never uses them. `ShowTutorial` (command 310, 0x00651780) does nothing. The `tutorials` table (four placeholder rows) is not used on this path: its NextID, TutorialStrRef, Image, X and Y play no part.
  - **The engine's check.** In `SetPlotFlag` (0x0075F7E0), when the value set is non-zero, the tail at 0x0076014A reads `plottypes[entry type].IsTutorial` (0x0075BCF0, hash 0x12E4743C). Types 13 (IsCodex 1) and 15 (IsCodex 0) have IsTutorial 1.
  - **The gate.** When IsTutorial is set and `ShowTutorials` (`ClientOptions` +0x98, read by 0x00838210, at 0x00760156) is on, it calls `TutorialControl::AddTutorial` (0x00693F10) on the module's +0x498.
    - **The tail runs only when the flag changes** (read 2026-10-22 by paragon-agent-2's helper, checked by lib-agent-2, for DAOParagonEngine #183) [C]: in `SetPlotFlag` (0x0075F7E0) the old value is read by the call to `0x0075BC70` at 0x0075F93F and kept at 0x0075F947 (before the plot script runs and before the store `0x0075BC40` at 0x0075FBA5), and the whole tail (the journal and plot-destination work, then the tutorial add at 0x00760152..0x007601CA) sits inside two nested tests: the value set is non-zero, and the value set differs from the old one (the byte compare at 0x0075FC78, its `je` at 0x0075FC7F). So setting a flag that is already TRUE to TRUE adds no tutorial and does not set flag + 1. This is the only "seen" record: the flag itself.
    - **The scripts' guard is the flag's own change** [C]: `module_core.nss` (game mode change to combat) calls `WR_SetPlotFlag(PLT_TUT_COMBAT_BASIC_MAGIC or PLT_TUT_COMBAT_BASIC, flag 1, TRUE)` at every combat, with no `WR_GetPlotFlag` test (only when the module int `TUTORIAL_ENABLED` is set); `gen_tut_enemyrank.nss` tests `WR_GetPlotFlag(PLT_TUT_COMBAT_ENEMY_RANKS, ..._1) == FALSE` first. The first set queues the pages; every later one is a no-op by the change test above.
    - **A defined flag's old value** [I]: for a flag of 128 and up the old value `0x0075BC70` reads was not traced; tutorial plots have stored flags only.
    - **ShowTutorials off** [C]: the flag is still stored on the change, so turning the option on later does not bring the pages back.
  - **The entry's fields come from the plot's file:** the title is the plot's `PlotName` (13008), the text the flag's `PlotFlagJournal` (13004), and `CodexResRef` the plot's resref. `PlotAllowPausing` (13023) decides the pause.
  - **The next page.** Whether or not ShowTutorials is on, it then sets flag + 1 to 1 (`SetPlotFlag` again, 0x007601C5) while the plot has that flag (0x0075F640) [C for the call, I for 0x0075F640's meaning]. So each flag is one page, and setting the first flag adds all the plot's pages in one go.
  - **Where an entry goes:** before the first entry with another `CodexResRef`, or at the end (the loop is read, the compare's convention is [I]). There is no check for duplicates and no record of what was seen.
    - **Where an entry goes** (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) [C for the loop, I for the compare's convention]: `AddTutorial` (`0x00693F10`, `this` the control; it does nothing when the party's +0x210 is 0) builds the `Tutorial` (0xE8 bytes) from `0x0075ED50` (the plot's name), `0x0075F520` (the flag's journal text) and `GetPlotResRef`, then loops i over the list at +0x58: for each existing entry it reads the entry's wide string at +0xC0 and calls `0x004C0E50(newCodex)` (the inverse of `0x004C0DE0`: both call `0x00414170`, the first returns "result != 0", the second "result == 0"); on the first true it inserts the new entry at index i (`0x006CE860`, which appends when i is past the end) and stops; when the loop ends with none it appends (`0x006F1340`). `0x00414170` was not read; if it returns 0 for equal strings (as `0x004C0DE0` uses it as an equality test), the new entry goes before the first entry whose codex resref differs, at the end when none does, and an empty list appends.
  - **After the add** it opens the PopupLayer (scene 0x1E) [I for the exact call], and pauses when `AllowPausing` is set and the mode is explore or combat.
    - **After the add** [C]: the list's change notice (vtable slot 10 of the list attribute), then the GUI (`app`+0x158) slot at vtable +0x60 is asked for scene 0x1E; if it exists and its byte at +0xBC is 0, its vtable +0x14 is called (the open call: its identity is I). Then, only when `0x0075F350(plot)` is non-zero (the plot's AllowPausing; the field was not re-read here [I]) and `GameModeController::GetGameMode` (`0x005D5E70`) is 0 or 1, +0x84 is set to 1 and `SetPaused(1, 0)` runs. So the popup call comes before the pause test.
- **`Tutorials.ConsumeTutorial`** (the handler 0x00693630) [C]:
  - 0 removes and deletes the list's head; 1 clears the whole list (the loop repeats while the message's byte at +4 is non-zero).
  - When the list is then empty and the control's +0x84 (paused by it) is set, it unpauses.
  - It marks nothing seen, adds no codex entry (that came with the flag) and does not follow `NextID`.
- **The gates** [C]:
  - **ShowTutorials** gates only the adding. Turning it off clears nothing.
  - **`Tutorials.Enabled`** is the control's +0x74, 0 from the constructor, set by `SetEnabled` (0x00692E60):
    - 1 on entering explore (0x0070E820), and combat's enter calls it too;
    - 0 on leaving explore (0x00711480), except into the GUI mode (3), or into loading (9) under a further test;
      - **Leaving explore `0x00711480`** (`GameModeExplore` slot 8, argument the mode being left for) [C]: nothing is done when the argument is 3. When it is 9, nothing is done only if `(*(*(*(app+0x114)+0x58)+0x24))->vtable[0x34]()` answers the short 3 (`app`+0x114 is the game mode controller); in every other case `SetEnabled(0)` (`0x00692E60`) is called on the module's tutorial control when the module and its +0x498 exist. What that vtable call returns (a 3 meaning a GUI mode is the overlay being returned to, or something else) was not read [I].
    - 0 on leaving the GUI mode (0x007188C0), whatever comes next, and on the main menu's slot 5 (0x0072E130).
    - So Enabled is 1 only after explore or combat has been entered. The scripts' `TUTORIAL_ENABLED` module int is unrelated.
  - **The movie** shows a box while the list has members and Enabled is non-zero (`PopupLayer.as`:190–195).
- **The control's layout** [C]: the list +0x58, Enabled +0x74, paused +0x84, the training step +0x8C. A tutorial: title +0x58, text +0x8C, `CodexResRef` +0xC0.

## A conversation line

A line's check is `0x007f0d30`, its action `0x007f0560`.

How long a line without a cut shows is `dev/AUDIO.md`'s ("A conversation line without a cut", lib-agent-2): a voiced line's timer is its voice's length less 0.3 s, and a line without voice waits for a click.

- `PlotCheck.test` is a bool byte: 255 or 1 for set, 0 for clear. For an action it is the value written.
- A script condition or action (`0x007f0420`, `0x007eedc0`) runs on the conversation's owner, its parameter kept at the client app's +0x210, where `GetConversationEntryParameter` (command 443) reads it. A condition passes on an int result that is not 0.

### Ambient conversations (barks), read 2026-10-18

Read in the GOG exe by lib-agent-2, for DAOParagonEngine #22 (Mouse's `MOUSE_JOINS_SHOUT` in the Harrowing). Not seen in play. The data: `bhm600_mouse_new.dlg`'s starting list is lines 0, 27–32 and 97, tried in order. Lines 27–31 have `ConversationLineAmbient` (12207) set and no children; line 31's condition is `MOUSE_JOINS_SHOUT` (flag 43). `bhm600cr_mouse.nss` sets that flag and calls `UT_Talk(OBJECT_SELF, OBJECT_SELF)`, which ends in `BeginConversation` with the creature's own conversation.

**Ambient is the starting line's flag alone.** `BeginConversation` (0x006976B0) builds the conversation (0x0068CEE0, a 0xA0-byte object in a `GameModeDialogData` with a force byte, 0 here) and asks for game mode 4, dialogue. Dialogue mode's entry (`GameModeDialog` vtable 0x00B0E3DC slot 5, 0x0068FDA0) calls `Conversation::Start` (0x007F2210), which answers `!IsLineAmbient(the line PlayLine reached after any skipped lines, +0x3C)` (0x007EF660 reads 12207; 0x007F231E). When that is false and the force byte is 0, the entry plays the conversation itself (0x007F2370) and refuses the mode. So there is no dialogue mode, camera, pause or reply screen, and play and the AI go on. The arguments, the line count and the replies play no part. `BeginSlideshow` sets the force byte, so a slideshow enters dialogue mode even from an ambient line.

**How it plays.** `PlayLine` (0x007F1EF0), at the first ambient line, drops the player from the participants, sets the conversation's +0x35 (ambient) and hands the conversation to the ambient dialogue manager (`cambientdialogmanager.cpp`; singleton 0x00C35CF8 from 0x006FEFA0, StartAmbient 0x006FF9E0). The manager keeps an entry per owner, 0x38 bytes (0x006FE200): owner, speaker and listener, the conversation, a timer (+0x10), its start time (+0x18), the floaty (+0x20), RevertAnim (+0x24), the line (+0x28), and the `_m.fxe` and `.fev`. The line's action runs as in any conversation (`DialogIsAmbient()`, client app +0x214, is 1 in its script). Replies are never offered. When a line's time is up the manager calls `Advance(-1)`, which takes the first available reply child and plays its first available NPC child. So an ambient conversation does go on through its children, and ends (0x006FF020: the floaty and the VO stopped, `Conversation::End`) when nothing is left or it meets a line not marked ambient.

**Distance.** The line is shown only if the player's creature is in the speaker's area and within 35 m (0x00B1524C), and not if the speaker is in the open conversation (Arm, 0x006FF190). Otherwise the line is skipped silently and the next update advances.

**How long (Show, 0x006FE4A0).** The timer is set to 7.0 s (0x00B15250). Unless `ConversationLineNoVOIngame` (12216) is set, the VO plays with its lip sync on the speaker's voice object (0x00A5DA60). If it starts, the timer becomes 2.0 s (0x00AE1998), and the update (0x006FF570) then waits while the VO still plays. So with VO the line lasts the VO's length, at least 2 s; with none it lasts 7 s. Text length plays no part. While the update's gate is shut (client app +0xD8, the mode's slot +0x54; pause is a guess), every timer is zeroed once (0x006FE8A0).

**Where.** The text is a floaty over the speaker: a `MessageFloaty` (vtable 0x00B1CFA4, style 3, white, no drift; 0x0063FAF0), which the manager removes. It is not a subtitle. When VO plays, the floaty is made only if the subtitles option (`[0x00C32938]+0x78`) is above 1 (0x006FE74A); with no VO it is always made. A creature with more than five message floaties gets none. The speaker plays the line's `ConversationLineAnimation` (12213) if it is not -1 (vtable +0xB0, (anim, 1.0, -2.0, 1)). With `ConversationLineRevertAnim` (12217) set, animation 1 plays at the end. A speaker and listener that differ are made to face each other (0x0069EAA0) until the end.

**A second bark.** It replaces the first, and nothing is queued. StartAmbient clears the owner's entries first (Clear, 0x006FF8E0: those keyed by the id and those whose conversation involves it). When a line is armed and another owner's entry involves the same speaker, the newer conversation wins: an older entry is removed, and a line from an older conversation is refused (0x006FF312). A non-ambient line spoken in a normal conversation clears its speaker's barks. `ClearAmbientDialogs(o)` (0x006510F0) is Clear(o), or every entry for `OBJECT_INVALID`; `UT_Talk` calls it on the speaker first, and the talk trigger on the whole party.

Not read: the gates at 0x006976B0's start (object +0x2C5, 0x00695300), what the update's gate is, and the manager's other callers (0x005FEFD1, 0x005FFB79, 0x006BB0F1, 0x0069A1AF, 0x006B6E7A, 0x00694693, 0x006C5E9A, 0x007F1EB3; likely clears on death or area change). The manager's queue (+0x38) re-creates conversations at a line, probably from a save's `SavegameAmbientDialog*` fields (16530–16534).

The line's flag is `ConversationLine::ambient` in `dao/core/Conversation.h` (DAOLib #174).

### A conversation line's visibility, speaker and skip: read 2026-10-18

Read in the GOG exe, saves and the game's conversations by lib-agent-2 (with subagents) for DAOParagonEngine's Harrowing. Not seen in play. The key claims below were checked in the disassembly.

### Once per game: the line's string id, in the party, saved

- **What is recorded.** A line whose `ConversationLineVisibility` (12206) is 2 is recorded when played (0x007F1700). The record is its text's talk-table string id: 0x007EF520 reads `ConversationLineText` (12201) into a LocString with the strref at +4 (0x007F178C). There is no line index or conversation in it.
- **Where it is kept.** In the party (`[app+0xD0]+0x58`) at +0x2F0, a vector of u32, appended if missing.
- **The save.** It is saved and loaded as **16503 `SavegamePartySeenLines`**, a list of UINT32 (writer 0x006346A8, reader 0x00637F80). Real saves hold talk-table ids of spoken lines; empty-text lines are recorded too, by their own strref.
- **A consequence** (inferred): two lines that share a strref share the record.
- **Once per conversation** (Visibility 1) is kept on the Conversation object at +0x60: a vector of line **indices**, written by 0x007F1640, tested at 0x007F05FD, and freed when the conversation ends. So it lasts one run of the conversation.

### An inactive speaker never speaks: the owner does, unless the line activates it

- **The tag lookup** 0x006973B0 (`__thiscall` (tag, includeInactive, maxDist, found*), `ret 0x10`):
  - It searches only the owner's area's objects, keeping one with the tag that is active, or any if `includeInactive` is set.
  - Active means 0x00699FA0: +0x2C5 set and +0x421 (being turned off) clear.
  - It returns the nearest, or -1.
- **The speaker and listener** (`LineSpeaker` 0x007F0210, `LineListener` 0x007F02F0) pass `includeInactive` 0 (0x007F0289). So a tag naming an inactive creature resolves to -1, and the speaker becomes the conversation's owner. `OWNER`, `PLAYER` and `HENCH` have no active filter.
- **The line's own switches are applied first.** `ConversationLineActive` (12500) is a list of (tag 12102, value 12103). 0x007F0620(line, phase) resolves each tag **including inactive** objects, and switches it with the object's slot 0x140 (as `SetObjectActive` does; no animation, no effect):
  - Phase 0, from `PlayLine` at 0x007F1FC1, before the speaker is resolved at 0x007F1FE1: 1 activates, 3 deactivates.
  - Phase 1, from `Advance` (0x007F1939), after the line: 2 activates, 4 deactivates.
  - So a line can turn its own speaker on before it speaks.
- **Mouse** (`bhm600_mouse_new.dlg`):
  - The first talk (start line 97, the rat) ends line 102 with the rat switched off (value 4). Player line 232's action sets `MOUSE_TURNS_HUMAN` (34), whose plot case (`bhm600pt_harrowing.nss`:117–127) turns the rat off and the human on before line 103, the human's first.
  - Lines 1, 2 and 32–34 switch the human on (value 1) at their start.
  - The shouts 27–31 (ambient, the human as speaker) have no switch, and the human is off then (`bhm600pt_harrowing.nss`:134–135). So the rat, the owner, speaks them (inferred, from the above).
  - `bhm600cr_mouse.nss`:35–44 moves the human to the speaking creature's spot on DIALOGUE but doesn't activate him.

### The skip: no text and no cut is passed over

`PlayLine` (0x007F1EF0) passes over a non-ambient NPC line when `Skip || (text empty && no Cut)`. An ambient line is passed over on empty text alone.
- 0x007EFC70 is `ConversationLineSkip` (12214) ≠ 0 (checked).
- 0x007EFC30 is whether `ConversationLineCut` (12211) holds a cutscene.
- Text empty means the resolved talk-table string's length is 0.
- Voice-over and the speaker play no part.

A passed-over line:
- is not shown: the dialogue mode (0x0068E250) and the ambient manager are not called;
- still has its visibility recorded, becomes the current line (+0x38), and runs its action (0x007F0560: the script and plot flag);
- then **offers the replies at once** (0x007F1A40), with no wait.

The conversation's +0x90 counts the lines passed in a row.

`OfferReplies` keeps the available replies with text. The first available reply with empty text is held back as a "continue":
- If no reply has text and the NPC line was passed over, it **Advances through that empty reply at once** (0x007F1E78), so the next NPC line plays with nothing shown.
- If the NPC line has text, the empty reply is offered as the continue.

A conversation's first line (+0x3C = -1) that passes the test goes the same way.

**The data** (every `.dlg` in `designerdialogs.erf`):
- 7,806 of 50,472 NPC lines have empty text, each with a real strref; 778 of them are non-ambient starting lines. `alistair_main.dlg` line 0 (OWNER, strref 234570 = "", one child, an empty reply) is the routing pattern.
- Skip is set on 271 NPC lines, almost all with text and a Cut.
- `Dialogue.cpp`'s empty "[OWNER]" line seen at Highever is likely this pass-through as the hooks see it.

Not read: where `OfferReplies` is called after a shown line ends. A Skip line's Cut never plays (below).

A line's switches are `ConversationLine::switches` in `dao/core/Conversation.h` (DAOLib #178).

### The replies' order: a fast-path reply first (read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe and the game's files by lib-agent-2 (with subagents) for DAOParagonEngine. Not seen in play. The key claims were checked again in the disassembly.

- `OfferReplies` (0x007F1A40) keeps the NPC line's children in `ConversationLineChildrenList` order: only `RPLY` lines (0x007F1BA9), available (0x007F0D30), with text. The empty "continue" is offered only when no reply has text.
- **`GameModeDialog::ShowResponses`** (0x00690B70, `__thiscall (list, flag)`, `ret 8`; the flag the NPC line's `NoVOIngame`):
  - It adds the first reply whose `ConversationLineFastPath` (12215) is set (0x00690C9D) first, and rotates it to index 0 of its kept list (+0x1A8; 0x00690D35–0x00690DE8).
  - Then it adds the rest in order. That index is the one the reply's selection uses.
- `conversation.gfx` (`Conversation.UpdateResponses`) doesn't sort, and shows at most 6.
- So `bhm400_greagoir.dlg` line 3 (children 11, 14, 15, all `Icon` 0; only 15 fast-path) shows 15, 11, 14.
- 2,287 lines have FastPath, all replies; 277 NPC lines have two or more fast-path children, and only the first available one moves (inferred from the code).

## A line's cut, as the engine plays it (read 2026-10-18 by lib-agent-2)

`ConversationLineCut` (12211) is a generic (type 0xFFFF, flags 0x2000) at the line's +0x4C, null as `FFFFFFFF FFFFFFFF`. Otherwise it holds a struct told apart by its template's fourcc: a `CUTR` (12210 `ConversationLineCutResRef`, a `.cut` by name) or a `CUT ` (a whole cutscene held in the line). Core's `Conversation` already reads both (`ConversationLine::cutscene`, `cutsceneInLine`, `ConversationFile::LineCutscene`). It doesn't read two fields the engine passes to the cut: 12100 `ConversationStageName` (string, +0x5C) and 12104 `ConversationStageAtCurrentLocation` (U8, +0x64).

- **The test** 0x007EFC30 is the generic's being non-null (0x007EFC55, checked), whatever its kind. Only `PlayLine` and `OfferReplies` use it.
- **A Skip line's cut never plays.** `PlayLine` passes over a line with Skip ≠ 0 at 0x007F20E5–0x007F20EC (checked) before the cut is looked at. In the campaign's 1,721 conversations, 365 lines with a cut have Skip set; no reply has a cut.
- **The player** (`Conversation::MakeLineCutscene` 0x007F0960, from `GameModeDialog`'s ShowLine 0x0068E250 at 0x0068E53C) is built as `LoadCutscene`'s is: from `<name>.cut` for a `CUTR` (0x007E26E0), from the line's struct for a `CUT ` (0x007E2A70, named `StringID_<strref>.___`). Its actors' `CutsceneActorMappingTag`s (5216) resolve through the conversation's own tag resolver (0x007F2D80, as a speaker's: OWNER, PLAYER, a tag), and `HENCHMAN` takes the next henchman in turn (0x007E36F0–0x007E3847).
- **ShowLine's cut branch** stops and deletes the previous line's player (dialog +0x208), keeps the new one there, attaches it as cutscene mode does (0x006C4AF0), sets the stage by 12100 (0x007D2AA0) and starts it (0x007CD0D0). With 12104 set, the stage is placed at the speaker (0x007CD040). It skips the facing (0x0069EAA0) and the dialogue camera (0x0068D4B0) a line without a cut gets.
- **The mode stays dialogue (4)**: the branch asks for no game mode, so the timers aren't held and there is no cutscene screen.
- **It holds its last frame:** the player's +0x110 is set to 1 (0x007F0B2B), so its end doesn't stop it; dialogue deletes it when the next line comes. Read 2026-10-18 by lib-agent-2.
- **The line's length:** the line timer (dialog +0x2C4) is `CutsceneRunTime` (5000) − 0.3 s (0x00C01810). It is 0.7 s when `CutscenePlayUntilVoCompletes` (5018) and the line's `NoVOIngame` (12216) are both set. The dialogue update (0x0068DDD0) advances the player (0x007E3D30) until it is finished (0x007CD290): failed to load, or ended with no line still speaking (the player's +0xE8, the count of its `ASLN`s' voices still playing). The replies are offered when the timer runs out or the cut finishes. An empty continue as the only reply is taken by itself once the cut has finished (0x0068DED1). **This is the only place 5018 matters:** "finished" has only dialogue mode's callers, and the cut's clock never waits for a voice, so a full cutscene ends at its `RunTime` whatever 5018 says (`dev/CUTSCENES.md`, "Spoken lines"). Read 2026-10-18 by lib-agent-2.
- **The voice** comes from the cut's own speak-line actions (`ASLN`, type 23), not the line's: the line's own voice isn't played, and the subtitles go to the conversation screen, its `NPCLine` with `HasVO`. The `ASLN`'s voice and face are `dev/CUTSCENES.md`'s. Read 2026-10-18 by lib-agent-2.
- **The skip** (0x0068DC10) first seeks to the cut's next skip marker (`ASMK`, type 47); with none left it sets the player's skip flag (+0x1B4) and offers the replies, the player still running. Read 2026-10-18 by lib-agent-2.
- **The end script is never run** for a line's cut: its only caller is cutscene mode's slot 8 (0x006925E9). Read 2026-10-18 by lib-agent-2.
- **Its creatures at the end:** a mapped actor drives the area's creature's model, and when the player is deleted a creature not moved by a script since binding goes to the cut's `FinalPos` placed by the stage, `S.p + R(S.q)·FinalPos`, facing `FinalOri` turned by it and flattened (0x007E1396–0x007E14CA); one a script moved stays where it is. The player's creature is put back where it stood instead, when the line has `StageAtCurrentLocation`. So `jowan_attacks`' creatures end at their `FinalPos`. Read 2026-10-18 by lib-agent-2; the stage is `dev/CUTSCENES.md`'s.

### The Circle's blood-magic scene

In `bhm100_cutscene_blood.dlg`, lines 0 and 1 have empty text and no cut (`jowan_attack` is their `ConversationStageName`), and lines 2–7 hold inline cuts with Skip 255. So in vanilla the whole conversation runs through with nothing shown, to reply 15's flag 27 `CUTSCENE_BLOOD_MAGIC_END` (inferred from the data and the code; not seen in play). The scene itself is `bhm100_jowan.dlg` line 23's `CUTR` `jowan_attacks` (51.8 s). Its reply 242 sets flag 26 `CUTSCENE_BLOOD_MAGIC`, whose case (`bhm000pt_tranquility.nss`:262) starts the blood conversation.
