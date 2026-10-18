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

`EVENT_TYPE_AREALOAD_SPECIAL` (40), 41, 37 and 80 were not found raised in the exe, and no probe log has them.

## A creature

`EVENT_TYPE_SPAWN` (3) goes to each creature once, the first time it is placed in an area (`CCCreature`'s slot 63, `0x006b6bc0`, its flag at +0xC54), and only if the creature's script takes event 3.
