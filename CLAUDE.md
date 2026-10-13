# DAOLib -- project context

A native plugin loader and library for Dragon Age: Origins, in the spirit of SKSE plus CommonLibSSE-NG for Skyrim. Read `dev/PLAN.md` first: what the three pieces are (loader, library, plugins), the rules, and the order of work.

DAOLib is a general library. Its first use case, a turn-based combat mode, is built in the sibling repo `C:\project\DAOTurnBased`, which records where the library falls short so it can be improved here; turn logic does not belong in this repo. `dev/TURN_BASED.md` keeps the engine findings made for that mode (pause, the command queue, the AI's loop). `dev/AI.md` is the creatures' AI apart from any mode: the scripts' own switches for it, one creature's AI off among them. Two tables map the engine. `dev/SCRIPT_COMMANDS.md` covers the 827 script commands, cross-checked against the game's own `script.ldf` (`dev/script_commands.csv`). `dev/MESSAGE_HANDLERS.md` covers the 314 message handlers, the engine's event API (`dev/message_handlers.csv`). `dev/TOOLSET.md` is what the Dragon Age toolset installer holds that matters here, the source of the game's 2,101 scripts among it (`tools/toolset_scripts.py` extracts them to `toolset\extract\scripts\`). `dev/CRASHES.md` is the crash logger the loader installs, and the crashes it has explained. `dev/LINEAGE.md` is what Neverwinter Nights' community work (NWNX:EE, xoreos) can and cannot tell us about this engine. `dev/PLUGINS.md` is the guide to writing a plugin against the library. `dev/FILES.md` is the game's files: where they are, and which copy the game uses when several places have one (the resource manager's order, read in the exe and seen in play). `dev/SAVES.md` is the save game as `tools/gff4.py` reads it, with GFF4's field names from xoreos (`dev/gff4_fields.csv`). DAOLib is GPLv3 (`LICENSE`), so GPL-compatible sources like xoreos can be copied in with their notices. `dev/GUI.md` is the Flash UI: movies calling the game and the game calling them, the movie records, how a movie is loaded. `dev/PROBE.md` is the probe plugin, which counts every script command and message handler and samples the game's threads during play, and how to read a session with `tools/probe_report.py`. `dev/LIBRARY.md` is the library as a plugin uses it: generated field descriptors, the classes over them, and the probe's checks of both in play. `dev/TODO.md` is what to reverse engineer next and why, most valuable first: native work where scripts and data tables cannot reach. `dev/BUILDS.md` is how the library works in more than one build of the game: IDs in place of addresses, the address library's files, the loader in EA's wrapped build, and what was seen in play there. `dev/TESTING.md` is what is seen in the game before a merge and before a release: each build's game, GOG's, Steam's and EA's, started under the Windows compatibility settings a player may have ticked (`tools\play_matrix.ps1`). `dev/RELEASING.md` is how a release is cut (`tools\release.ps1`, `CHANGELOG.md`): what the zip holds, the address library's release it is held at, and the release build seen in the game first. The builds themselves, and where everything is in each, are the sibling repo `C:\project\DAOAddressLibrary`'s, brought in as the submodule `extern/DAOAddressLibrary`.

Markdown here is not hard-wrapped: one line per paragraph or list item.

## The target -- verified 2026-09-29, do not "fix"

