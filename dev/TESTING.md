# Testing in the game

What is seen in the game before a merge to `master` and before a release, in each of the three builds players have, and under the Windows compatibility settings they may have ticked. The tests that need no game (`tools\build.ps1 -Test` and the rest of the gate) are `CLAUDE.md`'s "Before merging to master"; this is the part they cannot reach.

Why it is here. 0.1.0 was seen in GOG's and Steam's games on one machine with no compatibility setting, and two players then had a game that ran with no DAOLib in it and no word why (DAOLib #93): with "Disable fullscreen optimizations" ticked on the exe, Windows loaded its own `version.dll` ahead of the loader. The same day the engine heap was found never to have gone in in Steam's game (DAOLib #94). Both were there to be seen by starting each build's game under the setting. The user, 2026-10-12: "encode testing all vendors, all compatibility matrices", then, for the time it takes, "for merge to master test each platform, for release test whole matrix", and "we should support what someone w/ a modern os (windows 10/11) may have (compatibility mode etc.), not 30 yrs old oses".

## What is run, and when

| When | Builds | Settings | Starts | Time |
|---|---|---|---|---|
| Before a merge to `master` that changes what is built (`src\`, `CMakeLists.txt`, `extern\`) | GOG, Steam, EA | none, and Disable fullscreen optimizations | 6 | about 5 minutes |
| Before a release, on the release build that goes into the zip | GOG, Steam, EA | the whole matrix below | 36 | about 25 minutes |

A merge of notes or tools alone builds nothing into the game and needs neither.

`tools\play_matrix.ps1` does the starting and the checking; it runs the two settings of a merge unless told `-All`. **It starts the game, once per setting, and ends it:** say so first, and run it when no game as ever is up (other sessions' instances beside it are fine; it refuses while the game's own mutex is held).

```powershell
# GOG, from the private copy, so the shared install's files are not touched (others hold them open)
.\tools\deploy.ps1 -Game game\gogrun\bin_ship
.\tools\play_matrix.ps1 -Game game\gogrun\bin_ship

# Steam; Steam itself must be running
.\tools\deploy.ps1 -Install steam
.\tools\play_matrix.ps1 -Install steam
.\tools\deploy.ps1 -Install steam -Remove

# EA, from an elevated shell
.\tools\deploy.ps1 -Install ea
.\tools\play_matrix.ps1 -Install ea -SwitchPath
.\tools\deploy.ps1 -Install ea -Remove
```

For a release add `-All` to each, with `-Preset release` on the deploys. The private copy keeps Eluvian in its plugins; move `daolib\plugins\eluvian` aside for the run, or a game as ever answers on Eluvian's port while it is up.

A row passes when all of these hold, and the script fails on any that does not:

- the `dinput8.dll` in the process is the one beside the exe, not the system's;
- `loader.log`, written by this start, says the game's start was reached: `entry point reached` in GOG's build, `the game's code is running` in a wrapped one;
- the engine heap went in (`the engine's memory is the system's heap`);
- every plugin folder in the game has its `<name>.dll: loaded` line;
- no crash, hang or exit log was written;
- after 30 seconds the process is still there, its window the game's own and answering.

It does not play: a start to the main menu is what every setting shares. What a change does in the world is seen as that change needs (`CLAUDE.md`, the issue's own play test), in one build.

## How each build is started

- **GOG:** the exe.
- **Steam:** the exe, with `SteamAppId=47810` and `SteamGameId=47810` in its environment and Steam running. Without them the stub hands off to Steam, which opens the launcher's window and waits for Play.
- **EA:** the exe, which hands off to the EA app, which starts the game under the folder `HKLM\SOFTWARE\WOW6432Node\BioWare\Dragon Age`'s `Path` names. On this machine that is Steam's (Steam's installer writes the key), so `-SwitchPath` sets it to EA's folder for the runs and puts it back; that needs an elevated shell, and is done with the user's leave (2026-10-12).

All three run as the game as ever: the shared profile, `Documents\BioWare\Dragon Age`, and its `daolib\loader.log`.

## Which exes the loader finds a file for (`tools\version_matrix.py`, 2026-10-13, DAOLib #110)

Each exe is kept in `C:\Modding\DAOVersions\<store>\DAOrigins-<variant>-<version>.exe` (`MANIFEST.sha256` beside them): GOG's, Steam's stock, Steamless' output of it, the Nexus "decrypted Steam" exe with and without the 4GB flag, and EA's, stock and its decrypted dump. `python tools\version_matrix.py` stages each into the install it belongs to (GOG's, Steam's), with this tree's loader and address files, starts the game with Steam's variables (`SteamAppId=47810`) where the stub wants them, waits for Eluvian's `health` to count frames, loads `AutoSave_3` of Lyna (a fight in Redcliffe) through Eluvian's `saves`, and reads `loader.log` and `turnbased.log`. It ends the game after each, and puts back the stock exe (checked by hash), the loader and the address files. THIS STARTS THE GAME; have none open. It needs Eluvian and turnbased in each install's `daolib\plugins`.

It expects of each: the file the loader read (GOG's by its headers for GOG's exe, Steam's own for the stock exe, GOG's by the exe's code for the other three), Eluvian's frames running, turnbased ready, and the world loaded.

Seen 2026-10-13, all five ok: GOG's exe, Steam's stock exe (wrapped, then decrypted), Steamless' output (`SizeOfImage` `0x9ed000`), the Nexus exe and the Nexus exe with the 4GB flag (`0x96a000`), each into combat in Redcliffe with the turn-based mod ready and Eluvian answering. For the last three the loader logged the code match and made the header-named file, which is how turnbased and Eluvian (built before the code index) found their addresses. EA's exe is not in it: its folder is not writable, and the EA app starts the game named by the registry's `Path`, which is Steam's folder here. With `HKLM\SOFTWARE\WOW6432Node\BioWare\Dragon Age`'s `Path` set to `C:\Program Files\EA Games\Dragon Age` for the run (the user's leave, 2026-10-13; put back after, checked) `tools\ea_play.py --plugins <turnbased and Eluvian folders>` started EA's stock exe: its own file (103567 IDs), `wrapped`, `decrypted`, turnbased and Eluvian loaded, frames running, and the same save into combat with 4 hostile creatures.

## The settings

A setting on the exe's Compatibility tab is a layer: a value named by the exe's path under `HKCU\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers`, which is how the script sets one and takes it out. What matters to the loader is which shim DLLs Windows loads for the layer, before the exe's own imports and from the system folder with everything they import (read on Windows 10 19045, 2026-10-12):

| Setting on the tab | Layer | Shim DLLs | In |
|---|---|---|---|
| none | | none | merge, release |
| Disable fullscreen optimizations | `DISABLEDXMAXIMIZEDWINDOWEDMODE` | `AcGenral`, `AcLayers` | merge, release |
| High DPI scaling override: Application | `HIGHDPIAWARE` | none | release |
| High DPI scaling override: System | `DPIUNAWARE` | none | release |
| High DPI scaling override: System (Enhanced) | `GDIDPISCALING DPIUNAWARE` | none | release |
| Compatibility mode: Windows XP SP2, XP SP3 | `WINXPSP2`, `WINXPSP3` | `AcLayers` | release |
| Compatibility mode: Windows Vista, SP1, SP2 | `VISTARTM`, `VISTASP1`, `VISTASP2` | `AcLayers` | release |
| Compatibility mode: Windows 7, 8 | `WIN7RTM`, `WIN8RTM` | `AcLayers` | release |

`AcGenral.dll` imports `VERSION.dll` and `WINMM.dll`, which is what took the system's `version.dll` ahead of a loader of that name; no shim DLL imports `dinput8.dll` (`dev/BUILDS.md`, "Windows loads its own version.dll first"). The two settings of a merge are the two ends: no shim at all, and the layer that brings both shim DLLs. GOG's installer sets `HIGHDPIAWARE` on its own exe, so GOG's players have that one without asking.

Left out, each for its reason:

- **Reduced colour mode (8-bit, 16-bit) and 640 x 480** (`256COLOR`, `16BITCOLOR`, `640X480`): they change the desktop's own display mode while the game is up, on a machine other games are running on. Which DLL the layers load was seen without the game (a 32-bit exe importing `dinput8.dll` with the loader beside it, `dev/BUILDS.md`): the one beside the exe under all three, `256COLOR` bringing `AcGenral`.
- **Run this program as an administrator** (`RUNASADMIN`): it asks at a prompt. Not seen, with or without the game.
- **Disable visual themes** (`DISABLETHEMES`): Windows 10 and 11 no longer offer it. It brings `AcGenral`, as Disable fullscreen optimizations does.

Not reachable here: **Windows 11.** This machine is Windows 10. One of the two players who reported #93 has Windows 11 (26H2), and what its shim DLLs import was not read.

## Seen

2026-10-12, the debug build of #93 and #94 together (`try-93-94`, the loader as `dinput8.dll`, a wrapped build's start at the CRT's), with the plugins `example`, `probe` and `sparelist`:

| Build | Settings | Result |
|---|---|---|
| GOG (the private copy) | all twelve, and `DISABLETHEMES` | every one passed |
| EA | all twelve, and `DISABLETHEMES` | every one passed |
| Steam | the two of a merge through the script; all twelve and `DISABLETHEMES` by hand the same hour (the loader loaded, the start's cue, the engine heap, the plugins, no fault log) | every one passed |
