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
- The sliders: `0x0080ada0` builds about 60 appearance sliders (`GUI::AppearanceSlider<float>`), each with an id (0 to 0x3A), a talk-table label (string ids 0x5B7A5 to 0x5B7E5), a range and a getter and setter on the creature. The ranges of the list sliders (presets, hair, beards, eyes and the rest) are the counts of name lists in `CharGenMorphCfg.xml`, loaded once into a global (`0x008042d0`, at `0x00c39318`); the continuous ones have fixed ranges (−30..30, −45..45, and others). Which sliders appear depends on the creature's race (racial type 2 takes another set) and on a flag read by `0x006a3650`. `0x00739b10(id, mode, sliders)` adds them to the screen; slider 0x28 is the head preset, whose place it keeps at +0x114 (0x2A is the beard; corrected 2026-10-18). Read; what each slider id changes is not read.
- The screen's two beard controls (elements 5 and 6 of its list at +0x118) are hidden at the start and shown by `CharGenUpdateBeardMessage` (`0x00737050`) only when the current head preset's beard value is above 1e-6. That handler changes nothing on the creature. Read.

### The voice set

- `CharGenSelectAppearanceAndSoundSetMessage` (`0x00736e10`) raises the script event 48 (`CHARGEN_SELECT_SOUNDSET`) to the module with object 0 the creature and string 0 the sound set's name; natively it only refreshes the screen (`0x007fecf0`, `0x00803000`). The sound set is applied by the script. Read; the refresh's purpose inferred.

### Specializations

- `CharGenSelectSpecializationMessage` (`0x00736b80`) raises event 52 (`CHARGEN_SELECT_LEVELUP_CLASS`) to the module with object 0 the creature and int 0 the specialization, only when it is not 0; with the message's third field set it moves the screen to its step 4 (`0x007356f0(4)`) and rebuilds the sliders' data (`0x0080ac50`). Read.

### Saved heroes

