# Script commands, group "Arrays" (the world map, and GetItemValue): the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py` and capstone, the game's tables through ghilana, `script.ldf` and the toolset's scripts, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not. `script.ldf` files these under its "Arrays" group, but they are the world map's commands plus GetItemValue.

Shared facts, all checked unless marked:
- Every handler is `__thiscall (commandId, paramCount)`, `ret 8`. It returns 0, or -639 (0xFFFFFD81) when a pop fails (the script fails), or -638 when a push fails. No handler here fails because an object is wrong or missing: it does nothing instead.
- **The world map object** is `CClientExoApp` 0x005D57E0: `GetModule()` (0x005D4FC0) +0x48C, or null with no module. It is the class `WorldMap` (its message handlers 0x00700610, 0x00700B90 and 0x00701240 use the same fields as the `ScriptBackend::ForScript_*` world-map functions). Every handler that needs it does nothing when it is null. Fields: +0x68 the primary map, +0x78 the secondary map (attributes holding a map pointer); +0x98 the GUI status (int attribute); +0xA8 the travel state (0 none, 1 travelling to a random encounter, 2 travelling to the destination); +0xB8 the destination pin; +0xC8 the encounter area tag and +0xE4 the encounter waypoint tag (strings); +0x100 a byte set when a transition starts.
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
- **Edge cases:** **there is no `EVENT_TYPE_FINISHED_TRAVEL`**: `script.ldf` names it, but defines no such constant, and the engine sends 92. `sp_module_travel` then reads its own stored area and waypoint and transitions. The destination is whatever pin the GUI set at +0xB8 (the clicked pin; inferred). With no primary map set, 0x007C7720 is called on null (checked: no null test at 0x007009F1; a crash inferred). One string empty: no encounter.

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
- The rest of 0x007C7720 (the route along the map's graph and where an encounter is placed on it), the movie's travel animation, and how the clicked pin reaches +0xB8.
- `EVENT_TYPE_BEGIN_TRAVEL` (91), built at 0x007006F0: who calls it. `EVENT_TYPE_WORLDMAP_POSTTRANSITION` (103): where it is raised.
- The world-map messages' senders (0x00776910, 0x00776990, 0x006FFE80), and whether hiding the scene sends `WorldMapClosedMessage`.
- `WorldMapSwapMapsMessage` (0x00701240); what the secondary map is for.
- `GameModeGUI`+0x84 (assumed: the leave-mode flag) and `GUIWorldMap`'s byte +0xBC (assumed: open).
- What the area's +0x180 is (assumed: its tag); whether the explored-map bitmap is saved with the area.
- The item template loader 0x007572D0 in full: which `.uti` fields fill +0x680/+0x69C and +0x4BC.
