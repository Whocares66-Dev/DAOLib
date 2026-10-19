# The events of a module's start and an area's load

Which script events the engine raises as a module starts and an area loads, in what order, and to whose script. Read 2026-10-17 in the GOG exe by lib-agent-2, for DAOParagonEngine's script engine (its S2 and S3), and checked against the probe's event watch (`dev/PROBE.md`). The event numbers are `events_h.nss`'s `EVENT_TYPE_*`.

## The module

The module's load (`0x00605430`):

1. `EVENT_TYPE_MODULE_LOAD` (20) to the module.
2. `EVENT_TYPE_MODULE_START` (19) to the module, only for a game not loaded from a save (`0x005ff8b0`).
3. `EVENT_TYPE_MODULE_GETCHARSTAGE` (63), run at once rather than queued.

`EVENT_TYPE_ENTER` (8) goes to the module (`0x0069da70`) and to the area as the party enters.

## An area

The area's load is a machine of steps (`0x006c8ef0`, steps 1 to 8):

- Step 4 queues `EVENT_TYPE_AREALOAD_PRELOADEXIT` (38; 42 for an area loaded from a save) to the area, and runs `AIMaster::Update` at once, so the event is handled within the step.
- Step 6 waits for the area's rooms.
- Step 8 queues `EVENT_TYPE_AREALOAD_POSTLOADEXIT` (39; 43 from a save) to the area and runs the queue again.

Before them, at step 3, `EVENT_TYPE_AREALOAD_SPECIAL` (40, or 41 from a save) is run on the area's script at once, not queued (below). A saved game's load is told by the loading state's byte +0x68 (`0x006c90b8`), set only by load requests of kind 2 and 8. 37 and 80 are never raised.

## An object placed

`EVENT_TYPE_SPAWN` (3) goes to each creature, placeable and trigger once, the first time it is placed in an area, and only if event 3 is not in its disabled set (+0x28C, which only `EnablevEvent` changes and no shipped script calls; corrected 2026-10-18 by lib-agent-2, "The event queue's gate and delivery" below): a creature from its add-to-area (vtable +0xF8, index 62; `0x006b6bc0`, its flag at +0xC54), a placeable (`0x00750b10`, flag +0x6D0) and a trigger (`0x0079b6b0`, flag +0x500) the same way. An object restored from a save has the flag set, so a save's load sends none (corrected 2026-10-18 by lib-agent-2; below).

## AREALOAD_SPECIAL and the other load events

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe, after `dev/LOAD_EVENTS.md` had 40, 41, 37 and 80 as not found. "Read" is seen in the code; "inferred" is marked.

- `EVENT_TYPE_AREALOAD_SPECIAL` (40) and `EVENT_TYPE_AREALOADSAVE_SPECIAL` (41) are raised by `0x0067F1C0`, at step 3 of the client's area-load machine (`0x006C8EF0`, the call at `0x006C9030`): one step before `AREALOAD_PRELOADEXIT`/`AREALOADSAVE_PRELOADEXIT` (38/42, step 4) and so also before `POSTLOADEXIT` (39/43, step 8). Read.
- The type is computed, which is why a scan for immediates missed it: `setne cl; add ecx, 0x28; mov [esi+0x20], ecx` at `0x0067F24A`, so 40, or 41 when the load's byte +0x68 is set (the call pushes `movzx edx, byte [ebp+0x68]`). The byte is the loading state's +0x68 (`0x006c90b8`), set only by load requests of kind 2 and 8, a saved game's, and the same byte picks 42/43 over 38/39 (corrected the same day: not +0x44).
- Condition (read, `0x006C8FF4`–`0x006C9026`): the object from `0x00402CA0` has +0x158 equal to 0 (not read what it is; the same test gates step 5), the client's +0xD0 object exists, its player creature `0x0069D2E0` exists, and that creature has an area (`0x00695180`). Then `0x0067F1C0` is called `__thiscall` on that area with (the +0xD0 object, the flag).
- The event (read): type 40 or 41; creator (+0x24) the player creature, when there is one; no ints, strings or objects set. It is not queued: `CCArea::EventHandler` (`0x0067F180`, `kCCArea_EventHandler`) is called directly with (10, the +0xD0 object's +0x60, the event, the world time), so the area's script runs it at once, inside step 3.
- That direct call is why no probe log ever showed 40: the probe's event watch detours `AIMaster::ProcessEvent`, the queue's delivery, which this path never touches (read: no `AddEventDeltaTime` or `DeliverNow` in `0x0067F1C0`).
- So on every area load (not only the first visit; no first-time test is made): step 3 runs SPECIAL (40, or 41 from a save) on the area's script at once, step 4 queues and runs PRELOADEXIT (38/42), step 6 waits for the rooms, step 8 queues and runs POSTLOADEXIT (39/43). Area scripts that want an opening cutscene only once guard it themselves (the Circle's scripts check plot flags).
- The scripts never send 40 themselves: no `SignalEvent` or `Event(EVENT_TYPE_AREALOAD_SPECIAL)` in the shipped scripts (read).
- `EVENT_TYPE_GAMEOBJECTSLOADED` (37) and `EVENT_TYPE_AREALIST_POSTLOAD` (80): not found. No construction of a `CScriptEvent` (all 121 call sites of its constructor `0x0069BAC0`) stores 37 or 80 as an immediate or computes them the way 40 is computed, and no script sends them (`events_h` only declares 37). Inferred: unused by the engine in 1.05.

