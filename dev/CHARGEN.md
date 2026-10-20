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
| ASSIGN_ABILITIES | 51 | CharGenAssignAbilityMessage `0x007FCE60` | int 0 = message +8, the ability (negative: removed); int 1 = message +0xC: 0 a skill, talent or spell (properties 35 / 36), else a specialization (property 38). The script's `bSpecialization = (int 1 == 0)` (sys_chargen.nss:338) is misnamed: its true branch spends 35 / 36, and the movie sends 0 for skills, talents and spells, 1 for a specialization (lib-agent-2, 2026-10-19; "Level-up" below) |
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
- The camera: it looks up table 7, `portraits.gda`, for the row whose `Tag` is `default` (the string at `0x00aea918`), and copies that row's camera values (CameraPitch, CameraRotation, CameraDistance, CameraZAdjust, CameraXAdjust, Expression, by their column hashes) into the portrait camera at +0x598 of the `PaperCreature` the stage shows (its fields +0x58 to +0x68 and +0x7C; corrected 2026-10-19 by lib-agent-2: not the hero's, "The figure" below), then `CCCreature::UpdatePortrait`. Read; which hash is which column is inferred from the table's columns.
- The sliders: `0x0080ada0` builds about 60 appearance sliders (`GUI::AppearanceSlider<float>`), each with an id (0 to 0x3A), a talk-table label (string ids 0x5B7A5 to 0x5B7E5), a range and a getter and setter on the creature. The ranges of the list sliders (presets, hair, beards, eyes and the rest) are the counts of name lists in `CharGenMorphCfg.xml`, loaded once into a global (`0x008042d0`, at `0x00c39318`); the continuous ones have fixed ranges (−30..30, −45..45, and others). Which sliders appear depends on the creature's race (racial type 2 takes another set) and on a flag read by `0x006a3650`. `0x00739b10(id, mode, sliders)` adds them to the screen; slider 0x28 is the head preset, whose place it keeps at +0x114 (0x2A is the beard; corrected 2026-10-18). Read; what each slider id changes is not read.
- The screen's two beard controls (elements 5 and 6 of its list at +0x118) are hidden at the start and shown by `CharGenUpdateBeardMessage` (`0x00737050`) only when the current head preset's beard value is above 1e-6. That handler changes nothing on the creature. Read.

### The voice set

- `CharGenSelectAppearanceAndSoundSetMessage` (`0x00736e10`) raises the script event 48 (`CHARGEN_SELECT_SOUNDSET`) to the module with object 0 the creature and string 0 the sound set's name; natively it only refreshes the screen (`0x007fecf0`, `0x00803000`). The sound set is applied by the script. Read; the refresh's purpose inferred.

### Specializations

- `CharGenSelectSpecializationMessage` (`0x00736b80`) raises event 52 (`CHARGEN_SELECT_LEVELUP_CLASS`) to the module with object 0 the creature and int 0 the specialization, only when it is not 0; with the message's third field set it moves the screen to its step 4 (`0x007356f0(4)`) and rebuilds the sliders' data (`0x0080ac50`). Read.

### Saved heroes

- `CharGenSaveInfoMessage` (`0x007371c0`) writes the hero as a `.chr` file under `PLAYERS`, named from the hero's name (or `Hero`). Read.
- `UpdateHeroAppearanceMessage` (`0x00738550`) applies an entry of a list of saved heroes (the screen's +0x150, each with a name at +0x7C and a morph at +0x98) to the hero through `0x006d1850`, refreshes the hero (`0x007b8470(hero, 0, 1)`) and republishes `CharGenCreature`. That the list is the `.chr` files is inferred.

### What the engine shows behind the chargen movies (lib-agent-2, 2026-10-19, cgstage-scene)

Marks: [C] checked in the exe's code or in the game's files, [I] inferred. Addresses are GOG's. Prior work cited as file:line; the rest is new.

#### Short answers

1. The scene is a real area, not a Paperdoll scene of its own. `char_stage.are` (name strref 311656, layout `oth999d`, whose `.arl` calls itself `PartyPickerLevel`) is loaded as the module's `SharedCharacterStage` (a `CCArea` subclass, vtable 0x00B1592C, built by 0x007072E0 from the module's +0x350 name, default `char_stage` at 0x00C35E00; the party picker shares it). The module holds it at +0x388. The chargen screen's paperdoll (`GUI::GUIPaperdoll`, 0x320 bytes, built by GUICharGen at 0x0073553 via ctor 0x007BFE10) is put in "mode 1" (`SetMode` 0x007BBFF0, called with 1 at 0x00735581) and in mode 1 it does NOT use its own scene: it drives the game's camera over the stage. [C] Modes 0 (own scene, `SceneCamera` + `paperdoll_lightrig` = `el_lr_new_day2` + `paperdoll_lightprobe` = `el_lp_outside_day`, the same rig as the portrait renderer) are the inventory / character record / level-up paperdolls (0x007315C6 and 0x0073C397 set 0). Chargen never uses it. [C]
2. There are exactly two camera states, not one per page: preset 0 (whole body) and preset 1 (head close-up). The only sender of preset 1 is the appearance page (`AppearanceSoundSetScene.StartScene`, AS line 383 area: `Paperdoll.SetCameraPreset` 1); leaving it, forward or back, sends 0 (lines 358, 368, 376). Race/gender, class, origin (background), attributes, skills, spells/talents, review, specialization and name all show preset 0. [C] AS: `%TEMP%\lib-agent-2-guicmd-x\allgfx\as_chargen\scripts\__Packages\AppearanceSoundSetScene.as`.
3. Both presets are computed by code (`GUIPaperdoll::UpdateCamera(preset)`, 0x007B38A0), not by a `.stg` and not by portraits.gda. The numbers are below. The 0x32 to 0x39 sliders (Look Up/Down, Turn, Camera Distance, Move, Background, Expression) are the portrait record's, not the stage camera (CHARGEN.md:255). [C]
4. Lighting and backdrop are the area's: the level `oth999d`'s baked lightmaps, three character point lights, two level bounce lights, a grey sun, the `sb_newlayout` sky, the floor `hro_partypkr_floor`, three `hro_partypkr_arch` arches and the effect model `fxe_cha_sel_p` at the hero's feet, plus the area's bloom. The slider "Background 1..20" is the portrait tint (`PortraitTint`), not a model or texture of the stage [I]. [C]

#### Pseudo-code a port can copy (chargen = mode 1, game mode 0xB)

Coordinates: Z up, a creature at identity faces +Y, a quaternion is (x,y,z,w), angles in degrees, `YPR(yaw,pitch,roll)` is `Quaternion::FromYawPitchRoll` (0x004EE200; yaw about Z, positive pitch looks up) [I for the sign of pitch, the call and its argument order are C].

