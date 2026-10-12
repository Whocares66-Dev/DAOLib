# Writing a plugin

A DAOLib plugin is a 32-bit DLL that the loader loads when Dragon Age: Origins starts. It exports one function, `DAOLib_Load`, and works with the game from C++ through the library in `src/dao`, which is a static library compiled into the plugin, not a DLL of its own.

To start, copy `src/plugins/example/Example.cpp`, or one of the small plugins in `examples/` (one per part of the library; `examples/README.md` lists them).

## A minimal plugin

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

This plugin opens its own log, refuses to load into a loader or game it wasn't built for, and logs the answer every time a script asks for a creature's combat state.

Declare it in CMake with `add_daolib_plugin(myplugin SOURCES Plugin.cpp)` (`cmake/DAOLib.cmake`). Build with `.\tools\build.ps1`, and copy every plugin into the game with `.\tools\deploy.ps1` while the game is closed.

**Logging.** Each start writes a fresh `<name>.log` and keeps the previous session's as `<name>.previous.log`, so a crash's log survives the next start. Writing a log line doesn't slow the game: lines are buffered and written by the log's own thread every quarter of a second, and at once for a warning or an error. Formatting a line still costs a little, so choose its level by who it is for:

- `error`: something failed.
- `warn`: something a player or a developer should look at.
- `info`: what the session did, a line or a few per thing the player did, never per frame.
- `debug`: tracing for an investigation, off unless `DAOLIB_LOG_LEVEL=debug`. Remove it, or leave it at `debug`, once the investigation is over.

## Where a plugin's files go

Each plugin is one folder, so it is installed and removed as one:

```
bin_ship\daolib\plugins\<name>\<name>.dll   the plugin
bin_ship\daolib\plugins\<name>\...          its own files: movies, talk tables, data
bin_ship\daolib\plugins\<name>\override\    files that replace or add to the game's own
```

