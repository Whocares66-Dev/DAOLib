# Script commands, group "bio_appearanc": the GOG exe's handlers

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, for DAOParagonEngine #27. Not seen in play. "(checked)" means read in the disassembly; "(inferred)" means not.

Shared facts (checked unless marked):
- Pops, pushes and the -639/-638 failures: `dev/script_commands/commands.md`. Lookups: `GetCreatureById` 0x005D58C0; 0x005D5840 any game object by id (then the object's slot +0x10 casts it to a creature); 0x005D5870 an item (type byte +0x14 = 7); `GetObjectById` 0x005D5BE0.
- A creature's `CAppearance` is the pointer at creature+0x7B0: +0x50 gender, +0x54 racial type, +0x58 `apr_base` row (u16), +0x5A the original row (u16). Its layout and the model built from it: `dev/CHARACTERS.md`.
- `CCCreature` 0x006C1400(appearance, keepCopy, skipFull, force), `ret 0x10`, applies an appearance to the creature's model (checked in outline): nothing unless creature+0x7B4 is set and the creature has a graphics object (+0x70 slot +0x60; inferred: its model is loaded); "changed" is force, or a different `apr_base` row or gender from the current; it rebuilds the model through 0x006C0DE0 (CHARACTERS.md's builder) and reloads the animation trees (0x006BDC70); when changed it also refreshes the stats' ability GUI (0x0071DEE0), the area (0x00679A00) and the creature's slot +0x8C.

### GetCreatureGender (263)
- **Handler:** 0x0065A400 (not shared). 0x005D5840, slot +0x10, then the byte `[creature+0x7B0]+0x50` (checked).
- **Does:** pushes the gender (1 male, 2 female, 3 other; CHARACTERS.md).
- **Edge cases:** invalid or non-creature: -1 (checked). The appearance pointer is not null-checked (checked; a creature always has one, inferred).

### GetCreatureRacialType (269)
- **Handler:** 0x0065A590 (not shared). 0x005D5840, slot +0x10, then `CCCreature` 0x006A7D90 (checked).
- **Does:** with stats, the value of the stats' attribute at +0xB0 (0x00719EA0); with no stats, `CAppearance`+0x54; with neither, -1 (checked).
- **Edge cases:** invalid or non-creature: -1 (checked). The value is not checked against `race_base` (1 dwarf, 2 elf, 3 human, CHARGEN.md).

### SetCreatureRacialType (270)
- **Handler:** 0x0065A600 (not shared). 0x005D5840, slot +0x10; compares with 0x006A7D90 and returns if equal; else `CCCreature` 0x006A7D50(race), then 0x006C1400(own appearance, 0, 0, 0) (checked).
- **Does:** 0x006A7D50 sets the stats' race (0x00719EB0: the attribute at +0xB0, then the race table's (0x4DF580(5), inferred `race_base`) `Name` column for the row, hash 0xB22D0123, into a string the stats keep; checked in outline) and `CAppearance`+0x54 (0x00424090) (checked).
- **State:** the model is rebuilt from the creature's own appearance; row and gender are unchanged, so not as "changed" (no ability-GUI or area refresh) (checked). No attribute adjusts, abilities or appearance row change: the scripts do those (`Chargen_SelectRace`, CHARGEN.md).
- **Edge cases:** invalid or non-creature, or the same race: nothing (checked). No range check: any int is stored. Returns 0.

### SetLocName (307)
- **Handler:** 0x00664440 (not shared). `GetObjectById`, builds a localized string (0x004D1C70: 0x004D1B80 clears it, strref at +4 = the int, its text empty), then the object's slot +0x130 (checked).
- **Does:** every class but placeables uses 0x00699F80: the name attribute at object+0xD4, its slot +0x54 (set). A placeable (0x0074CFC0) does that and also copies the string into its attribute at +0x454 and calls that attribute's slot +0x24 (checked; inferred: the placeable's displayed name).
- **State:** `SetName` (276, 0x0066BA00) writes the text field of the same attribute's string (0x004D1BB0) (checked). So `SetLocName` replaces both fields and **clears an earlier `SetName` override** (checked that both write object+0xD4's value; that non-empty text wins is script.ldf's).
- **Edge cases:** an invalid object does nothing (checked). The strref is not validated; -1 or an unknown id gives an empty name (inferred). Unlike `SetName`, it does not update the party's copy of the player's name (`SetName`'s +0x48C write for `player_core`) (checked). Returns 0.

### GetCreatureGoreLevel (719)
- **Handler:** 0x0065D5D0 (not shared). `GetCreatureById`, then `CAppearance` 0x00748520 (checked).
- **Does:** pushes the appearance's float at +0x180, or 0.0 when the global at 0x00C32938 exists and its +0x88 object's byte +0xC is 0 (checked; that this is the gore option being off is inferred).
- **Which option byte** (read 2026-10-22 by paragon-agent-2's helper, for DAOParagonEngine #169) [I, strong]: the global `0x00C32938` is the client options object (`SetPlotFlag`'s tutorial gate reads `ShowTutorials` at `[0x00C32938]+0x98`, `PLOTS.md`, and `0x0075F7E0` at `0x00760167` shows the same). The options' registration code names `ShowGore` (the wide string at `0x00B010A8`, referenced at `0x005E6C86`) with the id `0x4F7C9` and the very next option, `ShowTutorials` (`0x00B01448`), with the id `0x4F7CA` (`0x005E6C94` and `0x005E6CCA`, each through `0x005DEC10`). With `ShowTutorials` at +0x98 and the byte `GetCreatureGoreLevel` tests at +0x88 (a stride of 0x10 between attributes) the byte at +0x88 is the `ShowGore` option [I: the attributes' offsets were not read from the constructor]. When it is 0, `GetCreatureGoreLevel` answers 0.0; this also names the "gore option byte" that `ANIMATION.md` ("Partsbased", `0x00838210` on `[0xC32938]+0x88`) lists as not identified.
- **Edge cases:** invalid, non-creature, or no appearance: 0.0 (checked).