```
// Setup (0x007BB890, mode 1)
wp   = object with Tag "char_chargen" in the SharedCharacterStage
P    = wp.position            // paperdoll+0x130 = (-2.363824, -2.653543, 0.900991)
Rc   = YPR(wp.euler)          // paperdoll+0x120; wp orientation quat (0,0,0.258819,0.965926) = yaw 30 deg
creature.SetPosition(P); creature.SetOrientation(Rc)     // SetCreature 0x007BC110 at 0x007BC592-0x007BC5AB
gameCamera.SetMode(0)         // 0x006C2890 on the module's camera
UpdateCamera(0)               // first call: sets at once (no blend), state +0x1C4

// UpdateCamera(preset)  0x007B38A0
FOV   = 45                    // 40 when the game mode is not 0xB; 0xC0FD7C / 0xC0FD78
gameCamera.SetFOV(FOV, 0 s)   // 0x006C4B20; vertical degrees [I]
pitch = -10                   // paperdoll+0x164, 0xC0FD80
Rot   = YPR(180, pitch, 0)    // camera orientation, always yaw 180: looks toward -Y, 10 deg down
fwdY  = Rot * (0,1,0)         // = (0, -cos10, -sin10) = (0,-0.98481,-0.17365)
right = Rot * (1,0,0)         // = (-1,0,0)   (0x004CBCE0 returns this column)

if preset == 0:               // whole body
    h    = 1.9                // chargen: a fixed height; other modes use the creature's bounds, 3.8 for racial type 0xE, 2.1 for gen00fl_shale
    dist = 1.1 * (h/2) / tan(FOV/2)          // 1.1 = 0xC0FD64; = 2.52284
    cam  = (P.x, P.y, P.z + h/2)             // + 0xC38AF0 which is never written (0)
    cam -= Rot * (0, dist, 0)                // = cam + dist*(0, 0.98481, 0.17365)
    cam += 0.7 * right                       // 0xC0FD68 = -0.7 as the factor; the character sits left of centre
    // numbers: cam = (-3.0638, -0.1692, 2.2892), looks along (0,-0.98481,-0.17365), aim point on the hero ~ (P.x-0.7, P.y, P.z+0.95)
else:                         // head close-up (preset 1)
    H    = CCCreature::HeadTransform(0,0,0,1)   // 0x006A1EF0 then 0x00452EF0: the head's world position
    cam  = H + (0,0,-0.05)                      // 0xC0FD74
    cam -= Rot * (0, 0.65, 0)                   // 0xC0FD6C; = H + (0, 0.64012, 0.11287)
    cam -= (-0.19) * right                      // 0xC0FD70 in mode 1 (0 in mode 0); = +0.19 along right = (-0.19,0,0)
    // net: cam = H + (-0.19, +0.6401, +0.0628); same Rot and FOV; the head sits 0.19 m left of centre

cam target for both: the pivot at paperdoll+0x190 = P (+ (0,0, h/2 [+ -0.19 mode 1... see Not read])) — used only to blend, the orientation is Rot.

// switching presets: blend, not a cut (per frame update 0x007B4C40..0x007B4E08)
t = elapsed / 4.0             // duration = 0xC38AEC (never written, 0) + 4.0 (double at 0xAF0C88) seconds [I for seconds]
pos = lerp(from, to, t)       // 0x005B0DB0;  rot = slerp(...) 0x004CBC10; each frame gameCamera.SetPosition / SetOrientation(.., 0 s)
done when t >= 1 or |from-to| < 1e-6
// dragging the hero: AS multiplies the mouse delta by -0.8 and sends RotatePaperdollCamera(dx,dy) (CharGenDragArea.as:44).
// handler 0x007B2430: finishes any blend at once, then Rc = Rc * YPR(-dx_msg, 0, 0); the hero turns about Z, the camera stays.
```

For a port that wants the picture: hero at P facing 30 degrees left of +Y, camera about 2.5 m in +Y looking back along -Y, 10 degrees down, 45 degree vertical FOV, shifted 0.7 m to its right so the hero fills the left half (the chargen panel is on the right). For the face: 0.65 m away, 0.19 m shifted, same angles.

#### The stage the camera looks at [C unless marked]

- `char_stage.are` (GFF3, designerareas.erf): Tag `char_stage`, ScriptResRefID `area_core`, Layout `oth999d`, `NoTeleport` 1, MusicPreset empty (the previous theme plays on, AUDIO.md:251), AREA_ID 10010. No creatures, placeables or stages in it. One audio-reverb trigger "DA custom stone room" over 23.6 by 15.8 m with two ambient loops (`amb_ext_rumble1_lp` -4 dB, `amb_ext_subair1_lp`).
- Waypoints (all z 0.900991): `char_chargen` (-2.363824, -2.653543; quat z 0.258819 w 0.965926 = yaw 30) is the hero's mark and the camera's base; `char_player` (-2.628284, -3.132766; yaw 28), and the companions' marks `char_gen00fl_alistair` (-2.960, -4.019), `_morrigan` (-2.241, -2.384), `_wynne` (-3.612, -3.949), `_zevran` (-4.358, -3.220), `_leliana` (-1.307, -2.401), `_loghain` (-2.961, -4.252), `_sten` (-1.373, -3.650), `_dog` (-3.697, -2.346), `_oghren` (-1.418, -1.347), `_shale` (-4.593, -4.242): the party picker's. `char_chargen` is found by the wide string `char_chargen` (0xB21DB4) in the Setup loop 0x007BB9BA, matching the object's tag at +0xB8.
- Level `oth999d` (`.arl` in `packages/core/env`, one room `oth999d_0`, 24 by 16 m, z -0.09 to 13.04). Room at (-0.0108, -0.7669, 6.0188). Models: `hro_partypkr_floor` (lightmap `oth999d_Room000.dds`), three `hro_partypkr_arch` at x -7.99, 0, +7.99, y -5.22, bounds centre z 3.95 (lightmaps `oth999d_Room001/002.dds`), the effect model `fxe_cha_sel_p` (user param `fog` 0.30,0.25,0.25,0.75) at (-2.035, -3.843, 0.909), yaw 34.5, which is the platform glow at the hero's feet, and two light-only models `oth999d_0_lc` and `oth999d_0_ll`. Looking down -Y from the camera the centre arch and the floor are the backdrop.
- Sky `sb_newlayout`. Sun colour 0.5,0.5,0.5 (and the same for characters, "SunlightColorChar"), direction (0.577,0.577,0.577), not occluded. Atmosphere: fog colour 0.16,0.19,0.16, intensity 0.0081, cap 0.65; clouds colour 0.12,0.14,0.12, density 0.667. Room fog disabled. Wind `oth999d_47.wnd`.
- Lights in world coordinates (room + model + local), `oth999d_0_lc` (AffectDomain 1 = characters): `char_002` warm key (0.51, 0.3375, 0.24) radius 160 at (-4.594, -0.922, 2.420); `char_001` (0.068, 0.136, 0.112) radius 60 at (-0.045, -1.602, 2.179); `char_003` (0.11, 0.19, 0.17) radius 60 at (-2.033, -4.601, 2.265). `oth999d_0_ll` (AffectDomain 2 = level): two static points `bounce_2_RT`, `bounce_2_RT0`, colour (0.915, 0.69, 0.585), radius 3, local (-1.888,-0.362,-2.440) and (-0.159,-0.919,-2.347) from the model at (-1.877,-1.100,-1.110); ambient `Ambient0` (0.026,0.028,0.027); light probe `OTH999d_probe_o.dds` + `.mtx` (SH given in the file, near black). All from ghilana `oth999d_0_lc.mmh`, `_ll.mmh`, `.rml`.
- Frame effects of the area: `da_bloom` Brightness 0.5, Contrast 0.5, GlowIntensity 0.25, HBlurWidth 3, VBlurWidth 3; `da_hsvmatrix` Brightness 1, Contrast 1, Hue 0, Saturation 1 (via `tools/gff4.py --dump`).

#### Evidence