| | |
|---|---|
| Game | Dragon Age: Origins Ultimate Edition, GOG, `C:\Program Files (x86)\GOG Galaxy\Games\Dragon Age Origins` |
| Executable | `bin_ship\DAOrigins.exe` 1.05.13263.0, 32-bit x86, no ASLR, no DRM, RTTI intact, link stamp 0x4eb19d53 |
| Our copy | `game\DAOrigins.exe` (ignored by git); `python tools\daoexe.py --check` says it is the expected build |
| EA's build | `C:\Program Files\EA Games\Dragon Age\bin_ship\daorigins.exe`, also 1.05.13263.0, link stamp 0x52164c37, wrapped. Read in `C:\project\DAOAddressLibrary`, whose `game\ea\` has the copies. `dev/BUILDS.md` |
| Steam's build | `E:\SteamLibrary\steamapps\common\Dragon Age Ultimate Edition\bin_ship\DAOrigins.exe`, 1.05.13263.0: GOG's code, and GOG's link stamp 0x4eb19d53, wrapped by SteamStub 2.1. Read in `C:\project\DAOAddressLibrary` (`dev/STEAM.md`). `dev/BUILDS.md` |
| Ghidra | 12.1.4 at `C:\Modding\ghidra_12.1.4_PUBLIC`; project `game\ghidra\dao.gpr` (ignored) |
| MCP | GhidrAssistMCP (our `agentic-switch` branch, `C:\Modding\GhidrAssistMCP`), port 8090; the Skyrim projects hold 8080-8082 |
| Toolchain | VS 2022 `cl.exe` targeting x86 (`vcvarsall amd64_x86`), vcpkg at `C:\vcpkg`, triplet `x86-windows-static-md`, LLVM 22 for clang-format/tidy, capstone for `tools\daoexe.py --disasm` |
| Logs | `Documents\BioWare\Dragon Age\daolib\`, which on this machine is under OneDrive: `C:\Users\tianx\OneDrive\Documents\BioWare\Dragon Age\daolib\` |
| Toolset | installer in `toolset\` (ignored); `dev/TOOLSET.md` |

**Everything that goes into the game is 32-bit.** The game is, so the loader and every plugin must be. `CMakeLists.txt` refuses a 64-bit configure of them, and `tools\build.ps1` sets the environment up right. Inline asm and `__declspec(naked)` in the proxy are x86 features, and that is fine. The one exception is `src/dao/core`, the target `daolib_core`, which builds 64-bit as well (`-Preset core-x64`, the core and its tests alone) for tools that read the game's files with no game, Eluvian's ghilana first (DAOLib #106). Core's overlays of the engine's memory (`Msvc8.h`'s, `ScriptArray.h`'s) are declared only in a 32-bit build.

**IDs, read in the GOG build.** `src/dao/Addresses.h` holds IDs, not addresses or names: `DAO_ADDRESS(kCClientExoApp_SetPaused, 10547)` is ID 10547 of the address library (`extern/DAOAddressLibrary`), which means the same function in every build of the game. What it is called and where it is in each build are in the library's table (`extern\DAOAddressLibrary\data\addresses.csv`) and nowhere else: `python tools\address_ids.py kCClientExoApp_SetPaused` (or `10547`, or `0x005D6740`, or part of a name) says both. Read a new one in the GOG exe above, write the row as it was read, `DAO_ADDRESS(kName, "Engine::Name", 0x005D6740)`, and run `python tools\address_ids.py`: it finds the ID, writes the name into this checkout's copy of the table and leaves the row as the ID alone, and says what the library notes about the ID (a function `changed` in a build is not the same code there: a detour at its start is fine, an offset into it may not be). **The address library has its own keeper: commit and push nothing in `extern\DAOAddressLibrary` from here** (the user's rule, 2026-10-01). Send its session the line the tool prints (ID, kind, name, GOG address) with what the thing is; it commits the name to the library's `master`; then check that commit out in the submodule and commit the submodule with the row. Hold the row's commit until then: one made earlier would hold the library at a commit without the name and fail the ctest for everyone else. A problem with the library, its tools or its docs goes to its session the same way. The addresses in the comments of `Addresses.h` are GOG's, for reading against Ghidra; no code uses them. A ctest fails on a row not yet turned into its ID, or whose ID the table does not have as that kind of thing with a name. At run time `dao::Module::Address(id)` looks the ID up in the address library's file for the running build (`daolib\addresses\daoaddresses-<link stamp>-<image size>-<entry rva>-<base>.bin`, beside the loader: all four header fields, since Steam's build has GOG's link stamp), and `dao::Module::Preferred` goes back to GOG's address, so logs read the same whichever build ran. Everything is guarded by `dao::Module::IsKnownExecutable()`: whether there is such a file for the running exe. No build is compiled in. An ID does not move a field: `dev/BUILDS.md` lists the classes whose layout differs between builds.

## Layout

```
src/loader/     dinput8.dll: the proxy (Proxy.cpp), the entry-point detour that loads plugins (Loader.cpp), the crash logger (CrashLog.cpp)
src/fix/        corrections to the engine's own faults, one file each, built into dinput8.dll and put in by the loader
                before the plugins load: always on, for every game, whatever plugins it has. A resource read at its own
                offset in its archive's file (ArchiveRead), a saved file's name handed to the renderer in a buffer
                of the kilobyte the renderer copies (SavedFileName), the mouse's movement timed by the last
                record read and not the one past it (MouseRecords). dev/CRASHES.md has the crash each one fixed