### SetCreatureGoreLevel (720)
- **Handler:** 0x0065D650 (not shared). Pops creature, float; `GetCreatureById`; `CAppearance` 0x00747C40(value), then `CCCreature` 0x006B73C0(appearance) (checked).
- **Does:** clamps the value to 0.0..1.0 and writes it to thirteen floats of the appearance, +0x180..+0x198 and +0x1B0..+0x1C4 (checked; inferred: one per body part or equipment slot). 0x006B73C0 then, if the creature has a graphics object, passes the level to the model (0x006B1BC0) (checked).
- **Edge cases:** NaN is not caught by the clamp's compares (inferred). Invalid, non-creature, or no appearance: nothing. The getter's gore-option check is not applied when setting (checked). Returns 0.

### GetItemHeraldry (171)
- **Handler:** 0x0065D770 (not shared). 0x005D5870 (an item), then the byte at item+0x46C (checked).
- **Does:** pushes the item's heraldry as 0–255.
- **Edge cases:** invalid or not an item: 0 (checked), as script.ldf says for items without heraldry.

### GetAppearanceType (150)
- **Handler:** 0x0065D6D0 (not shared). 0x005D5840, then the object's type byte +0x14 (checked).
- **Does:** creature (6): slot +0x10, `CAppearance`+0x58, the `apr_base` row. Placeable (10): slot +0x24, the u16 at +0x4F0 (the `placeable_types` row, as `placeable_h` uses it) (checked).
- **Edge cases:** invalid, any other type, or a creature with no appearance: -1 (checked). Values are u16, never negative.

### SetAppearanceType (178)
- **Handler:** 0x00674C80 (not shared). Pops creature, id, and bSetAsOriginal only when `paramCount` ≥ 3 (else 0); `GetCreatureById` (checked).
- **Does (checked):**
  - Only id -1 or id > 0 is accepted; 0 and below -1 do nothing.
  - bSetAsOriginal with id > 0 first writes the original row, `CAppearance`+0x5A (0x00747B40), on the live appearance.
  - If id's low 16 bits equal the current row (+0x58), it stops. So `-1` when the original is the current row does nothing.
  - It copies the appearance (`new` 0x1F0 bytes, 0x0074A440, copy 0x0074A610) and sets the copy's row (0x00748A40: +0x58, then `apr_base` `PerSpace` and `Height` or, for gender 2, `Height_F`, into the appearance) to id, or to the original (+0x5A) for -1. For id > 0 it also clears the copy's string at +0x1E4 (inferred: the head morph's name); -1 keeps it.
  - If the creature has a graphics object, the new row's model kind (0x007490D0: `apr_base` `ModelType`'s first letter, s 1, w 2, h 4, p 8, row 0 → 1) must share a bit with the graphics object's byte +0x10, or **nothing is applied** and the copy is freed (silently).
  - **Model rebuilt:** with creature+0x7B4 set, 0x006C1400(copy, 0, 0, 0): the row differs, so it rebuilds the model and the animation trees and does the "changed" refreshes. Without it (no model loaded), the live appearance's row is set directly (0x00748A40) and its +0x1E4 cleared, with no rebuild (applied at the next model load, inferred).
- **State:** `CAppearance`+0x58/+0x5A and the appearance's heights. Not the racial type (stats+0xB0) or `apr_base`'s `RulesRace` (checked: not read here).
- **Edge cases:** invalid or non-creature, or no appearance: nothing. Ids ≥ 65536 wrap to their low 16 bits (checked). No check that the row exists in `apr_base` (inferred: missing columns read as defaults). Returns 0.

