# DAOLib

A native plugin loader and reverse-engineered library for **Dragon Age: Origins** (GOG Ultimate Edition, `DAOrigins.exe` 1.05.13263.0), in the spirit of SKSE and [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) for Skyrim: for what the toolset's scripting cannot reach.

- `src/loader` -- `version.dll`, loaded by the game in place of the system's, which loads plugins from `bin_ship\daolib\plugins\` before the game starts.
- `src/dao` -- the library plugins are written against: addresses, the exe fingerprint, RTTI type names, detours, logging, and the first engine class (`CClientExoApp`: pause).
- `src/plugins/probe` -- the first plugin: hooks pause, logs every call and who made it, changes nothing.
- `dev/` -- the plan, the reading so far, and two maps of the engine: its 827 script commands and its 314 message handlers.
- `tools/` -- build, deploy, Ghidra analysis and MCP, exe readers.

Early, but the chain works. On 2026-09-29 the loader ran inside the game and loaded the probe before any game code. The probe's hooks on the engine's pause then saw the engine call them.

## Building

Visual Studio 2022 with the C++ workload, CMake and Ninja (both come with it), [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set, and Python 3. Formatting and linting want LLVM 22+ (`winget install LLVM.LLVM`), `pip install --user ruff gersemi`, and `Install-Module PSScriptAnalyzer`.

```powershell
.\tools\build.ps1 -Preset debug -Test          # loader, library, plugins, tests
.\tools\build.ps1 -Preset debug -Format -Lint  # formatters, then clang-tidy and friends
.\tools\deploy.ps1                             # copy into the game's bin_ship (game closed)
.\tools\deploy.ps1 -Remove                     # back to stock
```

Logs land in `Documents\BioWare\Dragon Age\daolib\`; `DAOLIB_LOG_LEVEL=debug` in the environment turns them up.

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
