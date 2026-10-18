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
