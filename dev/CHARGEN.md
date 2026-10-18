# Character creation: the scripts, the tables and the engine

How the game makes the hero, for DAOParagonEngine's playthrough from character creation (its #17). Read 2026-10-18: the scripts in the toolset's sources (`toolset\extract\scripts\`) and the data tables through ghilana, by two of lib-agent's subagents in parallel; the engine's side, the creation screen `GUI::GUICharGen`, is lib-agent-2's (below, when read). Paths are under `toolset\extract\scripts\`.

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

## The engine's side

Lib-agent-2's reading of `GUI::GUICharGen`: what it raises, with what, and what it does itself (the creature it works on, the appearance scene, the voice set, the specializations) is to come here.
