# DAOLib

A native plugin loader and reverse-engineered library for **Dragon Age: Origins** (GOG Ultimate Edition, `DAOrigins.exe` 1.05.13263.0), in the spirit of SKSE and [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) for Skyrim: for what the toolset's scripting cannot reach.

- `src/loader` -- `version.dll`, loaded by the game in place of the system's, which loads plugins, each a folder of its own in `bin_ship\daolib\plugins\`, before the game starts.
- `src/dao` -- the library plugins are written against: every script command callable from C++ (661 of them, generated from the game's own declarations) and interceptable, the engine's objects, effects, events, strings, arrays and data tables as C++ types, its messages and game modes, a per-frame callback, and the addresses, detours and RTTI type names underneath (`dev/LIBRARY.md`).
- `src/plugins/example` -- a whole plugin to copy from; `dev/PLUGINS.md` is the guide to writing one.
- `src/plugins/probe` -- watches the game and checks the library against it in play, changing nothing.
- `dev/` -- the plan, the reading so far, and two maps of the engine: its 827 script commands and its 314 message handlers.
- `tools/` -- build, deploy, Ghidra analysis and MCP, exe readers.

The loader, the probe and much of the library have been checked inside the game: object lookups, fields, the object layout, strings, locations, effects, events and arrays matched the engine's own answers on thousands of calls in play. `dev/LIBRARY.md` says what is checked and what is only read so far.

## Building

Visual Studio 2022 with the C++ workload, CMake and Ninja (both come with it), [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set, and Python 3. Formatting and linting want LLVM 22+ (`winget install LLVM.LLVM`), `pip install --user ruff gersemi`, and `Install-Module PSScriptAnalyzer`.

```powershell
.\tools\build.ps1 -Preset debug -Test          # loader, library, plugins, tests
.\tools\build.ps1 -Preset debug -Format -Lint  # formatters, then clang-tidy and friends
.\tools\deploy.ps1                             # copy into the game's bin_ship (game closed)
.\tools\deploy.ps1 -Remove                     # back to stock
```

Logs land in `Documents\BioWare\Dragon Age\daolib\`; `DAOLIB_LOG_LEVEL=debug` in the environment turns them up. A log is written on a thread of its own, so logging does not cost the game's frame a disk write.

DAOLib is free software under the GNU General Public License, version 3 (`LICENSE`).

A plugin in a repository of its own brings DAOLib in as a submodule, `add_subdirectory(extern/DAOLib EXCLUDE_FROM_ALL)`, and declares itself with `add_daolib_plugin(name SOURCES ...)`, as Skyrim plugins do with CommonLibSSE (`dev/PLUGINS.md`).

## Reading the executable

Copy the game's `bin_ship\DAOrigins.exe` to `game\DAOrigins.exe` (ignored by git). `--disasm` wants `pip install capstone`. Then:

```powershell
python tools\daoexe.py --check              # is it the expected build
python tools\daoexe.py --vtables Pause      # classes and vtables from the RTTI; also --disasm, --refs, --class-of
python tools\script_commands.py             # the script command table -> dev\script_commands.csv
python tools\message_handlers.py            # the message handler table -> dev\message_handlers.csv
.\tools\ghidra-mcp.ps1 -Import              # once: Ghidra analysis, RTTI, class recovery
.\tools\ghidra-mcp.ps1                      # serve it over MCP for Claude Code (.mcp.json)
.\tools\ghidra-mcp.ps1 -Query "decompile 0x006403C0"   # or ask from the shell
```

See `CLAUDE.md` for the project's working rules and `dev/PLAN.md` for where it is going.
