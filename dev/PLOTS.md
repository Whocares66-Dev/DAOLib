# Plots: a flag read and set, and a conversation's conditions and actions

How the engine reads and sets a plot's flags and runs a conversation line's conditions and actions. Read 2026-10-18 in the GOG exe by lib-agent-2 (corrected the same day: the script's `OBJECT_SELF`, `nCallScript`), two subagents in parallel and checked against each other, for DAOParagonEngine's conversations (its S4). The plot files themselves are `dao/core/Plot.h`'s.

## Stored and defined flags

A flag of 0 to 127 is stored, in the plot's four 32-bit words; one of 128 and up is defined, the plot's script's answer, decided by the number alone (`0x0075eb5a`). The toolset numbers the defined ones from 256, so no shipped plot has a flag between 128 and 255.

- `GetPlotFlag` (`0x0075ea60`) runs the plot's script with `EVENT_TYPE_GET_PLOT` (28), and for a stored flag ignores its answer.
- `SetPlotFlag` (`0x0075f7e0`) runs it with `EVENT_TYPE_SET_PLOT` (27) before storing the value, and the script cannot veto.
- The caller's `nCallScript` governs only the stored flags, 0 to 127: with it 0 neither runs the script. For a defined flag both run the plot's script whenever the plot has one, whatever `nCallScript` says; with no script a defined flag reads FALSE, and setting one stores nothing. Conversations always pass 1.

The event (`0x0075bea0`) is run at once on the plot's script, with the hero as `OBJECT_SELF` (`0x00697310` called on the hero's creature):

- creator: the hero; target: `OBJECT_INVALID` (its +0x28 left at -1);
- string 0: the plot's GUID; string 1: its TemplateResRef;
- int 1: the flag; int 2: the new value; int 3: the old value; int 4: whether it is defined; int 5: the plot's PlotEntryType;
- object 0: the conversation's owner, the only place the owner is.

The answer is `GetRunScriptReturnValue` not 0, a script that returns nothing (-1) counting as true.

## A conversation line

A line's check is `0x007f0d30`, its action `0x007f0560`.

- `PlotCheck.test` is a bool byte: 255 or 1 for set, 0 for clear. For an action it is the value written.
- A script condition or action (`0x007f0420`, `0x007eedc0`) runs on the conversation's owner, its parameter kept at the client app's +0x210, where `GetConversationEntryParameter` (command 443) reads it. A condition passes on an int result that is not 0.

### Ambient conversations (barks), read 2026-10-18

Read in the GOG exe by lib-agent-2, for DAOParagonEngine #22 (Mouse's `MOUSE_JOINS_SHOUT` in the Harrowing). Not seen in play. The data: `bhm600_mouse_new.dlg`'s starting list is lines 0, 27–32 and 97, tried in order. Lines 27–31 have `ConversationLineAmbient` (12207) set and no children; line 31's condition is `MOUSE_JOINS_SHOUT` (flag 43). `bhm600cr_mouse.nss` sets that flag and calls `UT_Talk(OBJECT_SELF, OBJECT_SELF)`, which ends in `BeginConversation` with the creature's own conversation.

**Ambient is the starting line's flag alone.** `BeginConversation` (0x006976B0) builds the conversation (0x0068CEE0, a 0xA0-byte object in a `GameModeDialogData` with a force byte, 0 here) and asks for game mode 4, dialogue. Dialogue mode's entry (`GameModeDialog` vtable 0x00B0E3DC slot 5, 0x0068FDA0) calls `Conversation::Start` (0x007F2210), which answers `!IsLineAmbient(starting line)` (0x007EF660 reads 12207; 0x007F231E). When that is false and the force byte is 0, the entry plays the conversation itself (0x007F2370) and refuses the mode. So there is no dialogue mode, camera, pause or reply screen, and play and the AI go on. The arguments, the line count and the replies play no part. `BeginSlideshow` sets the force byte, so a slideshow enters dialogue mode even from an ambient line.

**How it plays.** `PlayLine` (0x007F1EF0), at the first ambient line, drops the player from the participants, sets the conversation's +0x35 (ambient) and hands the conversation to the ambient dialogue manager (`cambientdialogmanager.cpp`; singleton 0x00C35CF8 from 0x006FEFA0, StartAmbient 0x006FF9E0). The manager keeps an entry per owner, 0x38 bytes (0x006FE200): owner, speaker and listener, the conversation, a timer (+0x10), its start time (+0x18), the floaty (+0x20), RevertAnim (+0x24), the line (+0x28), and the `_m.fxe` and `.fev`. The line's action runs as in any conversation (`DialogIsAmbient()`, client app +0x214, is 1 in its script). Replies are never offered. When a line's time is up the manager calls `Advance(-1)`, which takes the first available reply child and plays its first available NPC child. So an ambient conversation does go on through its children, and ends (0x006FF020: the floaty and the VO stopped, `Conversation::End`) when nothing is left or it meets a line not marked ambient.

**Distance.** The line is shown only if the player's creature is in the speaker's area and within 35 m (0x00B1524C), and not if the speaker is in the open conversation (Arm, 0x006FF190). Otherwise the line is skipped silently and the next update advances.

**How long (Show, 0x006FE4A0).** The timer is set to 7.0 s (0x00B15250). Unless `ConversationLineNoVOIngame` (12216) is set, the VO plays with its lip sync on the speaker's voice object (0x00A5DA60). If it starts, the timer becomes 2.0 s (0x00AE1998), and the update (0x006FF570) then waits while the VO still plays. So with VO the line lasts the VO's length, at least 2 s; with none it lasts 7 s. Text length plays no part. While the update's gate is shut (client app +0xD8, the mode's slot +0x54; pause is a guess), every timer is zeroed once (0x006FE8A0).

**Where.** The text is a floaty over the speaker: a `MessageFloaty` (vtable 0x00B1CFA4, style 3, white, no drift; 0x0063FAF0), which the manager removes. It is not a subtitle. When VO plays, the floaty is made only if the subtitles option (`[0x00C32938]+0x78`) is above 1 (0x006FE74A); with no VO it is always made. A creature with more than five message floaties gets none. The speaker plays the line's `ConversationLineAnimation` (12213) if it is not -1 (vtable +0xB0, (anim, 1.0, -2.0, 1)). With `ConversationLineRevertAnim` (12217) set, animation 1 plays at the end. A speaker and listener that differ are made to face each other (0x0069EAA0) until the end.

**A second bark.** It replaces the first, and nothing is queued. StartAmbient clears the owner's entries first (Clear, 0x006FF8E0: those keyed by the id and those whose conversation involves it). When a line is armed and another owner's entry involves the same speaker, the newer conversation wins: an older entry is removed, and a line from an older conversation is refused (0x006FF312). A non-ambient line spoken in a normal conversation clears its speaker's barks. `ClearAmbientDialogs(o)` (0x006510F0) is Clear(o), or every entry for `OBJECT_INVALID`; `UT_Talk` calls it on the speaker first, and the talk trigger on the whole party.

Not read: the gates at 0x006976B0's start (object +0x2C5, 0x00695300), what the update's gate is, and the manager's other callers (0x005FEFD1, 0x005FFB79, 0x006BB0F1, 0x0069A1AF, 0x006B6E7A, 0x00694693, 0x006C5E9A, 0x007F1EB3; likely clears on death or area change). The manager's queue (+0x38) re-creates conversations at a line, probably from a save's `SavegameAmbientDialog*` fields (16530–16534).

The line's flag is `ConversationLine::ambient` in `dao/core/Conversation.h` (DAOLib #174).
