# Script command handlers: area (DAOParagonEngine #27)

Read 2026-10-18 by lib-agent-2 in the GOG exe; not seen in play. Each section points to the note that describes the function first, with the handler's edge cases that note lacks.

### DoAreaTransition (745)
- **Handler:** 0x0065F5F0 (own handler, registered 0x0067687A); calls `CCModule::ForScript_DoAreaTransition` 0x00600A40 (checked).
- **Described in:** `dev/LOAD_EVENTS.md` ("The party placed on arrival": scripts only call it through `UT_DoAreaTransition`, `utility_h.nss`:1051-1095; the engine places the party in 0x006C73C0).
- **Does:** pops the area name and waypoint tag (two strings), hands them to the module, which queues the load; pushes 1 if a module exists and accepted, else 0 (checked).
- **Edge cases:** a failed pop returns -639, a failed push -638 (checked). With no current module (`0x005D4FC0` null) it pushes 0 and does not fail the script (checked). The handler only queues the transition, so a 1 does not mean the new area has loaded (inferred from the module call's name; the module's body not read). An unknown area or tag is not tested here (checked: no test in the handler).
- The handler also copies a word from its own `this+8` into the request (the calling object, inferred).

## Not read
- `CCModule::ForScript_DoAreaTransition` 0x00600A40's body: what it rejects.
