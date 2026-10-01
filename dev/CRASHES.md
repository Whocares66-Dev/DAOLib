# Crash logs

The loader (`src/loader/CrashLog.cpp`) installs a crash logger before anything else runs. On a crash anywhere in the game's process, it writes `crash-<date>-<time>.log` beside the other logs, in `Documents\BioWare\Dragon Age\daolib\` (under OneDrive on this machine), and puts a line pointing to it at the end of `loader.log`. Built 2026-09-30, on the shape of Skyrim's CrashLoggerSSE.

## What a log holds

| Section | What |
|---|---|
| Header | The exe's link stamp, whether it is the build DAOLib knows, the time |
| Unhandled exception | The code by name, the address and what it is, for an access violation the address read or written, and the first bytes of code at the fault |
| SYSTEM | Windows, physical memory, and the process's address space. DAO is 32-bit, so address space runs out long before memory does. |
| PROBABLE CALL STACK | dbghelp's walk of the crashing thread. Exact through our DLLs and Windows, with file and line for ours. It can stop early in the game's own code, which has no frame data. |
| RETURN ADDRESSES ON THE STACK | Every value in the stack's first 64 KiB that is a return address into any module (`dao/core/CallSite.h`). This keeps going where the walk stops: the game's callers, innermost first. A few may be stale values from earlier calls. |
| REGISTERS, STACK | Each value annotated: code by its module and function, an engine object by its RTTI class (`(CCCreature*)`), a string, or a small number |
| MODULES, DAOLIB PLUGINS | Everything loaded, with its range; our plugins |
| LAST LINES OF ... | The last twelve lines of every DAOLib log |

Names come from three places:
- **Our DLLs:** their PDBs, which `tools\deploy.ps1` copies beside them.
- **Windows DLLs:** their exports.
- **The game:** `bin_ship\daolib\functions.tsv`, Ghidra's function list, which deploy also copies (`dev/PROBE.md` says how to export it). With it, `0x0073D054` reads as `FUN_0073d000+0x54` or `GameModeGUI::vfunction7+0x1c`. Without it, the game's addresses stay bare. The names are of the GOG build's addresses. In the EA app's build (`dev/BUILDS.md`) an address in the game is given as that build's own, then as GOG's, by which the name is found: `DAOrigins.exe 0x004c1770 (GOG 0x005d6740) CClientExoApp::SetPaused+0x0`, or `(not in the GOG build)` for code EA's build alone has. Written 2026-10-01 and not yet seen: no crash has been made in the EA game.

## How it stays installed

It is an unhandled-exception filter, and the game's C runtime installs one of its own at start-up. So the logger also detours `SetUnhandledExceptionFilter`. A filter the game sets afterwards is remembered and called after ours, so the game's own handling still happens, and Windows Error Reporting still records the crash. The log is written on a new thread, because the crashing thread may be out of stack. The crashing thread waits up to a minute for it.

## Testing it

With `DAOLIB_PROBE_CRASH_TEST=1` in the game's environment, the probe crashes on purpose on the game's thread, in the first `SetPaused` at start-up. Start the game with the variable set only for that run:

```powershell
$env:DAOLIB_PROBE_CRASH_TEST = '1'; Start-Process "...\bin_ship\DAOrigins.exe" -WorkingDirectory "...\bin_ship"; Remove-Item Env:DAOLIB_PROBE_CRASH_TEST
```

The log from that test (2026-09-30) named `CrashIfTesting` at `Probe.cpp:63`, the hook at `Probe.cpp:87`, the engine's `FUN_005DD310` below it, and the chain down to `Eclipse::UpdateTask` on a `Multithreading::Thread`.

## The crashes it has explained

- **2026-09-30, the probe's queue hook.** `strnlen` in `dao::DecoratedTypeName`, reading `0x84C6CF93`. Its check that a pointer lies inside the exe's image computed `end - address` without first checking `address < end`. An address above the image wrapped the subtraction around and passed. A command object's garbage word took that path. The check is now `dao::core::Contains` (`tests/test_range.cpp`), and the crash before it, in `memcpy`, was almost certainly the same bug.
- **2026-09-30, a creature's properties.** An access violation reading `0x14` at `0x006436EB`, in the engine's property getter, called from `dao::CCCreature::Property` in the example plugin's frame callback; the log named `ESI` a `CCCreatureStats*`. The getter was given the creature's stats, but `GetCreatureProperty` gives it the stats' property list, 0x190 bytes in (`lea ecx, [eax + 0x190]` at `0x0066D002`). On the stats themselves it read another vector, whose entry for strength was `0x104FF`, and called through it. The register naming the class was the clue that the object was right and the offset wrong. `Property` now adds 0x190; the setters take the stats and add it themselves (`0x0071B239`).
