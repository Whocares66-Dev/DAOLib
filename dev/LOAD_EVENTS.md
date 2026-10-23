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
- **Step 7, before it** (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2) [C, 0x006C91A0–0x006C9247]:
  - Step 7 first polls a module count (`[module+0x658]`, through 0x005CA1B0/0x005C84D0). Above 0 it pauses (SetPaused(1, 0)) and stays in step 7. What the count is was not read.
  - Otherwise it unpauses (0x006C91E4) and tests the loading screen (+0x14C). When that is not 0x10 it goes to step 8.
  - When it is 0x10, the load holds, paused, while all of these hold: scene 0x10's +0xBC is set, and 0x0072EDC0 answers "not done". Otherwise it unpauses again (0x006C9238) and goes to step 8.
  - 0x0072EDC0 finds the "WorldMap" movie; with none it is done. It invokes `GetTravelling` and `GetAnimating`, and when the map is travelling and not animating it invokes `AreaLoadedNotify`. It is done only when the map is not travelling. Nothing is stored: the movie is asked at every poll.

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
- What an area's struct holds (`0x00683C80`, read): its object id (field `0x17`); the objects in the area's list at +0x694, sorted into their kinds by `0x006F1340` (a vector's push_back) and each written by its own slot 21: `SavegameAreaPlaceables` (16010), `SavegameAreaCreatures` (16011), `SavegameAreaTriggers` (16012), `SavegameAreaAOEs` (16013), `SavegameAreaWaypoints` (16015), `SavegameAreaStores` (16017), `SavegameAreaSounds` (16019); `SavegameAreaMinCreatureImportance` (16020) from the area's +0x23C; `SavegameAreaMap` (16016, `0x007C9680`); `SavegameAreaRoomsViewed` (16018) from the area's +0x464. A creature that `CCParty::IsInPartyPool` (0x006316F0, ID 12318, called at 0x00683F0D) answers true for is not written: the party pool's members, who travel with the party (read 2026-10-21 by lib-agent-2). The area's own variables come in `0x00705A70` (inferred from its place before the lists; not followed).
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
   - **SPAWN at the first entry** (read 2026-10-23 by lib-agent-2 with a subagent) [C]:
     - **The raiser:** SPAWN is raised by the creature's add-to-area (vtable +0xF8, 0x006B6BC0), not by `SetLocation`. It queues SPAWN when `+0xC54 == 0` and `0x00698490(3)` holds, sets `+0xC54 = 1` either way (0x006B6C87), and only then calls `SetLocation` (0x006B6C9C).
     - **The gate:** `0x00698490(type)` is true unless the type is in the object's disabled set `+0x28C` (no shipped script adds 3).
     - **Who writes `+0xC54`:** 0 in the constructor; the save reader (0x006BE980) sets it from two bytes the saved-heroes apply sets (0x006D1850).
     - **An area's placed creatures** keep 0 until their first add, so they get SPAWN [I that their loaders skip the save reader].
     - **Other raisers:** a placeable (0x00750B10) and a trigger (0x0079B6B0) raise their own.
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

## The loading mode's screen (read 2026-10-19 by lib-agent-2)

Read in the GOG exe and the movies (`tools\daoexe.py`). Checked where an address is given, else inferred. The screen ids were never logged in play, so which screen a player sees is from the code.

### A request's kind, and the screen

- **The kinds are the loading mode's states too.** Slot 14 (0x006C63F0, the choice in `dev/GUI.md`) runs with the request's kind on entry (slot 5, 0x006C6D90) and again each time a state hands over to the next.
- **Who asks for which kind** (the constructors' callers, `GameModeLoadingData`, vtable 0x00B1082C):
  - 7: `DoAreaTransition` (0x00600B90 in `CCModule::ForScript_DoAreaTransition`), and character creation's end (`CharGenEndMessage`, 0x007FCB1D).
  - 5: a `CommandJumpToObject` whose target is in another area (0x007A4078).
  - 3: saving (0x005D803E).
  - 2 or 8: the module load 0x005D95C0, `bool ? 2 : 8` (0x005DA1B6–0x005DA1BF); the bool is also the loading state's +0x68. 2 is a saved game (from `SaveLoadProxy::LoadGameMessage` and others). 8 is a new game or a module (the GUI's `LoadModule`, `LoadModuleSkipUnauthorizedMessage`, the startup bypass).
  - 4 and 6: no request is made of either. Slot 5 refuses 6.
- **A door's transition** (kind 7):
  1. The request's own call shows nothing, unless the world map is up (WorldMap 0x10, 0x006C6060).
  2. State 7 (0x006C7380 to 0x006C7080) finds the area and calls slot 14 with **5** if it is in the area list in memory (LightLoading 0x21, `load_light`) or **4** if not (Loading 0x0E, `load_town`), each with the same world-map exception (the pushes at 0x006C72AE, 0x006C72FA, 0x006C7300, 0x006C7318 and 0x006C732C). A failure calls slot 14 with 1 (nothing) or the load-failure routine.
  3. State 6 (0x006C8EF0) calls slot 14 with 6, the fade (0x006C60F0), which shows nothing new: the screen of step 2 stays until the mode's leave (0x006C5A00) hides it.
  - **The loading mode's leave (0x006C5A00)** (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2) [C]:
    - The leave first calls `[0x00C31650]`'s slot 0xF0 with 0, then 0x008472C0 → 0x00844F60 (not read).
    - It then hides the screen +0x14C through GUI slot 0x38 and sets +0x14C to −1.
    - Finally it calls 0x006945A0, the base leave.
  - **0x006C5FE0, which sets the loading screen** (read 2026-10-23 by paragon-agent-2's helper, checked by lib-agent-2) [C]:
    - When the id differs from +0x14C, it hides the old screen through GUI slot 0x38 (unless the old one is −1).
    - It stores the new id.
    - It shows the new screen through slot 0x34 when the loading mode is the controller's current mode.
- **A transition to the area the party is in does nothing at all: no reload and no move** (read 2026-10-21 by lib-agent-2, for DAOParagonEngine) [C unless marked].
  - **The command checks only that the tag is known.** `CCModule::ForScript_DoAreaTransition` (0x00600A40) looks the area's tag up in the module's areas. If it is known, it always pushes a kind-7 loading request (0x00600B6A..0x00600BAC) and returns 1 to the script. It does not test whether the area is the current one.
  - **State 7 drops the request.** Its lookup, 0x006C7080, finds the target in the area list in memory (`0x006FA910` on module +0x2CC). It then compares the target's id with the controlled creature's area id: `[[app+0xD0]+0x5C]` through 0x0069D2E0, then +0x254, at 0x006C7236..0x006C7246. If they are equal it returns false, and so does any failed lookup (0x006C725F, 0x006C733E).
  - **What the false means.** State 7 (0x006C7380) then calls the loading mode's slot 14 with 1, kind 1: no screen. Nothing is saved, unloaded or loaded, the waypoint is never read, the party is not placed (the arrival 0x006C73C0 is not reached), and no load event or enter script runs. How the mode then hands back to the game was not read [I: the same way as any finished request].
  - **The scripts don't guard it.** `UT_DoAreaTransition` (`utility_h.nss`:1051) passes any area to `DoAreaTransition`. A move within an area is the jump commands' job (`UT_LocalJump`, `utility_h.nss`:1148; `UT_TeamJump`:1749).
- **A transition to a tag no area has does nothing, and the command answers 0** (read 2026-10-22 by lib-agent-2, for DAOParagonEngine #164) [C unless marked]:
  - **The command's lookup.** `CCModule::ForScript_DoAreaTransition` (0x00600A40) looks the area's tag (the string at arg+0x44) up in the module's area map (+0x2D0, 0x00512A10). If it is not there, it looks in each area list of the vector at +0x300..+0x304 (0x2C bytes an entry). Only when one has it does it push the kind-7 request (0x00600B6A). Otherwise it returns false (0x00600B66): no request, no loading mode, and the waypoint is never read.
  - **The script sees 0.** Command 745's wrapper (0x0065F5F0) pushes that byte as the int result (0x0065F712..0x0065F71C). `UT_DoAreaTransition` (`utility_h.nss`:1051) ignores it. Before the call it has already set a load hint (`SetLoadHint`), which then goes unused until the next load [I].
  - **The placeable path.** A placeable's `AREA_TRANSITION` action goes `placeable_core` → `Placeable_DoAreaTransition` (`placeable_h.nss`:28): `PLC_AT_DEST_AREA_TAG` and `PLC_AT_DEST_TAG` from its locals, then `UT_PCJumpOrAreaTransition` (`utility_h.nss`:1636). That one jumps the leader locally (`UT_LocalJump`) only when the area tag equals the leader's area's tag; refuses in combat; otherwise calls `UT_DoAreaTransition`.
  - **The mage origin's stair** (`to_level_3` in `bhm200ar_tower_level_2`; template `bhm200ip_to_level_3.utp`, `EventScript` `placeable_core`, `PickLock` 10000, `PLC_AT_DEST_AREA_TAG` and `PLC_AT_DEST_TAG` both "none", the same in all three copies) [C, the template; the area's placed instance not checked]: an area-transition use runs `placeable_core` → `Placeable_DoAreaTransition`. The area "none" is not the leader's area's tag, so `DoAreaTransition("none", "none")` runs and does nothing. `bhm200ip_to_level_3.nss`, the script that sets `DOOR_TO_THIRD`, is not this template's script (corrected 2026-10-22 by lib-agent-2 from paragon-agent-2's reading; this said the stair ran it and the flag's GET started the templar's line). That no area is tagged "none" is [I].
- At 0x006C7F98 a LightLoading still shown when a game or save load follows becomes Loading. When that happens is not read.
- **The load-failure routine** (0x006C5A60, its error code the argument) sets the state's error, calls slot 14 with 0, and goes to the pregame mode (8) with a message: back to the main menu.
- **A custom loading image is not in 1.05.** CustomLoading's scene (`GUICustomLoading`, 0x00728BC0) has a `LoadingImage` token and no movie in the install, and `SetLoadImage` (361, 0x0064E450) is a stub.

### The hint

- **Set by `SetLoadHint` alone** (0x00660EE0): it reads the row's `StrRef` (column hash 0x406341BE) of the table it is given (206 by default) and stores the string's text, not the strref, in scene 14's +0xC0 (`ScriptBackend::ForScript_SetLoadHint`, 0x00652730), shown or not. `GUILoading` (0x007289F0) names it `GUILoading.LoadingText`.
- **Only `load_town` shows it** (`LoadScreen.as:30`, observer 1500): when the text is not empty and has no "?" (a missing string), fading in to 71% over 1.5 s. `load_light` and `load_fade` have no hint, so a transition within the area list in memory shows none even when a script set one. Nothing in the movie picks a hint.
- **The scripts pick it:** `UT_DoAreaTransition` (`utility_h.nss`, about lines 1051 to 1095) calls `SetLoadHint` before each `DoAreaTransition`:
  - with the module's `AREA_LOAD_HINT` set: that row of `loadhints` (206), and the local is reset;
  - otherwise a random row by the hero's level: up to 3, table 284 (`vlowlevelloadhints`); 4 to 7, 277 (`lowlevelloadhints`); 8 to 12, 275 (named `HighLeveLoadHints`); 13 on, 276 (named `MidLeveLoadHints`). The script's comments have the last two the other way round.
- **Not read:** the hint on a new game's or a save's load (whether a stale one shows); how markup such as `<LeftClickDefaultCapped/>` in a hint becomes text; `SendEventTransitionToWorldMap` has no engine side: it is a script event to the module (script_commands/arrays.md, "Leaving a random encounter") [I for no engine reader] (corrected 2026-10-23 by lib-agent-2); whether the world map scene's +0xBC is set on a normal world-map trip; the kind-3 early-out in slot 5 (0x006C6DC2–0x006C6DFE).
