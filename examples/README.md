# Examples

Small plugins, one per part of DAOLib, written to be read and copied from. Each is also a check of its part in the game: `tools\deploy.ps1 -Examples <part>` puts it in `bin_ship\daolib\plugins\` with the rest (`-Examples *` for all of them), and each logs what it does to `example_<part>.log` in the daolib log folder (`Documents\BioWare\Dragon Age\daolib\`). They go in only when named, since each changes the game in its own way: `camera/` moves the camera, and would fight another plugin that does. A deploy that does not name one takes it out.

They build with the library when DAOLib is the top-level project (`DAO_BUILD_EXAMPLES`), and each is one `add_daolib_plugin` line in `examples/CMakeLists.txt`, as a plugin in its own repository would be (`dev/PLUGINS.md`).

| Example | What it shows | The library's part |
|---|---|---|
| [`camera/`](camera/Camera.cpp) | Moving the camera smoothly when the player selects another party member: a glide in the tactical view, coming round behind them close in, and never fighting the player's own turning | `dao/CameraMoves.h`, `dao/Camera.h` |
| [`damage/`](damage/Damage.cpp) | Changing combat without replacing a script: the party takes half damage, changed where the scripts hand the new health to the engine, so no script override and other plugins can do the same beside it | `dao/ScriptHooks.h` |
| [`speed/`](speed/Speed.cpp) | The game's speed: fights run twice as fast, what the player sees keeping pace with the AI, effects and regeneration | `dao/Clock.h` |
| [`saves/`](saves/Saves.cpp) | A plugin's own data kept with the game's saves: a count of the saves made in a playthrough, written with each save and read back from the save being loaded, so loading an older save brings back its count and a new game starts from nothing | `dao/Saves.h` |

To see one work: deploy it, play, and read its log. For `saves/`, save twice, load the first save, and save again: the log counts 1, 2, then back to 1 on the load and 2 on the next save. A new game logs that it counts from nothing. For `speed/`, start a fight: everything but the menus should move twice as fast until it ends. For `damage/`, fight: its log gives each party member's hit and the half taken. For `camera/`, select another party member, close in and in the tactical view: the camera glides over, and close in comes round behind them.

Writing one is also a review of the API it uses. Where an example is awkward to write or has a trap in it, the library is changed rather than the example worked around: `saves/` is why `dao/Saves.h` has `OnGameLoaded` (which also tells a plugin of a new game, so no state leaks from one playthrough into the next) and `Read`/`Write` (so no plugin writes file boilerplate or leaves half a file in a save); `camera/` is why `dao/CameraMoves.h` has `Current()` (one call for the camera in the modes it moves in) and `OnTargetChanged` (selection as an event rather than a target polled every frame); `damage/` is why `CommandCall` has `SetInt`, `SetFloat` and `SetObject` (changing an argument before the command runs, which only an answer could be before); `speed/` is why `CClientExoApp::Mode` gives nothing, rather than a crash, before the game's modes exist, and `CurrentMode()` asks from anywhere.

`src/plugins/example/` is older: one plugin that checks many parts of the library at once in play. What it shows moves here, one part at a time.
