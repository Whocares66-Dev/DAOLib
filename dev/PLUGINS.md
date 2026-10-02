# Writing a plugin

A plugin is a 32-bit DLL in the game's `bin_ship\daolib\plugins\` that exports `DAOLib_Load`. The loader (`version.dll`) loads every such DLL when the game starts, before any game code runs, and calls `DAOLib_Load` once. A plugin links `daolib`, the library in `src/dao`, which gives it the engine as C++. `src/plugins/example/Example.cpp` is a whole plugin to copy from; `src/plugins/probe` is a larger one.

Worked examples are in `examples/`, one small plugin per part of the library, each a whole plugin to read and copy from (`examples/README.md` lists them).

## The shape of one

```cpp
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/ScriptHooks.h"
#include "dao/generated/ScriptApi.h"

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("myplugin"); // Documents\BioWare\Dragon Age\daolib\myplugin.log
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
        return false;         // not the loader or the game this was built for

    dao::script::InterceptCommand(dao::script::id::GetCombatState, [](dao::script::CommandCall &call) {
        const int result = call.Proceed();
        spdlog::info("combat state of {:#x}: {}", *call.Object(0), *call.IntAnswer());
        return result;
    });
    return true;
}
```

Declare it with `add_daolib_plugin(name SOURCES ...)` (`cmake/DAOLib.cmake`), as the example is: a DLL named `name.dll`, linked to `DAOLib::daolib`, written to `daolib/plugins` in the build tree. Build with `.\tools\build.ps1`, and `.\tools\deploy.ps1` copies every plugin into the game (with the game closed).

## A plugin in its own repository

A plugin need not live in this tree. Its repository brings DAOLib in the way a Skyrim plugin brings in CommonLibSSE, as a git submodule built with it:

```cmake
add_subdirectory(extern/DAOLib EXCLUDE_FROM_ALL)
add_daolib_plugin(myplugin SOURCES src/Plugin.cpp)
```

Its own `vcpkg.json` lists DAOLib's dependencies (`detours`, `spdlog`), since a manifest build reads only the top-level manifest, and its presets target x86 with the `x86-windows-static-md` triplet as `CMakePresets.json` here does. Inside another repository DAOLib builds the library and declares the loader (`version`, built only when asked for by name), and leaves out its own plugins, tests, and format and lint targets (`DAO_BUILD_PLUGINS` and `DAO_BUILD_TESTS` are on only at the top level). The game still needs the loader beside the exe, from this repository's `deploy.ps1` or the plugin's own.

## When things exist

`DAOLib_Load` runs before the game has made anything: no application, no objects, no scripts. Load is for hooks, not reads (`dao::CClientExoApp::Get()` is null there). Things come into being in this order, and the library says when each is there:

| What | When | How to wait for it |
|---|---|---|
| Detours on engine functions | at load | place them in `DAOLib_Load` |
| The application, `CClientExoApp::Get()` | a moment after load | null until then |
| The script command table | early in start-up | `dao::script::OnCommandsReady(callback)`; `InterceptCommand` waits by itself |
| The game mode controller, `CClientExoApp::GameModes()` | after the command table | null until then |
| Game objects | once a game is loaded | lookups by id answer null until then |
| Each frame | from the first frame | `dao::OnFrame(callback)` |

## Threads

The game runs on several threads. Scripts run on one, the thread that filled the command table (`dao::script::ScriptThreadId()`). Calling a script command (`dao::script::Invoke`, `dao::script::api::*`) belongs on that thread: inside a command intercept, or inside a hook on something the scripts' thread runs. The frame callback runs on that thread too: `dao::FrameThreadId()` equalled `ScriptThreadId()` in play (2026-09-30), so a frame callback may call script commands. A detour on an engine function runs on whichever thread calls it: never assume, and never block in one.

## The API, by what a plugin wants to do