## Not read
- 0x006C0DE0 and 0x006BDC70 in full (the rebuild itself; CHARACTERS.md and COMBAT.md read parts), and whether 0x006C1400 copies the appearance it is given into +0x7B0.
- The global at 0x00C32938 (inferred: options) and 0x006B1BC0 (gore on the model).
- What the stats keep from the race's `Name` in 0x00719EB0.
- `SetCreatureGender` (264, 0x0065A470), not this group: it builds an appearance copy with the new gender and calls 0x006C1400 the same way.

## Described elsewhere first (batch 4, read 2026-10-18 by lib-agent-2)

### SetCreatureGender (264)
- **Handler:** 0x0065A470 (not shared); `GetGameObjectById` 0x005D5840, virtual +0x10 (as creature).
- **Described in:** `dev/CHARGEN.md` (the creation screen's events: SELECT_GENDER), `dev/script_commands/bio_appearance.md` (mentioned under the appearance commands as building a copy and calling 0x006C1400).
- **Does:** if the creature's gender (0x006A3650, a byte) differs from nGender, makes a copy (0x1F0 bytes) of the creature's appearance (+0x7B0), sets the gender in it (0x00747B50), reloads the models (0x006C1400) and stores the gender (0x006A68C0).
- **Edge cases:** an invalid or non-creature object does nothing, returns 0 (checked). The same gender does nothing (checked). The copy source +0x7B0 is read without a null test (inferred risk for a creature with no appearance). nGender is compared after widening the stored byte, so a value past 255 never equals and is stored as given (inferred). Failed pop is -639 (checked).

### GetName (275)
- **Handler:** 0x0066BBA0 (not shared); builds a wide string for the answer.
- **Described in:** `dev/PROBE.md` and `dev/LIBRARY.md` (shadowed in play; the answer came through the pop and push unchanged); `dev/script_commands/bio_appearance.md` (SetLocName).
- **Does:** `GetObjectById`: an object's name is the text of its attribute at +0xD4 (0x004D1EB0). Otherwise the id is tried as an area (`GetAreaById`, name at +0x158), then as the module (`GetModuleById`, name from 0x0051B370).
- **Edge cases:** an id that is none of those pushes an empty string and does not fail (checked, the string starts empty). Failed pop is -639, failed push -638 (checked). The name is the one set by `SetName`/`SetLocName`, not the tag.

### SetName (276)
- **Handler:** 0x0066BA00 (not shared); pops object then string, `GetObjectById`.
- **Described in:** `dev/CHARGEN.md` (SELECT_NAME), `dev/script_commands/bio_appearance.md` (under `SetLocName`: it writes the text field of the +0xD4 attribute, 0x004D1BB0).
- **Does:** writes the text into the object's attribute at +0xD4, then calls the object's virtual +0x24 (a name-changed notification, inferred).
- **Edge cases:** an invalid object does nothing, returns 0 (checked). If the object's tag (+0xB8) equals `gen00fl_dog` (string at 0x00B05EF8), the party object's name field (+0x48C, via 0x005D57C0) is set too, so the dog's rename survives in the party (checked; that +0xB8 is the tag is inferred). Applies to objects only, not areas or the module (checked: no other branch).

### SetItemHeraldry (172)
- **Handler:** 0x0065D7D0 (not shared); `GetItemById` 0x005D5870.
- **Described in:** `dev/ITEMS.md` ("A creature's template items": "`SetItemHeraldry` refreshes the appearance").
- **Does:** stores the low byte of nHeraldry at item +0x46C, then, if the wearer (item +0x448) is a creature with an appearance (+0x7B0), maps the item's slot to an appearance slot (0x007480D0) and re-applies the heraldry to the model (0x00747D90) and reloads (0x006C1400).
- **Edge cases:** an invalid or non-item object does nothing, returns 0 (checked). The value is truncated to a byte (checked, `mov byte`). An unworn item only has the byte stored (checked). Failed pop is -639.

### LoadItemsFromTemplate (340)
- **Handler:** 0x0065D8A0 (not shared); pops object, string (0xA2F4C0), then int; `GetCreatureById`, then `ForScript_LoadItemsFromTemplate` 0x006C1E30(creature, &name, bReplaceInventory != 0).
- **Described in:** `dev/ITEMS.md` ("A creature's template items: `LoadItemsFromTemplate` (340)").
- **Does:** loads the template's equipment into the creature; the backpack is loaded and replaced only with bReplaceInventory.
- **Edge cases:** an invalid or non-creature object does nothing, returns 0 and the string is freed (checked). A failed pop of any of the three returns -639 and frees the string (checked). The bool is `!= 0` (checked). The template name is not checked here; a missing template is 0x006C1E30's matter (not read).

## Not read

- 0x006C1E30's handling of a missing template; 0x006C1400 and 0x00747D90 in full.
