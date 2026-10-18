# Items as the engine keeps them

Read 2026-10-18 by lib-agent-2 (three subagents in parallel) in the GOG exe, the toolset's scripts and the game's tables, for DAOParagonEngine's next batch (items as world objects), at paragon-agent-2's request. How a creature's items are made from its template and by script, how equipping works and what it changes natively, and where the stats the combat scripts read come from. Builds on `dev/CHARACTERS.md` (the equipment and appearance slots), `src/dao/Equipment.cpp`, `dev/PROPERTIES.md` (the property modifiers equipping adds to) and the item-tint reading (materialtypes, MaterialProgress). Each point says what was read and what is inferred. Not yet seen in play.

## Items made and given

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe, with `default_hm_noble.utc` read as GFF3 for its struct IDs. "Read" means seen in the code; "inferred" is marked.

### A creature's template items: `LoadItemsFromTemplate` (340) and spawning

- `LoadItemsFromTemplate(object oCreature, string sTemplate, int bReplaceInventory)` (`0x0065d8a0`) pops the object, the string and the int, finds the creature, and calls `CCCreature::ForScript_LoadItemsFromTemplate`. Read.
- That loads the template as a GFF3 of resource type `0x7EB` (a `.utc`, inferred from the type). If it loaded (`[+0x5C]` non-zero), it saves and clears the creature's byte +0x7B4 for the duration, then destroys what the creature holds before loading the template's. Read.
  - Every item in the list at creature+0xBE0, the equipped items (inferred from what follows), gets item slot 0x198 with 0: `CCItem` slot 103 (`0x00752c90`), which removes it from its holder (`ForScript_RemoveItem`) and destroys it (`ForScript_DestroyObject`). This happens whatever `bReplaceInventory` says. Read.
  - With `bReplaceInventory`, every item in the list at creature+0xBF0, the backpack, is destroyed the same way. Read.
- Then `0x006bcc30(gff, struct, bReplaceInventory)` loads the template's lists, and `SetItemHeraldry` refreshes the appearance. Read.
- `0x006bcc30`, only while the creature's +0xC58 is 0:
  - **`Equip_ItemList`.** For each entry, a new `CCItem` is loaded from it (`0x00753800`, below). The equip slot is the entry's **GFF3 struct ID**, not a field and not `bitm_base`'s `EquippableSlots` (`0x00a36960`, the struct record's first dword). The weapon set is the entry's `SetNumber` field (−1 when absent). Read.
  - `0x006af630(item, slot, &reason)` checks the slot. If it allows it, `CCCreature::Equip(item, slot, SetNumber)`. If not, with reason 0 or 2 the item goes into the backpack list (creature+0xBF0); with any other reason it's neither equipped nor stored (inferred: lost). Read.
  - **`ItemList`**, the backpack, is loaded only when `bReplaceInventory` is set (`0x00761ec0` on the +0xBF0 list). So `LoadItemsFromTemplate(o, t, FALSE)` replaces only the equipped items and leaves the backpack alone. Read.
- `default_hm_noble.utc`'s `Equip_ItemList` struct IDs are the `INVENTORY_SLOT_*` numbers (`script.ldf`: MAIN 0, OFFHAND 1, RANGEDAMMO 2, CHEST 4, HEAD 5, BOOTS 6, GLOVES 7, CLOAK 8, NECK 11, BELT 12, BITE 13):
  - the wooden shield is ID 1 (OFFHAND) with `SetNumber` 0;
  - the longsword is ID 0 (MAIN) with `SetNumber` 0;
  - the chest is ID 4, the boots 6, the gloves 7;
  - the shortbow is ID 0 (MAIN) with `SetNumber` 1, the second weapon set.
  - Its `ItemList` entries all have struct ID 255 (`INVENTORY_SLOT_INVALID`). Read in the file.
  - Each entry's fields are `TemplateResRef`, `Stealable` and `Droppable`, plus `SetNumber` on the weapons.
- A creature's own template load (`0x006c0b40`, inside the load at `0x006c06f0`) calls the same `0x006bcc30` with `bReplaceInventory` = 1, so a spawned creature gets both lists the same way. Read. A second caller, `0x006c1fb3`, is not read (inferred: the save's load).
- An entry's item (`0x00753800`): its `TemplateResRef` (or, if that's empty, `InventoryRes`) names the `.uti`, loaded through item slot 0x1C0. Then item slot 0x1D0 reads the entry's own fields (`Stealable`, `Droppable` and the like) over it. Read; which `.uti` fields slot 0x1C0 copies is not listed here.

### `CreateItemOnObject` (212)

