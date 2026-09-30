# Examples

Small plugins, one per part of DAOLib, written to be read and copied from. Each is also a check of its part in the game: `tools\deploy.ps1` puts them in `bin_ship\daolib\plugins\` with the rest, and each logs what it does to `example_<part>.log` in the daolib log folder (`Documents\BioWare\Dragon Age\daolib\`).

They build with the library when DAOLib is the top-level project (`DAO_BUILD_EXAMPLES`), and each is one `add_daolib_plugin` line in `examples/CMakeLists.txt`, as a plugin in its own repository would be (`dev/PLUGINS.md`).

| Example | What it shows | The library's part |
|---|---|---|
| [`saves/`](saves/Saves.cpp) | A plugin's own data kept with the game's saves: a count of the saves made in a playthrough, written with each save and read back from the save being loaded, so loading an older save brings back its count and a new game starts from nothing | `dao/Saves.h` |

To see one work: deploy, play, and read its log. For `saves/`, save twice, load the first save, and save again: the log counts 1, 2, then back to 1 on the load and 2 on the next save. A new game logs that it counts from nothing.

Writing one is also a review of the API it uses. Where an example is awkward to write or has a trap in it, the library is changed rather than the example worked around: `saves/` is why `dao/Saves.h` has `OnGameLoaded` (which also tells a plugin of a new game, so no state leaks from one playthrough into the next) and `Read`/`Write` (so no plugin writes file boilerplate or leaves half a file in a save).

`src/plugins/example/` is older: one plugin that checks many parts of the library at once in play. What it shows moves here, one part at a time.
