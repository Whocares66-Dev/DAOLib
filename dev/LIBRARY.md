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
| Lookups by id: object, creature, placeable | 2026-09-30, from the commands that call them | checked 2026-09-30, on every getter call |
| Lookups by id: game object, waypoint | 2026-09-30 | not exercised |
| 14 fields: `CCObject`'s group and team, `CCCreature`'s packages, treasure category, combatant type and combat state, `CCPlaceable`'s lock level, key, auto-remove key, base type, action, treasure rank and category | 2026-09-30, from their getters | matched on every call, 2026-09-30 |
| 4 fields: `CCCreature::BodyBag`, `CCObject::AILevel`, `CCPlaceable::State`, `CCWayPoint::MapPinState` | 2026-09-30, from their getters | not called in play yet |
| The command table, `InitializeCommands`, setting a slot | 2026-09-30 | used 2026-09-30: 18 slots replaced, the game ran on |
| The VM stack: int and object | 2026-09-30, voted across 665 commands | used 2026-09-30: on each of about 2,200 calls, the argument and the answer each popped and pushed back |
| The VM stack: float | 2026-09-30 | not exercised |
| `CCCreature::SetCombatState` | 2026-09-30 | not exercised |

## The first session

2026-09-30, its first eight minutes: a fight, some chests, the world map. The probe's detour on `InitializeCommands` ran five seconds into start-up, on the commands object at `CClientExoApp+0x1E4`, and shadowed all 18 getters. Scripts then called 14 of them about 2,200 times; every answer matched our field, with no failed pop or push. `GetCombatState` alone ran 978 times, `GetCombatantType` 366, `GetPackageAI` 330, `GetPackage` 273. Once, `GetCombatantType` was asked about an id that named no creature, and the engine's handler still answered, as the getters do when their lookup fails (they push a default: `GetTeamId`'s body, `0x0065BCD0`); our lookup found nothing, as it should.

The first matches included values other than 0, among them a package of 0x58, a team of -1, a group of 1, a placeable action of 0xB, a creature treasure category of 8 and a lock level of 1. Zero is the weak case: a wrong offset often reads 0 where the true field is 0 too. Since this session the probe also counts matches on values other than 0, and logs the first.

Every script command ran on one thread, the same one `InitializeCommands` ran on: the script VM's, and so the thread a turn-based mode's script-facing work happens on. It is not the process's first thread, which ran the loader.

## What comes next

- **Setters.** `dev/script_backends.csv` names the engine functions the set-commands call; each is a wrapper once its argument types are read from the command body, as `SetCombatState` was.
- **Strings, locations, effects.** The VM's pops and pushes for these are named (`dao/Addresses.h`), but their types are not yet: `CExoString` and the engine structures. NWN:EE's headers give the likely shapes.
- **Fields with no getter.** The harvest only finds fields a getter returns. The rest come from reading the engine's own code, a class at a time, into a header like `ClientExoApp.h`.
