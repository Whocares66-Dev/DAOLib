# Script commands, group "waypatches": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not.

Shared facts (checked unless marked):
- Handlers are `__thiscall (commandId, paramCount)`, `ret 8`, on `CVirtualMachineCommands`; -639 (0xFFFFFD81) a failed pop, -638 a failed push, both abort the script (`dev/script_commands/commands.md`).
- **`CVirtualMachineCommands::ExecuteCommand`** (0x0064DD60, vftable 0x00B0030C slot 2) calls `table[id]` for an id below 0x35F (863). An id at or past 863, or a slot holding null, returns **-642** (0xFFFFFD7E) without popping anything. `InitializeCommands` (0x00675210) zeroes all 863 slots before registering. -642 is the same code the VM uses for a division by zero (`dev/NCS.md`), so the script aborts.
- A map pin is a **waypoint** (`CCWayPoint`, vftable 0x00B227CC), looked up with `CClientExoApp::GetWayPointById` (0x005D5C60: the object array's lookup, then the object's slot +0x30 cast; null for anything else, inferred from the shape). Its template fields (`CCWayPoint` load around 0x007C4583): `HasMapNote` → +0x440, `MapNoteEnabled` → +0x444, `MapNoteType` → +0x448 (a row of `waypointtypes`, table 106, whose `Icon` column, hash 0xBCDDB871, is read into +0x480), `MapNote` (a localized string). A save keeps them as `SavegameWaypointMapNote`/`…Enabled`/`…Text`/`…Type`/`…LocText` (GFF4 16710–16714; 0x4146 read at 0x007C46C5). (checked)
- **The shown-pins list.** A singleton `MapNoteList` (pointer at 0x00C38D30, made on first use by 0x007C4300, ctor 0x007C3F90) holds at +0x58 a SmartData `Attribute<ECDynArray<CCWayPoint*>, …, ObserverNotification>`. 0x007C4370 puts the waypoint in that list when HasMapNote (+0x440) ≠ 0, MapNoteEnabled (+0x444) ≠ 0, its area id (+0x254) ≠ -1 and that area is the area of the creature whose id is at `[app+0xD0]+0x5C` (0x005D5CD0; inferred: the player character); otherwise it takes it out (0x004E9640, inferred: erase by value). Adding skips a waypoint already there (0x0063C4B0 find). The list is locked (0x00403C70) and unlocked through its slot +0x24, which notifies observers (inferred: the map screen). 0x007C4370 is also called at the end of the waypoint's load (0x007C4860) and from its area-set override 0x007C4910 (slot 0x154). (checked)

