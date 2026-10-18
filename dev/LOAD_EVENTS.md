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

## A creature

`EVENT_TYPE_SPAWN` (3) goes to each creature once, the first time it is placed in an area (`CCCreature`'s slot 63, `0x006b6bc0`, its flag at +0xC54), and only if the creature's script takes event 3.

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
