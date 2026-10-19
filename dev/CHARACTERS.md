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

1. The neck: `apr_base`'s `Neck_Model` after the `%hc%hc_` prefix, if the row has one. Nothing else guards it: no gore, helmet or `FullFace` check, and its mesh (`hm_hed_dcpa_0.mmh`, `HM_HED_DCP_NeckM1`) has `MMHMeshDefaultHidden` and `MMHMeshCutAway` 0, the first read at `0x005950fa`. So it is always drawn: a short cylinder at the neck, z 1.52 to 1.65 m, skinned to the head's bones, with `Face.mat` (`uh_hed_dcp.mao`), whose tint mask covers both its cut cap and its band, so it takes the skin tint like the face. The band sits under the head and the collar and the cap inside the head; the cap shows where the head is gone, a decapitation not read. Read by lib-agent-2, 2026-10-17; not seen in play whether the face covers the cap whole.
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

The engine turns the weapon no further. AttachWeaponToNode (`0x00a66b70`) calls the weapon object's attach (`GraphicsStructures::Object`, `0x0054d860`) with the model, the node, its name, the WeaponOffset vector and no rotation (param_5 = 0). The attach builds an `ObjectAttachDependency`: the offset is stored as a translation (+0x30, `0x00542ea0`) and the weapon is parented to the node. So the weapon's root (GOB, identity in `w_lsw_ls01a_0.mmh`, the blade along +Z) = the prop node's world × translate(offset). The prop's frame is its whole chain: `Prop10` is a child of `Prop15` (rotation about 172°) under `Chest1`. At rest, and with `mh_m.p.ani`'s spine keys (the clip keys neither prop), `Prop10`'s +Z points down and about 9° to the left, from the upper right back at (0.17, −0.18, 1.71). Read by lib-agent-2, 2026-10-17. Seen in play the same day: turn-based-agent ran lib-agent-2's read-only scratch reader of the scene nodes, with two `default_hm_noble` idle in the Dragonbone Wastes, each with `gen_im_wep_mel_lsw_lsw` sheathed. Each weapon's GOB world matrix equals its owner's `Prop10` to 0.0° and 0.1°, and it sits 0.0196 m along the creature's model-space +Y (`Prop10`'s −X), the WeaponOffset push. In model space, with the idle playing, `Prop10` was at (0.161, −0.178, 1.577), +Z (−0.166, −0.059, −0.984), +Y (−0.977, −0.128, 0.172). On those nodes, +0x60 is the node's world matrix, +0xA0 its model-space matrix, and +0x20 the model's world matrix, the same layout as the bones.

## A bone's transform, and the face at rest

`GraphicsStructures::ObjectNode` builds a node's local matrix (`0x0055e9d0`) from three translations added and two rotations composed: translation = rest (+0x100) + animation (+0x120) + FaceFX (+0x130); rotation = FaceFX (+0xF0) composed with the animated rotation (+0xE0); scale +0x110. A pose reset (`0x0055e160`) zeroes +0x120 and +0x130, sets +0xF0 to identity and puts the rest rotation back in +0xE0.

- Rest and morph (`0x0055ec80`): the rest is the model's translation and rotation; a head morph's node of the same name has its `Position` added to the rest translation. A morph's bone positions are offsets of a few millimetres (`hm_genfl_alistair.mor`: `outerUpperLip_left` (-0.3, 2.4, 3.0) mm).
- The bind includes the morph. Applying a morph (`GraphicsStructures::Object`, `0x0054eb20`) calls each node's `0x0055ec80` (+0x100 = the blueprint's rest + the morph's `Position`; +0xE0 = the blueprint's rest rotation), then the root's bind pass with an identity parent. A bone's bind pass (`ObjectNodeBone`, `0x005b5db0`) builds bind = parent × matrix(+0xE0, +0x100), stores its inverse at +0x180 (`0x004d3830`), and recurses into the children with the bind. Each frame (`0x005b5d40`), after the node is composed into +0xA0 (model space), the skin matrix +0x1C0 = +0xA0 × +0x180. At rest the skin is identity, so a morph's bone offsets move no vertex; the morph's own vertices reshape the head. In the same in-game read, +0x20 is the model's world matrix and +0x60 the bone's world matrix (+0x20 × +0xA0). DAOParagonEngine af025e5 fixed Morrigan's eyes by putting the offsets in its bind. Read by lib-agent-2, 2026-10-17.
- Animation: a clip's rotation keys replace the rest rotation; its translation keys are added to the rest translation. `cs_crowd.idle01.ani`'s `root_translation` at 0 s is (0, -0.006892, -0.0134) against `hm_arm_nuda_0.mmh`'s Root at (0, 0.006892, 1.0266): the y cancels exactly. `default.hm_exp.ani` keys every lip translation (0, 0, 0) at 0 s.
- FaceFX (`FacialAnimationPlayer`, `0x0084f550`, through the node's face setter `0x0055e000`): heads are driven by an actor per race and gender (`humanmale.fxa` and kin in `face.erf`), whose bones are named `default_<bone>` (`FacialAnimationBlueprint`, `0x0084d4f0`, strips the prefix). The engine hands the node the actor's output less the actor's reference pose: position (current - reference) / 100 (centimetres to metres, `0x00aed110`) with y and z swapped, and the turn from the reference to the current. At its reference FaceFX adds nothing. Stopping an animation puts each bone's current back to its reference (`0x0084ec40`).

At rest nothing moves the lips off the head model's rest, seen in play 2026-10-17. turn-based-agent ran a read-only reader in its game, GOG, Witch Hunt, the Dragonbone Wastes: `ObjectNodeBone`s found by their vftables (0x00AFE40C), named from their blueprint (+0x174, whose name is a std::wstring at +0xC0), and sampled 30 times over 3 s, unpaused and paused. The reader was lib-agent-2's scratch script, not in the repo. Finn, a human male at idle, is one model: its +0x20 matrix is at his position, and it has 21 face bones. On every lip, jaw, mouth, cheek and Sneer bone, +0xE0 is the rest rotation, +0x120 is zero, FaceFX's +0xF0 and +0x130 are identity and zero, and none of them changed. Only Head turned, 8.7°, by the look overlay. So FaceFX's graph adds nothing at idle. The face bones' rest (+0x100) is the head model's: within 0-6 mm of `hm_hed_dcpa_0.mmh`, his morph, but up to 34 mm from the same bones in `hm_arm_nuda_0.mmh` (jawBone 33.7 mm, mouthBase 30.9, headBase 27.6, cheeks 23.8, outerUpperLip 23.0). The body model's copy of the face bones is not the one the head is posed by.

`default.hm_exp.ani` (`global.rim`) is the rest at 0 s and four expressions after. It is `humanoid.anb`'s node `portrait_exp_m` (`portrait_exp_f` is `default.hf_exp.ani`), played by the party portraits. Their models sit at the world's origin, and in the same read they held upperLip and LipCorner turned 30-36°, with offsets of 2-9 mm.

## Skin on a body part: the tint mask's alpha (read 2026-10-18 by lib-agent-2)

Read 2026-10-18 in the GOG exe and vanilla's files (shaders through `fxc /dumpbin`) by lib-agent-2 (with subagents), for DAOParagonEngine's side-by-side of the Harrowing's Fade. Not seen in play. The key claims were checked again in the listings, the files or the disassembly.

- **The material.** A robe with bare hands (`hm_rob_appa_0.mmh`, `RobeM1`, `pm_rob_appa.mao`) is `Character.mat`'s `ArmourSkinTint` (`Ch1ArmTnt.psh`) with `mml_tTintMask`. The mask's **R, G and B are the three armour tints, and A is the skin**: the hands are about 0.77 alpha with no colour. Robes without skin use `NoTint`.
- **`mml_vTintMaskColours`**, 10 float4s in the shader's order: [0] diffuse opacity per channel, [1] specular opacity, [2]/[3] R's diffuse/specular, [4]/[5] G's, [6]/[7] B's, [8]/[9] A's (the skin). It is set by name on each node's material (slot 41, 0x00A604A0; the name at 0x00B107A4, pushed at 0x00A6057E).
- **The shader** (`Ch1ArmTnt`, c12..c20 = C[0..8]):
  ```
  o = mask * C[0]
  base   = diffuse.rgb * lerp(1, C[8].rgb, o.a)            // the skin multiplies
  tint   = C[2]*o.r + C[4]*o.g + C[6]*o.b
  albedo = lerp(base, tint, o.r + o.g + o.b)
  s = mask.rgb * C[1].rgb; spec = lerp(specMask, C[3]*s.r + C[5]*s.g + C[7]*s.b, s.r + s.g + s.b)
  ```
  `LerpedTint` (`Ch1.psh`) treats A as the others (a dp4: the skin replaces); `MultiplyTint` (`Ch1MulTnt`) multiplies the skin the same way.
- **A `.tnt`** (GFF4, fields 14000–14009, `TintFile::Load` 0x00749600): diffuse and specular opacity, and DiffuseA/R/G/B, SpecularA/R/G/B. A skin `.tnt` (e.g. `t1_skn_001`) fills the A slot only.
- **Where it comes from.**
  - The builder (0x006BFCD0, from 0x006C0DE0) makes `body_tints` (88), then `skin_tints` (85)'s A slot on top, from the appearance's fields. Every part's record gets the item's own tint (`materialtypes` 89, then `tint_override` 87), then the skin's A-slot values copied in (0x007494D0 → 0x007488F0).
  - With a head morph (0x006BD700, after), the morph's skin `.tnt` (its first tint) goes into the lips, eyes and hair, and into the chest's, gloves' and boots' materials' A slots (0x006B7400 → 0x006AB2A0; for the chest, `ArmorM1`, `RobeM1` or `ClothesM1`).
  - So the player and named characters' hands take the morph's skin; a creature with no morph takes `skin_tints`' row.

## Neck_Model for a morphed head (read 2026-10-18 by lib-agent-2)

### Short answer
Read in the exe: nothing in the head's assembly consults the morph's parts list for the neck. `CAppearance::HeadModels` (0x0074B010, ID 17834) takes the neck from apr_base `Neck_Model` (column hash 0xdc9c928d, checked with gda.column_hash) and puts it in slot 4 of the model-name list, with no morph test. So an empty MorphParts[4] does not mean "no neck loaded". CHARACTERS.md's "always drawn" is right as to loading and attaching, but I did not trace the draw itself. I found no flag, cut-away or material that hides it for a morph. The peer's screenshot evidence is not explained by anything I read in the exe, so look elsewhere (below).

### Evidence
- Checked: the caller `BuildAppearanceModel` (0x006C0DE0, ID 14965) switches on `0x007490D0` (appearance kind). Kinds 4 (0x6C10E5) and 8 (0x6C11BD) call HeadModels(list, 1, 1) at 0x6C10F0 and 0x6C11C8, with the third argument (flag) set. With the flag set, HeadModels resizes the list to 6 (0x74B068, `0x616f20(6)`) and writes the neck with `vector<wstring>::at(4)` (0x74B250, `0x4560f0`). Part indices are 0 Face, 1 Eyes, 2 Hair, 3 Beard, 4 Neck, 5 Lashes.
- Checked: the neck block (0x74B0A4 to 0x74B273) is skipped only if the Neck_Model string is empty (`jbe 0x74b2ae` at 0x74B0DA). HeadModels has no call to `0x74A7C0` (the morph-handle getter, this+0x1E4, loaded from the name at this+0x1C8). Its only caller is inside `0x74AB40`.
- Checked: `0x74AB40` (part name builder) is the only place that reads the morph's parts list. In mode 1 (morph) it takes MorphParts[idx] if idx is below the list's count (list at morph +0x90, 0x1C-byte strings), else falls back to the worksheet. HeadModels calls it for eyes, face, hair, beard and lashes. It is never called for the neck. The `.mor` list is a replacement for the worksheet choice of those five parts only.
- Checked: after the model is built, `0x006BFCD0` (called from 0x6C1282 and 0x6C0546) tests `0x74A7C0(this,1)` for a morph resource. With one, it calls `0x6BD700` (the morph path). Without one, it sets the parts' values by hand in-line. `0x6BD700` applies tints (12 strings at morph +0xA0) and then visits parts 0, 1, 2, 3, 5, 4 by name (`0x748160`, which copies the node name from the table at 0xC36DE0, 28 bytes per name). It finishes with `0x6B7400` using the NeckM1 slot's value. `0x6B7400` loops `0x6A1090` over three nodes (the chest-node trio) and sets parameter 0xC3549C on them. This looks like copying the neck's tint onto the chest. It does not remove a mesh (read only roughly; not traced to the end).
- Checked in data: `hm_pcc_b01.mor` (face.erf) has MorphNodes BeardM1-3, HairM1-3, EyesM1, FaceM1 and LashesM1. There is no NeckM1 node. So the morph cannot move or hide the neck's vertices. MorphParts is a string list that ghilana does not read yet. The CHARGEN note says slot 4 is empty in `.mop` and `.mor`.

### Where else to look (inferred)
- The neck may be drawn and covered. The face and hair meshes hide the cap, and `Face.mat` has a tint mask. The peer's screenshot may differ because of tint or age-blend params applied to FaceM1 only (the morph sets `mml_vTintMaskColours` on FaceM1 but not NeckM1) or because of the camera.
- The peer's engine may be skipping the neck because it treats the missing MorphParts[4] as a rule. The exe has no such rule that I could find.

### Not read
- `0x006A1180`, the loader of the list: I did not confirm it loads every non-empty slot, slot 4 included. I assumed it does. It does not appear to filter by slot.
- Whether `MMHMeshDefaultHidden` is honoured differently on a morphed head's model (this was the 0x5950FA check; not re-traced).
- The AnimBase `+0x18` method (0xA5D070) beyond its first lines: it seems to be a per-node parameter setter by dotted name.
- The MorphParts list of a player `.mor`. The reader returns it as unread. I did not decode it by hand, so slot 4's emptiness is taken from CHARGEN.md, not re-read.
- Not seen in play.
