# Save games

A save is a folder under `Documents\BioWare\Dragon Age\Characters\<name>\Saves\<slot>\`: the save itself (`<area>.das`), its metadata (`<area>.das.met`), a screenshot (`screen.dds`) and the character's picture. `tools/gff4.py --dump FILE` reads the first two whole, with field names from xoreos's table (`dev/gff4_fields.csv`). Read 2026-09-30 from a save made at Castle Cousland, on this machine.

## The save (`.das`)

GFF4, type `SAVE`, version 1.1, 78 struct templates. Its top struct:

- **`SavegameCampaign`** (`MODU`): the module and its state.
    - The groups and which are hostile to which.
    - The journal: active and completed quests, the plot destinations.
    - The world map, the plot actions, the ambient dialogs.
    - The module's script variables (`ScriptVarTable`: `APP_RANGE_VALUE_*` and the like, by type: int, float, string, object, resource).
    - Where the campaign starts (area list, area, position, orientation).
- **`SavegameAreaList`** (`ARE1`): the areas in memory, each with its tag, object id and objects. For the placeables: template, position, orientation, object id, state.
- **`SavegamePlayerchar`** (`PLYR`): the player's creature, the map's zoom and legend, the time played in seconds.
- **`SavegamePartyList`** (`PRTY`):
    - The party pool, the backpack, plot items, money.
    - Crafting recipes, the selected character and the leader, approval.
    - The party's settings: hold positions, run in drive mode, the quickbar lock.
    - The plot manager, the lines seen.
- **`SavegameVersion`** (`VERS`): the build that saved it (13263, this exe's) and whether cheats were used.
- **`SavegameGameState`** (`GMST`): the next effect id, the AI master (the engine's event queue, `dao/Addresses.h`), the world timer.
- **`SavegameAddinsList`** (`ADIN`): the 58 add-ins the game knows, each with its name in every language.
- **`SavegameStorySoFar`**: the story events and level-ups.
- **`SavegameWorldDatabase`**: id ranges per group.

## The metadata (`.das.met`)

GFF4, type `SVMT`: what the load screen shows without opening the save. The area's name, the time played, the level, class, gender, race and background, the character's and the save's names, the journal's story text. Two fields xoreos does not name, 16809 and 16810, hold `single player;` and `SP;`.

## How the game writes and finds one

Read 2026-09-30 (`src/dao/Addresses.h`, `SaveGame`):

- **Asking.** A save or load is asked for with `SaveGameMessage` or `LoadGameMessage` to `SaveLoadProxy`, with a slot and a kind: 0 a manual save (slots to 500), 1 an autosave (1–4), 2 the quicksave, and a fourth kind.
- **The folder.** It is `<documents>\<character>\Saves\<Slot_|AutoSave_|QuickSave_><n>` (`SlotFolder`).
- **Writing.** `SaveGame::Write` writes `<name>.das` and `<name>.das.met` there, `savegame` if the name is empty.
- **Loading.** `SaveGame::FindSave` finds the `.das` in a slot's folder for loading.

Seen in play, 2026-09-30:

- **A quicksave.** The key sent `SaveGameMessage` with slot -1 (the handler chooses), kind 2 and no name. `SaveGame::Write` was then called for Aedan, slot 1, kind 2, and returned 1. Last came the screenshot's path, `...\Aedan\Saves\QuickSave_1\screen.dds`.
- **Loads.** A load from the menu sent `LoadGameMessage` slot 2, kind 0, and `FindSave` was called with the same. A quickload sent slot 1, kind 2.
- **An earlier quicksave did not call `Write`**, though its files were written. Saves run in the loading game mode, whose worker (`GameModeLoading::Worker`) calls `Write` only when a check passes, and what that check is has not been read.
- **The save and load screen.** As it fills, it asks `SaveLoadProxy::ScreenshotPath` for every slot of every character, with an empty path for one with no screenshot.

So a plugin's save data can hang on `Write` returning: it has the character, the slot and the kind, which name the folder. It can hang on `FindSave` or `LoadGameMessage` for a load. The unread check is the gap: a save that skips `Write` would be missed.

`dao/Saves.h` does that. `OnSaved` runs after `Write` succeeds, which is before the save's picture: the game asks for `screen.dds`'s path after `Write` (the quicksave above), and a plugin that read the slot's folder from `OnSaved` found the picture missing or of 0 bytes (seen 2026-10-12 by the multiplayer mod's session, which sends a host's save to its guests; the save loaded all the same). So `OnSaved` means the `.das` and its `.das.met` are written, and a plugin that wants the picture waits a frame or more for it. `OnGameLoaded` runs when a playthrough is in the world: at the module's load event after a `FindSave`, with that save's slot, or at the module's start event for a new game, with none, so a plugin never carries one playthrough's state into another. `Slot::Read` and `Slot::Write` take the plugin's own file in the slot's folder, which the game's own `SlotFolder` names. The check that can skip `Write` turned out to be a recursive walk over the slot's folder (`0x004C9B10`) that the worker requires to succeed; why one quicksave wrote its files without `Write` is still not read. Read 2026-10-05: `Write` (`0x00646280`) is the only code that names a save's files (its own `.das` and `savegame`, at `0x006463EE` and `0x00646387`), and its only caller is the worker (`0x006C6AD5`), so every save of every kind is written by it; that quicksave was most likely made before the hook was in place, not by another path.

Once that is read, a plugin's data can go beside the save under the same name, `<name>.daolib`, the way SKSE keeps a `.skse` file beside a Skyrim save.

## Asking for a save or a load

Read 2026-09-30 from the save proxy's handlers, for `dao::saves::Load` and `Save`:

- **The proxy.** `SaveLoadProxy` is the application's `+0x150`; the game mode controller passes `LoadGameMessage` and `SaveGameMessage` on to it (`0x00640140`). Its state is SmartData attributes, each holding its value at its `+0xC`.
- **Whose saves.** A load means the character chosen at `+0xE4` (its folder's name). `SelectHeroMessage` chooses by its place in the list at `+0x150`, whose records hold the folder's name at `+0x98`; `RefreshHeroesMessage` fills the list from the folders, reading each save's `.das.met`.
- **The load screen's mode.** `+0xAC` is the save screens' mode (`SetSaveLoadModeMessage`). Setting it to 1 or 2 makes the first character in the list the chosen one, or in the world the one played, and refreshes the saves. The load handler does that itself unless the mode is 2, which is how the load screen keeps a choice. So a load of a chosen character is: mode 2, `SelectHeroMessage`, `LoadGameMessage`. The handler sets the mode back to 0 once the load has begun.
- **The checks.** Both handlers first ask the game mode controller whether a save or load may happen now (`0x006CB740`). A manual save's slot of -1 takes the next free one (`+0x6C`), an autosave's comes from `+0x7C`, and the quicksave's must be 1 (`+0x8C`); a manual or quick save also needs `+0x5C`, which saving's being allowed sets. A save then begins only once there is a game (the application's `+0x14C`), and the save handler answers whether it did. It hands the slot, kind and name to the application (`0x005D7FC0`), whose save state copies the name.

## A save sent from one game to another (seen 2026-10-12)

The multiplayer mod's session, two games on one machine (its master 8bbaff0, Lyna's manuals 342 and 16, a save of 7.9 MB): the host's game saved by `dao::saves::Save` to a manual slot of its own choosing, the slot's `.das`, `.das.met` and `screen.dds` sent to the other game, written there into the same slot of the character it was playing, and loaded by `dao::saves::Load`. The guest was in the host's world 9 to 19 seconds after. So a save's folder is all a save is, it loads in another profile as it is, and `Load` works from a plugin in a loaded game. The load numbers its new objects from the save's own counter (the guest's build began at the id the host's had, 276461, and made the same 109 objects), so after it the two games agree on every id again. A character the profile had never had works too (the same session, later that day, twice): a third game with no character and no save was sent the host's save, written to a slot under a character folder made for it, and `Load` of that slot from the main menu loaded it. Not tried: a slot whose folder is one character's and whose save is another's.

A load numbers its new objects from the save's counter, which is where the game that wrote the save numbers what it makes next. So a host that writes a save for a guest and plays on, and a guest that loads it, give the same ids to different things: seen there, the host's next creature and the guest's character-screen stage (`char_stage` and its waypoints, the load's hundred-odd new objects) both at 0x31562. Two ways out, both seen in play: the host moves its own numbering on after the save (`NumberNewFrom`), or the guest has the load's build numbered elsewhere (`OnAreasBuild`'s `starting`, which is followed for a load's build as for any: in one game, 2026-10-12, a load asked for from inside the game with `starting` answering 0x20000000 built its 110 new objects at 0x20000001 on, none refused, and left the counter at 0x2000006E). In that run `starting` was given the save's counter (0x314EA), the game's own having been 0x1131562 before the load, so the counter is the save's by the time the build begins; the multiplayer mod's session saw `starting` given the game being left's instead, which is not explained.

## What it means for plugins

A save is the engine's own objects written out. A plugin that keeps state of its own needs it saved and loaded with the game's: DAOLib's "save and load" item (`dev/LIBRARY.md`). The script variables are one place the game already saves by name. A plugin's own data could ride in a table of its own, as SKSE's co-saves do beside Skyrim's saves.
