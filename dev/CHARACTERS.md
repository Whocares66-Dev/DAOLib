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

How a creature is built depends on the first letter of `apr_base`'s `ModelType` (`0x007490d0` maps it to a kind, `0x006c0de0` switches on the kind): `p` is a humanoid built from parts, as below; `s` and `w` take `apr_base`'s `ModelName` whole; `h` is a hybrid, see the next section.

## H: a body model whole, a head of parts (read 2026-10-20, DAOLib #216)

The darkspawn (the genlock and hurlock rows of `apr_base` are `H`, as are 43 rows of the 91 in all; 10 are `P`) have a body of one model and a head of parts. The kinds `0x007490d0` returns, from `ModelType`'s first letter (column hash `0x5d092182`): `s` 1, `w` 2, `h` 4, `p` 8, any other 0. `0x006c0de0` switches on it, and the cases differ in what they ask for:

- Kind 8, `p` (`0x006c11bd`): `0x0074b010` (the head), then `0x0074b7e0` (chest, gloves, boots), then the models are loaded (`0x006a1180`).
- Kind 4, `h` (`0x006c10e5`): `0x0074b010` (the head) and the hair's and beard's (parts 2 and 3) models added, then one `0x0074ab40(out, 0xff, 0, 1)` for the body, then `0x006a1180`. `0x0074b7e0` is not called: no chest, gloves or boots part. `0x0074ab40` switches on the same first letter (jump table `0x0074afe8`, bytes `0x0074aff8`): for `h` with its mode argument 0 it answers `ModelName` (column hash `0xfd8403d5`) whole, with a mode of 1 (the head's parts) it goes by the worksheet and the template's PartList rows as for `p`. `0x0074b010` asks for the head's parts with mode 1, so an `h` creature's face, eyes, hair and beard are the `Head_Worksheet`, `Eyes_Worksheet`, `Hair_Worksheet` and `Beard_Worksheet` rows its template's `PartList` names, named `%hc%hc_` (`cn_` for a darkspawn: `ModelRace` C, `ModelGenderOverride` N) as a human's are, and the neck is `Neck_Model` (`cn_hed_dgla`).
- Kinds 1 and 2, `s` and `w` (`0x006c102d`): `0x0074ab40` for the body, `ModelName` whole, and no head.

The helmet is for `p` alone: `0x0074b010` tests the ModelType's first letter against `0x70` ('p', `0x0074b31b`) before it asks for the helmet's model, so an `h` creature draws none (its `FullFace` is never set). Armour on an `h` creature is stats only: the chest's, gloves' and boots' models are never asked for. Weapons are hung whatever the kind: `0x007e3980` (after the model is built, slots 3 and 4) and `0x006b1a52` (an item equipped) call `0x006a6940` with no look at the kind. So `pre200cr_genlock_archer.utc` (appearance 50, ModelName `CN_BDY_glka`, PartList rows 1 and 11) is the body `cn_bdy_glka`, a head of parts and its dagger on `Prop10`, and `pre200cr_hurlock_alpha.utc` (appearance 17) `cn_bdy_hlkb` and a head. Before this the core resolved every non-`p` creature as its ModelName alone, with no head and no weapons. Not seen in play.

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

1. The neck: `apr_base`'s `Neck_Model` after the `%hc%hc_` prefix, if the row has one. Nothing else guards it: no gore, helmet or `FullFace` check, and its mesh (`hm_hed_dcpa_0.mmh`, `HM_HED_DCP_NeckM1`) has `MMHMeshDefaultHidden` and `MMHMeshCutAway` 0, the first read at `0x005950fa`. So it is always loaded and attached, but **hidden** (corrected 2026-10-18 by lib-agent-2; this said always drawn): after every head build `0x006A6B90` (ID 14589, from `0x006C0424` in `0x006BFCD0`) sets node flag `0x400` on `NeckM1`..`NeckM9` and clears it on the other head parts while `CAppearance+0x5D` != 1, and the reverse when it is 1, the helmet's node with the head parts (`0x006A6C9C`–`0x006A6CC3`). `+0x5D` is set by animation event 28 (Decapitate, `0x006C0C81` → `0x006AAC20`) and saved as GFF `0x3FC5`. The neck is the decapitation stump: a short cylinder at the neck, z 1.52 to 1.65 m, skinned to the head's bones, with `Face.mat` (`uh_hed_dcp.mao`); `uh_hed_dcpa_0d.dds` is a gore cap and a bloodied strip, and the tint mask's alpha gives the strip the skin tint and leaves the cap red. The two neck sections below have the reading.
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

### A node the body lacks (read 2026-10-24 by lib-agent-2 with a subagent, for DAOParagonEngine)

Nothing was played. [C] read in the exe and the files, [I] inferred.

- **No node, no attach, and no fallback** [C]:
  - AttachWeaponToNode 0x00A66B70 reads the row's `AttachNode` (weaponparts, column 0x90028895) and looks the name up on the body model through its vtable +0x10 (0x00A66C32–0x00A66C41).
  - A null node (0x00A66C47) goes to 0x00A66E24, which frees the name and returns false (0x00A66E3B).
  - The detach of the old attachment (0x0054DAC0), the attach itself (vtable +0xF0, which builds the ObjectAttachDependency) and the slot's remembered row (+0x22C + slot × 4) all come after the node is found.
  - So nothing falls back: not the model root, not another Prop, not the Wield or Wear counterpart.
- **The callers carry on** [C]:
  - The first hang, 0x006A6940 (through 0x006A1350 at 0x006A6B0A), returns false. The equip routine (ID 14834, at 0x006B1A52) then only zeroes the slot's float at appearance +0x180 + slot × 4 (0x00747E50) and goes on.
  - The equip and unequip events' mover 0x006A64B0 ignores the result (0x006A65F9). So a missing Wield node leaves the weapon on its Wear node, or attached nowhere if that one was missing too.
  - That a weapon attached nowhere is not drawn is [I]: the weapon model's builder 0x00A672F0 and the other attach caller 0x007CFE5F were not read, and it was not seen in play.
- **Only a combat humanoid draws and sheathes** [C]:
  - The test is IsHumanoid 0x006A25A0 → 0x0069F030: apr_base's `CombatModelTree` (0xDE80B418) must be "combat_humanoid". ModelType (H or P) is not tested.
  - A creature that fails it plays `enter`/`exit` (anim_combat rows 100/101) and never moves a weapon (ANIMATION.md, "Drawing and sheathing weapons").
  - apr_base has no "always drawn" column. The template's WeaponsDrawn applies to humanoids only.
- **The revenant, worked through** [C, data]:
  - apr_base 26 has ModelType H but `CombatModelTree` c_reva_cbt.anb, so it is not a combat humanoid.
  - Its body (cn_bdy_reva_0, _2, _3) has only Prop01 (under RightWrist), Prop04, Prop19, Prop20, Prop21 and Prop22.
  - Its sword and shield are the body's own meshes: RightSpawn (bone 100, Mesh3, cn_lsw_reva) and LeftSpawn (bone 99, Mesh2, cn_shd_reva), always drawn.
  - Its own weapon, gen_im_cweapon_revenant.uti, is base item 23 (a creature weapon), whose Wear and Wield columns are all 0, so it asks for no node.
  - Any shield given to it (WearL Prop11, WieldL Prop03), or a sword or axe (Wear Prop10, or Prop07 for the left), finds no node and is not attached. A right-hand weapon's Wield node Prop01 exists, but it is reached only by an EquipR event, which a non-humanoid's combat state never queues [I for the listed items' base types, inferred from their names; their .uti files were not found].
- Where a node is missing, nothing is attached: the weapon is not hung at the drawn node or the root.

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
Read in the exe: nothing in the head's assembly consults the morph's parts list for the neck. `CAppearance::HeadModels` (0x0074B010, ID 17834) takes the neck from apr_base `Neck_Model` (column hash 0xdc9c928d, checked with gda.column_hash) and puts it in slot 4 of the model-name list, with no morph test. So an empty MorphParts[4] does not mean "no neck loaded". CHARACTERS.md's "always drawn" is right as to loading and attaching, but I did not trace the draw itself. I found no flag, cut-away or material that hides it for a morph. (Superseded 2026-10-18: the neck is hidden by node flag `0x400` until decapitation; see the neck2 sections below.) The peer's screenshot evidence is not explained by anything I read in the exe, so look elsewhere (below).

### Evidence
- Checked: the caller `BuildAppearanceModel` (0x006C0DE0, ID 14965) switches on `0x007490D0` (appearance kind). Kinds 4 (0x6C10E5) and 8 (0x6C11BD) call HeadModels(list, 1, 1) at 0x6C10F0 and 0x6C11C8, with the third argument (flag) set. With the flag set, HeadModels resizes the list to 6 (0x74B068, `0x616f20(6)`) and writes the neck with `vector<wstring>::at(4)` (0x74B250, `0x4560f0`). Part indices are 0 Face, 1 Eyes, 2 Hair, 3 Beard, 4 Neck, 5 Lashes.
- Checked: the neck block (0x74B0A4 to 0x74B273) is skipped only if the Neck_Model string is empty (`jbe 0x74b2ae` at 0x74B0DA). HeadModels has no call to `0x74A7C0` (the morph-handle getter, this+0x1E4, loaded from the name at this+0x1C8). Its only caller is inside `0x74AB40`.
- Checked: `0x74AB40` (part name builder) is the only place that reads the morph's parts list. In mode 1 (morph) it takes MorphParts[idx] if idx is below the list's count (list at morph +0x90, 0x1C-byte strings), else falls back to the worksheet. HeadModels calls it for eyes, face, hair, beard and lashes. It is never called for the neck. The `.mor` list is a replacement for the worksheet choice of those five parts only.
- Checked: after the model is built, `0x006BFCD0` (called from 0x6C1282 and 0x6C0546) tests `0x74A7C0(this,1)` for a morph resource. With one, it calls `0x6BD700` (the morph path). Without one, it sets the parts' values by hand in-line. `0x6BD700` applies tints (12 strings at morph +0xA0) and then visits parts 0, 1, 2, 3, 5, 4 by name (`0x748160`, which copies the node name from the table at 0xC36DE0, 28 bytes per name). It finishes with `0x6B7400` using the NeckM1 slot's value. `0x6B7400` loops `0x6A1090` over three nodes, `<chest>`, `GlovesM1` and `BootsM1`, and sets `mml_vTintMaskColours` (the string object at 0xC3549C) on them: the neck's tint block copied to the bare body (corrected 2026-10-18 by lib-agent-2, below).
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

## NeckM1: material side, and what hides it (neck2a) (lib-agent-2)

### Short answer
Vanilla HIDES NeckM1 by state, at run time. It is not a depth bias, a mask or a skin-coloured band. After every head assembly the engine calls `0x006A6B90`, which sets the scene-node flag bit `0x400` (hidden) on every `NeckM1..NeckM9` mesh node of the creature unless the creature's "headless" byte (`appearance+0x5D`) is 1. That byte becomes 1 on animation event 28 (Decapitate) and is saved (GFF field `0x3FC5`). While it is 1 the head parts (Face, Eyes, Hair, Beard, Lashes) get `0x400` instead and NeckM1 is shown: the stump. So for a normal creature the ring is never drawn, which is why the peer sees none of the "bloodied band". Paragon should not draw NeckM1 (and not tuck it); it should draw it only for a decapitated creature, with the head parts hidden. Also HelmetM1 is shown normally and hidden when headless (same function).

The material, for the record, is plain: same material, semantic, pass and render state as the face. Nothing in it would hide the ring.

### 1. What hides it (checked unless marked)
- `0x006A6B90(appearance, model)` (callers `0x006C0424`, `0x006AAC3E`). `appearance` = the head's appearance object (`creature+0x7B0`), `model` = `creature+0x70`'s `vt+0x60` result. A = `byte [appearance+0x5D]`.
  - For each part index p = 0..5 (loop at `0x006A6C06`, count 6): name = `0x00748160(p)` from the 6-name table `0xC36DE0` (FaceM1, EyesM1, HairM1, BeardM1, NeckM1, LashesM1; p=4 is NeckM1). For n = 1..9 (loop ends at `cmp esi,0xa`, `0x006A6CE8`): the name minus its last character plus n (`0x004C30F0` substr, `0x004C3510` append number), so `NeckM1`..`NeckM9`. The mesh node is found by `model->vt+0x70(name)`.
  - Flags at node `+0x24`: A == 0 (normal): p == 4 gets `|= 0x400`, every other part `&= ~0x400` (`0x006A6CA7`..`0x006A6CC3`, via `jne 0x6A6CB5`). A == 1: p == 4 gets `&= ~0x400`, every other part `|= 0x400`. If the value changed it calls `0x0053AD40(oldflags)`.
  - After the loop: `0x006A1090(appearance, model, 5)` finds the node by `0x00748120(5)` from the 7-name table `0xC36D18` (0 `<chest>`, 1 GlovesM1, 2 BootsM1, 3 `<main_weapon>`, 4 `<offhand_weapon>`, 5 HelmetM1, 6 CloakM1: names read from the static init `0x00AD1FF0`): A == 0 clears `0x400` (helmet shown), A == 1 sets it (helmet hidden) (`0x006A6D2E`..`0x006A6D5B`).
- Bit `0x400` means hidden. Checked: `0x0053AD40` treats bits `0x100`, `0x200`, `0x400` (mask `0x700`, `test esi,0x700` at `0x0053ADA5`) as reasons to switch the node's renderable off (visible = `!(flags & 0x700)`, `0xA1C71A`). Traversal tests skip a node with the bit set: `test dword [reg+0x24],0x400 / jne skip` at `0x00454C5B`, `0x00454DCE`, `0x0045CB16`, `0x0045CDCD`, `0x00454CF2`-area (I read those four tests; what each traversal is, I did not read: inferred render/pick/update walks).
- Called after every head build: `0x006BFCD0` (the head's material/tint setup) ends in a common tail `0x006C041B`: `0x006A13C0`, then `0x006A6B90`. Every branch (morph `0x006C01F3`, the worksheet branch, and the `0x006C03F6` branch) jumps or falls to that tail. Checked in disassembly.
- A default 0: the appearance constructor writes `[this+0x5D] = 0` at `0x0074A4B1`.
- A = 1 comes from: (a) `0x006AAC20` (`mov [eax+0x5d],1`, then `0x006A6B90` at `0x006AAC3E`), reached from `CCCreature::OnAnimationEvent` `0x006C0BF0` case id 28 (jump table `0x006C0D70`, byte table `0x006C0DB0`; I recomputed: id 28 -> `0x006C0C81` -> `0x006AAC20`). ANIMATION.md already named event 28 "Decapitate" and event 29 "DecapitateVFX" (`0x006A5C10`). The clips that carry it are the decapitating deathblow reacts (anim_base 327/330 and the dw/tw deathblow dies, per ANIMATION.md). (b) The save loader `0x0074A9E6`..`0x0074AA03`: if the appearance has GFF field `0x3FC5` (16325), `[this+0x5D] = (value != 0)`. Next to it `0x3FC4` is the gore float (`0x00747C40`) and `0x3FC6` another field (not read). So a decapitated creature stays decapitated across a save.
- `MMHMeshDefaultHidden` (`0x005950FA`) is a separate bool (`mesh+0x1AA`), 0 for NeckM1: it is not the mechanism. CHARACTERS.md's "always drawn" is wrong: the neck is always LOADED and attached, but hidden by `0x400` until A = 1.
- The other two 6-name tables the coordinator listed, `0xC4C2B8` (init `0x00AD4A40`) and `0xC4C488` (init `0x00AD4CA0`): checked, no reader. Their only references are the static initialiser, the SEH unwind funclets (`0x00ACD511..539`, `0x00ACD661..689`) and the vector-destructor registration (`0x00AD941A`, `0x00AD949A`, `push 0x1C / push 6`). They are per-translation-unit copies of the same header table; the only table with a reader is `0xC36DE0`, whose reader is `0x00748160` (callers: `0x006A6C26`, `0x006BD955`..`0x006BDBD5`, `0x006C001A`..`0x006C0391`, `0x006C0678`). The other references near them (`0xC4C360`, `0xC4C530`) are different global strings, not table ends.
- HeadModels (`0x0074B010`) puts Neck_Model in slot 4 with no morph test (earlier note), so loading is unconditional; only visibility depends on A.

### 2. Material (Face.mat), the neck and the face
All checked in the vanilla files (`materials.erf` face.mat.xml, `states.erf` face.sta.xml, `shaders.erf` face0.psh.fxo vanilla copy; DAOElgaraShaders overrides face0.psh.fxo, I read vanilla's, 8296 bytes, disassembled with d3dcompiler).
- Same material, semantic and state: `uh_hed_dcp.mao` and the face's `uh_hed_masa.mao` (FaceM1 of `hm_uhm_basa_0`, material object `uh_hed_masa`) both name `Face.mat`, DefaultSemantic `Default`. Semantic `Default` has technique LOD1 pass 1: `Face1.vsh` / `Face0.psh` / state `Face.sta` (LOD2/LOD3 semantics: `Char2.vsh`/`Face2.psh`, same `Face.sta`; the `_l2`/`_l3` mao files are 1021 bytes).
- `Face.sta`: ZEnable TRUE, ZFunc LESSEQUAL, ZWriteEnable TRUE, CullMode CULL_CW, AlphaBlendEnable FALSE, AlphaTestEnable FALSE, FillMode solid, stencil enabled (ALWAYS, pass INCRSAT). No DepthBias, no SlopeScaleDepthBias, no ColorWriteEnable entry, no ZFunc change. So the neck is drawn as an opaque, depth-written, depth-tested surface exactly like the face. Nothing hides it in state.
- Pixel shader `Face0.psh` (ps_3_0): no `texkill`, no `clip`; output alpha `oC0.w = mml_fOpacityMultiplier` (c47, default 1.0), `oC1` is a depth-ish output. No alpha test happens (state off), so alpha does nothing.
- Neck mao vs face mao: same texture slots, but the neck has `mml_tBrowStubble`/`Emotions*`= white defaults, no age maps (`mml_tAgeDiffuseMap`/`mml_tAgeNormalMap` default black/flat), tattoo mask `Default_Black`, `g_fRimPower` 0.00 (face 2.22), `g_fSpecularMult` 0.20 (face 0.68), `g_fLipSpecularBoost` 0.00 (face 0.35), `g_vAmbientMult` 1 1 1 0 (same). The neck's diffuse/normal/tint: `uh_hed_dcpa_0d/0n/0t.dds`. No "NeckBlend/HideNeck" value exists anywhere in Face.mat or its shader.
- Textures read (512x512): `uh_hed_dcpa_0d.dds` is one sheet: a round pink-red flesh/bone cross-section disc (the severed cap) filling the top, and a pale grey-white skin strip with faint speckles across the bottom (the band). Diffuse alpha is a low-contrast detail map. `uh_hed_dcpa_0t.dds` (tint mask): R, G, B are all black everywhere; only ALPHA carries data: white (full skin tint) on the band strip (with a darker smudge at its lower centre) and in the margins, dark/mottled on the disc interior (so the flesh stays red). In the shader the tint mask's alpha scales `mml_vTintMaskColours[8]` and `[9]` (c20, c21: skin diffuse and skin specular tint): `diffuse *= alpha*(c20.xyz-1)+1`. So the band takes the skin colour, the disc does not. (Reading of the shader: the tint-mask RGB use c12, c14, c16, c18 for the other tint zones; the neck mask has none.)
- Gore (the "blood" parameter): Face.mat declares `mml_fGoreThreshold` (Float, default 0) and texture `mml_tGoreMap` (sampler `smpGore`, a 3D volume, default `Gore_Default`), `mml_fOpacityMultiplier` (1.0), `mml_vTintColor` (0 0 0 0), `mml_vTintMaskColours` (10 x vec4, default: entries 0-1 = 0, entries 2-9 = 1), `mml_fAgeBlendWeight` (6 x vec4, 0), `mml_fEmotionsMapWeight` (2 x vec4, 0), `mml_vTattooParameters` (5 x vec4, 0), `g_vFacialHairWeights` (0). Shader: `g = clamp(min(mml_fGoreThreshold, 0.2), 0, 1)`; `t = tex3D(smpGore, (u, v, g)).x`; colour factor = `(g <= 0) ? 1 : (0.56, 0.09, 0) + t * (0.44, 0.91, 1.0)`, multiplied into the lit colour (`0x...` instructions around the `texld r2, r1.yzww, s8` of the listing). So gore 0 is exactly no effect; at most 0.2 of a 3D map in red.
- Who sets `mml_fGoreThreshold`: the wide string is at `0x00B0FEAC` ("mml_fGoreThreshold"), referenced by `0x006B1B28` (in `0x006B19B0`), `0x006B1C6D`, `0x006B1D4D`, `0x006B1E3D` (in `0x006B1BC0`) and `0x00A60F55`. `0x006B1BC0(obj, float)` sets it (parameter setter `0x004F3FE0` on each mesh of the model list `obj+0x18`->+4, vt+0x68) for the model's meshes and for item slots 3 and 4 (`0x00A66B40` handles only indices 3,4: `[this+idx*4+0x224]`); callers: `0x006B73C0` (the value is the appearance object's gore float, `0x00748520`, saved as GFF `0x3FC4`, set by SETCREATUREGORELEVEL / ActionSetGore), `0x007CFCEF`, `0x007CFD4B`, `0x007F45CD`. `0x006B19B0` (callers `0x006BDD7E`, `0x006C1D8B`) re-applies it per slot from `[this+0x7B0]`. Whether those meshes include NeckM1 I did not trace (inferred: the model-wide loop would, but NeckM1 is hidden anyway). Vanilla default gore is 0, so nothing is bloodied unless a script/ability sets a gore level.
- The 0xC3549C parameter: it is NOT a string at that address; `0xC3549C` is a global string object constructed at `0x00AD1450` (`push 0xB107A4` = wide "mml_vTintMaskColours"; `mov ecx,0xC3549C`). So `0x006B7400` (the neck slot's tint copy) sets the parameter `mml_vTintMaskColours` (checked), on the three nodes `0x006A1090(.., idx 0..2)` = `<chest>`, `GlovesM1`, `BootsM1` (checked from the 7-name table), with the value = the tint-set block (the same as the neck's) written into the vec4 array by `0x006AB2A0` (stores floats from the block, +0x28, +0x38, ... into the array entries' w components at 16-byte stride; I did not decode every entry). Purpose: the bare body gets the same skin tint as the head, not the neck. It runs only in the morph path.
- Tint setup, morph path `0x006BD700` (checked): the morph's 12 tint strings (at morph +0xA0) are parsed into 0xBC-byte blocks (`0x008714C0` loop); block 0 (`[esp+0x68]`) is copied by `0x007488F0` (copies +0x28, +0x38, vec at +0x3C and +0x7C through `0x00933D70`) into three working blocks: face `[esp+0x124]`, eyes `[esp+0x1E0]`, hair/beard/lashes `[esp+0x29C]`. Parts are applied in order 0 (FaceM1, with `0x006A0FA0` and `0x006AB170` adjustments), 1 (EyesM1, with `0x0069F6A0`), 2, 3, 5 (the `[esp+0x29C]` block), then 4 (NeckM1) with the unmodified block `[esp+0x68]` (`0x006BDBD5`..`0x006BDBEB`: `push 4; 0x00748160; vt+0x18(name, blob)`), then `0x006B7400` with the same block for the body. So NeckM1 gets the skin tint block (the same skin tint the face block starts from) via the node-name parameter setter (`vt+0x18` of the model object, `0x00A5D070`-class); face and neck get the same skin colour. (Which of the 12 strings is "block 0", and that it is the skin entry, is inferred from its position and from the copies.)
- Non-morph path (`0x006BFCD0` `0x006C00D5`..`0x006C0185`): parts 0, 1 (via `0x0069F6A0` at `[edi+0x6C]`), 2, 3, 5 get the tint blob (`vt+0x18(name, blob)`, blob from `vt+0x10`); part 4 (NeckM1) is NOT given a tint blob, and there is no chest copy. Checked: the push sequence is `ebx`(=0), eyes, 2, 3, 5, no 4. So in the worksheet path the neck keeps Face.mat's defaults, `mml_vTintMaskColours[8..9]` = 1, i.e. its diffuse is untinted (pale grey band, red disc). Only visible when A = 1.

### Conclusion for the peer
Not "drawn but skin-like": hidden by state. The band is invisible because the whole NeckM1 mesh has the hidden flag `0x400` unless the creature is decapitated (`appearance+0x5D == 1`, event 28 / save field `0x3FC5`). When decapitated the engine instead hides the head parts and the helmet, and draws the stump (disc + band), tinted with the skin block in the morph path and untinted in the worksheet path. Paragon: set hidden for NeckM1..M9 by default, show it (and hide FaceM1/EyesM1/HairM1/BeardM1/LashesM1/HelmetM1) on a decapitate event; remove the 8 mm tuck. Material/depth/alpha need nothing special.

### Not read
- The traversals around `0x00454C5B`/`0x00454DCE`/`0x0045CB16`/`0x0045CDCD` (what exactly skips a `0x400` node: draw, shadow, picking): inferred as render walks, plus `0x0053AD40` switching visibility off.
- What `0x00A1C71A` does exactly (visible flag on the node's listener); `0x00A5D070`/`vt+0x18` internals.
- Whether event 28's animation clips include every decapitating kill; which death or finishing move plays them in play (ANIMATION.md lists 327/330 and dw/tw deathblows).
- Whether the gore setters `0x006B1BC0`/`0x006B19B0` reach NeckM1 (moot while hidden), and the meaning of GFF `0x3FC6`.
- The entry-by-entry mapping of the tint block onto `mml_vTintMaskColours`; `SM2_Face0.psh` (the LOD2/LOD3 pixel shader `Face2.psh` is read: "A model's level of detail"); the shader of EA's/Steam's builds; nothing seen in play.
- Block index of the neck's tint (inferred 0).

## NeckM1 (hm_hed_dcpa_0): geometry, skinning and the draw (neck2b, 2026-10-18) (lib-agent-2)

### Short answer

Vanilla HIDES NeckM1 on a normal creature, at run time, and shows it only on decapitation. The file does not hide it (MMHMeshDefaultHidden 0), but the engine sets the scene-graph "hidden" bit 0x400 in the node's flags (+0x24) for part slot 4 (NeckM1) every time it builds the head's visibility, and clears it (while hiding FaceM1, EyesM1, HairM1, BeardM1, LashesM1 and the helmet node) only after the animation event 28 (Decapitate). So the ring's band never needs a depth bias, a draw order or an 8 mm tuck: it is not drawn. Paragon should not draw NeckM1 unless the creature is decapitated (checked, read in disassembly). "Always drawn" in master's CHARACTERS.md is wrong and should be fixed.

### 1. The run-time hide (checked in the exe)

- `0x006A6B90` (ID 14589, no name yet; callers `0x006C0424` and `0x006AAC3E`) takes (CAppearance* a, object* o). It reads the flag byte `a+0x5D` into a local and loops slot 0..5 of the head-part names (`0x00748160`, the table `0xC36DE0`: 0 FaceM1, 1 EyesM1, 2 HairM1, 3 BeardM1, 4 NeckM1, 5 LashesM1). For each it finds the node by name (vtable +0x70 of the object) and rewrites the node's flags at `+0x24` (bit 0x400), then calls `0x0053AD40(oldFlags)` if they changed.
  - `a+0x5D` != 1 (every normal creature): slots 0,1,2,3,5 get bit 0x400 cleared (`0x006A6CC3`, shown) and slot 4 gets it SET (`or eax,0x400` at `0x006A6CBC`, hidden). That is NeckM1 hidden.
  - `a+0x5D` == 1: slot 4 gets it cleared (shown, `0x006A6CA7` je `0x006A6CC3`) and every other slot gets it set (`0x006A6CAE`, hidden).
  - Afterwards it finds one more node with `0x006A1090(a, o, 5)` and does the same the other way round: shown when `a+0x5D` != 1 (`0x006A6D56`), hidden when 1 (`0x006A6D4F`). Read as the helmet/ head-gear node, not confirmed which.
- The flag is set to 1 by `0x006AAC20` (`mov eax,[ecx+0x7B0]; mov byte [eax+0x5D],1`, then calls `0x006A6B90`). It is `CCCreature::OnAnimationEvent` (`0x006C0BF0`, ID 14964) case id 28 at `0x006C0C81`: "28 -> 0x006AAC20 Decapitate" in ANIMATION.md, and the event is named Decapitate in AUDIO.md. A deathblow clip (dw_dblow_die_v3, "event 29 DecapitateVFX") and the decap sync-death rows (anim_base 327/330) are what raise it. The jump table `0x006C0D70` and the byte table `0x006C0DB0` were decoded here: message 28 -> `0x006C0C81`.
- The normal call: `0x006C0424`, in `0x006BFCD0` (the function that applies the head's parts, switch on the appearance kind at `0x006C0498`), straight after `0x006A13C0`, with `a+0x5D` still 0: so NeckM1 is hidden right after the head is built. (checked that the call is there; not traced which of the switch's cases reach that tail. The morph path `0x006BD700` and `0x006B7400` run before it in the same function, so they do not undo it.)
- What 0x400 means: the scene-graph traversals skip an object whose `+0x24` has 0x400: `0x00454C5B` (in the function at ID 2290, `test [eax+0x24],0x400; jne skip`), `0x0045CB16` (ID 2495), also `0x00454DCE`, `0x0045CDCD`, `0x0045CEB2`. Read two of them to the end; the other three only by the same `test`. (checked for the pattern; "hidden" is the inferred name.)
- Other setters of the same bit (not traced, likely generic hide/show or weapon sheath): `0x0053F090`/`0x0053F132`, `0x0057D0E1`, `0x006A0651/0x006A0661`, `0x006A7338`.
- MMHMeshDefaultHidden: the blueprint loader writes label 0x18CA (6346) to blueprint `+0x1AA` (`0x00595108`; the `ObjectNodeMeshBlueprint` vtable `0x00AFA65C`, slot 10 `0x0057CF00` is its getter) and the copy to the runtime mesh record `+0x15E` (`0x005F3F6F`; records are 0x2A4 bytes). The mesh draw/collect code tests `[rec+0x15E]` then `[rec+0x15F]`: skipped when DefaultHidden is set and the +0x15F "shown" flag is not (`0x005EF64A..0x005EF6A0`, `0x005F048E..0x005F04B9`, `0x005F5DAF..0x005F5DED`, `0x0066D823..0x0066D865`). NeckM1 has DefaultHidden 0, so this path never hides it; the 0x400 flag does. (checked)
- Depth bias / ZFUNC / mesh sort: not searched, because the mesh is not drawn in the normal case; a stump (decapitated) draws with the head parts hidden, so there is no face to tie with.

### 2. The files (checked, read with tools/gff4.py and a Python reader of my own over the msh)

- `hm_hed_dcpa_0.mmh` (the patch002.erf copy, 39,226 bytes, same size in chargen.rim and modelhierarchies.erf) has ONE mesh node, NeckM1: material object `uh_hed_dcp` (`Face.mat`), group `HM_HED_DCP_NeckM1`, bones used [13 Head, 61 Neck1, 60 Neck, 7 Chest2, 19 jawBone], DefaultHidden 0, CutAway 0, PunchThrough 0, UseVariationTint 0, receive shadows 0. The rest of the file is the skeleton (104 bones, Head, headBase, face bones). Its msh `hm_hed_dcp_0.msh` is one chunk: 934 vertices, 1824 indices (608 triangles), 52 bytes a vertex (Float16_4 position, Float16_2 uv, tangent, binormal, normal, blend weights, Short4 indices).
- The head the ring goes with is `hm_uhm_basa_0.mmh` (FaceM1, material `uh_hed_masa`, 34 bones used, which contain all five of the ring's) over `hm_uhm_bas_0.msh` (2,282 vertices, z 1.526 to 1.817). The face mesh itself runs down into the neck and trapezius (it has x +/-90 mm at z 1.57), as the ring does (+/-95 mm).
- Radius against the face, horizontal slices z 1.57..1.62 about the ring's axis: the ring is 3 to 6 mm OUTSIDE the face at the back and sides (for example z 1.59 back 75 vs 72 mm, sides 94 vs 89; z 1.60 sides 97 vs 92) and 4 to 14 mm INSIDE it at the front (z 1.60 front 57-70 vs 67-74). So the peer's 0-5 mm outward gap is confirmed at the back and sides; at the front the face is outside the ring. The band is only the lower 149 vertices (z 1.521 to 1.626); 785 of the 934 vertices (z 1.626 to 1.646, x +/-64 mm, y -54 to +85 mm) are the stump's cap.
- The cap lies inside the head: z 1.63 to 1.645 is below the chin's rear and inside the face's volume (the face has vertices to y 137 mm at those heights); with the head gone, it caps the cut where the band ends. Read from bounds only, not ray-tested against the face triangles.
- Skinning: same bones in the same roles. At z 1.60 to 1.626 the ring's weights are Head 0.45, Neck1 0.19, Chest2 0.17, Neck 0.14, jaw 0.05; the face's are Head 0.37, jaw 0.32, Chest2 0.14, Neck1 0.11, Neck 0.06. Nearest-vertex pairs in the upper band match to within 0.05 (for example Head 0.55 / Neck1 0.30 / Neck 0.15 against Head 0.51 / Neck1 0.31 / Neck 0.18). The lower band (z 1.52 to 1.60) differs: the ring blends Neck 0.15-0.38, Head 0.07-0.19, Neck1 0.05-0.12 and Chest2 0.37-1.0, where the face is 100% Chest2 there. So the ring's lower rim follows the neck bones where the face follows Chest2: in a pose the two would separate a few mm, which is one more reason the engine does not draw both. The cap is weighted Head 0.69, Neck1 0.15, Neck 0.08, jaw 0.08 (it moves with the head).
- The body's collar: `hm_arm_nud_0.msh` (naked chest, mesh ArmorM1) has its top at z 1.6045; at z 1.58 its crossing is x +/-101 against the ring's +/-94 at the sides and at the back 70-81 against the ring's 77. So the body does lie at or outside the ring below about z 1.59, but from z 1.59 to 1.62 at the back nothing of the body is there (its top is lower): had the ring been drawn, a 3-4 mm band of it would have been exposed at the back of the neck. (rough: horizontal slices, no armour chest model compared.)
- Textures: the ring's material `uh_hed_dcp.mao` is `Face.mat` with `uh_hed_dcpa_0d` (diffuse; the cap is pink flesh around (165,107,95), the band strip v 0.82 to 0.99 a skin beige (150-164,128-147,107-126)), `uh_hed_dcpa_0t` (tint mask; RGB 0 and only the alpha carries values: 250 on the band, about 157 on the cap), `uh_hed_dcpa_0n`, specular multiplier 0.2, rim power 0, and Default_white / Default_black for the stubble, emotion and tattoo maps. The band's uv: v 0.82 to 0.99 for z 1.52 to 1.626; the cap v 0.01 to 0.86. This is the decapitation stump's look. As the diffuse is a light skin strip, the peer's "bloodied band" probably means the cap's gore reaching the band by its uv or by a missing skin tint.

### Conclusion

Which of the peer's options: none of depth bias, draw order, skinning or alpha. The ring is HIDDEN by the engine, by node flag 0x400 on part slot 4, set at every head build and by `0x006A6B90` whenever the creature's `+0x5D` flag is 0, and cleared only by the Decapitate animation event (28 -> `0x006AAC20`, which also hides the face, eyes, hair, beard and lashes). Where it is shown it is the stump: ring plus cap closing the cut, with nothing of the head over it. For Paragon: do not draw NeckM1 (and drop the 8 mm tuck) unless the creature is decapitated; on a decapitation hide the five head parts and show NeckM1 (and probably the helmet node).

### Not read

- Which switch cases of `0x006BFCD0` reach the tail with the call at `0x006C0424` (human/elf/dwarf/qunari `p` appearances are the case that matters; kinds 4 and 8 are the ones that call HeadModels).
- What `0x006A1090(a, o, 5)`'s node is exactly (helmet?) and what `0x006A13C0` does; what the other 0x400 setters do.
- The scene graph's use of 0x400 beyond the `test ... jne` pattern at three of the five sites; `0x0053AD40`'s body beyond its first lines (it reacts to the bit's change).
- Whether anything outside the build (a script, SetGore (cutscene command 45), a respawn or load) writes `a+0x5D` back to 0; the only writer found in 0x006A... was `0x006AAC20` (a full scan of `+0x5D` writes was too noisy to finish).
- A ray test of the cap against the face's triangles; the armour chest models' collars; the seen-in-play check; depth bias and ZFUNC calls (not searched).
- Scratch: `%TEMP%\lib-agent-2-neck2b-*.py` and `lib-agent-2-neck2b-files\`.

## A model's level of detail (read 2026-10-23 by paragon-agent for DAOParagonEngine #111, checked by lib-agent-2)

GOG 1.05 addresses. [C] read in the code or the files, [I] inferred.

- **The level:** it comes from the creature's LOD band (PARTY.md, the LodManager), through `AnimBase` slot 0x44 (0x00A5D5D0) and the table 0x00B03B98. Bands 0–3 draw levels 0, 0, 2 and 3; level 1 is never used [C].
- **The two held items follow the body** [C]: AnimBaseWield's slot 0x44 (0x00A669C0) calls 0x00A5D5D0. On a change, it reloads its two holders at `+0x230`/`+0x234` (0x00A68DB0) [I that they are the hands' items].
- **The file:** 0x00A67BE0 (this, name, level, out) tries `%ls_%d.mmh` (0x00B598E0) over the level's row in 0x00B03B98 (0x00617020), then `%ls.mmh`, then `%ls_0.mmh` [C].
  - So a missing level falls back along the row.
  - The data has no `_1` files (for example `hm_hed_dcpa_0`, `_2`, `_3`) [C].
  - The reload fires when the level found differs from the holder's `+0xB4` [C; the rest of 0x00A68DB0 not read].
- **The materials:** the `_l2`/`_l3` `.mao` name LOD semantics [C].
  - For example, `pf_rob_mora.mao` uses `AlphaArmourSkinTint` with a normal map. Its `_l2` uses `PunchthroughArmourSkinTintL2`, and its `_l3` `ArmourSkinTintL3`, both with no normal map.
  - Vanilla `Character.mat.xml` gives L2 and L3 the technique `LOD2`: `Char2.vsh` with `Ch2ArmTnt.psh`. It requires only POSITION, NORMAL and TEXCOORD0.
  - The `_l2` textures are a quarter of the base's bytes, and the `_l3` a sixteenth.
- **The meshes** [C]: some `_2` and `_3` meshes have tangents and binormals and some do not (corrected 2026-10-23 by lib-agent; it said none had). Of the game's 543 `*_2.msh` (ghilana, every copy, read with tools/gff4.py), 318 declare a TANGENT (usage 6) and BINORMAL (7) element and 225 don't: `hm_arm_med_2.msh` has both, as `hm_arm_med_0.msh` does; `cn_hed_ogr_2`, `df_har_bld_2` and the levels' `*_v_2` meshes have neither [C, read 2026-10-23 by lib-agent]. `hm_hed_dcp_2.msh`/`_3` have 36-byte vertices, where `_0` has 52 with both.
- **The shaders** [C, vanilla `shaders.erf`, disassembled]:
  - `Ch2ArmTnt` is `Ch1`'s with the normal-map sampler dropped, so it uses the vertex normal (`nrm r1.xyz, v2`).
  - `Char2.vsh` drops the tangent and binormal inputs and skins by 2 bones, its weights over `v3.x + v3.y`. `Char1.vsh` skins by 4.
  - Not read: the SM2 variants and `Char2_VertexLit.vsh`.
  - **Which shaders a head's lower levels use** (read 2026-10-23 by paragon-agent for its far heads, checked by lib-agent-2; the shaders and materials from vanilla's .erf archives, since DAOElgaraShaders overrides face0/face2/hair0/hair2 and hairalpha.mat.xml) [C, vanilla `.mat.xml`]:
    - **The face:** `Face.mat` gives LOD2 and LOD3 as `Char2.vsh` with `Face2.psh`, under `Face.sta`.
    - **The hair:** `HairAlpha.mat` gives LOD2 and LOD3 as `Hair2.vsh` with `Hair2.psh`, under `Character.sta`. The file says "The low-LOD hair only renders the opaque bits".
    - **Opaque hair:** `Character.sta` has blending and the alpha test off, with z test and z write on. `Hair2.psh` has no texkill.
  - **`Face2.psh` against `Face0.psh`** [C]:
    - **No normal maps:** it drops the four normal maps (s2, s4, s9, s12) and takes the skinned vertex normal from `Char2.vsh`. The pixel shader does not normalise it.
    - **No self-shadow:** `Face0` multiplies each light's diffuse and specular by a head-bone occlusion term, and `Face2` has neither.
    - **The rim:**
      - Its view factor is `max_sat(0.01, TEXCOORD0.w) × sat(1.5 − d)`, and `Char2.vsh` writes TEXCOORD0 as (u, v, 0, 0). So it is 0.01 × sat(1.5 − d).
      - That is 0 beyond 1.5 m from the camera, so a head at level 2 or 3 has no rim.
    - **Otherwise:** the colour terms, the highlight and the tail are `Face0`'s line for line, apart from register numbers. The one other change is that the brow and stubble sample loses `Face0`'s −1 bias.
  - **`Hair2.psh`** [C]:
    - It has no highlights.
    - The lights are three point lights (c0, c5, c7) and the sun, each max(N·L × 0.75 + 0.25, 0). Each point light keeps `Hair0`'s sin² attenuation; the sun has none.
    - The lights are added to the probe's ambient from the vertex shader's colour output.
    - Then come `Hair0`'s tint (noise × c13 + c11 × G + c15 × (1 − G)), the remote material, the tint divide and the fog.
    - `Hair2.vsh` skins by two bones, as `Char2.vsh` does, and takes no tangent.
  - **A level-3 hair's missing textures** [C for the files, I for the engine's fallback]:
    - `HairAlpha.mat.xml` gives `Default_Missing` as the default for `mml_tTintNoise` and `mml_tPackedTexture`. `Default_Missing` is a flat grey 32×32: every pixel is BGRA (0x80, 0x80, 0x80, 0xFF). It is not black.
    - No `_l2` or `_l3` hair `.mao` names a tint noise.
    - `uh_har_h01a_l3.mao` names no packed texture either. It names `mml_tTintMask`, which `HairAlpha.mat` does not have. `pn_har_all_l3.mao` does name a packed texture.
    - That the engine takes the material's default for a parameter the `.mao` leaves out was not read in the exe [I].
  - **The eyes and lashes have no lower levels** [C]:
    - No eye (`uem`) or lash (`ulm`) mesh has a `_2` or `_3` file. `hf_uem_basa` and `hf_ulm_basa` exist only as `_0.mmh`.
    - So by the file rule above they stay at level 0 at every band.
    - Of the 52 `uem`/`ulm`/`uhm` `.mmh` names, only the `uhm` hair caps have lower levels. The heads have them too (`hf_hed_dcpa_2`/`_3.mmh`).
- **What Paragon saw in play in the camp** (Lyna, Slot_489):
  - Morrigan at 11.6 and 14.6 m from the hero was drawn by `Char2.vsh` and Ch2 (robe `pf_rob_mora_0dl2`), her head by `Face2.psh` and her hair by `Hair2`.
  - The dog at 26 m was at level 2, and the emissaries at 29.4–30.8 m at level 3.
  - Shale's body was at level 0 while his head was at level 2 (not explained).
- **A morph at the lower levels** (read 2026-10-23 by paragon-agent for its far heads, checked by lib-agent-2; the shaders and materials from vanilla's .erf archives, since DAOElgaraShaders overrides face0/face2/hair0/hair2 and hairalpha.mat.xml):
  - **Loading the morph** [C]:
    - The morph load 0x005895D0 (ID 9208) reads each node through 0x00588A60 (ID 9203).
    - For each node it reads the vertex stream and count, then the lists of texture, vector and float parameters.
  - **Applying it:** ObjectNodeMesh's vtable 0x00AF8F4C, entry +0x4C (slot 19 from 0), is 0x0055B540 (ID 8283) [C]. In order:
    - **Bones:** it calls 0x0055EC80 unconditionally, for the bone rest plus the morph's Position. So a morph's bone offsets apply at every level.
    - **The node:** it finds the morph's node by the mesh node's name.
    - **Material parameters:** 0x005696E0 (ID 8631) sets the node's textures, vectors and floats. It is called in every mode, before any count test, and has no level condition. It returns early only on a null material or argument.
    - **Vertices:** 0x0057D430 (ID 8999) replaces the vertices only when the morph node has a vertex stream (+0x10) and its count (+0x18) equals the mesh chunk's (`[+0x14C]+0x4C`) (0x0057D4F1..0x0057D509). Otherwise it jumps to 0x0057D6DC and silently skips the replacement. The buffer is then rebuilt from the mesh's own vertices.
  - **So a head at level 2 or 3 keeps its base model's shape,** but takes the morph's textures, tints and bone offsets [C].
    - For example, Morrigan's `hf_genfl_morrigan.mor` has FaceM1 at 2280 vertices, EyesM1 at 373 and LashesM1 at 271 [C].
    - Her FaceM1 names uh_hed_fema_0d, uh_hed_kida_0d and uh_pmt_001_0t among its five textures [C].
    - Her bone nodes have count 0 and only millimetre-scale Positions (:101–102) [C].
    - The level-2 face's 594 vertices are as seen in play [S]; the `_2` mesh was not parsed.
  - **Seen in the camp** (Lyna, Slot_489) [S]:
    - Morrigan at 29 m and 13.3 m: her face was drawn by `Char2.vsh` and `Face2.psh` (1025 triangles, 594 vertices), her hair cap by `Face2`, and her hair (588 vertices) by `Hair2`, opaque with no alpha test. Her eyes were at level 0.
    - At 13.3 m the face's smpDecal was uh_hed_fema_0d, the morph's full-size texture. The age diffuse uh_hed_kida_0d and the tattoo uh_pmt_001_0t were also the morph's.
    - The tint mask, the emotion masks and the brow and stubble were the `_l2` `.mao`'s L2 textures.
  - **Not read** [I]: whether a lower head's bone rest is the level-0 head's.
