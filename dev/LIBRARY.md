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

## Calling any script command

Everything a script can do, a plugin can do through the same commands. `dao::script::Invoke` (`dao/Script.h`) calls one the way a script does: the arguments pushed on the VM's stack last first, the command's handler run from the table, the answer popped; a failed call cuts the stack back to where it was. `dao/generated/ScriptApi.h` (`tools/gen_script_api.py`) wraps each of the 661 commands whose types the library moves in a function with its `script.ldf` name, argument order and defaults:

```cpp
#include "dao/generated/ScriptApi.h"
using namespace dao::script;

const std::int32_t team = api::GetTeamId(creature);
Array members = api::GetTeam(team);                     // object[]
OwnedCommand attack = api::CommandAttack(enemy);
api::AddCommand(creature, *attack.get(), /*bAddToFront=*/1);
const std::wstring tag = api::GetTag(creature);
```

`OBJECT_SELF`, the object a command acts for when the script does not say, is `dao::script::Self()`, the commands object's +8; `ScopedSelf` sets it for a call. Calls belong on the scripts' thread (the game's logic thread, where a hook on a command or on the engine's update is called), since the VM is not made for two. A `resource` (a file by name, `R"gen00fl_dog.utc"` in a script) is `dao::Resource`: on the stack a string under a tag of its own. The four commands with a `player` or `any` argument are not wrapped; the header lists them, and `Array::Size()` does what `GetArraySize` does. The probe calls three commands this way from inside its shadows and compares their answers with what it knows (`dev/PROBE.md`).

What the commands answer that is not in their documentation, found by the Eluvian server in play, 2026-10-01, on plugin API 2:

- **Plots.** `GetPlotGUID(resref)` gives a plot's GUID ("" for no such plot) and `GetPlotResRef(guid)` the name back; `GetPlotEntryName(guid)` the journal title's string id; `GetPlotFlagName(guid, n)` a flag's name, "" where there is none, the main flags from 0 and the defined ones from 256; `GetPartyPlotFlag(GetParty(hero), guid, n, 0)` whether a flag is set. `GetPlotSummary(guid, flag)` was "" for every set flag tried. What the journal holds, without knowing names, is `dao/Journal.h`.
- **Tactics.** `GetNumTactics`, `IsTacticEnabled`, `GetTacticTargetType`, `GetTacticCondition`, `GetTacticCommand` and `GetTacticCommandParam`, slots from 1; the names are in tables 140, 137, 141, 214 and 187 (`Label`, and `StrRef` for the player's language).
- **A container's contents.** `GetItemsInInventory(placeable, GET_ITEMS_OPTION_BACKPACK)` lists a chest's, a body's or a bone pile's items without opening it.
- **A trap.** `GetM2DAString` on a column that holds integers (`StrRef`, the tactics tables' `Type`) does not fail: it answers bytes read from memory as text. `GetM2DAInt` reads those columns right; `dao::gda` (below) checks a column's type.
- **Where an exit leads.** Script data, not the engine's: a transition placeable's local strings `PLC_AT_DEST_AREA_TAG` (the area), `PLC_AT_DEST_TAG` (the waypoint's tag there) and `PLC_AT_WORLD_MAP_ACTIVE_1` to `_5`, a transition trigger's `TRIGGER_AT_DEST_AREA_TAG` and `TRIGGER_AT_DEST_TAG` (`placeable_h.nss`, `trigger_core.nss`). Some doors set them in their own script when used, so a read before can be empty or the last use's. A placeable that is no exit answers `none` for both, its template's default (seen in play by the Eluvian server, 2026-10-02: the ruins' two exits named `bed100ar_forest_clearing` and `bed100wp_from_ruins`, everything else `none`).

## Intercepting a script command

`dao/ScriptHooks.h`: a plugin's function runs whenever a script calls a command, reads the call's arguments in place on the VM's stack, and decides what happens. `Proceed()` runs the handler the intercept replaced, the engine's command or another plugin's intercept, so intercepts from several plugins chain; the answer can then be read and replaced.

```cpp
dao::script::InterceptCommand(dao::script::id::GetCombatState, [](dao::script::CommandCall &call) {
    const auto creature = call.Object(0);   // the first argument
    const int result = call.Proceed();       // the engine answers
    if (result == dao::script::kOk && KeepCalm(creature))
        call.SetIntAnswer(0);                // the script sees 0
    return result;
});
```

`dao::script::id` has every command's number by its script name. The engine fills its command table after plugins load; an intercept made at load is installed as soon as it has (`OnCommandsReady`, a detour on `InitializeCommands`), one made later at once. `src/plugins/example` is a whole plugin doing this, and a test of it in play: it intercepts `GetCombatState` beside the probe's own shadow of it, compares the answer with `CCCreature::CombatState()`, and calls `api::GetTag` from inside the intercept.

## The game's data tables

`dao/Gda.h`: the GDA tables the engine has loaded (abilities, items, packages, ranges, 162 in all), read by table, row id and column name: `dao::gda::GetInt(Table::AbilitiesTalents, ability, "TargetType")`, and `GetFloat`. The tables are the scripts' `TABLE_*` numbers, now `dao::script::Table` (from `2da_constants_h.nss`); a column is found by the game's hash of its name, the CRC32 of the lowercased name as UTF-16LE (`dao/core/GdaHash.h`, tested against hashes the engine itself passes, and matching all 2,205 column names xoreos knows). These go straight to the engine's table getters, the ones `GetM2DAInt` and `GetM2DAFloat` use. The example plugin compares the two ways in play.

Which columns a table has is in `dev/gda_tables.csv`, read from the game's own files by `tools/gda.py` (GFF4 as xoreos reads it; tested on a file built in the test): all 749 files of `2da.erf`, 4,892 columns with their types, 4,830 of them named from xoreos's list. A numbered table is an M2DA, every file whose name starts with its prefix (`m2da_base.gda`: table 1, Abilities, is the `abi_*` files, whose columns include `TargetType`, `Range`, `Speed`, `Cooldown`). `python tools/gda.py --dump abi_base` prints a table's rows, and `--names` adds the text of each `*StrRef` column from the game's talk table (`tools/tlk.py`: 51,846 English strings; all 1,501 talk tables in the install, add-ons included, are GFF4 `TLK ` version 0.2), so ability 1 reads as Shield Pummel with its description.

## Script events

`dao/Events.h`: `dao::events::OnScriptEvent(callback)`, or `OnScriptEvent(EventType::Damaged, callback)`, runs a plugin's callback on each script event the engine's queue delivers, with its target, just before the target's scripts handle it. It detours `AIMaster::ProcessEvent`, which the fifth session saw deliver 824 events: damage, perception, attack impacts, ability casts, heartbeats, mode changes, module and area loads, and the scripts' own numbers. Most `COMMAND_COMPLETE`s go another way, so a callback sees only some of those. Callbacks run on the scripts' thread and only observe. The example plugin logs the first module loads, mode changes, damage and perception events it sees.

## Every frame

`dao/Frame.h`: `dao::OnFrame(callback)` runs a plugin's code once per frame, just before the game's update, on the thread that runs it. The update is `EclipseGame::Update` (`0x00412EB0`), which `Eclipse::UpdateTask` calls each frame on the object in `g_eclipseGame`. Whether that thread is the scripts' own, and so whether a frame callback may call script commands, is `dao::FrameThreadId()` against `dao::script::ScriptThreadId()`; the probe logs the two from the next session, with frames counted paused and unpaused.

## The engine's messages

The engine's classes send each other messages, and 314 handlers answer them (`dev/MESSAGE_HANDLERS.md`), each `bool Owner::HandleMessage(const Message &)`. `dao/generated/MessageHandlers.h` (`tools/gen_message_handlers.py`) names the 308 non-trivial ones as addresses to detour, `dao::messages::handler::GameModeExplore_PerformAbilityMessage` and so on, with a table of all of them; 281 have their one argument confirmed by their `ret 4`. `dao/Messages.h` has the typed handler signature and the message layouts read so far: `PerformAbilityMessage` (ability, caster, target: the player's ability orders, which a turn-based mode would gate) and `MoveCharacterToLocationMessage` (no fields: a click to move). A message class's clone, slot 1 of its vtable, allocates its size and copies its fields: `tools/messages.py` reads all 296 classes' clones into `dev/messages.csv` (size, field offsets and widths, which classes handle each), 237 of them wholly accounted for; 151 carry nothing but their class. `dao::messages::As<M>(message)` tells a typed message by its vtable, and `Messages.h` also has `SetGameModeMessage` (the new mode) and the three pause messages.

`dao/GameModes.h`: the `GameModeController` at `CClientExoApp+0x114`, whose current mode `CClientExoApp::Mode()` has read since the first session, now a class of its own: `Mode()`, `SetMode()` (the engine's own switch, as a `SetGameModeMessage` makes it), and the table of mode objects by number. The probe logs which class sits at each number.

## The scripts' constants

`dao/generated/ScriptConstants.h` (`tools/gen_script_constants.py`) has every constant the engine declares in `script.ldf` and the effect types the scripts declare in `effect_constants_h.nss`: 609, as 37 typed enums (`dao::script::EventType`, `EffectType`, `EffectDurationType`, `CommandResult`, `InventorySlot`, `ObjectTypeFlags`, ...) and again under their script names (`dao::script::constants::EVENT_TYPE_COMMAND_COMPLETE`). The accessors return the enums where the constants name the field: `CScriptEvent::Type()` is an `EventType`, `CGameEffect::Type()` an `EffectType`. An enum is 32-bit, so a number no constant names, like a script's own event type, still fits.

A creature property's id is a row of the game's properties table (`properties.gda`), which the engine builds each creature's properties from, and three of `script.ldf`'s ids are stale. `PROPERTY_ATTRIBUTE_MOVEMENT_SPEED_MODIFIER` (20) is the table's `Flanking_Angle`, `PROPERTY_ATTRIBUTE_MAGIC_CRIT_MODIFIER` (22) its `Missile_Shield`, `PROPERTY_SIMPLE_AGE` (18) its `AI_BEHAVIOR`, and the game has none of the three. The generator checks each id against the table and says so beside a stale one. The turn-based mod found the first in play, 2026-09-30: every creature's "movement speed" read 60, a flanking angle. Movement speed is not a property. Haste and slows are effects: `EFFECT_TYPE_MOVEMENT_RATE`, or `_DEBUFF` when hostile, with the rate in the engine float `EFFECT_FLOAT_POTENCY` (`effects_h.nss`, `EffectModifyMovementSpeed`). The paces themselves are columns of the appearance table, `apr_base`: `Game_WalkSpeed`, `Game_RunSpeed`, `Game_CbtSpeed`.

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
| `command` | 0x14 bytes from a pool the AI master keeps: the word (sequence and type), a `CDataArrays` of parameters of its own, the static and player-issued flags (`dao::Command`, whose comment tables each type's parameters) | a pop is the popper's copy, a real one from the pool; `Stack::PopCommand` and `dao::script::Copy` return it owned, released to the pool |

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
| Calling commands from C++: `Invoke` and the generated wrappers | 2026-09-30 | checked 2026-09-30, fifth session: 700 `GetObjectType`, 528 `GetStringLength` and 73 `GetPositionFromLocation` calls from inside script commands, every answer right; `GetTag` and `GetM2DAInt` from the example plugin |
| Intercepting commands: `InterceptCommand`, `CommandCall`, chaining across plugins | 2026-09-30 | checked 2026-09-30: the example plugin's `GetCombatState` intercept, chained with the probe's shadow of the same command, read its argument and the engine's answer on 300 of 300 calls, 0 and 1 |
| The frame callback and the scripts' thread | 2026-09-30 | checked 2026-09-30: the update and the scripts run on one thread, so a frame callback may call script commands; the update runs while paused too (26,849 paused frames in the session) |
| The event queue: what it delivers | 2026-09-30 | seen 2026-09-30: 824 events, all script events (queue id 10) but two; `COMMAND_COMPLETE` among them (14), so the engine delivers some command completions another way |
| The game mode controller: current and previous mode | 2026-09-30 | seen 2026-09-30: +0x78 the current mode (0, exploring) and +0x7C the one before (9, loading) |
| The game mode controller's array of modes | 2026-09-30 | checked 2026-09-30, seventh session: +0x58 points to an array of 13, one GameMode class per number, the same numbers as the scripts' GM_* constants (0 Explore, 1 Combat, ... 11 CharGen, the main menu's, 12 PartyPicker) |
| Script event callbacks, `dao::events::OnScriptEvent` | 2026-09-30, on the queue the probe watched in play | checked 2026-09-30, sixth session: the module's load (20) and mode changes (60), and perception (25) for the player, the dog and Bryce's wife, each with its target |
| The data tables through `dao::gda` | 2026-09-30 | checked 2026-09-30: Shield Pummel's AbilityType (1) and Cost (25.0) read the same straight from the table as through `GetM2DAInt` and `GetM2DAFloat` |
| `CGameObject`: id, type, the casts to object, creature and placeable | 2026-09-30, from the constructor and the vtables | checked 2026-09-30, on all 1,100 objects the second session's script checks looked up |
| Strings, locations and effects on the stack; the effect's type, duration type, creator and ability id | 2026-09-30 | checked 2026-09-30, third session: 411 `GetTag` and 43 `GetLocation` round trips, 95 effect getter calls, all matching |
| Vectors; the effect's flags and animation | 2026-09-30 | not called in play yet |
| Events on the stack; the event's type, creator and target | 2026-09-30 | checked 2026-09-30, fourth session: 3,635 `GetCurrentEvent` round trips, 4,514 event getter calls, all matching |
| Arrays on the stack: `object[]` and `effect[]` | 2026-09-30 | checked 2026-09-30: 5 `GetTeam` answers, every member on the team asked for; 493 `GetEffects` answers with the right tag, all empty, so an effect element is not yet read in play |
| `CCCreature::SetCombatState` | 2026-09-30 | not exercised |
| An object's command queue: the current command, one by position, the count; `QueueCommand`'s callers, and that the queue always keeps the command | 2026-09-30 | checked 2026-09-30, eighth session: of 112 queued commands, the player-issued flag was set on every one of the player's (drive, 33, from 0x006A22AE; use object, 31, and attack, 1, through QueuePlayerCommand; move to point, 20, through QueueNewCommand) and on none of the 84 the AI queued through AddCommand; the client command table's AddCommand was not called |
| Data tables' strings, bools, row and column counts | 2026-09-30 | checked 2026-09-30, eighth session: abilities' Label "SHIELD_PUMMEL", 697 rows and 51 columns, the same as `GetM2DAString`, `GetM2DARows` and `GetM2DAColumns` |
| Sending a message: `dao::messages::Send`, the registry and its dispatch | 2026-09-30 | checked 2026-09-30, ninth session: a `CenterOnObjectMessage` for the player's creature, sent to the current mode, went to GameModeExplore and was handled |
| The Flash UI's calls into the game: FSCommands and ExternalInterface calls by name, `GFxValue`, answering | 2026-09-30 | checked 2026-09-30, eighth session: FSCommands and ExternalInterface calls from Tooltips, PopupLayer, startmenu2 and SaveLoad, each with its movie's name, string arguments read as type 4, all on the scripts' thread; the manager is GUI::ScaleformManager |
| Calling into a movie: GFxMovie's vtable (SetVariable, the array setters, Invoke), the FSCommand answer convention (`_root.fsc_retval`) | 2026-09-30 | partly, 2026-09-30, eighth session: `IsAvailable("_root")` true and `ArraySize("_root.fsc_retval")` 0 on Tooltips; nothing set or invoked yet |
| The game's movies by name: the manager's set of records {name, movie}, walked with the engine's iterator | 2026-09-30 | checked 2026-09-30, ninth session: 9 movies while exploring, each loaded: FloatyLayer, ArmyControl, Minimap, Portraits, NavBar, Quickbar, PopupLayer, Notifications, Tooltips |
| The data tree (SmartData): finding an attribute by path, its value as text and as a number, its type by class, setting a plain one (bool, int, unsigned, short, byte, float) | 2026-09-30 | partly, 2026-09-30, tenth session: `GUI.SelectedChar` read "130" as text and 130 as a number; `Map.ShowAllLabels` and `CClientExoApp.GUIDialogResult` were not found, so those paths are wrong or not there while exploring; setting not yet |
| A plugin's own screen: a GUIScene registered as the game registers its 61, shown, hidden and closed through the GUI system | 2026-09-30 | checked 2026-09-30, ninth session: the turn-based mod's screen added from the frame callback at the main menu, its movie found as `turnorder.swf` in `packages\core\override`, and `HasMovie` true (DAOTurnBased's log); the example's screen, with no movie, not added (false), and showing and closing it harmless; OpenMovie's arguments as the eighth session read them; showing a screen with its movie not yet |
| Portraits: any creature's, rendered as the party's are, and the `img://` name a movie loads it by | 2026-09-30 | checked 2026-09-30, ninth to eleventh sessions: `RenderPortrait` on the player gave the party's own `img://portrait_player0`; the turn-based mod rendered enemies' (`img://portrait_bhn200cr_howe_landra33`), the name being "portrait_", the creature's tag and a counter; `ReleasePortrait` not yet |
| Talk tables: a string by its reference through the engine's CTlkTable | 2026-09-30 | checked 2026-09-30, eighth session: Shield Pummel's NameStrRef, 379897, read "Shield Pummel" |
| A creature's properties: every PROPERTY_* by value type, read and set through its CCCreatureStats | 2026-09-30 | checked 2026-09-30, tenth session, after the ninth's crash and its fix: the player's health 146 of 146, mana or stamina 115 of 115, level 2, strength 24 (base 22), and `GetCreatureProperty`'s health 146 and 146; setting not yet |
| Watching and taking messages: a detour on the registry's Dispatch, which every delivery goes through | 2026-09-30 | checked 2026-09-30, eighth to thirteenth sessions: each class seen with its receiver; a save's and a load's messages with their slot and kind; a click once per message, at the game mode controller, after the controller was found passing messages on to the GUI system and the current mode inside its own delivery. Nothing intercepted yet |
| Commands: the parameters of each type, the target, position, ability and wait; the static and player-issued flags; `CDataArrays`' flags and vectors and its setters | 2026-09-30, from the 17 script commands that build one | checked 2026-09-30, eighth session: AddCommand's commands read as use ability (25) with its target and ability (803) and move to object (21) with its target, and each copy agreed; the five words (the type in the word, the parameters, 1, static 0, player issued) as read |
| What is under the mouse cursor: explore mode's pick (the object, the ground point and its walkable verdict, the hits along the ray) and the latest move's destination, on explore and combat mode | 2026-09-30, from the pick (0x0070F270), the raycast and the click handlers | checked 2026-09-30, tenth session: primary clicks named the object clicked (a door, each enemy), with 2 to 20 hits along the ray; a move click on an unwalkable point read walkable false and the recorded destination stayed where it was; one on a walkable point read true, and the next click found that very point as the latest move's destination |
| The camera in explore and combat: the object followed, the focus, following another object with the game's pan, the snap; the two views, the focus offset, the scene camera's yaw, pitch and zoom | 2026-09-30, from the explore controller's update, its target change and CenterOnObjectMessage's handler | checked 2026-09-30, twelfth and thirteenth sessions and the turn-based mod's: the two views by the zoom (1 close, 0 tactical) and the controller's flag; the pitch (45 degrees in the tactical view, 65 to 125 close in); the yaw, equal to the direction from the focus to the camera within 0.02 in both views and not wrapped; the camera 3.8 m from the focus close in, 21.3 m in the tactical view; `Follow` and `SetYaw` putting the camera behind a party member, the yaw atan2(-fy, -fx) of its forward vector, or radians(90 - GetFacing): the scripts' facing is -(the forward vector's angle in degrees) - 90, not the yaw's measure. `SetFocusOffset` not yet |
| Camera moves: `PanTo` and `EaseYaw`, the player's pan seen through the game's own (`PlayerPan`), the tactical view's pull back to the party held while a pan runs, and why each move ended (`dao/CameraMoves.h`) | 2026-09-30, from the tactical step, the pull (`PullToParty`) and the player's pan | not yet this way. The first way, which took any offset change over 5 cm a frame for the player's, stopped short twice at the start of the hero's turn in the turn-based mod's play tests, leaving the view on an enemy's room |
| A plugin's save data: told after the game writes a save and before it loads one, with the slot, and a file of its own in the slot's folder | 2026-09-30, from SaveGame::Write and FindSave as seen in play | not yet: `examples/saves/` keeps a count of saves with each save and reads it back at each load |
| Asking for a save or a load as the menus and keys do, and a save's metadata (`dao::saves::Load`, `Save`, `Characters`, `List`, `Slot::Meta`; the metadata's reader in `dao/core/SaveMeta.h`) | 2026-09-30, from the save proxy's handlers and the saves on this machine | the reader: tests. Checked 2026-10-01 through the Eluvian server: `Characters`, `List` and `Meta` listed the ten saves there, and `Load` of a manual save, asked at the main menu, had it loaded and in explore mode 14 s later with no menu touched, `OnGameLoaded` called. `Save` not yet |
| A message box the game has up: its text and its buttons' labels and callbacks (`dao::gui::CurrentPopup`) | 2026-10-01, from the popup layer's constructor and the GUI system's dialog box handlers | checked 2026-10-01 through the Eluvian server: a script's ShowPopup read back its text, resolved, and its two buttons with their callbacks; nothing before the box was up |
| A plugin's own action: registered with the game's, listed in its controls screen and rebound there, its key kept in `KeyBindings.ini`, a callback at the next frame when it is pressed in a mode it names (`dao::input::RegisterAction`, `dao/Input.h`; `examples/input/`) | 2026-10-01, from the game mode controller's start and the input mapper | checked 2026-10-01 with `examples/input/`: the engine took both actions, F11 ran the callback once a press (once for a held key), silent with the map, the main menu and the inventory up, the actions written into `KeyBindings.ini`, the game's own keys unharmed. Then with the controls screen's rows: a key bound there ran its callback, a rebound one answered the new key only, both were still bound after a restart, and Restore Defaults put them back |
| The world on the screen: where a point of the world is drawn (`dao::world::ToScreen`), everything the game's pick would meet under a pixel (`PickAt`), and a pixel to click an object at, where the pick meets it before any other (`AimAt`); `examples/world/` logs them for the creatures nearby | 2026-10-01, from the cursor's ray cast and the scene's `Renderer::PerspectiveCamera` | checked 2026-10-01 through the Eluvian server: the view's size the window's client size, the controlled creature's feet at the screen's bottom middle, a point behind the camera not in front, the pick over her body meeting her, and `AimAt` finding the visible part of a wolf behind another creature. The first try hung the game: `ScreenSize` asked the wrong object |
| A plugin's settings in the game's options screen: a section on the Game tab, a heading and checkboxes that are the game's own, each asked its value when the screen opens and answering a click, which the plugin may refuse (`dao::options::AddSection`, `dao/Options.h`; `examples/options/`) | 2026-10-01, from the options movie and how the engine tells a movie its tokens' values | checked 2026-10-01 with `examples/options/` beside `examples/input/`: the heading and three boxes under the Game tab's left column in line with the game's own, each asked its value at every opening, a click kept and a refused one drawn back, tooltips, the game's own boxes unharmed, and the same in a fight; then the heading as the game's own gold bar, matching "Gameplay" |
| One of the game's movies changed as the game opens it, in memory: a patch is given the movie uncompressed and answers the changed bytes (`dao::gui::OnMovieLoad`), and calls are added to its script with every size and branch around them moved (`dao/core/GfxMovie.h`, `dao/core/Inflate.h`). `dao::input` lists a plugin's actions in the controls screen with it | 2026-10-01, from the file object GFx reads a movie through and the options movie's script | checked 2026-10-01 with `examples/input/`: the patched options movie opened every time, its other tabs as ever, with a "Mods" header and the two rows at the end of the key list, nothing doubled on a second opening |
| A message sent by its text, as a movie's button names one (`dao::messages::SendText`) and as a key's binding does (`SendBinding`), and pressing a message box's button with it (`dao::gui::PressPopupButton`) | 2026-10-01, from the movies' SendMessage and the engine's message makers | checked 2026-10-01 through the Eluvian server: a script's box left the screen at `PressPopupButton`, and at `SendText` with its own callback; an unknown receiver answered false. `SendBinding` paused and ran the game with `UserTogglePauseMessage`, changed the selection with `SelectPartyMemberIndexMessage`, and answered false for an unknown name. Whether a script heard a box's answer is not yet seen |
| Effects' time: a temporary effect's duration and time left, and the clock they all age on (hold it, advance it, decide per effect) | 2026-09-30, from Engine_ApplyEffectOnObject, the effects holder's update and AddElapsed | not yet |
| Regeneration's clock: health, mana and stamina coming back, held and advanced with the effects' (`dao/Clock.h`) | 2026-09-30, from UpdateRegeneration and the depletable property's step | not yet |
| Script timers on a plugin's clock: a damage over time's ticks and a lasting area's pulses, held and advanced with the effects' (`dao/Timers.h`) | 2026-10-02, from DelayEvent, the event queue's way in (AddEventDeltaTime), its update and its list, effect_dot2_h and the area scripts; asked for by the user's play, a trap's poison hitting a turn-based party all fight long | not yet |
| A creature's own clock: its effects, regeneration and timers moved by its own advances, the shared clock passing it by (`dao::clock::OwnClock`, `Advance(id, seconds)`) | 2026-10-02, from SEffectsHolder::AgeEffect (handed the owner) and CCCreature::Update (UpdateRegeneration's one caller); asked for by the user's play, "things on each creature's own clock" | not yet |
| A file's bytes as the game finds it (`dao::resources::Read`, `Exists`) | 2026-10-02, from the movie file's constructor and its length slot: Resources::Find, Data, Size, Release | not yet; `gui::HasMovie`, now built on it, was seen in play; the example plugin logs rules_core.ncs's size and header |
| The game's speed: both clocks, the world timer's scale and the frame timer (`dao::clock::SetSpeed`) | 2026-09-30, from the world timer, the objects' timers and EclipseGame's frame step | the first way failed in play (2026-09-30): a meter in `examples/speed` found the creature's own time at 1000 ms a wall second "at speed 2", since the application rewrites the world timer's scale from its own time scale each frame. `SetSpeed` now calls the game's own SetTimeScale, which reaches the world timer and the scene. Checked 2026-09-30, 20:02: at 2 the creature's time ran about 2020 ms a wall second and the hero ran 8 to 9 m/s against 4.5 |
| Asking for an ability's target: explore mode's BeginTargeting (`GameModeExplore::RequestTarget`) | 2026-09-30, from the quickbar's handler for PerformAbilityMessage | checked 2026-09-30 by the turn-based mod at 14:59 and 15:03: Shield Pummel and ability 617 asked for their target, and the click used them |
| The world's geometry: every shape along a ray, the nearest between two points, line of sight as the scripts judge it, the current area (`dao/World.h`) | 2026-09-30, from the cursor pick, World::Raycast, the scripts' CheckLineOfSight commands and the physics scene's slots | checked 2026-09-30, the second play test (`examples/world`): line of sight named four creatures seen and one, behind a closed door, not, blocked at the door; a ray down met the creature's body (groups `0x10`, with its id) then the ground (`0x82002020`, walkable); `CurrentPath` read a walked path of 5 waypoints, 10.33 m, what was left falling as the creature walked. A new `FindPath`, a search of the library's own that leaves the creature's alone, was checked 2026-10-01 (`examples/world` on a timer, the Elven Ruins and the forest clearing): straight walks to a companion, searched paths of 3.42 m to 118.12 m against 3.21 m to 60.10 m straight, none to creatures in a room never seen or behind shut doors, and the creature walking as before throughout. With no path the door in the way is named (`PathSearch::blocker`): standing at it, by the pathfinder's own answer; from 53 m off, by the footprint test on points ahead of where the search gave up. The first `FindPath` was taken out after that test (`dev/TODO.md`, item 5) |
| The area's rooms and whether each is shown: whether the party has seen it, how much the streaming wants it and whether it is loaded, the room at a point and of an object (`dao::world::Rooms`, `RoomAt`, `RoomOf`) | 2026-09-30, from the area layout's update, the indoor stream policy and CRoom's state | in part, 2026-10-01 (`examples/world`, Castle Cousland): 61 rooms, 53 wanted, 1 wanted now, 7 dropped, the controlled creature's own room wanted and loaded, which corrected a first reading of "wanted now" as "drawn". A room drawn black is one the party has never seen, kept per room (`Room::seen`, `Shown()`), read 2026-10-01 after the play test showed a room behind an opened door black until the hero walked in; outdoors (the Dalish forest clearing, the same day) every room read in view, with terrain-cell numbers and a load that never read loaded, each reading state 6 and load 2, so `Shown()` goes by seen alone; indoors (the Elven Ruins, the same day) the party's room read in view and the others never seen and not shown, though loaded, one behind a shut door going to in view the moment the door opened. What is loaded follows the camera; what is seen follows the party |
| The journal: the quests under way and finished, the active one and the groups, through the data tree (`dao/Journal.h`) | 2026-10-01, from Journal's and JournalEntry's constructors, the tree's path walk and the journal movie's paths | the paths: checked 2026-10-02 by the Eluvian server, one by one in Lyna's save (four quests, the active one, a group); the flags read as text, and a description carries its objective in marks, which the wrapper now handles; the wrapper itself: checked 2026-10-02 in Steam's game (`examples/journal`): the same quests, titles and active one as the journal screen |
| Conversations: the lines spoken and replies offered, as their file has them, and which lines are offered (`dao/Dialogue.h`), over a GFF4 reader (`dao/core/Gff4.h`, `Conversation.h`) | 2026-09-30, from the conversation's functions, dialogue mode's reply list and the game's CONV files | the reader: tests, and 104 of the game's conversations read the same as `tools/gff4.py` reads them; the hooks: checked 2026-09-30 (`examples/dialogue`, Eleanor at Highever), NPC lines and the replies as on the screen; the reply chosen added after |
| Answering a conversation and skipping a cutscene, by the messages the screen and the keys send (`dao::dialogue::Choose`, `Continue`, `InCutscene`, `SkipCutscene`) | 2026-10-01, from the dialogue and cutscene modes' handlers | checked 2026-10-01 through the Eluvian server: `Choose` took a reply, `OnReplyChosen` following, and `Continue` went on through a reply with no text. Then the cutscene calls: `LoadCutscene` with `dao::script::Array::Empty` arrays and the name with `.cut` put the game in cutscene mode, `InCutscene` true, and `SkipCutscene` ended it within half a second. And a pre-rendered video: the scripts' `PlayMovie` with a `.bik`'s name put the game in movie mode, `InMovie` true, and `SkipMovie` ended it within half a second |
| Calling a script command by name: every command's signature as data, the arguments checked and defaulted, the call made as `Invoke` makes one (`dao/ScriptCall.h`) | 2026-09-30, generated from script.ldf's signatures | the table and the argument checks: tests; the call: not yet seen in play |
| The player's orders as a click gives them: attack, use an object, use an ability, move (`dao/Orders.h`) | 2026-09-30, from `QueuePlayerCommand`'s three callers and explore mode's move | not yet seen in play |
| Commands starting and ending, with how they ended (`dao/CommandEvents.h`) | 2026-09-30, from the object's command step, ClearCurrentCommand and the commands system's two thunks | not yet seen in play; `examples/commands` logs them |
| An ability's cooldown on a creature: the seconds left and the seconds it was set to, read and set apart (`dao::abilities::GetCooldown`, `SetCooldown`) | 2026-09-30, from the scripts' SetCooldown and GetRemainingCooldown and the cooldowns' tick | checked 2026-10-01 (`examples/cooldown`): set to 800 s of 1200 s, it read back so, ticked a second a second, and the quickbar slot drew about two thirds |
| Script events delivered at once as well as queued: hooked where an object's script meets its event (`CCObject::RunEventScript`, `dao/Events.h`) | 2026-09-30, after deaths were missed | checked 2026-09-30, 20:07 to 20:14: every death came, 1.5 to 2.5 s after IsDead, once it had played out |
| Drawing in the world: the game's aiming rings and any vfx_base row placed (`dao/Vfx.h`) | 2026-09-30, from explore mode's aiming | checked 2026-09-30 (`examples/preview`): a ring followed the cursor, placed, moved and removed; the out-of-reach row looked the same as the other |

## The first session

2026-09-30, its first eight minutes: a fight, some chests, the world map. The probe's detour on `InitializeCommands` ran five seconds into start-up, on the commands object at `CClientExoApp+0x1E4`, and shadowed all 18 getters. Scripts then called 14 of them about 2,200 times; every answer matched our field, with no failed pop or push. `GetCombatState` alone ran 978 times, `GetCombatantType` 366, `GetPackageAI` 330, `GetPackage` 273. Once, `GetCombatantType` was asked about an id that named no creature, and the engine's handler still answered, as the getters do when their lookup fails (they push a default: `GetTeamId`'s body, `0x0065BCD0`); our lookup found nothing, as it should.

The first matches included values other than 0, among them a package of 0x58, a team of -1, a group of 1, a placeable action of 0xB, a creature treasure category of 8 and a lock level of 1. Zero is the weak case: a wrong offset often reads 0 where the true field is 0 too. Since this session the probe also counts matches on values other than 0, and logs the first.

Every script command ran on one thread, the same one `InitializeCommands` ran on: the script VM's, and so the thread a turn-based mode's script-facing work happens on. It is not the process's first thread, which ran the loader.

## The second session

2026-09-30, a fight. With the counts of matches on values other than 0: `GetCombatState` matched on a value other than 0 in 570 of its 632 calls, `GetPackageAI` in 224 of 292, `GetGroupId` in 33 of 41, so those fields read real, changing values. `GetCombatantType` only ever answered 0 in either session, so it stays the weakest of the 14. Every object looked up, 1,100 of them, had the id asked for at +0x10, its class's type byte at +0x14, and the engine's cast from the game object gave the typed lookup's pointer.

## The third session

2026-09-30, a fight, with the value types shadowed. `GetTag` ran 411 times and every answer came back the same through our pop and push, among them 21-character tags that live on the engine's heap (`bhn200cr_howe_landra1`). `GetName` gave `Dog` and `Lesser Lyrium Potion`. `GetLocation` ran 43 times: positions such as (-110.26, -79.82, 12.02), unit orientations, and an area id that named an area every time. With no spells in the party, the fight still put effects on its creatures: 46 `GetEffectType`, 19 `GetEffectCreator`, 16 `GetEffectAbilityID` and 14 `GetEffectDurationType` calls, each matching our accessor on the probe's own copy of the effect, the creators real creatures and the effect ids counting up (654, 655, 656).

## The fourth session

2026-09-30, a fight, with events and arrays shadowed. `GetCurrentEvent` ran 3,635 times and each answer was a `CScriptEvent` that round-tripped the same; `GetEventType` matched 3,400 times, `GetEventCreator` 1,099 and `GetEventTarget` 15. The types seen, read against the scripts' constants: 63 `EVENT_TYPE_MODULE_GETCHARSTAGE`, 94 `PLAYER_COMMAND_ADDED`, 78 `PLACEABLE_ONCLICK`, 11 `EQUIP`. `GetTeam` answered five times with two to four creatures, all on team 9 as asked. `GetEffects` answered 493 times, every array empty.

## The fifth session

2026-09-30, one fight, with the example plugin beside the probe. Calling commands from C++ held on every call: from inside the probe's shadows, on the scripts' thread and in the middle of a script's own command, `api::GetObjectType` agreed with the type byte 700 times, `api::GetStringLength` with the string's length 528 times, `api::GetPositionFromLocation` with the location 73 times. The example plugin's intercept of `GetCombatState` sat in the table in front of the probe's shadow of the same command, the two chaining, and read the creature and the engine's answer right on all 300 calls, calling `api::GetTag` from inside it (`bhn100cr_mother`, `bhn200cr_howe_landra3`). The frame callback runs on the scripts' own thread (one thread id for both) and runs while the game is paused (2,736 frames running, 26,849 paused), which is what a turn-based mode's clock needs.

The event queue delivered 824 events: script events (queue id 10) by `EVENT_TYPE_*`, most of them `EQUIP` (444), `HEARTBEAT2` (99), `PERCEPTION_APPEAR` (61), `ATTACK_IMPACT` (32), and the scripts' own numbers above 1000. `COMMAND_COMPLETE` came through 14 times, fewer than the fight's commands, so the engine delivers it another way too; the turn-based design holds the AI at `AddCommand` instead (`dev/TURN_BASED.md`).

## The eighth session

2026-09-30, about two minutes of play: a save loaded, the inventory and the character and ability screens opened, a quicksave, the main menu. The UI's calls, the movies the game opened with their scene ids, layers and flags, the talk tables, the data tables' strings and sizes, the message watch, and the commands' fields and player-issued flag all checked out (the table above). Two things did not:

- **The example plugin's one-time checks while exploring did not run.** They waited for 600 frames running in explore mode, and a menu opened first. They now count explore frames in total.
- **`SaveGame::Write` was not called for the quicksave**, though `QuickSave_1\savegame.das` and its `.met` were written. It builds that very name, so the quicksave reaches the file another way, or reaches this function a way the detour does not see. That is to read before a plugin's save data rests on it. `FindSave` was called for the load (character, slot 2, kind 0).

## The ninth session

2026-09-30, about a minute of play, alongside the turn-based mod's session: a save loaded, a move click, then a crash. The example plugin's one-time checks ran this time, once the player had explored for five seconds. Sending a message, the movie list, the resource lookups and the player's portrait checked out (the table above). The next check read a creature's properties and crashed the game (`dev/CRASHES.md`). The example plugin was taken out of the game folder until the fix, so the turn-based mod's testing could go on.

From the turn-based mod's side, in the same sessions (DAOTurnBased's `dev/DAOLIB_NEEDS.md`):

- A plugin's screen is added from the frame callback and its movie found in the override folder.
- An event signalled with `bProcessImmediate` from the frame callback runs the creature's script inside the call, and its `AddCommand` reaches the queue before `SignalEvent` returns.
- The player's orders reach `QueueCommand` at once while the game is paused, on the scripts' thread.

## The tenth session

2026-09-30, about six minutes of play with the turn-based mod: a load, a fight in Highever, a quicksave and a quickload. The creature properties that crashed the ninth session read right, and so did the cursor's pick (the table above). The save flow was seen whole (`dev/SAVES.md`): the quicksave's message, `SaveGame::Write` with the slot it chose, then the screenshot's path. What had been taken for writing the screenshot turned out to be the save screen's path lookup, and is renamed `SaveLoadProxy::ScreenshotPath`. Every click's message reached the example plugin's observer four times, and the fix is to show observers only the receiver with a handler.

The session ended in a crash, not yet explained: an area was loading 60 ms after its loading screen opened, and the engine built a graphics object from a blueprint whose `+0x44`, the part its destructor deletes, was null (`0x0059669E`, in the graphics structures manager; no plugin on the stack). The lead is the turn-based mod's portrait renderers for six enemies, each holding a clone of its creature, destroyed with the old area: a path the engine's own screens avoid by releasing a portrait when they close. `CCCreature::ReleasePortrait` now lets a plugin do the same, and whether that stops it is for the next session.

## What comes next

- **Save and load.** Hooks on both, and somewhere a plugin keeps its own data with a save, as SKSE's co-saves do.
- **The Flash UI, from the game's side** (`dev/GUI.md`). Where the game looks for a movie's file, so a plugin can ship one; publishing values to the GUI's data tree (SmartData) that movies bind to; portraits.
- **Messages.** 9 of the 296 message classes have a C++ struct; the rest, their fields' meanings. A plugin can send one, hook a handler, and watch or take any message by class (`OnMessage`, `InterceptMessage`). Sending by name, as key bindings do (`SelectResponseMessage 0`), goes through a registry of factories (`0x0078D1A0`).
- **Creatures.** Beyond the fields and properties read: abilities, inventory, the party. From the constructors and NWN's namesakes.
- **Talk tables.** A plugin's own talk table beside the game's (`dao::tlk::String` reads the game's).
- **Setters.** `dev/script_backends.csv` names the engine functions the set-commands call; each is a wrapper once its argument types are read from the command body, as `SetCombatState` was.
- **The rest of the command.** Its +8 and the top byte of its word; an event when a command starts and when it ends.
- **The client command table.** `CClientVirtualMachineCommands`, 103 handlers of its own; whether single-player runs anything on it (the probe counts its `AddCommand`).