src/perf/       what makes the game faster and corrects no fault, built into dinput8.dll the same way. The engine's
                memory from the system's heap in place of its own 850 MiB of pools (EngineHeap, off with
                DAOLIB_ENGINE_HEAP=0), the game on every processor where GOG's exe starts it on the
                first two (AllProcessors, off with DAOLIB_ALL_PROCESSORS=0), and fast load (FastLoad, its parts under fastload/): the area lists the party
                leaves kept, an area's rooms kept and the indoor ones beside it loaded ahead, the loading screen at a
                turn of 16 ms and presented fifteen times a second. Fast load has a switch on the options screen's
                Game tab, kept in daolib.ini beside the logs; DAOLIB_FAST_LOAD=0 leaves it out
src/dao/        daolib, what plugins link: Addresses.h, Module.h, Hook.h (Detours), TypeName.h (RTTI), Log.h, Plugin.h,
                the engine's classes (ClientExoApp.h, Objects.h), the command table and VM stack (Script.h), calling a command by name (ScriptCall.h), the player's orders (Orders.h), abilities' names (Abilities.h), drawing in the world (Vfx.h), commands starting and ending (CommandEvents.h), a plugin's own text (Strings.h), the party's selection and who is controlled (Party.h), equipment going on and coming off (Equipment.h),
                the script's value types (Structures.h, String.h), intercepting commands (ScriptHooks.h),
                the engine's messages (Messages.h), the game's modes (GameModes.h), a callback per frame
                (Frame.h), script events (Events.h), the game's data tables (Gda.h), the Flash UI's calls (Gui.h), the data tree (Data.h), the talk tables (Tlk.h), the camera (Camera.h, CameraMoves.h), a plugin's save data (Saves.h), the effects' and regeneration's clock and the game's speed (Effects.h, Clock.h), the world's geometry (World.h), conversations (Dialogue.h), the journal's quests (Journal.h), how long animations, attacks and abilities take (Animation.h), a plugin's own rebindable keys (Input.h), a plugin's settings in the options screen (Options.h), the scripts' own timers on a plugin's clock (Timers.h), a file as the game finds it (Resources.h), a spell held in its conjure (Conjure.h), a portrait's camera (Portraits.h), an attack's reach and approach (Attack.h), a creature's weapon damage per hand (Damage.h), a command's sub-actions and the time it still holds a creature (SubActions.h), the area's sky and sunlight (Atmosphere.h), the framebuffer's effects (Framebuffer.h), an object's id chosen by a plugin (ObjectIds.h),
                Field.h; dev/LIBRARY.md says how they fit
src/dao/generated/  written by tools: field descriptors (gen_script_fields.py), cast slots (object_casts.py),
                the scripts' constants as enums (gen_script_constants.py), every script command as a
                C++ function and as a table of signatures (gen_script_api.py), message handlers as hook points
                (gen_message_handlers.py); do not edit
