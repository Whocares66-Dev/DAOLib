# The save writer's fields

What the exe writes into a saved game, struct by struct, in write order: each field's id, name, GFF4 type, where its value comes from and when it is written. Read 2026-10-21 by lib-agent-2 with three subagents in the GOG exe, for DAOLib #221; `dev/SAVES.md`, "The engine's writer", is the summary. The scratch files named here (`lib-agent-2-*.md`) stayed local.

## What an object's save writes: CCObject's common writer, the creature's writer, their nested writers (wr-obj)

Read 2026-10-21 by lib-agent-2 in the GOG exe (`game\DAOrigins.exe`, base 0x400000), read-only. C = checked in disassembly, I = inferred. Prior work used and not redone: `lib-agent-2-sav-cre.md` (the creature loader 0x006BE980 and the base loader 0x00698ED0, all fields), `lib-agent-2-rst-cmd.md` (the command writer part), `lib-agent-2-sav-inv.md` (equipment, inventory, effects loaders), `lib-agent-2-sav-area.md`, master `dev/SAVES.md`, `dev/PARAGON_SAVES.md` (the 78 templates), `dev/PARTY.md:8` (+0x421), `dev/ITEMS.md` (the shared party Inventory at creature+0xBF0). Types are the templates' of `Characters\Lyna\Saves\AutoSave_1\savegame.das` (`tools/gff4.py`, my `lib-agent-2-wr-tplfull.py`), checked against the schema functions in the exe. New here: the whole writer side.

### Short answers