- Chargen creates and modes the paperdoll: 0x00735500-0x00735581 (`new` 0x320, ctor 0x007BFE10, `call 0x7bd030`, `push 1; call 0x7BBFF0`) [C]. `SetMode` 0x007BBFF0 is `this+0x140 = arg; Setup()` [C]. Other callers 0x007315C6, 0x0073C397 pass 0 [C]; which screens those are is [I] (level-up and import paths).
- Setup 0x007BB890: mode 1 branch 0x007BB8CF-0x007BBA34 (copies the module's stage +0x62C, activates the stage 0x00707040, iterates the stage's objects from 0x0076AE60, matches the tag `char_chargen`, converts its +0x130..0x138 Euler with 0x004EE200 into this+0x120, copies its +0x120 vec4 into this+0x130); mode 0 branch 0x007BBA39-0x007BBC44 (`SceneCamera` 0xADEEB4, `paperdoll_lightrig` 0xB21D8C from `el_lr_new_day2` 0xB21D6C, `paperdoll_lightprobe` 0xB21D40 from `el_lp_outside_day` 0xB21D1C) [C]. The mode 0 rig is the portrait's (lib-agent-2-portrait2-lights.md:16, 59, 69; GUI.md:1130).
- UpdateCamera 0x007B38A0 constants (floats read at): 0xC0FD64 1.1; 0xC0FD68 -0.7; 0xC0FD6C 0.65; 0xC0FD70 -0.19; 0xC0FD74 -0.05; 0xC0FD78 40; 0xC0FD7C 45; 0xC0FD80 -10; 0xC0FD84 180; 0xB219E0 1.9 (height in game mode 0xB, 0x007B3ACB); 0xB219BC 3.8; 0xB219C0 2.1 and 0xB0839C -0.1 (gen00fl_shale, 0x007B3AEC); doubles 0xAE70F8 0.5, 0xAF14C0 0.0174533, 0xAF0C88 4.0 [C]. Game mode via `GameModeController::GetGameMode` 0x00640060 == 0xB, the same value CHARGEN.md sets in `StartCharGen` [C].
- The camera calls: `SceneCamera::SetFOV` (0x006C4B20, sets +0xA0 when the time is 0), `SceneCamera::SetPosition` 0x006C45F0, `SceneCamera::SetOrientation` 0x006C4EC0, on the controller's camera at `[app+0x114]+0x5C` (0x007B3FBC-0x007B4052 and the per-frame 0x007B4D4D-0x007B4E03; same API as CUTSCENES.md and lib-agent-2-cutq-camera.md:30) [C].
- `SetCameraPresetMessage` handler 0x007B49B0 = `UpdateCamera(msg+4)` (messages.csv:241) [C]. `RotatePaperdollCameraMessage` handler 0x007B2430 [C for the quaternion product, I for the axis names].
- Both messages come from the movie only as `Paperdoll.SetCameraPreset` and `Paperdoll.RotatePaperdollCamera` (`SharedLibrary/CharGenDragArea.as:44`, `AppearanceSoundSetScene.as`); `Paperdoll.CameraMode` is read for the drag cursor only (dev/gui_data_paths.csv:553) [C].
- Arithmetic: 1.1 * 0.95 / tan(22.5 deg) = 2.5228; cam.x = -2.363824 - 0.7 = -3.0638; cam.y = -2.653543 + 2.5228 * 0.98481 = -0.1692; cam.z = 0.900991 + 0.95 + 2.5228 * 0.17365 = 2.2892 (pitch sign inferred).
- Prior notes used: CHARGEN.md:98-112 and :255 (the portrait camera and sliders, unchanged), GUI.md:461 (`char_stage.are` strref), :1130-1134 and lib-agent-2-portrait2-camera.md (the portrait scene and the default row: pitch -7.5, rotation 23.5, distance 0.675, Z 0.038, X 0.007, tint 1), lib-agent-2-scg-plotman.md:31 (module defaults `char_stage`/`partypicker`, the 0x6FC-byte stage), dev/object_casts.csv:2 (`SharedCharacterStage` is a `CCArea`), SAVES.md:67 (the guest's char_stage load and its ~100 objects). New: the paperdoll modes, the two presets and all their numbers, the stage's contents and lights.

#### Not read

- The sign convention of `FromYawPitchRoll`'s pitch (inferred from the camera ending 10 degrees down and aimed at the hero), and the exact rotation order; a play check with Eluvian's camera tool would settle it (the camera's orientation after chargen's first page).
- What the pivot at paperdoll+0x190 and +0x170/+0x1A0 (blend endpoints) is used for besides the blend; whether the 4 s duration is seconds (dt units not traced).
- The unit of the vertical-FOV argument (degrees inferred from 45/40 and tan(FOV/2) use).
- The head position H (the head bone's world position) is a runtime value; for the default human male about P.z + 1.6 is a guess.
- The Background slider's setter (0x0069ED50 and kin); "tint" is from the range 1..20 and the portraits.gda `PortraitTint` column (default row 1; Alistair 17, Dog 18, Morrigan 16), and GUI.md:550/975 (the portrait frame's tint). Which colours the 20 tints are was not read.
- Whether the stage's party-picker model `fxe_cha_sel_p` is shown during chargen: it is a room's placed effect model, loaded and run as any other (`dev/EFFECTS.md`, "An effect a level places", 2026-10-20), so it is there unless something hides it; what might hide it was not traced. And how the game hides the previous area while the stage is shown.
- How the stage is loaded and entered at `StartCharGen` (0x00707040 is its activation; the loader calls 0x005D6B30, 0x0067C3B0, 0x0067B3A0, 0x0067F5C0 were not read).
- The mode 0 paperdoll's camera (inventory etc.): only the shared constants were read.

### Chargen stage: the figure and the portrait circle (lib-agent-2, 2026-10-19)

Marks: [C] checked in the GOG exe (disassembly, `tools/daoexe.py`, capstone scans), the scripts, the AS or ghilana; [I] inferred. Addresses are GOG VAs. Scratch: `%TEMP%\lib-agent-2-cgfigure-*`. No game, no Ghidra.

#### Short answers

1. **The figure is not the hero.** It is a second creature, the paperdoll's `PaperCreature` (a fresh `CCCreature`, tag `"paperdoll"`, id -1), built as a clone of the hero from the hero's CAppearance record (`hero+0x7B0`), the hero's head-morph name (`hero+0x96C`) and the hero's equipment, and given to the movies as the token `Paperdoll.PaperCreature`. The hero's own 3D model is switched off for the whole screen (`hero+0x7B4 = 0`; `0x006C1400`, the model build, returns at once while that byte is 0) [C for the writes and the early return, I for the meaning]. No template is loaded: the hero is whatever the scripts made (`Chargen_InitializeCharacter`, `Chargen_SelectRace`, ...) and the figure follows it. [C]
2. **Outfit.** There is no class preview outfit in the engine. What the figure wears is what the hero has equipped, and the scripts equip it: race chosen: `default_player.utc`; class chosen: `default_warrior/rogue/wizard.utc`; background chosen: the `Template` of `background_defaults.gda` (table 1001) row for race+class+background (`Chargen_GetEquipIndex`). Each is `LoadItemsFromTemplate(hero, tmpl, TRUE)` after `Chargen_ClearInventory`. `default_warrior.utc` equips only `gen_im_cth_com_a01` (common clothes). The engine's `EquipmentChangedMessage` then rebuilds the figure. [C: sys_chargen.nss:200-300, sys_chargen_h.nss:423-470, ghilana default_warrior.utc, table 1001]
3. **Rebuild granularity.** Whole creature (new `PaperCreature`, old one destroyed, old one's pose and portrait settings carried over): race, gender, class, background (via GUICharGen `0x007356F0` -> `0x007BC110`), hero reset, hero import / saved-hero pick, and every equipment change. Head only (the live `PaperCreature` keeps its body, animation and renderer; its head morph is swapped by name): every appearance slider 0..0x2D (presets, morph weights, hair, beard, eyes, tints, tattoo, makeup) through `CommandCreateMorph` + `CommandApplyMorph`. Portrait camera sliders 0x32..0x39: only the paper creature's portrait settings, then a portrait re-render. [C]
4. **Pose.** The paper creature plays animation row **1 `pause`** (the normal idle loop, `Pause`, AnimName `pause`) through `CCCreature` vtable slot 44 (+0xB0) as `Play(1, speed 1.0, blend -1.0, 1)` (blend -1 is a hard cut) [C]. Idle twitches: a timer in the paperdoll's per-frame update plays row `640 + rand()%5` (`pausetwitch_v1..v5`) with blend -2.0 every 10 to 49 s, except while the camera mode (`Paperdoll.CameraMode`) is 1, the appearance page's close view [C]. Not per race, gender or class by the engine: the animation tree behind row 1 is the creature's own [I].
5. **Facing and place.** The creature's scene node is set to position (0,0,0) (that is mode 0's, the inventory's own scene; in chargen, mode 1, `SetCreature` 0x007BC110 takes the position from paperdoll+0x130, which Setup fills from the waypoint `char_chargen`, 0x007BC592–0x007BC5AB: corrected 2026-10-19 by lib-agent-2) and orientation = the paperdoll's quaternion (+0x120), initially a rotation of **30 degrees** (`0x004EE200(30.0, 0, 0)`, its first argument being the turn about the vertical, as in the rotate handlers) [C]. Hold-to-rotate (`RotatePaperdollLeft/Right`) turns that quaternion at 25 degrees a second [C: the constant, I: the sign]. There is no waypoint. In chargen the paperdoll is in its mode 1 (`GUICharGen` open sets it, `0x00735581` -> `0x007BBFF0(1)`), whose scene is the current area's scene (`world->areas(+0x388)+0x62C`; the `char_stage` area), so (0,0,0) is the stage area's origin [C for the mode and the scene pointer, I that this is char_stage's origin; the other reader has the stage].
6. **Look-at.** Not settled. The model build turns the look-at controller on (mode 1, automatic) unless creature vtable +0x1D0 is true (`0x006BE23E..0x006BE245`), but the paperdoll's own per-frame update (`0x007B4BB0`) never calls the look-at update (`0x0069EAF0` -> `0x006CAD80`); only `0x0069A600(paper, 0)`. So eyes following the camera is NOT something the paperdoll does itself [C for what the update calls; I that no look-at runs on it]. Safe port behaviour: none.
7. **The portrait circle** is a render of the **paper creature** by `PortraitRenderer`, not a crop of the stage view. `AppearanceSoundSetScene.as:317-320` and `ReviewScene.as:62-64`: `Portrait_mc_init` -> `SetCreatureToken("Paperdoll.PaperCreature")`; `SimplePortrait` observes `Paperdoll.PaperCreature.Portrait` and loads it with `MovieUtils.LoadImageFixedSize(PortraitPicture_mc, name, w, h)` (so `img://portrait_paperdoll<N>.dds?Width=..`, N the global renderer counter), `blendMode = "overlay"`, hidden when the value is empty or ends in `?` [C for the AS; I for the exact name from `"portrait_" + tag + counter` with tag `paperdoll`]. The backdrop ring is `PortraitProperties.PortraitTint` from the same creature (frame number). The attribute page (`AttributesScene.as:42-55`) instead loads `CharGenCreature.Portrait` (the hero) into `PortraitPicture_mc.Image_mc` at 146x146 [C].

#### Pseudo-code a port can copy

```
// state: hero (the script's creature), paper (PaperCreature, null at first), gen (int), yaw = 30 deg, spin = 25 deg/s,
//        cameraMode (0 = body view, 1 = appearance close view), twitch = 10 + rand()%40 seconds

onChargenOpen():                       // GUICharGen 0x007354D0
    paperdoll.mode = 1                 // draw into the stage area's scene
    hero.model_build_enabled = false   // hero+0x7B4 = 0: only the paper creature is shown
    releasePortrait(hero)              // 0x0069F470
    refreshFull(hero, loadMorph=true, syncPose=false)

refreshFull(hero, loadMorph, syncPose):            // Refresh 0x007B8470, ret 0xC; (hero,1,1) reset, (hero,0,1) import/race.., (hero,0,0) equipment
    gen += 1
    if loadMorph: editor.loadFromHero(hero); queue CreateMorph(gen)      // head recipe -> morph resource "Head_Morph<N>"
    queue CreateCreature(gen, hero, syncPose)

CreateCreature.update():                           // 0x007C0BF0, polled every frame until done
    if hero != paperdoll.hero or gen != paperdoll.gen: discard partial creature; return     // stale: only the newest rebuild finishes
    if not started:                                // 0x007C1061
        c = new CCCreature(id=-1)
        c.setAppearanceType(copy(hero.appearance).type, hero.gender)       // 0x00A63D10 on c+0xA30
        c.setTag("paperdoll")                                              // vtable +0x1C0
        c.loadAppearance(copy(hero.appearance))                            // 0x006C1E00 -> 0x006C1400 (parts, armour, head from CAppearance)
        started = true; return
    if c.isStillLoading(): return                                         // 0x00A5CB20
    c.node.position = (0,0,0); c.node.orientation = paperdoll.orientation   // vtable +0x14 / +0x1C of the scene node
    c.setHeadMorph(hero.headMorphName)                                     // 0x006BD0E0, hero+0x96C
    c.node.flags |= (0x1A8 = 1)
    c.play(anim=1 /*pause*/, speed=1.0, blend=-1.0, flag=1)               // slot 44
    old = paperdoll.PaperCreature
    if old:
        if not syncPose and old.hasModel:
            c.play(old.currentAnim, 1.0, -1.0, 1); c.setAnimTime(old.animTime)   // continue the pose across the rebuild
        if old.portraitRenderer:                                           // old+0x78C
            c.portraitSettings = copy(old.portraitSettings)               // 0x007B2730: pitch,yaw,dist,Zadj,Xadj(+0x58..0x68), tint(+0x6C), expression(+0x7C)
            updatePortrait(c)                                              // 0x006AE2D0: new renderer, new "portrait_paperdoll<N>"
        destroy(old)                                                       // 0x007B2230
    paperdoll.PaperCreature = c ; paperdoll.created = true
    if gameMode == CHARGEN and paperdoll.cameraMode == 1: applyCameraPreset(1)    // 0x007B38A0
    done

onSlider(id in 0..0x2D except portrait ids):       // setter ends in 0x00802BF0
    editor.set(id, value)
    queue CreateMorph(gen) (coalesced if one is still waiting)
    queue ApplyMorph(n)                              // 0x007C1390: paper.setHeadMorph(editor morph name); NO new creature, NO animation reset;
                                                     // if cameraMode==1 it also toggles model parts 3,4 (0x007B2D80, hide helmet-like parts: I)

onPortraitSlider(id 0x32..0x39):                   // setters 0x007FF310 ...: write paper.portraitSettings.{pitch,yaw,dist,Zadj,Xadj,tint,expr}
    renderPortrait(paper, pose=0, camera=1, flag=1)  // 0x0069ED50 = Render(0,1,1) if the creature has a renderer

onAppearancePageStart():                           // AppearanceSoundSetScene.StartScene:380-390
    GUICharGen.CharGenStartAppearanceScene          // 0x00739D20: head reset on hero appearance (0x0074BA60), editor reset,
                                                    //   paper.portraitSettings = portraits.gda row "default" (+0x598 block),
                                                    //   paper.flag7D4 = 1 (needed by UpdatePortrait), updatePortrait(paper)
    Paperdoll.SetCameraPreset 1                     // camera mode 1: close view, twitch disabled
    Paperdoll.UpdatePortrait                        // 0x007BC6C0: hero.appearance+0x1E4 = paper head morph name; UpdatePortrait(paper); clear it

perFrame(dt):                                      // 0x007B4BB0
    if cameraMode != 1: twitch -= dt
    if twitch <= 0: paper.play(640 + rand()%5, 1.0, -2.0, 1); twitch = 10 + rand()%40
    serviceComponents(paper)                        // 0x0069A600(paper, 0)
rotateLeft/Right(dt): orientation = q(yaw += -/+ 25*dt) * orientation; paper.node.orientation = orientation

portraitCircle: texture = renderTarget(146,146) registered as bare name "portrait_paperdoll<N>"; token Paperdoll.PaperCreature.Portrait = that name;
    movie: LoadImageFixedSize(PortraitPicture_mc, "portrait_paperdoll<N>" -> img://portrait_paperdoll<N>.dds, w, h), blend "overlay"; backdrop frame = PortraitTint
    clone for the render: UpdatePortrait(p1=1) copies appearance+equipment to a hidden clone and PlaceCamera(Default-row-style values from the creature's OWN settings because tag == "paperdoll"); Render(p1=0) instead plays portrait_exp_m (677) / portrait_exp_f (678) on the clone
```

#### Evidence

##### The figure

- `GUIPaperdoll` fields (from the code): +0x58 the hero (raw pointer), +0x5C the `PaperCreature` attribute (`0x0053CC40` gets the pointer, vtable +0x54 sets), +0x6C created flag, +0x84 the scene, +0xC0 (in GUICharGen) the hero's id, +0x11C rotate speed, +0x120 orientation quaternion, +0x130 position (zeros), +0x140 mode, +0x144 `CameraMode` attribute, +0x1CC twitch timer, +0x2DC command queue, +0x2F0 generation, +0x2F4 apply-morph counter. [C]
- Refresh `0x007B8470(hero, loadMorph, noSync)` (`ret 0xC`): `++[this+0x2F0]`, `[this+0x58] = hero`, in game mode 0xB `hero+0x7B4 = 0`; if arg 2 is non-zero it re-reads the head from the hero (`0x00808040`, `0x007FDCF0`, `0x0080AB80`, `0x00805F20`, `0x0080AB30`) and queues `CommandCreateMorph` (vftable `0x00B21848`, 0x1C bytes); it always queues `CommandCreateCreature` (vftable `0x00B218B8`, 0x210 bytes, ctor `0x007B2070(gen, hero, paperdoll, arg2, arg3)`, which copies the hero's appearance record `hero+0x7B0` into the command at +0x10 with `0x0074A610`). The commands run oldest first from the queue; a command whose `(hero, gen)` no longer match the paperdoll's (`0x007C0C32..0x007C0C47`, jumping to `0x007C1161`) deletes what it built and ends. [C]
- Callers of Refresh (found with a `call` scan): `0x007315AD` (0x731280, (creature,0,1)), `0x007384C4` CharGenResetCreature (hero,1,1), `0x00738624` UpdateHeroAppearance, `0x00738785` ImportHero, `0x0073C37E` (0x73C1D0, (creature,0,1)), `0x007B88ED` EquipmentChanged handler 0x7B8880 (creature,0,0). With the last, arg 3 is 0, so the command's byte +0x20D is 0 and the pose is synced from the old creature (below). [C]
- Start of the command (`0x007C1061`): `new` 0xD20 bytes + ctor `0x006B9ED0(-1, 0, 1)`; `0x00A63D10` on `creature+0xA30` with the appearance type word (`appearance+0x58`) and the gender flag; `0x00694DB0(creature, 0x007490D0(appearance,0))` (the appearance's body/gear class, from table 2 column `0x5D092182`; I); `SetTag` via vtable +0x1C0 (`0x006A2170` writes `creature+0xB8` into the tag attribute) with the global string `0x00C38B24` = `paperdoll` (static init `0x00AD2E70`, string `0x00AECC9C`); `0x006C1E00(&appearanceCopy, 1, 0, 1)` -> `0x006C1400` model build; `0x00695220(0xE)`; vtable +0x144(0). [C]
- Finish (`0x007C0C57..0x007C0FB2`): wait while `0x00A5CB20(creature->vt+0x60 object)` says loading; scene node (`0x006958D0` result + 8) vtable +0x14 with a zero vector and +0x1C with `paperdoll+0x120`; `0x006BD0E0(creature, hero+0x96C, 0)` (set the head morph by the resource name string); node byte +0x1A8 = 1; Play slot 44 with `(1, 1.0, -1.0, 1)` (floats `0xAE2E00` = -1.0); if the old `PaperCreature` exists and `[cmd+0x20D] == 0`: Play(old's `+0x13C` animation id, 1.0, -1.0, 1) and set its time scaled from the old's (`0x00A5F980`, `0x00A5C980`); if the old has a portrait renderer (`+0x78C`): copy its `+0x598` settings (`0x007B2730`: floats +0x58..+0x68, attribute +0x6C, float +0x7C) then `UpdatePortrait(new)` (`0x006AE2D0`); `0x007B2230` removes the old (removes it from the scene, deletes it if `+0x6C`, sets the attribute to null); the attribute is set to the new one; `+0x6C = 1`; `0x006951D0`, vtable +0x104 (add to `paperdoll+0x84`), +0x134(1); in game mode 0xB with `CameraMode == 1` it calls `0x007B38A0(1)` (else `0x007B38A0(0)`); finally `0x007C0190`. [C]
- Animation 1: ghilana Animations row 1 `Pause`, AnimName `pause`, Type 1, TransitionTo 1. Slot 44 is `Play(anim, speed, blend, 1)` (dev/ANIMATION.md:359, :456-463); `0x0069B3D0` adds the speed jitter `(rand%90)/100 - 0.45` to a play of row 1 (dev/ANIMATION.md:908), so the figure's pause plays at 0.55 to 1.44 times speed. Rows 640 to 644: `pausetwitch_v1..v5` (ghilana). The twitch rule: dev/ANIMATION.md:896-898 (0x007B4BB0, double `0xAE7D48` = 10.0, `rand()%0x28` seconds added). [C]
- Constructor `0x007BFE10` area: orientation `0x004EE200(0xADEBCC = 30.0, 0, 0)` into +0x120; speed `0xAE7528` = 25.0 into +0x11C (the float at +4 there is 100.0, not used here); +0x130..0x138 = 0; +0x140 = 0; twitch timer from `rand()%0x28` + 10.0. Rotate handlers `0x007B2310` (left) and `0x007B23A0` (right): angle = frame time (`[0xC31650]+0x48`) x `+0x11C` x -1.0 (double `0xAE7118`; right negates first), quaternion from `0x004EE200`, multiplied in with `0x004CB640`, stored at +0x120 and applied with `0x007B22A0` (node vtable +0x1C). The multiplication order and the sign were not tested in play. [C for the numbers, I for the direction]
- The mode: `GUICharGen` open (`0x007354D0`: creates the paperdoll `0x007BD030`, `0x00735581 push 1; call 0x007BBFF0`), the level-up GUI (`0x0073C397`) and `0x007315C6` pass 0. `0x007BBFF0` stores +0x140 and calls `0x007BB890`, whose mode-1 branch (`0x007BB8CF..0x007BB8F3`) takes the scene from `world->0x388 area +0x62C` instead of making an own scene (`0x007BBA99`, via the renderer's vtable +0x34). [C]
- Race, gender, class and background changes: the movie sends `GUICharGen.CharGenSelectGender/Race/Class/Background` with a trailing "update appearance" flag (`CharGen.as:285-297`); the handler runs `0x007356F0(0)`, which does `0x007BC110(paperdoll, hero, 1, 1, 0)` unless `0x007FEF20` is true, after clearing `hero+0x7B4`. `0x007BC110` has two bodies: one that reuses a cached paper creature from a global list (`0xC38B00`, compared by the appearance bytes +0x50 and +0x54 and the `+0x7C4` attributes: `0x007BC1B0..0x007BC295`) and one that builds synchronously (`0x007BC40A..0x007BC5B1`) with the same recipe plus `Play(1, 1.0, -1.0, 1)` and, in mode 1, position `+0x130` and orientation `+0x120`. Which of the two a given change takes was not traced; the list's purpose (the hero-import "seeded characters", `Paperdoll.SeededCharacters`) is inferred. [C for the code, I for the purpose]
- Head sliders: setters end in `0x00802BF0`, which queues `CommandCreateMorph` (unless one is waiting unstarted for the same hero: `[this+0x2EC]`) and `CommandApplyMorph` (vftable `0x00B24D88`, built at `0x00802C96`, counter `[this+0x2F4]`). Apply (`0x007C1390`) rebuilds the morph name, calls `0x006BD0E0(paperCreature, morphName, 0)`, sets node byte +0x1A8, notifies the attribute `0xC38AF4`, and with `CameraMode == 1` calls `0x007B2D80(0)` (which, for model parts 3 and 4 of the creature, calls their vtable +0xA0 (+0x9C for 1): hide / show; I) and, for the newest apply, `0x007C0190`. It does not call `UpdatePortrait` or `Render`. [C]
- `0x007C0190` builds a second paperdoll for a 256 x 512 render target named `CharGenScreenshot` (string `0xB21F10`; `0x00847330` with 0x100, 0x200): the full-body picture for the saved hero (`.chr`, `HeroEntry.Screenshot`). Not read further. [C for the size and name, I for the purpose]

##### The portrait

- Chargen start of the appearance scene (`0x00739D20`): `hero+0x7B4 = 0` (`0x00739E14`), `0x0074BA60` head reset on the hero's appearance, `0x007FEED0`, then it copies the Default row's camera values (read through the table with hash `0x39950924` and others) into **`[paperdoll+0x5C].PaperCreature + 0x598`** (`0x00739FDB..0x0073A042`: floats at +0x58, +0x5C, +0x60, +0x64, +0x68, attribute +0x6C, float +0x7C), `0x007C0190`, `0x0069E480(paper, 1)` (stores `creature+0x7D4 = 1`, which `UpdatePortrait`'s prepare step `0x006ADA50` needs) and `UpdatePortrait(paper)` at `0x0073A08B`. So the settings and the renderer belong to the paper creature, not the hero. This corrects dev/CHARGEN.md:112 ("the creature's portrait camera"): it is the PaperCreature's. [C]
- `UpdatePortraitMessage` (`0x007BC6C0`, sent by the movie's `Paperdoll.UpdatePortrait`, only found at `AppearanceSoundSetScene.as:384`): if the paperdoll is ready (+0xD9), it stores the figure's current head-morph name (`model vtable +0xE4`) in `hero.appearance+0x1E4`, calls `UpdatePortrait(PaperCreature)` and clears the field again (`0x007BC73E..0x007BC77C`). [C]
- `PortraitRenderer::Render` (`0x007F46E0`): for a creature tagged `paperdoll` or `player` the camera comes from the creature's own `+0x598` settings (`0x007F4938..0x007F4A14`; dev/GUI.md:1295-1297, :1336), so the Default-row copy above is what frames the circle until the portrait sliders change it. With `p1 = 1` (UpdatePortrait) it refreshes the hidden clone from the creature's appearance and equipment (`0x007F4440`); with `p1 = 0` (the slider path) it plays row 677 `portrait_exp_m` (678 `portrait_exp_f` when `0x006A3650(creature)` returns 2) on the clone with `Play(id, 1.0, 0.0, 1)` and sets the clip's time from the settings' `+0x7C` expression value. Ghilana: 677 "Portrait Expression - Male" AnimName `portrait_exp_m`, 678 "... Female" `portrait_exp_f`. [C]
- Portrait sliders 0x32..0x39: setters at `0x007FF310` (+0x58 look pitch), `0x007FF350` (+0x5C yaw), and the others down to `0x007FF4E0` (+0x7C expression); each writes the float into the paper creature's `+0x598` block and calls `0x0069ED50`, which is `Render(0, 1, 1)` if the creature has a renderer (`+0x78C`). 146 x 146 render target and the registered bare name: `lib-agent-2-gui3-portrait.md` section 2 and 3 (:15-19, :31-34). [C]
- AS: `SimplePortrait.as:15-35` (observer on `<token>.Portrait`; `LoadImageFixedSize`; overlay blend; hide on empty or `?` suffix), `Portrait_mc.as:1` (class registration), `AppearanceSoundSetScene.as:317-320`, `ReviewScene.as:62-64`, `AttributesScene.as:42-55` (`CharGenCreature.Portrait` at 146 x 146; backdrop `PortraitProperties.PortraitTint` from `Paperdoll.PaperCreature` if that has the property, else `CharGenCreature`). The name is `"portrait_" + tag + counter` (`lib-agent-2-gui3-portrait.md:7`), so for the figure `portrait_paperdoll<N>`; every new `PaperCreature` that gets `UpdatePortrait` makes a new renderer and so a new N (the counter moves per renderer creation), and the token path changes with the `PaperCreature` attribute. [C for the AS and the name rule, I for the exact string]
- Prior work reused: dev/CHARGEN.md:111-113, :168-169, :185-188, :255; dev/GUI.md:1123, :1295-1297, :1336; dev/ANIMATION.md:896-908 and :728-757 (look-at); `lib-agent-2-gui3-portrait.md`; `lib-agent-2-chargen-events.md`; DAOTurnBased dev/UI.md:53 (RenderPortrait). New in this note: the PaperCreature as the figure and as the portrait's creature, tag `paperdoll`, the command/generation machinery and the events that use it, animation 1 and the twitch ids, the 30 degree start and the 25 degrees a second turn, mode 1's scene, the equipment source in the scripts.

#### Not read

- Whether the head sliders re-render the circle: no direct call exists in `CommandApplyMorph`, and `0x006BD0E0` was not followed into the model to see if it reaches `0x006BDC70` (which does `Render(1,1,1)` for a creature with a renderer). A play check with Eluvian (`data` on `Paperdoll.PaperCreature.Portrait` before and after a slider) settles it.
- The cached-creature path of `0x007BC110` and which race/gender/class change takes it; what `0x007B38A0` (the camera preset) and `0x007B2D80` do exactly (the camera is the other reader's); `0x007C0190`'s second paperdoll.
- Whether any look-at update runs on the PaperCreature (the update is a virtual, `0x0069EAF0`, not called by the paperdoll); what vtable +0x1D0 (`0x006A3660`) tests.
- The sign and composition order of the rotation quaternion, the unit of its first argument (taken as the turn about the vertical from the rotate handlers), and the node space of (0,0,0) in `char_stage`.
- `0x0069A600(paper, 0)`: a per-frame service of the creature's components (calls vtable +0xB8 on each of a global list); what it does for the paper creature was not read.
- What `CharGenCreature.Portrait` (the hero) holds on the attributes page, since the hero's renderer is released when the screen opens (`0x00735599` -> `ReleasePortrait`) and the only `UpdatePortrait` calls found in chargen are on the PaperCreature.
- The exact outfit per race/class/background (`background_defaults.gda` rows' templates were not opened beyond the first rows); weapons' draw state on the figure.

### chargen review page: who fills sName (cgreview)

Paths below are under `%TEMP%\lib-agent-2-guicmd-x\allgfx\as_chargen\scripts\__Packages\`. Output file requested was `lib-agent-2-cgstage-review.md`; see the copy at the end of this task (same text).

#### Short answers

1. The engine does NOT fill `sName` [checked]. Nothing in the exe names it: `daoexe.py --strings sName` finds only `ResName` substrings (0x00af9d90, 0x00afcaa0), and there is no `nNameStrRef` string in the exe. dev/GUI.md:372 (master): the engine writes only `_root.fsc_retval`, `_root.Filename`, `_root.OriginalStageWidth/Height`, `_root.WaitingToBeStopped` by SetVariable.
2. The movie does NOT fill it either [checked, whole chargen AS tree grep for `sName`]: `RaceGenderScene` builds each entry with `nNameStrRef` (and nID, nIndex, nType, sIcon, nDescriptionStrRef, sLabel, bDisabled, oScene) but never assigns `.sName` (RaceGenderScene.as:288-297 genders, 314-323 races, 340-349 classes, 376-385 backgrounds). The only `x.sName =` assignments in chargen are LevelUp.as:209 (its own class list) and Ability.as:40 (abilities). So `ReviewScene` reads a property no one sets.
3. Consequence in vanilla [inferred, not played]: `ReviewScene` Race_mc/Gender_mc/Class_mc/Background_mc call `SetText(undefined)`; `SharedLibrary/TextField.as:22-26` does `if(a_sText == null) a_sText = ""` (undefined == null is true), so the four lines are BLANK in vanilla's review page. The peer's "sName stays undefined" is therefore vanilla behaviour, not a port fault. Level ("1") and the hero name (`AppearanceSoundSetScene.m_sCharacterName`) work.
4. Other pages resolve the name correctly through `ExternalCommands.GetTlkString(nNameStrRef)`; no data-tree token is involved for race/gender/class/background.

#### AS chain (file:line)

- Review lines: ReviewScene.as:86-105: `Race_mc_init`: `this.SetText(RaceGenderScene.m_aRaces[RaceGenderScene.m_nRaceIndex].sName)`; Gender_mc (90-93, `m_aGenders[m_nGenderIndex]`), Class_mc (94-97, `m_aClasses[m_nClassIndex]`), Background_mc (102-105, `m_aBackgrounds[m_nBackgroundIndex]`). Labels (66-85) are `Localization.Strings.LOC_RACE` etc.
- Indices set in RaceGenderScene.as:409 (gender), 432 (race), 447 (class), 474 (background) when the player picks; reset to INDEX_INVALID at 244-247.
- Entries built in RaceGenderScene.as:279-385, nNameStrRef from the 2DAs: gender `Get2DAValueByRowIndexColumnHash(TABLE_GENDER,id,HASH_NAME)` (:292); race `TABLE_RACE ... HASH_NAME` (:318); class `TABLE_CLASS ... HASH_NAMESTRREF` (:344); background `Get2DAValueByRowIndexColumnName(TABLE_BACKGROUND_NAMES,row,race.sLabel)` (:380).
- Resolution where it does work: RaceGenderScene.as:411-422, 433-434, 448-449, 475-476 and InitSubHeader :689: `SubHeaderText_mc.SetText(ExternalCommands.GetTlkString(entry.nNameStrRef))`. CharGenSpecButton.as:46 likewise.
- GetTlkString: ExternalCommands.as:185-190: `_root.fsc_retval = ""; fscommand("GetTlkString", id + " " + fallback); return _root.fsc_retval;`.

#### GetTlkString contract (port rule)

- Argument: ONE string, `"<id> <fallback>"`, fallback optional and may contain spaces; note the movie sends `"<id> undefined"` when it omits the fallback (the string concatenation of an undefined arg) [inferred from the AS; GUI.md:241, 301].
- Answer: `_root.fsc_retval` set to a plain STRING (GFx wide string, type 5), not an array (GUI.md:242, 301, 557; handler `GuiFSCommand::GetTlkString` 0x0051EAE0). Arrays are only for ListTactic*/ListMemberNames. It must be set synchronously before `fscommand()` returns; the AS pre-sets `""`, so a handler that finds nothing leaves `""` (the engine uses the fallback text if the id is invalid).
- Number type matters upstream: the movie does `Number(Get2DAValue...)` on the 2DA answers, so Get2DAValue* must return the cell text/number (a string "NNN" or number both survive `Number()`); a non-numeric answer gives NaN, and GetTlkString("NaN ...") then returns the fallback/"" .

#### Port rule

- To match vanilla: implement GetTlkString (string answer) and the 2DA getters and leave the review lines blank, since `sName` is never defined. Nothing to fix on the port side if the lines are blank.
- To show names on the review page (a deliberate improvement over vanilla): do not touch the engine; patch the movie's data after `RaceGenderScene.Initialize` builds the arrays, e.g. in the port's loader: for each entry in `RaceGenderScene.m_aRaces/m_aGenders/m_aClasses/m_aBackgrounds` set `entry.sName = ExternalCommands.GetTlkString(entry.nNameStrRef)` (a string, not an array), before the Review scene's `*_mc_init` run (Review is played after the appearance scene, AppearanceSoundSetScene.as:344 / SpellsTalentsScene.as:220), or rewrite ReviewScene.as:88/92/96/104 to call GetTlkString on `.nNameStrRef`. Label it a port enhancement, not vanilla.
- Wrong answers: if GetTlkString returns an array or number the TextField does `String(a_sText)` and shows e.g. "12345" or "a,b"; if it returns null/undefined the line is blank.

#### Not read

- No play check of the vanilla review page (blank lines are inferred from the AS); no Ghidra read of 0x0051EAE0 beyond GUI.md's note; whether the review page's Race_mc symbols are SharedLibrary.TextField (assumed from SetText's null handling; the symbol-to-class link table was not decoded); the fallback text actually sent when the movie omits the second argument.

## Level-up

Read 2026-10-19 by lib-agent-2 for paragon-agent's screens, in GOG's exe with `tools\daoexe.py --disasm`, the scripts' source and `as_levelup`'s ActionScript; the tables through ghilana. [C] read, [I] inferred.

### The ability chains (FindAbilityChains)

- **The lists are flat, one member per ability.** `CharGenCreature.Stats.TalentChains`, `SpellChains` and `SkillChains` are `ECDynArray<AbilityGUIInfo*>`. Each member is one AbilityGUIInfo (0xA8 bytes, constructor 0x0075B940) of five fields 16 bytes apart: `AbilityTemplate` +0x58 (the abi record), `Column` +0x68, `Row` +0x78, `DisplayState` +0x88, `AbilityInstance` +0x98 (the creature's instance, 0 if none) [C for offsets and contents]. The names are from `src/dao/Data.cpp` and Eluvian's `Character.cpp`, not the exe, whose attribute names are hashes. So Data.cpp's "each chain 5 members" is the five fields of one ability.
- **Order:** a pre-order walk of a tree built from `PrereqAbility`, so an ability is followed by the one that has it as `PrereqAbility`; siblings in abi row order [C for the walk, I for the row order]. `Column` is the chain's running index, up by one when the walk starts a sibling after a finished subtree; `Row` is the depth, 0 for the first rank, hidden GUIType 12 ancestors not counted [C]. The movie reads neither: it rebuilds chains from the list's order and `PrereqAbilityID`.
- **The handler**, `GameModeGUI::HandleMessage_FindAbilityChainsMessage` 0x00718090: message +4 creature, +8 type, +0xC mode; creature → stats (0x0069FE20) → 0x00719A90 → 0x0062E9C0 [C]. That needs stats +0x1B4 (the interface's ability object; without it nothing happens), builds the trees once (0x0062E050), picks by type 1 talents / 2 spells / 3 skills the root 0xC34118 / 0xC34104 / 0xC340F0 and the list at +0xB0 / +0xCC / +0xE8, clears the list, walks (0x0062DCD0, column 0, row 0) and notifies (the list's vtable +0x24). Any other type leaves the list as it was [C].
- **The trees**, built once per process (flag 0xC340D8) by 0x0062E050 from every abi row with `ID > 0`, `UseType != 0` and a record (0x0062C730), split by the record's `AbilityType` (+0x68): 1, 2, 3; item abilities (4) are never listed [C]. 0x0062DB40 attaches each row under the row whose ID is its `PrereqAbility` (record +0x168; 0 is a root), and writes the depth into the record's +0x1F8, the computed rank that the combat-training test reads (`dev/script_commands/stats.md`); depth does not grow under a GUIType 12 row (0x0062DC64) [C].
- **The walk** (0x0062DCD0) emits a node with a record whose GUIType (+0x1B8) is not 12, and walks a child only if (0x0062DE76..0x0062E01A) [C]:
  - `ShowInCharGen` (record +0x88, hash 0xECE22F72) & mode; the mode is a mask: 1 chargen, 2 level-up, 4 spellbook (the movie's `DISPLAYMODE_*`). Nothing else differs between chargen and level-up in this call.
  - a GUIType 12 (hidden) child is one the creature has. That is the class gate: every warrior talent chain hangs under `HIDDEN_WARRIOR` 4022.
  - its guitypes row's `Flags` bit 0 is clear, or the creature's appearance `CombatModelTree` holds "combat_humanoid" (0x006A25A0) [C; the appearance table's meaning I].
  - its guitypes `Ability` is 0 or one the creature has (the class and specialization GUITypes 101 to 117).
  - its guitypes `Orphanage` is 0, or the creature has the ability (racial, traits, plot, monster: shown only if had).
  - There is no race filter.
- **DisplayState** (0x0062DD93..0x0062DDC2) [C]: 1 if `HasAbility` (0x0071BEC0), else 2 if 0x0071BEF0 passes, else 3. 0x0071BEF0 tests the record, level (property 15) ≥ `PrereqLevel`, `PrereqAbility` 0 or had, the `PrereqAttribute` total ≥ `PrereqAttValue`, and, for a talent whose GUIType has `CombatTraining`, rank ≤ the combat-training rank (skills 100100 to 100103 give 1 to 4). **It reads no points**, neither property 35 nor 36. The movie checks points itself (`AbilityScene.SpendPoint` needs `nPointsToSpend > 0`, `Learn` needs state 2).
- `GetAbilityAttributes` (0x00714570) gives the movie `[ID, UseType, Icon, GUIType, PrereqAbilityID, DisplayState]` from record +0x58, +0x1B8, +0x168 and info +0x88 [C]. The movie groups by GUIType, class GUITypes first, drops orphanage members not in state 1, and starts a new chain unless `PrereqAbilityID` is the last ability's id, 4 to a chain (`MAX_CHAIN_LENGTH`). Skills load flat.
- `<ability>.Abilities` (SkillsScene.as:193-198) is not part of the chains: it is `Paperdoll.SeededCharacters.<HeroIndex>.Abilities`, an imported hero's ability ids.
- **Not read:** the names Column and Row in the exe; Row and Column values in play, or the sibling order in a live list; the add-ins' tables (enciphered); whether `justClosed` (0xC34198) is set when the filter skips a child between two accepted siblings; how a duplicate ID across add-in files attaches; the specialization chains (GUIType 106 to 117) walked.

### A pick and the points (CharGenAssignAbility, event 51)

- **The engine adds nothing itself.** `CharGenAssignAbilityMessage` (0x007FCE60) makes a script event 51 (0x33 at 0x007FCEF7) with object 0 the creature (message +4), int 0 the ability (+8, negative to remove) and int 1 (+0xC), delivers it at once to the module (`AIMaster::DeliverNow` 0x0064AD80, 0x007FCF28), and returns true [C].
- **`sys_chargen.nss:341-396` does the rest** [C, in the source]:
  - int 1 == 0, a skill, talent or spell: a skill (`ABILITY_TYPE_SKILL`) takes property 35, a talent or spell 36. A pick takes 1 if the creature lacks the ability, a removal gives 1 back if it has it.
  - int 1 != 0, a specialization: property 38, −1 for a pick, +1 for a removal, unguarded, so a removal refunds even one not held.
  - Then `_AddAbility` (`AddAbility` and `SetQuickslot(-1)`, the first free slot) or `RemoveAbility`. The base value is changed (`Chargen_ModifyCreaturePropertyBase`); attribute points (34) are event 50's.
- **The movie** (`as_levelup`): a pick sends `GUICharGen.CharGenAssignAbility <cid> <id> 0` and `Paperdoll.CharGenAssignAbility` (AbilityScene.as:74-75), then `GameModeController.FindAbilityChains <cid> <type> 2`; delivery is synchronous, so the rebuilt chains already show the pick. Its point count is its own, read from `Attributes.35` / `.36.Total` as the scene starts and counted down. A specialization goes as `GUILevelUp.CharGenAssignAbility <cid> <id> 1` (LevelUp.as:265-288). Unlearning is only of this session's picks with no picked ability depending on them (the movie's rule).
- **The end commits nothing new.** Each message applied its change. An abort is the movie sending the negative messages itself before `CharGenEnd` (`RevertAllChanges`, LevelUp.as:230-255). `CHARGEN_END` in level-up (sys_chargen.nss:508-526) only sets the tactics slots and presets, `SetCanLevelUp` from points 34 + 35 + 36 (38 left out on purpose, core_h.nss:2357), the achievements, and health and stamina regeneration; it does not read the exit point.
- **Not read:** how the class property (27) changes with a specialization (no chargen script sets it); `GUIPaperdoll`'s handler 0x007B8900; how the `GUICharGen.` and `GUILevelUp.` prefixes reach the one handler (once per pick in Eluvian's runs); the auto-level functions (event 56).

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