## Area lists: what is kept and how it comes back

Read 2026-10-18 in the GOG exe by lib-agent-2 (a subagent), for DAOParagonEngine's mage origin (#22). Builds on `dao/Addresses.h` ("The loading mode's states", "The module's area list") and `perf/fastload/AreaLists.h`, which read the swap's steps. "Read" means seen in the code; "inferred" is marked.

### Leaving a list: the areas are written into the save kept in memory

- The game keeps a save game in memory at the client application's +0x14C (`g_clientExoApp`, `0x00C327EC`): a GFF4 tree of the save's own shape (`dev/SAVES.md`). Its `SavegameAreaList` (16001, `0x3E81`) is a list of structs, one per area, found by the area's tag (`0x006439A0`, `__thiscall (out, tag, bCreate)`: walks 16001 comparing each struct's string field with the tag; with bCreate it appends a new one through `0x006432A0(1, tag)`). Read. That the tree is there in a new game as well as after a load is inferred (every reader checks +0x14C for null first).
- An area list swap (the loading mode's state 4, `dao/Addresses.h`) saves the list it leaves before deleting it. Step 3 (`CSAreaList` `0x006FB3A0`, on the list): for each of the list's areas, the struct of that tag is removed from 16001 and a fresh one appended, so the save holds exactly one, empty, per area. Step 4 (`0x006FA5C0`, one area a frame from the list's cursor +0xC8): `CCArea` `0x00683C80` fills its area's struct. Read.
- What an area's struct holds (`0x00683C80`, read): its object id (field `0x17`); the objects in the area's list at +0x694, each written by `0x006F1340` into the list of its kind: `SavegameAreaPlaceables` (16010), `SavegameAreaCreatures` (16011), `SavegameAreaTriggers` (16012), `SavegameAreaAOEs` (16013), `SavegameAreaWaypoints` (16015), `SavegameAreaStores` (16017), `SavegameAreaSounds` (16019); `SavegameAreaMinCreatureImportance` (16020) from the area's +0x23C; `SavegameAreaMap` (16016, `0x007C9680`); `SavegameAreaRoomsViewed` (16018) from the area's +0x464. A creature for which `0x005D57C0`'s check (decompiled as `ForScript_GetCanDiePermanently`) answers true is not written: inferred, the party's members, who travel with the party. The area's own variables come in `0x00705A70` (inferred from its place before the lists; not followed).
- Then step 5 deletes the list and its areas (`CCModule::DropAreaList`; fast load keeps the list in memory instead, `perf/fastload/AreaLists.h`). Nothing is written to disk: the state lives in the save tree at +0x14C until the player saves, when that tree is what is written (`dev/SAVES.md`'s `SavegameAreaList`, inferred).

### Returning: the area is built from its saved struct

- Step 9 builds the new list's areas, one a call (`CSAreaList::LoadAreas`, `0x006FABB0`). For each, if +0x14C has a struct of the area's tag (`0x006439A0`, bCreate 0) its object id (field `0x17`, `0x0047C8E0`) is the area's id, so an area comes back as the same object it was. Read.
- The area's load (`CCArea` `0x0068B1B0`): if the save tree has the area's struct with an object id, that struct is passed to the area's object load (`0x006882C0`) in place of the template's, and the area's +0x234 is set (read); so its creatures, placeables, triggers and the rest come back as they were left (inferred from the field names; the object load not followed). The rooms viewed (16018) are put back (`0x0067BAD0`, read), and the map's state from 16016 (`0x007C9640`, read). +0x234 is cleared for every area by the client load's state 2/8 step 0x16 (`0x006C76F0`), read.
- So the party finds an area as it left it, without a disk save between: in memory, in the save tree. A list never visited has no struct and is built from its templates. A list fast load kept is handed back whole and is not rebuilt (`perf/fastload/AreaLists.h`).

### `AREALOADSAVE_*` (41, 42, 43) against `AREALOAD_*` (38, 39, 40)

- The area load's steps (`0x006C8EF0`) send 38 or 42 at step 4 and 39 or 43 at step 8 as the loading state's byte +0x68 is 0 or 1 (`cmp byte ptr [ebp+0x68], 1` at `0x006C90B8`, `ebp` the loading mode's +0x130). Read.
- That byte is copied from the loading request's byte +8 only when the loading mode is entered with a request of kind 2 or 8 (`GameModeLoading` slot 6, `0x006C6DB0`; kinds 2 and 8 run the state machine `0x006C76F0`, a game or a save being loaded), and set to 0 by the state's resets (`0x006C50E9`, `0x006C54F6`). A kind-7 request (`DoAreaTransition`, `0x00600A40`; also character creation's end, `0x007FCB1D`) and kind 5 (`0x007A4078`) do not touch it. Read. So 42 and 43 are a save's load only; walking back into a kept area, or into a list rebuilt from the save tree, is 38 and 39. Seen in play: after a save's load 42 and 43, then 38 and 39 at the next transition (`dev/LOAD_EVENTS.md`'s probe logs).
- 40 and 41 (`AREALOAD_SPECIAL`, `AREALOADSAVE_SPECIAL`) are not sent by these steps (`dev/LOAD_EVENTS.md`); a sibling reading covers where they come from.

### `EVENT_TYPE_AREALIST_POSTLOAD` (80)

- Not raised: no code stores 0x50 as an event's type (`mov dword [reg+0x20], 0x50`, none), and no `push 0x50` reaches the event gate or the plot event builder. No shipped script handles it either (`toolset\extract\scripts`, none). Read. It looks like a number reserved and never used.

## EVENT_TYPE_SPAWN (3): once per object, for creatures, placeables and triggers

**Where it is sent.** Three places build it, each the class's add-to-area (vtable +0xF8, index 62; `dev/LOAD_EVENTS.md`'s "slot 63" is Ghidra's 1-based count):

| Class | Add-to-area | Flag |
|---|---|---|
| `CCCreature` | 0x006B6BC0 | +0xC54 |
| `CCPlaceable` | 0x00750B10 | +0x6D0 (checked) |
| `CCTrigger` | 0x0079B6B0 | +0x500 |

Items, waypoints, stores and sounds get none.

**How it is sent.**
- It is sent only if the flag is 0 and event 3 is not in the object's disabled set, +0x28C (0x00698490(3); only `EnablevEvent` changes the set and no shipped script calls it, so in practice it passes; corrected 2026-10-18 by lib-agent-2, below). The flag is then set to 1 either way.
- A creature also skips it when added to the area it is already in.
- It is **queued** (`AIMaster::AddEventDeltaTime(0, 0, …, 10, ev)`: no delay; the 10 is the handler id for a script event, not a delay; corrected 2026-10-18 by lib-agent-2), creator and target the object itself, with no ints.

**`CreateObject`** (0x00669550 → 0x00687830 → 0x006861F0), in order:
1. The object is constructed, with the flag 0.
2. The template is loaded. For a creature, its `Equip_ItemList` is made and equipped and its `ItemList` filled (0x006BCC30), **before** SPAWN.
3. The object is added to the area, which queues SPAWN.
4. The orientation is set; the active flag is set from `bSpawnActive`; the override script is set; `bNoPermDeath` is stored.

Two consequences:
- A pooled template (`CreatePool`) reuses a creature by `SetPosition` and sends no SPAWN.
- The event filter is checked against the template's script, before an override script is set (inferred: an override alone doesn't bring SPAWN).

**Placed objects** (a first visit):
- The area's loaders (creatures 0x00683820, placeables 0x00683380) put them on waiting lists. 0x0067F420 adds each one whose resources are ready, placeables before creatures.
- That happens at the loading states' step 13; step 14 places the party (0x006C73C0).
- So their SPAWNs are queued before state 6's AREALOAD_SPECIAL (40) and PRELOADEXIT (38).
- Triggers, waypoints and stores are added as they load.

**Never resent.**
- No SPAWN field is saved, but an object restored from save data (its vtable +0x88) sets its flag to 1. So a save's load, or a return to an area kept in memory, sends no SPAWN.
- The exception is a hero applied from the saved-heroes list (0x006D1850), which gets the flag 0 and SPAWN again.
- `creature_core.nss` also guards with the local `CREATURE_SPAWNED`.

Read 2026-10-18 by lib-agent-2 for DAOParagonEngine, not seen in play.

## The party placed on arrival: the engine's formation

The scripts only call `DoAreaTransition` (`UT_DoAreaTransition`, `utility_h.nss`:1051–1095); no script places followers after a load. The engine does, in 0x006C73C0 (the loading states' step 13/14, and the save load's step 0x15).
- **The point.** It is the waypoint's position and facing (copied in 0x006C7080).
- **The leader** is the controlled creature (`[[app+0xD0]+0x5C]`). It stands on the waypoint, facing the waypoint's way.
- **Each other active party member** (the party's vector at +0x234) gets a slot (0x00631D90: its index in the vector, minus one; followers outside the party come after). Its spot is the leader's position plus the slot's offset, turned to the facing (0x007605B0 with table 0x00C0D2F0, checked):

  | slot | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
  |---|---|---|---|---|---|---|---|---|---|---|---|
  | x (right) | -1 | 1 | 0 | -2 | 2 | -3 | 3 | -1 | 1 | -3 | 3 |
  | y (forward) | -0.5 | -0.5 | -1 | -1 | -1 | -1 | -1 | -2 | -2 | -2 | -2 |

  - The slots are behind the leader, alternating left and right.
  - When the party's byte +0x450 is 0, left and right swap (0↔1, 3↔4, 5↔6, 7↔8, 9↔10).
- **The spot is checked.** A spot not free (`MovementController::IsFree`) moves to the nearest free cell within 2.0 m back toward the leader. With no straight walk to the leader, it takes a safe location around the leader within 5.0 m, starting at the two creatures' personal spaces added.
- **Everyone faces the waypoint's way** (0x006C75C8).
- **Last,** each creature's spot goes through `GetSafeLocation` within 20.0 m (0x00AE72FC), reachable, snapped to the ground; if that fails the spot is kept.
- `GetSafeLocation` (0x0073E370) walks square rings outward by half a grid cell.
- **This snap is the party's arrival only** (corrected 2026-10-18 by lib-agent-2). An area's placed creatures are not snapped at load: `SetPosition` 0x006B5EF0 skips the snap 0x006B1790 in game mode 9, Loading (the gate at 0x006B5F76, checked). `dev/PATHFINDING.md` has it.
- The loading data's byte +0x68 makes everyone keep their own position (inferred: a save's load).

**Jumping to the leader mid-area** uses the same table (0x006AA440) for a party member that isn't the controlled creature:
- `CommandJumpToObject(controlled creature)` keeps its own facing. Any other creature target gives a random angle at the two personal spaces' distance, facing the target.
- `CommandJumpToLocation`, or `SetPosition(…, TRUE)`, to within 0.01 m of the controlled creature.
- So `UT_LocalJump`'s party jump puts the leader on the waypoint and the followers into their slots (inferred).

**Walking follow** uses another table (0x00C0D240: (∓1.5, -1), (0, -2.5), (∓1.5, -4), (∓3, -2), (∓1, -3), (∓3.5, -1)). `GetFollowerWouldBeLocation` answers from it, turned by the leader's movement.

Read 2026-10-18 by lib-agent-2 for DAOParagonEngine, not seen in play.

## The event queue's gate and delivery (read 2026-10-18 by lib-agent-2)

Read in the GOG exe and the scripts by lib-agent-2 (with a subagent) for paragon-agent-2's Circle basement. Not seen in play. Checked in the disassembly where an address is given.

### The gate is a disabled set, not the script

- **0x00698490(type)** looks the type up in the object's set at +0x28C (0x005FE960) and answers true when it is **not** there (0x006984D5–0x006984D9).
  - The set is saved as `EnabledEvents` (0x006980ED is the script override's save).
  - Only `EnablevEvent` (command 123, 0x00658CC0; `void EnablevEvent(object, int bEnable, int nEventType)`) changes it (0x00698C00, a set insert or erase), and no shipped script calls it.
  - So in practice every event passes.
- **Corrected the same day:** the notes that read this gate as "the object's script takes event N" (an object placed and SPAWN above, `dev/ITEMS.md`'s INVENTORY_ADDED and EQUIP, `dev/COMBAT.md`'s perception, `dev/PLACEABLES.md`'s use).

### AddEventDeltaTime's arguments

- `AIMaster::AddEventDeltaTime(days, ms, caller, target, id, event)` (0x0064ACE0): the 10 (0xA) that every script event passes is the **handler id** for a script event (NWN's `EVENT_SIGNAL_EVENT`), not a delay or a priority. The delay is (days, ms), 0 and 0 at add-to-area (0x006B6C77–0x006B6C7F).
- With no delay it goes to AddEventNow (0x0064A630), which inserts before the first node due strictly later, so events due together are delivered first in, first out.
- **Corrected the same day:** SPAWN above and `dev/PLACEABLES.md`'s use, which wrote the 10 as if it were a delay. `dev/COMBAT.md`'s perception has the arguments right.

### Delivery and its script

- The queue is drained only in `AIMaster::UpdateState` (0x0064AE30, from `AIMaster::Update` 0x0064AF00), called by the per-frame AI update (0x0064B047), the loading states and a few others; never by a script command. So nothing queued while a script runs is delivered before the script returns.
- A non-immediate `SignalEvent` (0x00657E40) queues the same way (0x00657EDE); only an immediate one is delivered at once (`DeliverNow`, 0x00657EFC).
- **First, the engine's own events go to their script.** `RunEventScript` looks the event type up in `engineevents.gda` (table 114, `0x006989BD`) and takes its Script column (hash 0xF333FE09, `0x006989D4`), which sends events 33–36 and 65 to `rules_core` whatever script the object has (checked the table and column; the list of events read by lib-agent-2's subagent, 2026-10-18).
- **The script run is the object's at delivery.** Otherwise `RunEventScript` (0x00698960) uses the event's own script (+0x2C) only when the event's +0x40 is set; otherwise it reads the object's +0x26C (0x006989F0), which `SetEventScript` (0x00658AA0) writes. +0x288 marks the script as overridden for the save.

### Events for objects of an area left (read 2026-10-19 by lib-agent-2)

Read in the GOG exe for paragon-agent's saves work (DAOParagonEngine #20). Checked where an address is given, else inferred. Not seen in play.

- **One queue, untouched by a swap.** The queue is one global list, `AIMaster` at the application's +0x1AC.
  - `AIMaster::ClearEvents` (0x0064AC80) has one caller, the module's unload (0x005D6A50, from the load-game and quit paths: 0x006C5B2D, 0x006C78CB, 0x005D722F, 0x0064CB9F). None of these is the area transition.
  - `DeleteEventData` (0x0064A520) is called only by `ProcessEvent` and `ClearEvents`. No function removes an object's events.
  - The creature's destructor (0x006BAFE0) takes the creature out of the AI update lists only (0x0064B240).
  - The area's save (0x00683C80) writes no events.
- **Delivery finds the target by id each time.** `ProcessEvent` (0x0064A8C0) looks the target id up (0x005D5840 at 0x0064A90C). A target that is not found has the event's data freed and nothing else: no log, no re-queue (0x0064A913 to 0x0064A9B5). The creator id is passed along as a number and never looked up, so delivery depends on the target alone.
- **So an event pending when the party leaves:**
  - Events already due when the swap starts are delivered first: state 4's first step runs `AIMaster::Update(0)` (0x006C8B06) before anything is saved. It is budgeted to about a second (`[0x00C01384]`), so a long backlog can remain.
  - Later ones stay queued on the global clock. One that comes due while its target is gone is dropped. One that comes due after the area is back reaches the rebuilt object, which has the same id: the creature loader (0x00683820) builds each creature with its saved `ObjectID` (field 0x17) unless one with that id exists already (0x005D58C0).
  - A backlog is next drained at state 6's steps 4, 5 and 8 (0x006C9122, 0x006C9170, 0x006C9333). Inferred: nothing drains it between, because of the callers of `Update`. 0x007182B8, 0x007193D0 and 0x007FCD26 are not identified.
- **Saved with the game, not the area.** The game save writes the whole queue as `SavegameEventQueue` (16630), one entry per event with creator, target and data. 0x0064A9D0 writes it (called at 0x006C6943) and 0x0064AAD0 reads it (0x006C820E). Which of the two is which is inferred from their shapes and callers. An entry whose data fails to rebuild (0x0079C0C0) is freed.
  - A creature's own pending commands are in its object save (`SavegameCurrentActionQueue`, 16740), apart from the queue. Their restore is not read.
- **Other objects' lists keep the ids.**
  - A perception set (+0xC18) keeps a gone creature's id until the perceiver's next perception update. That update looks every id up (0x006A8D60, `dev/COMBAT.md`) and raises DISAPPEAR to the perceiver.
  - A melee ring's claim is released by its attacker at the command's shared end (0x00746A65 to 0x006A3610 to 0x007CC060). A destroyed attacker's commands are hard-cleared (0x0073C930). That this runs the shared end and so releases the claim is inferred.
- **Not read:** the loading mode's world-update gate (slot 18); the object event handler's use of the creator id; the restore of placeables' and triggers' ids (0x00683380); the unidentified calls in the creature's destructor; what `GetEventCreator` (0x006582A0) does after 0x006582F6.