1. **The "0x00698150" of SAVES.md and rst-cmd is the middle of the common writer.** The writer is the CCObject vtable slot 21 (+0x54), **0x00697EE0** (`ret 4`, `this` = object, argument = the GFF4 struct builder). 0x00698150 is its `Name`/`Tag` step. Loader is slot 34 (+0x88), 0x00698ED0. The creature's pair: writer **0x006B2470** (slot 21), loader 0x006BE980 (slot 34); placeable 0x0074D600 / 0x0074FE30; item 0x00754020 / 0x00758340; trigger 0x00799E10 / 0x0079A7E0 (vtables CCObject 0xB0EE9C, CCCreature 0xB10484, CCPlaceable 0xB1C084, CCItem 0xB1C61C, CCTrigger 0xB1FEF4; C). Slot 35 (+0x8C) is 0x006978C0 in the base, 0x006A9460 in the creature (not read).
2. **The writers fill a struct whose template already exists** (made by a schema function, e.g. 0x006B7A30 + 0x006B7CF0 + 0x006B7E70 for CRE1; 0x0069B7A0 adds 16721 and 16740). Every writer step is `Has(id)` then a typed setter, and **a setter silently does nothing when the template has no such field, or its type differs, or the field is a list/struct/reference** (checked in each helper: `4D9F10`/`4D5230` look the field up, then the type word's bits 31/30/29 must be clear and the low word must equal the helper's type; table in section D). So the field set of a saved object **is its template**: a writer for a new template needs only the value of each field its template lists. The struct's bytes start as all 0xFF; a field never written stays 0xFF ("None"): proof, `SavegameCanLevelUp` (16456) is in the CRP1/CRL1 schema (0x006B7D9F) and has no writer at all, and every saved CRP1/CRL1 shows it as -1. A string never written reads as '' (null offset). (C)
3. **Conditions that are not just "the template has it"**: the base writer skips Orientation and the looping animation by the same `Has` plus state checks; the creature's backpack is written only for a creature that is not in the party pool; stats only for a combatant (`+0xBD4 == 0`); the tactics table only if the creature has one; `ObjectLoopingAnimation` only if the object has an animation controller; `EventScript` only if the object has one (+0x288). Everything else is value-from-field.
4. **Writer/loader mismatches** are few (section C): `Active` is saved as `active && !deactivation-pending`; the tag is saved as '' when it equals the template resref and the loader maps '' back to the resref (a round trip); `Backpack` of a pool member is saved as an empty list (the shared party Inventory is the party's own); `TeamID` and `InteractionRadius` are read by the base loader but written by the kind's own writer (the creature's writes both; the base writer neither).

### A. The common writer 0x00697EE0, in write order (C unless marked)

`this` = the object (`ebp`), `esi` = the struct builder. Setter names are the table in section D.

| # | id | name | type | value | written when |
|---|---|---|---|---|---|
| 0 | - | (guard) | - | the builder is valid (`[esi] != 0`) is tested by the caller (0x006B24A1) | - |
| 1 | 23 | ObjectID | s32 | `0x00A582C0(struct)`: `[this+4]` (the id attribute) through the s32 setter | always (setter, template must hold it) |
| 2 | 4 | Position | vector3 | `this+0x120` (`0x005FE630`) | always |
| 3 | 5 | Orientation | quaternion | if the object has an animation controller (`[this+0x70]` vtable +0x60): `0x00A5CD00(controller)`, the controller's scene node's quaternion (node vtable +0x38; identity (0,0,0,1) when it has no node). Without a controller: built from the facing at `this+0x130` (`0x004EE5A0`/`0x004EE570`/`0x004EE200`, a rotation about Z; I). Setter `0x006968E0` | `Has(5)` |
| 4 | 16256 | ObjectLoopingAnimation | u32 | the controller's `+0x140` | `Has` and the object has a controller (loader: the same two tests) |
| 5 | 16623 | SavegameEffectList | list GME1 | `0x0079F1F0(struct)` (= `Has(16623)`, `0x00A64630`), then effects manager (`this+0x70`) `0x0079F200(struct)` -> vtable +8 gives the effects object -> **`0x00A64E20`** (writer; per effect that `0x00A64A80` keeps, element writer **`0x00A65490`**, section E) | `Has` and the object has an effects holder |
| 6 | 16201 | SavegameObjectActive | u8 | `(+0x2C5 != 0) && (+0x421 == 0)`; **+0x421 is a deactivation pending while its animation plays** (PARTY.md:8): an object about to deactivate is saved inactive | `Has` |
| 7 | 16220 | SavegameObjectImmortal | u8 | `+0x2C0` | `Has` |
| 8 | 16250 | SavegameObjectPlot | u8 | `+0x2C4` | `Has` |
| 9 | 3 | TemplateResRef | string | `+0x1CC` (a copy; setter `0x004799E0`, wide string object) | `Has(3)` |
| 10 | 17000 | ScriptVarTable | generic list | `0x00702490(struct)` (= Has(17000)) then variable table (`this+0x2E0`) `0x00705A70(struct)`: finds each existing element of the struct's generic list by name (17001) and type (label SVI1/SVF1/SVS1/SVO1/SVR1) and sets 17003, or appends (helpers `0x00702BC0`, `0x00703DB0..0x00704200`); I for the update-vs-append detail | `Has` |
| 11 | 16221 | SavegameObjectEventScript | string | `+0x26C` | `Has` and `+0x288 != 0` (the object has an event script) |
| 12 | 16255 | SavegameObjectName | TlkString (0x11) or string (0xE) | the name attribute at `this+0xD4` (value at +0xE0, `0x00403C70`). If the template's field is string (type 0xE): written only if the name object's flag `[+0x26]` (a custom string) is set, as the wide string (`0x004D1EB0` -> `0x004799E0`). Else (TlkString): the name's string id through `0x00630810` | `Has(16255)`: CRP1, CRL1 and PLC1 only; CRE1 has none |
| 13 | 16222 | SavegameObjectTag | string | `+0xB8`; **if it equals the TemplateResRef (`+0x1CC`, compared by `0x004C0DE0`) the string written is '' (0xC2BE00)**; setter `0x004799E0` | always (setter) |
| 14 | 16721 | SavegameCommandList | list CMD1 | saving is the loading data object's byte `+0x70` (`0x005D5EA0()`) non-zero: resize the list to the queue's count (`this+0x2CC`, `0x007B51E0`, `0x004DD730`), each by `0x007439C0`. Zero (an area kept for later): the list is only emptied | see rst-cmd.md |
| 15 | 16740 | SavegameCurrentActionQueue | struct CAQ1 | `this+0x2DC` by `0x00745110`; zero: `0x00743D80` blanks it | see rst-cmd.md |

A note on 14/15: the guard is the byte at `+0x70` for the writer and `+0x68` for the loader (two different bytes of the loading data object; rst-cmd.md).

Not written by this function (so by the kind's own writer or nowhere): **16453 SavegameTeamID, 16262 SavegameObjectInteractionRadius, 16254 TreasureGroup, 16263 Importance, 16251..16253 (placeable health), 16455 IsPlotGiver, 16223 Items, 16226 MaxItems**.

### B. The creature's writer 0x006B2470 (CRE1, CRP1, CRL1; slot 21), in write order

`ebp` = creature, `esi` = struct builder. Everything that is "Has" is true exactly when the template has the field.

| # | id | name | type | value | written when |
|---|---|---|---|---|---|
| 0 | - | common writer | | `0x00697EE0(struct)` (section A) | `[struct] != 0` |
| 1 | 16263 | SavegameObjectImportance | s32 | `+0xCF8` (`0x004783B0`) | `Has` |
| 2 | 16209 | SavegameCreatureStats | struct ref CRS1 (CRE1, CRL1) / CPS1 (CRP1) | a **referenced** struct: the ref field's handle by `0x00643310(id)`. **`+0xBD4 == 0` (combatant)**: if the ref is null `0x00642D90` allocates the referenced struct (`0x004DB0E0`, `0x004DC530`); then if the stats attribute (`+0x7C4`) holds an object: **`0x0071BA40(stats, struct)`** (section F). **`+0xBD4 != 0`**: `0x0069F9F0(handle)` = `0x004DC680`, the ref is set to null ("None") | always (the struct field is ref-typed, not via a setter) |
| 3 | - | party lookup | | `p = app->party (0x005D57C0)`; if `p`: `p->0x006316F0(id)` (is a pool member) -> `state = p->0x00632810(id)` (follower state); `flag = p->0x00631870(id)` (the "non-party member" byte). State and flag are 0 without a party or if not a member | - |
| 4 | 16203 | SavegamePartyMembers | s8 (0x62A090) | `state` | `Has` (CRP1, CRL1) |
| 5 | 16280 / 16281 / 16282 | SavegamePartyMemberSubstate / Locked / Follow | s32 / u8 / u8 | the party record of the creature (`creature->0x006A2CC0() + 0x2A8` map, found by `0x00631890`, record by `0x008170B0`): substate `rec+4`, locked `rec+8`, follow `rec+9`. **No record: 0, 0, 1** | `Has(16280)` (CRP1, CRL1) |
| 6 | 16821 | SavegameTacticsHasTable | u8 | `+0xC00` tactics-table pointer non-null | `Has` |
| 7 | 16822 | SavegameTacticsTable | struct PTT1 | only if 6 wrote 1: **`0x007ADF40(table, struct)`** (writes the table into the same creature struct; a PTT1, field 16822) | as 6 |
| 8 | 16279 | SavegameNonPartyMembers | s8 | `flag` (step 3) | `Has` |
| 9 | 16451 | SavegameGroupID | u32 | `+0x3C0` (`0x00478420`) | always |
| 10 | 16453 | SavegameTeamID | s32 | `+0x3C4` | always |
| 11 | 16262 | SavegameObjectInteractionRadius | f32 | `+0x3CC` (`0x0062F610`) | `Has` |
| 12 | 16454 | SavegameCreatureStealth | u8 | `+0x7F0` | always |
| 13 | 16455 | SavegameIsPlotGiver | u8 | `+0x25A` | always |
| 14 | 16467 | SavegameCreatureIsGhost | u8 | low byte of `+0xC90` | always |
| 15 | 16470 | SavegameCreatureIsStatue | u8 | `+0xC94 != 0` | `Has` |
| 16 | 16474 | SavegameCreatureItemsScaled | u8 | `+0xCC4 != 0` | `Has` |
| 17 | 16457 | SavegameCreatureTrackable | u8 | low byte of `+0xB94` | always |
| 18 | 16460 | SavegameCreatureRace | u8 | `creature->0x006A7D90()` | always |
| 19 | 16459 | SavegameCreatureInteractive | u8 | `+0x42C` | `Has` |
| 20 | 16458 | SavegameCreatureControllable | u8 | `+0x7F9` | always |
| 21 | 16464 | SavegameCreatureClassRankList | list CSR1 | the class list (`0x00610000` over the attribute at `+0x544`) resized to its count (`0x004DD730`); element i: **16465 SavegameCreatureClassID u32** from the class entry's attribute `+0x58`, **16466 SavegameCreatureClassRank u32** from `+0x68` (I: the entry's identity; both read through `0x0053CC40`) | `Has` |
| 22 | 16218 / 16219 | SavegameEquipmentActiveSet / SavegameEquipmentItems | u32 / list ITL1 | **`Equipment::Save` 0x00797700** (the Equipment at `+0xBE0`, vtable slot +0x68; section G) | always |
| 23 | 16210 / 16211 / 16212 / 16226 / 16227 | Backpack / PlotItems / Money / MaxItems / CraftingRecipeList | | **`Inventory::Save` 0x0073FC70** (the Inventory at `+0xBF0`, slot +0x68; section G) | **only if `state == 0`**: the creature is not in the party pool (its `+0xBF0` is then its own; a pool member's is the party's shared Inventory, ITEMS.md "The party's shared inventory"). For a pool member the template's lists stay empty: saved CRP1/CRL1 show `Backpack = []` (C: save) |
| 24 | 16600 | SavegameBodybagID | u32 | `+0x790` | always |
| 25 | 16612 | SavegameCreatureRank | u32 | the rank attribute `+0x4B0` | always |
| 26 | 16254 | SavegameObjectTreasureGroup | s32 | `+0xBD0` (`0x004783B0`) | `Has` |
| 27 | 16320 | SavegameAppearance | struct APP1 / APL1 / APC1 | the struct field's handle (`0x004D9FC0`), then `[creature+0x7B0]` (a `CAppearance`, vtable 0xB1BB6C) vtable +0xC = **`0x00748E90`** (section H) | `Has` |
| 28 | 16952 / 16951 | SavegameDefaultSoundSet / SavegamePlayerSoundSet | string | `+0x988` / `+0x96C` (`0x004799E0`) | `Has` each (CRP1 only) |
| 29 | 16332 / 16333 / 16336 / 16337 / 16338 / 16334 / 16335 | SavegamePlayerPortrait Pitch / Yaw / Distance / PositionH / PositionV / Tint / Expression | f32 | the portrait record `[creature+0x598]` + 0x58 / 0x5C / 0x60 / 0x64 / 0x68 / (Tint: `0x00478290(rec+0x6C)`) / 0x7C (`0x0062F610`) | `Has` each (CRP1 only) |
| 30 | 16463 | SavegameCreatureCanChangeEquipment | u8 | `0x00838210(+0x5F4) != 0` (a property object's value) | always |
| 31 | 16469 | SavegameCreatureShowAsAllyOnMap | s8 | `0x00838210(+0xCB4)` | `Has` |
| 32 | 16471 | SavegameCreatureMinimizedSkillHeaderList | list CMA1 | the set at `+0x70C` (16-byte entries, count = (end-begin)>>4), list resized to the count; element i: **16473 SavegameCreatureAbilityHeaderID s16** (CMA1's field is type 3 = s16; setter `0x006A4860`) <- the entry's id (`0x00424010`) | `Has` (CRP1, CRL1) |
| 33 | 16472 | SavegameCreatureMinimizedTalentHeaderList | list CMA1 | as 32 from the set at `+0x728` | `Has` |
| 34 | 16475 | SavegameCreatureHeartbeatInterval | f32 | `+0xC98` | `Has` |
| 35 | 16477 | SavegameCreatureRoamCenter | vector3 | `+0xCD0` | `Has` (CRE1 only) |
| 36 | 16476 | SavegameCreatureRoamRadius | f32 | `+0xCE0` | `Has` (CRE1 only) |
| 37 | 16478 | SavegameCreaturePoolName | string | the pool at `+0xB60`: `0x0069E280(&tmp)` (the name) | `Has` (CRE1 only) |
| 38 | 16479 | SavegameCreaturePoolAvailable | u8 | `0x0069E310(pool)` | `Has` (CRE1 only) |
| 39 | 16480 | SavegameCreatureNoPermDeath | u8 | low byte of `+0xBD8` | `Has` (CRE1 only) |
| 40 | 16481 | SavegameCreatureTimeSinceDeath | s32 | `+0xC88` | `Has` (CRE1 only) |

Fields of the creature templates that **no writer sets** (stay 0xFF): **16456 SavegameCanLevelUp** (CRP1, CRL1; the loader does not read it either, it recomputes with `0x006ACA10`), and 16950 SavegamePlayerMorph, which is not written by the creature writer but by the hero's save code (writers of the label at 0x007FEED0 and 0x00803000; not followed; the loader reads the morph from the loaded PlayerChar, sav-cre.md). (C by scan: the label occurs in .text only at the schema 0x006B7D9F for 16456 and at 0x006B80E6, 0x006C0EBC, 0x007FEEEE, 0x007FEF44, 0x008031DC for 16950.)

Which template carries what (the schemas, C): CRE1 = common creature schema 0x006B7A30 (16263, 16451, 16600, 16612, 16254, 16453, 16454, 16467, 16470, 16474, 16457, 16460, 16459, 16458, 16463, 16279, 16469, the CSR1 class list 16464, 16475) plus the CRE1 part 0x006B7E70 (stats 16209 via `0x0071DE50`, appearance 16320 via `0x00749FF0`, roam 16477/16476, pool 16478/16479, 16480, 16481). CRP1/CRL1 = 0x006B7CF0 (the common schema + name 16255 via `0x0069B7F0`, 16203/16280/16281/16282, stats via `0x0062D6D0`, 16456, 16821 tactics via `0x007AF850`, the CMA1 lists 16471/16472) plus the hero extras 0x006B80B0 (appearance APP1 via `0x0074A1A0`, 16950, 16951, 16952, the seven portrait floats). Types from the schema (type word 0 u8, 1 s8, 3 s16, 4 u32, 5 s32, 8 f32, 10 vector3, 13 quaternion, 14 string, 0x11 TlkString; `|0x40000000` struct, `|0x80000000` list): 16263 s32, 16451 u32, 16600 u32, 16612 u32, 16254 s32, 16453 s32, 16454/16467/16470/16474/16457/16460/16459/16458/16463 u8, 16279 s8, 16469 s8, 16475 f32, 16477 vector3 (0xA), 16476 f32, 16478 string (0xE), 16479/16480 u8, 16481 s32, 16203 s8, 16280 s32, 16281/16282 u8, 16821 u8, 16456 s8, 16473 s16 (all C, matching the real templates).

### C. Base loader 0x00698ED0 against the writer (mismatches)

The loader's reads are in `lib-agent-2-sav-cre.md` (common read table); compared here with section A.

| field | writer | loader | note |
|---|---|---|---|
| 23 ObjectID | written (A1) | read by the area-list step (default -1), not by 0x00698ED0 | match |
| 4 Position | `+0x120` | kept verbatim (`0x00695540`) | match |
| 5 Orientation | scene node / facing | copied to `+0x140..`, SetOrientation | match |
| 16256 | controller `+0x140` | needs a controller | match |
| 16623 effects | `0x00A64E20` | `0x0079F220` -> `0x00A64B10`, **before** Active | order differs (the loader needs the effects first), not a value mismatch |
| 16201 Active | `+0x2C5 && !+0x421` | `+0x2C5` only | **writer filters a deactivation in progress**; loading gives an active flag with no pending state |
| 16220, 16250 | `+0x2C0`, `+0x2C4` | same | match |
| 17000 | `0x00705A70` | `0x00705E90`, merged over the template's | match |
| 16221 | only if the object has a script | assigned, `+0x288 = 1` if non-empty | match |
| 16255 | as A12 | string id or string by the field's type byte (0xE) | match |
| 3 TemplateResRef | `+0x1CC` | assigned to `+0x1CC` | match |
| 16222 Tag | '' if tag == resref | **'' (length 0) -> the resref** (0x00699368-0x00699377: the read string, if empty, is replaced by the +0x1CC copy; then SetTag if it differs from `+0xB8`) | a deliberate pair: a new object whose tag is its resref must be saved with tag ''. Seen: 0 of 2,196 saved CRE1 have tag == resref; 1,598 have '' (C: the save, `lib-agent-2-wr-tags.py`) |
| **16453 TeamID, 16262 radius** | **not in the common writer** (the creature writes both, B10/B11; a placeable's template has both too) | read always (defaults current) | a kind whose writer omits them leaves the 0xFF, which the s32/f32 getter returns as -1 / NaN-ish: a writer for a new kind must write them |
| 16721 / 16740 | by the `+0x70` byte (saving) | by the `+0x68` byte (a saved game's load) | match in effect (rst-cmd.md) |

Creature loader against the creature writer (loader table in sav-cre.md):

- 16203 loader reads s8 with default 0; 16280 block: Locked/Follow only if 16280 present (writer writes all three in one block) - match.
- 16209: writer nulls the ref for `+0xBD4 != 0`, loader skips for `+0xBD4 != 0` - match. A combatant with no `+0x7C4` stats object would get an allocated, empty CRS1 (writer step 2 allocates before testing the stats object; I).
- 16210/16211/16212/16226/16227/16218/16219: loader calls Equipment then Inventory loads whatever the struct holds; the writer writes the Inventory only for non-pool creatures. For a pool member the empty `Backpack = []` it sees is "present, even empty" (sav-inv.md section 3: remove and destroy every held item); sav-inv.md notes the order (the creature's own fresh Inventory is replaced by the party's in the later join).
- 16471/16472: writer s16 setter, loader reads 2 raw bytes (u16 getter) - match.
- 16475: writer writes it always (`Has`); loader reads it only when the load-state pair is 0 (sav-cre step 31) - the value is dropped on a saved-game load; harmless.
- 16456 is never written nor read.

### D. The GFF4 write helpers (the struct builder = `ecx`: `[ecx]` file, `[ecx+4]` data base, `[ecx+8]` template index; all `ret 8`, args `(id, &value)`)

Each does: look up the field by id in the builder's template (`0x004D9F10`, `0x004D5230`); if absent, or the type word's bit 31 (list), 30 (struct) or 29 (reference) is set, or the type differs from the helper's: do nothing; else store at `data + field offset`. (C: read for 0x00478340, 0x004783B0 in full, the rest by pattern and by the stored-type compare.)

| helper | type (GFF4) | stores via | seen with |
|---|---|---|---|
| 0x00478340 | u8 (0) | 0x0062A040 (1 byte) | flags, 16201, 16454 |
| 0x0062A090 | s8 (1) | 0x0062A040 | 16203, 16279, 16469 |
| 0x006EB250 | u16 (2) | 0x006EACE0 (2 bytes) | 16321, 16327 |
| 0x006A4860 | s16 (3) | 0x006EACE0 | 16473 |
| 0x00478420 | u32 (4) | 0x00474E10 (4 bytes) | 16451, 16465, 16218 |
| 0x004783B0 | s32 (5) | 0x00474E10 | 23, 16263, 16453, 16212 |
| 0x00643390 | u64 (6) | 0x00642DF0 (8 bytes) | not seen in these writers |
| 0x0062F610 | f32 (8) | 0x0062F0C0 | 16262, 16332.. |
| 0x005FE630 | vector3 (10) | 0x005FE3E0 | 4, 16477 |
| 0x006968E0 | quaternion (13) | 0x00695470 | 5 |
| 0x004799E0 | string (14), from a wide-string object | 0x00478A40 (copies into the data block, `0x004DB0E0`) | 3, 16222, 16221, 16478 |
| 0x006432A0 | string (14), from a `wchar_t*` | 0x00475930 | not seen here |
| 0x00630810 | TlkString (0x11) | 0x006303C0 | 16255 |

Type 7 (s64), 9 (f64), 12 (vector4/color) and 15/16 (matrix) setters were not found by the same prologue (not looked for further).

Getters, for the reading side: 0x0047C8E0 s32 (raw 4 bytes), 0x0062AB60 u8, 0x006AD530 u16, 0x006315C0 f32, 0x00479960 string, 0x005FF7A0 / 0x00697C10 vector4 / quaternion (16 bytes) - all raw widths, no type check (sav-inv.md section 0).

Lists and structs (writer side):

| helper | meaning |
|---|---|
| 0x004D9F90(id) | `Has` |
| 0x004D9F60(id) | the field descriptor (type word at `+4`), used by the name step |
| 0x004D9FC0(id, &handle) | the struct field's handle `{file, offset, template}` |
| 0x00643310(id, &handle) | the **referenced** struct field's handle (`0x2000` ref bit); `0x00642DC0` = the ref is null; `0x00642D90` allocates the referenced struct; `0x00643220` its data; `0x0069F9F0` = `0x004DC680` sets the ref null |
| 0x004DA040(id, &handle) | a struct-list field's handle; **0x004DD730(handle, n)** sets its count (0 clears it to null); `0x004DB2E0(handle, i)` the i-th element's builder |
| 0x004DA130(id, &handle) | a generic-list field's handle (variables) |
| 0x00A5A8E0(id, &handle) | a scalar-list field's handle; element setters `0x004782F0(handle, i, &u8)` and `0x0067BAD0(handle, i, &u32/s32)`; resized with `0x004DD730` |
| schema: `0x004860F0(builder, id, typeword)` | adds a field to a template being made; typeword = GFF4 type, `|0x40000000` struct (low word = the nested template index, e.g. `0x00746770` returns CAQ1's), `|0x80000000` list, `0xC0000000` list of structs |

### E. Nested writers: the effects (GME1) and commands

- **Effects**: `0x00A64E20` (the effects object; filter `0x00A64A80`; list 16623). Element writer **`0x00A65490`** (this = the effect, argument = the GME1 builder), template GME1 (schema 0x00A659C0), order and sources (C): 16613 EffectID s32 `+0x20`; 16614 Type s32 `+0x24`; 16615 DurationType s32 `+0x28`; 16616 Duration s32 `+0x2C`; 16618 TimeIndex s32 `+0x30`; 16619 Animation s32 `0x00A53690()`; 16620 Priority s32 `0x00A65380()`; 16621 Creator **u32** `+0x40`; 16622 AbilityID s32 `+0x44`; 16625 Resource2 s32 `+0x78` if `Has`; 16627 Flags s32 `0x004F6A90()` if `Has`; 16640 Dataarray struct DTA1 `0x00A5AA60(effect +0)`; 16624 EngineData struct DTA1 `0x00A5AA60(effect +0x54)`. Loader `0x00A656F0` (sav-area.md section 9) reads Dataarray before EngineData, as the writer lays them (both structs exist before the writes at 0x00A655FB/0x00A6560C).
- **Commands**: 16721 / CMD1 (writer 0x007439C0), 16740 / CAQ1 (0x00745110, element SAC1 0x00743C70): rst-cmd.md.

### F. The stats writer 0x0071BA40 (this = the stats object at creature+0x7C4; argument = the CRS1 / CPS1 builder)

C for the order, I for the reason. It is the mirror of the loader 0x0071EDC0 and is **a save with side effects**: it first reads property 7 and 8 (the stats object's property vector at `+0x190`; likely the two properties that abilities adjust, the health/mana-like depletables), then for all four ability lists calls `0x0071B600(ability, 0xFF/level, 0)` to **take every ability's effect off**, writes, and calls `0x0071B600(.., 1)` to put each back, and finally restores properties 7 and 8 to the values read (`0x0071B320`) if they changed. So the saved property bases are those without the abilities' modifiers. Steps:

1. Remove abilities (4 lists at `stats+0x150, +0x160, +0x170, +0x180`, i.e. `0x0071B600(id, ...)`).
2. **Properties: `0x00797000(props = stats+0x190, struct, isHero = creature(+0x5C)+0x7D4 == 1)`**: writes 16350 SavegameStatList / 16351 HeroicStatList (and 16352 for the hero) as generic lists of SMPL (16300 f32, 16353 index u16), ATTR (+ 16301), DPLT (+ 16302 current f32, 16304 s32, 16303 s32) elements, one per property (not read further: I).
3. Abilities: label table 0xB17490 = {16306 Talent, 16305 Spell, 16307 Skill}, enable flags 0xB174A0 = 1,1,1,0: each list is the scalar-list field resized to the vector's count and filled with `0x0067BAD0` (u32 ids).
4. 16468 CreatureModalAbilityList (`0x4054`): scalar list from the vector at `stats+0xFC`, s32 elements.
5. Quickslots `0x0062B1E0(bar +0x1B4, struct)` for a creature with a quickslot bar (party members): CPS1's 16308, 16313-16316 (CQS1 lists), 16317, 16318, 16329, 16339, 16340 (not read).
6. Abilities put back (`0x0071B600(.., 1)`), properties 7/8 restored.

Templates: CRS1 (31): 16305, 16306, 16307 u32 lists, 16350, 16351, 16352 generic lists (ref), 16468 s32 list; CPS1 (47) adds the quickslots. SMPL (28), ATTR (29), DPLT (30) per `PARAGON_SAVES.md`.

### G. Items, equipment and inventory (nested)

- **Equipment::Save 0x00797700** (vtable 0xB1FB8C slot 26): if the builder is valid: 16218 SavegameEquipmentActiveSet u32 <- the attribute at `Equipment+0x10`; then `0x00761D10(this, struct, 0x3F5B)` writes 16219 (see below).
- **Inventory::Save 0x0073FC70** (vtable 0xB1B974 slot 26): in order: the Backpack (`Inventory+8`, vtable slot +0x68, thunk 0x0080D140 -> **`0x007CA810(struct, 0x3F52)`**): `0x00761D10(struct, 16210)` then **16226 SavegameMaxItems s32 <- `[Backpack+0x28]`**; the PlotItems (`Inventory+0x18`, slot +0x68, thunk 0x0080CFD0 -> `0x00761D10(struct, 16211)`); the money (`Inventory+0x28`, slot +0x14 = **`0x007CA060`**): **16212 SavegameMoney s32 <- `[money+0x90]`**; then **16227 SavegameCraftingRecipeList** (s32 scalar list) from the vector at `Inventory+0x38` (16-byte entries, `0x00403C70`, `0x0067BAD0` per element) if `Has` and the list handle is non-null.
- **The item-list writer `0x00761D10(this = the item container, struct, listId)`** (shared by 16210, 16211, 16219; template ITL1): gets the list handle (`0x004DA040`), gathers the container's items by an iterator (`0x007641D0`), **skipping items whose `+0x5D0` flag (being destroyed, `0x00838210`) is set**, resizes the list (`0x004DD730`), then per item: the item's vtable **+0x54** (the CCItem writer **0x00754020**, which itself starts with the common writer 0x00697EE0, so an item's struct has ObjectID, Position (overwritten below), Tag, effects, variables...), `0x007612E0` (the location of the item in the container), the container's vtable +0x64 (location -> vector3: equipment `0x00797490` stores (slot, weapon set, 0.0) as **unsigned**; backpack/plot `0x007CA730`), and `0x005FE630(element, 4, &pos)` **overwriting the item's Position**. ITL1's own fields (ItemCost, ItemStackSize, ... the item kind's extras) come from the CCItem writer, not read here (sav-inv.md, ITEMS.md).
- Template names: ITL1 (26), CSR1 (27), CMA1 (51), PTT1 (50), CAQ1 (25), CMD1 (23), SAC1 (24), GME1 (22), DTA1 (21).

### H. The appearance writer 0x00748E90 (CAppearance vtable 0xB1BB6C slot 3; this = the CAppearance, argument = the APP1 / APL1 / APC1 builder)

All steps are `Has` then the setter (C):

| id | name | type | value |
|---|---|---|---|
| 16321 | SavegameAppearanceType | u16 | `+0x58` |
| 16327 | SavegameAppearanceOriginalType | u16 | `+0x5A` |
| 16322 | SavegameAppearanceGender | u8 | `+0x50` |
| 16324 | SavegameAppearanceGore | f32 | `0x00748520()` |
| 16325 | SavegameAppearanceDecapitated | u8 | `+0x5D` |
| 16326 | SavegameAppearanceItemHeraldryVariation | u8 list | resized to 7, element i <- `0x00747D70(i)`, i = 0..6 |
| 16328 | SavegameAppearanceMorphName | string | `+0x1C8`, only if the string passes `0x004C0E80(.., 0xB1BB38)` (non-empty; I) |

Templates: APC1 (32; creature in an area) holds only 16324, 16325, 16326; APP1 (52; the hero) holds all seven; APL1 (55; pool member) in between (`PARAGON_SAVES.md`). So a CRE1's saved appearance carries no type: the type comes from the template (sav-cre.md).

### Evidence (addresses, all C unless marked)

- **Writer boundaries**: the vtable dumps (`lib-agent-2-dw.py 0xB0EE9C`): slot 21 = 0x00697EE0, slot 34 = 0x00698ED0; CCCreature 0xB10484: 21 = 0x006B2470, 34 = 0x006BE980. `0x00697EE0` begins `push -1 / push 0xAA0180` (a function with a frame); 0x00698150 is inside it (the `call 0x4D1EB0` of the Name step, then 0x3F7F pushes). The `0x697EE0` calls: `0x00A582C0` (ObjectID, ids 0x17), `0x005FE630` (id 4), `0x006968E0` (id 5), `0x00478420` at 0x00697FF2 (id 0x3F80), `0x0079F1F0`/`0x0079F200` at 0x00697FF8/0x00698008, Active/Immortal/Plot at 0x0069800D-0x006980A0, `0x004C2510` copy of +0x1CC at 0x006980A7, `0x00702490`/`0x00705A70` at 0x006980D5/0x006980E8, EventScript at 0x006980ED-0x00698113, Name 0x00698118-0x00698171, Tag 0x00698176-0x006981BA, commands 0x006981BF-0x00698355.
- **Active**: `cmp [ebp+0x2C5],0 / je 0x698034 / cmp [ebp+0x421],0 / mov [esp+0x17],1 / je 0x698039 / 0x698034: mov [esp+0x17],0` (0x0069801D-0x00698039).
- **Tag**: the compare is `0x004C0DE0` (wstring equality: size and `0x00414170` compare; returns 1 when equal) between the copy of `+0x1CC` ([esp+0x64]) and the copy of `+0xB8` ([esp+0x48]); true -> `0x004C2AB0` assigns the constant at 0xC2BE00. Loader 0x00699326-0x00699377 reads 16222 into a local; `cmp [local+0x14 length],0; jne` else assigns the +0x1CC copy; then `0x004C0E50` compares with `+0xB8` and calls SetTag `0x00696590`.
- **Creature writer**: the step addresses are in the order of the file; examples: 16209 at 0x006B24D0-0x006B2583 (`cmp [ebp+0xBD4],ebx; jne 0x6B2574` -> `0x0069F9F0`), party lookup 0x006B2583-0x006B25D4, 16203 0x006B25D8, 16280.. 0x006B2601-0x006B26D1, 16821 0x006B26D1-0x006B271B, equipment/backpack 0x006B29E1-0x006B2A12 (`call [edx+0x68]` on `+0xBE0`; `cmp [esp+0x14],0; jne` before `+0xBF0`), appearance 0x006B2A68-0x006B2A9B, the last field (16481) 0x006B2F28; the function ends at 0x006B2F5E (`ret 4`).
- **Schema functions**: `lib-agent-2-wr-schema.py` over 0x006B7A30 (CRE1 common), 0x006B7CF0 (+party), 0x006B7E70 (CRE1), 0x006B80B0 (hero); small base pieces 0x0069B660 (ObjectID/tag), 0x0069B6A0 (TemplateResRef), 0x0069B6C0 (Position/Orientation), 0x0069B6E0 (16201), 0x0069B700 (16262 f32), 0x0069B720 (16220), 0x0069B740 (16250), 0x0069B760 (16455), 0x0069B780 (16221), 0x0069B7A0 (16721, 16740), 0x0069B7F0 (16255 type 0x11), 0x0069B810 (16256 u32).
- **Unwritten = 0xFF**: save check. All of CRP1/CRL1 `SavegameCanLevelUp` read -1 (dump lines 3551, 4009, 4486, 4935), though 16456 is only declared (0x006B7D9F) and never written; 16456 has no loader read either.
- **Backpack of a pool member**: `[esp+0x14]` holds `p->0x00632810(id)` or 0 (0x006B258D, 0x006B25C1); it is the value stored as 16203 (0x006B25E8) and tested at 0x006B29F6 (`cmp dword [esp+0x14],0; jne 0x6B2A12`) before the `+0xBF0` save. Saved hero and followers show `Backpack = []` (dump lines 3421, 3893, 4342).

### Not read

- The orientation fallback (`0x004EE5A0/570/200`) arithmetic and what the controller's node returns when the object has no node; whether `+0x130` is a facing vector (I).
- `0x00705A70` (the variable writer) in full: how a variable already present in the struct is matched and what is appended; which variables are skipped (a "changed" byte at entry+0x10 is set by the loader; whether the writer filters on it was not followed).
- `0x00797000` (properties writer) and `0x0062B1E0` (quickslots writer) internals, `0x007ADF40` (tactics writer), `0x00A64A80` (which effects are not saved), `0x00A5AA60` (DTA1 writer), the item writer 0x00754020 (CCItem's own fields), the placeable / trigger / waypoint / sound / store / AOE writers (only the placeable, item and trigger slot addresses were taken).
- What `0x006A2CC0()+0x2A8` (the party-record map) and `0x00631870` (the "non-party member" byte) are in the party code; the class entry layout behind `+0x58` / `+0x68`.
- The hero-only writers of 16950 PlayerMorph (0x007FEED0, 0x00803000) and how the PLYR / PRTY / party list structs get their creatures (CRP1 and CRL1 creation).
- The area-list writer that creates the CRE1 elements and the template choice per object kind (0x00683C80 is the area's save, LOAD_EVENTS.md:42-56).

## The save writers of placeables, items, effects and stores, and their loaders (GOG exe)

Read 2026-10-21 by lib-agent-2 in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`; types checked on `Characters\Lyna\Saves\AutoSave_1\savegame.das` with `%TEMP%\lib-agent-2-wr-tpl.py` (the templates' field types). C = read in the disassembly, I = inferred. Prior work used and not redone: master dev/SAVES.md, dev/PARAGON_SAVES.md (templates), dev/LOAD_EVENTS.md:46-56 (the area's save, `0x00683C80`), dev/COMBAT.md:167-186 (the effect's invalid word +0x48, AgeEffect), dev/script_commands/vars.md:34 (the variable writer), %TEMP%\lib-agent-2-sav-area.md (the loaders of PLC1, STO1, GME1), -sav-inv.md (ITL1 loader, item-list loader, GME1/DTA1 loader), -rst-cmd.md (commands: not repeated). New here: every writer, its order, its conditions, its sources.

### Short answers

1. **Every kind's writer is vtable slot 21 (+0x54) of the object's main vtable; its loader is slot 34 (+0x88).** CCObject 0x00697EE0 / 0x00698ED0, CCPlaceable 0x0074D600 / 0x0074FE30, CCItem 0x00754020 / 0x00758340, Store 0x007EB510 / 0x007EC0B0. Each derived writer calls the base writer 0x00697EE0 first, then adds its own fields. (C: vtable dumps with `%TEMP%\lib-agent-2-dw.py`.)
2. **The writer fills a struct that already exists; it never decides what fields the struct has.** The caller (the area's writer 0x00683C80, the item-list writer 0x00761D10, the effects holder 0x00A64E20) sizes a list field with `0x004DD730(n)` and gets element i with `0x004DB2E0`; the element has the list's schema template (PLC1, ITL1, STO1, GME1, ...). Every setter first looks the label up in the struct's own template and does **nothing if the label is missing or its type is not the one the setter writes** (s32 0x004783B0 = type 5, u32 0x00478420 = type 4, u8 0x00478340 = type 0, u16 0x006EB250 = type 2, f32 0x0062F610 = type 8, string 0x004799E0 = type 14, tlkstring 0x00630810 = type 17, vector3 0x005FE630 = type 10, quaternion 0x006968E0 = type 13; the flag bits 0x80000000/0x40000000/0x20000000 of the type word must be clear). So a field with another type in the file is silently not written (C: 0x00478420-0x0047848A, 0x004783B0-..., 0x006EB250-..., 0x0062F610-..., 0x004799E0-..., 0x00630810-..., 0x005FE630-..., 0x006968E0-...). The "if has" tests below are extra gates on top of that, for fields a kind's schema may lack; with the real templates all of them are present for these four kinds.
3. **An unwritten field stays at the new struct's default (zero, empty string, empty list),** e.g. PLC1 `SavegameObjectEventScript` is `''` unless the object's event-script flag +0x288 is set, and `ScriptVarTable` is `[]` unless the object has changed variables. (C for the gates; I for the defaults, seen as `''` and `[]` in the real save.)
4. **Items on the ground are not saved at all.** The area's writer takes, per object of the area's list (+0x694), the first non-null of the cast slots creature (+0x10; skipped when `0x006316F0` on the party's record of that id is nonzero, I: a party-pool member), trigger (+0x28), placeable (+0x24), AOE (+0xC), waypoint (+0x30), store (+0x50), sound (+0x40, and only if +0x44, the pannable cast, is null). There is no item cast: a CCItem in the area's list is not written. Items are saved only inside a container that has a list: a creature's equipment/backpack/plot items, a placeable's `SavegameItems`, a store's `SavegameStoreItemList` (a bodybag/loot bag is a placeable with `SavegameIsBodybag`). (C: 0x00683EEA-0x0068405E, cast table `src/dao/generated/ObjectCasts.h`.)
5. **Effects whose invalid word (+0x48) is 1 are not saved**; the effect's started flag (+0x50) is never saved or loaded, so the first `AgeEffect` after a load (0x007A0B10) sees "not started", re-runs the effect's VFX start (engine int 2) and sets +0x50 = 1 before ageing. (C)
6. **Loader mismatches that matter for a writer of our own** (details in each table): the placeable's saved Active is `active && !deactivation-pending`; an item's Position is replaced by the container's location (index, +0x600 attribute, 0) after the item writes; an item's sub-items are saved as resrefs only and remade on load; a store's will-not-buy/will-only-buy lists are appended on load to the template's, so each save/load round trip would double them; a name given as literal text is saved in the tlkstring's second word but the loader reads only the strref; STO1's `ObjectID` is written and never read; an effect's `Priority` and `Flags` are the getters' values, not raw fields.

### The base writer CCObject 0x00697EE0 (slot 21), in write order (C)

Arguments: `this` = the object, stack = the struct handle {file, offset, template} (a null file does nothing).

| order | id | name | type | value / source | condition |
|---|---|---|---|---|---|
| 1 | 23 | ObjectID | s32 (0x004783B0) | `0x00A582C0`: the object's id attribute (`this+4`, `0x0053CC40`) | always (if the template has it) |
| 2 | 4 | Position | vector3 | `this+0x120` | always |
| 3 | 5 | Orientation | quaternion | from the animation component (`[this+0x70]` slot 0x60 then `0x00A5CD00`), else built from the facing vector at `this+0x130` (`0x004EE5A0`/`0x004EE570`/`0x004EE200`) | only if the template has 5 |
| 4 | 16256 | ObjectLoopingAnimation | u32 | animation component `+0x140` | only if the template has it and the object has the component |
| 5 | 16623 | SavegameEffectList | list GME1 | `0x0079F200(holder)` -> effects holder writer 0x00A64E20 (below) | if `0x0079F1F0` (the struct has 16623) and the holder exists |
| 6 | 16201 | SavegameObjectActive | u8 | `[+0x2C5] != 0 && [+0x421] == 0` (active and no SetObjectActive(FALSE) pending; PARTY.md:8,14) | only if the template has it |
| 7 | 16220 | SavegameObjectImmortal | u8 | `[+0x2C0]` low byte | has |
| 8 | 16250 | SavegameObjectPlot | u8 | `[+0x2C4]` | has |
| 9 | 3 | TemplateResRef | string | copy of `+0x1CC` | has |
| 10 | 17000 | ScriptVarTable | generic list | `0x00705A70(this+0x2E0, struct)` if `0x00702490(struct)`; see Variables | if the struct has 17000 |
| 11 | 16221 | SavegameObjectEventScript | string | `+0x26C` | only if `[+0x288] != 0` and the template has it |
| 12 | 16255 | SavegameObjectName | tlkstring (17) or string (14) | the LocalisedString at `+0xD4`: type 14: its literal text, only if it has one (flag byte at `+0xFA`); type 17 (PLC1): word 0 = its strref, word 1 = the literal wide string if the name has one, else the empty string (`0x006303C0`) | has |
| 13 | 16222 | SavegameObjectTag | string | `+0xB8`, **written as empty when equal to the TemplateResRef** (`0x004C0DE0` compare, then assign the empty string) | always (if the template has it) |
| 14 | 16721 | SavegameCommandList | list CMD1 | the queue at `+0x2CC`, one CMD1 per command by `0x007439C0` | only when the loading data's byte +0x70 is nonzero (a game being saved); otherwise the list is only emptied |
| 15 | 16740 | SavegameCurrentActionQueue | struct CAQ1 | the record at `+0x2DC` by `0x00745110` | same gate, and the object has the record; else `0x00743D80` blanks it |

Not written by the base: TeamID (16453), InteractionRadius (16262): the placeable writes them itself; items, stores and the others have no such fields in their templates. (C: whole of 0x00697EE0-0x0069839B read.)

Loader differences against the base writer: (a) Active is read raw into +0x2C5 (the writer's `+0x421` clause is lost, harmless: it is 0 after a load); (b) a tag equal to the resref is saved empty and the loader refills it from the resref (sav-area.md section 4 step 12), consistent; (c) the loader assigns +0x288 = 1 only when the saved script is non-empty and different; (d) Name as above.

### Placeable: CCPlaceable 0x0074D600 (slot 21), PLC1 = template 34 (C)

Base fields first (table above: 23, 4, 5, effects, 16201, 16220, 16250, 3, 17000, 16221, 16255, 16222, commands, action queue). Then, in order:

| order | id | name | type | source (+offset) | condition |
|---|---|---|---|---|---|
| 1 | 16252 | SavegameObjectMaxHealth | s32 | virtual getter +0x128 | always |
| 2 | 16251 | SavegameObjectHealth | s32 | virtual getter +0x120 | always |
| 3 | 16100 | SavegameAreaPlaceableState | **u16** (0x006EB250) | `word [+0x658]` | always |
| 4 | 16102 | SavegameAreaPlaceableUseable | u8 | `[+0x42C]` | always |
| 5 | 16601 | SavegameIsBodybag | u8 | `[+0x600] != 0` | always |
| 6 | 16602 | SavegameLootableObjectID | u32 | `[+0x664]` (0xFFFFFFFF = none, as seen) | always |
| 7 | 16253 | SavegameObjectRank | s32 | `[+0x6BC]` | has |
| 8 | 16258 | SavegameObjectPickLock | s32 | `[+0x5E0]` | has |
| 9 | 16254 | SavegameObjectTreasureGroup | s32 | `[+0x6C0]` | has |
| 10 | 16257 | SavegameObjectLootableCreatureAppearanceType | u16 | `[+0x668]` | has |
| 11 | 16455 | SavegameIsPlotGiver | u8 | `[+0x25A]` | has |
| 12 | 16223 + 16226 | SavegameItems (list ITL1) + SavegameMaxItems (s32) | | container at `+0x678` (`0x0053CC40` on the attribute) vtable slot 4 (+0x10) = `0x007CA810(struct, 16223)`: item-list writer 0x00761D10, then MaxItems 16226 from the container's attribute `+0x28` (0x007CA828-0x007CA840) | always |
| 13 | 16453 | SavegameTeamID | s32 | `[+0x3C4]` | always |
| 14 | 16262 | SavegameObjectInteractionRadius | f32 | `[+0x3CC]` | has |
| 15 | 16259 | SavegameObjectTrapDetected | u8 | `byte [+0x5EC]` | has |
| 16 | 16260 | SavegameObjectDCDetectCheck | s32 | `[+0x644]` | has |
| 17 | 16261 | SavegameObjectDCDisarmCheck | s32 | `[+0x648]` | has |

(Immediates in the code: 0x3F7C, 0x3F7B, 0x3EE4, 0x3EE6, 0x40D9, 0x40DA, 0x3F7D, 0x3F82, 0x3F7E, 0x3F81, 0x4047, 0x3F5F, 0x4045, 0x3F86, 0x3F83, 0x3F84, 0x3F85.) The placeable states are dev/PLACEABLES.md:166-173's; the state is stored as the placeable's current one, +0x658, and the loader applies it in the post-load by `0x0074DBA0(state, 1)` (snap, no animation), so a saved state restores its end pose. A door's patch of the walk grid follows the state (PATHFINDING.md), not a saved field.

Loader (`0x0074FE30`) against this writer: all 17 own fields are read with the same widths; 16100 is stored before the base read and only applied in the post-load; Health is set after MaxHealth (the writer writes Max first too); `SavegameIsBodybag` is read with default `(+0x600 != 0)` (same expression). Nothing written is unread; nothing read is unwritten. Two differences: the loader reads 16201 into +0x2C5 raw; RoomID1/RoomID2 (+0x6A8/+0x6AC) are in neither (taken from the `.are` by X/Y, sav-area.md section 5). (C)

### The item list writer 0x00761D10 (container -> list of ITL1), used by all containers (C)

`ItemContainer::Save(struct, listLabel)`; called by the container's slot 4 (ListItemLocation, Backpack, PlotItems, ItemRepository share `0x007CA810`; Backpack/PlotItems have their own slots 26-28 for the three labels, not read here).

1. If the struct is null or the list label is missing from its template: return.
2. Walk the container's iterator (`0x007641D0`; item = iterator slot 4 (+0x10)); **skip an item whose attribute `+0x5D0` is true** (the "being destroyed" flag, ITEMS.md:144, `0x00838210`); collect the rest into a vector in container order.
3. Size the list to the vector's count (`0x004DD730`).
4. For each item i: element i of the list; **the item's own writer (vtable +0x54, 0x00754020)**; then `0x007612E0(&loc, item)` = the container's location of the item (container vtable slot 19, +0x4C); then the container's location-to-position (slot 25, +0x64) into a vector, and **`Position` (4) is overwritten with it** (`0x005FE630(4)`). (0x00761E10-0x00761E95)

Locations and the Position they give:
- ListItemLocation (placeable, store) slot 19 `0x007CAA00`: `{ index of the item in the container (0x0063C4B0), item attribute at +0x600, 0xFF, -1, -1, -1 }`; slot 25 `0x007CA730`: Position = `(float(unsigned dword0), float(unsigned dword1), 0.0)`. The real save has `(0.0, 1.0, 0.0)` for the first item of a loot chest. Backpack/PlotItems use the same two slots (their vtables hold 0x007CA6C0/0x007CA730). (C)
- Equipment: slot 25 `0x00797490` writes `(slot, weapon set, 0)` (sav-inv.md section 2). (not repeated)

The container's `SavegameMaxItems` (16226) is written by `0x007CA810` after the list (the attribute at the container +0x28). The loader (`0x007CA990`) reads MaxItems first, then the list; for ListItemLocation (placeable/store) both labels are the kind's: placeable 16223/16226, store 16156/16226. (C)

### Item: CCItem 0x00754020 (slot 21), ITL1 = template 26 (C)

Base fields first (23, 4, 5 [absent in ITL1], effects, 16250, 3, 17000, 16222; 16201/16220/16221/16255 and commands are not in ITL1). Then, in order:

| order | id | name | type | source (+offset) | condition |
|---|---|---|---|---|---|
| 1 | 16224 | SavegameItemDroppable | u8 | `[+0x4B0] != 0` | always |
| 2 | 16231 | SavegameItemStealable | u8 | `[+0x4B1] != 0` | has |
| 3 | 1002 | ItemStackSize | s32 | attribute `+0x5F0` | always |
| 4 | 1011 | ItemAbilityID | **u32** | attribute `+0x660` | always |
| 5 | 1020 | ItemCraftingRecipeID | s32 | attribute `+0x670` | has |
| 6 | 16225 | SavegameItemDamaged | u8 | attribute `+0x5E0` | has |
| 7 | 1019 | ItemSubitemsResRefs | list string | the sub-item id array at `+0x4BC`: if the template's field is of type string (14) and the list can be made: if every id is -1 the list is sized 0; else one string per id: the **resref (`+0x1CC`) of the object with that id** (`0x005D5870`), empty if it is gone | field type = 14 |
| 8 | 1013, 1014, 1015, 1018 | ItemProperties, ItemPropertyPowers, ItemPropertyEffectID, ItemPropertyVFXID | list s32 each | n = count of the property vector at `+0x680` (16-byte elements = attributes); i-th: ItemProperties = value of element i of the vector at `+0x680`; Powers = value of element i of the vector at `+0x69C`; EffectID = dword i of the vector at `+0x6B8` (begin at `+0x6BC`); VFXID = dword i of the vector at `+0x6C8` (begin at `+0x6CC`); missing entries are 0xC2A8D4's default | only if all four lists are in the template and could be made (they are in ITL1) |
| 9 | 16233 | SavegameItemCurrentVFXPropertyID | s32 | `[+0x6D8]` | has |
| 10 | 16234 | SavegameItemCurrentVFXPropertyPower | u32 | `[+0x6DC]` | has |
| 11 | 1021 | ItemBaseCost | s32 | `[+0x5B8]` | has |
| 12 | 1001 | ItemCost | s32 | attribute `+0x5A8` | always |
| 13 | 16228 | SavegameItemIrremovable | u8 | attribute `+0x630` (`0x00838210`) | has |
| 14 | 16229 | SavegameItemIndestructible | u8 | attribute `+0x640` | has |
| 15 | 16230 | SavegameItemMaterialType | s32 | attribute `+0x4FC` | has |
| 16 | 16232 | SavegameItemInfinite | u8 | attribute `+0x650 != 0` | has |
| 17 | 16237 | (unnamed) | u8 | `[+0x46C]` | has |

Position (4) is then **replaced by the container** (previous section). `ObjectID` is the item's own id (base), so an item reached again by the same id after a load is reused (item-list loader, sav-inv.md section 3). The item base cost / cost: the real save shows ItemCost 1320, BaseCost 1 for a stack of 34 arrows.

Loader (`0x00758340`, sav-inv.md section 4) against this writer:
- 1011 ItemAbilityID is the gate for the four property lists on load (they are read only inside "1011 present") and for their arrays being cleared and refilled; the writer always writes 1011, so the lists are read. They need both 1013 and 1014 non-null: an item with **no properties writes empty lists, which read back as null, so on load the property arrays are not touched** (they keep the template's, which is the same unless a property was removed at run time: I).
- 1019: the loader makes **new** items from the saved resrefs and stores their new ids in the sub-item array; the saved sub-items' own state (stack, properties, ids) is not restored. The writer writes the resref only, so there is nothing else to restore. (C)
- 16233/16234: the loader reads them only when present and not null (all-0xFF); a -1 written for 16233 is read back as absent (the constructor default stays; I: -1 as well).
- 16232, 16237: read only when present and not null.
- 1020, 1002, 1001, 1021, 16225, 16228-16230: written and read with the widths above; the loader's setters (0x00752BF0, 0x00752C30, 0x00756540, 0x00753250) set the attributes the writer reads from (I: the setters were not opened).
- Position and ObjectID are read by the item-list loader and the base, not by the item's own fields.

### Effects holder writer 0x00A64E20 (holder; called by 0x0079F200 on the object's `+0x70` / the area's `+0xCC`) (C)

1. n = number of effects in the holder's vector (`+8`..`+0xC`, 4-byte pointers).
2. The list field 16623 (0x40EF) of the struct: if absent, nothing is written.
3. V = the effects the holder's filter matches (`0x00A64A80` with a predicate built by `0x00670A80` from `0x006953E0`, `1`, `0`: the effect's **`+0x48 == 1`**, the "invalid word" `SetIsCurrentEffectValid(FALSE)` writes, COMBAT.md:173). The list is sized `n - |V|`.
4. For each effect `e` in holder order (null entries skipped): if `e[+0x48] != 1` and the output index < the list's count: element = list[index++]; `0x00A65490(e, 0, 0, &element)` writes it (below).

The loader (`0x00A64B10`) makes one new effect per list element (`0x00A652A0(0)`, type 0 so no new id is drawn), reads it with `0x00A656F0` and adds it with `0x00A64900`; nothing is applied or scripted, the list is not cleared first. The effect-id counter `0x00C1CF10` (the next id; `SavegameEffectStartingID` of GMST) is where a new effect of a nonzero type takes its id (ctor `0x00A652A0`: `id = counter; counter = counter+1`, wrapping from -1 to 1). A writer that adds effects must keep the counter above the ids it uses. (C)

### One effect: CGameEffect writer 0x00A65490, GME1 = template 22 (C)

`this` = the effect (0x7C bytes), stack = (a, b, struct); with a struct it writes through the setters; the other path (a null struct, name tables 0x00B1BA08...) is the other serializer and not used for saves. Order and sources:

| order | id | name | type | source (+offset) | loader reads |
|---|---|---|---|---|---|
| 1 | 16613 | SavegameEffectID | s32 | `+0x20` | `+0x20`, default 0 |
| 2 | 16614 | SavegameEffectType | s32 | `+0x24` | `+0x24` |
| 3 | 16615 | SavegameEffectDurationType | s32 | `+0x28` (0 instant?, 1 permanent, 2 temporary, 6 special: COMBAT.md:186, sav-inv.md) | `+0x28` |
| 4 | 16616 | SavegameEffectDuration | s32 | `+0x2C` (ms) | `+0x2C` |
| 5 | 16618 | SavegameEffectTimeIndex | s32 | `+0x30` = **elapsed ms** (`AddElapsed`, capped at the duration: COMBAT.md:186) | `+0x30` |
| 6 | 16619 | SavegameEffectAnimation | s32 | getter `0x00A53690` = `+0x34` | `+0x34` |
| 7 | 16620 | SavegameEffectPriority | s32 | getter `0x00A65380`: **999 if the duration type is 6**; else if animation == 0 and type >= 12 then **0**; else `+0x38` | the same getter recomputes it after the duration type and animation are read, so the value read back is the same one |
| 8 | 16621 | SavegameEffectCreator | **u32** | `+0x40` (-1 = none) | `+0x40` via `0x00A65250` |
| 9 | 16622 | SavegameEffectAbilityID | s32 | `+0x44` | `+0x44` |
| 10 | 16625 | SavegameEffectResource2 | s32 | `+0x78` (-1 default) | `+0x78`, only if present |
| 11 | 16627 | SavegameEffectFlags | s32 | getter `0x004F6A90` = `+0x3C` | `+0x3C`, only if present and not null |
| 12 | 16640 | SavegameDataarray | struct DTA1 | the effect **is** a `DataArray` (lists at `+4..+0x1C`): `0x00A5AA60(effect, a, b, handle)` | loaded second |
| 13 | 16624 | SavegameEffectEngineData | struct DTA1 | the second `DataArray` at `+0x54` | loaded first, only if present |

Not saved: `+0x34`'s siblings `+0x48` (invalid word; an invalid effect is skipped whole), **`+0x4C`** (-1 by the constructor), **`+0x50` (started flag)**, `+0x74` (-1). On load these take the constructor's values, so a loaded temporary effect is "not started"; the first `AgeEffect` (0x007A0B10: skip if `+0x48 == 1`; if `+0x50 == 0`, run the VFX start when engine int 2 is > 0, `0x0079FA70`, then `+0x50 = 1`) starts it and ages it from the saved `+0x30`. (C for 0x007A0B2E-0x007A0B71; the VFX start's detail not followed)

DTA1 writer `0x00A5AA60` (also used for commands' and sub-actions' data): for each of the seven lists in this order, fetch the list field (`0x00A5A8E0`), set its count to the array's list size (0 when the array's list is null), write the elements:
16641 ints (array `+4`, s32), 16642 floats (`+8`), 16643 bools (`+0xC`, u8), 16644 OIDs (`+0x10`, u32), **16647 quaternions (`+0x1C`) before 16646 vectors (`+0x18`)**, 16645 strings (`+0x14`). The loader (`0x00A5BAD0`) handles the same seven in the order ints, floats, bools, ids, quaternions, vectors, strings, only lists that are present, non-null and non-empty, replacing the array's list. Difference: the writer writes an empty array as empty lists; the loader leaves an existing list alone when the saved one is empty, which is harmless on a fresh effect. (C)

Ctor defaults for a fresh effect (for a writer that builds one): id 0 (or the counter for a nonzero type), type = the argument, duration type 0, duration 0, time index 0, animation 0, priority 0, flags 0, creator -1, ability 0, `+0x48` 0, `+0x4C` -1, `+0x50` 0, `+0x74` -1, `+0x78` -1. (C: 0x00A652C6-0x00A6531C)

### Store: Store 0x007EB510 (slot 21), STO1 = template 44 (C)

Base fields first (23, 4, 3, 16222, 16221 only if the object's script flag is set, 17000; STO1 has no Orientation, Active, Plot, Name, effects, commands). Then:

| order | id | name | type | source (+offset) | condition |
|---|---|---|---|---|---|
| 1 | 16150 | SavegameStoreMarkDown | s32 | attribute `+0x464` | always |
| 2 | 16151 | SavegameStoreMarkUp | s32 | attribute `+0x454` | always |
| 3 | 16152 | SavegameStoreGold | s32 | `[+0x474]` | always |
| 4 | 16153 | SavegameStoreMaxBuyPrice | s32 | `[+0x478]` | always |
| 5 | 16154 | SavegameStoreWillNotBuy | list u32 | the vector at `+0x47C` (begin `+0x480`), element by element (a missing element is the default -1) | the list can be made |
| 6 | 16155 | SavegameStoreWillOnlyBuy | list u32 | the vector at `+0x48C` (begin `+0x490`) | same |
| 7 | 16156 + 16226 | SavegameStoreItemList (ITL1) + SavegameMaxItems | | the container at `+0x444` (ListItemLocation) slot 4 -> item-list writer | always |

Loader (`0x007EC0B0`) against it: all read with the same widths. Differences: (a) the base writes `ObjectID` (23) and the store list loader **never reads it** (a new id is made); (b) 16154/16155 are **appended** to the vectors the template (`.utm`) already filled, with no duplicate test (`0x007EC194-0x007EC1A9`, `0x007EC200-0x007EC240` plain push_backs), so saving the template's lists and loading them again doubles them on every cycle (I: not seen in play; harmless if the engine only tests membership); (c) after the item list is read the loader calls `0x007CA4F0(container, -1)` (not followed). (C)

### Variables (ScriptVarTable 17000), the writer `0x00705A70(table = object+0x2E0, struct)` (C for the shape, I for the type filter)

For the one list field 17000 (generic list, label 0x4268): for each of 13 type buckets (the table is 13 vectors of 0x14-byte records), every record whose **changed byte is nonzero** is looked up in the list already in the struct (same name 17001 and same template); if found its value (17003, label 0x426B) is replaced, else collected and appended after (`0x004DD940` with the template label `0x00702930(template index)`, name `0x004799E0(17001)`, value `0x00702BC0`). The template for a type comes from `0x007024A0`: types 1, 2, 3, 4, 12 have the five known templates (SVI1, SVF1, SVS1, SVO1, SVR1 at `0x00C35D94-0x00C35DA4`), types 5-11 map to template 0 (no template; I: such variables, locations, events, commands, effects, item properties, are not skipped by the writer at that point, they would be written with template 0, which the reader skips as "any other label"). The loader's 17000 handling is in sav-area.md section 9. (Not followed to the end: the exact test that keeps template-0 types out; `dev/script_commands/vars.md:139` lists the same gap.)

### Evidence (addresses)

- Vtables (dwords, `lib-agent-2-dw.py`): CCObject main vtable 0x00B0EE9C: slot 21 = 0x00697EE0, slot 34 = 0x00698ED0. CCPlaceable main 0x00B1C084 (the dword at 0x00B1C080 is the RTTI locator): slot 21 = 0x0074D600, slot 34 = 0x0074FE30. CCItem main 0x00B1C61C: slot 21 = 0x00754020, slot 34 = 0x00758340. Store main 0x00B242D4: slot 21 = 0x007EB510, slot 34 = 0x007EC0B0. ListItemLocation vtable 0x00B2308C: slot 4 = 0x007CA810 (save), 6 = 0x007CA990 (load), 19 = 0x007CAA00, 24 = 0x007CA6C0, 25 = 0x007CA730.
- The area calls each writer through `call [vtable+0x54]` on the object found in its cast loop: 0x00684148-0x00684156 (placeables), 0x0068450E-0x0068451C (another kind), same pattern for each list; the list is sized first (0x006840D9).
- Set helpers read: 0x00478340 (type 0), 0x004783B0 (5), 0x00478420 (4), 0x006EB250 (2), 0x0062F610 (8), 0x004799E0 (14), 0x00630810 (17), 0x005FE630 (10), 0x006968E0 (13); each is `get field entry by label (0x004D5230)`, `test the flag bits`, `cmp type`, then the raw store.
- Sources: base writer 0x00697F1F (id), 0x00697F2F (position), 0x00697FAD (orientation), 0x0069801D-0x00698026 (Active: +0x2C5 and +0x421), 0x006980E8 (variables), 0x006980ED-0x00698113 (event script), 0x00698128-0x00698171 (name), 0x00698176-0x006981BA (tag blanked when equal to the resref), 0x006981C5-0x006981CA (`cmp [eax+0x70]` the saving byte).
- Placeable writer 0x0074D60E-0x0074D834; item writer 0x00754071-0x007545B5; store writer 0x007EB526-0x007EB6DF; item list 0x00761D10-0x00761EB8; holder 0x00A64E20-0x00A64F8F; effect 0x00A65490-0x00A656E4; DTA1 0x00A5AA60 (field pushes 0x4101..0x4107 at 0x00A5AA9A, 0x00A5AB0F, 0x00A5AB84, 0x00A5AC09, 0x00A5AC81 [0x4107], 0x00A5ACDA [0x4106], 0x00A5AD2E [0x4105]).
- Seen in a real save (Lyna AutoSave_1, via `tools/gff4.py --dump`): PLC1 with `SavegameItems` of one ITL1 whose Position is `[0.0, 1.0, 0.0]`, a GME1 with duration type 6, priority 999, creator 269, Resource2 -1, Flags 0 and two DTA1s with empty lists written as `[]`.

### Not read

- What `[+0x600]` of an item is (the second Position coordinate in a placeable/store/backpack), and `+0x5D0` beyond "being destroyed".
- The loader setters 0x00752BF0, 0x00752C30, 0x00756540, 0x00753250 (that they set the attributes the writer reads, taken as true).
- Who sets the placeable's `+0x658` state, `+0x600` bodybag dword and `+0x664` from play; the post-load `0x0074DBA0` was not reopened.
- The writer's type filter for variables with no template (types 5-11), the effect's VFX start `0x0079FA70`, and `0x007CA4F0(container, -1)` after a store's items load.
- The party-pool test (`0x006316F0`) that keeps a creature out of the area's creature list.
- Whether items on the ground (dropped, not in a bodybag) exist as area objects at all (the area's loop has no item cast; a dropped item is probably a bodybag placeable: I).
- The Equipment/Backpack/PlotItems own writers (Inventory slot 27 `0x0073F7A0`, Equipment slot 26 `0x00797700`): the item-list writer they share is read here; their other fields are the creature note's.
- The other serializer path of the effect writer (null struct; name tables) and of the command writer.

## The area writer and the small kinds' writers (GOG exe), for the save writer (DAOLib #221)

Read 2026-10-21 by lib-agent-2 in `game\DAOrigins.exe` (GOG 1.05, base 0x400000), `tools\daoexe.py --disasm` and a labelled disassembly. C = checked in disassembly, I = inferred. Types are the GFF4 types of the save's own templates (read from `Characters\Lyna\Saves\AutoSave_1\savegame.das`, templates 34-45 and 26) and are the ones the writer helpers check (below), so they agree.

Prior work used, not redone: master dev/SAVES.md (the load), dev/PARAGON_SAVES.md (the templates), dev/LOAD_EVENTS.md "Area lists" (:36-62: leaving a list writes the areas into the save tree at app+0x14C; the area writer `0x00683C80` is called there), `%TEMP%\lib-agent-2-sav-area.md` (the LOADERS of everything here: ARE1, PLC1, triggers, SND1, WPT1, AOE1, STO1, the container, variables; section numbers below refer to it), `%TEMP%\lib-agent-2-rst-cmd.md` (the command part of the base writer), dev/AUDIO.md:537-555 (a sounds region's `CCSoundObjectPannable`). New here: every writer.

### Short answers

1. **The area writer is `CCArea` `0x00683C80`** (`this` = the area, arg = none; it finds its own struct). It writes into an ARE1 struct that **must already exist**: it looks it up by the area's tag (`0x006439A0`, bCreate 0, the tag being the 65-byte buffer at area+0x80) and does nothing if there is none. The struct is made earlier, by the area-list step `0x006FB3A0`, with **only `Tag` (1) set**; every other field is the template's empty default (C).
2. **The order of ARE1's fields as written** (C): `SavegameEffectList` 16623, `ObjectID` 23, `ScriptVarTable` 17000, then the objects are sorted by kind and `SavegameAreaMinCreatureImportance` 16020, then the lists: creatures 16011, triggers 16012, sounds 16019, placeables 16010, AOEs 16013, waypoints 16015, stores 16017, then the map 16016 and the rooms viewed 16018. Each list is **resized to its count first, then each element is filled** by the object's own save slot (CCObject vtable slot 21, +0x54), in the order of the area's object vector (area+0x694, ids).
3. **Each object goes into exactly one list, chosen by cast predicates tested in this order** (C): creature (slot 4) -> creatures, except a creature that `CCParty::IsInPartyPool` (id 12318, `0x006316F0`) answers true for, which is **not written at all** (this corrects LOAD_EVENTS.md:44, which guessed `GetCanDiePermanently`); trigger (slot 10) -> 16012; placeable (slot 9) -> 16010; AOE (slot 3) -> 16013; waypoint (slot 12) -> 16015; store (slot 20) -> 16017; sound (slot 16 true and slot 17 false) -> 16019. A `CCSoundObjectPannable` (slot 17 true) is never in the area's list: it is written inside its sounds-region trigger (TRAV 16111). **Nothing else is written**: an object matching none (an item lying in the area, `CCItem`; `CCVisualEffect`; a projectile) is dropped. There are no other kinds (no encounters, stages or cameras in the exe's `CC*` classes; map notes are waypoints).
4. **Every object's writer is "base, then the kind's own fields"** (the base `0x00697EE0` first; the loader does the opposite for some kinds, e.g. PLC1's 16100 is read before the base). The base writes `ObjectID` for every kind, including triggers and stores whose loaders ignore it. A field is written only if the struct's template has it (`0x004D9F90`, or inside the typed writer, which silently does nothing for a missing label or a different type), so a writer that adds a new struct must give it the **full vanilla template** and then write all of what is below.
5. **The writers' helpers check the field's type and do nothing on a mismatch**: `0x00478340` u8 (type 0), `0x00478420` u32 (4), `0x004783B0` s32 (5), `0x0062F610` f32 (8), `0x005FE630` vec3 (10), `0x006968E0` quaternion (13), `0x004799E0` string (14), `0x00630810` tlkstring (17), `0x006EB250` u16 (2), list-element setters (`0x0067BAD0` 4-byte, `0x004782F0` 1-byte for the map, `0x00799C30` vec3). So the width is the template's, and a field of another type is silently skipped.

### The area list (the struct that holds the areas)

- **Top struct field 16001 `SavegameAreaList`: a generic list** (type 0xFFFF, list, reference: template 0 at +0x80), elements ARE1 (template 45; the element template is the global `[0xC34BD0]`). The loader accepts a typed list as well (`0x006439A0` tries `0x004DA130` then `0x004DA040`) (C). The area list holds **one ARE1 per area ever in memory**, found by Tag (1, string), an equality test of the tag against the area's name (`0x004C0E10`, wstring compare).
- **Who writes the list** (C): the loading mode (`GameModeLoading::Worker` `0x006C6520` step 3, and `0x006C8800`, both) calls `CSAreaList` `0x006FB3A0` on the module's area list (module+0x2CC), then one area a frame `0x006FA5C0` (the list's cursor +0xC8; each call `0x00683C80` on `GetObject(entry id)`, `0x005D5960`). `0x006FB3A0`: first removes from 16001 every struct whose Tag equals one of the list's areas' names (the area's name at area+0x80), then **appends one fresh struct per area of the list, in the list's order** (element template `[0xC34BD0]`, then Tag (label 1) set by `0x006432A0`); structs of areas not in the list stay where they were. Entries of the module's list are 0x48 bytes apart (+0xB8 .. +0xBC) (C).
- So the saved area list is: the areas of other lists kept as left earlier, then the current list's areas in list order, each refilled by `0x00683C80`. **An area with no struct is built from its `.are` on load**; one with a struct (and `ObjectID` != -1) is rebuilt from it (sav-area.md section 1-2).
- **Current area / where the party is is not in this list.** It is in the campaign struct MODU, written by `0x005FEC00` (the same step, called just before `0x006FB3A0`; C): `CampaignCIFEntryAreaList` 17100 (string), `CampaignCIFEntryArea` 17101 (string), `CampaignCIFEntryPosition` 17102 (vec3, the controlled creature's +0x120) and `CampaignCIFEntryOrientation` 17103 (vec3, its +0x130, a direction, not a quaternion). Two branches: with a module area list and a controlled creature that has an area, the names come from that creature's area (I for which strings: the list name from module+0x2CC+0x68; the area name through the creature's area object +0x180; special case when `[app+0x114]` is in a mode where two data-table names (hashes 0x64210C1B, 0x4F978E51) replace them, I); otherwise from the module's own entry fields (+0xD0, +0x114, +0x150, +0x160). Then the same function writes `SavegameCampaignResource` 16014, `TimeBeforeDecay` 16499 (global `[0xC021C4]`), the module `ScriptVarTable`, the journal, world map, plot actions (campaign part, not mine).
- **The top struct is built in memory once** (`0x006489A0` makes a new GFF4 file, type `SAVE`, `V1.1`, platform `PC  `, and `0x00648530` lays the schema: fields MODU 16000 (struct), 16001 (generic list ref), PLYR 16002, PRTY 16003, GMST 16005, VERS 16004, ADIN 16006 (list), SSF1 16008 (struct), WDBE 16500 (list)); the area writers then fill the same tree (the tree at app+0x14C is the save, LOAD_EVENTS.md:38). Save-to-disk assembly was not followed further.

### The area writer `0x00683C80`, step by step (C)

| step | what | where | notes |
|---|---|---|---|
| 0 | copy the area's name (65 bytes at area+0x80), `0x006439A0(tree, tag, 0)` finds the ARE1 | | none found: return, nothing written |
| 1 | `SavegameEffectList` 16623 | `0x0079F200(area+0xCC, struct)` -> `0x00A64E20` | the template must have 16623 (it gets the list, returns if null). Per effect of the area's effect array: **skipped when the effect's dword +0x48 == 1**; the list is sized to the count minus those; each effect written by `0x00A65490` (the effects' part) |
| 2 | `ObjectID` 23 | area+4 object id, **u32** (`0x00478420`) | ARE1's template has 23 as u32 |
| 3 | `ScriptVarTable` 17000 | `0x00705A70(area+0x260, struct)` | a merge-writer: for each of 13 type buckets and each variable it looks for an element of the same type and name already in the list and updates it, else appends a new element whose template is `0x007024A0(type)` (SVI1..SVR1), name 17001 by `0x004799E0`, value 17003 by `0x00702BC0`. Order of new ones: by bucket, then by index. A fresh struct has none, so all are appended. The variables part has the rest |
| 4 | get the seven list handles | `0x004DA040` (typed: 16010, 16011, 16013, 16015, 16017, 16019) and `0x004DA130` (generic: 16012) | a missing field gives a null handle and the list is skipped later |
| 5 | sort | loop over the area's id vector (+0x694): `GetObject(id)` (`0x005D5840`), null skipped | by the predicates in short answer 3; the creature test is `GetParty()->IsInPartyPool(id)` |
| 6 | `SavegameAreaMinCreatureImportance` 16020 | area+0x23C, **s32** (`0x004783B0`) | only if the template has it (it does) |
| 7 | creatures 16011 | resize, element = `0x004DB2E0(i)`, `creature->vtbl[21](&elem)` | the CCCreature writer is `0x006B2470` (not read here) |
| 8 | triggers 16012 (generic list) | resize; per element `0x004DA1C0(i)`; **`0x004DD940(obj->vtbl[114]())` sets the element's template** (the kind's global, below); `0x00642D10` gets the struct; `obj->vtbl[21](&struct)` | an element whose struct cannot be got is skipped |
| 9 | sounds 16019 | typed, as 7 | `CCSoundObject` writer `0x007C2970` |
| 10 | placeables 16010 | typed | `CCPlaceable` writer `0x0074D600` |
| 11 | AOEs 16013 | typed | `0x007E4C70` |
| 12 | waypoints 16015 | typed | `0x007C3C30` |
| 13 | stores 16017 | typed | `Store` writer `0x007EB510` |
| 14 | map 16016 | only if the template has it and the list is got: `0x007C9680(area+0x4F8 map, &size)` gives the bit array and its byte size; list resized to the size; **one u8 per byte** (`0x004782F0`) | written even for size 0; the loader copies it back only if the size equals |
| 15 | rooms viewed 16018 | only if the template has it: the vector at area+0x464 (room ids), resized to its count, **each as 4 bytes** (`0x0067BAD0`) | all of them, in vector order |

The trigger kinds' element templates (slot 114, `0x00B1FEF4`+0x1C8; each returns a global the schema filled, C): TRGR `[0xC3899C]`, TRTP `[0xC389A0]`, TRAT `[0xC389A4]`, TRAR `[0xC389A8]`, TRAV `[0xC389AC]`, TRAM `[0xC389B0]`. In a vanilla save these are templates 35, 36, 37, 38, 40, 41 (I: the order of the globals matches the template order in the file).

### The base object writer `0x00697EE0` (CCObject slot 21; every kind calls it first) (C)

Everything below is "if the struct's template has the label". The command part is in rst-cmd.md.

| order | label | name | type | value | condition |
|---|---|---|---|---|---|
| 1 | 23 | ObjectID | s32 (`0x004783B0`; via `0x00A582C0`) | object id (obj+4 -> `0x0053CC40`) | always |
| 2 | 4 | Position | vec3 | obj+0x120..0x128 | always |
| 3 | 5 | Orientation | quaternion x,y,z,w | the animation component's rotation (`0x00A5CD00` on obj+0x70 slot 24), or if there is none **built from the facing vector at obj+0x130** (yaw `0x004EE570`, pitch `0x004EE5A0`, roll 0, to a quaternion `0x004EE200`) | template has 5 |
| 4 | 16256 | LoopingAnimation | u32 (`0x00478420`) | the animation component's +0x140 | template has it and the object has a component (creatures; not PLC1/triggers) |
| 5 | 16623 | EffectList | list | `0x0079F200(obj+0x70, struct)` (as the area's, step 1) | template has it. **Before** Active, Immortal and Plot |
| 6 | 16201 | Active | u8 | **1 iff byte +0x2C5 != 0 and byte +0x421 == 0, else 0** | template has it. (+0x421's meaning not found; I: set while the object is gone from the world) |
| 7 | 16220 | Immortal | u8 | byte +0x2C0 | template has it |
| 8 | 16250 | Plot | u8 | byte +0x2C4 | template has it |
| 9 | 3 | TemplateResRef | string | wstring at +0x1CC | template has it |
| 10 | 17000 | ScriptVarTable | generic list | `0x00705A70(obj+0x2E0, struct)` | template has it |
| 11 | 16221 | EventScript | string | wstring at +0x26C | **only if byte +0x288 != 0** (set by the loader only for a script that is non-empty and differs from the template's) and the template has it; else the field stays empty |
| 12 | 16255 | Name | tlkstring (PLC1, CRE1) or string | the LocalisedString at +0xD4 | template has it. Tlkstring (type 17, `0x00630810` -> `0x006303C0`): writes its **strref** (the LocalisedString's +4) as the first word and, **only if its byte +0x26 is set** (it holds literal text), the text as the second part (a string, `0x004DB0E0`). String-typed field (14): the text, only if +0x26 is set |
| 13 | 16222 | Tag | string | wstring at +0xB8, **written as "" when it equals the TemplateResRef string of step 9** (`0x004C0DE0` -> a wstring compare, case-sensitive: I) | always |
| 14 | 16721, 16740 | CommandList, CurrentActionQueue | | rst-cmd.md | the commands only when the loading data's byte +0x70 (`0x005D5EA0`) is set; else the lists are emptied and the CAQ1 blanked |

The base does **not** write `SavegameTeamID` 16453 or `SavegameObjectInteractionRadius` 16262: the kinds that have them (placeables, creatures) write them themselves.

### Placeables (PLC1, template 34): writer `0x0074D600` (CCPlaceable slot 21) (C)

Base first, then:

| order | label | name | type | value | condition |
|---|---|---|---|---|---|
| 1 | 16252 | MaxHealth | s32 | virtual getter +0x128 | always |
| 2 | 16251 | Health | s32 | virtual getter +0x120 | always |
| 3 | 16100 | AreaPlaceableState | u16 (`0x006EB250`) | word +0x658 | always |
| 4 | 16102 | AreaPlaceableUseable | u8 | byte +0x42C | always |
| 5 | 16601 | IsBodybag | u8 | (dword +0x600 != 0) | always |
| 6 | 16602 | LootableObjectID | u32 | +0x664 | always |
| 7 | 16253 | ObjectRank | s32 | +0x6BC | template has it |
| 8 | 16258 | ObjectPickLock | s32 | +0x5E0 | template has it |
| 9 | 16254 | ObjectTreasureGroup | s32 | +0x6C0 | template has it |
| 10 | 16257 | LootableCreatureAppearanceType | u16 | word +0x668 | template has it |
| 11 | 16455 | IsPlotGiver | u8 | byte +0x25A | template has it |
| 12 | 16223, then 16226 | Items, MaxItems | list of ITL1; s32 | the item container at +0x678: `container->vtbl[4](struct, 16223)` = `0x007CA810`: the item list (below), **then** `MaxItems` = the container attribute at container+0x28 | always |
| 13 | 16453 | TeamID | s32 | +0x3C4 | always |
| 14 | 16262 | InteractionRadius | f32 | +0x3CC | template has it |
| 15 | 16259 | ObjectTrapDetected | u8 | byte +0x5EC | template has it |
| 16 | 16260 | DCDetectCheck | s32 | +0x644 | template has it |
| 17 | 16261 | DCDisarmCheck | s32 | +0x648 | template has it |

Differences from the loader (sav-area.md section 5): the loader reads 16100 first and 16226 inside the container load, MaxItems **before** the items; the writer writes MaxItems after. The writer never writes anything the loader does not read. `RoomID1/2` are not written (the loader takes them from the `.are`).

**The container's item list `0x00761D10(struct, label)`** (C): iterate the container's items (`0x007641D0` iterator), **skip an item whose bool attribute at item+0x5D0 is true** (`0x00838210` reads byte +0xC; the attribute's name not found, I), resize the list to the count of the others, and per item: element `0x004DB2E0(i)`, **`item->vtbl[21](&elem)`** (the item writer `0x00754020`, below), then the item's cell is worked out (`0x007612E0`, container slot 19 `+0x4C`/`+0x64`, I) and written as **`Position` (4, vec3) after the item's own fields**, overwriting the item's Position. ITL1's `ObjectID` is the item's (base).

**Item writer `0x00754020`** (CCItem slot 21; shown because every ITL1 needs it; the items part is another reading): null struct = nothing; base `0x00697EE0`; then 16224 Droppable u8 (byte +0x4B0 != 0, always), 16231 Stealable u8 (+0x4B1, if the template has it), 1002 ItemStackSize s32 (attribute +0x5F0), 1011 ItemAbilityID u32 (attribute +0x660), 1020 ItemCraftingRecipeID s32 (+0x670, if has), 16225 Damaged u8 (attribute +0x5E0, if has), 1019 ItemSubitemsResRefs (string list; only if the field is a string-typed list: the resrefs of the object ids in +0x4BC, `-1` ids dropped), 1013 ItemProperties / 1014 Powers / 1015 EffectID / 1018 VFXID (four s32 lists, all four sized to the element count of the vector at +0x680, filled with attributes' values, +0x69C, +0x6BC, +0x6CC; **only if all four lists exist**), 16233 CurrentVFXPropertyID s32 (+0x6D8, if has), 16234 CurrentVFXPropertyPower u32 (+0x6DC, if has), 1021 ItemBaseCost s32 (+0x5B8, if has), 1001 ItemCost s32 (attribute +0x5A8, always), 16228 Irremovable u8 (+0x630 attribute, if has), 16229 Indestructible u8 (+0x640), 16230 MaterialType s32 (+0x4FC), 16232 Infinite u8 (+0x650), 16237 u8 (+0x46C). The base's `Active` is absent from ITL1; its `Plot` 16250, `Tag` 16222, `EffectList`, `ScriptVarTable`, `ObjectID` are the base's. Not followed further (C for the order and offsets; value names I).

### Stores (STO1, template 44): writer `0x007EB510` (class `Store` slot 21) (C)

Does nothing if the struct is null. Base, then: 16150 StoreMarkDown s32 (attribute +0x464), 16151 StoreMarkUp s32 (attribute +0x454), 16152 StoreGold s32 (+0x474), 16153 StoreMaxBuyPrice s32 (+0x478), 16154 StoreWillNotBuy (list of u32, the vector at +0x480..+0x484, if the template has it: resized, each element as 4 bytes), 16155 StoreWillOnlyBuy (the vector at +0x490..+0x494, same), then the item container at +0x444: `0x007CA810(struct, 16156)` (the ITL1 list as above, then `MaxItems` 16226). **Base writes `ObjectID` and `TemplateResRef` (3) and Position (4), `Tag`, `EventScript`**; STO1 has no Orientation/Active. Differences from the loader (sav-area.md section 9): the loader ignores the saved `ObjectID`; MaxItems is written after the items (read inside the container load).

### Triggers (templates 35-38, 40, 41)

Writers are CCObject slot 21 of each class (C): TRGR `0x00799E10`, TRAT `0x00799F70`, TRTP `0x00799EE0`, TRAR `0x0079A450`, TRAV `0x0079A600`, TRAM `0x00799FA0`.

**The trigger base `0x00799E10`** (all six): base `0x00697EE0` (ObjectID, Position, Orientation, Active, EventScript, TemplateResRef, Tag, variables, commands; **no Immortal/Plot**: not in the templates), then:

| label | name | type | value | condition |
|---|---|---|---|---|
| 16112 | AreaTriggerType | s32 | `obj->vtbl[115]()`: TRGR 0, TRAT 1, TRTP 2, TRAR 3, TRAV 4, TRAM 5 (the returns are constants: `0x004137B0`, `0x0079B310`, `0x004CCAE0`, `0x004CE240`, `0x00A11120`, `0x0046E640`) | always. This confirms sav-area.md section 6's table, which had 1 and 2 as I |
| 16101 | AreaTriggerGeometry | list of vec3 | the point array at obj+0x490 (count obj+0x48C): **each stored point minus the trigger's own Position** (x,y,z; `0x00799C30(i, point)`), list sized to the count | list got (template has it) and obj+0x490 != 0 |

Then per kind:

| kind | label | name | type | value |
|---|---|---|---|---|
| TRAT | 16110 | AreaTriggerLoadScreen | s32 | word +0x510 zero-extended (written; never read) |
| TRTP | 16104 | Disarmable | u8 | byte +0x514 |
| TRTP | 16103 | Detectable | u8 | byte +0x510 |
| TRTP | 16105 | DCDetectCheck | s32 | +0x518 |
| TRTP | 16106 | DCDisarmCheck | s32 | +0x51C |
| TRTP | 16107 | LastDisarmed | s32 | +0x520 (all five written, never read) |
| TRAR | 16108 | ReverbPreset | string | wstring +0x510 |
| TRAV | (as TRAR), then 16111 | AreaTriggerSounds | list of SND1 | the vector of sound ids at +0x538 (count +0x53C), `0x00799B90`: list sized to the count, per id `GetObject(id)` (`0x005D59B0`); if found, `sound->vtbl[21](&elem)` (the sound writer), **if not found the element stays empty** |
| TRAM | 16113 / 16114 | MusicVolumeEnterState / ExitState | s32 | +0x510 / +0x514 |
| TRAM | 16115 / 16116 | EnterStateDelay / ExitStateDelay | f32 | +0x518 / +0x51C |
| TRGR | | | | nothing more |

Loader differences (sav-area.md section 6): none for the read fields; the fields marked "never read" are the only ones written and ignored. The geometry the loader reads as "saved point plus saved Position" is what is written (point minus Position). **`ObjectID` is written but ignored by the loader**: a trigger gets a new id.

### Sounds (SND1, template 39): writer `0x007C2970` (CCSoundObject, and the Pannable class, slot 21) (C)

Base (ObjectID, Position, Tag; no Orientation/Active/TemplateResRef in SND1), then, all unconditional: 16900 SoundTag string (wstring +0xB8), 16901 SoundActive s32 (byte +0x4A0), 16902 SoundName string (+0x444), 16903/4/5 X/Y/ZPosition f32 (+0x120/124/128), 16906/7/8 X/Y/ZOrientation f32 (+0x130/134/138), **16909 WOrientation f32 = the constant 1.0** (not read from the object), 16910 Volume f32 (+0x47C), 16911 Pitch (+0x480), 16912 FadeIn s32 (+0x484), 16913 FadeOut s32 (+0x488), 16914 MaxDistanceMult f32 (+0x48C), 16915/16/17 ConeInside/Outside/Volume f32 (+0x490/494/498), 16918 Priority s32 (+0x49C), 16919 Occludable s32 (byte +0x4A3).

Differences from the loader (sav-area.md section 7): the loader stores 16909 into +0x13C, so a reloaded sound's W is 1.0 whatever it was; `SavegameObjectTag` 16222 is written (by the base) and overwritten on load by 16900.

### Waypoints (WPT1, template 43): writer `0x007C3C30` (CCWayPoint slot 21) (C)

Base (ObjectID, Position, Orientation, Active, Tag), then: 16710 MapNote u8 (byte +0x440), 16711 MapNoteEnabled u8 (+0x444), 16713 MapNoteType **u32** (+0x448), then if the template has 16714: **16714 MapNoteLocText tlkstring** (the LocalisedString at +0x44C: strref and, if it holds literal text, the text) and **16712 MapNoteText string = ""** (the literal empty string `0x00ADEF14`); else (no 16714 field) 16712 MapNoteText = the LocalisedString's text. Always written, whatever MapNote is (the loader stops reading after MapNote = 0). Real saves: `MapNote 1, Enabled 1, Text '', Type 2, LocText [238787, 0]`, which matches.

### AOEs (AOE1, template 42): writer `0x007E4C70` (CCAreaOfEffectObject slot 21) (C)

Base (ObjectID, Position, Orientation, EventScript, Tag; no Active/TemplateResRef/Plot in AOE1), then: 16603 AOEID s32 (+0x4CC), 16604 Shape u8 (byte +0x483), 16605/6/7 Radius/Width/Length f32 (+0x468/46C/470), 16608 Creator **s32** (+0x4AC; the template says s32: sav-area.md called it u32), 16609 Duration **u32** (+0x4B8), 16610 DurationType u8 (byte +0x4BC), 16611 Linked u8 (byte +0x4C4), 16750 AbilityID s32 (+0x4D4), 16751 Flags s32 (+0x4DC), 16752 Stationary u8 (byte +0x4A4, only if the template has it). Loader differences: none beyond sav-area.md section 8's quirk (an absent Flags reads the AbilityID); the writer always writes Flags.

### Not written / written but ignored, and mismatches in one list

- Written, never read: TRTP 16103-16107, TRAT 16110, every trigger's `ObjectID` and a store's `ObjectID`, the item `Position` from the base (overwritten by the container's cell).
- Read, with a different value on a round trip: Active (the writer's 0 when +0x421 is set), Orientation (the writer's is the component's rotation), SND1 W (always 1.0), `Tag` ("" if equal to the TemplateResRef; the loader turns "" back into the TemplateResRef, so it is a round trip), `EventScript` (empty unless the object's flag +0x288).
- Order differences writer vs loader: MaxItems after the items; PLC1 16100 first in the loader (stored, applied in the post-load); the loader's effect list is read after Position/Orientation/animation and before Active, as the writer's.
- Dropped: a party-pool creature; an item not in a container; an effect with dword +0x48 == 1; an item with attribute +0x5D0 set.
- Per kind, "always": ObjectID, Position, Tag; per template, the rest (above).

### Evidence (addresses)

- Area writer `0x00683C80` (the `CCArea` save, called from `0x006FA646`); dispatch loop `0x00683E92..0x0068407B`; vector pushes `0x006F1340` (a vector push_back); writes `0x00684080..0x0068471B`; the seven vectors are at esp+0x9C (creatures), +0x44 (triggers), +0xAC (placeables), +0x8C (AOEs), +0x74 (waypoints), +0x64 (stores), +0x54 (sounds); the list handles at E+0x104 (16011), +0xD4 (16012), +0xEC (16019), +0xF8 (16010), +0xE0 (16013), +0xBC (16015), +0xC8 (16017) (frame E at the loop).
- Predicates: slots 4/9/10/3/12/20/16/17 of the main vtables of `CCCreature`, `CCPlaceable`, `CCTrigger`, `CCAreaOfEffectObject`, `CCWayPoint`, `Store`, `CCSoundObject`/`CCSoundObjectPannable` each return `this` (`0x0079AF30`) where they are the cast and 0 (`0x004137B0`) elsewhere. `CClientExoApp::GetParty` (id 10504, `0x005D57C0`), `CCParty::IsInPartyPool` (id 12318, `0x006316F0`).
- Slot 21 (writer) and 34 (loader): CCObject `0x00697EE0` / `0x00698ED0`; CCPlaceable `0x0074D600` / `0x0074FE30`; CCItem `0x00754020` / `0x00758340`; Store `0x007EB510` / `0x007EC0B0`; CCTrigger `0x00799E10`, TRAT `0x00799F70`, TRTP `0x00799EE0`, TRAR `0x0079A450`, TRAV `0x0079A600`, TRAM `0x00799FA0`; CCSoundObject `0x007C2970`; CCWayPoint `0x007C3C30`; CCAreaOfEffectObject `0x007E4C70`; CCCreature `0x006B2470`.
- Item container (`0x007CA990` loader, writer = slot 4 of the same vtable `0x00B230A4`/`0x00B263F4`: `0x007CA810`), list writer `0x00761D10`.
- `CSAreaList` `0x006FB3A0` (id 15827), `0x006FA5C0`; `0x006432A0` creates the ARE1's Tag; `0x006439A0` finds. Loading mode `0x006C6520` (id 15080) and `0x006C8800` (id 15092) call both; `0x005FEC00` (id 11042) is the module writer.
- Tag-equals-template: `0x006980A0..0x006981AE`; Active: `0x0069801D..0x00698045`; tlkstring writer `0x006303C0`.
- Templates and types: `%TEMP%\lib-agent-2-wr-tpl.txt` (templates 34-45 with each field's type, flags and offset, from a real save).

### Not read

- The creature writer `0x006B2470` and the creature part of the areas (CRE1), the effect writer `0x00A65490` and the effects' skip rule (dword +0x48 == 1), `0x00A64A80`, the variable writer's value step `0x00702BC0`, the item attributes' names at +0x5D0 and the cell calculation `0x007612E0`, the meaning of byte +0x421 (the Active rule), the map object's bit order on write (the loader's, section 3 of sav-area.md, is what `0x007C9680` returns), the exact strings of the MODU entry area names (`0x005FEC00`, I), and how a save to disk assembles the tree after the area steps (`0x006489A0` makes the file; the tree at +0x14C is the save).
- The bytes after the last ITL1 field; the second word of a tlkstring beyond "literal text if the flag +0x26 is set".
