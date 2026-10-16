---
title: Home
nav_order: 1
permalink: /
---

# DAOLib

DAOLib lets you write native plugins for **Dragon Age: Origins** in C++, much as [SKSE](https://skse.silverlock.org/) and [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) do for Skyrim. It has two parts: a loader that loads your plugin when the game starts, and a library that gives your plugin access to the game: its objects, script commands, messages, interface, saves and world.

Players install only the loader and the address files. The library is never installed: a plugin compiles it in from source, so each plugin carries its own copy and works with any loader of the same API version.

## Getting started

- **[The loader]({{ '/loader.html' | relative_url }})** is what a player installs: how it loads the game, and the fixes and performance changes it puts in.
- **[Compatibility]({{ '/compatibility.html' | relative_url }})** is how other mods are installed beside it, DAFix among them.
- **[Writing a plugin]({{ '/guide.html' | relative_url }})** walks through a first plugin, how to build and ship it, and which part of the library to use for what.
- **[The API reference]({{ '/api/' | relative_url }})** lists the library by namespace: `dao::gui` for the Flash interface, `dao::world` for geometry and paths, `dao::abilities` for abilities and cooldowns, `dao::saves` for save games, and so on.

## License

DAOLib is free software under the [GNU General Public License, version 3](https://github.com/Whocares66-Dev/DAOLib/blob/master/LICENSE).

## Credits

- **The dragon** in the title bar is Dragon Age: Origins' emblem, © Electronic Arts, as the [Dragon Age Wiki](https://dragonage.fandom.com/) shows it.
- **The code blocks' copy icons** are [Lucide](https://lucide.dev)'s, under the [ISC License](https://lucide.dev/license).
