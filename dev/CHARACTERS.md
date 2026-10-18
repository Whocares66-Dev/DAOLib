# A character's model, assembled from its parts

How the engine builds a humanoid's model from its appearance and equipment: which parts it loads, what an empty slot draws, what hides what, and where a weapon hangs. Read 2026-10-17 in the GOG exe (Ghidra, `tools\daoexe.py`) and the game's data tables (ghilana), for DAOParagonEngine's character resolver and the core's. Addresses are GOG's; each has an ID in the address library.

## The appearance's slots

`CAppearance` holds the creature's appearance: `+0x50` its gender (1 male, 2 female, 3 other; any other value is stored as 1, `0x00747b50`), `+0x58` its `apr_base` row, and per equipment slot the item's base type (`+0x12a`, seven u16, `0x00747cf0`) and its variation (`+0x138`, seven bytes, `0x00747d30`). Its equipment slots are not the scripts' `INVENTORY_SLOT_*`; `0x007480d0` maps them:

| Slot | Part name (`0x00748120`) | From `INVENTORY_SLOT_` |
|---|---|---|
| 0 | `<chest>` | CHEST (4) |
| 1 | `GlovesM1` | GLOVES (7) |
| 2 | `BootsM1` | BOOTS (6) |
| 3 | `<main_weapon>` | MAIN (0) |
| 4 | `<offhand_weapon>` | OFFHAND (1), RANGEDAMMO (2) |
| 5 | `HelmetM1` | HEAD (5) |
| 6 | `CloakM1` | CLOAK (8) |

The head's parts are another enum (`0x00748160`): 0 `FaceM1`, 1 `EyesM1`, 2 `HairM1`, 3 `BeardM1`, 4 `NeckM1`, 5 `LashesM1`. Parts 0 to 3 take their variation table from `apr_base`'s `Head_Worksheet`, `Eyes_Worksheet`, `Hair_Worksheet` and `Beard_Worksheet` (`0x00748630`).

## A part's model name

`0x00749200` names a part's model from a row of a variation worksheet: `ModelType`, then `_` and `ModelSubType` if it has one, then `ModelVariation`, all after a two-letter prefix `%hc%hc_`, the first letter of `apr_base`'s `ModelRace` and the first of `ModelGenderOverride` if the row has one, else the gender's letter. `hm_arm_nuda` is a human male's bare torso; the level of detail's `_0` follows when the model is loaded. An item's worksheet is `bitm_base`'s `Variation_Worksheet` of its base type and the row its variation (`0x00748550`).

How a creature is built depends on the first letter of `apr_base`'s `ModelType` (`0x007490d0`, `0x006c0de0`): `p` is a humanoid built from parts, as below; the others (`h`, `s`, `w`) take `apr_base`'s `ModelName` whole.

## The bare body

An empty equipment slot (no base type, or no variation) draws row N of data table 80, `naked_variation.gda`, N being the slot (`0x00748550`, `0x0074ab40`): row 0 `arm`/`nud`/`a`, row 1 `glv`/`nud`/`a`, row 2 `boo`/`nud`/`a`. So a human male with no chest, gloves or boots is drawn with `hm_arm_nuda`, `hm_glv_nuda` and `hm_boo_nuda` (`_0.mmh` and its lower levels). The table has no rows past 2.

## The body's parts

`0x0074b7e0` loads the body: chest, gloves and boots, slots 0 to 2, each its own model. The chest's base type cuts that short, by its ID in the exe, not by a column:

- base type 38, Clothing (`0x007480b0`): the chest alone, no gloves and no boots model;
- base type 22, Armor - Massive - no boots (`0x007480c0`): chest and gloves, no boots.

Besides the models' names, the builder keeps a record per part (0xbc bytes, `0x00749e70`) holding the part's node name (`0x00748120`) and two of the slot's item values (`0x00749be0`), its material and tint by the look of it (not read). Slots 4 and 5 get such a record and no model; the helmet's model comes with the head (below). The cloak (6) is loaded by neither builder. Weapons and shields are not parts but attachments (below).

The chest part's material node is one of `ArmorM1`, `RobeM1`, `ClothesM1`: to drop the chest the engine tries the three in turn (`0x006a1090`), where any other slot is named directly.

