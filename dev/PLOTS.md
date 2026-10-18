# Plots: a flag read and set, and a conversation's conditions and actions

How the engine reads and sets a plot's flags and runs a conversation line's conditions and actions. Read 2026-10-18 in the GOG exe by lib-agent-2, two subagents in parallel and checked against each other, for DAOParagonEngine's conversations (its S4). The plot files themselves are `dao/core/Plot.h`'s.

## Stored and defined flags

A flag of 0 to 127 is stored, in the plot's four 32-bit words; one of 128 and up is defined, the plot's script's answer, decided by the number alone (`0x0075eb5a`). The toolset numbers the defined ones from 256, so no shipped plot has a flag between 128 and 255.

- `GetPlotFlag` (`0x0075ea60`) runs the plot's script with `EVENT_TYPE_GET_PLOT` (28) for every flag, and for a stored one ignores its answer.
- `SetPlotFlag` (`0x0075f7e0`) runs it with `EVENT_TYPE_SET_PLOT` (27) before storing the value, and the script cannot veto.

The event (`0x0075bea0`) is run at once on the plot's script:

- creator: the hero;
- string 0: the plot's GUID; string 1: its TemplateResRef;
- int 1: the flag; int 2: the new value; int 3: the old value; int 4: whether it is defined; int 5: the plot's PlotEntryType;
- object 0: the conversation's owner.

The answer is `GetRunScriptReturnValue` not 0, a script that returns nothing (-1) counting as true.

## A conversation line

A line's check is `0x007f0d30`, its action `0x007f0560`.

- `PlotCheck.test` is a bool byte: 255 or 1 for set, 0 for clear. For an action it is the value written.
- A script condition or action (`0x007f0420`, `0x007eedc0`) runs on the conversation's owner, its parameter kept at the client app's +0x210, where `GetConversationEntryParameter` (command 443) reads it. A condition passes on an int result that is not 0.
