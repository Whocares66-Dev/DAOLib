# Writing a plugin

A plugin is a 32-bit DLL in the game's `bin_ship\daolib\plugins\` that exports `DAOLib_Load`. The loader (`version.dll`) loads every such DLL when the game starts, before any game code runs, and calls `DAOLib_Load` once. A plugin links `daolib`, the library in `src/dao`, which gives it the engine as C++. `src/plugins/example/Example.cpp` is a whole plugin to copy from; `src/plugins/probe` is a larger one.

## The shape of one

```cpp
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/ScriptHooks.h"
#include "dao/generated/ScriptApi.h"

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("myplugin"); // Documents\BioWare\Dragon Age\daolib\myplugin.log
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->expectedExecutable)
        return false;         // not the loader or the game this was built for

    dao::script::InterceptCommand(dao::script::id::GetCombatState, [](dao::script::CommandCall &call) {
        const int result = call.Proceed();
        spdlog::info("combat state of {:#x}: {}", *call.Object(0), *call.IntAnswer());
        return result;
    });
    return true;
}
```

Add it to `CMakeLists.txt` as the example is (`add_library(name MODULE ...)`, output to `daolib/plugins`, `target_link_libraries(name PRIVATE daolib)`), build with `.\tools\build.ps1`, and `.\tools\deploy.ps1` copies every plugin into the game (with the game closed).

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

The game runs on several threads. Scripts run on one, the thread that filled the command table (`dao::script::ScriptThreadId()`). Calling a script command (`dao::script::Invoke`, `dao::script::api::*`) belongs on that thread: inside a command intercept, or inside a hook on something the scripts' thread runs. Whether the frame callback runs on that thread too is `dao::FrameThreadId()` against `ScriptThreadId()`; the probe logs the answer, not yet seen in play. A detour on an engine function runs on whichever thread calls it: never assume, and never block in one.

## The API, by what a plugin wants to do

| To | Use | Header |
|---|---|---|
| Call any script command | `dao::script::api::GetTeamId(creature)` and 660 more, with the scripts' names and defaults | `dao/generated/ScriptApi.h` |
| Call one by number, with its result or failure | `dao::script::Invoke<std::int32_t>(id, args...)` | `dao/Script.h` |
| See, change or replace a command when scripts call it | `dao::script::InterceptCommand(id, fn)`, `CommandCall` | `dao/ScriptHooks.h` |
| Add a new command | `dao::script::Set(freeSlot, handler)`, plus a `script.ldf` declaration for the toolset's compiler | `dao/Script.h` |
| Name a script constant | `dao::script::EventType::CommandComplete`, `dao::script::constants::EVENT_TYPE_COMMAND_COMPLETE` | `dao/generated/ScriptConstants.h` |
| Find an object by id | `CClientExoApp::Get()->GetCreatureById(id)` and kin | `dao/ClientExoApp.h` |
| Read an object's fields | `creature->CombatState()`, `->TeamId()`, `->Id()`, `->Type()`, casts `AsCreature()` | `dao/Objects.h` |
| Read an effect or an event | `dao::CGameEffect`, `dao::CScriptEvent`, `CCObject::CurrentEvent()` | `dao/Structures.h` |
| Use strings, locations, arrays | `dao::WString`, `dao::CScriptLocation`, `dao::script::Array` | `dao/String.h`, `dao/Structures.h`, `dao/Script.h` |
| Read a data table | `dao::gda::GetInt(Table::AbilitiesTalents, ability, "Cooldown")`; columns per table in `dev/gda_tables.csv` | `dao/Gda.h` |
| Pause, or switch the game's mode | `CClientExoApp::SetPaused`, `GameModeController::SetMode` | `dao/ClientExoApp.h`, `dao/GameModes.h` |
| Hook an engine message (the player's orders, pause, mode changes) | `dao::messages::handler::GameModeExplore_PerformAbilityMessage` with `TypedHandlerFn`, `As<M>()` | `dao/Messages.h`, `dao/generated/MessageHandlers.h` |
| Run code every frame | `dao::OnFrame(fn)` | `dao/Frame.h` |
| Hook any engine function | `dao::Detour<Fn>` on an address from `dao/Addresses.h` | `dao/Hook.h` |
| Know what an object is | `dao::TypeName(ptr)`, from the RTTI | `dao/TypeName.h` |
| Read memory that might not be there | `dao::TryRead<T>(address)` | `dao/SafeRead.h` |

## How much to trust each part

Every address and layout was read from the one supported exe, with a note in `dao/Addresses.h` or the class's header saying how and when. `dev/LIBRARY.md` keeps a table of what has been checked in play and what has only been read. As of 2026-09-30: object lookups, 14 fields, the object layout and casts, strings, locations, effects, events and object arrays have all matched the engine's own answers on thousands of calls. Calling commands from C++, intercepting them, the frame callback, the event queue, the game mode controller's table, the data tables and the messages' layouts are built but not yet seen in play. A plugin that relies on those should expect to be the one that finds out.

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
