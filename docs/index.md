# DAOLib

DAOLib lets you write native plugins for **Dragon Age: Origins** in C++, much as [SKSE](https://skse.silverlock.org/) and [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) do for Skyrim. It has two parts: a loader that loads your plugin when the game starts, and a library that gives your plugin access to the game: its objects, script commands, messages, interface, saves and world.

## Getting started

- **[Writing a plugin](../dev/PLUGINS.md)** walks through a first plugin, how to build and ship it, and which part of the library to use for what.
- **Namespaces**, in the menu, list the API by area: `dao::gui` for the Flash interface, `dao::world` for geometry and paths, `dao::abilities` for abilities and cooldowns, `dao::saves` for save games, and so on.
- **Files** list the public headers, each with a short description of what it is for.

## License

DAOLib is free software under the [GNU General Public License, version 3](https://github.com/Whocares66-Dev/DAOLib/blob/master/LICENSE).

## Credits

- **The dragon** in the title bar is Dragon Age: Origins' emblem, © Electronic Arts, as the [Dragon Age Wiki](https://dragonage.fandom.com/) shows it.
- **The search, clear and arrow icons** are [Font Awesome Free](https://fontawesome.com)'s, under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- **The sidebar icons** are [Lucide](https://lucide.dev)'s, under the [ISC License](https://lucide.dev/license).