- `CreateItemOnObject(resource rItemFileName, object oTarget, int nStackSize = 1, string sTag = "", int bSuppressNotification = FALSE, int bDroppable = TRUE)` (`0x00673630`) pops in that order; the sixth only when given. Read.
- It does nothing and answers `OBJECT_INVALID` unless the implementer is the server's (`[this+4]` is 1, inferred), `nStackSize` > 0, and the target has an inventory (`0x005d5c20(target)`). Read.
- The items (`0x007595a0(list, tag, stack)`):
  - `0x00757eb0` makes one `CCItem` from the `.uti` (item slot 0x1C0, as above) and, if `sTag` isn't empty, sets its tag (`0x00696590`). Read.
  - If the item stacks (`0x00753c60`), the stack is split by the maximum stack size (`0x00753070`, inferred: `bitm_base`'s `StackSize`): full stacks as copies (`CCItem`'s copy constructor), then the rest. Each item's stack is set through its slot 116. Read.
  - If it doesn't stack, `nStackSize` separate copies are made. Read.
  - Every item gets `bDroppable` at +0x4B0. Read.
- While they're added, the global inventory flags `0x00C371B4` get 0x10, or 0x14 with `bSuppressNotification` (inferred: 0x4 suppresses the on-screen notice), and are put back after. `0x007634b0(list, inventory)` adds the items to the target's inventory. The answer is the first item's id. If the target's slot 0x24 answers (inferred: a placeable or container), it's set active. Read.
- **`EVENT_TYPE_INVENTORY_ADDED` (6)** (`0x00763100`, from the inventory's add):
  - It's not raised while the global flag 0x2 is set, when event 6 is in the owner's disabled set, +0x28C (`0x00698490(6)`; only `EnablevEvent` changes it and no shipped script calls it; corrected 2026-10-18 by lib-agent-2, `dev/LOAD_EVENTS.md`), or when the item's +0x5D0 check answers (not understood). Read.
  - Creator and target: the inventory's owner. Object 0: the item.
  - With the global flag 0x400, int 0 is 1 and it's delivered at once (`DeliverNow`). Otherwise int 0 is 0 and it's queued with no delay. Read.
  - The `0x00763342` site nearby raises 7, `INVENTORY_REMOVED`, the same way (inferred from the type stored there).

## Equipping

Read 2026-10-18 in the GOG exe by lib-agent-2 (a subagent), for DAOParagonEngine's items batch, building on `src/dao/Equipment.cpp` (the equip `0x006BC7F0` and unequip `0x006B7690` every path meets in, DAOLib #22, seen in play 2026-10-04) and `dev/CHARACTERS.md` (the appearance's slots). "Read" is seen in the code; "inferred" is marked. Slots are `script.ldf`'s: `INVENTORY_SLOT_MAIN` 0, `OFFHAND` 1, `RANGEDAMMO` 2, `CHEST` 4, `HEAD` 5, `BOOTS` 6, `GLOVES` 7, `CLOAK` 8, `RING1` 9, `RING2` 10, `NECK` 11, `BELT` 12, `BITE` 13 (a creature's natural weapon), 14–19 Shale's and the dog's; `INVENTORY_SLOT_INVALID` 255; `INVALID_WEAPON_SET` −1. An item's base item row is its word at +0x464; its `bitm_base` `Type` is cached at +0x6E0 (`0x00752C70`): 1 misc, 2 melee weapon, 3 shield, 4 armour and clothing, 5 staff, 6 bow or crossbow, 7 ammunition, 8 rune.

### The script commands

- `EquipItem` (392) and `UnequipItem` (393) share `0x0065CDA0`. It pops the creature and the item (objects); for `EquipItem` it then pops the slot and the weapon set with their defaults, 255 and −1 (`ForScript_CommandEquipItem`). It calls `CCCreature::Equip(item, slot, set)` or `CCCreature::Unequip(item)` and pushes the success as an int (0 when the creature or the item is not found). Read.
- `GetItemInEquipSlot` (401, `0x00669C70`) pops the slot (int) and the creature (object), and the weapon set (int) only when called with three arguments, else −1. For slot 0 (`MAIN`) it first looks in slot 13 (`BITE`, set −1) and answers a natural weapon there if there is one; otherwise it looks in (slot, set) of the creature's equipment (+0xBE0 → `0x00761260`). An item being destroyed (+0x5D0) answers `OBJECT_INVALID`. Read. That set −1 means the active weapon set is inferred (`SwitchWeaponSet`, 273, changes it).

### The equip, `CCCreature::Equip` (`0x006BC7F0`, `__thiscall (item, slot, set)`)

- Slot 255 lets the item choose its own slot (`0x00762C40`, inferred: its first fitting `EquippableSlots`); any other slot goes to (slot, set) (`0x00762990`), which validates the item against the slot (not read further). What was in the slot is moved to the inventory by the equip itself, not through the unequip (seen in play 2026-10-04, `src/dao/Equipment.cpp`). Read.
- Ammunition (Type 7): before equipping, the weapons in the set's main and off hands are looked up and, when the main hand's `bitm_base` `Ammo` column (hash `0xE6203B7E`) is 0 or not the ammunition's base item, the hands' items are unequipped. Read in outline; the decompiled variables are tangled, so which item is tested against which is inferred.
- On success the item leaves the inventory (`0x00762FB0` on +0xBF0) unless `0x0086BEE0` answers. Then, for a melee weapon, shield or staff (Type 2, 3, 5), whatever is in `RANGEDAMMO` (slot 2) of that set is unequipped: ammunition that no longer fits (read; `src/dao/Equipment.cpp`: "three places"). For a follower in a party the module is told (`0x00693290(0x42F, …)`, not read). Last, unless the game mode is 9, `0x006951A0`, and the stats object's `0x0071D4A0` (inferred: recompute). Read.