- The loader (`version.dll`) loads the DLL in every plugin folder, in order of the folders' names, before any game code runs, and calls `DAOLib_Load` once. A DLL loose in `daolib\plugins\` is not loaded; the loader's log says where it should go.
- `add_daolib_plugin(... FILES ...)` copies the plugin's own files beside its DLL on every build. A movie there is served to the game when the game has none by that name (`dao::gui::AddScreen` turns this on), and talk tables there are read by `dao/Strings.h`.
- `add_daolib_plugin(... OVERRIDE ...)` copies files into the `override\` folder. The game finds any file there or in its subfolders by name (a texture, a material, a model, a table, an `.erf`), just as it does in its own `packages\core\override`, and merges an `M2DA_` table there with its own at start.
- The player's own override folder wins over a plugin's file of the same name, and a plugin's file wins over the game's archives and patches. The folder is read once at start, not watched. Only the game's kinds of file go there, not the DLL.
- A file a plugin keeps beside the logs (`dao::LogDirectory()`) under a name of its own is named as the logs are: `dao::OwnsLogs()` says whether this game has the plain names there, and a second game in the folder, which is told false, puts its process id in the name as `OpenLog` does. Decide by that and not by a port or a file being there: every DLL of a game gets the one answer for the game's whole life.
- Files that go with a setting do not go in `override\`, which is always read. Keep them in a folder of their own beside it (`FILES` copies a whole folder too) and add it with `dao::resources::AddFolder` when the setting is on, from `DAOLib_Load` or later; `RemoveFolder` takes it away. The game finds its files there as in `override\`. A file the game has loaded already stays as it is until the game loads it again.

## A plugin in its own repository

A plugin doesn't need to live in this repository. It can bring DAOLib in as a git submodule, the way a Skyrim plugin brings in CommonLibSSE:

```cmake
add_subdirectory(extern/DAOLib EXCLUDE_FROM_ALL)
add_daolib_plugin(myplugin SOURCES src/Plugin.cpp)
```

- The plugin's `vcpkg.json` must list DAOLib's dependencies (`detours`, `spdlog`), since a manifest build reads only the top-level manifest.
- Its presets target x86 with the `x86-windows-static-md` triplet, as this repository's `CMakePresets.json` does.
- Inside another repository, DAOLib builds the library and declares the loader (`version`, built only when asked for by name). It leaves out its own plugins, tests, and format and lint targets.
- The game still needs the loader beside its exe, from this repository's `deploy.ps1` or the plugin's own.

## Startup order

`DAOLib_Load` runs before the game has created anything: no application, no objects, no scripts. Use it to place hooks, not to read the game (`dao::CClientExoApp::Get()` is null there). Things become available in this order:

| What | When | How to wait for it |
|---|---|---|
| Detours on engine functions | at load | place them in `DAOLib_Load` |
| The application, `CClientExoApp::Get()` | a moment after load | null until then |
| The script command table | early in start-up | `dao::script::OnCommandsReady(callback)`; `InterceptCommand` waits by itself |
| The game mode controller, `CClientExoApp::GameModes()` | after the command table | null until then |
| Game objects | once a game is loaded | lookups by id return null until then |
| Each frame | from the first frame | `dao::OnFrame(callback)` |

## Threads

The game runs on several threads. Scripts run on one of them (`dao::script::ScriptThreadId()`), and script commands (`dao::script::Invoke`, `dao::script::api::*`) must be called from it: inside a command intercept, or inside a hook on something that thread runs. The frame callback runs on the same thread, so it may call script commands too.

A detour on an engine function runs on whichever thread called the function. Never assume which, and never block in one.

## What to use for what

### Scripts

| To | Use | Header |
|---|---|---|
| Call any script command | `dao::script::api::GetTeamId(creature)` and 660 more, with the scripts' names and defaults | `dao/generated/ScriptApi.h` |
| Call a command by number | `dao::script::Invoke<std::int32_t>(id, args...)` | `dao/Script.h` |
| Call a command by name, with values typed at run time (a console, a remote tool) | `dao::script::Call("GetTeamId", {Value::Object(id)})`; every command's signature as data in `dao::script::table` | `dao/ScriptCall.h` |
| See, change or replace a command when scripts call it | `dao::script::InterceptCommand(id, fn)` with `CommandCall` (`SetFloat` before it runs, `SetFloatAnswer` after); `examples/damage/` | `dao/ScriptHooks.h` |
| Add a new command | `dao::script::Set(freeSlot, handler)`, plus a `script.ldf` declaration for the toolset's compiler | `dao/Script.h` |
| Name a script constant | `dao::script::EventType::CommandComplete`, `dao::script::constants::EVENT_TYPE_COMMAND_COMPLETE` | `dao/generated/ScriptConstants.h` |
| Show a constant or an ability as a person reads it | `Name(EffectType::Root)` for any generated enum, `Name(core::CommandType)`; `dao::abilities::Name(id)` | `dao/generated/ScriptConstants.h`, `dao/Abilities.h` |
| React to script events (damage, perception, mode changes, loads) | `dao::events::OnScriptEvent(EventType::Damaged, fn)` | `dao/Events.h` |
| Log every delayed script event as it is queued and delivered | `dao::timers::Trace(true)` | `dao/Timers.h` |

### Objects and data

| To | Use | Header |
|---|---|---|
| Find an object by id | `CClientExoApp::Get()->GetCreatureById(id)` and its kin | `dao/ClientExoApp.h` |
| Read an object's fields | `creature->CombatState()`, `TeamId()`, `Id()`, `Type()`; casts like `AsCreature()` | `dao/Objects.h` |
| Read or set a creature's stats | `Health()`, `MaxHealth()`, `Level()`, `Property(PropertyAttribute::Strength, PropertyValue::Base)`, `SetProperty(...)` | `dao/Objects.h` |
| Read an effect or an event | `dao::CGameEffect`, `dao::CScriptEvent`, `CCObject::CurrentEvent()` | `dao/Structures.h` |
| Use the scripts' strings, locations and arrays | `dao::WString`, `dao::CScriptLocation`, `dao::script::Array` | `dao/String.h`, `dao/Structures.h`, `dao/Script.h` |
| Read a data table | `dao::gda::GetInt(Table::AbilitiesTalents, ability, "Cooldown")`, `GetFloat`, `GetString`, `GetBool`, `Rows`, `Columns`; the columns of each table are in `dev/gda_tables.csv` | `dao/Gda.h` |
| Get the game's text by string reference, in the player's language | `dao::tlk::String(strref)`; `String(strref, true)` for the female form | `dao/Tlk.h` |
| Resolve a line's tokens as the game shows it (`<FirstName/>`, a key as bound) | `dao::tlk::Resolve(text)` | `dao/Tlk.h` |
| Use a plugin's own text, in the player's language | talk tables beside the DLL (`<name>_<locale>.tlk`); `dao::strings::Locale` | `dao/Strings.h` |
| Read a file as the game finds it, a mod's version over the game's | `dao::resources::Read(L"rules_core.ncs")`, `Exists(name)` | `dao/Resources.h` |
| Read or set a value in the engine's data tree, which the interface binds to | `dao::data::Text(L"GUI.SelectedChar")`, `Number(path)`, `Find(path)->Kind()`, `->Set(value)` | `dao/Data.h` |
| Read a list in the data tree, as a screen lists it | `dao::data::MemberCount(path)`, `MemberNames(path)`, `Members(path)` | `dao/Data.h` |
| Read the journal | `dao::journal::Current`, `Completed`, `Active`, `Groups` | `dao/Journal.h` |

### Time, effects and the party

| To | Use | Header |
|---|---|---|
| Pause, or switch the game's mode | `CClientExoApp::SetPaused`, `GameModeController::SetMode` | `dao/ClientExoApp.h`, `dao/GameModes.h` |
| Hold the clock for effects, regeneration and their timers, and spend time at once (a fight in turns) | `dao::clock::Hold(true)`, `Advance(seconds)`; per creature: `OwnClock(id)`, `AdvanceObject(id, seconds)` | `dao/Clock.h` |
| Run the world faster or slower | `dao::clock::SetSpeed(0.5f)` | `dao/Clock.h` |
| Stop effects wearing off, or age them at once | `dao::effects::Hold(true)`, `Advance(seconds)`, `OnAgeing(fn)`; an effect's `Duration()`, `Remaining()`, `SetRemaining(s)` | `dao/Effects.h`, `dao/Structures.h` |
| List a creature's effects with the time each has left | `dao::effects::On(id)` | `dao/Effects.h` |
| Hold the scripts' delayed events and deliver them on a clock of your own | `dao::timers::Hold`, `Advance`, `OwnClock(id)`, `AdvanceObject(id, seconds)`, `HoldKind(type)`, `HoldFrom(seconds)`, `Waiting()`, `WaitingFor(id)` | `dao/Timers.h` |
| Know who is selected in the party bar | `dao::party::Selected()` | `dao/Party.h` |
| Read or change the area's sky (sun, air, clouds, fog, moon) and its sunlight (the light on the ground and creatures, and its direction) | `dao::atmosphere::CurrentSky()`, `SetSky(sky)`, `ResetSky()`, `CurrentSunlight()`, `SetSunlight(light)`, `ResetSunlight()`, `dao::core::Blend` | `dao/Atmosphere.h` |
| Read or change what the game does to the whole picture: the colour grade, the bloom | `dao::framebuffer::Effects()`, `Enabled(effect)`, `Value(effect, name)`, `SetEnabled`, `SetValue` | `dao/Framebuffer.h` |

### Commands, combat and abilities

| To | Use | Header |
|---|---|---|
| Read, keep or change a queued command | `creature->CurrentCommand()`, `CommandAt(i)`, `CommandQueueSize()`; in an `AddCommand` intercept, `call.Command(1)`; `dao::script::Copy(*command)` to keep one | `dao/Objects.h`, `dao/Script.h`, `dao/ScriptHooks.h` |
| Hear an object start and finish a command, and how it ended | `dao::commands::OnStart`, `OnEnd` | `dao/CommandEvents.h` |
| Give orders as the player's clicks do | `dao::orders::Attack`, `UseObject`, `UseAbility`, `MoveTo` | `dao/Orders.h` |
| Know how long a command still holds a creature | `dao::commands::SubActions`, `HeldFor` | `dao/SubActions.h` |
| Know whether an attacker, or an ability's user, is in reach, where its approach walks, and how it turns to face | `dao::attack::IsRanged`, `ApproachOf`, `Walk` (each with or without an ability), `TurnTo`, `FaceExactly`; `dao::commands::RunningSubAction` | `dao/Attack.h`, `dao/Conjure.h` |
| Hold a spell in its conjure and feed it time | `dao::commands::HoldConjure(caster)`, `FeedConjure(caster, seconds)`, `Conjure(caster)` | `dao/Conjure.h` |
| Know how long an animation, attack, shot or ability takes | `dao::animation::Length`, `StepLength`, `AttackDuration`, `AimDuration`, `OfShot`, `OfAbility` | `dao/Animation.h` |
| Know whether a creature can use an ability now, and what it costs | `dao::abilities::CanUse(creature, ability)`, `Cost(creature, ability)` | `dao/Abilities.h` |
| Follow a conversation, and choose which replies are offered | `dao::dialogue::OnNpcLine`, `OnReplies`, `OnLineAvailable`, `Current()->LineAt(i)`, `Speaker(line)`, `Listener(line)`, `Owner()` | `dao/Dialogue.h` |

### The world and the camera

| To | Use | Header |
|---|---|---|
| Ask the world's geometry | `dao::world::Raycast`, `FirstHit`, `LineOfSight`, `CurrentArea`, `CurrentPath` | `dao/World.h` |
| Know where something is on screen, or what is under a pixel | `dao::world::ToScreen(point)`, `PickAt(x, y)`, `AimAt(object)`, `ScreenSize()` | `dao/World.h` |
| Know what is under the mouse cursor | `modes->Explore()->CursorObject()`, `CursorGround()`, `CursorGroundWalkable()`, `CursorHits()` | `dao/GameModes.h` |
| Act through explore mode's click state | `GameModeExplore::SelectTarget`, `SelectedTarget`, `SetCursorObject` | `dao/GameModes.h` |
| Ask the player to pick an ability's target | `modes->Explore()->RequestTarget(ability, caster)` | `dao/GameModes.h` |
| Draw in the world with the game's own effects | `dao::vfx::PlaceRing`, any `vfx_base` row placed, scaled, moved and removed | `dao/Vfx.h` |
| Move the camera smoothly | `dao::camera::PanTo(id, seconds)`, `EaseYaw`, `YawBehind(id)`, `EaseZoom(zoom, seconds)`; `examples/camera/` | `dao/CameraMoves.h` |
| Point the camera directly | `modes->Camera()->Follow(id)`, `Target()`, `Focus()`, `SnapToTarget()`, `Scene()->SetYaw()`, `TurnBehind(id)` | `dao/Camera.h`, `dao/GameModes.h` |
| Frame a creature's portrait with a camera of your own | `dao::portraits::SetCamera(id, {.distance = 1.6f})`, then `RenderPortrait()` | `dao/Portraits.h`, `dao/Objects.h` |
| Get a creature's portrait for a movie | `creature->RenderPortrait()`, `PortraitImage()`, `ReleasePortrait()` | `dao/Objects.h` |

### The interface

| To | Use | Header |
|---|---|---|
| Show a movie of your own, layered with the game's screens | `dao::gui::AddScreen(L"Name", L"movie", dao::gui::kLayerHud)`, `ShowScreen`, `HideScreen`, `CloseScreen` | `dao/Gui.h` |
| Know which of the game's screens are up | `dao::gui::ShownScreens()`, `Screens()`, `CurrentFullScreen()` | `dao/Gui.h` |
| Answer a movie's `ExternalInterface.call` or `fscommand`, or watch the game's | `dao::gui::OnExternalInterface("Name", fn)`, `OnFSCommand`, `OnAnyFSCommand`, `OnAnyExternalInterface` | `dao/Gui.h` |
| Call into a movie: set a variable or an array, call a function | `Movie.SetVariable(path, value)`, `SetArray`, `Invoke("_root.Fn", args)`, `GetNumber(path)` | `dao/Gui.h` |
| Find one of the game's movies | `dao::gui::FindOwner(L"name")`, `Owners()`, `NameOf(owner)`, `MovieOf(owner)`, `ScreenMovie(L"Options")` | `dao/Gui.h` |
| Add to a game screen with its own art | `dao::gui::LoadInto(movie, clip, L"mymovie")` | `dao/Gui.h` |
| Change a game screen whose contents are in its script | `dao::gui::OnMovieLoad(L"optionsmenu", patch)` with `dao::core::gfx::InsertAfter`; `dev/GUI.md` | `dao/Gui.h`, `dao/core/GfxMovie.h` |
| Replace one of the game's movies while the plugin is loaded | `dao::gui::ReplaceMovie` | `dao/Gui.h` |
| Know whether the mouse is over the interface | `dao::gui::MouseOverInterface()` | `dao/Gui.h` |

### Input, settings and saves

| To | Use | Header |
|---|---|---|
| Add a key of your own, rebindable in the controls screen | `dao::input::RegisterAction({.name, .label, .key}, fn)` from `DAOLib_Load`; `examples/input/` | `dao/Input.h` |
| React to a key the player has bound | watch the message its action sends with `dao::messages::OnMessage<M>`; `dev/TODO.md`, item 3, maps actions to messages | `dao/Messages.h` |
| Point the game's world pick at a pixel without moving the mouse | `dao::input::PointCursor(x, y)`, `ReleaseCursor()` | `dao/Input.h` |
| Add settings to the game's options screen | `dao::options::AddSection(L"My plugin", {toggles}, {choices})` from `DAOLib_Load`; `examples/options/` | `dao/Options.h` |
| Keep data with the game's saves, as SKSE's co-saves do | `dao::saves::OnGameLoaded`, `OnSaved`, `save.Read("myplugin")`, `save.Write("myplugin", data)`; `examples/saves/` | `dao/Saves.h` |
| Know why the game won't save (its slot limits) | `dao::saves::MostSlot(kind)` | `dao/Saves.h` |

### The engine itself

| To | Use | Header |
|---|---|---|
| Run code every frame | `dao::OnFrame(fn)` | `dao/Frame.h` |
| Watch or take any engine message | `dao::messages::OnMessage<SetGameModeMessage>(fn)`, `InterceptMessage<PerformAbilityMessage>(fn)`, `OnAnyMessage(fn)` | `dao/Messages.h` |
| Hook an engine message handler | `dao::messages::handler::GameModeExplore_PerformAbilityMessage` with `TypedHandlerFn` | `dao/Messages.h`, `dao/generated/MessageHandlers.h` |
| Send an engine message | `dao::messages::Send(modes->CurrentModeObject(), message)`, `Make<CenterOnObjectMessage>()` | `dao/Messages.h` |
| Hook any engine function | `dao::Detour<Fn>` on an ID from `dao/Addresses.h` | `dao/Hook.h` |
| Know what an object is | `dao::TypeName(ptr)`, from its RTTI | `dao/TypeName.h` |
| Read memory that might not be there | `dao::TryRead<T>(address)` | `dao/SafeRead.h` |

## Game versions

DAOLib was read from the GOG build of the game. Steam's build is the same code at the same addresses. EA's build has the same functions at different addresses, so the library refers to each by an ID and looks it up in whichever build is running. A plugin needs to do nothing for this: one DLL works in every build the address library knows (`dev/BUILDS.md`).

- **Your own addresses:** read the address in GOG's listing, and `python tools\address_ids.py 0x005D6740` gives its ID. Write it as `dao::AddressId{10547}` or as a `DAO_ADDRESS` row of your own.
- **A place inside a function** (a return address, say) differs between builds. Ask which function it is in with `dao::Module::Containing(address)`, which gives the ID and the offset, and compare the ID.
- **Field offsets** are the same in every build, except in a few classes EA changed (`ContentManager`, `GUI::ScaleformManager` from +0x190, the `AnimBase` family from +0x224).
- **A function EA's build lacks** has no address there, and `Detour::Attach` on it returns false.
- **A detour on a tiny function** whose code is shared with others (a getter, a destructor) may catch different callers in each build, because each build's linker merged identical functions its own way.

## Exceptions

An exception can't safely pass back through the game's code: it ends the process, or leaves the game half-way through a change. So the library catches exceptions around every callback it calls (frames, script events, messages, the interface, saves, command intercepts), logs them, and carries on as if the callback hadn't been there. A message interceptor that throws hasn't taken the message; a command intercept that throws lets the game's own command answer.

Your own Detours hooks are yours to guard, since the library never sees those calls:

```cpp
void __fastcall SetPaused(dao::CClientExoApp *self, int edx, bool paused, bool second)
{
    try { MyPauseLogic(paused); } catch (...) { spdlog::error("pause hook threw"); }
    g_setPaused.Original()(self, edx, paused, second);
}
```

Where failure is expected, prefer calls that can't throw: `std::from_chars` over `std::stoi` for data read from a file.

## Hooking by hand

An engine member function is `__thiscall`: `this` in ecx, the arguments on the stack, and the callee cleans up. Write a hook on one as `__fastcall`, with `this` first and an unused `int` (edx) second, and declare `dao::Detour<Fn>`'s `Fn` the same way:

```cpp
using SetPausedFn = void(__fastcall *)(dao::CClientExoApp *self, int edx, bool paused, bool second);
dao::Detour<SetPausedFn> g_setPaused;

void __fastcall SetPaused(dao::CClientExoApp *self, int edx, bool paused, bool second)
{
    g_setPaused.Original()(self, edx, paused, second);
}

g_setPaused.Attach(dao::addr::kCClientExoApp_SetPaused, &SetPaused); // in DAOLib_Load
```

A wrong signature leaves the stack off by the arguments' size when the function returns, so the crash comes after the hook, not in it. A `ret N` at the end of the function means it takes N bytes of arguments. If the game does crash, the loader's crash logger writes `crash-<time>.log` beside the logs, with the stack and the names it knows.

## Shipping a plugin

Players install DAOLib from its release zip, extracted into the game's folder as SKSE is: `bin_ship\version.dll` and `bin_ship\daolib\addresses\`. Ship a plugin the same way, as a zip holding its folder, `bin_ship\daolib\plugins\<name>\`. A `.dazip` can't hold it, because DAUpdater unpacks into the Documents folder, never the game's.
