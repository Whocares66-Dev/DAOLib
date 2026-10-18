# Character creation: the scripts, the tables and the engine

How the game makes the hero, for DAOParagonEngine's playthrough from character creation (its #17). Read 2026-10-18: the scripts in the toolset's sources (`toolset\extract\scripts\`) and the data tables through ghilana, by two of lib-agent's subagents in parallel; the engine's side, the creation screen `GUI::GUICharGen`, by lib-agent-2 in the exe (the last two sections). Paths are under `toolset\extract\scripts\`.

## How it starts

- `sp_module_start.nss` (132–142), unless the ini's `DebugOptions/SkipCharGen` is 1, plays `game_intro.cut` with the plot action `GEN_START_CHARGEN` of `PLT_GEN00PT_GENERIC_ACTIONS` at its end, and calls `PreloadCharGen()`.
- The plot action (`_Global\Plot Scripts\gen00pt_generic_actions.nss`, 42–44) calls `StartCharGen(GetHero(), 0)`.
- The hero exists already: the engine's player creature, `GetHero()` and every chargen event's object 0. The scripts never make one; they reset it in place. Its appearance and racial type stay the engine's until a race is chosen.
- `module_core.nss` (625–627) hands every event from `CHARGEN_START` to `CHARGEN_END`, and `PLAYERLEVELUP`, to `sys_chargen.ncs`. `sp_module` passes on what it does not handle.

## The events, as `sys_chargen.nss` handles them

The numbers are `script.ldf`'s (142–154) and `events_h.nss`'s. Each handler takes the hero as object 0 (`sys_chargen.nss`:96). The screen sends them in its order, race before class before background: each rebuilds the character from the choices before it.

| Event | | Reads | Does |
|---|---|---|---|
| `PLAYERLEVELUP` | 24 | | Out of combat and not shapeshifted, in explore or the GUI: `StartCharGen(player, 1)` |
| `CHARGEN_START` | 44 | int 0: mode, 0 create, 1 level up | Keeps the mode in the module's `CHARGEN_MODE`. Creating: `Chargen_InitializeCharacter` (`sys_chargen_h`:484): level 1, no XP, six attributes at 10, health 1, the rest 0; 1 skill point (humanoids), 5 attribute points, 2 talent points; ability lists 1 to 3 cleared; the hero takes hidden ability 4001 |
| `SELECT_SOUNDSET` | 48 | string 0: the sound set | `SetSoundSet`, the hero alone |
| `SELECT_GENDER` | 54 | int 0: gender | `SetCreatureGender` |
| `SELECT_RACE` | 46 | int 0: `RACE_*` (1 dwarf, 2 elf, 3 human) | Initialises again, gender again, then `Chargen_SelectRace` (663): appearance from the race's `Appearance`, its racial type, trait 4002, its attribute adjusts and `Ability1`/`Ability2` (a dwarf's 3080 Dwarven Resistance); then `default_player.utc`'s items |
| `SELECT_CLASS` | 47 | int 0: `CLASS_*` (1 warrior, 2 wizard, 3 rogue); int 1: 1 selected, 0 not | Selected: `Chargen_SelectCoreClass` (784): the class property (27), its adjusts, attack and defence, mana or stamina, health (BaseHealth × the rank's `fHealthScale` − 1), damage per level, `StartingAbility1`/`2`, and one skill (warrior 100100, rogue Poison 1, wizard Herbalism 1); then `default_warrior`, `_wizard` or `_rogue.utc`'s items |
| `SELECT_BACKGROUND` | 55 | int 0: `BACKGROUND_*` (1 Dalish, 2 commoner, 3 city, 4 magi, 5 noble) | Rebuilds race and class, sets the background property (37), adds a skill by background (city Persuade 1, commoner Stealing 1, Dalish Survival 1, magi Combat Tactics 1, noble Combat Training 1, or 2 if held), clears every `GEN_BACK_*` flag of `PLT_GEN00PT_BACKGROUNDS` and sets the race's and background's one, and loads the row race × 1000 + class × 100 + background's items |
| `ASSIGN_ATTRIBUTES` | 50 | int 0: the attribute's `PROPERTY_*`; int 1: points | Takes the points from property 34 and adds them to the attribute |
| `ASSIGN_ABILITIES` | 51 | int 0: an ability (negative removes it); int 1: 0 a talent, spell or skill, else a specialization | A skill spends property 35, a talent or spell 36, one each and only for one not held; a specialization property 38. Then the ability is added with a quickslot, or removed |
| `SELECT_NAME` | 49 | string 0: the name | `SetName` |
| `CHARGEN_AUTOLEVEL` | 56 | | `AL_DoAutoLevelUp`, the tactics count, `SetCanLevelUp` |
| `CHARGEN_END` | 59 | int 0: mode; int 1: 0 quick start, 1 normal, 2 advanced, 3 "level of the week" | Creating: 70 inventory slots, the tactics presets, nine recipes (four more with Awakening's resources); quick start a random class as a human noble (warrior, rogue) or human magi (mage), +3 Str +2 Dex; 1 auto levels; then `SendEventModuleChargenDone("", "")`, for 3 `("lot100ar_lothering", "lot0100wp_from_wilds")`; the tactic preset 1 warrior, 2 rogue, 5 mage |

`SCREEN_ENTERED` (45), `SELECT_LEVELUP_CLASS` (52) and `IMPORT_HERO` (53) are not handled.

## The origin starts

`EVENT_TYPE_MODULE_CHARGEN_DONE` (1027) goes to the module (`SendEventModuleChargenDone`, `events_h`:1108); `sp_module.nss` (230–237) passes its two strings to `Campaign_ChargenDone` (`_Main_Plot\Campaign Includes\campaign_h.nss`, 87–119). That turns the party picker off, sets `TUTORIAL_ENABLED`, and moves the party: to the strings' area and waypoint if both are given, else to `StartArea` and `StartWP` of the hero's row of `background_defaults` (race × 1000 + class × 100 + background), by `UT_DoAreaTransition`. The origin's plot state is the `GEN_BACK_*` flag set at the background.

## The tables

By their m2da numbers, merged as the game merges them (`dao/core/M2da.h`, `dao/core/GdaFile.h`); none needs a reader of its own.

- **Races, 5** (`race_base`): the attribute adjusts, `Appearance`, `Ability1`/`2`, `PlayerRace`. Playable: 1 dwarf (Str +1, Dex +1, Con +2; ability 3080; appearance 3), 2 elf (Will +2, Mag +2; appearance 2), 3 human (Str, Dex, Int, Mag +1; appearance 15).
- **Classes, 3** (`cla_`): health, mana or stamina, adjusts, attack and defence, per-level gains, `StartingAbility1`/`2`, `CharGenLabel` on the three chosen at creation:

| | Class | Health | Mana/stamina | Adjusts | Attack/defence | Starting abilities |
|---|---|---|---|---|---|---|
| 1 | Warrior | 100 (+6) | 100 | Str +4, Dex +3, Con +3 | 60/45 | 4022 |
| 2 | Wizard | 85 (+4) | 115 mana | Mag +5, Will +4, Int +1 | 50/40 | 4023 |
| 3 | Rogue | 90 (+5) | 90 | Dex +4, Int +4, Will +2 | 55/50 | 4020, 603 Dirty Fighting |

  The specializations are rows 4 to 15 (`BaseClass` their class, a `MinLevel`, a hidden ability and a first talent): mage 4 Shapeshifter, 5 Spirit Healer, 10 Arcane Warrior, 12 Blood Mage; warrior 6 Champion, 7 Templar, 8 Berserker, 9 Reaver; rogue 11 Assassin, 13 Bard, 14 Ranger, 15 Duelist.
- **Backgrounds, 97** (`backgrounds`): which race and class may take each: 1 Dalish (elf; warrior, rogue), 2 commoner (dwarf; warrior, rogue), 3 city (elf; warrior, rogue), 4 magi (elf, human; mage), 5 noble (human, dwarf; warrior, rogue). Its `Skill_*` columns are 0; the scripts' skills are their own. Names, descriptions and icons by race: 210, 208, 209.
- **The origins, 1001** (`background_defaults`, the scripts' `TABLE_STARTING_EQUIPMENT`), the row race × 1000 + class × 100 + background: `StartArea`, `StartWP`, `Template` (the items' .utc), default names, `Ability`. Dalish `bed100ar_forest_clearing`; city elf `bec110ar_players_house`; dwarf commoner `bdc200ar_slums`; dwarf noble `bdn120ar_royal_palace`; human noble `bhn100ar_castle_cousland`; the mage's (2204, 3204) `bhm500ar_tower_harrowing` at `bhm400wp_start`, template `default_mage`, ability 200254 Arcane Bolt.
- **Preloaded templates, 293** (`chargen_preload`): the same rows and 1000, 2000, 3000 for `default_player.utc` (a human male, package 1, script `player_core`, 75 gold, in `gen_im_cth_com_a01`).
- **Sound sets, 212** (`playersoundsets`): six a race and gender, `ss_pc_{m|f}{human|elf|dwarf}_*`; the screen hands the name to `SetSoundSet`.
- **Auto level, 1002 to 1004** (`alwarrior_default`, `alwizard_default`, `alrogue_default`) through the player's package (163, row 1) and its `LevelupTable`; `AttInit` spends the 5 starting points (rows 1 to 6, in an order not yet checked).
- **Heads and tints**: 69 to 79 the beards, eyes, hair and heads by race, 81 to 88 the tints; `chargenmorphcfg.xml` (`misc.erf`) the morph and face presets the appearance scene offers.
- The screen is `chargen.gfx` (`chargen.rim`, patched in 000 to 003); the creation templates are in `packages/core/data/chargen.rim`.

## The chargen events, as the screen raises them

Read 2026-10-18 by lib-agent-2 in the GOG exe (GUICharGen's message handlers) and the toolset's scripts (`sys_chargen.nss`, `module_core.nss`, `sp_module.nss`, `events_h.nss`, `campaign_h.nss`).

### How every chargen event is sent

- Each handler builds a `CScriptEvent`, sets its creator to the module (`CClientExoApp::GetModule` +4, or -1 with no module), sets object 0 to the message's +4 (the creature being made), sets its type at +0x20, fills its fields from the message, and delivers it at once with `AIMaster::DeliverNow(module, module, 10, ev)`: the target is the module, nothing is queued. Read.
- The module's script takes them: `module_core.nss` passes every event from `EVENT_TYPE_CHARGEN_START` (44) to `EVENT_TYPE_CHARGEN_END` (59), and `EVENT_TYPE_PLAYERLEVELUP`, to `sys_chargen.ncs` through `HandleEvent` (line 625). `sys_chargen` reads `oChar = GetEventObject(ev, 0)`.
- String 0 is set by `CDataArrays::SetString` (`0x00A5BA10`, by its use here; the address library has no name for it).

### The events

| Event | Number | Handler | Fields |
|---|---|---|---|
| CHARGEN_START | 44 | `GUICharGen` CharGenStartMessage `0x00736680`; also `GUILevelUp` CharGenStartMessage `0x00733D10` | int 0 = message +8, the mode: 0 `CHARGEN_MODE_CREATE`, 1 `CHARGEN_MODE_LEVELUP`. `sys_chargen` keeps it in the module's `CHARGEN_MODE` and, for create, blanks the character (`Chargen_InitializeCharacter`) |
| SELECT_GENDER | 54 | CharGenSelectGenderMessage `0x00736760` | int 0 = message +0xC, the gender. After the event, if message +0x10 is set, the GUI refreshes the appearance (`0x007356F0(0)`). The script's comment: it also fires on entering the screen, defaulting to male |
| SELECT_RACE | 46 | CharGenSelectRaceMessage `0x00736860` | Up to two events: if message +8 is nonzero, int 0 = +8 and int 1 = 0 (the race left); then if +0xC is nonzero, int 0 = +0xC and int 1 = 1 (the race chosen). `RACE_*` values. The script reads int 1 as "1 selected, 0 unselected (unused)" |
| SELECT_CLASS | 47 | CharGenSelectClassMessage `0x007369F0` | The same pair as race: int 0 = +8 with int 1 = 0, then int 0 = +0xC with int 1 = 1. `CLASS_*` values |
| SELECT_BACKGROUND | 55 | CharGenSelectBackgroundMessage `0x00736C80` | The same pair: int 0 = +8 with int 1 = 0, then int 0 = +0xC with int 1 = 1. `BACKGROUND_*` values |
| SELECT_SOUNDSET | 48 | CharGenSelectAppearanceAndSoundSetMessage `0x00736E10` | string 0 = the message's string at +0xC, the sound set's resource; `sys_chargen` calls `SetSoundSet(oChar, r)` on the hero |
| SELECT_NAME | 49 | CharGenSelectNameMessage `0x00736F00` | string 0 = the name GUICharGen keeps at +0xE8, or "Hero" (`0x00B1B250`) if it is empty |
| ASSIGN_ATTRIBUTES | 50 | `GUICharGenBase` CharGenAssignAttributeMessage `0x007FCD90` | int 0 = message +8, the attribute; int 1 = message +0xC, the points. One event per change; the script takes the points off `PROPERTY_SIMPLE_ATTRIBUTE_POINTS` and spends them |
| ASSIGN_ABILITIES | 51 | CharGenAssignAbilityMessage `0x007FCE60` | int 0 = message +8, the ability (negative: removed); int 1 = message +0xC. The script's comment says int 1 is 1 for a specialization, but it computes `bSpecialization = (int 1 == 0)`: which is right was not read |
| SELECT_LEVELUP_CLASS | 52 | CharGenSelectSpecializationMessage `0x00736B80`; `GUILevelUp` CharGenSelectLevelupClassMessage `0x00733DD0` | int 0 = message +8, the specialization. No shipped script handles 52: it reaches `sys_chargen` and falls through |
| AUTOLEVEL | 56 | `GUICharGenBase` AutoLevelUpMessage `0x007FCF60` | object 0 only. The scripts' own constant, `events_h` line 98; not in `script.ldf`. `sys_chargen` runs `AL_DoAutoLevelUp` |
| CHARGEN_END | 59 | CharGenEndMessage `0x007FCA90` | int 0 = message +8, the mode (0 create, 1 level up); int 1 = message +0xC, the start: 0 quick start, 1 normal, 2 advanced, 3 the "level of the week". In create mode (int 0 = 0) the handler first switches the game to loading (`GameModeLoadingData(7, …)`, `0x005D5E90(9, 1, …)`), sends the event, and then runs the event queue (`AIMaster::Update(0)`) |

- Not raised by the engine: SCREEN_ENTERED (45) and IMPORT_HERO (53); no constructor site stores either type, and no shipped script handles them. CharGenUpdateBeard, SaveInfo, ResetCreature, ImportHero, UpdateHeroAppearance and `GUICharGenBase` ResetMessage build no script event (read: no `CScriptEvent` in their first 0x600 bytes).

### The order in a run

Inferred from the screens and the scripts' comments, not read in the movie: START (mode 0) → SELECT_GENDER (also on entering the screen) → SELECT_RACE (left, then chosen) → SELECT_CLASS → SELECT_BACKGROUND → SELECT_SOUNDSET (the appearance screen) → ASSIGN_ATTRIBUTES (one per change) → ASSIGN_ABILITIES (one per pick) → SELECT_NAME → CHARGEN_END. AUTOLEVEL comes from the auto-level button; SELECT_LEVELUP_CLASS from the specialization screen.

### MODULE_CHARGEN_DONE (1027)

- The scripts send it, not the engine. `events_h` `SendEventModuleChargenDone(sParam1, sParam2)`: an `Event(1027)` with string 0 = sParam1 and string 1 = sParam2, `SignalEvent(GetModule(), ev)` (queued to the module).
- `sys_chargen`'s CHARGEN_END in create mode sends it: with ("", "") after a quick start (it also runs `_RunChargen` with a random class: human noble for warrior or rogue, Circle mage for a mage), and after a normal or advanced start (int 1 = 1 or 2); with (`LEVEL_OF_THE_WEEK_RESREF`, `LEVEL_OF_THE_WEEK_WAYPOINT`) for int 1 = 3.
- `sp_module.nss` handles it with `Campaign_ChargenDone(string 0, string 1)` (`campaign_h`): it turns the party picker off, sets the tutorial on, and, with both strings empty, finds the start from `starting_equipment` (`TABLE_STARTING_EQUIPMENT`) by `Chargen_GetEquipIndex(race, class, background)`, columns `StartArea` and `StartWP`; then `UT_DoAreaTransition(area, waypoint)`. So the mage origin's first area is that table's row for human or elf mage, Circle background.

## What the character creation screen does itself

Read 2026-10-18 in the GOG exe by lib-agent-2. GUI::GUICharGen's handlers mostly raise script events (the section above); what they do natively follows. "Read" is from the decompiled code; "inferred" is marked.

### The creature

- The screen works on the creature the script hands it, not on a template of its own: script command 746 `StartCharGen(object oCreature, int nMode, int bImportEnabled)` (`0x0066b0e0`) pops the three. No `default_player`-like resref is in the exe. Read.
- With nMode 0 (creation) it publishes the creature to the data tree under the token `CharGenCreature` (the creature's +0x18), opens GUI screen 1 (the client app's +0x158, slot 0x60) and stores the creature's id at that screen's +0xC0, sets the screen's import button from bImportEnabled, and sets game mode 0xB (character creation). With nMode 1 it opens screen 0x2E (level-up) instead, or, for a creature with auto level-up on, sends `AutoLevelUpMessage` to the `GUILevelUp` screen and returns. Read.
- `PreloadCharGen` (390, `0x00651770`) only calls `ScriptBackend::ForScript_PreloadCharGen`. Not read further.
- The creature stays whatever object the script made: `CharGenResetCreatureMessage` (`0x00738410`) takes the hero (`ForScript_GetHero`), keeps its id at the screen's +0xC0, refreshes its appearance (`0x007b8470(hero, 1, 1)`) and republishes it as `CharGenCreature`. So the script's creature is the hero itself, and nothing copies it into the player at the end. Read.

### The appearance scene (CharGenStartAppearanceSceneMessage, `0x00739d20`)

- It resets the head first: `0x0074ba60` on the creature's appearance (the head reset: HeadMorph set to empty and the face, eyes and hair rows set to 1, as dev/CHARACTERS.md and the head-part reading have it). Read.
- The camera: it looks up table 7, `portraits.gda`, for the row whose `Tag` is `default` (the string at `0x00aea918`), and copies that row's camera values (CameraPitch, CameraRotation, CameraDistance, CameraZAdjust, CameraXAdjust, Expression, by their column hashes) into the creature's portrait camera at +0x598 (its fields +0x58 to +0x68 and +0x7C), then `CCCreature::UpdatePortrait`. Read; which hash is which column is inferred from the table's columns.
- The sliders: `0x0080ada0` builds about 60 appearance sliders (`GUI::AppearanceSlider<float>`), each with an id (0 to 0x3A), a talk-table label (string ids 0x5B7A5 to 0x5B7E5), a range and a getter and setter on the creature. The ranges of the list sliders (presets, hair, beards, eyes and the rest) are the counts of name lists in `CharGenMorphCfg.xml`, loaded once into a global (`0x008042d0`, at `0x00c39318`); the continuous ones have fixed ranges (−30..30, −45..45, and others). Which sliders appear depends on the creature's race (racial type 2 takes another set) and on a flag read by `0x006a3650`. `0x00739b10(id, mode, sliders)` adds them to the screen; slider 0x2A is the head preset, whose place it keeps at +0x114. Read; what each slider id changes is not read.
- The screen's two beard controls (elements 5 and 6 of its list at +0x118) are hidden at the start and shown by `CharGenUpdateBeardMessage` (`0x00737050`) only when the current head preset's beard value is above 1e-6. That handler changes nothing on the creature. Read.

### The voice set

- `CharGenSelectAppearanceAndSoundSetMessage` (`0x00736e10`) raises the script event 48 (`CHARGEN_SELECT_SOUNDSET`) to the module with object 0 the creature and string 0 the sound set's name; natively it only refreshes the screen (`0x007fecf0`, `0x00803000`). The sound set is applied by the script. Read; the refresh's purpose inferred.

### Specializations

- `CharGenSelectSpecializationMessage` (`0x00736b80`) raises event 52 (`CHARGEN_SELECT_LEVELUP_CLASS`) to the module with object 0 the creature and int 0 the specialization, only when it is not 0; with the message's third field set it moves the screen to its step 4 (`0x007356f0(4)`) and rebuilds the sliders' data (`0x0080ac50`). Read.

### Saved heroes

- `CharGenSaveInfoMessage` (`0x007371c0`) writes the hero as a `.chr` file under `PLAYERS`, named from the hero's name (or `Hero`). Read.
- `UpdateHeroAppearanceMessage` (`0x00738550`) applies an entry of a list of saved heroes (the screen's +0x150, each with a name at +0x7C and a morph at +0x98) to the hero through `0x006d1850`, refreshes the hero (`0x007b8470(hero, 0, 1)`) and republishes `CharGenCreature`. That the list is the `.chr` files is inferred.