### What equipping does to the creature (`0x006C16E0`, `__thiscall (item, slot)`; inferred to be the slot's on-equip, from what it does)

- A two-handed weapon (`0x00A664C0`) unequips what the other hand holds; equipping the off hand (slot 1) while a two-hander is in the main hand unequips the main hand. Read.
- The item's +0x4A8 is set to the creature's id (its owner). Read.
- **Stats:** unless the creature's +0xC58 is set, `0x0071B3C0(item, +1.0)` adds the item's stats to the creature's property modifiers: for each row of `itemstatdefines` (109) with a creature property, the stat named by its `Label` is read from `itemstats` (110) at the item's base item row and multiplied by the same stat of `materialrules` (159) at the item's material (`materialtypes`' `Material`, from the item's MaterialType +0x4FC; 1 when not above 0), times +1.0; `0x0071B2D0(property, value)` adds it. Then `0x00759A40(creature, item)` (inferred: the item's own properties). Unequipping calls the same with −1.0. Read in outline; which factor is the material's is inferred from the tables named.
- The appearance's slot (`0x00748DA0(slot, item)`), heraldry for slots 0–2, the wield (`0x006A6640(slot)`), and the stats recomputed (`0x0071B970`). Read.
- **`EVENT_TYPE_EQUIP` (11):** when event 11 is not in the creature's disabled set, +0x28C (`0x00698490`; only `EnablevEvent` changes it and no shipped script calls it; corrected 2026-10-18 by lib-agent-2, `dev/LOAD_EVENTS.md`) and the item is not being destroyed, delivered at once (`AIMaster::DeliverNow`) to the creature (caller the item): creator the creature, object 0 the item, int 0 the slot, int 1 a bit of a global flag word (`DAT_00C371B4` bit 7; the equip sets 0x52 and 0x10 there, meaning not read). Read.
- If the creature is the hero, the party's +0x4AA is set (inferred: the inventory screen to refresh). Read.

### Unequipping (`0x006C19F0`, the matching on-unequip)

- The item's owner (+0x4A8) back to −1, the equipment updated, `0x00757B90`, the stats taken off (`0x0071B3C0(item, −1.0)`), and unless `0x006AB100` answers, the appearance's slot cleared (`0x007489C0`), heraldry and the wield as above, then `0x0071B970`. Read.
- **`EVENT_TYPE_UNEQUIP` (12):** as EQUIP, to the creature (caller the item): creator the creature, object 0 the item, int 0 the slot. Read.

### What the scripts add

- The armour and weapon numbers the combat scripts read come through the creature's property modifiers this adds (armour, attack, defense and the rest named in `itemstatdefines`), so a fight needs no script on equip to have them; the scripts' own handlers of 11 and 12 (in `creature_core` and the item scripts) add what they add on top. Inferred; the scripts' handlers not read here.

## Item stats

Read 2026-10-18 by lib-agent-2 (a subagent) in the GOG exe, the toolset's scripts and the game's tables. "Read" is what the code shows; "inferred" is marked. An item's base item type (bitm_base's row) is the u16 at item+0x464, its MaterialType (materialtypes' row, which `ScaleEquippedItems` sets by level) the attribute at item+0x4FC.

### GetItemStat (305), the stat the combat scripts read

- `ScriptCommand::GETITEMSTAT` (`0x00665670`) pops the item and the stat type and pushes `ScriptBackend::ForScript_GetItemStat(item, type)` (0.0 for no item). Read.
- `ForScript_GetItemStat`: the stat type is a row of `itemstatdefines.gda` (table 109), whose `Label` names a column (`0x007530A0` reads it and hashes it). The value is `itemstats.gda` (table 110) at the item's base item row, that column, times `materialrules.gda` (table 159) at the material's row, the same column; the material's row is `materialtypes.gda`'s `Material` for the item's MaterialType (`0x00753C90`). A multiplier of 0 or less, or a missing column, counts as 1. Read. So: stat = itemstats[base item][stat] × materialrules[materialtypes[MaterialType].Material][stat].
- The stat types (`itemstatdefines`): 1 Armor, 2 Damage, 3 Defense, 4 Attack, 5 CritChanceModifier, 6 ArmorPenetration, 7 AbilityCostModifier, 8 MissileDeflection, 9 OptimumRange, 10 Spellpower, 11 RngdPenalty, 12 AttributeMod. `itemstats` has these as columns (and DamageRange, DSpeed, BaseAimDelay, Attribute0/1); a longsword's row: Damage 7, DamageRange 1.5, ArmorPenetration 2, CritChanceModifier 2, DSpeed −0.1. `materialrules` scales Damage, Armor, CritChanceModifier, ArmorPenetration and others: Iron 1.0, Grey Iron 1.1/1.2/1.1/1.15, Steel 1.2/1.4/1.2/1.3, Veridium 1.3/1.6/1.3/1.45.
- Not counted: the item's own properties (the .uti's PropertiesList); `GetItemStat` reads only the base item's row and the material. What item properties add is not read here (inferred: the scripts' item property handling, as effects).
- The scripts' use: `combat_h` attack rating adds `GetItemStat(weapon, ATTACK)`; critical chance `CRIT_CHANCE_MODIFIER`; ranged penalty distance `OPTIMUM_RANGE`; `core_h`'s `DmgGetWeaponBaseDamage` is `GetItemStat(weapon, DAMAGE)` (unarmed `COMBAT_DEFAULT_UNARMED_DAMAGE`), its max damage that times `itemstats`' DamageRange; the attribute factor `ATTRIBUTE_MOD`; `combat_damage_h`'s armour penetration `GetItemStat(weapon, ARMOR_PENETRATION) + GetCreatureProperty(attacker, PROPERTY_ATTRIBUTE_AP)`. Read in the scripts.

