# The library, as a plugin uses it

What `daolib` gives a plugin beyond addresses: engine objects as C++ classes with named accessors, the engine's own functions as member calls, and the script command table as something a plugin can read from and add to. Built 2026-09-30.

## Three layers

The tables in `dev/` map addresses to names. Getting from those to code a plugin calls takes three layers, each checked against the one below.

1. **Generated descriptors.** `tools/gen_script_fields.py` turns `dev/script_fields.csv` (each field a getter command reads, found by following the command's body) and `dev/script_commands.csv` (its script-side name and type) into `src/dao/generated/ScriptFields.h`. Each field is a `dao::Field<T, Depth>` (`dao/Field.h`): a path of offsets (`dao/core/FieldPath.h`, tested without the game), read by `Get` as the engine reads it or by `TryGet` with every load guarded. The ctest `generated_fields` fails when the header no longer matches the tables, so a new harvest means running the generator again.
2. **Hand-written classes.** `dao/Objects.h` gives the descriptors natural names on the RTTI's classes, and wraps the engine functions whose signatures are read. `dao/ClientExoApp.h` is the way in: objects by id, pause, game mode. `dao/Script.h` is the command table and the VM's stack. The classes are opaque: never made, copied or destroyed by a plugin, only pointed at. Nothing of the engine is reimplemented.
3. **Checks in play.** Every field harvested from a getter has a free test: the getter's own answer. The probe shadows each getter's slot in the command table, lets the engine answer, then reads our field on the same object and compares (`dev/PROBE.md`, "Script checks"). A field is trusted once a session of play has matched it on every call.

## What a plugin writes

```cpp
#include "dao/ClientExoApp.h"

auto *app = dao::CClientExoApp::Get();
if (auto *creature = app->GetCreatureById(id))
{
    if (creature->CombatState() != 0 && creature->TeamId() == playerTeam)
    {
        creature->QueueCommand(command, /*addToFront=*/true);
    }
}
app->SetPaused(true);
```

A new script command goes into a free slot of the table, once the engine has filled it (`dao/Script.h` says when):

```cpp
#include "dao/Script.h"

int __fastcall IsTurnBased(void *commands, int edx, int id, int paramCount)
{
    dao::script::Stack::PushInt(g_turnBased ? 1 : 0);
    return dao::script::kOk;
}

dao::script::Set(kIsTurnBasedCommand, &IsTurnBased);
```

A script calls it once a `script.ldf` the toolset compiles against declares `int IsTurnBased() = <number>;`. About 150 of the 863 slots are empty. Replacing a command's handler works the same way: `Set` returns the handler it replaced, to call on.

## What is known how well

| Piece | Read | In play |
|---|---|---|
| `CClientExoApp`: pause, paused time, game mode | 2026-09-29 | seen, 2026-09-29/30 |
| `CCObject::QueueCommand`, the command word | 2026-09-30 | seen, every action in several fights |
| Lookups by id: game object, object, creature, placeable, waypoint | 2026-09-30, from the commands that call them | lookups by object seen through `QueueCommand`; the typed ones checked by the script checks, from the next session |
| 18 fields on `CCObject`, `CCCreature`, `CCPlaceable`, `CCWayPoint` | 2026-09-30, from their getters | checked by the script checks, from the next session |
| The command table and `InitializeCommands` | 2026-09-30 | from the next session |
| The VM stack: int, float, object | 2026-09-30, voted across 665 commands | used by the script checks, from the next session |
| `CCCreature::SetCombatState` | 2026-09-30 | not exercised |

## What comes next

- **Setters.** `dev/script_backends.csv` names the engine functions the set-commands call; each is a wrapper once its argument types are read from the command body, as `SetCombatState` was.
- **Strings, locations, effects.** The VM's pops and pushes for these are named (`dao/Addresses.h`), but their types are not yet: `CExoString` and the engine structures. NWN:EE's headers give the likely shapes.
- **Fields with no getter.** The harvest only finds fields a getter returns. The rest come from reading the engine's own code, a class at a time, into a header like `ClientExoApp.h`.
