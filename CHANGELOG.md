# Changelog

## Unreleased

- **Dynamic occlusion culling,** on by default in the options screen's Performance section above Fast loading: the game no longer waits for the graphics card's answer to an occlusion query when the same object's answer from the frame before is there, and after each area's load it keeps occlusion culling only where that is faster (#281). Denerim's market +11%; the Brecilian Forest +47%, where it turns occlusion culling off.

## 0.8.0

For Dragon Age: Origins 1.05.13263.0 from GOG, EA and Steam, with the address files of [DAOAddressLibrary 0.8.0](https://github.com/Whocares66-Dev/DAOAddressLibrary/releases/tag/v0.8.0). The loader and its contract with plugins are as in 0.3.0.

- **The options screen driven as a player drives it:** a plugin opens it from the game's menu, shows a tab as its button does, lists and sets the game's own controls and plugins' alike, and presses OK or Cancel, each refused where the screen would refuse it (`dao/Options.h`, #272, #273). Cancel now gives a plugin's own options back to their values when the screen opened, as it does the game's.
- **Visual effects:** a tint now shows on an effect's every part whose material takes a colour, the aiming rings and auras among them (`dao::vfx::SetTint`, #271); and a paused frame's fire and spell effects come out the same each launch, their particles run again from fixed seeds (`dao::vfx::ReplayEmitters`, #131).
- **The light on trees and grass** follows `SetSunlight` and `SetTreeLight` from the next frame, lit on the sun's side (#269).
- **Files on disk:** an engine source name's file through the engine's alias table (`dao::resources::PathOf`, #267); and a data table's row found by its ID and a column by its hash at once, and a row read whole (`daolib_core`, #274).
- **For graphics mods:** the draw-distance functions and globals in the address library (#266).

## 0.7.0

For Dragon Age: Origins 1.05.13263.0 from GOG, EA and Steam, with the address files of [DAOAddressLibrary 0.7.0](https://github.com/Whocares66-Dev/DAOAddressLibrary/releases/tag/v0.7.0). The loader and its contract with plugins are as in 0.3.0.

- **The game's screens as functions:** a plugin levels a character up, picks a specialization, travels on the world map, loots, buys and sells, crafts, sets tactics, chooses the party, takes a Chanter's board quest, reads an item's requirements and makes a character in the creation screen, each by the engine's own path and refused wherever the screen would grey the choice out (`dao/LevelUp.h`, `WorldMap.h`, `Loot.h`, `Store.h`, `Crafting.h`, `Tactics.h`, `PartyPicker.h`, `Board.h`, `CharGen.h`, `Items.h`, #259).
- **Moments as callbacks, not polling:** a death and a revival as they happen, a made creature ready, a game mode's change, a load's end with the party placed, and the game taking input again (`dao::creatures`, `dao::modes`, #255); and what holds a creature, an approach's own verdict and a creature's movement rate (#257).
- **The engine's messages typed:** the abilities', party's, items', conversations', popups' and journal's messages read and sent with named fields (`dao/Messages.h`, #253).
- **The renderer as data:** the scene as a tree of graphics objects and nodes (`dao/Scene.h`, #244), a frame's draws with the node, model, mesh and material each one draws (`dao/Render.h`, #246, #247), and the area's point lights and probes with their place, reach and irradiance (`dao/Lights.h`, #256).
- **The game's files written back byte for byte, with no game running:** talk tables, data tables, GFF3 templates and ERF archives, each tested over every file of the install; typed views of data tables and creature, item, placeable, store and trigger templates; a cloth node and a room's animated models; and a model's collision shapes posed as the engine poses them (`daolib_core`, #106, #258, #261, #262, #264).

## 0.6.0

For Dragon Age: Origins 1.05.13263.0 from GOG, EA and Steam, with the address files of [DAOAddressLibrary 0.6.0](https://github.com/Whocares66-Dev/DAOAddressLibrary/releases/tag/v0.6.0). The loader and its contract with plugins are as in 0.3.0.

- **Saved games read and written, with no game running:** `daolib_core` reads a save into plain structs (the campaign, its journal and variables, every area with what is left in it, the hero and the party, the event queue), checked against the game's own loaders (#202), and writes a changed one back over the file it came from, its metadata too (#221, #226).
- **For lighting and effects mods:** each point light's colour, a light probe's irradiance, an object's share of the sun, and a sprite emitter's colour, as the engine hands them to a draw, read and changed (`dao/Lights.h`, `dao/Vfx.h`, #223, #225, #230, #224); the effects running in the area with their models and places (#231); a texture as the engine makes it from a `.dds` (`dao/Resources.h`, #228); and changing the sunlight moves the sun's disc in the sky with it (#222).
- **More for plugin authors:** a callback when a creature plays an animation and when a sound event starts (#217, #218); the world's clock as a save keeps it (`dao::clock::Now`, #232); the camera's projection, and the near and far planes given back after a free view (#204, #205); and a few of the exe's bytes changed in place only where they are as expected (`dao/Patch.h`, #220).
- **The game's files read whole:** compiled scripts and `script.ldf` (#166, #167), plots, campaigns, cutscenes and conversations' switches (#168 to #179), head presets (#171), FaceFX's lip-sync and expressions (#206), sound banks and event projects (#191, #192), a level's collision, walk-grid patches, wind and trees (#176, #182, #189, #196, #197), with the overlap test the game makes when it places a creature (#233). The install's index builds from a warm cache in half a second (#207).
- **For players:** the documentation names the requirement, the latest x86 Visual C++ Redistributable, whose older versions stop the game at start with 0xc0000142; and each release's documentation is kept, with a version selector (#201).

## 0.5.0

For Dragon Age: Origins 1.05.13263.0 from GOG, EA and Steam, with the address files of [DAOAddressLibrary 0.5.0](https://github.com/Whocares66-Dev/DAOAddressLibrary/releases/tag/v0.5.0). The loader and its contract with plugins are as in 0.3.0.

- **One creature's animation held:** a mod pauses, slows and resumes one creature's animation apart from the game's speed, by the engine's own pause for it, and its attacks' hits wait with it (`dao/Animation.h`, #159). What the turn-based mod holds a grab with between the grabber's turns.
- **A character as the engine assembles it, with no game running:** `daolib_core` resolves a creature's template and items into the models the game loads: its body's parts, the bare body where a slot is empty, the helmet and what it hides, and where each weapon hangs (#161). It reads a creature's animation trees and finds an animation's clip in them (#163), and an animation's channels and keys (#157).
- **A level's files read whole:** an area's placed creatures, objects, triggers and variables (#160); a level's layout with its sun, sky and frame effects (#154); a room's lamps and light probes (#152); a level's walk grid (#156); and a material's definition (#153), a head morph's material values (#158) and a cube map's six faces (#162).
- **For testers:** the play matrix blocks inbound connections for each of its copies of the game, so Windows Firewall stops asking (#151).

## 0.4.0

For Dragon Age: Origins 1.05.13263.0 from GOG, EA and Steam, with the address files of [DAOAddressLibrary 0.4.0](https://github.com/Whocares66-Dev/DAOAddressLibrary/releases/tag/v0.4.0). The loader and its contract with plugins are as in 0.3.0.

- **Fires no longer strobe:** the game's flickering lights (fires and torches) were meant to ease from one brightness to the next, but a fault in the game made them jump instead, many times a second. They now ease, and change about once a second rather than up to twenty times. Every game with the loader gets it, whatever mods it has (#150).
- **For mod authors:** the ambient light each object is drawn with, read and changed as the engine works it out (`dao/Lights.h`, #147); and the light SpeedTree's trees are drawn with, read and set, which changing the sunlight now sets as well (`dao/Atmosphere.h`, #148).

## 0.3.1

For Dragon Age: Origins 1.05.13263.0 from GOG, EA and Steam, with the address files of [DAOAddressLibrary 0.3.0](https://github.com/Whocares66-Dev/DAOAddressLibrary/releases/tag/v0.3.0). The loader and its contract with plugins are as in 0.3.0.

- **ASI mods load with DAOLib:** the loader loads `*.asi` files from `bin_ship`, `bin_ship\scripts` and `bin_ship\plugins`, as Ultimate ASI Loader does. Install a mod such as [DAFix](https://github.com/Lyall/DAFix) without the `dinput8.dll` and `dinput8.ini` it comes with ([compatibility](https://whocares66-dev.github.io/DAOLib/compatibility.html), #145).
- **Saves:** a save made through the library is no longer also copied into `bin_ship` (#141).
- **For tools without the game:** `daolib_core`'s install reader reads the plugins folder the game was started with, as `DAOLIB_PLUGINS` names it (#143), and reads an install in a third of the time, or less with archives' directories kept on disk between runs (#144).

## 0.3.0

For Dragon Age: Origins 1.05.13263.0 from GOG, EA and Steam, with the address files of [DAOAddressLibrary 0.3.0](https://github.com/Whocares66-Dev/DAOAddressLibrary/releases/tag/v0.3.0). The loader and its contract with plugins are as in 0.2.0: a 0.2.0 plugin runs with it, and the plugins below need it.

- **A camera of a mod's own:** the view drawn from where a mod puts it, the game's own fly camera with its keys held off and the game's camera given back as it was (#125), the field of view read and held (#126), and the mouse's cursor hidden and held while a mod turns the view, as the game's own orbit does (#132). What Photo Cam is built on.
- **Keys and mouse for mods:** a key, a mouse button, the mouse's movement and the wheel read as the game reads them, and the game's own reaction to them held off while a mod uses them (#127); a mod's own keys held rather than pressed, and a mod's key map that shares keys with the game's walking and turning without unbinding them, on while the mod is in use (#130).
- **The game's files changed as they load:** a mod patches a type of file as the game reads it (#124), now also for a file first read where the game asks its length before its bytes, as an add-in's materials are (#135); a mod's own folder of files watched, so a file it writes is found without a restart (#137); and a large file, a texture above all, read off the game thread (#138).
- **The options screen and screenshots:** a mod's settings go back to their defaults with the screen's own Restore Defaults (#140), and a mod's own screen is kept out of the game's screenshots (#133).
- **More of the game's files with no game running:** `daolib_core` reads and writes models, morphs, textures, templates, tables and talk tables, packs a mod as an ERF archive or DAUpdater's `.dazip` (#116 to #123), reads a material object (#128), and resizes a picture as Pillow's box filter does (#129).

## 0.2.0

For Dragon Age: Origins 1.05.13263.0 from GOG, EA and Steam, with the address files of [DAOAddressLibrary 0.2.0](https://github.com/Whocares66-Dev/DAOAddressLibrary/releases/tag/v0.2.0):

- **The loader is `dinput8.dll` now, not `version.dll`:** a compatibility setting on the game's exe ("Disable fullscreen optimizations" among them) had Windows answer the game's `version.dll` with its own, so DAOLib never ran and wrote no log (#93). From 0.1.0, delete `bin_ship\version.dll` and `version.pdb` first, or every mod loads twice. Another mod's `dinput8.dll` keeps working renamed `dinput8.chain.dll` beside it. Each store's game was started under each compatibility setting a player may have ticked before this release (#97), and in EA's and Steam's wrapped builds mods load as the game's runtime starts (#94).
- **Faster loading:** the engine's memory comes from the system's heap in place of its own 850 MiB of pools (#69), the game runs on every processor where GOG's exe kept it to two (#92), and fast load keeps the areas and rooms the party leaves, loads the indoor areas beside it ahead, and takes the loading screen's black in and out faster (#64, #71, #72, #73, #81, #83). Fast load is off until it is ticked on the options screen's Game tab ("Fast loading", #115); `DAOLIB_ENGINE_HEAP=0`, `DAOLIB_ALL_PROCESSORS=0` and `DAOLIB_FAST_LOAD=0` leave each out.
- **A repack of the game is found by its code:** an exe whose headers no address file is named for, but whose code is a known build's (Steamless' output, the Nexus "decrypted Steam" exe with or without the 4GB flag), reads that build's file, found by a hash of its `.text` read from the exe on disk. `loader.log` says so; a wrapped exe or one whose code is changed is found by its headers or not at all, as before (#110). Plugins built before it find the file they ask for: the loader makes it, the matched build's with the exe's header.
- **Two more fixes to the game:** the mouse's movement is timed by the last input it read, not one past it, a read that crashed the game once its memory was the system's heap (#105), and a screenshot's file name is no longer read past its end (#78). A crash log's last lines are of the logs this run of the game wrote, not older ones (#57).
- **More of the library for mod authors:** an area's sky and sunlight (#62), options sections with dropdowns, buttons and Video and Online tabs (#66, #79, #82, #104), the game's files by name, a mod's own folder of them, and textures as their source holds them (#85, #96, #100, #102), objects and items under ids a mod chooses (#86, #87, #103), movement keys held by a mod (#95), how far creatures are shown (#88), and a creature's weapon damage per hand (#108). `daolib_core` reads the game's archives, tables, talk tables, templates, textures and levels with no game running, 64-bit as well, and says which copy of a file the game takes (#106, #111, #113).

## 0.1.0

The first release, for Dragon Age: Origins 1.05.13263.0 from GOG, EA and Steam:

- **A loader for native mods:** `version.dll`, extracted into the game's `bin_ship` folder beside `DAOrigins.exe`. When the game starts it loads each mod from a folder of its own, `bin_ship\daolib\plugins\<mod>\`, and says what it loaded in `Documents\BioWare\Dragon Age\daolib\loader.log`. It changes none of the game's files, and deleting what was extracted removes it.
- **One mod for all three stores:** the zip holds the address files of [DAOAddressLibrary 0.1.0](https://github.com/Whocares66-Dev/DAOAddressLibrary/releases/tag/v0.1.0), which say where each function and global is in the GOG, EA and Steam executables, so a mod is built once and runs in each.
- **Crash and freeze logs:** a crash, or a freeze of five seconds or more, leaves a log beside `loader.log` with the call stack, the engine objects and files on it, and the last lines of every mod's log. A crash that came of the game running out of address space, as the Steam and EA builds do in a long session, says so. `version.pdb` in the zip is what lets a log name DAOLib's own code.
- **A fix to the game:** two resources read from one archive at the same moment could each get the other's bytes, which crashed the game in a model's load or left a model out. Each read is now at its own place in the file, for every game the loader is in, whatever mods it has.
- **A library for mod authors:** `daolib`, a static C++20 library a mod compiles in. It has the game's script commands, callable and interceptable from C++, its objects, effects, events and messages, the Flash interface, the camera, the world's geometry, saves, key bindings and the options screen. The [documentation](https://whocares66-dev.github.io/DAOLib/) has the API reference and the guide to writing a plugin, and [`examples/`](https://github.com/Whocares66-Dev/DAOLib/tree/v0.1.0/examples) a small plugin for each part.