### The armour value, and stats that become creature properties

- The scripts read a defender's armour as a creature property: `combat_damage_h`'s `DmgGetArmorRating` is `GetCreatureProperty(defender, PROPERTY_ATTRIBUTE_ARMOR)` (11), rolled 70 % fixed and 30 % random. Read.
- The engine puts equipment's stats into those properties. `itemstatdefines`' `CreatureProp` column maps a stat to a creature property: Armor → 11, Defense → 10, AbilityCostModifier → 41, MissileDeflection → 22, Spellpower → 13 (the others 0, none). `0x0071B3C0(item, sign)` walks the stats with a property and adds sign × (itemstats × materialrules, as `GetItemStat`) to that property's MODIFIER (`0x0071B2D0` keeps the current value: `CreatureProperties::GetCurrent`, then the property's slot 7, which adds to +0x54, the modifier (`0x00795A30`), then the current put back). Read.
- Its callers: the creature's item-equipped handler (`0x006C16E0`, from `Equipment`'s ItemLocation slot 22, `0x00797D5A`) with +1, unless the creature's +0xC58 is set; the item-unequipped one (`0x006C19F0`, from `0x00797E67`) with −1 (inferred by symmetry; its call at `0x006C1A5D` read); and the scripts' `SetItemMaterialType` (`0x00756540`) with −1 then +1 around the change, on the wearer. Read. So a creature's armour total is its base (properties.gda) plus every equipped item's Armor stat, scaled by material, plus effects' modifiers.

### The four weapon commands (one implementation)

- `GetWeaponAttackRating` (693), `GetWeaponBaseDamage` (699), `GetWeaponStrengthModifier` (701) and `GetWeaponArmorPenetration` (703) share `0x0066A3B0`, which switches on the command. Each pops the attacker and an int (`nRightHandWeapon`, 1 the main hand, otherwise the off hand; 703 also pops the target first), takes the hand's weapon from the creature's equipment (`0x006A2B40`: its slot-0 and slot-1 item), and reads one `bitm_base` column at the weapon's base item: 693 `CombatRating` (float, `0x006A8680`), 699 `BaseDamage` (int, `0x006A86F0`), 701 would read `StrengthModifier` (`0x006A8770`) but never reaches it: its case (`0x0066A4B6`) looks up a target handle it never pops (−1, always null) and so always pushes the default −0.1f (`0x00B0839C`; corrected 2026-10-18 by lib-agent-2, "The weapon style" below), 703 `ArmourPenetration` (`0x006A87E0`, British spelling). No material, no attributes. Read.
- `bitm_base` has `CombatRating`, `StrengthModifier` and `BaseDamage`, but no `ArmourPenetration` column, so 703 answers 0.0 for every weapon (read: the table's columns; the read fails and the default 0 stays). The target 703 pops is not used.
- The shipped scripts barely use these four: `rules_h`'s `Rules_GetWeaponAttackRating`, `Rules_GetWeaponArmorPenetration` and `Rules_GetWeaponBaseDamage` wrap them, and nothing calls those wrappers (grep of the toolset's scripts). Combat goes through `GetItemStat` and the creature properties above.

