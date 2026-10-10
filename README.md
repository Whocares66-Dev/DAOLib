# DAOLib

Native C++ plugins for **Dragon Age: Origins**: a loader that loads them when the game starts, and a library that gives them the game. What [SKSE](https://skse.silverlock.org/) and [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) are for Skyrim.

**Documentation: [whocares66-dev.github.io/DAOLib](https://whocares66-dev.github.io/DAOLib/)**, the API reference and the guide to writing a plugin.

It works with version 1.05.13263.0 of the game from GOG, EA and Steam. Each store's executable is different, and the [address library](https://github.com/Whocares66-Dev/DAOAddressLibrary) maps every function and global DAOLib uses in each of them, so one plugin runs in all three.

## A plugin

```cpp
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/ScriptHooks.h"
#include "dao/generated/ScriptApi.h"

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("myplugin");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
        return false;

    // Log every creature's combat state as the scripts ask for it.
    dao::script::InterceptCommand(dao::script::id::GetCombatState, [](dao::script::CommandCall &call) {
        const int result = call.Proceed();
        spdlog::info("combat state of {:#x}: {}", *call.Object(0), *call.IntAnswer());
        return result;
    });
    return true;
}
```

The library covers the game's script commands (all 661, callable and interceptable from C++), its objects, effects and events, the Flash interface, the camera, the world's geometry, saves, key bindings and the options screen, and the engine's messages. [Writing a plugin](https://whocares66-dev.github.io/DAOLib/guide.html) is the place to start; `examples/` has a small plugin for each part of the library.

## What is here

- `src/loader`: the loader, `version.dll`, which the game loads in place of the system's.
- `src/dao`: the library plugins link. Its public headers are the API the docs site documents.
- `src/plugins`: the example plugin, and the probe, which checks the library against the game in play.
- `examples/`: one small plugin per part of the library.
- `dev/`: notes for working on DAOLib: what has been learned about the engine, and the plan.
- `tools/`: building, deploying, the docs site, and reading the executable.

## Building

Visual Studio 2022 with the C++ workload, [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set, and Python 3. Everything is 32-bit, as the game is.

```powershell
.\tools\build.ps1 -Preset debug -Test   # the loader, the library, the plugins, the tests
.\tools\deploy.ps1                      # copy them into the game (with the game closed)
.\tools\deploy.ps1 -Remove              # put the game back to stock
python tools\build_docs.py --serve      # the docs site, at http://127.0.0.1:8045
```

Logs are written to `Documents\BioWare\Dragon Age\daolib\`. A plugin in a repository of its own brings DAOLib in as a git submodule; the [guide](https://whocares66-dev.github.io/DAOLib/guide.html) says how.

## Working on DAOLib

[CLAUDE.md](CLAUDE.md) has the project's rules: building, the checks every commit passes, how the executable is read (with `tools\daoexe.py`, Ghidra, and the address library's IDs), and where the notes in `dev/` are.

## License

GPLv3 ([LICENSE](LICENSE)).

## Thanks

- [SKSE](https://skse.silverlock.org/) and [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG), whose split between a loader and a library DAOLib follows, and Skyrim's [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444), the idea behind the [DAO address library](https://github.com/Whocares66-Dev/DAOAddressLibrary).
- [NWNX:EE](https://github.com/nwnxee/unified) and [NWNX2 for Linux](https://github.com/NWNX/nwnx2-linux), for the names and shapes of the classes Dragon Age inherited from Neverwinter Nights' engine, and for how a plugin hooks its scripting.
- [xoreos](https://xoreos.org/) ([source](https://github.com/xoreos/xoreos)), for the GFF4, GDA and talk-table formats and their field names, used under the GPL with its notices.
- The [Dragon Age Toolset Wiki](https://www.datoolset.net/wiki/Main_Page) and the [Dragon Age Wiki](https://dragonage.fandom.com/), for what the scripts and the game mean.
- The tools that made the reading possible: [Ghidra](https://ghidra-sre.org/) and [JPEXS Free Flash Decompiler](https://github.com/jindrapetrik/jpexs-decompiler).
- The libraries DAOLib builds on: [Microsoft Detours](https://github.com/microsoft/Detours), [spdlog](https://github.com/gabime/spdlog), [Catch2](https://github.com/catchorg/Catch2) and [Doxygen](https://www.doxygen.nl/).
- BioWare, for the game and its toolset.
