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

## The game object

Every game object is a `CGameObject` at its start, then DAO's own bases (`tools/daoexe.py --bases CCCreature`, from the RTTI's class hierarchy descriptors):

| Offset | Base | What it is |
|---|---|---|
| +0x00 | `CGameObject` | the vtable of 29 slots NWN's shape begins with; the id at +0x10, the type byte at +0x14 |
| +0x04 | `SmartData::Attribute<unsigned int>` | the id, as an observable attribute: its value at +0xC of it |
| +0x18 | `SmartData::SmartObject` | DAO's attribute and message system: a message receiver and observer, a hash map of named attributes |
| +0x70 | `SEffectsHolder` | the effects on the object (+0xCC in an area) |
| +0x440 | `ItemPossessor` | the inventory, in creatures, placeables and stores (+0x70 in the party) |

`dao::CGameObject` gives the id, the type (`dao::core::ObjectType`, one bit off the scripts' `OBJECT_TYPE_*`) and the engine's casts, `AsCreature()` and the rest, virtual calls as the engine's own lookups make them. The probe checks all three on every object a script check looks up.

## The value types

What a script passes besides ints, floats and objects, read 2026-09-30 from the VM's pops and pushes and the commands that use each (`dao/Structures.h`, `dao/String.h`, `dao/Script.h`):

| Script type | In the engine | Who frees it |
|---|---|---|
| `string` | MSVC 2005's `std::wstring`, 0x1C bytes, UTF-16 (`dao/core/Msvc8.h`) | the engine's heap: `dao::WString` frees through the engine's destructor; `Stack::PopString`/`PushString` copy to and from a plugin's own `std::wstring` |
| `vector` | three floats, pushed and popped one at a time | nobody: plain values |
| `location` | 0x30 bytes, NWN's `CScriptLocation` shape: position at +0, orientation at +0x10, area id at +0x20 | plain data; a pop copies it out and frees the VM's copy |
| `effect`, `itemproperty` | `CGameEffect`, 0x7C bytes, polymorphic: id, type, duration type, animation, flags, creator, ability id, and lists of int, float, object and string parameters | a pop is the VM's copy, the popper's to destroy: `Stack::PopEffect` returns it owned, destroyed through the engine |
| `event` | `CScriptEvent`, 0x50 bytes, polymorphic: type at +0x20, creator at +0x24, target at +0x28, and the same parameter lists as an effect (both are `CDataArrays` at +0) | as an effect: `Stack::PopEvent` returns it owned |
| `T[]` | 8 bytes: a tag (0x40 int, 0x41 float, 0x42 string, 0x43 object, 0x44 resource, 0x50 plus a structure's number) and a payload shared between copies, a `std::vector` of 4-byte elements with a reference count (`dao/core/ScriptArray.h`) | a pop is the popper's 8 bytes, sharing the elements: `Stack::PopArray` returns a `dao::script::Array` that frees them through the engine |
| `command` | from a pool the application keeps | not read yet |

The engine structures are made, destroyed and copied by the commands object's slots 5, 6 and 8, which is also how the VM copies one on every push and pop.

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
| `CGameObject`: id, type, the casts to object, creature and placeable | 2026-09-30, from the constructor and the vtables | checked 2026-09-30, on all 1,100 objects the second session's script checks looked up |
| Strings, locations and effects on the stack; the effect's type, duration type, creator and ability id | 2026-09-30 | checked 2026-09-30, third session: 411 `GetTag` and 43 `GetLocation` round trips, 95 effect getter calls, all matching |
| Vectors; the effect's flags and animation | 2026-09-30 | not called in play yet |
| Events and arrays on the stack; the event's type, creator and target; `GetTeam`, `GetEffects`, `GetNearestObject` answers | 2026-09-30 | checked by the script checks, from the next session |
| `CCCreature::SetCombatState` | 2026-09-30 | not exercised |

## The first session

2026-09-30, its first eight minutes: a fight, some chests, the world map. The probe's detour on `InitializeCommands` ran five seconds into start-up, on the commands object at `CClientExoApp+0x1E4`, and shadowed all 18 getters. Scripts then called 14 of them about 2,200 times; every answer matched our field, with no failed pop or push. `GetCombatState` alone ran 978 times, `GetCombatantType` 366, `GetPackageAI` 330, `GetPackage` 273. Once, `GetCombatantType` was asked about an id that named no creature, and the engine's handler still answered, as the getters do when their lookup fails (they push a default: `GetTeamId`'s body, `0x0065BCD0`); our lookup found nothing, as it should.

The first matches included values other than 0, among them a package of 0x58, a team of -1, a group of 1, a placeable action of 0xB, a creature treasure category of 8 and a lock level of 1. Zero is the weak case: a wrong offset often reads 0 where the true field is 0 too. Since this session the probe also counts matches on values other than 0, and logs the first.

Every script command ran on one thread, the same one `InitializeCommands` ran on: the script VM's, and so the thread a turn-based mode's script-facing work happens on. It is not the process's first thread, which ran the loader.

## The second session

2026-09-30, a fight. With the counts of matches on values other than 0: `GetCombatState` matched on a value other than 0 in 570 of its 632 calls, `GetPackageAI` in 224 of 292, `GetGroupId` in 33 of 41, so those fields read real, changing values. `GetCombatantType` only ever answered 0 in either session, so it stays the weakest of the 14. Every object looked up, 1,100 of them, had the id asked for at +0x10, its class's type byte at +0x14, and the engine's cast from the game object gave the typed lookup's pointer.

## The third session

2026-09-30, a fight, with the value types shadowed. `GetTag` ran 411 times and every answer came back the same through our pop and push, among them 21-character tags that live on the engine's heap (`bhn200cr_howe_landra1`). `GetName` gave `Dog` and `Lesser Lyrium Potion`. `GetLocation` ran 43 times: positions such as (-110.26, -79.82, 12.02), unit orientations, and an area id that named an area every time. With no spells in the party, the fight still put effects on its creatures: 46 `GetEffectType`, 19 `GetEffectCreator`, 16 `GetEffectAbilityID` and 14 `GetEffectDurationType` calls, each matching our accessor on the probe's own copy of the effect, the creators real creatures and the effect ids counting up (654, 655, 656).

## What comes next

- **Setters.** `dev/script_backends.csv` names the engine functions the set-commands call; each is a wrapper once its argument types are read from the command body, as `SetCombatState` was.
- **Strings, locations, effects.** The VM's pops and pushes for these are named (`dao/Addresses.h`), but their types are not yet: `CExoString` and the engine structures. NWN:EE's headers give the likely shapes.
- **Fields with no getter.** The harvest only finds fields a getter returns. The rest come from reading the engine's own code, a class at a time, into a header like `ClientExoApp.h`.
