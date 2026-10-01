# DAOLib -- project context

A native plugin loader and library for Dragon Age: Origins, in the spirit of SKSE plus CommonLibSSE-NG for Skyrim. Read `dev/PLAN.md` first: what the three pieces are (loader, library, plugins), the rules, and the order of work.

DAOLib is a general library. Its first use case, a turn-based combat mode, is built in the sibling repo `C:\project\DAOTurnBased`, which records where the library falls short so it can be improved here; turn logic does not belong in this repo. `dev/TURN_BASED.md` keeps the engine findings made for that mode (pause, the command queue, the AI's loop). Two tables map the engine. `dev/SCRIPT_COMMANDS.md` covers the 827 script commands, cross-checked against the game's own `script.ldf` (`dev/script_commands.csv`). `dev/MESSAGE_HANDLERS.md` covers the 314 message handlers, the engine's event API (`dev/message_handlers.csv`). `dev/TOOLSET.md` is what the Dragon Age toolset installer holds that matters here, the source of the game's 2,101 scripts among it (`tools/toolset_scripts.py` extracts them to `toolset\extract\scripts\`). `dev/CRASHES.md` is the crash logger the loader installs, and the crashes it has explained. `dev/LINEAGE.md` is what Neverwinter Nights' community work (NWNX:EE, xoreos) can and cannot tell us about this engine. `dev/PLUGINS.md` is the guide to writing a plugin against the library. `dev/SAVES.md` is the save game as `tools/gff4.py` reads it, with GFF4's field names from xoreos (`dev/gff4_fields.csv`). DAOLib is GPLv3 (`LICENSE`), so GPL-compatible sources like xoreos can be copied in with their notices. `dev/GUI.md` is the Flash UI: movies calling the game and the game calling them, the movie records, how a movie is loaded. `dev/PROBE.md` is the probe plugin, which counts every script command and message handler and samples the game's threads during play, and how to read a session with `tools/probe_report.py`. `dev/LIBRARY.md` is the library as a plugin uses it: generated field descriptors, the classes over them, and the probe's checks of both in play. `dev/TODO.md` is what to reverse engineer next and why, most valuable first: native work where scripts and data tables cannot reach.

Markdown here is not hard-wrapped: one line per paragraph or list item.

## The target -- verified 2026-09-29, do not "fix"

| | |
|---|---|
| Game | Dragon Age: Origins Ultimate Edition, GOG, `C:\Program Files (x86)\GOG Galaxy\Games\Dragon Age Origins` |
| Executable | `bin_ship\DAOrigins.exe` 1.05.13263.0, 32-bit x86, no ASLR, no DRM, RTTI intact, link stamp 0x4eb19d53 |
| Our copy | `game\DAOrigins.exe` (ignored by git); `python tools\daoexe.py --check` says it is the expected build |
| Ghidra | 12.1.4 at `C:\Modding\ghidra_12.1.4_PUBLIC`; project `game\ghidra\dao.gpr` (ignored) |
| MCP | GhidrAssistMCP (our `agentic-switch` branch, `C:\Modding\GhidrAssistMCP`), port 8090; the Skyrim projects hold 8080-8082 |
| Toolchain | VS 2022 `cl.exe` targeting x86 (`vcvarsall amd64_x86`), vcpkg at `C:\vcpkg`, triplet `x86-windows-static-md`, LLVM 22 for clang-format/tidy, capstone for `tools\daoexe.py --disasm` |
| Logs | `Documents\BioWare\Dragon Age\daolib\`, which on this machine is under OneDrive: `C:\Users\tianx\OneDrive\Documents\BioWare\Dragon Age\daolib\` |
| Toolset | installer in `toolset\` (ignored); `dev/TOOLSET.md` |

**Everything is 32-bit.** The game is, so the loader and every plugin must be. `CMakeLists.txt` refuses a 64-bit configure, and `tools\build.ps1` sets the environment up right. Inline asm and `__declspec(naked)` in the proxy are x86 features, and that is fine.

**One build.** Every address in `src/dao/Addresses.h` was read from the exe above. Each is guarded at runtime by `dao::Module::IsExpectedExecutable()`, which compares the loaded image's headers with `dao::core::kExpected` in `src/dao/core/Fingerprint.h`. `tools/daoexe.py` carries the same numbers, and the two must agree. Addresses are preferred-base virtual addresses, as the Ghidra listing shows them; `dao::Module::Address` relocates.

## Layout

```
src/loader/     version.dll: the proxy (Proxy.cpp), the entry-point detour that loads plugins (Loader.cpp), the crash logger (CrashLog.cpp)
src/dao/        daolib, what plugins link: Addresses.h, Module.h, Hook.h (Detours), TypeName.h (RTTI), Log.h, Plugin.h,
                the engine's classes (ClientExoApp.h, Objects.h), the command table and VM stack (Script.h), calling a command by name (ScriptCall.h), the player's orders (Orders.h), abilities' names (Abilities.h), drawing in the world (Vfx.h), commands starting and ending (CommandEvents.h),
                the script's value types (Structures.h, String.h), intercepting commands (ScriptHooks.h),
                the engine's messages (Messages.h), the game's modes (GameModes.h), a callback per frame
                (Frame.h), script events (Events.h), the game's data tables (Gda.h), the Flash UI's calls (Gui.h), the data tree (Data.h), the talk tables (Tlk.h), the camera (Camera.h, CameraMoves.h), a plugin's save data (Saves.h), the effects' and regeneration's clock and the game's speed (Effects.h, Clock.h), the world's geometry (World.h), conversations (Dialogue.h),
                Field.h; dev/LIBRARY.md says how they fit
src/dao/generated/  written by tools: field descriptors (gen_script_fields.py), cast slots (object_casts.py),
                the scripts' constants as enums (gen_script_constants.py), every script command as a
                C++ function and as a table of signatures (gen_script_api.py), message handlers as hook points
                (gen_message_handlers.py); do not edit
src/dao/core/   the part that never touches the process: Fingerprint, Relocate, Rtti undecoration, FieldPath,
                CommandWord, ObjectType, Msvc8 (the engine's std::wstring and std::vector), ScriptArray, Angle, Gff4 and Conversation (the game's files), ScriptValue (a command's signature as data),
                VmStack, GdaHash; tested
src/plugins/    one folder per plugin: probe/ watches and checks, example/ checks many parts of daolib in play
examples/       small plugins, one per part of daolib, to read and copy from and to check it in play (examples/README.md)
tests/          Catch2 over src/dao/core; ctest also runs tools/test_*.py and gen_script_fields.py --check
tools/          build.ps1, deploy.ps1, ghidra-mcp.ps1, daoexe.py, msvc_names.py, script_commands.py,
                message_handlers.py, script_backends.py, script_fields.py, gen_script_fields.py, object_casts.py,
                names.py, probe_targets.py, probe_report.py, sqlbak.py, toolset_scripts.py, gen_script_constants.py, gen_script_api.py, gen_message_handlers.py, messages.py, gff4.py, gda.py, tlk.py, ghidra/
dev/            plan, reading, tables
game/           the exe and its Ghidra project (ignored)
toolset/        the toolset installer and what is extracted from it (ignored)
```

**The one architectural rule: `src/dao/core` never includes `<Windows.h>` or reads process memory.** There is no headless Dragon Age, so the only automated tests are over logic that needs no game: PE headers as bytes, address arithmetic, name undecoration. If code in `core/` wants the process, it belongs one level up.

## Building

```powershell
.\tools\build.ps1 -Preset debug -Test           # loader, library, plugins, tests. Seconds after the first run.
.\tools\build.ps1 -Preset debug -Format -Lint   # rewrite with the formatters, then clang-tidy, ruff, PSScriptAnalyzer
.\tools\build.ps1 -Preset debug-asan -Test      # the tests under AddressSanitizer
.\tools\build.ps1 -Preset debug-analyze        # our own targets under MSVC's /analyze
.\tools\build.ps1 -Preset release
```

Output is `build\<preset>\version.dll` and `build\<preset>\daolib\plugins\*.dll`, a game folder's worth of files. **A build copies nothing into the game.** `.\tools\deploy.ps1` does, with the game closed, since it holds the DLLs open. `-Remove` returns the install to stock: `version.dll` there is ours alone, and the system's stays in `SysWOW64`. Logs go to one file per DLL (`loader.log`, `probe.log`), at info unless `DAOLIB_LOG_LEVEL` says otherwise. The first line of each names the level it took.

The first `debug` build compiles spdlog, Detours and Catch2 through vcpkg for x86, which takes a few minutes, and so does the first of each other preset. Last verified green 2026-09-30: build, tests, format, lint, the tests under ASan, and /analyze.

**Verified in the game 2026-09-29.** The game started with the probe deployed. `loader.log` showed the fingerprint matching, the entry point detoured and `probe.dll` loaded, and `probe.log` showed both pause hooks placed. The engine then called `CClientExoApp::SetPaused(false, false)` from `0x005DD34F`, a startup routine. The hook read the live object's RTTI as `CClientExoApp`, and the game played on. That call came on a different thread from the one the entry point ran on, so the game loop is not on the process's first thread. The install was put back to stock afterwards with `deploy.ps1 -Remove`.

## Before every commit

```powershell
.\tools\build.ps1 -Preset debug -Test -Format -Lint
.\tools\build.ps1 -Preset debug-asan -Test
.\tools\build.ps1 -Preset debug-analyze
```

Tests first, then the formatters (clang-format, ruff format, gersemi for CMake) rewrite in place, then the linters. The `format-check` target fails instead of rewriting. Every one of these fails the run on a finding:

- MSVC at `/W4 /WX` on our own targets; third-party headers are `/external:W0`.
- clang-tidy (`WarningsAsErrors`). `.clang-tidy` says which checks are off and why: each is either style, or fires on what a hook or an address cast is by nature. The stamp per file under `build\debug\tidy\` means only changed files are re-checked.
- ruff check over the Python, PSScriptAnalyzer over the PowerShell.
- AddressSanitizer over the tests (`debug-asan`). Only the tests: a sanitized DLL in the game would need the ASan runtime beside the exe. The STL's container annotations are off there, since vcpkg's libraries are built without them.
- MSVC's static analyser (`debug-analyze`), in its own tree because clang-tidy reads the debug tree's compile database and clang rejects `/analyze`. A finding is fixed, not suppressed, unless the code is a hook doing what hooks do; then the suppression says why.

## Reading the executable

The exe is the authority. The wiki, the toolset's `dascript.chm` and memory are leads. There are three ways in, cheapest first:

1. **`tools\daoexe.py`**: `--rtti`/`--vtables <text>` for the classes and their vtables (its own walk of the RTTI, 2,990 vtables), `--class-of 0xVA`, `--bases Class` for a class's bases at their offsets in the object, `--refs 0xVA` for code holding an address, `--disasm 0xVA [n]`, `--commands`, `--strings`, `--imports`. It needs no Ghidra and answers in seconds, the cheapest way to follow a call chain.
2. **`.\tools\ghidra-mcp.ps1 -Query "..."`** from a shell: `vtable <class>`, `string <text>`, `refs <0xaddr>`, `decompile <0xaddr>`, `symbols <text>`, `analyzers`, `info`. It opens the project read-only for the question, with a minute of Ghidra start-up each time.
3. **The MCP server**: run `.\tools\ghidra-mcp.ps1` in its own window, then use the `ghidra-dao` tools in Claude Code (`.mcp.json`). Stop it with `-Stop` so Ghidra saves what was renamed over MCP. The project is locked while any of these has it open, so run one at a time. Start the server before Claude Code, which connects at session start. If the server came up later, `/mcp` reconnects without a restart. It takes about a minute to report "Headless MCP server ready".

`-Import` analyses the exe once, in about six minutes, then runs Ghidra's RTTI pass and class recovery. `-Recover` runs those two over an existing analysis, in about fifteen minutes. In Ghidra 12 they are scripts, not analyzers, and `tools\ghidra\set_compiler.py` makes them work on this exe. Done 2026-09-29, the result is saved in the project:

| Class recovery | |
|---|---|
| Classes | 2,383 |
| Constructors | 1,540 |
| Destructors | 673 |
| Virtual functions assigned to classes | 30,439 |
| Functions | 33,358 |

It also produced a structure per class. For 616 functions Ghidra could not retype `this` ("Auto-parameter may not be modified"), and they keep their old parameter type.

Two traps made this take three tries. First, both scripts check that the compiler is exactly PeLoader's label `visualstudio:unknown`, and the bare `visualstudio` fails silently. Second, the RTTI pass saves an "RTTI Found" flag on its first run and skips every later run, so a run with the wrong compiler blocks all the rest. `set_compiler.py` sets the label and clears a stale flag.

The class names also come from `tools\daoexe.py`'s own walk, which needs no Ghidra. On every launch `tools\ghidra\ImportNames.java` applies what `tools\names.py` knows, about 3,900 names, the earlier source winning an address:

- every `DAO_ADDRESS`/`DAO_DATA` row of `Addresses.h`
- every message handler, as `Owner::HandleMessage_Message`
- every implemented script command, as `ScriptCommand::NAME`
- every vftable, as `Class::vftable`

It creates a function where analysis found none. A name set by hand in Ghidra wins over all of them.

**MSVC templates** have names like `MemberFunctionMessageHandler<GameModeController,UserTogglePauseMessage,&GameModeController::HandleMessage>`. `tools\msvc_names.py` undecorates them, and `names.py` writes their inner `::` as `.` and their spaces as `_` so Ghidra accepts them. The C++ `dao::TypeName` leaves a template's name decorated rather than guess.

**Recording a finding.** An address goes into `src/dao/Addresses.h` with the engine's name for it, what it is, how it was found, and the date. A layout goes into a class header under `src/dao/`, like `ClientExoApp.h`, with the evidence per field. A field a getter command returns goes through `dev/script_fields.csv` and `tools/gen_script_fields.py` instead, and gets its name in `Objects.h`. A hypothesis is tried in the probe plugin before anything relies on it: detour, log what arrives (`dao::TypeName` says what an object is), change nothing.

## Hooking on x86

Member functions are `__thiscall`: `this` in ecx, arguments on the stack, and the callee cleans up. A hook on one is written `__fastcall` with `this` first and an unused `int` second (edx), and the `Fn` type in `dao::Detour<Fn>` is declared the same way; `src/dao/Hook.h` has the pattern. Free functions and statics are `__cdecl` unless the listing shows otherwise; a `ret N` says `__stdcall`. Get this wrong and the stack is off by the arguments' size at the return, so the crash comes somewhere after the hook, not in it.

The loader detours the exe's entry point from `DllMain`, which Detours permits (its own samples do it). Plugins are loaded from the detour, on the process's first thread, with the loader lock released. A plugin's `DAOLib_Load` runs before any game code, so a detour placed there is in place for the first call. No game object exists yet, so load does hooks, not reads (`CClientExoApp::Get()` is null there, verified). The game's own work then runs on other threads, so a hook must not assume which thread calls it.

A hook that wants its caller logs `_ReturnAddress()` rebased to the preferred base, as `src/plugins/probe/Probe.cpp` does. That gives an address to look up with `tools\daoexe.py --disasm` or in Ghidra.

## Style

Formatting is clang-format's stock Microsoft style (`.clang-format`, one line), and lint is correctness only. Neither is up for discussion per file. Comments say why and what was read, with dates, not what the code does. A file's head says what the file is for and how it fits.