## The head's parts, and what a helmet hides

`0x0074b010` loads the head, for a `p` appearance:

1. The neck: `apr_base`'s `Neck_Model` after the `%hc%hc_` prefix, if the row has one.
2. With a helmet in slot 5, the helmet's model, and its worksheet row's `FullFace` column, kept at `CAppearance+0x5e`.
3. With `FullFace` set: only the eyes, from a model named with `2` before its last letter (`cn_eye_glka` becomes `cn_eye_glk2a`). No face, hair, beard or lashes.
4. Otherwise: eyes, face (its morph applied), beard, hair unless there is a helmet, and lashes.

So any helmet hides the hair; a `FullFace` helmet hides the whole head but the eyes, which it swaps for the variant made to sit inside it. This is the exe's logic over one data column. The game's option to hide helmets was not traced.

## Where a weapon hangs

A weapon or shield is attached to a node of the body by `0x00a66b70`, for slot 3 or 4. The node is the `AttachNode` of a row of `weaponparts.gda` (table 91); the row comes from the base type's `bitm_base` columns:

| | Right hand (slot 3) | Left hand (slot 4) |
|---|---|---|
| Sheathed | `WearR` | `WearL` |
| Drawn | `WieldR` | `WieldL` |

An item equipped is first hung sheathed (`0x006a6940`). It is moved by the animation events `EquipR`, `EquipL`, `UnequipR`, `UnequipL` (`events.gda`, its `EquipEvent` and `RightHand` columns, `0x006a64b0`): an equip event draws to `Wield`, an unequip sheathes to `Wear`, `RightHand` picks the slot.

`weaponparts.gda` (the number is not the node's: 5 is `Prop10`):

| Row | Label | Node | Offset |
|---|---|---|---|
| 3 | mid upper left back | Prop17 | 1 |
| 4 | left hip | Prop05 | 0 |
| 5 | upper right back | Prop10 | 1 |
| 6 | mid upper right back | Prop09 | 1 |
| 7 | right hip | Prop12 | 0 |
| 8 | mid right back | Prop11 | 1 |
| 9 | lower right back | Prop06 | 1 |
| 10 | upper left back | Prop07 | 1 |
| 11 | lower left back | Prop08 | 1 |
| 14 | right hand | Prop01 | 0 |
| 15 | left hand | Prop02 | 0 |
| 16 | left lower arm | Prop03 | 0 |

Rows 0 to 2, 12 and 13 have no node.

Where the row's `WeaponOffset` is set (the back), the weapon is moved off the body, so it sits on the armour and not in it. The distance is the sum of three `WeaponOffset` columns: `apr_base`'s for the creature (times 0.2 for a female), `bitm_base`'s for the chest's base type (0.02 if it has none) and `bitm_base`'s for the weapon's. If the sum is not within 1e-6 of zero, the attachment is translated by it along the creature's +Y axis, taken into the node's frame: the model root's orientation turns (0, 1, 0), the node's orientation inverted turns that, and the result is scaled by the sum. The translation is the attachment's only change: no rotation is added. Clothing's own `WeaponOffset` is 0.02.

## The face at rest

Not settled. What was read:

- A head's morph (`.mor`) moves face bones by offsets of a few millimetres (`hm_genfl_alistair.mor`: `cheek_left` by (0.25, 0.29, 2.08) mm). It is not what makes the mouth.
- Heads are driven by FaceFX: one actor per race and gender (`humanmale.fxa`, `humanfemale.fxa`, `dwarfmale.fxa`, ... in `face.erf`). The actor's bones are named `default_<bone>`; `FacialAnimationBlueprint` (`0x0084d4f0`) strips the `default_` to find the model's bone. The actor holds a reference pose for its 38 bones, in centimetres with w-first quaternions, in a frame and parentage that do not compare directly with the `.mmh`'s rest.
- `default.hm_exp.ani` and `default.hf_exp.ani` (in `global.rim`) are 0.13 s clips keying the whole skeleton, every face bone among it. Nothing in the exe names them; how they are reached was not read.

Next: whether the facial player writes FaceFX's output over a bone's transform or adds it to the rest, read where `FacialAnimationPlayer` sets the bones; or a face bone's transform read in play at idle.