- `CharGenSaveInfoMessage` (`0x007371c0`) writes the hero as a `.chr` file under `PLAYERS`, named from the hero's name (or `Hero`). Read.
- `UpdateHeroAppearanceMessage` (`0x00738550`) applies an entry of a list of saved heroes (the screen's +0x150, each with a name at +0x7C and a morph at +0x98) to the hero through `0x006d1850`, refreshes the hero (`0x007b8470(hero, 0, 1)`) and republishes `CharGenCreature`. That the list is the `.chr` files is inferred.

## The head morph a preset and the sliders make

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe and the game's files (`hm_cps_p01.mop` from chargen.rim, `hm_pcc_b01.mop` and `hm_pcc_b01/b02.mor` from face.erf, read with `tools/gff4.py`), for DAOParagonEngine #17 and #22 (dev/PARAGON_CHARGEN.md part 1). "Read" is seen in the code or the files; "inferred" is not read to the end.

### The `.mop` file (read)

A `.mop` is a GFF4 file, header `GFF V4.0 PC  MOP V1.0`, five struct templates. It is not a morph: it holds no vertices or bones. It is a head *recipe*: the parts, the tint files, material values, and a list of morph-target models with weights. The editor's own state is exactly this struct (below), and a saved hero (`.chr`, "CHAR V1.0", written at `0x007BE490`) embeds it as a struct named `Head_Morph`.

| Struct | Field | Label | Type (GFF4) | Flags | Meaning |
|---|---|---|---|---|---|
| `MOP ` (0x1C bytes) | MorphName | 23008 (0x59E0) | 14 ECString | | the preset's name, `hm_cps_p01` |
| | MorphParts | 23000 (0x59D8) | 14 ECString | list (0x8000) | the head's part models by head-part slot, as a `.mor`'s MorphParts: [0] face `HM_UHM_BASa_0`, [1] eyes `hm_uem_basa_0`, [2] hair `hm_har_hb4a_0`, [3] beard, [4] (neck) empty, [5] lashes `hm_ulm_basa_0`; empty = none |
| | MorphTintFileNames | 23001 (0x59D9) | 14 ECString | list | the tint files (`.tnt`), by tint slot, as a `.mor`'s: [0] skin `t1_skn_001`, [2] eyes `t3_eye_grn`, [3] hair `t3_har_dbr`, [10], [11] stubble `t1_stb_dbr`; 12 slots, empty = none |
| | MorphModelParams | 23018 (0x59EA) | struct `MOD` | list of structs (0xC000) | the morph targets and their weights (below) |
| | MorphMatParams | 23014 (0x59E6) | struct `MAT` | list of structs | single floats into a part's material |
| | MorphMatVecParams | 23015 (0x59E7) | struct `MAT4` | list of structs | Vector4s into a part's material |
| | MorphTextures | 23022 (0x59EE) | struct `TEX` | list of structs | textures into a part's material |
| `MOD ` (8) | MorphModelName | 23016 (0x59E8) | 14 ECString | | a morph-target model, e.g. `UH_NOS_30a_0` (an `.mmh` with its mesh: `uh_nos_30a_0.mmh` in chargen.rim, modelhierarchies.erf) |
| | MorphModelValue | 23017 (0x59E9) | 8 float | | its weight |
| `MAT ` (0x10) | MorphMatNodeName | 23009 (0x59E1) | 14 ECString | | the part's mesh node: `FaceM1`, `HairM1`, `EyesM1`, `BeardM1`, `LashesM1` ... |
| | MorphMatParamName | 23010 (0x59E2) | 14 ECString | | the material parameter: `g_vFacialHairWeights`, `mml_fAgeBlendWeight`, `mml_vTintMaskColours` |
| | MorphMatParamIndex | 23011 (0x59E3) | 5 int32 | | the component: a Vector4 parameter's element as index/4 and index%4 (inferred from the values: 0..23 for six Vector4s, 0..7 for two) |
| | MorphMatParamValue | 23012 (0x59E4) | 8 float | | the value |
| `MAT4` (0x20) | MorphMatParamVector | 23013 (0x59E5) | 12 Vector4f | | the value, at offset 0 |
| | MorphMatNodeName, MorphMatParamName, MorphMatParamIndex | 23009, 23010, 23011 | as `MAT` | | at offsets 0x10, 0x14, 0x18; the index is which Vector4 of an array parameter (`mml_vTintMaskColours` 0..9 as a `.tnt`'s ten) |
| `TEX ` (0xC) | MorphTexNodeName | 23019 (0x59EB) | 14 ECString | | the mesh node |
| | MorphTexParamName | 23020 (0x59EC) | 14 ECString | | the texture parameter, `mml_tAgeNormalMap` ... |
| | MorphTexName | 23021 (0x59ED) | 14 ECString | | the texture, `uh_hed_elfa_0n.dds` |

`hm_pcc_b01.mop` (face.erf) has the same layout (the chargen base: bald `hm_har_blda_0`, three MAT entries, no targets of note); `hm_pcc_b01.mor` (3,600 bytes) and `hm_pcc_b02..b04.mor` (~50 KB) are ordinary `.mor` files (GFF4 `MORP V0.1`: parts, tints, nodes with vertices), as core's Morph reads.

### The weights (read in the data; the formula inferred)

In `hm_cps_p01.mop` each mesh's base model carries minus the sum of its targets' weights: the face base `UH_UHM_BASa_0` is −7.12 and the face targets (`UH_*`, `UHM_*`, 17 of them) sum to exactly 7.12; the eyes' base `UH_UEM_BASa_0` −0.15 against `UEM_EYE_09a_0` 0.15; the lashes' base `UH_ULM_BASa_0` −1.43 against `ULM_*` 0.28 + 0.15 + 1.00. So a mesh's vertices are its base part's (MorphParts) plus Σ wᵢ·(targetᵢ − base): the targets are whole meshes of the same topology (blend shapes), each weighted by its slider value, the base entry the bookkeeping that keeps the sum affine. Inferred from the numbers; the blend itself is not read.

### How the engine makes the head (read, except where marked)

- The editor (the appearance scene's slider object; its recipe at +0x1E4, laid out as a `.mop`: targets at +0x24 (12-byte entries, weight at +8), MAT at +0x34 (0x40 each), MAT4 at +0x44 (0x4C), TEX at +0x54 (0x54)) loads a preset at `0x00809F50`: MorphParts (23000) and MorphTintFileNames (23001) into the recipe; each MorphModelParams entry through `0x00807D10(name, weight)`, which finds the target by name (`0x007FF570`), adds it if the model resource exists (resource manager slots 0x94/0x9C), sets its weight (or adds to it), and drops a target whose weight returns within ±0.001 of 0 (`0x00807AE0`); MAT, MAT4 and TEX entries through `0x00800820`, `0x008008B0`, `0x00800950`; then `0x00804390` rebuilds the target set by race and gender (a fixed list per race/gender through `0x008009F0`, and chargenmorphcfg's entries, four per entry), and for an elf or dwarf pairing `0x008039B0(1.0)`.
- Each slider moves a target's weight through `0x00807D10` (inferred: what each slider id names is not read).
- Applying (`GUI::GUIPaperdoll::CommandCreateMorph`, `0x007C11D0`): it fills a `GUI::ComposeMorphTask` from the editor (`0x00809980`: resets a set of exclusive targets, `UH_SHP_*`, `UH_CHM_*`, `UH_JAW_*` and kin, to the editor's choices) and runs it. The task (`0x00802E60`): `0x00800670` (re)loads the head's part models when the parts changed (resource manager slot 0x2C); the head model, with the targets' weights on it, is then written out as a whole morph and registered as an in-memory resource named `Head_Morph` plus a running counter (`DAT_00C3930C`; resource manager slot 0x40), and applied to the head model (its slot 0xE4).
- The writer of a morph (`GraphicsStructures::Morph`, `0x0056FA20`) makes an ordinary `.mor`: GFF4 `MORP V0.1`, struct `morp` with MorphParts (23000), MorphTintFileNames (23001) and the nodes (0x59DA) with their vertices. So **the result is a whole morph, drawable as any `.mor`** (core's `Morph`: parts, vertices, bones, tints), not a base plus targets at draw time. Inferred: the hero's appearance's HeadMorph (CAppearance +0x1C8) names that `Head_Morph<N>` resource, and a save keeps the hero's morph (`SavegamePlayerMorph`, 16950, and `SavegameAppearanceMorphName`, 16328); not read.

### For Paragon

A preset is a recipe: take MorphParts and the tints as a `.mor`'s; for each mesh (face, eyes, lashes), start from its base part's vertices and add Σ weight·(target − base) over the MorphModelParams whose models are that mesh's targets (`UH_*`/`UHM_*` the face's, `UEM_*` the eyes', `ULM_*` the lashes', by name; inferred), each target's vertices from its own `.mmh`/`.msh`; apply MAT/MAT4/TEX to the parts' materials. The engine bakes that into a `.mor` at the end, so drawing the hero afterwards is drawing a `.mor`.

### Not read

The vertex blend inside the model (how the targets' meshes are matched to the base and summed; whether bones move too); what each slider id (0..0x3A) names; the exact use of MorphMatParamIndex for scalar parameters; where HeadMorph is set to `Head_Morph<N>`.

## The appearance sliders and chargenmorphcfg.xml

Read 2026-10-18 in the GOG exe by lib-agent-2 (a subagent), for DAOParagonEngine's character creation (dev/PARAGON_CHARGEN.md part 1). "Read" is from the decompiled or disassembled code; "inferred" is marked. How the head morph is composed from these values, and the `.mop` format, are a sibling reading's.

### Where the sliders live

- The appearance scene's sliders belong to `GUI::GUIPaperdoll`. `0x0080ada0` builds them: for each, an id, a talk-table label (string ids 374693 to 374757, `0x5B7A5` to `0x5B7E5`), a minimum and maximum, an integer flag, a setter and a getter (`0x00800a80` adds one). Read.
- Every slider keeps its value as a float in the paperdoll at +0x1F8 + 4 × id: slider 0 at +0x1F8, 0x21 at +0x27C, 0x29 at +0x29C, 0x2C at +0x2A8, and so on. Read from each setter's store.
- Almost every setter ends with `0x00802bf0`, which queues `GUIPaperdoll::CommandCreateMorph` (vftable `0x00b21848`, carrying the paperdoll and its morph description at +0x2F0) and then `CommandApplyMorph` (`0x00b24d88`): any change rebuilds the hero's head morph from all the values and applies it. Read; the composition itself is the sibling reading's.
- The paperdoll's morph description (its +0x1E4) holds a list of names at +0x14 (0x1C-byte strings: the morph's tint files, inferred from the slots' contents below), the morph's head-part names (`0x004560f0(part)`, the same list the head builder reads), and a list of material-parameter overrides at +0x34 (0x40 bytes each: part name, parameter name, component, value at +0x3C). `0x008029a0(part, parameter, component, value)` finds or adds an override and sets its value; `0x00802870(part, parameter, name)` sets a texture override. Read.
- The race and gender index (`0x007fdcf0`) picks the config's per-race lists; the parser numbers them human_male 0, human_female 1, dwarf_male 2, dwarf_female 3, elf_male 4, elf_female 5 (`0x00800b20`). Read; how `0x007fdcf0` maps a creature to it is inferred (race and gender).

### Morph weights (continuous sliders)

- Sliders 0 to 0x21, except 7, 8, 9, 0xA and 0x1E, are morph weights. Each setter calls `0x007febb0(k, &v, invert, signed)` with k = the slider id and stores the result at +0x1F8 + 4k. Read.
- The conversion: `0x007fdd00(k)` gives two scales lo and hi for k and the race and gender index (1.0 and 1.0 by default; per-race tables of 0.1 to 1.0, e.g. human male k 0, 0x12, 0x1C: 0.8 and 0.8). With signed set (every slider but 0x20), the weight is v × lo for v < 0 and v × hi otherwise, the slider running −1..1; slider 0xD (invert set, 0..1) gets hi × (1 − v) (its lo or hi by v's sign). Slider 0x20 (signed clear) gets (v + 1) / 2 × (lo + hi) − lo. Read.
- That the 34 weights index the preset's morph targets in that order is inferred (the sibling reading has the targets).
- Sliders 0xA (Eye Shape, 0..4) and 0x1E (Nose Shape, 0..n−1, n from `0x007fe610`) store the integer as is at +0x220 and +0x270, with no conversion. Read; that they pick among alternative targets is inferred.

| id | label | range | what it sets |
|---|---|---|---|
| 0 | Mouth Height | −1..1 | weight k 0 |
| 1 | Lip Size | −1..1 | weight 1 |
| 2 | Mouth Width | −1..1 | weight 2 |
| 3 | Overbite / Underbite | −1..1 | weight 3 |
| 4 | Eye Spacing | −1..1 | weight 4 |
| 5 | Eye Height | −1..1 | weight 5 |
| 6 | Eye Size | −1..1 | weight 6 |
| 0xA | Eye Shape | 0..4, integer | value 0xA as is |
| 0xB | Cheekbone Depth | −1..1 | weight 0xB |
| 0xC | Cheekbone Width | −1..1 | weight 0xC |
| 0xD | Cheek Shallow / Full | 0..1 | weight 0xD, inverted |
| 0xE | Cheekbone Height | −1..1 | weight 0xE |
| 0xF | Cheek Width | 0..1 | weight 0xF |
| 0x10 | Brow Depth | −1..1 | weight 0x10 |
| 0x11 | Brow Height | −1..1 | weight 0x11 |
| 0x12 | Chin Height | −1..1 | weight 0x12 |
| 0x13 | Chin Depth | −1..1 | weight 0x13 |
| 0x14 | Chin Width | −1..1 | weight 0x14 |
| 0x15 | Jaw Width | −1..1 | weight 0x15 |
| 0x16 | Bridge Depth | −1..1 | weight 0x16 |
| 0x17 | Bridge Width | −1..1 | weight 0x17 |
| 0x18 | Nose Height | −1..1 | weight 0x18 |
| 0x19 | Nose Depth | −1..1 | weight 0x19 |
| 0x1A | Tip Width | −1..1 | weight 0x1A |
| 0x1B | Nose Size | −1..1 | weight 0x1B |
| 0x1C | Tip Depth | −1..1 | weight 0x1C |
| 0x1D | Nostril Width | −1..1 | weight 0x1D |
| 0x1E | Nose Shape | 0..n−1, integer | value 0x1E as is |
| 0x1F | Ear Height | −1..1 | weight 0x1F |
| 0x20 | Ear Size | −1..1 | weight 0x20, linear |
| 0x21 | Neck Thickness | −1..1 | weight 0x21 |

### Choices and colours

| id | label | range | what it sets |
|---|---|---|---|
| 0x28 | (none: the head preset) | 0..heads−1 | `0x0080ac50(1, 1)`: loads the preset (the race's `heads` list, `HM_CPS_P01.mop` and on) and rebuilds the sliders. Read; the load is the sibling's. |
| 0x29 | Hair | 0..hairs−1 | the hair's model name into the morph's part 2 (`HairM1`, inferred from the slot at +0x38); for the old hair and the new, if its cut c is not 0, `g_vFacialHairWeights` component c + 3 on `HairM1` set to 0 and 1. Read. |
| 0x2A | Beard | 0..beards−1 | the beard's model name into the morph's part 3 (`BeardM1`, `0x004560f0(3)`); an empty name clears it. Males with a beard list only (human and dwarf). Read. |
| 0x2B | Hair / Beard Color (Hair Color with no beards) | 0..hair_colors−1 | tint slot 3 the `hair_colors` entry and tint slot 11 (+0x134) the same row of `crew_cut_colors` (inferred: crew cut). Read in outline. |
| 0x25 | Eyebrows | −1..3, integer | `g_vFacialHairWeights` on `FaceM1`: component n set to 1 for n ≥ 0, the previous one to 0; −1 none. Read. |
| 0x26 | Stubble | 0..15, integer | `g_vFacialHairWeights` components 4 to 7 on `FaceM1`, each 1 or 0 by the value's bits 0 to 3. Males not elves. Read. |
| 0x27 | Eyebrow / Stubble Color (Eyebrow Color for elves and females) | 0..brow_stubble−1 | tint slot 10 (+0x118) the `brow_stubble_colors` entry. Read. |
| 7 | Eye Color | 0..eyes_colors−1 | tint slot 2 (+0x38) the `eyes_colors` entry. Read. |
| 0x2C | Skin Complexion | 0..skins−1 | `FaceM1`'s `mml_tDiffuse` the `skins` entry (`uh_hed_fema_0d` …), and `mml_tNormalMap` the same name with its last letter made `n`. Read. |
| 0x2D | Skin Tone | 0..skin_colors−1 | tint slot 0 the `skin_colors` entry (`t1_skn_001` …). Read. |
| 0x22 | Tattoo | 0..tattoos−1 | `FaceM1`'s `mml_tTattooMask` the tattoo's mask; the old tattoo's component of `mml_vTattooParameters` set to 0 and the new one's to the tattoo intensity (slider 0x24). The paperdoll's tattoo list (+0x2C4, 0x20 each: name and component at +0x1C) is built from `tattoos` (inferred). Read. |
| 0x23 | Tattoo Color | 0..tattoo_colors−1 | tint slots 6, 7 and 8 (+0xA8, +0xC4, +0xE0) from `tattoo_colors`. Read in outline. |
| 0x24 | Tattoo Intensity | 0.4..1.0 | the current tattoo's component of `mml_vTattooParameters` on `FaceM1`. Read. |
| 8 | Eye Shadow Color | 0..eyes_makeup−1 | tint slot 4 (+0x70) the `eyes_makeup_colors` entry; `FaceM1`'s `mml_vTintMaskColours` component 1 set to the intensity (slider 9), or 0 when the entry is empty. Females. Read. |
| 9 | Eye Shadow Intensity | 0..1 | `mml_vTintMaskColours` component 1 on `FaceM1`. Read. |
| 0x2E | Blush Color | 0..blush−1 | tint slot 5 (+0x8C) the `blush_makeup_colors` entry; `mml_vTintMaskColours` component 2 as for eye shadow. Females. Read. |
| 0x2F | Blush Intensity | 0..1 | `mml_vTintMaskColours` component 2. Read. |
| 0x30 | Lip Color | 0..lip−1 | tint slot 1 (+0x1C) the `lip_makeup_colors` entry; `mml_vTintMaskColours` component 0 likewise. Females. Read. |
| 0x31 | Lip Intensity | 0..1 | `mml_vTintMaskColours` component 0. Read. |
| 0x32 to 0x39 | Look Up / Down (−30..30), Turn Left / Right (−45..45), Camera Distance (0.5..0.72), Move Left / Right (±0.036), Move Up / Down (−0.02..0.1), Background (1..20), Expression | | the portrait's camera and backdrop (`0x0069ed50` and kin), not the head. Read in outline. |

- Which sliders appear (`0x0080ada0`): everyone gets 0 to 0x21 (morph), 7, 0x22 to 0x25, 0x28, 0x29, 0x2C, 0x2D and the portrait sliders. With `0x006a3650` true (inferred: male) the beard (0x2A, when the race has beards), Hair / Beard Color, Stubble (not elves, racial type 2) and Eyebrow / Stubble Color; otherwise the makeup sliders (8, 9, 0x2E to 0x31), Hair Color and Eyebrow Color. Read.
- So the morph's tint slots, as these setters fill them: 0 skin, 1 lip makeup, 2 eyes, 3 hair, 4 eye makeup, 5 blush, 6 to 8 tattoo, 10 brows and stubble, 11 crew cut (9 not written here). That this list is the `.mor`'s `MorphTintFileNames`, in that order, is inferred.

### chargenmorphcfg.xml as the engine reads it

- `0x008042d0` loads `CharGenMorphCfg.xml` (misc.erf) once into a global at `0x00c39318`; the parser is `0x00802290` with the element handler `0x00800b20`, which keeps a stack of states by element name (`morph_config`, `heads`, `hairs`, `beards`, the six race and gender names, the colour lists, `tattoos`, `skins`) and reads each `resource`'s `name`. Read.
- The global's lists, each a vector of names (0x1C-byte strings) 0x10 apart: `heads` per race and gender at +0x14 + 0x10 × index; `hairs` at +0x74 + 0x10 × index, with a parallel list of ints (the cuts) at +0xD4 + 0x10 × index; `beards` at +0x134 + 0x10 × index; then `hair_colors` +0x194, `skin_colors` +0x1A4, `eyes_colors` +0x1B4, `eyes_makeup_colors` +0x1C4, `blush_makeup_colors` +0x1D4, `lip_makeup_colors` +0x1E4, `brow_stubble_colors` +0x1F4, `crew_cut_colors` +0x204, `tattoo_colors` +0x214, `tattoos` +0x224, `skins` +0x234. Read. After loading, an empty list gets one empty entry.
- A hair's `cut` (state 9, `0x00800b20`): with no cut the hair is listed once; otherwise it is listed once per digit of `cut` from 0 to 4, each time with that digit in the cut list. So `hm_har_blda_0` cut `0234` gives four Hair choices (bald with cuts 0, 2, 3, 4) and a hair with cut `1` one choice, which is why the Hair slider has more steps than the file has hairs. Read. The cut then drives `HairM1`'s `g_vFacialHairWeights` (component cut + 3; nothing for cut 0), as slider 0x29 does; that the cuts are the scalp's crew-cut masks is inferred from `crew_cut_colors`.
- In the shipped file: per race and gender 9 heads (8 presets and one `_PCC_B01`), 16 or 17 hairs, beards for human and dwarf males only (7 each, the first empty), 9 hair colours, 7 skin colours, 11 eye colours, 31 eye, 25 blush and 25 lip makeup colours (the first empty), 9 brow and stubble and 9 crew-cut colours, 15 tattoo colours, 9 tattoos, 6 skins.

## Seen in play: driving the screen with its own messages

Seen 2026-10-18 by eluvian-agent (Eluvian #153): its `game_new` goes from the start menu to the creation screen in 9 s, and its `chargen` builds the hero through `GUICharGen`'s own messages; a mage to the Harrowing, a city elf rogue to the alienage and a human noble warrior to Cousland, each from Eluvian's tools alone.

- The movie's own `GUICharGen.CharGenStart` goes out with creature 0, sent before its `CharGenCreature.ID` observer is answered: the hero kept 0 in every attribute until `CharGenStart` was sent again with the id. In a player's run the race event initialises again, so nothing shows.
- `CharGenEnd`'s int 1 is the movie's exit point: 0 DEFAULT (a debug button the movie does not place), 1 BASIC, the Quick Play (`sys_chargen` levels automatically), 2 ADVANCED, every page, 3 level of the week (the movie never sets it), 4 import. The movie sends `CharGenEnd` from its `FinishScene` as the screen closes, and only with `CharGen.bUserExit` set. A `GUI.HideFullScreenGUI` with it unset closed the screen with no `CharGenEnd`, and the game crashed under `DragonAgeGUISystem` at `0x0072bb11` (`0x0072bb00`+0x11 reading +0x694, from `0x0053b060`+0x41 and `GUI::DragonAgeGUISystem`'s slot 14 +0x109): anything else that hides this screen must end it so.
- `ASSIGN_ABILITIES`: the movie sends int 1 0 for skills, talents and spells; Awakening's specialization goes as `GUILevelUp.CharGenAssignAbility <cid> <ability> 1`. The movie never sends `CharGenSelectSpecialization` or `AutoLevelUp`.
