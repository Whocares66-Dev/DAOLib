# Plan

A CommonLibSSE-NG-style library for Dragon Age: Origins, plus the SKSE-style loader it needs: native plugins that hook the engine where the toolset's scripting cannot reach. The first use case is [a turn-based mode](TURN_BASED.md).

## The shape

Three deliverables, all in this repository:

1. **The loader**, `version.dll` (`src/loader`). Dropped beside `DAOrigins.exe` it is loaded in place of the system DLL of that name (the game imports three of its functions, forwarded to the real one), detours the exe's entry point, and from there loads every DLL in `daolib\plugins\` on the main thread before any game code runs. The SKSE half.
2. **The library**, `daolib` (`src/dao`). What a plugin compiles against: the address of every known function and global (`Addresses.h`), relocated to where the exe actually is; the fingerprint of the one build those addresses were read from; the class of any engine object from its RTTI; a detour wrapper; a log per DLL. As it grows: class layouts (`struct` per engine class, with the fields at their offsets) and thin wrappers that call the engine's own functions. The CommonLib half. Nothing of the engine is reimplemented; everything is called.
3. **Plugins** (`src/plugins`). The probe, which hooks and logs and changes nothing, and the example, a plugin to copy from. Plugins built on the library live in their own repos: the turn-based mode in `DAOTurnBased`, which records the library's shortcomings for this repo to fix.

## Why DAO is an easier target than Skyrim

| | DAOrigins.exe (GOG 1.05) | SkyrimSE.exe |
|---|---|---|
| Architecture | 32-bit x86, one build | 64-bit, many builds |
| ASLR | off (preferred base 0x400000 kept) | on, hence the Address Library |
| DRM | none | Steam stub, unpacked with Steamless |
| RTTI | ~2375 classes, 2990 vtables, namespaced | ~3150 classes |
| Script layer | 827 numbered engine commands, named in the exe and declared with signatures in the game's `script.ldf` | Papyrus natives |
| Event layer | 314 message handlers, each named with its owner and message in RTTI | events and sinks |

So addresses are fixed per build and there is only one build; there is no unpacking step; and the script command table names 692 engine functions before any reading is done ([SCRIPT_COMMANDS.md](SCRIPT_COMMANDS.md)).

## The order of work

1. **Loader and library skeleton.** Done 2026-09-29.
2. **Prove the chain in play.** Done 2026-09-29. Deployed, the game started, `loader.log` showed the entry point detoured and the probe loaded, and the game ran as before.
3. **First hook.** Partly done, 2026-09-29. The pause key's handler and `CClientExoApp::SetPaused` are found and hooked. The engine called the setter at startup, and the hook read the object's type through RTTI and the flag through `dao::CClientExoApp`. Still to see in `probe.log` during play: the pause key itself, and auto-pause on combat.
4. **Read for the turn-based mode** ([TURN_BASED.md](TURN_BASED.md)).
5. **Grow the library** as the reading does: a header per engine class that a plugin needs, each field with the evidence for its offset.

## Rules

- **The executable is the authority.** An address, an offset or a convention comes from the Ghidra listing of `game\DAOrigins.exe`, with a note saying where it was read, and is verified in play by the probe before anything relies on it. The wiki, the toolset docs and memory are leads, not facts.
- **One build.** Everything is conditional on `dao::Module::IsExpectedExecutable()`; on any other exe the loader loads nothing and says so in its log.
- **Call, do not reimplement.** A wrapper calls the engine's function at its address. Rewriting engine logic in C++ is how a library drifts from the game.
- **Log what a hook read, not only what it did.** A session in play is the test fixture there is no headless substitute for.
- **Testable code lives in `src/dao/core`**, never touches the process, and is covered by `tests/`. Everything else is verified by playing.
