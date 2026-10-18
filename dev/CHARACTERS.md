# A character's model, assembled from its parts

How the engine builds a humanoid's model from its appearance and equipment: which parts it loads, what an empty slot draws, what hides what, and where a weapon hangs. Read 2026-10-17 in the GOG exe (Ghidra, `tools\daoexe.py`) and the game's data tables (ghilana), for DAOParagonEngine's character resolver and the core's, `dao/core/Character.h` (DAOLib #161), which follows this note. A template's equipment list numbers each item by its entry's struct id, the scripts' `INVENTORY_SLOT_*` (`gen00fl_alistair.utc`: 1 the shield, 0 the longsword, 6 the boots, 4 the chest, 7 the gloves, 9 a ring). A part's prefix is `%hc%hc_`, the two letters and an underscore; a weapon's or shield's model is `w_` and its variation's name with no prefix (`0x006a6940`): Alistair's longsword `w_lsw_ls03a`, sheathed on `Prop10` and drawn to `Prop01`, his kite shield (its `WearL` 8, `WieldL` 16) sheathed on `Prop11`, the mid right back, and drawn to `Prop03`, the left forearm. Addresses are GOG's; each has an ID in the address library.

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

## A bone's transform, and the face at rest

`GraphicsStructures::ObjectNode` builds a node's local matrix (`0x0055e9d0`) from three translations added and two rotations composed: translation = rest (+0x100) + animation (+0x120) + FaceFX (+0x130); rotation = FaceFX (+0xF0) composed with the animated rotation (+0xE0); scale +0x110. A pose reset (`0x0055e160`) zeroes +0x120 and +0x130, sets +0xF0 to identity and puts the rest rotation back in +0xE0.

- Rest and morph (`0x0055ec80`): the rest is the model's translation and rotation; a head morph's node of the same name has its `Position` added to the rest translation. A morph's bone positions are offsets of a few millimetres (`hm_genfl_alistair.mor`: `outerUpperLip_left` (-0.3, 2.4, 3.0) mm).
- The bind includes the morph. Applying a morph (`GraphicsStructures::Object`, `0x0054eb20`) calls each node's `0x0055ec80` (+0x100 = the blueprint's rest + the morph's `Position`; +0xE0 = the blueprint's rest rotation), then the root's bind pass with an identity parent. A bone's bind pass (`ObjectNodeBone`, `0x005b5db0`) builds bind = parent × matrix(+0xE0, +0x100), stores its inverse at +0x180 (`0x004d3830`), and recurses into the children with the bind. Each frame (`0x005b5d40`), after the node is composed into +0xA0 (model space), the skin matrix +0x1C0 = +0xA0 × +0x180. At rest the skin is identity, so a morph's bone offsets move no vertex; the morph's own vertices reshape the head. In the same in-game read, +0x20 is the model's world matrix and +0x60 the bone's world matrix (+0x20 × +0xA0). DAOParagonEngine af025e5 fixed Morrigan's eyes by putting the offsets in its bind. Read by lib-agent-2, 2026-10-17.
- Animation: a clip's rotation keys replace the rest rotation; its translation keys are added to the rest translation. `cs_crowd.idle01.ani`'s `root_translation` at 0 s is (0, -0.006892, -0.0134) against `hm_arm_nuda_0.mmh`'s Root at (0, 0.006892, 1.0266): the y cancels exactly. `default.hm_exp.ani` keys every lip translation (0, 0, 0) at 0 s.
- FaceFX (`FacialAnimationPlayer`, `0x0084f550`, through the node's face setter `0x0055e000`): heads are driven by an actor per race and gender (`humanmale.fxa` and kin in `face.erf`), whose bones are named `default_<bone>` (`FacialAnimationBlueprint`, `0x0084d4f0`, strips the prefix). The engine hands the node the actor's output less the actor's reference pose: position (current - reference) / 100 (centimetres to metres, `0x00aed110`) with y and z swapped, and the turn from the reference to the current. At its reference FaceFX adds nothing. Stopping an animation puts each bone's current back to its reference (`0x0084ec40`).

At rest nothing moves the lips off the head model's rest, seen in play 2026-10-17. turn-based-agent ran a read-only reader in its game, GOG, Witch Hunt, the Dragonbone Wastes: `ObjectNodeBone`s found by their vftables (0x00AFE40C), named from their blueprint (+0x174, whose name is a std::wstring at +0xC0), and sampled 30 times over 3 s, unpaused and paused. The reader was lib-agent-2's scratch script, not in the repo. Finn, a human male at idle, is one model: its +0x20 matrix is at his position, and it has 21 face bones. On every lip, jaw, mouth, cheek and Sneer bone, +0xE0 is the rest rotation, +0x120 is zero, FaceFX's +0xF0 and +0x130 are identity and zero, and none of them changed. Only Head turned, 8.7°, by the look overlay. So FaceFX's graph adds nothing at idle. The face bones' rest (+0x100) is the head model's: within 0-6 mm of `hm_hed_dcpa_0.mmh`, his morph, but up to 34 mm from the same bones in `hm_arm_nuda_0.mmh` (jawBone 33.7 mm, mouthBase 30.9, headBase 27.6, cheeks 23.8, outerUpperLip 23.0). The body model's copy of the face bones is not the one the head is posed by.

`default.hm_exp.ani` (`global.rim`) is the rest at 0 s and four expressions after. It is `humanoid.anb`'s node `portrait_exp_m` (`portrait_exp_f` is `default.hf_exp.ani`), played by the party portraits. Their models sit at the world's origin, and in the same read they held upperLip and LipCorner turned 30-36°, with offsets of 2-9 mm.
