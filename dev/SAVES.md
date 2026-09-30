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

In play, 2026-09-30: `FindSave` was called for a load (character, slot 2, kind 0), but `Write` was not called for a quicksave that was written. Saves run in the loading game mode, whose worker (`GameModeLoading::Worker`) calls `Write` only when a check passes. Its step 10 writes the save's screenshot through `SaveGame::WriteScreenshot`, with the slot and kind resolved. The probe logs that too, and the example plugin logs every `SaveGameMessage` and `LoadGameMessage` with its slot, kind and name.

Once a session shows them, a plugin's data can go beside the save under the same name, `<name>.daolib`, the way SKSE keeps a `.skse` file beside a Skyrim save.

## What it means for plugins

A save is the engine's own objects written out. A plugin that keeps state of its own needs it saved and loaded with the game's: DAOLib's "save and load" item (`dev/LIBRARY.md`). The script variables are one place the game already saves by name. A plugin's own data could ride in a table of its own, as SKSE's co-saves do beside Skyrim's saves.
