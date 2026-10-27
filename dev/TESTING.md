# Testing in the game

What is seen in the game before a merge to `master` and before a release, in each of the three builds players have, and under the Windows compatibility settings they may have ticked. The tests that need no game (`tools\build.ps1 -Test` and the rest of the gate) are `CLAUDE.md`'s "Before merging to master"; this is the part they cannot reach.

Why it is here. 0.1.0 was seen in GOG's and Steam's games on one machine with no compatibility setting, and two players then had a game that ran with no DAOLib in it and no word why (DAOLib #93): with "Disable fullscreen optimizations" ticked on the exe, Windows loaded its own `version.dll` ahead of the loader. The same day the engine heap was found never to have gone in in Steam's game (DAOLib #94). Both were there to be seen by starting each build's game under the setting. The user, 2026-10-12: "encode testing all vendors, all compatibility matrices", then, for the time it takes, "for merge to master test each platform, for release test whole matrix", and "we should support what someone w/ a modern os (windows 10/11) may have (compatibility mode etc.), not 30 yrs old oses".

## What is run, and when

| When | Exes | Settings | Starts | Time |
|---|---|---|---|---|
| Before a merge to `master` that changes what is built (`src\`, `CMakeLists.txt`, `extern\`) | GOG's and Steam's, and EA's with `-EaApp` | none, and Disable fullscreen optimizations | 4, and EA's 2 | about 2 minutes, and EA's 2 minutes |
| Before a release, on the release build that goes into the zip | every kept exe that runs (below), and EA's with `-EaApp` | the whole matrix below | 72, and EA's 12 | about 13 minutes, and EA's 10 minutes |

A merge of notes or tools alone builds nothing into the game and needs neither.

`tools\play_matrix.ps1` does the starting and the checking (DAOLib #146). **It starts the game, eight copies at a time (`-Batch`), and ends them:** say so first. A row is one exe under one setting. The exes are those kept in `C:\Modding\DAOVersions`, each checked against its manifest. Each row is staged in a folder of its own under `build\matrix`: the store's `bin_ship` with the row's exe and `build\<preset>`'s loader, address files and plugins (those that are not examples), hard-linked from one copy of each, and junctions to the install's other folders. A setting is a registry value keyed by the exe's path, so each row's exe has a path of its own. Each row's game is a DAOLib instance, `matrix-<exe>-<n>`, so the rows run together and beside any other game, and each is checked by its own `loader.log`. Its profile is given the default profile's Settings and marked seeded before its first start, so none of the player's saves are copied (0.6 MB each). Nothing is deployed and no install is written; the folders and junctions go at the end, each junction removed as a link first. Each row's exe is a path of its own and the game opens a port, so Windows Firewall asks about each the first time it runs (DAOLib #151): `play_matrix.ps1 -Firewall`, once from an elevated shell, blocks inbound for every row's path, in the rule group `DAOLib play matrix`.

```powershell
.\tools\build.ps1 -Preset debug
.\tools\play_matrix.ps1                                   # a merge: GOG's and Steam's exes; Steam must be running

.\tools\build.ps1 -Preset release
.\tools\play_matrix.ps1 -Preset release -All              # a release: every kept exe, the whole matrix

# EA's exe, after the others, from an elevated shell, deployed first
.\tools\deploy.ps1 -Install ea -Preset release
.\tools\play_matrix.ps1 -Preset release -All -EaApp -SwitchPath
.\tools\deploy.ps1 -Install ea -Remove
```

EA's exe cannot be staged: it hands off to the EA app, which starts the game in the one folder the registry's `Path` names, as the game as ever. So `-EaApp` runs its rows after the others, one at a time, in EA's install, and refuses while the game as ever is up.

A row passes when all of these hold, and the script fails on any that does not:

- the `dinput8.dll` in the process is the row's, not the system's;
- the row's `loader.log`, written by this start, says the game's start was reached: `entry point reached` in an unwrapped exe, `the game's code is running` in a wrapped one;
- the engine heap went in (`the engine's memory is the system's heap`);
- every plugin has its `<name>.dll: loaded` line, and the log names the row's instance;
- no crash, hang or exit log was written;
- after 30 seconds the process is still there, its window the game's own and answering.

It does not play: a start to the main menu is what every setting shares. What a change does in the world is seen as that change needs (`CLAUDE.md`, the issue's own play test), in one build.

## How each build is started

- **GOG:** the exe.
- **Steam:** the exe, with `SteamAppId=47810` and `SteamGameId=47810` in its environment and Steam running. Without them the stub hands off to Steam, which opens the launcher's window and waits for Play.
- **EA:** the exe, which hands off to the EA app, which starts the game under the folder `HKLM\SOFTWARE\WOW6432Node\BioWare\Dragon Age`'s `Path` names. On this machine that is Steam's (Steam's installer writes the key), so `-SwitchPath` sets it to EA's folder for the runs and puts it back; that needs an elevated shell, and is done with the user's leave (2026-10-12).

The matrix, `version_matrix.py` and `ea_play.py` start the game shown but not activated (`SW_SHOWNOACTIVATE`), so it opens behind the front window and takes no keys from it; it is not minimised, which would stop a fullscreen game drawing (DAOLib #198).

All three run as the game as ever: the shared profile, `Documents\BioWare\Dragon Age`, and its `daolib\loader.log`.

## Which exes the loader finds a file for (`tools\version_matrix.py`, 2026-10-13, DAOLib #110)

Each exe is kept in `C:\Modding\DAOVersions\<store>\DAOrigins-<variant>-<version>.exe` (`MANIFEST.sha256` beside them): GOG's, Steam's stock, Steamless' output of it with and without the 4GB flag, the Nexus "decrypted Steam" exe with and without the 4GB flag, and EA's, stock and its decrypted dump. Steamless' output with the 4GB flag was made 2026-10-16 as the Nexus copy has it, the flag set and the checksum left as it was (DAOLib #146). `python tools\version_matrix.py` stages each into the install it belongs to (GOG's, Steam's), with this tree's loader and address files, starts the game with Steam's variables (`SteamAppId=47810`) where the stub wants them, waits for Eluvian's `health` to count frames, loads `AutoSave_3` of Lyna (a fight in Redcliffe) through Eluvian's `saves`, and reads `loader.log` and `turnbased.log`. It ends the game after each, and puts back the stock exe (checked by hash), the loader and the address files. THIS STARTS THE GAME; have none open. It needs Eluvian and turnbased in each install's `daolib\plugins`.

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

2026-10-13, the release build of 0.2.0 (#114, `f857ac1`), each build's game through `play_matrix.ps1 -All` after `deploy.ps1 -Preset release`, EA's with `-SwitchPath`; `loader (daolib 0.2.0)` and each build's own address file in every `loader.log`:

| Build | Settings | Result |
|---|---|---|
| GOG | all twelve, with `elgarashaders`, `eluvian`, `example`, `probe` and `turnbased` | every one passed |
| Steam | all twelve, with `example` and `probe` | every one passed |
| EA | all twelve, with `example` and `probe` | eleven passed; `WINXPSP2` kept no game up, twice, with the loader never run (`loader.log` not rewritten, no fault log). Vanilla EA, no `dinput8.dll` in the folder, kept none up under `WINXPSP2` either, so it is EA's exe or the EA app under that layer and not DAOLib. It passed on 2026-10-12 (above); what changed is not known |

The same day the three repacks through `tools\version_matrix.py --preset release` (Steamless' output, the Nexus exe, and the Nexus exe with the 4GB flag): each found GOG's file by its code, made the file of its own headers, and loaded a save into combat in Redcliffe with `turnbased` ready.

2026-10-16, the release build of 0.3.1 (`ee905f9`), the last run one setting at a time: GOG's (the private copy) and Steam's games, all twelve each with `example` and `probe`, every one passed; then Lyna's Redcliffe save loaded with the zip's `dinput8.dll` and Eluvian. EA's was left out (the user, that day: "Just skip ea").

2026-10-16, the matrix side by side (#146), the debug build of `gh-146-parallel-matrix`, `example`, `probe` and `sparelist`, beside two other sessions' instances: the merge's four rows passed together; then the whole matrix, the six kept exes under all twelve settings, 72 rows. Read once at 30 s from a batch's last start, 15 rows of 72 failed at 12 a batch, each "its window does not answer", each among the last four started, with no fault log: a dozen copies starting at once reach the menu later than one does. Read until the window answers, up to 60 s more, all 72 passed at 12 a batch (11.4 minutes, windows answering by 40 to 64 s, median 54) and at 8 (13.1 minutes, 30 to 61 s, median 48). 8 is the default (the user: "You should probably run no more than eight at a time"). The staged folders and junctions were gone afterwards, the installs' folders whole, no layer left in the registry, and each instance's profile 0.6 MB.

2026-10-22, the release build of 0.6.0 (#235, `9d27b65`, the address library at its 0.6.0, `78c0006`), `play_matrix.ps1 -Preset release -All` with `example` and `probe`, 8 at a time, then EA's with `-EaApp -SwitchPath` after `deploy.ps1 -Install ea -Preset release`, and `-Remove` after:

| Build | Settings | Result |
|---|---|---|
| GOG, Steam, Steamless, Steamless with the 4GB flag, the Nexus exe, and it with the 4GB flag | all twelve | every one of the 72 passed, twice (`-All -EaApp` runs the staged rows again first); windows answered at 30 to 52 s |
| EA | all twelve | five passed: no setting, `DISABLEDXMAXIMIZEDWINDOWEDMODE`, `HIGHDPIAWARE`, `DPIUNAWARE` and `GDIDPISCALING DPIUNAWARE`. The seven compatibility modes, `WINXPSP2` to `WIN8RTM`, kept no game up, with no fault log. Run again under `WIN7RTM` and `WINXPSP3`, the 0.6.0 loader and 0.5.0's (the 0.5.0 zip's `dinput8.dll` and address files) failed alike, and neither wrote a `loader.log`: the game did not get to the loader, so it is EA's exe or the EA app under those layers and not DAOLib, as `WINXPSP2` was at 0.2.0 |

Then in play, the zip's `dinput8.dll`, `dinput8.pdb` and address files in GOG's private copy (`game\gogrun`, instance `lib-agent`), with `eluvian` and the release build's `example` and `probe`: `loader (daolib 0.6.0)`, "address library: 103187 IDs for this build", each plugin "loaded", the main menu, and Lyna's Redcliffe autosave loaded into explore. `DAOLIB_PROBE_CRASH_TEST=1`: a log of 370 lines, 865 of 4096 MiB in use, its first frame `probe.dll+22486 SetPaused+0xe6 (Probe.cpp:156)` and then the engine's `FUN_005dd310+0x3f`. `=2`: 373 lines, 4089 of 4096 MiB in use and "All but full", the same frames. The zip's three address files and its index are DAOAddressLibrary 0.6.0's zip's byte for byte.

2026-10-24, the debug build of master at `e44851a` (after #255 to #258), `play_matrix.ps1 -EaApp -SwitchPath` with `example` and `probe`: GOG's and Steam's four rows passed, and EA's with no setting. EA's `DISABLEDXMAXIMIZEDWINDOWEDMODE` row failed once, "the game ended by itself; no loader.log written", so the game was gone before the loader ran; both EA rows passed when run again at once (`-Exes @() -EaApp -SwitchPath`). Not reproduced; taken as the EA app's start, not the build's.
2026-10-27, two unexplained closes (lib-agent-3, #259): lib-agent-3's own instance (game\gogrun) closed twice, at 13:09:40 and 13:13:08, each a minute or two into an origin after character creation. Each time `loader.log` has the window activated, Alt keydowns within 2 s, then `WM_SYSCOMMAND SC_CLOSE` (lParam 0x768, then 0x77f) and `WM_CLOSE` sent by the game's own thread, and `ExitProcess(0)`: a close by Alt+F4 or the window menu, not a crash. Every agent playing then said it sent no keys or close. elgara-agent3's Eluvian test build may have handed the front to the window (activation only, since changed not to). The cause is not known; the same run at 13:37 went untouched.