### SetMapPatchState (649)
- **Handler:** none. `InitializeCommands` registers id 649 at 0x00676851 with a **null function** and the name `COMMAND_SETAREAPROPERTIES` (0x00B0BE44), not script.ldf's name. (checked)
- **Does:** nothing. `ExecuteCommand` finds the null slot and returns -642 before popping. (checked)
- **State:** none.
- **Edge cases:** **any call aborts the calling script** with -642, whatever the arguments. No game script calls it (the toolset's 2,101 scripts grepped), and there is no map-patch object type behind it. script.ldf's states (-1 invisible, 0–N shown, clickable by action id) describe nothing the engine has. (checked; "no map-patch type" inferred)

### GetMapPatchState (650)
- **Handler:** none. Registered at 0x00676BD6 with a **null function** and the name `COMMAND_SETMAXITEMSTACKSIZE` (0x00B0B998). (checked)
- **Does:** nothing; returns -642 before popping, so nothing is pushed. (checked)
- **State:** none.
- **Edge cases:** any call aborts the script with -642. No game script calls it. (checked)

### SetMapPinState (652)
- **Handler:** 0x0065ED90 (not shared). It calls `CClientExoApp::GetWayPointById` (0x005D5C60) and `CCWayPoint::ForScript_SetMapPinState` (0x007C4930). (checked)
- **Does:** pops the object, then nEnable. For a waypoint it stores `nEnable != 0` (so 0 or 1) at +0x444 (MapNoteEnabled) and calls 0x007C4370, which adds the waypoint to the shown-pins list or takes it out (shared facts). Nothing else: no event, no GUI call of its own. (checked)
- **State:** waypoint +0x444 written; `MapNoteList`+0x58 changed. `GetMapPinState` (653, 0x0065EE00) reads +0x444 back, -1 for a non-waypoint. (checked)
- **Edge cases:**
  - Any non-zero nEnable is stored as 1. (checked)
  - A non-waypoint or invalid object does nothing; no failure. Returns 0. (checked)
  - Enabling a waypoint with HasMapNote 0 stores the flag but shows nothing: 0x007C4370 takes it out of the list. (checked)
  - Enabling a pin in another area than the player's stores the flag but does not list it. It is listed when the waypoint's area is set again (0x007C4910) or reloaded (0x007C4860); whether an area change by the player alone re-lists it was not traced. (checked apart from the last clause)
  - The game's scripts call it 81 times (`SetMapPinState(GetObjectByTag("…"), TRUE)` style). (checked by grep)

## Not read
- The map screen's side: how the GUI reads `MapNoteList` and draws a pin's icon and `MapNote` text.
- Whether the shown-pins list is rebuilt when the player moves to another area (beyond the two callers of 0x007C4370 above).
- `GetPlayerMapPatch` (651), `GetMapPinState` (653) and `GetPlayerMapPin` (654) beyond the one-line read of 653 above.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe. Not seen in play. Shared facts (the map pin is a `CCWayPoint`; the null-slot rule of `ExecuteCommand`; the shown-pins list): top of `waypatches.md`.

### GetPlayerMapPatch (651)
- **Handler:** none. No row in `dev/script_commands.csv` and no registration in `InitializeCommands`: `dev/SCRIPT_COMMANDS.md` lists it among the 3 "declared, not registered in this exe". (checked: 652, 653 and 660 are registered at 0x00678DD0, 0x00678DE6 and 0x00678DFC, 22 bytes apart, which is the size of one registration, so none lies between them; 651 and 654 are in neither table)
- **Described in:** `dev/SCRIPT_COMMANDS.md` (the count only), `waypatches.md` (the shared fact on null slots).
- **Does:** nothing. Slot 651 stays zero from the table's clearing, so `ExecuteCommand` (0x0064DD60) returns **-642** before popping anything (the rule in `waypatches.md`).
- **Edge cases:** any call aborts the calling script, whatever the arguments; nothing is pushed. script.ldf's "Nth object of a patch tag a player has" has no engine behind it. Like 649 and 650 there is no map-patch object behind it either. No script in the toolset calls it (inferred from `waypatches.md`'s grep for its siblings).

### GetMapPinState (653)
- **Handler:** 0x0065EE00 (not shared). `CClientExoApp::GetWayPointById` 0x005D5C60, `StackPushInteger`.
- **Described in:** `waypatches.md` (the "State" line of `SetMapPinState`: "`GetMapPinState` reads +0x444 back, -1 for a non-waypoint") and `dev/SCRIPT_COMMANDS.md` (the field list, `CCWayPoint` +0x444).
- **Does:** pops oMapPin and pushes the int at the waypoint's +0x444 (`MapNoteEnabled`), unchanged. (checked)
- **State:** read only.
- **Edge cases:** an invalid id, or an object that is not a waypoint, pushes **-1** (the result starts at -1, checked) and the script goes on. A waypoint answers whatever its +0x444 holds: 0 or 1 after a `SetMapPinState`, else the template's `MapNoteEnabled`; it is **not** limited by `HasMapNote` (+0x440), so a waypoint with no map note still answers its +0x444. script.ldf says TRUE or FALSE; -1 is outside that. A failed pop is -639, a failed push -638.

### GetPlayerMapPin (654)
- **Handler:** none, as 651: not in the CSV, not registered (`dev/SCRIPT_COMMANDS.md`). 653 is registered at 0x00678DE6 and 660 at 0x00678DFC, 22 bytes on, the size of one registration, so nothing was registered for 654 between them. (checked)
- **Described in:** `dev/SCRIPT_COMMANDS.md` (the count only).
- **Does:** nothing; `ExecuteCommand` returns **-642** for the empty slot.
- **Edge cases:** any call aborts the script before a pop; nothing is pushed. No game script calls it (the same grep). script.ldf's "Nth object of a pin tag for a player" is not implemented.

## Not read
- Nothing further in the handlers; `GetPlayerMapPatch` and `GetPlayerMapPin` were checked only by absence from the registration table.
