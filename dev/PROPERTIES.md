# Creature properties as the engine keeps them

Read 2026-10-18 by lib-agent-2 (two subagents in parallel) in the GOG exe, the toolset's scripts and the game's tables, for DAOParagonEngine's next batch, at paragon-agent-2's request. How a creature's properties (attributes, depletables such as health and mana, the rest of properties.gda) are stored, set at load, scaled, read and written, and regenerated; what the engine does and what the scripts (creature_core, sys_autoscale_h) do. The two sections were read separately and agree on the store and the derived formulas. Each point says what was read and what is inferred. Not yet seen in play.

## Properties at load and scaling

Read 2026-10-18 in the GOG exe and the toolset's scripts by lib-agent-2 (a subagent), for DAOParagonEngine. "Read" is seen in the code; "inferred" is marked. Builds on `src/dao/Addresses.h` (a creature's +0x7C4 attribute holds its `CCCreatureStats`; the stats' +0x190 the properties by id; `CreatureProperties::GetTotal/Base/Current/Modifier`, `CCCreatureStats::SetBase/SetCurrent/SetModifier`) and `src/dao/Clock.cpp` (regeneration).

### The properties table and the four kinds

- `properties.gda` (table 98): `ID`, `Stat`, `Type` (`ATTRIBUTE`, `DEPLETABLE` or `SIMPLE`; 60 rows, ids 0–59), `Min`, `Max`, `NonCombatEffect` and `CombatEffect` (the regeneration rate properties of a depletable: health 14 and 28, mana/stamina 29 and 30), `EngineLink`, and UI columns. Read in the table.
- The engine has four property classes (RTTI): `CreaturePropertyAttribute` (vftable `0x00B1FA74`), `CreaturePropertyDepletable` (`0x00B210BC`), `CreaturePropertySimple` (`0x00B21064`) and `CreaturePropertyDerived` (`0x00B1FAD4`), each with an "engine link" sub-object at +0x3C. No row of `properties.gda` is typed `DERIVED`, so the derived class is unused in 1.05 (inferred from the table).
- The getters dispatch through each property's vtable: total slot 3 (+0xC), base slot 5 (+0x14), modifier slot 6 (+0x18), current slot 9 (+0x24). Read in `0x006435C0`, `0x006436B0`, `0x00643890`, `0x006437A0`.
  - Base is the float at +0x4C (`0x00795A10`), modifier +0x54 (`0x00795A20`) for every kind; a simple or derived property's modifier is always 0 (`0x0083A430`).
  - Current: a depletable's +0x5C (`0x0054F7B0`); every other kind answers −1.0 (`0x00795940`, the constant at `0x00AE2E00`).
  - Total, attribute and depletable (`0x0079C760`): base + modifier + the engine link's value, clamped (`0x0079C2D0`) between the property's limits at +0x2C and +0x34 (inferred: `Min` and `Max`). Simple (`0x0079C6D0`) and derived (`0x0079C970`) take no modifier.
- Setters (depletable, read): `SetBase` (`0x0079C860`) stores the base and then re-clamps current; `SetModifier` (`0x0079C890`) the same; current is clamped to 0…total (`0x0079C8D0`). So a depletable's current never exceeds its total, and lowering the total pulls current down.

### Derived values are the engine's, on the fly: the engine link

- A property's `EngineLink` (from `properties.gda`) indexes a static table of functions at `0x00C0F604`, called each time the total is read (`0x0079C5B0`, with a re-entry guard). Each reads the creature's attribute totals (property ids 1–6: STR, DEX, WIL, MAG, CUN, CON) and adds to the property's base and modifier. Read:

| EngineLink | Property | Adds | Function |
|---|---|---|---|
| 0 | most | 0 | `0x0083A430` |
| 1 | Attack (9) | max(0, (STR − 10) × 0.5) + max(0, (DEX − 10) × 0.5) | `0x0079CB30` |
| 2 | Defense (10) | max(0, DEX − 10) | `0x0079CC90` |
| 3 | Armor (11) | 0 | `0x0083A430` |
| 4 | AP_BONUS (53) | max(0, CUN − 10) / 7 | `0x0079CD40` |
| 5 | SpellPower (13) | max(0, MAG − 10) | `0x0079CDF0` |
| 6 | Health (7) | max(0, CON − 10) × 5 | `0x0079CEA0` |
| 7 | Mana_Stamina (8) | max(0, WIL − 10) × 5 | `0x0079CF50` |
| 8 | Resistance_Mind (32) | ((MAG − 10) + (WIL − 10) + (CUN − 10)) × 0.5, no floor | `0x0079D000` |
| 9 | Resistance_Physical (33) | the same over STR, CON and DEX (the operand order read; that each term is −10 inferred) | `0x0079D0F0` |

- So attack, defense, spellpower, the resistances, and health's and mana's maximum follow the attributes live: there is no stored derived value to recompute. Everything else (armor, crit, damage, regeneration rates, level, class) is a plain base + modifier set by the scripts and effects.

### Load from a template, and from a save

- The creature's template load (`CCCreature` slot 24, `0x006C0720`, read) reads, from the `.utc`'s GFF3: `Race`, `GroupId`, `Package` (+0x7E8), `PackageAI` (+0x7EC), `Rank` (the attribute at +0x4B0), `TreasureCategory`, `Combatant` (+0xBD4), `SoundSet`, `NoPermDeath`, `CharType`, `IsSelectable`, `IsPlotGiver`, `Trackable`, `WeaponsDrawn`, `StealthMode` (+0x7F0), `StealthRadius`, `DialogResRef`, then hands the template to its stats' slot 5 (`0x0071F520`).
- The stats' load (`0x0071F520`, read): every property is first reset to its default, max(Min, 0) clamped to Max, by `0x00795C10` (through `0x00795E20`, which drops the template argument: `call 0x795C10; ret 8`). Then `StartingPackage`, the spell, talent and skill lists (`0x0071F1F0` → `0x0071ECD0` for types 2, 1, 3: `SpellNumber`, `TalentNumber`, `SkillID`), and `PerceptionClass` (→ `ranges.gda`, see Perception).
- So the `.utc`'s `PropertyList` (`Index`/`Base` structs, e.g. `default_hm_noble`'s ten entries with `CurrentClass` 1) is not read by the template load: a template creature starts with every attribute at its `Min` (0), health and mana at 1 and 0. Read (the dropped argument); that nothing else reads the template's `PropertyList` is inferred (the exe has no "PropertyList" label; the list label it knows is the save's).
- A save's properties come back through `0x00795E30`, which reads the GFF4 list 16350 (`0x3FDE`) of the save's creature (read in outline; its fields not followed).

### Scaling is the scripts'

- Nothing in the engine scales by level or rank at load (read: the load above sets no level-dependent value; `Rank` is only stored). The scripts do it all at spawn: `creature_core`'s `EVENT_TYPE_SPAWN` computes `AS_GetCreatureLevelToScale(creature, AB_GetAreaTargetLevel(creature))` (the area's `areadata` row by `AREA_ID`, its `TargetLevel`, `MinLevel`, `MaxLevel`, `IgnoreAppMaxLevel`; the creature's rank through the difficulty's autoscale table `nLevelScale`; `apr_base`'s `MaxScaleLevel`), then `AS_InitCreature`, then `ScaleEquippedItems`.
- `sys_autoscale_h.nss` (read in the scripts): the class from the package (`packages.gda` `StartingClass`), the level and experience (`AS_AddClassLevels`: health, mana and damage bonus per level through `Chargen_ModifyCreaturePropertyBase`), the attributes from the strategy's `TABLE_AUTOSCALE_DATA` columns `s1`…`s6` (Constitution at least 10), the rank's bonus points (`nBonusPoints`), the package's abilities (`Ability1`…`Ability12`, `LevelsPerAbility`), the regeneration and threat defaults (`AS_SetCommonStats`), and the depletables filled with `SetCreatureProperty` base then current (`AS_SetDepletable`).
- A creature never spawned through these scripts keeps the template defaults: attributes 0, so every engine link adds 0.

