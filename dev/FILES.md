# The game's files: where they are, and which copy the game uses

The game finds a file by its name with its extension (`rules_core.ncs`), in one of a few hundred places: folders, archives (`.erf`), and archives read whole (`.rim`). Many names are in more than one place: a patch replaces a core file, an add-in or a mod replaces either, the player's override folder replaces everything. This is which copy the game takes, read in the exe and seen in play (DAOLib #106, step 4, 2026-10-12). A tool that reads the game's files with no game (Eluvian's ghilana) answers by these rules, which `dao::core::install::Index` (`dao/core/Install.h`) applies: each resource's copies in the game's order, the one it takes first, each with its priority, and why the game has none of a copy (another campaign's, a level's own, another language's or texture quality's, a disabled add-in's). A plugin in the game asks the game itself: `dao::resources::Sources()` lists every place in the order the game looks, and `dao::resources::SourceOf(file)` says where a file is taken from (`dao/Resources.h`).

## The rule

The resource manager keeps its sources in one list, ordered by:

1. **priority**, lower first;
2. **kind**, a folder before a RIM before an ERF (each source's slot 8: a folder 0 or 1, a RIM 2, an ERF 3);
3. **name**, the later in the alphabet first (the source's name: an alias and a file, or a folder's path).

`AddSource` puts each new source before the first that sorts after it (0x004E6660 compares, `dao/Addresses.h`, "The resource manager's sources"). Its index keeps, for each file name, the sources that have it in the same order (0x004C7C50), and **a file comes from the first source in that order that has it**.

An archive found inside a folder source (the game's own `packages\core\data\*.erf`, an `.erf` in a plugin's `override\`) is a source of its own at the folder's priority, so a loose file beats an archive beside it, and of two archives in one folder the later name wins (`patch004.erf` before `patch000.erf`).

## The priorities

Read from `AddSource`'s 57 callers (`SetUpSources` and its kin, the add-ins' and the campaign's loaders) and seen in GOG's game, the private copy `game\gogrun`, at the main menu (488 sources) and in Forest Clearing with a save loaded (859), 2026-10-12. The alias before a `:` stands for a folder (the engine's alias table, `kFileAliases_Path`).

| Priority | Source | Where |
|---|---|---|
| 0 | `WRITE_OVERRIDE:` | `Documents\BioWare\Dragon Age\packages\core\override\`, the player's |
| 1 | `OVERRIDE:` | the install's `packages\core\override\` |
| 2 | a DAOLib plugin's `override\`, and folders a plugin adds (`dao::resources::AddFolder`) | `bin_ship\daolib\plugins\<name>\...` |
| about 9000 − 7*i* | an add-in installed in Documents (`ADDINS:<UID>\`) | `Documents\BioWare\Dragon Age\AddIns\<UID>\` |
| about 19000 − 7*i* | an add-in installed in the game's folder (`BUILD_ADDINS:<UID>\`) | the install's `Addins\<UID>\` |
| 20007 | `WRITE_DATA:` | |
| 40000 | `SEEDEDCHARACTERS:` | |
| 50001 | `PATCH_TEXTURES:` | |
| 50002 | the loaded campaign's patch | `modules\single player\patch\` |
| 50004 | its data, the area's RIMs among it | `modules\single player\data\` |
| 50006, 50007 | its voice and sound | `modules\single player\audio\...` |
| 50008 | its own folder | `modules\single player\` |
| 100000 | `PATCH:` | `packages\core\patch\` |
| 100002 | `ENV:` | `packages\core\env\` |
| 100003 | `TEXTURES:` | `packages\core\textures\high\` (`texturepack.erf`) |
| 100004 | `DATA:`'s RIMs (`globalvfx.rim`, `designerscripts.rim`, ...) | `packages\core\data\` |
| 100005 | `DATA:`, its folders and its 30 ERFs | `packages\core\data\` |
| 100006 | `AUDIO_VO:` | `packages\core\audio\vo\...` |
| 100007 | a loaded level's `ENV:<level>\` RIMs | `packages\core\env\<level>\` |
| 100008 | `AUDIO_SOUND:` | `packages\core\audio\sound\` |
| about 200000 − 7*i* | an offer, a DLC's advertisement (`BUILD_OFFERS:<UID>\`) | the install's `Offers\<UID>\`, behind the core |

**Add-ins.** The enabled add-ins of `Settings\AddIns.xml` are taken in descending order of their `Priority` attribute (in the order below), and the *i*-th (from 0) gets a base of 9000 − 7*i*: under `ADDINS:` if the add-in's folder is in Documents, else 10000 more under `BUILD_ADDINS:` (0x005ECB30 tries the one, then the other, 0x005EF460 counts down from 9000 by 7). So **an add-in with a lower `Priority` attribute wins**: here the promotional items' (`Priority="1"`) at 18734 and up, the Golems of Amgarrak's (`595`) at 19000. Within one add-in, its `core\` and, while it extends the loaded campaign, its `module\`, each folder by the alias the loader asks with (0x005EF460, the names pushed before each call): `data` and `env` at the base, `audio\sound` −1, `audio\vo\<language>` −2, `textures\<quality>` −3, `locale\<language>` −4, `patch` −5, `override` −6. Seen in play: the base, −2, −3 and −6; this install has no add-in with the others.

**The order of add-ins of equal `Priority`.** The game takes `AddIns.xml`'s items from the last to the first and sorts them by `Priority` with MSVC 2005's `std::sort`, which is not stable, so add-ins of equal `Priority` come out in an order of the sort's own, neither alphabetical nor the file's. Running that sort (`dao/core/AddInOrder.h`: its `_Sort`, `_Unguarded_partition` and `_Insertion_sort` step for step) over this install's items gives the order the game took: all 39 add-ins, the 17 in groups of equal `Priority` among them, and all 9 offers (`offers.xml`'s, the same way). `install::Index` over this install gives every add-in's priority as the game's own list had it, 39 of 39. Reproduced, not read in the code: how the list is made and sorted is not traced.

**Offers.** An offer is not a DLC but its advertisement: what the game shows of a DLC the player does not own. Each DLC is in the install twice, as an add-in (`Addins\DAO_PRC_CP_1\`, the content: its module, data, levels, voice) and as an offer (`Offers\dao_prc_ocp_1\`), whose `manifest.xml` is `<Manifest Type="Offer">` with an `OfferItem` (`o_PRC_CP1_Cailan`, `Presentation="16"`), the DLC's title and store text. An offer holds little: Return to Ostagar's a `gui.erf`, a `2da.erf`, models, animations and talk tables; Leliana's Song's a `gui.erf` and talk tables; the Blood Dragon armour's one picture (read 2026-10-12). Behind everything, an offer can only add names no other source has, and never shadows the core or the DLC it sells, which may be installed beside it as here. Where each offer shows in the game was not looked at.

**Sources that come and go.** The campaign's are added when a campaign loads and the level's `ENV:` RIMs while the level is loaded; neither is there at the main menu. A plugin's folders come with the plugin.

## Merged tables

A data table the game merges (an M2DA: abilities, items, portraits, most of what the scripts' `TABLE_*` name) is not one winning file but the rows of many: the core's `abi_base.gda`, an add-in's `abi_cp1.gda`, a mod's `abi_mymod.gda`. Read in the exe 2026-10-13 (`dao/core/M2da.cpp` has the addresses) and seen in play:

- **Which files.** At start the game lists every GDA file it has, its sources in the order above, each name once, from the source that wins it. `m2da_base` is itself merged, from every file starting `m2da_`. Then each of its rows makes a table of the row's `Worksheet` from **every file whose name starts with the worksheet, in any case**: so `aip_archer_dwarf.gda` would be part of `aip_archer`'s table as well as its own. But a row whose ID is **10001 to 10999** (the AI's packages, `aip_*`) is a single table, loaded from the one file of the worksheet's own name and not merged. A file whose first column is not `ID` gives no rows.
- **Which row.** The table's rows are one array sorted by ID, filled file by file in the list's order, and **a row whose ID the table has already is dropped**: of the rows of one ID, the game keeps the first file's, which is the file of the source looked in first. An add-in's or a mod's row beats the core's of the same ID, and a mod in a plugin's or the player's override beats an add-in's. Two rows of one ID in one file: the first.
- **Seen in play** (GOG's game, the script commands through Eluvian): `GetM2DAInt(268, "TitleStrRef", 25)` is 445854, from Awakening's `achievements_ep1.gda`, where the core's `patch003.erf` has `-1` in `achievements.gda`; `GetM2DARows(10076)`, `aip_archer`, is 12, its own file's rows and not the 14 with `aip_archer_dwarf`, `_elf` and `_human`; `GetM2DARows(4)`, `prj_`, is 76, `prj_base.gda`'s 67 and more, which a rule of the longest prefix (`prj_base` is a worksheet too) would have left at none.

`dao::core::m2da` applies it: `WorksheetsOf` gives every worksheet a file is part of, `Takes` says whether one is, and `Merge` marks each ID's row the game keeps (`taken`), given the files in the game's order.

Talk tables: the game asks for one by a name it makes of the table's, the language's and `.tlk` (`core_en-us.tlk`; `_f.tlk` for the female player's), so a talk table of another language is never the game's; `install::Index` marks it `Unused::Language`.

## Seen in play

GOG's game from `game\gogrun`, `examples/resources` with a `where.txt`, 2026-10-12. Text files of one name were put in several places, each holding the name of its place, and the game read each from the place the rules say:

| File | Put in | Read from |
|---|---|---|
| `prec_a.txt` | the Documents override, the install's override, the plugin's override, Return to Ostagar's (`DAO_PRC_CP_1`) `core\override` | `WRITE_OVERRIDE:` (0) |
| `prec_b.txt` | the install's override, the plugin's, the add-in's | `OVERRIDE:` (1) |
| `prec_c.txt` | the plugin's override, the add-in's | the plugin's `override\` (2) |
| `prec_d.txt` | the add-in's `core\override`, `packages\core\patch\` | `BUILD_ADDINS:DAO_PRC_CP_1\core\override\` (18910) |
| `tie_name.txt` | `zz_a.erf` and `zz_b.erf` in the plugin's override | `zz_b.erf` |
| `tie_kind.txt` | loose in the plugin's override, and in `zz_a.erf` there | the loose file |

The same at the main menu and with the save loaded. The game's own: `rules_core.ncs` from `DATA:designerscripts.rim` (100004), `2da_base.gda` from `PATCH:patch003.erf` (100000).

## Not settled

- **A disabled add-in, or one with no folder**: whether it takes a place in the game's count of add-ins (`install::Index` marks the order unsettled where there is one).
- **An add-in's folders this install has none of** (−1, −4, −5 of the base): read in the code, not seen.
- **`ADDINS:`'s** add-ins, installed from a `.dazip` into Documents: none were installed here.
- **Other builds**: read in GOG's; the code is the same in Steam's (GOG's code) and the address library's session found the resource manager the same in EA's.
- **The order of two files of one merged table in one source**: the order the source lists its own names in (its index, 0x004E6A90), not read. `m2da::Merge` marks a row so decided unsettled.
