---
title: Loader
nav_order: 2
permalink: /loader.html
---

# The loader

The loader is what a player installs: `dinput8.dll` and `dinput8.pdb` in the game's `bin_ship` folder beside `DAOrigins.exe`, and the address files in `bin_ship\daolib\addresses\`. It changes none of the game's files; deleting what was installed removes it.

## How it loads the game

1. **Windows loads it as `dinput8.dll`.** The game imports DirectInput's `dinput8.dll` by name, and Windows looks in the exe's folder before the system's. The loader passes every DirectInput call on to the system's `dinput8.dll`, so input works as before. Another mod's `dinput8.dll` keeps working renamed `dinput8.chain.dll` beside it: the loader passes the calls to it instead. It was `version.dll` in 0.1.0: a compatibility setting on the exe made Windows answer that name with its own, so it is `dinput8.dll` now ([#93](https://github.com/Whocares66-Dev/DAOLib/issues/93)).

   The loader was `version.dll` in 0.1.0. Delete `bin_ship\version.dll` and `bin_ship\version.pdb` before installing 0.2.0.
   {: .warning }
2. **It finds the build of the game.** The address file named for the exe's headers says which build this is, and where each function and global is in it, so one plugin runs in GOG's, Steam's and EA's games. A repack of a known build (Steamless' output, the Nexus "decrypted Steam" exe, with or without the 4GB flag) is found by a hash of its code instead ([#110](https://github.com/Whocares66-Dev/DAOLib/issues/110)). With no file for the exe, the loader loads nothing and says so.
3. **It waits for the game's start.** The loader detours the exe's entry point, so the plugins load on the game's first thread before any of the game's own code runs. Steam's and EA's exes are wrapped and decrypt their code first; there the loader waits for the C runtime's start, the first moment the game's code is there to hook ([#94](https://github.com/Whocares66-Dev/DAOLib/issues/94)).
4. **It puts in its fixes and performance changes**, below, and the crash logger.
5. **It loads the plugins**, each from a folder of its own, `bin_ship\daolib\plugins\<name>\<name>.dll`, and calls each one's `DAOLib_Load`. A plugin built for another API version is refused. Then the game starts.

What happened is in `Documents\BioWare\Dragon Age\daolib\loader.log`: the version, the build and its address file, and each plugin "loaded". A crash, or a freeze of five seconds or more, leaves a log beside it with the call stack, named by the PDBs.

## Fixes

Faults of the game's own, corrected in every game the loader is in, whatever plugins it has. Always on.

| Fix | |
|---|---|
| Archive reads | Two resources read from one archive at the same moment could each get the other's bytes, which crashed the game in a model's load or left a model out. Each read is now at its own place in the file ([#3](https://github.com/Whocares66-Dev/DAOLib/issues/3)). |
| Saved file names | A screenshot's file name was read a kilobyte past its end, which could crash the game ([#78](https://github.com/Whocares66-Dev/DAOLib/issues/78)). |
| Mouse records | The mouse's movement took its time from a record past the last one read, which could crash the game ([#105](https://github.com/Whocares66-Dev/DAOLib/issues/105)). |

## Performance

Changes that make the game faster and correct no fault. Each can be left out by an environment variable set to `0` before the game starts.

| Change | | Default | Left out by |
|---|---|---|---|
| Engine heap | The engine's memory comes from the system's heap in place of its own 850 MiB of pools, which were slow to search and a ceiling on what the game could use ([#69](https://github.com/Whocares66-Dev/DAOLib/issues/69)). | on | `DAOLIB_ENGINE_HEAP` |
| All processors | GOG's exe starts the game on the machine's first two processors; the loader gives it all of them, as Steam's and EA's builds have ([#92](https://github.com/Whocares66-Dev/DAOLib/issues/92)). | on | `DAOLIB_ALL_PROCESSORS` |
| Fast loading | The areas and rooms the party leaves are kept, the indoor areas beside it are loaded ahead, and the loading screen paces and fades faster: a door between Redcliffe village and its tavern in 0.1 to 0.3 s ([#75](https://github.com/Whocares66-Dev/DAOLib/issues/75), of [#64](https://github.com/Whocares66-Dev/DAOLib/issues/64), [#71](https://github.com/Whocares66-Dev/DAOLib/issues/71), [#72](https://github.com/Whocares66-Dev/DAOLib/issues/72), [#73](https://github.com/Whocares66-Dev/DAOLib/issues/73), [#81](https://github.com/Whocares66-Dev/DAOLib/issues/81), [#83](https://github.com/Whocares66-Dev/DAOLib/issues/83)). | off | `DAOLIB_FAST_LOAD` |

Fast loading is optional: a player turns it on with **Fast loading** in the **Performance** section of the options screen's **Game** tab ([#115](https://github.com/Whocares66-Dev/DAOLib/issues/115)). It takes effect from the next loading screen, and the choice is kept in `daolib.ini` beside the logs.

![The Performance section of the options screen's Game tab, with Fast loading]({{ '/assets/loader-performance.png' | relative_url }})
