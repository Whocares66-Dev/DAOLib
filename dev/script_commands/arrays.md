# Script commands, group "Arrays" (the world map, and GetItemValue): the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py` and capstone, the game's tables through ghilana, `script.ldf` and the toolset's scripts, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not. `script.ldf` files these under its "Arrays" group, but they are the world map's commands plus GetItemValue.

Shared facts, all checked unless marked:
- Every handler is `__thiscall (commandId, paramCount)`, `ret 8`. It returns 0, or -639 (0xFFFFFD81) when a pop fails (the script fails), or -638 when a push fails. No handler here fails because an object is wrong or missing: it does nothing instead.
- **The world map object** is `CClientExoApp` 0x005D57E0: `GetModule()` (0x005D4FC0) +0x48C, or null with no module. It is the class `WorldMap` (its message handlers 0x00700610, 0x00700B90 and 0x00701240 use the same fields as the `ScriptBackend::ForScript_*` world-map functions). Every handler that needs it does nothing when it is null. Fields: +0x68 the primary map, +0x78 the secondary map (attributes holding a map pointer; the swap 0x00700E00, called from the handler 0x00701240, does nothing unless both are set, and then exchanges them through 0x00700D80 and 0x00700DB0 [C, read 2026-10-23 by lib-agent-2]; the movie's toggle button is the secondary map's user and is disabled when `GetValue("WorldMap.SecondaryMap") == 0`, from DAOTurnBased's WorldMap.as decompile, [C] in the movie, not the exe; "null until SetWorldMapSecondary" is [I]); +0x98 the GUI status (int attribute); +0xA8 the travel state (0 none, 1 travelling to a random encounter, 2 travelling to the destination); +0xB8 the destination pin; +0xC8 the encounter area tag and +0xE4 the encounter waypoint tag (strings); +0x100 a byte set when a transition starts.
- **Typed lookups:** `0x005D5A00` a map by id (object type byte 19, `Map`, `OBJECT_TYPE_MAP`), `0x005D5A50` a map pin, which the scripts call a location, by id (type 17, `MapPatch`, `OBJECT_TYPE_MAPPATCH`). The wrong type or an invalid id gives null. The type bytes are at 0x00B59406 (0x11) and 0x00B59408 (0x13); `src/dao/core/ObjectType.h` has the numbering.
- **A map** (type 19): +0xD0 its pins (a vector of pin pointers), +0x16C the pin the player is at, +0x70 the world map number (`WM_*`, sent as `BEGIN_TRAVEL`'s int 2), +0xFC and +0x118 the route being travelled (cleared and rebuilt by 0x007C7720). **A pin** (type 17): +0x88 its status (int attribute, `WM_LOCATION_*`), +0xA0/+0xA4 the previous status, +0xC8 its name (a string-ref record), +0x130 its node on the map's graph, +0x158 the tag of its area, +0x180 its waypoint tag, +0x84 its terrain, +0x1A8 a "flash" attribute, +0x1B8 a byte set the first time it becomes active. The field meanings come from `EVENT_TYPE_BEGIN_TRAVEL`'s arguments (built at 0x007006F0) and `sp_module_travel.nss`'s reading of them (inferred).
- **Full-screen GUI.** `OpenPrimaryWorldMap` and `OpenFadeMap` send `SetFullScreenGUIMessage(name)` (ctor 0x00605DB0) to the game mode controller (`app+0x114`), not to `gui` (`app+0x158`) as the scoreboards do. The controller's `Messages::GameMessage` handler (0x00640080) passes any game message to `gui` first, and to the current mode (+0x74) only if `gui` did not take it. So the result is the same: `GUI::DragonAgeGUISystem::HandleMessage_SetFullScreenGUIMessage` (0x0060B260), which finds the scene by name, then GUI mode 3 (paused), opens the scene and raises `EVENT_TYPE_GUI_OPENED` (84) (master's `dev/script_commands/inventory.md`, shared facts). The scenes: `WorldMap` is scene 0x10, a `GUI::GUIWorldMap` (ctor 0x0072ECE0, registered at 0x006151E1); `FadeMap` is scene 0x1F, a plain `GUIScene` on the movie `FadeMap` (registered at 0x00615589).

### SetWorldMapGuiStatus (752)
- **Handler:** 0x00664EB0 (own) → `ScriptBackend::ForScript_SetWorldMapGuiStatus` (0x006FFB00) on the world map.
- **Does:** pops nStatus and sets `WorldMap`+0x98 through the attribute's Set (vtable +0x54), which notifies. (checked)
- **State:** `WorldMap`+0x98. That the area map's world-map button reads it (0 hidden, 1 shown with no travel, 2 travel by clicking a pin) is `script.ldf`'s and inferred; the reader was not traced.
- **Edge cases:** no range check: any int is stored. No module, so no world map: nothing. (checked)

### SetWorldMapLocationStatus (753)
- **Handler:** 0x0066DC90 (own) → the pin's set-status 0x0066DA00 (status, flash-suppressed).
- **Does:** pops the pin, nStatusId, and bSuppressActiveFlash only when there are more than 2 arguments (default 0, made bool). (checked) In 0x0066DA00 (checked):
  - When the pin's byte +0x1B8 is 0, its current status is not 2 and the new status is 2 (`WM_LOCATION_ACTIVE`), +0x1B8 is set to 1. Then, unless bSuppressActiveFlash is set, the "flash" attribute +0x1A8 is set to 1.
  - Then, if the new status differs from +0x88, it saves the old status at +0xA4 and +0xA0 and sets +0x88 (Set, notifies).
- **State:** pin +0x88, +0xA0/+0xA4, +0x1A8, +0x1B8. (checked)
- **Edge cases:** **the flash fires only the first time a pin becomes active:** after that +0x1B8 stays set, so re-activating it never flashes again, whatever the flag is (checked). No range check on the status (checked). An object that is not a map pin: nothing (checked).

### SetWorldMapPlayerLocation (754)
- **Handler:** 0x00664EF0 (own) → the map's 0x007C68E0(pin id).
- **Does:** pops oMap, then oLocation. With oMap a map, it looks the pin up by id. Only if the pin is in that map's own pin list (+0xD0, a linear find, 0x008663A0) does it set the map's player pin (+0x16C, Set, notifies). (checked)
- **State:** map +0x16C. (checked)
- **Edge cases:** a pin from another map, an invalid or non-pin object: nothing. oMap not a map: nothing. The result (bool) is ignored. Not the same as the refresh that Primary and Secondary run (below), which picks the player's pin by the current area. (checked)

### SetWorldMapPrimary (755)
- **Handler:** 0x00664F50 (own) → `ScriptBackend::ForScript_SetWorldMapPrimary` (0x00700D80).
- **Does:** sets `WorldMap`+0x68 to the map by id (Set), then runs the refresh 0x00700A90 (checked). The refresh, when the module has a current area (`CCModule::CurrentArea` 0x005FE470), does this for the primary map and then the secondary map, each if set (checked): 0x007C5D60 walks the map's pins and makes the first pin whose area tag (+0x158) equals the current area's +0x180 the map's player pin (+0x16C). The area's +0x180 is inferred to be its tag. Then it notifies the attribute (vtable +0x24).
- **State:** `WorldMap`+0x68; the map's +0x16C. (checked)
- **Edge cases:** an invalid id or a non-map object stores null (the lookup's null is stored unchecked), which clears the primary map. A map with no pin for the current area keeps its old player pin. (checked)

### SetWorldMapSecondary (756)
- **Handler:** 0x00664F90 (own) → `ScriptBackend::ForScript_SetWorldMapSecondary` (0x00700DB0).
- **Does:** as 755 on `WorldMap`+0x78. `OBJECT_INVALID` is tested explicitly and stores null; anything else goes through the map lookup (null when not a map), then the same refresh. (checked)
- **Edge cases:** as 755. What the secondary map is for in the GUI (`WorldMap::HandleMessage_WorldMapSwapMapsMessage`, 0x00701240, by its name a swap of the two) is not read.

### OpenPrimaryWorldMap (757)
- **Handler:** 0x00664FD0 (own) → `ScriptBackend::ForScript_OpenPrimaryWorldMap` (0x006FFF40), which does not use the world map object (`this`).
- **Does:** sends the full-screen GUI message for `WorldMap` (scene 0x10): GUI mode 3, paused, `EVENT_TYPE_GUI_OPENED`. (checked; the GUI part is master's reading)
- **State:** the game mode and the GUI's stack of full-screen scenes. That the movie draws the primary map is inferred.
- **Edge cases:** no module: nothing, since the handler checks the world map first (checked). It does not check whether a primary map is set (checked). When the map closes, `WorldMap::HandleMessage_WorldMapClosedMessage` (0x00700610) resets the travel state (+0xA8) to 0 and raises `EVENT_TYPE_WORLD_MAP_CLOSED` (75) to the module, int 0 being the message's +4 (checked; the movie's sender, 0x00776910, not read).

### ClosePrimaryWorldMap (387)
- **Handler:** 0x00664FF0 (own) → `ScriptBackend::ForScript_ClosePrimaryWorldMap` (0x006FFFF0).
- **Does:** sends `HideGUIScreenMessage("WorldMap")` (ctor 0x006FFB30, vtable 0x00B152E8) the same way. `gui` handles it at 0x0060FAF0 (checked):
  - It finds the scene by name; an unknown name does nothing.
  - It hides the scene (vtable +0x38) and takes it off the stack of full-screen scenes (+0x1E4).
  - If that stack is now empty, it calls 0x00717BA0 on mode 3's object (controller +0x58 → [+0xC]), which sets `GameModeGUI`+0x84 to 1 (inferred: leave GUI mode).
- **Edge cases:** a world map that is not open: the scene is still found and hidden, and the stack removal finds nothing (inferred). No module: nothing (checked). Whether this raises `EVENT_TYPE_WORLD_MAP_CLOSED` depends on the movie sending `WorldMapClosedMessage` when hidden (not read).

### OpenFadeMap (309)
- **Handler:** 0x00665010 (own) → `ScriptBackend::ForScript_OpenFadeMap` (0x007000A0).
- **Does:** the full-screen GUI message for `FadeMap` (scene 0x1F, a plain `GUIScene` on the movie `FadeMap`): GUI mode 3, paused, `EVENT_TYPE_GUI_OPENED`. (checked)
- **Edge cases:** as 757: nothing with no module. No close command exists for it. `WM_LOCATION_COMPLETE` (5) is "for the fade map puzzle" (`script.ldf`), so the Circle tower's Fade pins (`cir_mapgui_h.nss`) are ordinary pins on a map the movie shows (inferred).

### RevealCurrentMap (332)
- **Handler:** 0x00665030 (own). It calls `0x005D6B30`: `GetAreaById` (0x005D5960) of the area id (+0x254) of the creature `[app+0xD0]+0x5C` (`0x005D5CC0`; inferred: the player's creature). Then the area's +0x4F8 → 0x007C9E00(1).
- **Does:** fills the area's explored-map bitmap (+0x10, size +0x14) with 0xFF through memset (the argument 1 gives 0xFF, 0 would give 0). Then 0x007C9D70 refreshes the minimap, but only when this explored map is the one shown (global 0x00C38DEC): it sets the texture and scale and a dirty byte on +0x2C. (checked)
- **State:** the area's explored map (fog of war); kept with the area (inferred: saved with it).
- **Edge cases:** it reveals the area of `[app+0xD0]+0x5C`, not the camera's area: no player creature, or no explored map (+0x4F8 null), means nothing happens (checked). Areas linked by `LinkAreaMiniMaps` (357) share that object (inferred).

### WorldMapStartTravelling (696)
- **Handler:** 0x00665060 (own) → `WorldMap` 0x00700990(area, waypoint, pin).
- **Does:** pops both strings always, and oSourceLocationOverride only with more than 2 arguments (default `OBJECT_INVALID`). It looks the pin up (null if not a pin). Then (checked):
  - It stores the area tag at +0xC8 and the waypoint tag at +0xE4. It is a random encounter only when **both** are non-empty.
  - The primary map's 0x007C7720(destination +0xB8, encounter, override) builds the route: it starts from the override pin, or else from the map's player pin (+0x16C), along the map's graph into +0xFC/+0x118. With no start pin the route is cleared. The rest of the route-building was not read.
  - It sets the travel state +0xA8 to 1 (encounter) or 2 (straight) and notifies.
  - If the `WorldMap` scene's byte +0xBC is 0 (inferred: not open), it opens the map as 757 does.
- **Later:** the movie's `WorldMapPreTransitionMessage` (0x00700B90) sets +0x100. In state 1 it asks the module (0x00600A40) for an area transition to +0xC8/+0xE4. If that fails, it runs 697's continuation. In state 2 it raises `EVENT_TYPE_WORLDMAP_PRETRANSITION` (92) to the module with no arguments (0x00700830). (checked)
- **Edge cases:** **there is no `EVENT_TYPE_FINISHED_TRAVEL`**: `script.ldf` names it, but defines no such constant, and the engine sends 92. `sp_module_travel` then reads its own stored area and waypoint and transitions. The destination is whatever pin the GUI set at +0xB8 (the clicked pin: the movie's attribute `WorldMap.LastClickedPinIndex`, read 2026-10-23; corrected 2026-10-23 by lib-agent-2: it was inferred). With no primary map set, 0x007C7720 is called on null (checked: no null test at 0x007009F1; a crash inferred). One string empty: no encounter.

### WorldMapCompleteRandomEncounter (697)
- **Handler:** 0x006651A0 (own) → `ScriptBackend::ForScript_WorldMapCompleteRandomEncounter` (0x00700A60).
- **Does:** sets the travel state (+0xA8) to 2 and notifies, then opens the world map (0x006FFF40, as 757). The movie then travels on from the encounter point (inferred). At the destination, the pre-transition message raises `EVENT_TYPE_WORLDMAP_PRETRANSITION` (92). (checked)
- **Edge cases:** no check that an encounter was under way: called at any time, it opens the map in state 2. The encounter tags (+0xC8/+0xE4) are not cleared. (checked)

### SetWorldMapLocationLocName (600)
- **Handler:** 0x006651C0 (own) → the pin's 0x00652600.
- **Does:** pops the pin and nStringID. It makes a string-ref record of the id (0x004D1C70) and copies it into the pin's name attribute (+0xC8: a byte, the id, a string and three bytes), then notifies. (checked)
- **Edge cases:** not a pin: nothing (checked). The id is not checked against the talk table: what an unknown id shows is up to the GUI's lookup (inferred).

### GetItemValue (291)
- **Handler:** 0x0066BD00 (own). Item lookup 0x005D5870, then the int attribute at item+0x5A8 (value +0x5B4).
- **Does:** pushes the item's computed value in copper. 0x00755430 computes it (checked):
  - It starts from `Cost`. The item's template load (0x007572D0, at 0x007578E4) reads the field into +0x5B8.
  - That is multiplied by `materialrules` (table 159) `CostMultiplier` (hash 0x74E8EA95), at the row of `materialtypes` (table 89) `Material` (hash 0x56BCE3D9) for the item's MaterialType (+0x4FC), and truncated.
  - It adds `itemprps` (107) `BaseCost` (0xB28DB889) × power for each item property. The property ids are at +0x680 and the powers at +0x69C, and they count only when both lists are the same length.
  - Ammunition (`bitm_base` `Type` 7, read into item+0x6E0; arrows, bolts, ballista bolts) with a value above 0 is multiplied by 0.33 (constant 0x00B1C4D8), but not below 1.
  - It adds the value of every rune in the item (+0x4BC, a list of item ids).
- **State:** recomputed on the template load (0x0075795C), by `SetItemMaterialType` (0x007566DC), and by `CCItem`'s attach-rune and detach-rune handlers (0x00759BF3, 0x00759DCE). (checked)
- **Edge cases:** **not the "base value" `script.ldf` says:** material, properties and runes are in it. **Not multiplied by the stack size** (+0x5F0 is not read). Not an item, or no object: 0 (checked). Whether `AddItemProperty`/`RemoveItemProperty` recompute it was not checked here.

## Not read
- Who reads `WorldMap`+0x98 (the GUI status) and how the area map's button uses it.
- The rest of 0x007C7720 (the route along the map's graph and where an encounter is placed on it), the movie's travel animation, and how the clicked pin reaches +0xB8 (read 2026-10-23: the movie's `MapPinIcon.PinClicked` sets it, corrected 2026-10-23 by lib-agent-2).
- `EVENT_TYPE_BEGIN_TRAVEL` (91), built at 0x007006F0: 0x00700CA0 calls it, from the AreaTransition handler (read 2026-10-23, corrected 2026-10-23 by lib-agent-2). `EVENT_TYPE_WORLDMAP_POSTTRANSITION` (103): where it is raised.
- The world-map messages' senders (0x00776910, 0x00776990, 0x006FFE80), and whether hiding the scene sends `WorldMapClosedMessage`.
- `WorldMapSwapMapsMessage` (0x00701240); what the secondary map is for.
- `GameModeGUI`+0x84 (assumed: the leave-mode flag) and `GUIWorldMap`'s byte +0xBC (assumed: open).
- What the area's +0x180 is (assumed: its tag); whether the explored-map bitmap is saved with the area.
- The item template loader 0x007572D0 in full: which `.uti` fields fill +0x680/+0x69C and +0x4BC.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe. Not seen in play. Arrays on the VM stack: `dev/NCS.md` ("A script array read or written past its end", the type tags 0x40 to 0x44 and 0x50 + n) and `src/dao/core/ScriptArray.h`.

### GetArraySize (708)
- **Handler:** 0x006608F0 (not shared). The untyped pop 0x00A2F720 (an `any`, as {type tag, value}), the array's size method 0x00A2DB80 (name inferred from its use), `StackPushInteger`, the free 0x00A2F7C0.
- **Described in:** `dev/NCS.md` (the array tags and their cells); not as a command.
- **Does:** pops the argument as an `any`. If the tag is one of the array tags, 0x40 to 0x44 (int, float, string, object, resource arrays) or 0x50 to 0x59 (engine-structure arrays), and the array pointer is non-null, it pushes the array's size (0x006609AF to 0x006609BC); anything else pushes 0. (checked)
- **State:** none; the popped cell is released.
- **Edge cases:** a non-array value (an int, a string, an object) pushes **0** and does not fail the script (checked). A null array pointer also pushes 0. A tag in 0x45 to 0x4F is not in the list, so it gives 0 as well (checked: the compare chain skips them). The count is the array's element count, so an array that was read past its end by index and not grown still reports its real size (`NCS.md`'s note on reads past the end). A failed pop is -639, a failed push -638.

## Not read
- 0x00A2DB80 inside (taken as the size from its use here and the command's purpose).

## A journey as the engine runs it (read 2026-10-22 by lib-agent-2 with a subagent, for DAOParagonEngine #112)

- **`EVENT_TYPE_WORLDMAP_POSTTRANSITION` (103)** [C]:
  - **Where it is raised:** its only raiser is 0x007008D0, with one caller (0x006C8FE1), case 2 of the area-load machine 0x006C8EF0 (`LOAD_EVENTS.md`, step 2). That is early in the arrived area's load, before `AREALOAD_SPECIAL`, so despite its name it comes before the area's own load events.
  - **When:** only when the loading screen's id (+0x14C) is 0x10, the world map, and the world map's +0x100 is set. The PreTransition handler 0x00700B90 sets +0x100 at its start, and only `WorldMapClosed` (0x00700610) clears it.
  - **So** it is raised on every area load made while travelling, not only at the destination: on the encounter area's too [I], then at the destination.
  - **To whom:** the module only, with no arguments, delivered at once (0x005FF850 → 0x005FEB00). `EVENT_TYPE_WORLDMAP_PRETRANSITION` (92) is also delivered at once to the module (`AIMaster::DeliverNow`, 0x0064AD80).
  - **The scripts:** vanilla's handler is `WM_HandleCutscenesWOW`, which plays plot cutscenes (Redcliffe, Loghain's).
- **The GUI status (+0x98)** [C]:
  - **The engine** reads it in one place, `SetFullScreenGUIMessage` (0x0060B260, at 0x0060B292): for scene 0x10 a status of 0 refuses to open the map.
  - **The movie** reads `WorldMap.GUIStatus` once, at the scene's start. 2 allows travel; 0 or 1 makes the map read-only. It also turns read-only once the travel status is non-zero.
  - **The click gate is the movie's:** `MapPinIcon.PinClicked` sends `GameModeController.AreaTransition` only for an ACTIVE pin on a map that is not read-only. The engine's handler (0x007180E0 → 0x00700CA0 → `BEGIN_TRAVEL`) does not read +0x98.
- **The route's animation and the wait** [C]:
  - **No wait before the pre-transition:** when the travel status changes, the movie's `AnimatePathStart` sends `WorldMapPreTransition` at once, then animates. So the area's load runs while the route is drawn.
  - **The animation's length:** the movie runs at 30 fps. Each trail point takes frames 1–16 (0.5 s), one point per path node; an encounter's point takes frames 1–150 (about 5 s), on the encounter's leg only.
  - **The engine's wait** is load step 7 (0x006C91A0..0x006C9238), before `POSTLOADEXIT`. It polls `GUI::GUIWorldMap::PollTravelDone` 0x0072EDC0, which asks the movie's `GetTravelling` and `GetAnimating`, then invokes `AreaLoadedNotify`. The game is paused (`SetPaused(1, 0)`) through it, so no game time passes.
  - **So** the journey's time on screen depends on the route's length, about 0.5 s per path node plus about 5 s for an encounter, or the load's time if that is longer [I for the "longer"].
- **Swapping the maps** [C]: `WorldMapSwapMapsMessage` (0x00701240) calls 0x00700E00. When both +0x68 and +0x78 are set, it calls `SetPrimary`(the secondary's id) (0x00700D80), then `SetSecondary`(the old primary's id) (0x00700DB0), the ordinary setters with their refresh. With either map null it does nothing.
- **A pin's "activated once" flag (+0x1B8)** [C]: the save's field 16792 (`SavegameWorldmapMapPinActivatedPreviously`, 0x4198) sets it at load (the pin's loader 0x007F7090, at 0x007F7200..0x007F7216: `pin+0x1B8 = value != 0`), and the saver 0x007F6B60 writes it back. A plain byte store, so a loaded pin does not flash.
- **Leaving a random encounter** (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2):
  - **The exit placeable** [C, scripts and template]:
    - `wmt_rand_encounter_exit` has `PLC_AT_DEST_TAG` "rand", `PLC_AT_DEST_AREA_TAG` "world_map", and all five `PLC_AT_WORLD_MAP_ACTIVE_n` empty. Each empty pin lookup logs a CRITICAL trace.
    - Its collision (`placeable_core.nss:266`) goes to `Placeable_PromptAreaTransition` (`placeable_h.nss:44`). That shows popup 321167 only when the placeable is interactive and the game mode is neither GM_COMBAT nor GM_GUI.
    - The popup's result goes to `Placeable_DoAreaTransition`, then `UT_PCJumpOrAreaTransition`, which refuses combat again with a message. That calls `UT_DoAreaTransition` with "world_map", which calls `SendEventTransitionToWorldMap`.
  - **`EVENT_TYPE_TRANSITION_TO_WORLD_MAP` (1005)** is a script event:
    - It is built by `events_h.nss` with strings 0..6: the map, the transition type and five pins. It is signalled to the module, and `sp_module` hands it to `sp_module_travel`.
    - "rand" calls `WorldMapCompleteRandomEncounter`; "camp" opens the party picker; anything else does `SetWorldMapGuiStatus(USE)` and `OpenPrimaryWorldMap`. The five pins are made active first [C].
    - No engine code compares an event type with 1005 [I]: a scan of .text for 0x3ED immediates found five, all in library code at 0x88xxxx–0x91xxxx. Jump tables were not scanned.
  - **697 (0x00700A60) keeps the route** [C]: it does not call 0x007C7720, so the route (+0xFC/+0x118) and the destination (+0xB8) are those from before the encounter.
  - **The map opens read-only** [C, scripts]: the encounter area's enter runs `WM_SetWorldMapGuiStatus` (`area_core.nss:68`, and again on WORLD_MAP_CLOSED at `module_core.nss:528`). It sets USE, READ_ONLY or NO_USE from the area's and module's flags.
  - **The movie with status 2** [C, AS]: `AnimatePathStart` draws the leg before the encounter as already travelled, sends `WorldMapPreTransition` at once, and animates the rest.
- **+0xB8 is set by the movie** [C]. This answers "The destination is whatever pin the GUI set at +0xB8 (the clicked pin; inferred)" in WorldMapStartTravelling's edge cases, and "how the clicked pin reaches +0xB8" in its Not read.
  - +0xB8 is the attribute `WorldMap.LastClickedPinIndex`: the wide string is at 0x00B155C4, registered at 0x007022F3, with the field at 0x00702326.
  - `MapPinIcon.PinClicked` sets it with SetValue and then sends `GameModeController.AreaTransition`.
  - 0x007006F0 only builds `BEGIN_TRAVEL` (91, at 0x00700763) from the two pins and delivers it through 0x0064AD80; it writes nothing into the WorldMap.
  - This also answers "`EVENT_TYPE_BEGIN_TRAVEL` (91), built at 0x007006F0: who calls it": 0x00700CA0 calls it, from the AreaTransition handler below.
- **AreaTransitionMessage is handled only in the GUI mode** [C]:
  - `GameModeController::HandleMessage_GameMessage` 0x00640080 offers a game message to the GUI system (app+0x158) first, and to the current mode (+0x74) only if the GUI system did not take it.
  - The type cell 0x00C0859C has three references in the whole image: its getter 0x00717DF6, its initialisation check 0x00717E37, and the registration at 0x00718ADA/0x00718AE5. That registration is in GameModeGUI's registry 0x00C085C8, with the handler 0x007180E0.
  - So outside mode 3 the click reaches no handler, and no `BEGIN_TRAVEL` follows.
  - In mode 3 the handler needs only the module's WorldMap (0x005D57E0) and always returns true. 0x00700CA0 does nothing without a primary map, and sends nothing without a destination pin. It does not test that scene 0x10 is up.
- **The map's close** [C]:
  - **The scene:** `GUIWorldMap` (constructor 0x0072ECE0) is `GUIScene("WorldMap", "WorldMap", 3, 8, 0x10)`. Its hide (vtable +0x18 = `GUIWorldMap::Close` 0x0072EF40; slot 6 is Close, slot 7 Hide) calls `GUIScene::OnHide` 0x00A55910, then 0x00716A00(0) on scene 0x3B (the book background, not read).
  - **OnHide** tests the mask at +0xA0:
    - it returns at once when the name (+0x58) is empty;
    - bit 1 calls `[0xC31650]` slot 0x98(1);
    - bit 4 plays the outro (GUI slot 0x144); otherwise it calls slot 0x48, CloseScreen, on the name;
    - it then clears +0xBC and +0xBD;
    - bit 8 calls GUI slot 0x64(0).
    - The map's mask is 8, so a hide closes it, which finishes the movie (GUI.md, FinishSceneFromGame).
  - **The movie's close:** `WorldMap.FinishScene` (WorldMap.as:141–146) sends `WorldMap.WorldMapClosed` with `m_bClosedByTravel ? 1 : 0`.
    - `m_bClosedByTravel` is a static (WorldMap.as:36). It is set true by a pin's click, but only for an ACTIVE pin on a map that is not read-only (MapPinIcon.as:134). It is set false by the close button (WorldMap.as:69).
    - Nothing resets it at the scene's start, so it stays true until the movie is unloaded [I: that dropping the record unloads it].
  - **The engine's handler:** `WorldMap::HandleMessage_WorldMapClosedMessage` 0x00700610 clears +0x100, sets the state +0xA8 to 0 and notifies, then raises `EVENT_TYPE_WORLD_MAP_CLOSED` (75) with the message's int, delivered at once (0x0064AD80).
  - **Who hides the scene:** `DragonAgeGUISystem::CloseScene` (GUI slot 0x38, 0x00606650) calls the hide when the scene's +0xBC or +0xBD is set. Its callers here are:
    - `HideGUIScreenMessage` 0x0060FAF0 → 0x0060FAC0, which also erases the id from the full-screen stack and, when that empties, calls 0x00717BA0 (not read);
    - the loading mode's leave `GameModeLoading::Leave` 0x006C5A00;
    - `GameModeGUI::Leave` 0x007188C0: with +0x95 set, slot 0x40 (0x0060F240) pops the stack +0x1E4 one id at a time through 0x0060F1E0.
  - **So** the travel state and +0x100 do not outlive the scene through any of these hides [C]. Toggling the screen's visibility (HideScreen) is not a hide [I], and whether the shutdown unload (0x005151F0) sends WorldMapClosed was not read.
- **WorldMapStartTravelling opens the map** through scene 0x10, fetched by GUI slot 0x60 (0x00700A37), with no null test on the scene (0x00700A3E reads its +0xBC directly) [C].
- **A save's world map** (read 2026-10-24 by paragon-agent-2's helper, checked by lib-agent-2) [C]:
  - **The saver:** MapObject's 0x007C61B0 (vtable 0x00B22E04 index 21) writes field 16785 (`0x4191`, 0x007C638B–0x007C63AF) from the player pin's area tag (+0x158).
  - **The loader 0x007C7DD0, in order:** the pins (16786 and 16787, through 0x007C6110 and 0x007F7090); then 16796 replacing +0xFC and 16797 replacing +0x118, the route being travelled (each point through 0x007C66E0, constructed by 0x007C5A90; the list notified through its vtable +0x24); then 16785 through 0x007C5D60.
  - **0x007C5D60** walks the pins (+0xD0) and makes the first whose area tag (+0x158) equals the saved one the player pin (+0x16C, through slot +0x54). The compare is 0x004C0DE0, exact and case-sensitive. With no match it changes nothing.
  - **So** a save keeps the route and the player pin by its area tag.
- **Not read** (more):
  - 0x00716A00 (scene 0x3B on the map's show and hide);
  - 0x00717BA0;
  - what a GUIScene's +0x54 is;
  - whether a save keeps +0xA8 or +0x100 [I: they are not among the offsets the loader writes; the loader past the player pin, and the template 0x007C7CB0, were not read]. The route is answered (read 2026-10-24 by lib-agent-2): a save keeps it, with the player pin (below).
- **Not read:** whether the movie's message reaches the engine in the same frame; field 16793.

### The click, and where the maps come from (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169)

GOG addresses.

- **The click's two arguments** [C]: `GameModeGUI::HandleMessage_AreaTransitionMessage` (`0x007180E0`) passes the message's wide string at +0x04 and its int at +0x20 to `0x00700CA0` (unnamed, `this` = the `WorldMap`) after checking that the module's world map exists. `0x00700CA0` does nothing when the primary map (`WorldMap`+0x68) is null. Otherwise the SOURCE is `0x007C5E30(string)` on the primary map: the first pin in its pin vector (+0xD0) whose wide-string attribute at pin +0x158 (this file calls it the pin's area tag) equals the string (`0x004C0DE0`, an equality test); with none found the source is an empty `MapLocation` built with id -1 (`MapLocation::MapLocation(-1)`), which is sent all the same. The DESTINATION is `0x007C5EE0(int)` on the primary map: the pin at that INDEX of the pin vector (+0xD0), not an object id; an index past the end gives null. Only when the destination is non-null is `0x007006F0(source, destination)` called, which builds `EVENT_TYPE_BEGIN_TRAVEL` (91). So the engine does not take the source from the map's player pin (+0x16C): the movie's message names it by the area-tag string. Nothing in it reads a pin's status; the active-pin gate is the movie's ("The click gate is the movie's" above).
- **The refresh's area comparison** [C], completing `SetWorldMapPrimary`'s "The area's +0x180 is inferred to be its tag": `GetTag` reads an area's tag from area +0x180 (`script_commands/objects.md`, "an area (type 4) from area +0x180", and the area adding its own tag at `0x0068B3D4`), so the comparison in `0x007C5D60` of the pin's +0x158 with the current area's +0x180 is a tag comparison. The compare is case-sensitive, a plain word compare (0x00414170, 0x00413800) (answered read 2026-10-23 by lib-agent-2; it was not read).
- **Where the maps come from** [C for the chain, I for the filter]: `0x00701990` (unnamed; one caller, `0x00605430`) lists the resource manager's names with `ECResMan::ResMan::ListNames`, given the wide strings `L"MAP"` (`0x00B155BC`) and an empty one (`0x00ADEF14`) as the type and the prefix; for each name it does `Resources::Find`, and for each found builds a `MapObject` from the name (`MapObject::MapObject`, vtable `0x00B22E04`) into a vector at `WorldMap`+0x5C. So every resource of type MAP the manager lists becomes a map object; which sources `ListNames` walks and whether a name in several sources is listed once was not read [I]. It then runs `0x00701550`: for each loaded map object it looks the live map up with `0x00700F50` by the name at +0x134; when one is found and that map's +0x148 is 0, `0x007C9440` copies the object's pins and connections into it (setting each pin's +0x70, the world map number, from the live map), and the loaded object is freed; then `0x00700E60`. The GFF layout of a `.map` is `MapObject::MapObject`'s and was not read.
- **How the maps load and merge** (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2) [C]:
  - **Loaded once** [C]: `WorldMap::LoadMapResources` 0x00701990 lists the MAP names only while the map vector (+0x5C) is empty (0x0086BEE0); otherwise it goes straight to the merge.
    - **The load:** each name becomes a MapObject (constructor 0x007C89F0, the name at +0x150) loaded by 0x007C8E40.
    - **Fade_map:** a map named exactly `fade_map.map` (case-sensitive) is also kept in the WorldMap's +0x88 (not followed).
  - **0x007C8E40, the fields** [C]:
    - MapTag (24000) → +0xA8;
    - **MapMapParentResRef (24012) → +0x134, with ".map" appended when not empty**;
    - MapType (24001) → +0x70;
    - the pins (24002) → +0xD0;
    - the trails (24014) through 0x007C87E0.
  - **`WorldMap::MergeLoadedMaps` 0x00701550** [C]:
    - **The lookup:** for each loaded map in order, 0x00700F50 finds the first map whose name (+0x150) equals its parent name. The compare is ASCII case-insensitive (0x004C0A50 → 0x004C0350).
    - **Merged:** when the parent found has no parent of its own (+0x148 is 0), `MapObject::CopyPinsFrom` 0x007C9440 appends the child's pins to the parent's +0xD0, setting each pin's map number from the parent. It also adds the child's trails (+0xF0) to the parent through 0x007C87E0.
    - **Removed, merged or not:** either way the child is erased and deleted. So a child whose parent has a parent of its own loses its pins.
    - **No parent found:** the map stays a map of its own.
    - It makes one pass.
  - **Case rules** [C], which settle the earlier "case was not read" (answered, read 2026-10-23 by lib-agent-2):
    - the parent lookup ignores case;
    - the click's area-tag compare 0x004C0DE0 is case-sensitive, a plain word compare (0x00414170, 0x00413800).
  - **So** an add-in's map with a parent is no map of its own: its pins and trails join the parent's, as for Warden's Keep's and Honnleath's onto `wide_open_world`. That the add-ins' `.map` files name those parents is from paragon-agent-2's reading of the files and was not checked here [I].
