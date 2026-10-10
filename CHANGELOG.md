# Changelog

## 0.1.0

The first release, for Dragon Age: Origins 1.05.13263.0 from GOG, EA and Steam:

- **A loader for native mods:** `version.dll`, extracted into the game's `bin_ship` folder beside `DAOrigins.exe`. When the game starts it loads each mod from a folder of its own, `bin_ship\daolib\plugins\<mod>\`, and says what it loaded in `Documents\BioWare\Dragon Age\daolib\loader.log`. It changes none of the game's files, and deleting what was extracted removes it.
- **One mod for all three stores:** the zip holds the address files of [DAOAddressLibrary 0.1.0](https://github.com/Whocares66-Dev/DAOAddressLibrary/releases/tag/v0.1.0), which say where each function and global is in the GOG, EA and Steam executables, so a mod is built once and runs in each.
- **Crash and freeze logs:** a crash, or a freeze of five seconds or more, leaves a log beside `loader.log` with the call stack, the engine objects and files on it, and the last lines of every mod's log. `version.pdb` in the zip is what lets it name DAOLib's own code.
- **A fix to the game:** two resources read from one archive at the same moment could each get the other's bytes, which crashed the game in a model's load or left a model out. Each read is now at its own place in the file, for every game the loader is in, whatever mods it has.
- **A library for mod authors:** `daolib`, a static C++20 library a mod compiles in. It has the game's script commands, callable and interceptable from C++, its objects, effects, events and messages, the Flash interface, the camera, the world's geometry, saves, key bindings and the options screen. The [documentation](https://whocares66-dev.github.io/DAOLib/) has the API reference and the guide to writing a plugin, and [`examples/`](https://github.com/Whocares66-Dev/DAOLib/tree/v0.1.0/examples) a small plugin for each part.
