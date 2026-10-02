# Builds

Dragon Age: Origins Ultimate Edition is sold by GOG, EA and Steam, all at 1.05.13263.0, and they are not one executable. What each is, how they differ, and where every function and global is in each belongs to the address library, `extern\DAOAddressLibrary` (its `dev\BUILDS.md`). This is how DAOLib works in more than one of them. Read 2026-10-01 ([issue 1](https://github.com/Whocares66-Dev/DAOLib/issues/1)).

| Build | State |
|---|---|
| GOG | The one everything was read in: the Ghidra project, the names, every address in a comment. Verified in play. |
| EA app | Known to the address library. Seen in play 2026-10-01 with the loader put in from the build tree ("Seen in play"). Not yet run deployed. |
| Steam | `steam-1.05`, read 2026-10-01 in the address library (its `dev/STEAM.md`): GOG's code byte for byte, every GOG ID at its GOG address, wrapped by SteamStub 2.1, and with GOG's link stamp. Below. |

## IDs, not addresses

The library does not hold addresses. It holds IDs: numbers the address library gives, one for every function and global of the game, that mean the same thing in every build. `dao/Addresses.h` is rows of them, and at run time each is looked up in the file for the build that is running.

```cpp
DAO_ADDRESS(kCClientExoApp_SetPaused, 10547)
```

The row is the ID and nothing else about the function. What the engine calls it (`CClientExoApp::SetPaused`) and where it is in each build (`0x005D6740` in GOG's, `0x004C1770` in EA's) are in the address library's table, `extern\DAOAddressLibrary\data\addresses.csv`, and only there: one name for a thing wherever it is spoken of, and no address the code uses written twice (the comments in `Addresses.h` still tell their stories in GOG's addresses, for reading against Ghidra). `python tools\address_ids.py 10547` says them, with what the library notes about the ID in each build, and takes a constant, an address of GOG's or part of a name as well. `tools\names.py` gives the table's names to the Ghidra project.

- **Adding one.** Read the address in GOG's Ghidra project as before, and write the row as it was read: `DAO_ADDRESS(kName, "Engine::Name", 0x005D6740)`, or `DAO_VFTABLE(0x00B007D0);` inside a message class's struct. Then run `python tools\address_ids.py`: it finds the ID, writes the name into this checkout's copy of the address library's table (a vftable has its from the exe's RTTI), leaves the row as the ID alone, and says if the ID is `changed`, `shared` or `ambiguous` in a build. The name is a change to the address library, which is kept by its own session and not committed to from here: send that session the line the tool prints, and when it has committed the name, check its commit out in the submodule and commit the submodule with the row (`tools\address_ids.py`'s head has the steps). Until the tool has run the row does not compile, and a ctest fails on one left over.
- **An address with no ID.** IDs are for what something refers to: a function's start, a global. A place inside a function is that function's ID plus an offset; the tool says which. In a function the address library marks `changed` in a build, an offset into it is GOG's, and may be wrong there (`dao::Module::Find`).
- **At run time.** `dao::Module::Address(id)` is the address in the running process, zero where the running build has no such thing; `dao::At` gives a null pointer for that and `dao::Detour::Attach` refuses it. `dao::Module::Preferred(runtime)` goes back to GOG's address, so the logs, the probe's call chains and the crash logger's names read the same whichever build ran.
- **The generated tables.** The message handlers (`dao/generated/MessageHandlers.h`) and the probe's census list are generated with IDs from the same table; the script commands need none, since the engine's own table gives their functions at run time.

## The files

The address library's build packs its table into one file per build of the game, `daoaddresses-<link stamp>-<image size>-<entry rva>-<base>.bin`, in `build\<preset>\daolib\addresses\`. `tools\deploy.ps1` puts them in `bin_ship\daolib\addresses\`, beside the plugins. The loader and each plugin read the one named for the running exe's four header fields (`dao::Module::Build`), and check that the header inside says the same (`dao::Module::Library`). All four, since Steam's build has GOG's link stamp. The library also still writes GOG's and EA's files under their first names, by the link stamp alone; nothing here reads those any more.

**A build with no file is a build nothing is done in**: the loader logs which file it wanted and loads no plugin. So which builds the library works in is not compiled into anything. A new build of the game needs a new file in that folder, and neither the loader nor a plugin rebuilt. The GOG build's file goes with the others whichever build is installed, since `Preferred` reads it.

`LoaderInfo::knownExecutable` says whether there is a file, and `kPluginApiVersion` is 2: a plugin built against 1 holds addresses, and the loader's version check refuses it.

## What an ID cannot do

All in the address library's `dev\BUILDS.md`; what matters to a plugin:

- **A field offset is not an address.** Three classes are laid out differently in EA's build: `ContentManager` (0x10 bytes inserted at +0x17C), `GUI::ScaleformManager` (0x10 inserted at +0x190) and the `AnimBaseWield`/`AnimBaseParts`/`AnimBaseHead` family (fields at +0x224 and up 0x10 lower). The library reads `GUI::ScaleformManager` at +0x18, +0x44, +0x70 and +0xA8 (`dao/Gui.h`), all below the insertion, and none of the others. A field added for one of these needs an offset per build.
- **98 functions are not the same code in EA's build**, the DLC's online check among them. A detour at the start of one still catches it. Of what the library names, `GUI::ScaleformManager::Initialize` is one.
- **Identical functions are folded differently by each build's linker.** A detour on a small generic function (a getter, a destructor) catches a different set of callers in each build. Of what the library names, `std::wstring::~wstring` and the vftables of `SmartData::Attribute<int>` and `<bool>` are such, none of them hooked.

## The loader in a wrapped build

EA's exe is wrapped: `.text` and `.data` are encrypted on disk, and its entry point is a jump into `Core\Activation.dll`, which checks the licence, decrypts them and goes on to the game's own entry point. The headers in memory stay the file's, so the address library's file for the build carries the wrapper's entry point, and the game's own is an ID like any function (`dao::addr::kEntryPoint`).

In GOG's build the loader detours the entry point from `DllMain` and loads the plugins there. Where the entry point in the headers is not the game's, that is a wrapper's jump, the game's code is still encrypted, and a plugin's hooks would be written into bytes about to be decrypted over. There the plugins load at the first call the game's own code makes: the CRT's start reads the clock for its stack cookie before anything else (`__security_init_cookie`, the entry point's first call), so the loader detours `GetSystemTimeAsFileTime` and loads the plugins the first time its caller is inside the exe's `.text`. That is the same moment as GOG's, one call later, on the same thread. The loader does not know EA's build by name: it knows a wrapped one by its entry point.

