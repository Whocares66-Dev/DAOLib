---
title: Loader
nav_order: 2
permalink: /loader.html
---

# The loader

The loader is `dinput8.dll` in the game's `bin_ship` folder, with the address files in `bin_ship\daolib\addresses\`. It changes none of the game's files.

## Requirements

- The latest Microsoft Visual C++ Redistributable, the **x86** one (`vc_redist.x86.exe`) even on 64-bit Windows, from [Microsoft's download page](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist). Without it, or with an older one, the game does not start and Windows reports error 0xc0000142.

## How it loads the game

1. Windows loads it in place of DirectInput's `dinput8.dll`, and it passes every DirectInput call on to the real one.

   The loader was `version.dll` in 0.1.0. Delete `bin_ship\version.dll` and `bin_ship\version.pdb` before installing 0.2.0.
   {: .warning }
2. It finds the build of the game, GOG's, Steam's or EA's, from the address files ([#110](https://github.com/Whocares66-Dev/DAOLib/issues/110)).
3. Before the game's own code runs, it puts in its fixes and performance changes, below, and the crash logger.
4. It loads the plugins, `bin_ship\daolib\plugins\<name>\<name>.dll`.
5. It loads the ASI mods, `*.asi` in `bin_ship`, `bin_ship\scripts\` and `bin_ship\plugins\` ([Compatibility]({{ '/compatibility.html' | relative_url }})).

The log is `Documents\BioWare\Dragon Age\daolib\loader.log`. A crash or a freeze leaves a log beside it.

## Fixes

Always on.

| Fix | |
|---|---|
| Archive reads | Two resources read from one archive at once could get each other's bytes, which crashed the game ([#3](https://github.com/Whocares66-Dev/DAOLib/issues/3)). |
| Saved file names | A screenshot's file name was read past its end, which could crash the game ([#78](https://github.com/Whocares66-Dev/DAOLib/issues/78)). |
| Mouse records | The mouse's movement was timed from a record past the last one read, which could crash the game ([#105](https://github.com/Whocares66-Dev/DAOLib/issues/105)). |
| Flickering lights | Fires and torches jumped from one brightness to the next many times a second; they now ease between them, about once a second ([#150](https://github.com/Whocares66-Dev/DAOLib/issues/150)). |

## Performance

| Change | | Default | Off with |
|---|---|---|---|
| Engine heap | The engine's memory from the system's heap, not its own 850 MiB of pools ([#69](https://github.com/Whocares66-Dev/DAOLib/issues/69)). | on | `DAOLIB_ENGINE_HEAP=0` |
| All processors | The game on every processor; GOG's exe starts it on two ([#92](https://github.com/Whocares66-Dev/DAOLib/issues/92)). | on | `DAOLIB_ALL_PROCESSORS=0` |
| Fast loading | Areas the party leaves are kept, nearby indoor areas are loaded ahead, and the loading screen is quicker ([#75](https://github.com/Whocares66-Dev/DAOLib/issues/75)). | off | `DAOLIB_FAST_LOAD=0` |

Turn fast loading on with **Fast loading** under **Performance** on the options screen's **Game** tab ([#115](https://github.com/Whocares66-Dev/DAOLib/issues/115)).

![The Performance section of the options screen's Game tab, with Fast loading]({{ '/assets/loader-performance.png' | relative_url }})
