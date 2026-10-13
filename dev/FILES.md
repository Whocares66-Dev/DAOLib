# The game's files: where they are, and which copy the game uses

The game finds a file by its name with its extension (`rules_core.ncs`), in one of a few hundred places: folders, archives (`.erf`), and archives read whole (`.rim`). Many names are in more than one place: a patch replaces a core file, an add-in or a mod replaces either, the player's override folder replaces everything. This is which copy the game takes, read in the exe and seen in play (DAOLib #106, step 4, 2026-10-12). A tool that reads the game's files with no game (Eluvian's ghilana) answers by these rules. A plugin in the game asks the game itself: `dao::resources::Sources()` lists every place in the order the game looks, and `dao::resources::SourceOf(file)` says where a file is taken from (`dao/Resources.h`).

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

**Add-ins.** The enabled add-ins of `Settings\AddIns.xml` are taken in descending order of their `Priority` attribute, and the *i*-th (from 0) gets a base of 9000 − 7*i*: under `ADDINS:` if the add-in's folder is in Documents, else 10000 more under `BUILD_ADDINS:` (0x005ECB30 tries the one, then the other, 0x005EF460 counts down from 9000 by 7). So **an add-in with a lower `Priority` attribute wins**: here the promotional items' (`Priority="1"`) at 18734 and up, the Golems of Amgarrak's (`595`) at 19000. Within one add-in, seen: `core\data`, `core\env`, their `talktables\` and `movies\`, and the `module\` folders of an add-in that extends the loaded campaign at the base; `core\audio` at −2; `core\textures\high` at −3; `core\override` at −6. The loader asks with the base and −1 to −6 for its other folders; the ones not listed were not in this install.

**Offers.** An offer is not a DLC but its advertisement: what the game shows of a DLC the player does not own. Each DLC is in the install twice, as an add-in (`Addins\DAO_PRC_CP_1\`, the content: its module, data, levels, voice) and as an offer (`Offers\dao_prc_ocp_1\`), whose `manifest.xml` is `<Manifest Type="Offer">` with an `OfferItem` (`o_PRC_CP1_Cailan`, `Presentation="16"`), the DLC's title and store text. An offer holds little: Return to Ostagar's a `gui.erf`, a `2da.erf`, models, animations and talk tables; Leliana's Song's a `gui.erf` and talk tables; the Blood Dragon armour's one picture (read 2026-10-12). Behind everything, an offer can only add names no other source has, and never shadows the core or the DLC it sells, which may be installed beside it as here. Where each offer shows in the game was not looked at.

**Sources that come and go.** The campaign's are added when a campaign loads and the level's `ENV:` RIMs while the level is loaded; neither is there at the main menu. A plugin's folders come with the plugin.

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

- **Add-ins of equal `Priority`.** Their order is neither alphabetical nor `AddIns.xml`'s as seen here (of the nine at 100, The Stone Prisoner's came first and the rest in reverse order of the file). A tool takes it from the game where it matters.
- **An add-in's folders this install has none of** (−1, −4, −5 of the base).
- **`ADDINS:`'s** add-ins, installed from a `.dazip` into Documents: none were installed here.
- **Other builds**: read in GOG's; the code is the same in Steam's (GOG's code) and the address library's session found the resource manager the same in EA's.
- **Merged tables** (`M2DA_*`, `2da_base.gda`'s lists): a table the game merges from many files has rows from all of them, by the merge's own rules, not one winning file.