`daorigins.exe` started by anything but the EA app decrypts nothing: it asks the EA app to start the game and exits. That run never gets there, and loads nothing.

The loader looks for `daolib\plugins` and `daolib\addresses` beside itself, which deployed is beside the exe.

## Seen in play

2026-10-01, the EA game started through the EA app with `python tools\ea_play.py`, which loads `build\debug\version.dll` into the run the EA app starts, more than a second before the wrapper has decrypted, and copies nothing into the game (the EA app's folder gives the user no write access).

**On IDs, both builds, the same morning.** The library as it is now, each build for 150 seconds in the same save (Lyna's autosave at the Elven Ruins, loaded by `examples\resume`), with the probe and the example:

| | GOG | EA |
|---|---|---|
| How it was run | a copy of GOG's `bin_ship` with the loader deployed beside the exe (`deploy.ps1 -Game`), so the install the other sessions use was not touched | `ea_play.py --force-load` |
| `loader.log` | `link stamp 0x4eb19d53 ... entry rva 0x4e3024`, `address library: 103293 IDs for this build`, `entry point detoured` | `link stamp 0x52164c37 ... entry rva 0x1000`, `address library: 103567 IDs for this build`, `the game's code is wrapped; plugins load when it first runs`, then `the clock read from 0x8cac01` |
| Census | 871 of 872, none refused | 871 of 872, none refused |
| Script checks | 18 of 18 getters and 18 of 18 value-type commands shadowed | the same |
| First `SetPaused` | `from 0x005dd34f`, `CClientExoApp` | `from 0x005dd34f`, `CClientExoApp` |
| In the world | 2 s after it was asked for | 8 s, the save's missing content forced past |
| Checks in play | all agree: `GetGroupId` 42, `GetCombatState` 17, `GetTag` 26, `GetEventType` 710, `GetCurrentEvent` 684, `GetEffects` 20, `api::GetObjectType` 111 | all agree: `GetGroupId` 42, `GetCombatState` 18, `GetTag` 26, `GetEventType` 702, `GetCurrentEvent` 676, `GetEffects` 20, `api::GetObjectType` 114 |
| Warnings, mismatches, crashes | none | none |

Both were run again at 11:06, the same way, once master's work of that morning was in (a plugin's keys, its section in the options screen, a movie patched as it loads, the world on the screen), with `examples\input` and `examples\options` beside the rest: five plugins loaded in each, the census and the checks as in the table, and in both builds the input example's two actions registered (`DAOLibExamplePing: registered; DAOLibExampleUnbound: registered`) and the options example's section added. Nobody opened the options screen or pressed a key in either run.

**Played by hand in both, that evening.** The play tester ran the same save (Lyna's second autosave) in each game with the real keyboard and mouse, GOG's with the branch deployed (17:10) and EA's through `ea_play.py` (18:23), with the examples input, options, world and preview, the example plugin and the probe: six plugins in each. The EA game did what the GOG game did in every step:

- **Start-up.** The lines of the table above in each, the census on 871 of 872, no warning or error in any log, no crash log.
- **The world on the screen** (`examples\world`). The same to the pixel: the view 1600 by 1000, both creatures' feet and click points at the same pixels, the same rooms, the same picks.
- **A function added through the IDs that day**, `GUI::ScaleformManager::MouseOverInterface` (ID 6593): over and off as the cursor was, in both.
- **A plugin's keys.** F11 logged its line; the controls screen showed both headings; a key bound to the empty row there worked in the game.
- **A plugin's section in the options screen.** The gold bar, the three boxes, a click kept and a click refused; the pictures of the two games cannot be told apart. So the patched options movie, the token observer and the delivery hook work in EA's build, `GUI::ScaleformManager`'s larger layout notwithstanding.
- **The preview's ring** placed and moved: the same effect ids in the same order.
- **A walk and a pause.**

The save needed Force Load in the EA game, which asks twice; the EA game wrote no save. Not covered: cutscenes and movies, pathfinding across doors, cooldowns, the camera examples, a fight, and the plugins of other repositories, which were still built against the library before IDs.

The rest of this section is of the night before, when the library still mapped GOG's addresses to EA's through a compiled-in table made by the same comparison the address library's EA column comes from. What it shows of the game and of the wrapper holds as it stands.

- `DAOrigins.exe at 0x400000: link stamp 0x52164c37, image size 0x96b000, entry rva 0x1000`: the wrapper leaves the headers in memory as they are on disk.
- `the game's code is running, decrypted (the clock read from 0x8cac01); loading plugins`, 1.4 s after the loader came in. 0x008CAC01 is the return address of `__security_init_cookie`'s first call, as read from the image.
- The probe placed its seven detours and the three watches, and its census hooked 871 of 872 script commands and message handlers, none refused.
- `script checks: 18 of 18 getters shadowed` and `18 of 18 value-type commands shadowed`, four seconds in, when the engine had filled its command table.
- `SetPaused(false, false) from 0x005dd34f: ... app CClientExoApp at 0x18068120`: the call `CLAUDE.md` records for GOG's build, its caller given as GOG's address (EA's is 0x004C978F), the object's class read from its RTTI.
- `frames: the update runs on thread 73560, scripts on thread 73560`, and the main menu's movies opening (`Tooltips`, `PopupLayer`, `startmenu2`), each call into the game from Flash heard by the example with `manager GUI::ScaleformManager`.

Then in the world, 150 seconds in a loaded save (`python tools\ea_play.py --for 150 --examples resume --force-load`; Lyna's autosave at the Elven Ruins, loaded by `examples\resume` with no menu touched, in the world 8 s after it was asked for):

- Every check the probe makes in play agreed, with no mismatch and no warning. The fields behind the getters the scripts called (`GetPackageAI` 43 of 43, `GetGroupId` 40 of 40, `GetCombatState`, `GetTeamId`, `GetCombatantType`), each with the object's id, type byte and cast checked; the other value types (`GetTag` 25, `GetLocation` 101, the effects' four, `GetEffects` 216, `GetTeam` 4); the events (`GetCurrentEvent` 1,046, `GetEventType` 1,075, `GetEventCreator` 495); and commands called through the library (`api::GetObjectType` 95, `api::GetStringLength` 25, `api::GetPositionFromLocation` 101). Five of the eighteen getters were called in that time, as few are when nothing is fought or opened.
- The mode controller's array holds the same 13 classes at the same numbers, `SaveGame::FindSave` was heard with its slot, `SetPaused` around the load came from 0x006C77BE and 0x006C8365, and `CCObject::QueueCommand`'s callers read `0x0073d054 via 0x006ab8ca 0x006be889 0x004c2afe`: GOG's addresses, from return addresses on EA's stack.
- 6,604 frames, 404 events through the queue, 73 threads sampled, no crash.

**The wrapper finds VERSION.dll by name.** After the game had started, its own first import from `VERSION.dll` pointed into the loader's file (`ea_play.py` reads the slot and prints it). So the wrapper, resolving the game's imports, asked for `VERSION.dll` by name, as the system's loader does, and got the module of that name already in the process. That lookup, with none loaded, takes a `version.dll` beside the exe before the system's, which is all a deployed loader needs.

**The game runs one at a time.** A second `DAOrigins.exe`, of either install, exits a few seconds after starting with 0xffffffff and no crash: the first try, made while the GOG game was up, ended so, as a second GOG game had an hour before. `ea_play.py` refuses while any is running, or waits (`--when-free`).

**The two installs share their saves and do not have the same content.** Both keep saves in `Documents\BioWare\Dragon Age`, and GOG's Ultimate Edition has the Collector's Edition's items where the EA app's has not: the add-in `DAO_PRC_PROMO_C2`, `addins\dao_prc_promo_c2` in GOG's folder. Read 2026-10-01 from a save's own list (`SavegameAddinsList`, `tools\gff4.py --dump`): 39 add-ins, all BioWare's, and that one alone not in EA's `addins`. A save made in the GOG game, asked for in the EA game, gets "This saved game cannot be loaded. It requires the following content: Collector's Edition", with Force Load and Cancel, and the game waits. That is the game's, not the library's. `dao::saves::ForceLoad` is the Force Load button (the dialog's answer, `GUIDialogResultMessage` with 3, to the application), for a tool that drives the game; `examples\resume` uses it only where a file `example_resume.force` says a check is running unattended, which `ea_play.py --force-load` writes.

## Steam

Read 2026-10-01 in the address library (`extern\DAOAddressLibrary\dev\STEAM.md`), from the install at `E:\SteamLibrary\steamapps\common\Dragon Age Ultimate Edition\bin_ship`. Unwrapped, Steam's exe is GOG's: the whole `.text` is the same bytes, and every one of GOG's IDs is at its GOG address, so nothing here reads differently in it. What differs is the wrapper, SteamStub 2.1: `.text` is encrypted on disk, a section of its own (`.bind`) holds the stub, and the headers' entry point is the stub's (`0x9ed2ee`, image size `0xa49000`). And the link stamp is GOG's, `0x4eb19d53`, which is why the address files are named by all four header fields (above).

The loader needs nothing of its own for it. Its test for a wrapped build, the entry point's ID against the headers' entry point, says wrapped, and it waits for the game's own code's first call, as in EA's build.

**Started once, 2026-10-01 23:18**, by the session that read the address library's Steam column at the user's request, from a DAOLib worktree of its own with the same changes (`C:\project\DAOAddressLibrary\game\daolib-steam`, branch `feat/steam-platform`), deployed into the Steam folder with the probe and two examples. `loader.log`:

- `DAOrigins.exe at 0x400000: link stamp 0x4eb19d53, image size 0xa49000, entry rva 0x9ed2ee`: the stub leaves the headers in memory as they are on disk.
- `address library: 103528 IDs for this build, from ...\daoaddresses-4eb19d53-00a49000-009ed2ee-00400000.bin`
- `the game's code is wrapped; plugins load when it first runs`, then 0.3 s later `the game's code is running, decrypted (the clock read from 0x8e21bb); loading plugins`: the CRT's first call, at the address GOG's listing has it.
- `example.dll`, `example_resume.dll`, `probe.dll`: `loaded`; `starting the game`.

Not yet seen: anything in the game through the plugins, the hooks' first calls, a fight. `tools\deploy.ps1 -Install steam` puts a build there; the Steam library's folder is writable, unlike EA's.

## What is left

- **A deployed run in EA's build.** `.\tools\deploy.ps1 -Install ea`, then the game from the EA app. The EA app's folder gives the user no write access; it is granted once from an elevated shell (the script's help has the line). What only this can show is the last step of "The wrapper finds VERSION.dll by name": that with the loader beside the exe and not yet in the process, it is the one found. `ea_play.py` puts the loader in itself.
- **More of the game.** The in-world run stood in one place. A fight, a conversation, a chest and a save made are what the remaining getters and the examples want, in the EA build as they did in GOG's.
- **Steam, played.** One start so far (below); nothing yet in the game through the plugins.
