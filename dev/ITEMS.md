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

- `GetWeaponAttackRating` (693), `GetWeaponBaseDamage` (699), `GetWeaponStrengthModifier` (701) and `GetWeaponArmorPenetration` (703) share `0x0066A3B0`, which switches on the command. Each pops the attacker and an int (`nRightHandWeapon`, 1 the main hand, otherwise the off hand; 703 also pops the target first), takes the hand's weapon from the creature's equipment (`0x006A2B40`: its slot-0 and slot-1 item), and reads one `bitm_base` column at the weapon's base item: 693 `CombatRating` (float, `0x006A8680`), 699 `BaseDamage` (int, `0x006A86F0`), 701 `StrengthModifier` (`0x006A8770`), 703 `ArmourPenetration` (`0x006A87E0`, British spelling). No material, no attributes. Read.
- `bitm_base` has `CombatRating`, `StrengthModifier` and `BaseDamage`, but no `ArmourPenetration` column, so 703 answers 0.0 for every weapon (read: the table's columns; the read fails and the default 0 stays). The target 703 pops is not used.
- The shipped scripts barely use these four: `rules_h`'s `Rules_GetWeaponAttackRating`, `Rules_GetWeaponArmorPenetration` and `Rules_GetWeaponBaseDamage` wrap them, and nothing calls those wrappers (grep of the toolset's scripts). Combat goes through `GetItemStat` and the creature properties above.