| To | Use | Header |
|---|---|---|
| Call any script command | `dao::script::api::GetTeamId(creature)` and 660 more, with the scripts' names and defaults | `dao/generated/ScriptApi.h` |
| Call one by number, with its result or failure | `dao::script::Invoke<std::int32_t>(id, args...)` | `dao/Script.h` |
| See, change or replace a command when scripts call it: its arguments before it runs (`SetFloat`), its answer after (`SetFloatAnswer`), or all of it | `dao::script::InterceptCommand(id, fn)`, `CommandCall`; `examples/damage/` changes the damage the party takes | `dao/ScriptHooks.h` |
| Add a new command | `dao::script::Set(freeSlot, handler)`, plus a `script.ldf` declaration for the toolset's compiler | `dao/Script.h` |
| Name a script constant | `dao::script::EventType::CommandComplete`, `dao::script::constants::EVENT_TYPE_COMMAND_COMPLETE` | `dao/generated/ScriptConstants.h` |
| Find an object by id | `CClientExoApp::Get()->GetCreatureById(id)` and kin | `dao/ClientExoApp.h` |
| Read an object's fields | `creature->CombatState()`, `->TeamId()`, `->Id()`, `->Type()`, casts `AsCreature()` | `dao/Objects.h` |
| A creature's stats: attributes, health, mana or stamina, level | `creature->Health()`, `MaxHealth()`, `Level()`, `Property(PropertyAttribute::Strength, PropertyValue::Base)`, `SetProperty(...)` | `dao/Objects.h` |
| Read an effect or an event | `dao::CGameEffect`, `dao::CScriptEvent`, `CCObject::CurrentEvent()` | `dao/Structures.h` |
| Read, keep or change a queued command | an object's queue, `creature->CurrentCommand()`, `->CommandAt(i)`, `->CommandQueueSize()`; in an `AddCommand` intercept, `call.Command(1)`; `->Type()`, `->Target()`, `->Position()`, `->Ability()`, every parameter through `->Parameters()`; `dao::script::Copy(*command)` to keep one | `dao/Objects.h`, `dao/Script.h`, `dao/ScriptHooks.h` |
| Use strings, locations, arrays | `dao::WString`, `dao::CScriptLocation`, `dao::script::Array` | `dao/String.h`, `dao/Structures.h`, `dao/Script.h` |
| Read a data table | `dao::gda::GetInt(Table::AbilitiesTalents, ability, "Cooldown")`, `GetFloat`, `GetString`, `GetBool`, `Rows(table)`, `Columns(table)`; columns per table in `dev/gda_tables.csv` | `dao/Gda.h` |
| The game's text by string reference, in the player's language | `dao::tlk::String(strref)`, `String(strref, true)` for the female form | `dao/Tlk.h` |
| Pause, or switch the game's mode | `CClientExoApp::SetPaused`, `GameModeController::SetMode` | `dao/ClientExoApp.h`, `dao/GameModes.h` |
| Hook an engine message (the player's orders, pause, mode changes) | `dao::messages::handler::GameModeExplore_PerformAbilityMessage` with `TypedHandlerFn`, `As<M>()` | `dao/Messages.h`, `dao/generated/MessageHandlers.h` |
| Send an engine message, as the engine does (move the camera, pause, switch mode) | `dao::messages::Send(modes->CurrentModeObject(), message)`, `Make<CenterOnObjectMessage>()` | `dao/Messages.h`, `dao/GameModes.h` |
| Hold the game's clock for effects and regeneration, and spend a round's worth at once (a fight in turns): `dao::clock::Hold(true)`, `Advance(seconds)`; run the world faster or slower: `dao::clock::SetSpeed(0.5f)` | `dao/Clock.h` |
| Show a number as a person reads it: a script constant's name (`Name(EffectType::Root)`, every generated enum; `Name(core::CommandType)`), an ability's name and label (`dao::abilities::Name(id)`) | `dao/generated/ScriptConstants.h`, `dao/Abilities.h` |
| Draw in the world with the game's own effects: a ring on the ground of any radius, the engine's aiming ring (`dao::vfx::PlaceRing`), any vfx_base row placed, scaled, moved and removed; a placed effect's size changed where it stands (`SetScale`, not yet seen in play); a tint, at placing or after (`SetTint`), has shown no colour in play on the reticles | `dao/Vfx.h` |
| Who is selected in the party bar: `dao::party::Selected()` | `dao/Party.h` |
| A plugin's own text in the player's language: its own talk tables beside its DLL (`<name>_<locale>.tlk`), the game's language (`dao::strings::Locale`) | `dao/Strings.h` |
| Hear any object start and end a command, with how it ended (success, cleared, a COMMAND_FAILED_*): `dao::commands::OnStart`, `OnEnd` | `dao/CommandEvents.h` |
| Act through explore mode's own click state: select a target as the cycle-target keys do (`GameModeExplore::SelectTarget`, `SelectedTarget`), set the object under the cursor for the next click message (`SetCursorObject`) | `dao/GameModes.h` |
| Give the player's orders as a click gives them (player issued, the creature told its order arrived): `dao::orders::Attack`, `UseObject`, `UseAbility`, `MoveTo` | `dao/Orders.h` |
| Call a script command by its name with values typed at run time, for a console or a remote tool: `dao::script::Call("GetTeamId", {Value::Object(id)})`; every command's parameters, defaults and answer as data (`dao::script::table`, generated) | `dao/ScriptCall.h` |
| Follow a conversation (each NPC line, the replies offered) and decide which lines are offered: `dao::dialogue::OnNpcLine`, `OnReplies`, `OnLineAvailable`; read any line of the one on the screen (`Current()->LineAt`), and conversation files themselves (`dao/core/Conversation.h`, over the GFF4 reader `dao/core/Gff4.h`) | `dao/Dialogue.h` |
| Ask the world's geometry: every shape along a ray (`dao::world::Raycast`), the nearest between two points (`FirstHit`), whether one creature or point can be seen from another as the scripts judge it (`LineOfSight`), the area the player is in (`CurrentArea`), the path a creature is walking and its length (`CurrentPath`); on the game's thread | `dao/World.h` |
| Stop effects wearing off, or spend time on them at once (turns, slow motion); an effect's duration and time left | `dao::effects::Hold(true)`, `Advance(seconds)`, `OnAgeing(effect, ms)`; `effect.Duration()`, `Remaining()`, `SetRemaining(s)` | `dao/Effects.h`, `dao/Structures.h` |
| Keep data with the game's saves, as SKSE's co-saves do | `dao::saves::OnGameLoaded` (a save loaded, or a new game), `OnSaved`; `save.Read("myplugin")`, `save.Write("myplugin", data)`, the plugin's own file in the save's folder; `examples/saves/` | `dao/Saves.h` |
| Move the camera smoothly, the player's own turning taken into account: glide to a creature, come round behind it | `dao::camera::PanTo(id, seconds)`, `EaseYaw(*dao::camera::YawBehind(id), seconds)`; a move stops if the player takes the camera; `dao::camera::Current()`, `OnTargetChanged(now, before)` for a new selection; `examples/camera/` | `dao/CameraMoves.h` |
| Move the camera: follow any creature, an enemy too, with the game's own pan (within 15 m) and without selecting it; where it looks; snap | `modes->Camera()->Follow(id)`, `Target()`, `Focus()`, `SnapToTarget()`; `Tactical()`, `FocusOffset()`/`SetFocusOffset()` to pan in the tactical view; `Scene()->Yaw()`/`SetYaw()`, `Pitch()`, `Zoom()`; `TurnBehind(id)`, the engine's own turn to a creature's heading | `dao/Camera.h`, `dao/GameModes.h` |
| Have a key of the plugin's own, registered with the game's and kept in its `KeyBindings.ini` | `dao::input::RegisterAction({.name, .label, .key}, fn)` from `DAOLib_Load`; `examples/input/`. Seen in play: the key, its row in the controls screen (under "Mods", or the action's own `section`), rebinding there and the restart after | `dao/Input.h` |
| Know whether the mouse is over the interface, where the game's pick is nothing | `dao::gui::MouseOverInterface()`: the engine's own test. Seen in play in both builds | `dao/Gui.h` |
| Know where something is on the screen, or what is under a pixel | `dao::world::ToScreen(point)`, `PickAt(x, y)`, `AimAt(object)`, `ScreenSize()`; on the game's thread. Seen in play | `dao/World.h` |
| Put settings of the plugin's own in the game's options screen | `dao::options::AddSection(L"Turn-based", {toggle})` from `DAOLib_Load`: a section on the Game tab, each checkbox with a `get` and a `set` that may refuse; `examples/options/`. Seen in play | `dao/Options.h` |
| Add to one of the game's own screens where its contents are written in the movie's script | `dao::gui::OnMovieLoad(L"optionsmenu", patch)` with `dao::core::gfx::InsertAfter`; `dev/GUI.md`. Seen in play through the controls screen's rows | `dao/Gui.h`, `dao/core/GfxMovie.h` |
| React to a key the player has bound, through the game's own bindings: watch the message its action sends (the pause key sends `UserTogglePauseMessage`; `dev/TODO.md`, item 3, has how actions map to messages) | `dao::messages::OnMessage<M>`, `InterceptMessage<M>` | `dao/Messages.h` |
| Ask the player to pick an ability's target with the targeting cursor, as the quickbar does | `modes->Explore()->RequestTarget(ability, caster)` | `dao/GameModes.h` |
| What is under the mouse cursor: the object a click would act on, the ground point and whether it is walkable, every hit along the ray | `modes->Explore()->CursorObject()`, `CursorGround()`, `CursorGroundWalkable()`, `CursorHits()`, `LastMoveDestination()` | `dao/GameModes.h` |
| Watch or take any engine message by class, as it is delivered | `dao::messages::OnMessage<SetGameModeMessage>(fn)`, `InterceptMessage<PerformAbilityMessage>(fn)`, `OnAnyMessage(fn)` | `dao/Messages.h` |
| Answer a Flash movie's `ExternalInterface.call` or `fscommand` by name, or watch the game's | `dao::gui::OnExternalInterface("Name", fn)` with `call.Return(Value::Number(n))`, `OnFSCommand`, `OnAnyFSCommand`, `OnAnyExternalInterface` | `dao/Gui.h` |
| Call into a movie: set a variable or an array, call an ActionScript function | `dao::gui::Movie{dao::gui::MovieOf(owner)}.SetVariable(path, value)`, `SetArray`, `Invoke("_root.Fn", args)`; `ReturnFSCommand(owner, ints)` | `dao/Gui.h` |
| Find one of the game's movies by name, or list them | `dao::gui::FindOwner(L"name")`, `Owners()`, `NameOf(owner)`, `MovieOf(owner)` | `dao/Gui.h` |
| Show a movie of the plugin's own, layered like the game's screens | `dao::gui::AddScreen(L"Name", L"movie", dao::gui::kLayerHud)`, `ShowScreen`, `HideScreen`, `CloseScreen` | `dao/Gui.h` |
| A creature's portrait for a movie (enemies too) | `creature->RenderPortrait()` gives `img://portrait_<tag><counter>`; `PortraitImage()` without rendering; `ReleasePortrait()` when done, as the game's screens do | `dao/Objects.h` |
| Read or set a value in the engine's data tree, as the UI binds to it | `dao::data::Text(L"GUI.SelectedChar")`, `Number(path)`, `Find(path)->Kind()`, `->Set(value)` | `dao/Data.h` |
| React to the game's script events (damage, perception, mode changes, loads) | `dao::events::OnScriptEvent(EventType::Damaged, fn)` | `dao/Events.h` |
| Run code every frame | `dao::OnFrame(fn)` | `dao/Frame.h` |
| Hook any engine function | `dao::Detour<Fn>` on an ID from `dao/Addresses.h` | `dao/Hook.h` |
| Know what an object is | `dao::TypeName(ptr)`, from the RTTI | `dao/TypeName.h` |
| Read memory that might not be there | `dao::TryRead<T>(address)` | `dao/SafeRead.h` |

## How much to trust each part

Every address and layout was read from the one supported exe, with a note in `dao/Addresses.h` or the class's header saying how and when. `dev/LIBRARY.md` keeps a table of what has been checked in play and what has only been read. As of 2026-09-30: object lookups, 14 fields, the object layout and casts, strings, locations, effects, events and object arrays have all matched the engine's own answers on thousands of calls. Calling commands from C++, intercepting them, the frame callback, the event queue, the game mode controller's table, the data tables and the messages' layouts are built but not yet seen in play. A plugin that relies on those should expect to be the one that finds out.

**Other builds.** The exe everything was read from is GOG's. In the EA app's build the same functions and globals are elsewhere, so the library holds each as an ID, a number that means the same thing in every build, and looks it up in the running build as it is used (`dao::Module::Address`, `dao::At`, `dao::Detour`). A plugin does nothing for it: one DLL serves every build the address library has a file for, a build it was never built against included (`dev/BUILDS.md`). A plugin's own findings are IDs too: read the address in GOG's listing and `python tools\address_ids.py 0x005D6740` gives the ID to write, as `dao::AddressId{10547}` or a `DAO_ADDRESS` row of the plugin's own. A place inside a function, such as the return address a hook tells its callers apart by, is a number of one build only: ask which function it is in instead, `dao::Module::Containing(returnAddress)`, which answers the ID and the offset past it, and compare the ID (`python toolsddress_ids.py 0x0066E1B4` says `ID 13455 + 0xd4` for an address read in GOG's listing). Three things an ID cannot do, each rare. A field offset holds in both builds except in the few classes EA changed (`ContentManager`, `GUI::ScaleformManager` from +0x190, the `AnimBase` family from +0x224). A function EA's build lacks maps to nothing, and `Detour::Attach` on it returns false, as it does for any refusal. And a detour on a small function whose code is shared with others (a getter, a destructor) may catch a different set of callers in each build, since each build's linker folded identical functions its own way.

## Exceptions

A plugin's code runs on the game's own stack, and an exception has no safe way back through the game's code: it ends the process, or unwinds the game half-way through a change. The library catches around every callback it calls (frame, script events, messages, the Flash UI, saves, command intercepts), logs what was thrown and where, and lets the game go on as it would have without that callback: a message interceptor that throws has not taken the message, and a command intercept that throws leaves the game's own command to answer. A plugin's own Detours hooks are its own to guard the same way, since the library never sees those calls:

```cpp
void __fastcall SetPaused(dao::CClientExoApp *self, int edx, bool paused, bool second)
{
    try { MyPauseLogic(paused); } catch (...) { spdlog::error("pause hook threw"); }
    g_setPaused.Original()(self, edx, paused, second);
}
```

Prefer calls that cannot throw where a failure is expected: `std::from_chars` over `std::stoi` for data read from a file.

## Hooking by hand

An engine member function is `__thiscall`: `this` in ecx, arguments on the stack, the callee cleans up. A hook on one is written `__fastcall` with `this` first and an unused `int` (edx) second, and `dao::Detour<Fn>`'s `Fn` the same way:

```cpp
using SetPausedFn = void(__fastcall *)(dao::CClientExoApp *self, int edx, bool paused, bool second);
dao::Detour<SetPausedFn> g_setPaused;

void __fastcall SetPaused(dao::CClientExoApp *self, int edx, bool paused, bool second)
{
    g_setPaused.Original()(self, edx, paused, second);
}

g_setPaused.Attach(dao::addr::kCClientExoApp_SetPaused, &SetPaused); // in DAOLib_Load
```

A wrong signature leaves the stack off by the arguments' size at the return, so the crash comes after the hook, not in it; `ret N` at the function's end says N bytes of arguments. The crash logger in the loader writes `crash-<time>.log` beside the logs with the stack and the names it knows.