## The weapon style, and GetWeaponStrengthModifier

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG, base 0x400000) with `tools\daoexe.py --disasm/--refs`, a scratch rel32 call scan, and `bitm_base` from `packages\core\data\2da.erf` (`tools\gda.py --dump bitm_base`; base table, add-ins' M2DA rows not merged). No Ghidra, no game. **Checked** = read in the disassembly or the table; **inferred** = a reading of purpose.

### 1. Weapon style (GetWeaponStyle, 354)

#### The chain (checked)

- `GetWeaponStyle` → `0x006A3C30`: `[creature+0xC60]` (combat data), returns its `+0x5C`; 0 when there is no combat data.
- `+0x5C` is written only by `0x007C3600` (`__thiscall` on the combat data, one arg: a set record). It copies the record (`+0x1C` id, `+0x20` and `+0x3C` the two .anb names) and maps the id:

| Record | id | Trees (UTF-16 strings) | Style written to `+0x5C` |
|---|---|---|---|
| `0x00C38C04` (default, ctor at `0x00AD2F20`; also built on the stack by `0x006A4450`) | **−1** | `""`, `""` | **0** |
| `0x00C38C40` (`0x00AD2FC0`) | 0 | `OW.anb`, `OWOverlay.anb` | 1 |
| `0x00C38C7C` (`0x00AD3060`) | 1 | `WS.anb`, `WSOverlay.anb` | 1 |
| `0x00C38CB8` (`0x00AD3100`) | 2 | `TW.anb`, `TWOverlay.anb` | 3 |
| `0x00C38CF4` (`0x00AD31A0`) | 3 | `DW.anb`, `DWOverlay.anb` | 2 |

  The code: id == [C7C] or [C40] → 1; == [CF4] → 2; else `(id != [CB8]) - 1 & 3` → 3 for TW, 0 for anything else. These five are the only set records in the exe (refs to the four statics: `0x006A9167`, `0x006BF98E`, the chooser, the setter, the static ctors/dtors). **There is no ranged, staff or unarmed set**: those creatures get the id −1 record with no trees.

- `0x006B8D40` (CCCreature, no args) is the only caller of both the chooser and the setter:
  1. Gate `0x006A25A0` → `0x0069F030`: the appearance (`[+0x7B0]+0x58`, `apr_base` = table 2) column **`CombatModelTree`** (hash `0xDE80B418`) must contain `"combat_humanoid"` (`0x00B0F318`). Otherwise nothing is chosen and the style stays what it was (0 from construction, inferred).
  2. The chooser `0x007C3500` (on the combat data; args: out record, the creature's equipment `+0xBE0`) picks a record (below).
  3. If the new id differs from the current (`0x00616C20` reads it), `0x007C3600` stores it, then `0x006B5B30` is called four times: (2, old base tree), (2, old overlay), (0, new base), (0, new overlay) (checked; inferred: the old trees taken out of the animation player and the new put in).

#### The decision table (the chooser `0x007C3500`, checked)

Items are read through `0x007F4EA0(slot)` → `0x00761260({slot, set −1})`, slot 0 MAIN and slot 1 OFFHAND of the active weapon set (set −1 = active is inferred, as in ITEMS.md). An item's `+0x6E0` is `bitm_base` **`Type`** (hash `0x12A02374`, stored at load by `0x007547FF`; checked). The two-handed test `0x00A664C0` on the item's base-item record (`item+0x440`, its `+0x24` the bitm_base row, so `item+0x464`) is `bitm_base` **`EquippableSlots`** (hash `0xD5B0AE22`, `0x00A66440`) **== 1** (`0x00A66470(0)` = 1<<0, the MAIN slot's bit alone). `WeaponWield` (`0x1DE44006`) is **not** read.

| Main hand | Off hand | Record | GetWeaponStyle |
|---|---|---|---|
| empty | anything (incl. a dagger) | default (−1) | **0** |
| `Type` ≠ 2 (bow/crossbow 6, staff 5, …) | anything | default (−1) | **0** |
| `Type` 2 and `EquippableSlots` == 1 | anything (not looked at) | TW (2) | **3** |
| `Type` 2, `EquippableSlots` ≠ 1 | empty | OW (0) | **1** |
| `Type` 2, `EquippableSlots` ≠ 1 | `Type` 3 (shield) | WS (1) | **1** |
| `Type` 2, `EquippableSlots` ≠ 1 | `Type` 2 (melee weapon) | DW (3) | **2** |
| `Type` 2, `EquippableSlots` ≠ 1 | any other `Type` | default (−1) | **0** |

`bitm_base` rows that matter (checked): Longsword 2, Dagger 3, Mace 4, Waraxe 49: Type 2, EquippableSlots 3 (one-handed). **Greatsword 5, Maul 36, Battleaxe 40: Type 2, EquippableSlots 1 → TW → 3** (two-handed = 3 confirmed from the code; not played). Shields 6/7/8/13/14: Type 3, EquippableSlots 2. Shortbow 19, Longbow 20, Crossbow 21: Type 6, EquippableSlots 1. Staff 16: Type 5. Creature Weapon 23/56/57: Type 2, EquippableSlots 8192 (→ OW in MAIN). The `BITE` slot (13) is not looked at.

#### The play results explained (checked against the table)

- **Bow → 0**: main `Type` 6 is not 2, so the chooser returns the default record (id −1, no trees); not a "ranged set", there is none. (Bow animations come from the appearance's own trees, inferred.)
- **Two daggers → 2**: main Type 2, EquippableSlots 3; off Type 2 → DW.
- **One dagger → 1**: off empty → OW.
- **Nothing → 0**: main empty → default.
- **Off-hand dagger alone → 0**: the chooser reads MAIN first and returns the default when it is empty; the off hand is never read.
- **Sword and shield → 1**: off Type 3 → WS.

#### When it is recomputed (checked: the three direct calls to `0x006B8D40`)

- `0x006BE1F8`, in `0x006BDC70`, the creature's animation player set up with its trees (COMBAT.md), gated by the same `0x006A25A0`: at spawn/load/appearance change.
- `0x006C1808`, in the on-equip `0x006C16E0` (after the appearance's slot `0x00748DA0`).
- `0x006C1A8B`, in the on-unequip `0x006C19F0`, **skipped when `0x006AB100` answers** (the creature is dead, vtable `+0x1D0`, and something about its `+0x790` object; inferred: a corpse being looted keeps its set).
- **Weapon-set switch** (`SwitchWeaponSet` 273 `0x0065CCC0` → `0x006A8FA0` → equipment `0x007977D0`): no direct call to `0x006B8D40`; the switch takes the old set's items out of their slots (`0x00762100(…, item, −1)`) and equips the new set's through `0x006BC7F0`. The recompute then comes from the on-unequip/on-equip handlers, reached virtually (`Equipment`'s ItemLocation, ITEMS.md: `0x00797D5A` → on-equip, `0x00797E67` → on-unequip). Inferred: the virtual dispatch was not followed to the end.
- No other writer of `+0x5C`; no per-frame recompute. Non-humanoid appearances (CombatModelTree without `combat_humanoid`) never change it.

(Other readers of the record id, not needed here: `0x006A9140` maps OW/WS → 0x150, TW → 0x151, DW → 0x152; `0x006BF960` maps them → 0x16C/0x16D/0x16E.)

### 2. GetWeaponStrengthModifier (701) and DEPRECATED_GetCreatureStrengthModifier (700)

#### The handler (checked)

`0x0066A3B0`, shared by 693/699/700/701/703; `eax = cmd − 693`, byte table `0x0066A590`, jump table `0x0066A578`: 693 → `0x0066A424`, 699 → `0x0066A455`, 700 → `0x0066A490`, **701 → `0x0066A4B6`**, 703 → `0x0066A4EC`.

Prologue (all cases): the float result is initialised from **`[0x00B0839C]` = −0.1f** (`0xBDCCCCCD`), the int (nRightHandWeapon) to 1, the target object slot and `edi` to −1; the attacker is popped and looked up (`GetCreatureById` `0x005D58C0` → esi).

Case 701 (`0x0066A4B6`):
1. pops `nRightHandWeapon`;
2. calls **`GetCreatureById(edi)` with edi = −1**, a target lookup copied from 703's case; 701 never pops a target;
3. if the attacker **or that lookup** is null → `0x0066A54A`: pushes the initial float, **−0.1**;
4. only otherwise would it call the backend `0x006A8770(hand)`.

`GetCreatureById(0xFFFFFFFF)` is always null (checked): `0x00A58190` → `0x004D5EB0` returns 0 at once for handle −1. **So 701 returns −0.1 for every call**: any weapon, either hand, no weapon, even an invalid attacker. Only a failed int pop differs (−639, script error). That is the −0.1 seen in play for the longsword and the daggers.

The unreachable backend `0x006A8770` (checked): `0x006A2B40` gives the creature's slot-0 and slot-1 items; hand == 1 → main, else off; `GetItemById` `0x005D5870`; reads `bitm_base` (table 6) **`StrengthModifier`** (hash `0x1772174C`) at the item's base row `item+0x464`; 0.0 when there is no item or the read fails. It would have answered 1.0 for a longsword or dagger (bitm_base: Longsword 1.0, Dagger 1.0, Longbow 0.0, Staff 0.0). No `−1.1` or other arithmetic anywhere: the −0.1 is purely the handler's default constant.

#### Compared with 700 (checked)

Case 700 (`0x0066A490`) pops nothing more, pushes an **int**: 0 if the creature exists and its stats attribute `+0x7C4` is non-null, else −1 (`edi`'s initial −1). Neither command reads a weapon or an attribute: 700 is a stub returning 0, 701 is a broken case returning its default −0.1.

#### Side effects of the same default (checked)

- 693 `GetWeaponAttackRating` with an invalid attacker → −0.1 (not 0); its backend `0x006A8680` reads `CombatRating` (`0xA2CE7725`) without checking the read, 0.0 with no weapon.
- 703 `GetWeaponArmorPenetration` with an invalid attacker or target → −0.1.
- 699 `GetWeaponBaseDamage` pushes an int, −1 for an invalid attacker.

#### What the scripts use instead (checked)

No script in `toolset\extract\scripts` calls 701 or 700. The damage scripts' strength factor is `GetWeaponAttributeBonusFactor` (`core_h.nss`): `GetItemStat(oWeapon, ITEM_STAT_ATTRIBUTE_MOD)`, or `UNARMED_ATTRIBUTE_BONUS_FACTOR` when there is no weapon (used in `combat_damage_h.nss:289`, `core_h.nss:2860`). `2da_data_h.nss`'s `GetItemStrengthModifierBaseValue` (bitm_base `StrengthModifier` through `GetM2DAFloat`) is defined but not called.

This corrects "The four weapon commands" above, which said 701 reads `StrengthModifier`.

## ScaleEquippedItems (386): a creature's equipment set to a level's material

Read 2026-10-18 by lib-agent-2 (a subagent) in `game\DAOrigins.exe` (GOG 1.05, base 0x400000) with `tools\daoexe.py --disasm`, a call scanner over `.text`, `tools\gda.py`, and ghilana's merged tables, for DAOParagonEngine. Not seen in play. "(checked)" means read in the disassembly or the table; "(inferred)" means not. Builds on `dev/ITEMS.md` (item stats, `SetItemMaterialType`), `dev/PROPERTIES.md` (property slots), `dev/COMBAT.md` (0x0071EAC0, ability add), and `dev/script_commands/stats.md` (254, 333).

### The chain
- **Handler** `ScriptCommand::SCALEEQUIPPEDITEMS` 0x006664D0 (not shared): pops the object, then the int `nTargetLevel`; `GetObjectById` 0x005D5BE0, slot +0x10 (as a creature); a non-creature or invalid id does nothing; then `ScriptBackend::ForScript_ScaleEquippedItems` 0x006A88F0 (ID 14612), `__thiscall(creature, level)`, `ret 4`. Returns 0 (checked).
- **Script caller**: `Core Scripts\creature_core.nss:469-473`, `EVENT_TYPE_SPAWN`: `nLevelToScale = AS_GetCreatureLevelToScale(OBJECT_SELF, AB_GetAreaTargetLevel(OBJECT_SELF))`, `AS_InitCreature`, then `ScaleEquippedItems(OBJECT_SELF, nLevelToScale)`. Also `_Debug\active.nss:29` (debug). `script.ldf:8122-8133` says "area must have a valid area_id… creature must not be a party member" (checked).

### 0x006A88F0, step by step (checked)
1. `if (creature+0xCC4 != 0 && level == 0) return;` creature+0xCC4 is the byte **ItemsScaled**: saved and loaded as GFF4 field 16474 `SavegameCreatureItemsScaled` (save 0x006B2825, load 0x006BEDE5), 0 in the constructor (0x006BA6FD).
2. `if (creature+0xBD4 != 0) return;` +0xBD4 is the template's `Combatant` (`GetCombatantType` 763 reads it): `CREATURE_TYPE_NON_COMBATANT` (1) creatures are never scaled.
3. `if (CCParty::IsInParty(creatureId)) return;` (0x00631830 on the party from 0x005D57C0): **party members are never scaled**.
4. `creature+0xCC4 = 1` (set before any item is looked at, whether or not anything changes).
5. Area: `CCObject::GetArea` (slot +0xA0, 0x00695CD0); `areaId = area+0x6B4` (0 with no area). area+0x6B4 is the area's predefined variable 2, `AREA_ID` (written at area load 0x0068835C through the variable lookup 0x00702E90 with index 2; index 2 is registered with the UTF-16 string "AREA_ID" at 0x004E8608 → 0x00AF13B4).
6. `if (areaId == 0 && level <= 0) return;`
7. Level used: `L = level` if `level > 0`; else `L = areadata[areaId].TargetLevel` (table 225 `areadata`, column hash 0x39E0446F = `TargetLevel`, through 0x00A2CCC0), **−1 if the row/cell cannot be read**. So the area is needed only for `level <= 0`; a positive level never reads it.
8. `col = L / 3` as **signed integer division truncating toward zero** (`imul 0x55555556`, high word + sign bit). `if ((unsigned)col > 7) return;` **No clamp**: a column outside 0..7 (L ≥ 24, or L ≤ −3) makes the whole call do nothing.
9. For each of 12 equipment slots, in the order of the dword list at 0x00B0F880..0x00B0F8AC: **0 MAIN, 1 OFFHAND, 2 RANGEDAMMO, 4 CHEST, 5 HEAD, 6 BOOTS, 7 GLOVES, 9 RING1, 10 RING2, 11 NECK, 12 BELT, 13 BITE** (CLOAK 8 and the Shale/dog slots 14-19 are not visited):
   - item = equipment(creature+0xBE0)+0x24 → 0x007F4EA0(slot), which looks the slot up with weapon set −1 (inferred: the active set). Skip if none.
   - skip unless 0x00752C80: `item+0x6E4 != 0`, which is `bitm_base` (table 6) column **`ShowMaterial`** (hash 0x66F63962) at the item's base item, loaded at 0x00754822. ShowMaterial is 1 for weapons (longsword, dagger, mace, greatsword, maul, battleaxe, waraxe, bows, crossbow, staff), shields, armour, gloves, boots and helmets (32 base items); 0 for rings, amulets, belts, ammunition, clothing, mage hoods, creature weapons (bite), runes (checked in bitm_base). So rings/neck/belt/bite/ammo slots are visited but never change.
   - skip unless `item+0x50C` (the item's **MaterialProgress**: the template field "MaterialProgress", string 0x00B1C880, read at 0x00757571; also what `GetItemMaterialProgression` 369 returns) is non-zero.
   - `mat = ts_material[MaterialProgress].Material{col+1}`: table **199 `ts_material`** ("Treasure System Material"), the column hash from the 8-entry table at 0x00B0F860 = `Material1`…`Material8` (checked by hash). Default 0; **if the read fails or the cell is 0, the item is skipped** (no change).
   - otherwise `ScriptBackend::ForScript_SetItemMaterialType(item, mat)` 0x00756540 (ID 18027), **even if mat equals the current material** (no comparison).

### Column from level (checked)
| Level L | col (0-based) | ts_material column |
|---|---|---|
| ≤ −3 (only via an area TargetLevel) | negative | none: call does nothing |
| −2..2 | 0 | Material1 |
| 3..5 | 1 | Material2 |
| 6..8 | 2 | Material3 |
| 9..11 | 3 | Material4 |
| 12..14 | 4 | Material5 |
| 15..17 | 5 | Material6 |
| 18..20 | 6 | Material7 |
| 21..23 | 7 | Material8 |
| ≥ 24 | ≥ 8 | none: call does nothing |

So, in the peer's 1-based terms, column = L ÷ 3 + 1 with integer division, valid 1..8; **Material9..Material15 are never read by the engine** (only 8 hashes in the exe; checked). In the shipped `ts_material` (62 rows, core only; the one enciphered add-in file `ts_material_kcc` unread) the progressive rows are 43-48 and 49-53, 59; every row's Material8 equals Material9..15 except 59 "Robe, Cloth" (Material8 86 = Material9+). E.g. row 46 "Armor, All Metal": 1, 2, 8, 4, 5, 3, 6, 6 (materialtypes rows).

### Matching the peer's measurements
- "Above 23 leaves items alone": step 8 (col 8+ returns). Checked.
- "A column past 8 changes nothing": same bound; and the data's Material9+ equal Material8 anyway.
- "Below 1 leaves items alone": the engine does **not** use a level ≤ 0 as given. Level 0 with ItemsScaled already 1 (every spawned non-party creature: the spawn call sets it) returns at step 1. A negative level, or 0 on a never-scaled creature, scales to the **area's** `areadata.TargetLevel` (step 7), or does nothing if the area has no `AREA_ID` (step 6). Since the spawn scaling used a level derived from the same TargetLevel, re-scaling to it usually lands on the column already applied, which looks like "no change" (inferred). A creature in an area whose TargetLevel lies in a different column than its current material would change at level −1.
- A party member or a non-combatant is never changed at any level (steps 2-3).

### What SetItemMaterialType changes (0x00756540, checked in outline)
1. On the wearer (creature id at item+0x4A8, `GetCreatureById`): `0x0071B3C0(item, −1.0)` takes the item's stats off the creature's property modifiers (constant −1.0 at 0x00AE2E00).
2. Sets MaterialType (item+0x4FC, attribute slot +0x54).
3. `0x0071B3C0(item, +1.0)` puts them back with the new material: per `itemstatdefines` row with a `CreatureProp`, `itemstats[base][stat] × materialrules[materialtypes[Mat].Material][stat]` (ITEMS.md "Item stats"). So Armor/Defense/Spellpower/… modifiers are recomputed; `GetItemStat` (Damage, Attack, crit, AP read by the combat scripts) follows the new material live.
4. If the wearer has an appearance (creature+0x7B0) and the item's equipped slot maps to an appearance slot < 7 (0x007480D0): the model's material is changed (0x00747DD0), the part reloaded (0x006C1400), 0x006A8580(1,1) (inferred: appearance refresh).
5. Runes: compares the item's rune vector size (item+0x4BC) with 0x00753D90 (bitm_base `RuneCount` hash 0x017132EC, with a materialrules factor; inferred: the new rune-slot count); if fewer slots now, the runes past the count are destroyed (object slot 0 with 1), and the vector resized.
6. The holder (item+0x4E8) is notified (slot +0x28), 0x00755430 recomputes the item's value (`materialtypes.Material` hash 0x56BCE3D9 → `materialrules.CostMultiplier` 0x74E8EA95), then item slot +0x64 (not read).

### Not read

- 0x007F4EA0's weapon set −1 (inferred: the active set); item slot +0x64 at the end of SetItemMaterialType; 0x00753D90 in full.
- Whether the enciphered add-in `ts_material_kcc` adds rows (the running game reads it; Eluvian can).