## Properties at run time

Read 2026-10-18 in the GOG exe and the toolset's scripts by lib-agent-2 (a subagent), for DAOParagonEngine. "Read" is seen in the code; "inferred" is marked. Builds on `src/dao/Addresses.h`'s property and regeneration rows (`kCreatureProperties_Get*`, `kCreatureStats_Set*`, `kCCCreatureStats_UpdateRegeneration`). The property ids are `script.ldf`'s `PROPERTY_*` and `properties.gda`'s rows; value types `PROPERTY_VALUE_TOTAL` 1, `BASE` 2, `CURRENT` 3, `MODIFIER` 4; property types `ATTRIBUTE` 1, `SIMPLE` 2, `DEPLETABLE` 3, `DERIVED` 4.

### Where they live

- A creature's stats (`CCCreatureStats`, creature+0x7C4's value) hold its properties at +0x190, one object per id (ids below 1000; 1000+ and 2000+ index two more lists, `CreatureProperties::GetTotal` `0x006435C0`). Each is one of four classes by `properties.gda`'s `Type`: `CreaturePropertyAttribute` (vftable `0x00B1FA74`), `CreaturePropertySimple` (`0x00B21064`), `CreaturePropertyDepletable` (`0x00B210BC`), `CreaturePropertyDerived` (`0x00B1FAD4`). Read.
- Fields of a property object (read from its slots): +0x2C the minimum and +0x34 the maximum (`properties.gda`'s `Min` and `Max`, inferred from the values: Health 1–9999), +0x3C a formula block (+0x40 the formula's index, `properties.gda`'s `EngineLink`; +0x44 the stats; +0x48 a re-entry guard), +0x4C the base, +0x54 the modifier; a depletable adds +0x5C the current, +0x64 and +0x6C the ids of its regeneration properties (`NonCombatEffect`, `CombatEffect`), +0x74 a "don't regenerate" flag, +0x78 the current's minimum.

### GetCreatureProperty (738, `0x0066D0E0`)

Pops object, property, value type (default 1); the four getters each call a slot of the property object: total slot 3 (+0xC), base slot 5 (+0x14), modifier slot 6 (+0x18), current slot 9 (+0x24). Read. What each class answers:

| | total | base | modifier | current |
|---|---|---|---|---|
| ATTRIBUTE (`0x0079C760`) | clamp(base + formula + modifier, min, max) | +0x4C | +0x54 | −1 |
| DEPLETABLE (same total) | clamp(base + formula + modifier, min, max) | +0x4C | +0x54 | +0x5C |
| SIMPLE (`0x0079C6D0`) | clamp(base + formula, min, max) | +0x4C | 0 | −1 |
| DERIVED (`0x0079C970`) | clamp(formula, min, max) | 0 | 0 | −1 |

- So TOTAL is base + modifier plus the engine's formula, clamped to `properties.gda`'s Min and Max (`0x0079C2D0`). CURRENT is −1 for anything but a depletable. Read.
- The formula (`0x0079C5B0`) is a table of 12 functions at `0x00C0F604`, indexed by the property's `EngineLink`, called on the stats, guarded against re-entry; index 0 and 3 answer 0. It is computed on every read, from the attributes' TOTALs (each attribute read through its own slot 3), nothing stored. Read. `properties.gda` has no row of type DERIVED; the engine-linked rows are attributes and depletables. The formulas (read; "max0" is the attribute's total less 10, not below 0):
  - `EngineLink` 1, Attack (9): max0(STR)/2 + max0(DEX)/2 (`0x0079CB30`).
  - 2, Defense (10): max0(DEX) (`0x0079CC90`).
  - 3, Armor (11): 0.
  - 4, AP_BONUS (53): max0(CUN)/7 (`0x0079CD40`).
  - 5, SpellPower (13): max0(MAG) (`0x0079CDF0`).
  - 6, Health (7): max0(CON)×5 (`0x0079CEA0`).
  - 7, Mana_Stamina (8): max0(WIL)×5 (`0x0079CF50`).
  - 8, Resistance_Mind (32): from MAG, WIL and CUN, each less 10, summed and ×0.5 (`0x0079D000`; the exact signs not checked).
  - 9, Resistance_Physical (33): from STR, CON and DEX the same way (`0x0079D0F0`; the signs not checked).
  - The attribute ids are 1 STR, 2 DEX, 3 WIL, 4 MAG, 5 CUN, 6 CON.

### SetCreatureProperty (740, `0x00669890`) and UpdateCreatureProperty (741, `0x006699C0`)

- SetCreatureProperty pops object, property, value, value type (default 2 BASE). TOTAL (1) and BASE (2) both set the base (`CCCreatureStats::SetBase` `0x0071B230` → property slot 4: +0x4C = value); CURRENT (3) sets the current (`SetCurrent` `0x0071B320` → slot 11); MODIFIER (4) sets the modifier (`SetModifier` `0x0071B280` → slot 8: +0x54 = value). Read.
- UpdateCreatureProperty adds instead: CURRENT (3) adds to the current (`0x0071B370` → slot 10: current + value, then set through slot 11), MODIFIER (4) adds to the modifier (`0x0071B2D0` → slot 7: +0x54 += value). BASE and TOTAL do nothing. Read.
- A depletable's current is always clamped: setting it (slot 11, `0x0079C8D0`) stores clamp(value, +0x78, TOTAL), so CURRENT never passes TOTAL nor falls below its minimum. Setting a depletable's base (`0x0079C860`) or its modifier (slots 7 and 8, `0x0079C8B0`, `0x0079C890`) re-sets the current through the same clamp, so a lowered total pulls the current down; a raised total does not raise it. Read.
- Each setter then calls `0x00719A00` with the old current: for property 7 or 8 it tells the creature (`0x006AFB90` for 8) of the change, inferred for the UI's bars.

### Depletables (7 Health, 8 Mana_Stamina)

- The current (+0x5C) is kept apart from the total; its total is base + formula (CON or WIL) + modifier. Read.
- Nothing in the engine fills the current from the total at spawn (no such call found). The scripts do: `creature_core`'s SPAWN → `AS_InitCreature` (`sys_autoscale_h`) → `AS_SetDepletable(creature, max, prop)`: SetCreatureProperty(base = max), then GetCreatureProperty(total) and SetCreatureProperty(current = that total). Read in the scripts; that the template's saved current is used for a loaded creature is inferred.
- `SetCurrentHealth` (`core_h`) is SetCreatureProperty(7, value, CURRENT) for creatures.

### Regeneration (native)

- The engine regenerates: `CCCreature::Update` → `CCCreatureStats::UpdateRegeneration` (`0x0071B0E0`, ms, in combat) adds the milliseconds to the stats' +0x58 and each 3000 ms out of combat or 2000 ms in combat calls `RegenerateAll` (`0x00795BA0`), for the living only (`Addresses.h`, read 2026-09-30).
- A depletable's step (slot 13, `0x0079CAD0`) skips if +0x74 is set, else adds the TOTAL of the property named at +0x64 out of combat or +0x6C in combat to its current, through the clamp. Read (`Addresses.h`). From `properties.gda`: Health regenerates by Regeneration_Health (14, 0–50) out of combat and Regeneration_Health_Combat (28, 0–20) in combat; Mana_Stamina by Regeneration_Stamina (29, −20–50) and Regeneration_Stamina_Combat (30, −20–20). So each property's figure is per tick (3 s out, 2 s in), not per second; a negative stamina rate drains.
- The rates themselves are ordinary attributes the scripts set (base and modifiers, e.g. by effects).