src/dao/core/   the part that never touches the process: Fingerprint, Relocate, Rtti undecoration, FieldPath,
                CommandWord, ObjectType, Msvc8 (the engine's std::wstring and std::vector), ScriptArray, Angle, Gff4, Conversation and TalkTable (the game's files), ScriptValue (a command's signature as data),
                VmStack, GdaHash, InputCode (a key as the engine's input word), Inflate, GfxMovie and OptionsMovie (a Flash movie patched as bytes, and the options screen's), HeldEvents and OwedTime (delayed events and ageing on a plugin's clocks), Sky (an area's sky as the engine's block of floats), Erf (an archive's directory, versions 2.0 and 2.2, and an archive on disk with any file of it read by name, #106) and Dds (a texture's first picture), Gff3 (the toolset's templates, .utc and kin), GdaFile and M2da (a data table, and tables merged as the game merges them), AddIns (AddIns.xml and offers.xml), AddInOrder (the order the game takes them in, by MSVC 2005's sort) and Install (an install's sources and every copy of a resource in the game's order, paths in; #106, moved in from Eluvian's ghilana), Level (a room's definition, a model's meshes and a placed model's lightmapped triangles, from the game's files), RayCast (rays against triangles) and LightProbes (a level's light probes baked from its lightmaps); tested.
                The target daolib_core, which daolib links; x86 and x64
src/plugins/    one folder per plugin: probe/ watches and checks, example/ checks many parts of daolib in play
examples/       small plugins, one per part of daolib, to read and copy from and to check it in play (examples/README.md)
tests/          Catch2: dao_core_tests over src/dao/core (x86 and x64), dao_tests over what of daolib needs no game;
                ctest also runs tools/test_*.py and gen_script_fields.py --check
tools/          build.ps1, deploy.ps1, ghidra-mcp.ps1, daoexe.py, msvc_names.py, script_commands.py,
                message_handlers.py, script_backends.py, script_fields.py, gen_script_fields.py, object_casts.py,
                names.py, probe_targets.py, probe_report.py, sqlbak.py, toolset_scripts.py, gen_script_constants.py, gen_script_api.py, gen_message_handlers.py, messages.py, gff4.py, gda.py, tlk.py, address_ids.py (the IDs in the source are the address library's), package.ps1 and release.ps1 (the zip a player installs, and a release of it), ea_play.py (the EA game with the build's loader, nothing deployed), play_matrix.ps1 (a build's game started under each compatibility setting, and the loader's start checked), mutants.py (the core broken on purpose), ghidra/
dev/            plan, reading, tables
extern/         DAOAddressLibrary (a submodule): the IDs, each build's addresses, the reader daolib links, the files it reads
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
.\tools\build.ps1 -Preset core-x64 -Test       # dao/core and its tests alone, built 64-bit
.\tools\build.ps1 -Preset debug -Mutants      # the core broken on purpose, one promise at a time: minutes
```

Output is `build\<preset>\dinput8.dll` and `build\<preset>\daolib\plugins\<name>\`, each plugin a folder of its own (its DLL and its files), a game folder's worth of files. **A build copies nothing into the game.** `.\tools\deploy.ps1` does, with the game closed, since it holds the DLLs open. `.\tools\package.ps1` makes the zip a player installs from (`dist\DAOLib-<version>.zip`, the loader, its PDB and the address files laid out as the game folder, with a README), after a `release` build, which keeps its PDBs for that; `.\tools\release.ps1` tags and publishes it (`dev/RELEASING.md`). `-Remove` returns the install to stock: `dinput8.dll` there is ours alone, and the system's stays in `SysWOW64`. Logs go to one file per DLL (`loader.log`, `probe.log`), at info unless `DAOLIB_LOG_LEVEL` says otherwise. The first line of each names the level it took.

The first `debug` build compiles spdlog, Detours and Catch2 through vcpkg for x86, which takes a few minutes, and so does the first of each other preset. Last verified green 2026-09-30: build, tests, format, lint, the tests under ASan, and /analyze.

**Verified in the game 2026-09-29.** The game started with the probe deployed. `loader.log` showed the fingerprint matching, the entry point detoured and `probe.dll` loaded, and `probe.log` showed both pause hooks placed. The engine then called `CClientExoApp::SetPaused(false, false)` from `0x005DD34F`, a startup routine. The hook read the live object's RTTI as `CClientExoApp`, and the game played on. That call came on a different thread from the one the entry point ran on, so the game loop is not on the process's first thread. The install was put back to stock afterwards with `deploy.ps1 -Remove`.

## Before merging to master

```powershell
.\tools\build.ps1 -Preset debug -Test -Format -Lint
.\tools\build.ps1 -Preset debug-asan -Test
.\tools\build.ps1 -Preset debug-analyze
.\tools\build.ps1 -Preset core-x64 -Test
```

**And in the game** (`dev/TESTING.md`), for a merge that changes what is built: each of the three builds started with no compatibility setting and with "Disable fullscreen optimizations", `.\tools\play_matrix.ps1` after a deploy into each. A release is seen under the whole matrix, `-All`, on its release build.

**Not before every commit** (the user, 2026-10-11: "no need to run the fucking build gates every commit. only when merging to master"). On a feature branch, build and test what the change needs and commit. The three runs are for the merge to master.

Tests first, then the formatters (clang-format, ruff format, gersemi for CMake) rewrite in place, then the linters. The `format-check` target fails instead of rewriting. Every one of these fails the run on a finding:

- MSVC at `/W4 /WX` on our own targets; third-party headers are `/external:W0`.
- clang-tidy (`WarningsAsErrors`). `.clang-tidy` says which checks are off and why: each is either style, or fires on what a hook or an address cast is by nature. The stamp per file under `build\debug\tidy\` means only changed files are re-checked.
- ruff check over the Python, PSScriptAnalyzer over the PowerShell.
- AddressSanitizer over the tests (`debug-asan`). Only the tests: a sanitized DLL in the game would need the ASan runtime beside the exe. The STL's container annotations are off there, since vcpkg's libraries are built without them.
- MSVC's static analyser (`debug-analyze`), in its own tree because clang-tidy reads the debug tree's compile database and clang rejects `/analyze`. A finding is fixed, not suppressed, unless the code is a hook doing what hooks do; then the suppression says why.

**Mutation testing** (`tools/mutants.py`, `build.ps1 -Mutants`) is not part of the gate: run it after changing `src/dao/core` or its tests. Each mutant is one promise of the core broken by a small edit, written by hand where a failure would cost in play: a bound on damaged bytes, a size fixed around code put into a movie, an address one off. Each is built and tested in a worker's copy of the tree under `build\mutants`, a worker a core, below normal priority; the working tree is never written. A survivor is a promise no test holds. It gets a test if the promise matters, or the mutant goes, with a line in the tool saying why (the user, 2026-10-03: "make sure tests and mutations earn their keep"). First run, 2026-10-03: 45 of 82 caught; after tests for what mattered and five mutants left out, 77 of 77, in under four minutes on twenty cores.

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

The class names also come from `tools\daoexe.py`'s own walk, which needs no Ghidra. On every launch `tools\ghidra\ImportNames.java` applies the names `tools\names.py` reads from the address library's table, about 4,400 of them:

- everything a `DAO_ADDRESS`/`DAO_DATA` row of `Addresses.h` was read as
- every message handler, as `Owner::HandleMessage_Message`
- every implemented script command, as `ScriptCommand::NAME`
- every vftable, as `Class::vftable`

The handlers' and commands' names are made from the tables under `dev/`; `python tools\names.py --give` names in the address library's table what those tables have come to name since.

It creates a function where analysis found none. A name set by hand in Ghidra wins over all of them.

**MSVC templates** have names like `MemberFunctionMessageHandler<GameModeController,UserTogglePauseMessage,&GameModeController::HandleMessage>`. `tools\msvc_names.py` undecorates them, and `names.py` writes their inner `::` as `.` and their spaces as `_` so Ghidra accepts them. The C++ `dao::TypeName` leaves a template's name decorated rather than guess.

**Recording a finding.** An address goes into `src/dao/Addresses.h` with the engine's name for it, what it is, how it was found, and the date; `tools\address_ids.py` then turns the row into its ID and moves the name to the address library (above). A layout goes into a class header under `src/dao/`, like `ClientExoApp.h`, with the evidence per field. A field a getter command returns goes through `dev/script_fields.csv` and `tools/gen_script_fields.py` instead, and gets its name in `Objects.h`. A hypothesis is tried in the probe plugin before anything relies on it: detour, log what arrives (`dao::TypeName` says what an object is), change nothing.

## Hooking on x86

Member functions are `__thiscall`: `this` in ecx, arguments on the stack, and the callee cleans up. A hook on one is written `__fastcall` with `this` first and an unused `int` second (edx), and the `Fn` type in `dao::Detour<Fn>` is declared the same way; `src/dao/Hook.h` has the pattern. Free functions and statics are `__cdecl` unless the listing shows otherwise; a `ret N` says `__stdcall`. Get this wrong and the stack is off by the arguments' size at the return, so the crash comes somewhere after the hook, not in it.

The loader detours the exe's entry point from `DllMain`, which Detours permits (its own samples do it). Plugins are loaded from the detour, on the process's first thread, with the loader lock released. A plugin's `DAOLib_Load` runs before any game code, so a detour placed there is in place for the first call. No game object exists yet, so load does hooks, not reads (`CClientExoApp::Get()` is null there, verified). The game's own work then runs on other threads, so a hook must not assume which thread calls it.

A hook that wants its caller logs `_ReturnAddress()` rebased to the preferred base, as `src/plugins/probe/Probe.cpp` does. That gives an address to look up with `tools\daoexe.py --disasm` or in Ghidra.

## Style

Formatting is clang-format's stock Microsoft style (`.clang-format`, one line), and lint is correctness, cost and readability, with the checks of taste left out and each exclusion's reason in `.clang-tidy` (the user, 2026-10-05: "We want to write good clean code, avoid complexity"). A function over a cognitive complexity of 25 is split where it has parts with a meaning of their own, not for the number. Neither is up for discussion per file. Comments say why and what was read, with dates, not what the code does. A file's head says what the file is for and how it fits.

**A public header is the API's documentation** (DAOLib #45; the user, 2026-10-09: "accurate, succinct descriptions for the functions and the parameters ... don't ramble"). `tools/build_docs.py` publishes `src/dao/*.h` as a Doxygen site, and `tools/doxygen_filter.py` makes a `//` block directly above a declaration its documentation. That block is: one plain sentence on what the thing does, `@param`/`@return` where the name and type do not say it, then at most a few short sentences a caller needs (thread, when it works, what failure returns). The reading behind it (addresses, dates, play tests, issue numbers) goes beside the definition in the `.cpp`; in a header-only file, as a `// Notes on X:` block followed by a blank line, which the filter leaves off the site.
