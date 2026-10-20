# Effects

A visual effect as the game's files hold it: the table row that names it, the model that is its parts, and the clips and events that run it. The reader is `src/dao/core/Effect.h` (DAOLib #181), for DAOParagonEngine's effects (Paragon #26, `dev/PARAGON_VFX.md`'s first part). The first sections are the files; the last, how the engine plays them, is lib-agent-2's reading in the exe (the request's second part).

Read 2026-10-18 by a subagent of lib-agent over ghilana's extract of what the game takes: the 600 models `fx*.mmh`, their 549 trees `fx*.anb`, 1,091 clips `fx*.ani` with as many `.gad`, 1,070 event files `fx*.evt`, and `vfx_base.gda`'s 993 rows (tools/gff4.py, tools/gda.py, and scripts of the session's own). "By the look of it" marks what the files suggest and the exe has not confirmed.

## There is no effect file

There is no `.vfx` resource. An effect is four kinds of file, all GFF4, named for the effect: `<effect>.mmh`, a model hierarchy (MMH V0.1) whose nodes are the effect's emitters, lights and hooks; `<effect>.anb`, an animation tree (ANBT) whose nodes are its phases; `<effect>.<phase>.ani`, a clip (ANI V0.1) per phase that keys the parts' parameters; `<effect>.<phase>.evt`, the clip's events (EVT V1.0); and `<effect>.<phase>.gad` beside each clip. They are in `packages/core/data/globalvfx.rim` and again in `modelhierarchies.erf`, `modelmeshdata.erf` (an effect's `.msh`) and `anims.erf`; `fxc_wisp_c.mmh` has a newer copy in `patch/patch002.erf`, which the game takes.

## The table: Visual Effects (table 8, `vfx_`)

m2da_base names table 8 "Visual Effects", prefix `vfx_`: `vfx_base.gda` and the add-ins' `vfx_base_*.gda`, `vfx_str.gda` and kin (enciphered, not read). An ID in it is what a creature's appearance (apr_base's Crust_Effect: 3006 for `cn_wispa`) and the scripts' effects name. Its columns: ID, Label, Type, FixedOrientation, AlignToGround, ModelName, ModelName1, Size1, ModelName2, Size2, ModelName3, Size3, BlendTreeName, CrustEffect, SoundImpact, SoundImpactVolume, SoundImpactPitch, SoundDuration, SoundDurationVolume, SoundDurationPitch, SoundCessation, SoundCessationVolume, SoundCessationPitch, Tint, LoadGlobal, PauseAnimate, Priority, ShowInPaperDoll.

- ModelName1 to 3 are the model's file without `.mmh` (ModelName, which the engine never reads, equals ModelName1 in 991 of the 993 rows; the other two have both empty), BlendTreeName the tree's without `.anb`. Model and tree are the same name in 991 rows; 928 rows name one of the 600 `fx*.mmh`, 63 another (`SR_PTY`, the selection rings; `fxm_*`; test rows), 2 none. 899 name an `fx*.anb`, 4 none. One ModelName starts with a space (4024, `" fxe_R_Ripple_aoe_P"`); the engine trims spaces and tabs before it loads the name, and so does the reader.
- ModelName1 is ModelName in every row. Seven rows (4007, a fire; 30005 and kin, the rings) name three models with their sizes: 0.35, 1.3 and 2.5 for the fires small, medium and large. The engine takes the model whose Size is nearest the owner's `PerSpace` and scales it by `PerSpace / Size`, ModelName2 and 3 read only when Size1 > 0 (lib-agent-2, 2026-10-18; the section on the model's choice below); `effect::Row::ModelFor` does the same.
- Type is `D` in 601 rows, `F` in 381, `T` in 9 (the sprite armies), `C` in one, empty in one. `F` rows' trees are mostly the impact alone (323 of 381), `D` rows' the three phases (395 of 601): by the look of it `F` is an effect that plays once and `D` one that lasts.
- CrustEffect is 1 in 637 rows, a crust (an effect on a creature, at its crust hooks). The sounds are sound events, `vfx_spells/vfx_new/monster/fxc_wisp_c_dur_n` and kin, one a phase.

The wisp's row, 3006 "Wisp Crust": type D, model and tree `fxc_wisp_c`, a crust, its three phases' sounds.

## The model: an effect's parts

An effect's `.mmh` is a model hierarchy as `dao/core/Model.h` reads one: a top struct (`mdlh`) and a tree of nodes under MMHChildren (6999), a generic list of references. A node's own children are its transform (`trsl`, `rota`, `scal`), its exports (`xprt`) and the nodes under it. The kinds of node in the 600 models, with every field the kind has, each kind's fields the same in every file:

| Kind | Count | What it is | Its fields |
|---|---|---|---|
| `node` | 949 | a plain node; GOB at the top | MMHName, MMHBoneIndex (-1), MMHNodeSoundMaterial |
| `nemt` | 2,927 | an emitter | 59 fields, below |
| `crst` | 376 | a crust hook: where on the creature its children go | MMHName, MMHNodeCrustHookID (1 in 119, then 5, 2, 17, 13 ...) |
| `nemg` | 166 | an emitter group: its emitters under it | MMHName, MMHMaterialObject |
| `emtg` | 123 | a target group (by the look of it what emitters aim at) | MMHName |
| `nplt` | 109 | a point light | MMHID, MMHNodePointLightColor (it passes 1: the wisp's 0.6, 1.4, 2), Radius, IsStatic (0 in all), IntensityVariation, IntensityPeriod, IntensityPeriodDelta (set in 45), MMHNodeLightAffectDomain (1 or 2), MMHLightCanBeOccluded (0), MMHUseVariationTint, MMHNodeLightVersion (0) |
| `mshh` | 49 | a mesh, as any model's; MMHMeshIsVFXMesh 1 in 14 | Model.h's |

The top struct has, besides a model's fields, MMHEmitterPresimulateTime (423 files) and, in 45 of the 600 effect models, a remote material: MMHRemoteMaterialDecalName (`White.dds` in 22 of them, and in `remotematerialdummy.mmh` outside the fx set, 23 in all), FresnelFalloff, InvertFresnel, Alpha and Tint. At run time it is laid on every material of the crust target that has `mml_tRMPDecal`, as a colour lerp in the target's own pass, with no alpha (read 2026-10-20: "An effect's remote material and its emitters' attachments", below).

An emitter (`nemt`) has 59 fields of its own, every one in every emitter (the labels' names are xoreos's, `dev/gff4_fields.csv`): its material (MMHMaterialObject, of the library MMHMaterialLibrary, a `.mal`, named with its extension in 2,290 emitters and without it in 637), MMHNodeEmitBirthRate and its range, Life and its range, ScaleRange, InitialSpeed and its range, Acceleration, InitialRotation and its range, InitialRotationSpeed and its range, RotationalAcceleration, MMHNodeInvEmitterMovementSpreadUpdateDelay, SpawnSpreadX and Y (180 and 180 in the wisp's glow: by the look of it degrees, all round), MovementSpreadX and Y, GravityMultiplier, WorldAxisAcceleration (a vector: the wisp's smoke rises at 0.5), ObjectSpaceAcceleration, BirthrateInParticlesPerMeter (1 in 131), ParticlesAffectedByWind (1 in one), ParticlesFollowPath (25), LinkParticlesTogether (29: by the look of it a ribbon), UpdateOnlyWhenVisible, EnableParticleCollisions (0 in all), InheritVelocityInsteadOfPos, Bounciness, Friction, OrientationBehavior (0 in 1,868, 3 in 629, 1 in 256, 2 in 88, 4 in 86), ParticleInheritance (0 to 3), the flipbook (MMHEmitterFlipbookType 0 to 2, FramesPerSecond, Rows and Columns up to 64 and 16, RandomStartFrame), the target (MMHEmitterTargetName, set in 278, TargetAttraction, TargetRadius, SpawnDirectionTracksTarget, KillParticleWhenTargetHit), IsPhysicsEmitter (61), IsPhysicsObjectSpawnEmitter (185), the mesh particle (MMHNodeEmitMeshParticleModelName, a model such as `fx_rock`, and its up and roll axes), MmnNodeEmitVertexFormat (255 in all), UvDistributionSize, IgnoreDistortion (0), CanParticlesSplat (7), Lod (0 to 2), MMHNodeEmitUserParamName (`color`, `fire`, `Drops`: by the look of it the name a tint is given by), MMHUseVariationTint, the age map's multipliers (MMHNodeEmitAgemapColorMultiplier, ScalexMultiplier, ScaleyMultiplier), and MMHNodeEmitType.

MMHNodeEmitType is 0 in 1,887 emitters and 1 in 1,040. 1,026 of the 1s are named by an event of their clips (below), and only 98 of the 0s are; 26 of the 1s export their birth rate for a clip to key, 1,043 of the 0s. The files suggest 0 emits all the time at its birth rate and 1 when an event fires it; the runtime section reads the same in the exe (1 a burst, armed by event 16). The wisp's glow and rays are 0, keyed through its phases; its expiration ring and smoke are 1, fired at the cessation's 0.97 s.

An emitter's structs:

- `amap`, its age map (in every emitter): MMHNodeAgeMapCount and its keys, `amel` structs, each MMHNodeAgeMapElementPercentLifeElapsed (0 at birth, 1 at death, rising), ScaleX, ScaleY and Color, a particle's size and colour at that share of its life. 13,868 keys. The wisp's glow: 0.6 by 0.5 and transparent at birth, 0.56 by 0.47 and opaque at 0.1, 0.2 by 0.15 and transparent at death, blue (0.4, 0.7, 1).
- `spnv`, its spawn volume (1,145): MMHNodeSpawnVolumeType 0 a mesh (20), 1 a sphere (200, its radius), 2 a cylinder (608, its radius, length and axis), 3 a box (317, its corners); the fields of the other shapes are NaN. With SpawnWithinVolume, InvertSpawnVolumeNorms and NormalsAsDirection. A mesh volume holds two `data` structs: the points (MMHDataIsIndexStream 0, a Vector3fList) and the indices into them (1, a Uint16List), as many as MMHDataCount says (20 to 384), by the look of it a triangle list.
- `emas`, its attachments (185), each an `emat`: MMHEmitterEmitterAttachmentType (1 in 153, in 66 models; 4 in 32, in 5), Name (empty in all), SpawnOnSurface, UseNormalForVelocity. The particles are born on the owner's `.phy` shapes, not its visible mesh: type 1 the owner's physics nodes, type 4 a scene-level source [inferred] (read 2026-10-20, below).
- `spla`, its splat (7, the blood pools): a material, a size, samples, an orientation range, a life, a flipbook, HoldLastFrame, a colour multiplier, and an age map of its own.

A list of Vector3 keeps each element in 16 bytes, the fourth float junk (`2.28e-38` in `fxa_lit_cone_c.mmh`): the 384th point of that file's lists is a point at that stride and not at 12. `tools/gff4.py --dump` reads such a list at 12 and shows it garbled; `dao::core::gff4::Struct::ListBytes` reads it at 16.

## Exports: what a clip drives

A node's export (`xprt`, 2,896) is MMHExportExportName, the name a clip's channel goes by, `<node>_<what>` in lower case; MMHExportControllerType (0 a vector, 2 a rotation, 3 a float, 6 and 8 the GOB's translation and rotation, 9 a colour); MMHExportTagVariableType, what it drives; and MMHExportControllerIndex, 0 to MMHTotalExports - 1 in 593 of the 600 models. Each name's ending goes with one tag in every export of the game's (Effect.h's `Parameter`): 0 rotation, 1 translation, 4 gobrotation, 5 gobtranslation, 6 life, 7 liferange, 8 birthrate (1,079 exports), 9 birthraterange, 10 initialspeed, 11 initialspeedrange, 12 acceleration, 13 gravitymultiplier, 14 initialrotationspeed, 15 initialrotationspeedrange, 16 spawnspreadx, 17 spawnspready, 18 movementspreadx, 19 movementspready, 20 invmovementspreadupdatedelay, 21 scalerange, 22 targetattraction, 23 targetradius, 24 color (a light's), 25 radius (a light's), 27 scale, 28 agemapcolormultiplier (434), 29 and 30 agemapscalex and y multiplier, 32 worldaxisacceleration, 33 remotematerialtintcolor, 34 remotematerialfresnelfalloff, 35 remotematerialalpha, 36 materialcolor, 37 splatcolormultiplier.

Of the 6,773 channels of the effects' clips, 6,624 name an export of their effect's model and 149 none: the wisp's clips key `trail_birthrate` and `light_c_color`, which the copy of `fxc_wisp_c.mmh` the game takes (patch002's) does not export.

## The tree and its clips: the phases

An effect's tree (`dao/core/Anb.h`) is its root, named for the effect, and up to three clips: `vfximpact` (474 trees; flags 1), `vfxduration` (312; flags 0) and `vfxcessation` (274; flags 1), each clip `<effect>.impa.ani`, `.dura.ani` or `.cess.ani` (and `vfx` in one tree, `.cust.ani` in two). The flags are those of a creature's trees, 1 on a clip that plays once: the impact plays once at the start, the duration loops while the effect lasts, and the cessation plays once at its end, as the runtime section's phase machine reads them. The wisp's tree has the duration (1 s) and the cessation (3.33 s), no impact.

A clip (`dao/core/Clip.h`) keys the exports' values over its length: the targets 8 (a float, 2,688 birthrate channels), 10 (a colour), 6 (a vector) and 2 (a rotation in 32 bits). The wisp's duration holds every value constant (glow's birth rate 7, rays' 10, the lights' radius 2); its cessation drops the birth rates to 0 at 1.03 s, flares the lights' radius to 4 at 1 s and fades their colour to black at 1.1 s, and moves NewDummy_14 out and up. How a key's value stands to the node's own (whether it replaces the emitter's birth rate or multiplies it) is not read; the wisp's duration keys the same values its model has.

A `.gad` beside each clip has AnimationGeneralAnimName (`impa`, `dura`, `cess`), AnimationAnimLength, the clip's length in every one, AnimationHasGobAnim (0 in all), AnimationCombatRange and an empty node list. The reader does not read it.

## Events

A clip's events (`<clip>.evt`, `clip::EventsOf`) are a list (AnimationEventList, 4020) of AnimationEventTime (a double, seconds), AnimationEventID, AnimationEventTarget and AnimationEventString. All 2,333 of the effects' events are id 16, target 1, at a time within their clip's length (197 past 1 s), and name a node of their effect's model in 2,098 (an emitter, by the look of it fired: see MMHNodeEmitType above). 1,856 are in impact clips, 297 in durations, 180 in cessations. The wisp's cessation fires `expirationsmoke` and `expiration` at 0.97 s.

## What is not read

- What the files hold that the runtime section does not explain: the target groups (`emtg`) and `nemg`'s own material. The attachments and the remote material are read below (2026-10-20); attachment type 4's source and types 2 and 3 (by Name, which no file uses) are not.
- What Type `T` and `C`, Tint, LoadGlobal and Priority do.
- No tree of the game's has a `vfxcast` node, the runtime's first phase; one has `vfx` (`.cust.ani` in two), which the phases do not name.
- Where the two readings differ: the runtime section reads WorldAxisAcceleration as a vec4, where the files' field is a Vector3 (type 10) in every emitter.

## As the engine plays them: read 2026-10-18 by lib-agent-2

Read in the GOG exe, the game's files, the scripts and elgara-agent's shader listings by lib-agent-2 (with subagents), for `dev/PARAGON_VFX.md` part 2 (DAOParagonEngine #26). Not seen in play. [C] read in the code or data, [I] inferred; the key claims were checked again in the disassembly.

### What an effect is

- **There is no `.vfx` resource.** An effect is a `vfx_base` row (table 8): a model (`.mmh`/`.msh`; **corrected 2026-10-18 by lib-agent-2:** the engine never reads `vfx_base`'s `ModelName`, only `ModelName1..3` with `Size1..3`, chosen by the owner's `PerSpace`, as the section on the model's choice below says) with an animation tree (`BlendTreeName`, `.anb`) whose emitters, lights and meshes are the model's nodes. `Type` is `D` (duration) or `F` (fire and forget).
- **The row loader** (0x007E9E00) keeps `PauseAnimate` (+0x0D, hash 0x5C35C9C5), `CrustEffect` (+0x0E, 0xBE06F2EF), `FixedOrientation` (+0x0F, 0x9E459C07) and `BlendTreeName` (+0x10, 0x1109E419). The rows are cached at 0x00C11978.
- **Every effect is a phase machine** (`CCVisualEffect` +0x444; the phase's animation by the table at 0x00B23D94) [C]:
  - 1 `vfxcast` (anim 500), 2 `vfximpact` (502), 3 `vfxduration` (501, loops), 4 `vfxcessation` (503), 5 done.
  - It advances (0x007EAC30) past any phase the tree lacks.
  - The stop (0x0079E3E0) stops the impact and duration sounds, plays the cessation sound, and goes to cessation; with no cessation clip it destroys the effect at once (0x0079DF10).
  - The sounds are the row's `SoundImpact`, `SoundDuration`, `SoundCessation`.
- **The manager** is the singleton at [0x00C389C8] (0x0079E820): create 0x0079EDC0 → 0x0079E8A0. A "held" create (+0x448 = 1) loads without starting; 0x0079D810 starts it.
- **While the game is paused** the manager's update (0x0079EE20) animates only effects whose `PauseAnimate` is set [C].

### A creature's crust effect: the scripts apply it

- **The exe never reads `apr_base`'s `Crust_Effect`**: its hash (0x1BB8AB1F) is nowhere in it [C].
- **`creature_core.nss`:398, on SPAWN**, reads it and, when it is above 0, calls `ApplyEffectVisualEffect(self, self, n, 5 /* innate */, 0.0)`. The local `CREATURE_SPAWNED` makes that once per creature.
- **The wisp** (`apr_base` 14, `Cn_WISPa`): `vfx_base` 3006 "Wisp Crust" is `fxc_wisp_c` (model and tree, `CrustEffect` 1; tree nodes `vfxduration` → `fxc_wisp_c.dura.ani`, `vfxcessation` → `fxc_wisp_c.cess.ani`).
  - Its `.mmh` (patch002.erf's copy) has under GOB a `crst` node `CrustHook1` (hook id 1). Under it hang dummies, the emitters `rays` and `glow` (`ParticleInheritance` 1) and `expiration` and `expirationsmoke` (0), and two point lights of radius 2.
- **The wisp's own model is not a point mesh** [C]. `cn_wisp_0.msh` is 24 triangles about 0.00007 m across at z = 2, `ContactSheetBlend` with `fx_glow02.dds`, and nothing turns its vertices into sprites. What shows is the crust. `cn_wispa_0.mmh` has `crst` nodes `CrustHook1` (1) and `CrustHook2` (2) under the `Root` bone, which sits 2 m up and is animated.
- **The command** (`ApplyEffectVisualEffect`, 635, 0x0065D300) makes a `CGameEffect` of type 4: int 0 the `vfx_base` row, the duration type, the duration in ms. It is applied through the holder (0x007A1A20).
- **The apply handler** (0x007A00A0) makes the effect through the object (0x00695E70 → 0x0079F540: the row, the scale `EFFECT_FLOAT_SCALE`, a tint; 0x00697830 → the manager) and keeps the handle at +0x74. For an innate effect on a creature whose death effect is in place it makes nothing (0x0079F4A0).
- **The attachment** (0x0079D9F0 → 0x0079D690) [C]:
  - A **crust row**: `GraphicsStructures::Object` slot 0xDC (0x00543BD0) binds the hooks. For each `crst` node of the effect (`MMHNodeCrustHookID`, field 6235, u8), it finds the owner's `crst` node with the same id (0x00562FE0) and makes a `NodeAttachDependency` (vtable 0x00AF837C) from the owner's node to the effect's. The effect's GOB is added as a child at the owner's GOB. So each `crst` subtree follows the creature's hook of its id, animated with it (inferred: every frame, through the dependency). `crusts.gda` names the ids: 1 Default, 2 Right Cast, 3 Left Cast, 4 Root, 5 Head, … 40–49 Custom.
  - A **non-crust row**: a plain child of the owner's root (slot 0xD4, 0x0054DD80). An owner with no model has the effect placed at its position, and its orientation unless `FixedOrientation` (0x0079D580).
- **When it goes:**
  - At **death**: the death effect's apply (0x007A12E0) removes innate visual effects (inferred: the three matchers ANDed), and `effect_death_h.nss`:135 removes all effects. The crust goes at the start of dying, playing its `vfxcessation` (the wisp's `expiration` emitters).
  - **Nothing puts it back** on revival: the spawn code is guarded, and the innate apply is refused while dead.
  - When the object is **destroyed** (slot 0x8C → 0x0079EC20(id, 0)), every effect whose owner (+0x450) or crust target (+0x454) it is goes at once.
  - Not read: an area's unload, `SetObjectActive(FALSE)` (inferred: hidden with the model), stealth.

### A spell's effects against its conjure, cast and impact

- **`abi_base`'s columns** [C]: `ConjureVFX` (0x2B6BDC9F), `VFX_Impact0` (0xC956AC09), `VFX_Impact1` (0xD04D9D48), `Projectile` (0x015032BB, a `prj_base` row), `Crust_ID` (0xC4D9FED5), `AOE_Type`/`AOE_Idx`. There is no cast column. The engine reads `ConjureVFX` (0x007445A7), `Projectile` and `Crust_ID`; **only the scripts read `VFX_Impact0/1`**.
- **The conjure effect** [C]:
  - It is made when the command starts, before any approach (0x007445A7 onward in 0x00743F50). It is a temporary type-4 effect on the caster, lasting the conjure's ms ÷ the playback scale, held.
  - It starts when the conjure does: sub-action 7 (0x007A66A0) reads its id from int 3 and starts it (0x007A6721 → 0x0079FD00, the start callback 0x0079F3B0), only when the conjure has an animation.
  - The effect's clock doesn't run while its effect is unstarted (0x0079F330), so it ends with the conjure: `vfxduration` loops till then, then `vfxcessation` plays.
  - An interrupted command's end (0x00746BA0) removes the caster's effects of that row and ability.
  - An instant spell (Speed 1, a conjure of 0 ms) gets a 0-ms effect, so only its cast and cessation clips show (inferred).
- **The cast** is the cast animation's events (`events.gda`; `CCCreature::OnAnimationEvent` 0x006C0BF0):
  - 7 `AbilityCast` → the impact (0x006AF430);
  - 9 `ProjectileLoad` → the projectile made, held at the hand (vtable index 60, 0x006B6F60); with no projectile, straight to the impact;
  - 10 `ProjectileFire` → released (index 55, 0x006AFA40), and event 93 raised;
  - 38 `VFXInstant` → an instant type-4 effect on the caster, the row from the event's string (0x00696DE0).
  - The times are the clips' `.evt`: e.g. `m.ua_acast` `AbilityCast` 0.97 s; `mh_c.ua_spl_d_cst` `ProjectileLoad` 0.90 s, `ProjectileFire` 0.93 s (the clips' names by inference).
- **The projectile's look** is `prj_base`'s `ModelName`. `Crust_ID` (+0x4B4 on the projectile) is not an effect: it is the caster's hook the projectile is held at until it is released (**corrected 2026-10-18 by lib-agent-2**). The engine makes no impact effect when it lands; it queues the impact.
- **The impact effects are the scripts'** [C]:
  - `Ability_ApplyLocationImpactVFX` (`ability_h.nss`:1175–1186): `VFX_Impact0`, instant at the location.
  - `Ability_ApplyObjectImpactVFX` (:1188–1203): `VFX_Impact1`, instant on the target.
  - A damage effect with no impact effect gets one by its damage type (`effect_damage_h.nss`:857–891; spirit 1514 `fxa_spi_imp_c` is Arcane Bolt's hit).
  - Lingering looks are D-type effects applied temporary for a duration (Winter's Grasp's 90159 for 5 s, Inferno's area 90144 for 30 s).
- **Examples:**

| Spell | ConjureVFX | Projectile | Impact |
|---|---|---|---|
| Arcane Bolt (200254) | 90290 `fxa_mana_conj_c` | 137, `fxa_arc_bolt_p` | by damage: 1514 |
| Winter's Grasp (13001) | 90252 `fxa_ice_conj_c` | none | `VFX_Impact1` 90157 `fxa_ice_wg_c` |
| Flame Blast (10001) | 90201 `fxa_fire_conj_c` | none | `VFX_Impact0` 90139 `fxa_fire_cone_c` |

`dev/ABILITIES.md` corrections, checked against `CCCreature`'s vtable (0x00B10484): the projectile maker 0x006B6F60 is index 60 (offset 0xF0), not 61; PROJECTILE_LAUNCHED's is index 55, the function at 0x006AFA40 (not 56, 0x006AFA50).

### How particles are drawn

- **Built on the CPU, in world space, every frame** [C]. The `VFX0*` vertex shaders take only `aur_mViewProjection`.
  - Two shared dynamic pools (`ParticleVertexPool`, vtable 0x00AF9B0C): [0x00C31A74] of 32-byte vertices, 0x3FFF quads; [0x00C31A70] of 68-byte, 5000 quads, for materials needing tangents.
  - A static index buffer of 256 quads (`b, b+1, b+2, b+2, b+3, b`). **A sprite emitter holds at most 256 particles** (0x00552AFE, checked).
  - The 32-byte vertex: float3 position, float4 TEXCOORD0 (u, v, frame, alt), D3DCOLOR. The 68-byte adds normal, binormal, tangent.
- **A sprite** (`ObjectNodeEmitterSprite<SpriteParticle>`, vtable 0x00AF87F4; the fill 0x00550860, drawn only while the emitter's opacity +0x4E0 > 0):
  ```
  t   = invLife == -1 ? 0 : age < 0 ? 1 : min(age*invLife, 1)
  rgba, sA, sB = AgeMap(t)            // 0x005607C0: 32-byte keys, linear in t: t, rgba, scale A, scale B
  rgba *= tint(+0x39C); sA *= E+0x3AC * p.size; sB *= E+0x3B0 * p.size
  rgba.a *= E+0x4F4 * E+0x4E0; rgba = saturate(rgba)
  S = the owner object's scale (+0x5C)
  A = p.axisA * sA * S*S; B = p.axisB * sB * S*S     // S twice, as read
  v0 = P-A-B uv(0,0); v1 = P+A-B uv(0,1); v2 = P+A+B uv(1,1); v3 = P-A+B uv(1,0); TEXCOORD0.zw = (p.frame, 0)
  ```
  Which scale is the toolset's X and which Y is open.
- **A sprite's size, end to end** (read 2026-10-20 by lib-agent-2, for Paragon's fire size; checked unless said):
  - **The owner's scale S** (+0x5C) is 1.0 from the object's constructor (0x0054FA46); the fill reads it through the owner's slot 0xE0 (0x0054F7B0). A room's placed model sets it through slot 0xAC (0x00545330) only when the record's scale is not 0, so a 0 leaves 1.0.
  - **A particle's size** is 1 + r · ScaleRange, r uniform in [−1, 1], set at birth (0x00552AF0). No factor of 0.5 or 2 anywhere on the way.
  - **A key is a half size:** the corners are P ± A ± B, so the age map's key × the emitter's multiplier × size is half the quad's extent. A key of 1 makes a quad 2 m by 2 m.
  - **S² applies to sprites, and S not at all to ribbons**, whose half-width is axisB × the Y key × the Y multiplier × size.
  - **Worked:** `fxe_fire_cnd_p`'s glow (size 0.85 to 1.15, multipliers 1.25) is 0.30 m wide and 0.39 m high at S 1; a candle record of scale 1.5 makes it 2.25 times that.
- **Facing** (`OrientationBehaviour`, emitter params +0x334; set at spawn 0x0057C520, each frame 0x0057C7B0), with the camera's position, forward, up and right:

  | Mode | Name | Axes |
  |---|---|---|
  | 0 | None (screen-aligned) | each frame A = −up, B = right, turned about −forward by the angle |
  | 1 | BillboardToEmitterZ | at spawn the emitter's world X, Y; then only spun |
  | 2 | BillboardToWorldZ | world X, Y: flat on the ground |
  | 3 | AlignedToParticleDirection | D = normalize(velocity); A = −D, B = normalize(cross(−D, P − cam)) |
  | 4 | AlignedToWorldZ | as 3 with D = (0, 0, 1) |

  Spin: `angle += spin·dt·2π; spin += spinAccel·dt`; the initial angle is `InitialRotation + rand[−1, 1]·InitialRotationRange`.
- **The contact sheet:** `mml_vContactSheetParams` = (cols, rows, cols·rows, 0) from the blueprint (+0x202, +0x201), set when the instance is made (0x00560CB7). The frame starts at 0, or rand[0, 1) with `RandomStartFrame`; each update adds `FramesPerSecond·dt / (cols·rows)` (0x005528E0). The vertex shader picks `f = N·z; row = floor(f/cols); col = floor(frac(f/cols)·cols)`, with no blend between frames; the sampler's wrap loops it. `FlipbookType` 2 is a volume texture, W = z.
- **A ribbon** (`ObjectNodeEmitterSpriteLinked`, vtable 0x00AFADAC; fill 0x005850E0):
  - One quad per segment, from the linked particles (+0x520 to +0x524), the last to the emitter's current place.
  - Half-width `W = axisB · scaleY` (the age map's Y scale × +0x3B0 × size; **corrected 2026-10-18 by lib-agent-2:** not scaleA, the X scale is computed and unused), averaged at joints; it faces the camera only as well as axis B does (mode 3 makes a proper ribbon). The object's scale isn't applied.
  - Vertices `P+W` (1, u, frame, 1) and `P−W` (0, u, frame, 1), each pair its particle's colour.
  - u: with N (+0x2B0) 0, 0→1 over the ribbon; else k/N wrapping, phased by the birth count so the texture stays on the particles.
  - One frame value for the whole ribbon; the fade is the age curve's alpha.
- **Sorting:** per emitter or group, farthest first, not per particle. Inside an emitter, array order; a dead particle is replaced by the last (0x005529A0).
- **The draw order** (`dadefaultsub.sub.xml`): opaque, characters, `SplatParticles` (sorted), vista, water, fading trees, distortion, distortion water, then `TransparentGeometry` (sorted farthest first: the transparent class, emitter groups, emitters not `sorted_emitter`), then fading geometry.
- **Blend and depth by `VFX.mat` family** (depth test LESSEQUAL and no depth write unless said; linear, wrapping samplers):

| Semantic | Blend | Alpha test | Note |
|---|---|---|---|
| Blend, ContactSheetBlend, Blend_Atm | SRCALPHA, INVSRCALPHA | off | no culling |
| Addv, ContactSheetAddv | SRCALPHA, ONE | off | no fog |
| VolTexBlend / VolTexAddv | as above | > 0 | 3D texture |
| AlternateDecalBlend / Addv | SRCALPHA, ONE (both, as read) | > 0 | lerp(decal, alt, TEXCOORD0.w) |
| *_DBA (soft) | as Blend / Addv | > 0 | `a *= saturate((sceneD − viewD) / mml_fDepthAlphaBias)` |
| Opaque, Chunky* | none | | depth write on, cull CW |
| ChunkyBlend / ChunkyAddv | as Blend / Addv | off | cull CW |
| *Splat | | | class `emittersplat` |
| Sorted | | | class `EmitterGroup` |

- **The pixel shaders** (elgara's `vfx0*.asm`):
  ```
  fogF = min(1, exp(-|P - g_vCameraPosition|^2 * mml_vFogParams[0].w))                          // VS
  blend: c = tex*vcolor; rgb = lerp(fog[0].rgb, c.rgb*mml_vUserColorMultiplier.rgb, max(fogF, fog[1].x)); a = c.a*UserMul.a
  addv:  c = tex*vcolor*UserMul; out = (c.rgb, min(c.a, max(c.r, c.g, c.b)))
  ```
  `mml_vUserColorMultiplier` is filled at 0x00541472 and 0x0054675A from a named user parameter (inferred: the emitter's `UserParamName`).
- Not read: weapon trails (`ObjectNodeWeapontrail`, 0x00AFE634), mesh particles (the Chunky semantics), an emitter group's order of its children.

### How an emitter spawns and moves its particles

**The emitter's fields** (the model's `nemt` node, loaded by 0x00592C60 into the blueprint):

| Field (GFF id) | Blueprint | Live (animatable, tag) |
|---|---|---|
| BirthRate 6011, BirthRateRange 6012 | +0x178, +0x17C | +0x33C (8), +0x340 (9) |
| Life 6013, LifeRange 6014 | +0x180, +0x184 | +0x344 (6), +0x348 (7) |
| ScaleRange 6015 | +0x188 | +0x368 (21) |
| InitialSpeed 6016, InitialSpeedRange 6017 | +0x18C, +0x190 | +0x34C (10), +0x350 (11) |
| Acceleration 6018 | +0x194 | +0x354 (12) |
| GravityMultiplier 6031 | +0x198 | +0x358 (13) |
| InitialRotation 6299, InitialRotationRange 6300 | +0x19C, +0x1A0 | |
| InitialRotationSpeed 6019, …Range 6020 | +0x1A4, +0x1A8 | +0x35C (14), +0x360 (15) |
| RotationalAcceleration 6021 | +0x1AC | +0x364 (31) |
| WorldAxisAcceleration 6294 (vec4) | +0x1B0 | +0x3C0 (32) |
| SpawnSpreadX/Y 6023/6024 (degrees, made radians) | +0x1C0, +0x1C4 | +0x36C (16), +0x370 (17) |
| MovementSpreadX/Y 6025/6026 (the same) | +0x1C8, +0x1CC | +0x374 (18), +0x378 (19) |
| InvEmitterMovementSpreadUpdateDelay 6022 | +0x1D0 | +0x37C (20) |
| the age map (`amap`) | +0x1D4 | |
| AgemapColorMultiplier 6279, AgemapScalex/yMultiplier 6280/6281 | +0x1E4, +0x1F4, +0x1F8 | +0x39C (28), +0x3AC (29), +0x3B0 (30) |
| FlipbookType 6189, Rows 6181, Columns 6182, RandomStartFrame 6183, FramesPerSecond 6180 | +0x200 … +0x204 | |
| OrientationBehavior 6037, ParticleInheritance 6038, EmitType 6234 | +0x16C, +0x170, +0x174 | +0x334, +0x338, +0x414 |
| TargetAttraction 6185, TargetRadius 6186 | +0x280, +0x284 | +0x390 (22), +0x394 (23) |

- **The flags** are bits:
  - +0x130: PhysicsEmitter 6239, PhysicsObjectSpawn 6243, BirthrateInParticlesPerMeter 6028, AffectedByWind 6030, FollowPath 6032, LinkParticlesTogether 6033, (a spawn volume), SpawnWithinVolume 6045;
  - +0x131: InvertSpawnVolumeNormals 6046, UpdateOnlyWhenVisible 6034, EnableCollisions 6035, InheritVelocityInsteadOfPos 6036, NormalsAsDirection 6291, ObjectSpaceAcceleration 6298, (a target), SpawnDirectionTracksTarget 6187;
  - +0x132: KillParticleWhenTargetHit 6188.
- **An age-map element** (`amel`, 0x20 bytes, 0x0058D0A0): PercentLifeElapsed, the reciprocal of the gap to the previous element, Color RGBA, ScaleX, ScaleY.
- The animation drives the live values by the tag of the node's export (slot 14, 0x0053DAD0).

**The update** (slot 34: `Sprite<SpriteParticle>` 0x00552CF0), once a frame, one step:
- It is skipped where UpdateOnlyWhenVisible and the model is unseen; the particles don't age then.
- The emitter's previous and current world position and rotation are kept (0x00542440).
- `UpdateParticles(dt)` (0x005528E0), then, while the model's emit bit is set, `SpawnCount` particles are spawned (0x00552AF0, at most 256).

**The spawn count** (0x0053EC80, checked; randS in [−1, 1], rand01 in [0, 1]):
```
if burst (EmitType 1): if armed(+0x410) { armed = 0; return trunc(rate + randS*range) } else return 0
if rate <= 0: acc = 0; return 0
if perMeter: acc += |cur - prev|; r = rate + randS*range; if r > 0 { n = trunc(acc*r); acc -= n/r; return n }
elif range == 0: acc += dt; n = trunc(acc*rate); if n > 0 acc = 0; return n     // the remainder is dropped
else: x = (rate + randS*range)*dt; return x >= 1 ? trunc(x) : (rand01 <= x ? 1 : 0)
```

**A particle's birth** (0x00552AF0, 0x0053F980, 0x0057C520; 0x80 bytes: position, velocity, age, 1/life, scale, spin, frame, … angle at +0x70):
```
pos   = prev + (cur - prev) * rand01          // perMeter with linked particles: (i+1)/n; or a spawn volume (0x00560010)
life  = Life + randS*LifeRange; if life <= 0 life = Life;  invLife = 1/life; age = 0
spin  = RotSpeed + randS*RotSpeedRange        // revolutions a second
scale = 1 + randS*ScaleRange
frame = RandomStartFrame ? rand01 : 0
angle = InitialRotation + randS*InitialRotationRange
dir   = the node's local +z in the world (or a volume's normal, or toward the target)
dir   = rotate(dir, emitter X, randS*SpawnSpreadX); dir = rotate(dir, emitter Y, randS*SpawnSpreadY)
vel   = dir * (Speed + randS*SpeedRange)      // + the emitter's velocity with InheritVelocityInsteadOfPos
```
A sphere volume (0x0053E380): radius × (rand01 when within, else 1), the direction (0, 0, 1) turned by yaw rand01·360 and pitch rand01·180, which isn't uniform. Cylinder (0x0053E540), box (0x0053E790) and mesh volumes weren't read.

**Motion** (`SpriteParticle::Update` 0x0057BF30; h = dt, T = the age after the step):
```
dv  = Acceleration*h * v + W*h               // Acceleration along the velocity; W world-axis (or object-space) acceleration
v'  = v + dv
p'  = p + h*(v' + 0.5*h*dv)
p'.z += -9.8*Gravity*0.5*(T^2 - (T - h)^2)    // gravity never enters v
```
- A target pulls `dir·attraction·h`; within its radius, scaled by (1 − d/r). A particle about to pass the target with KillWhenHit dies.
- Wind isn't applied on this path.

**Ageing** (0x005528E0): `age += dt` while age ≥ 0. A particle dies when `age < 0 || age·invLife ≥ 1`, and the last particle takes its slot. Then `frame += fps·dt/(rows·cols)`, `spin += spinAccel·dt; angle += spin·dt·2π`. With probability `invDelay·dt` the velocity is turned by `randS·MovementSpreadX/Y` (0x0057C340).

**The curves:**
- **Over a particle's life, the age map** (0x005607C0): linear between elements, flat beyond the ends. There is no Hermite, table or second curve.
- **Over the effect's time, the clip's channels** (`<export>`, e.g. `rays_birthrate`), bound to the tags above:
  - Time is quantised to u16 of the clip's length.
  - The keys are found from the last one used (0x00818490) and blended linearly (floats 0x00811330, colours 0x00811B20, vectors 0x00811A60, rotations 0x004CBC10).
  - A channel naming no node is ignored.

**Space** (0x00542440, 0x0053EBC0): particles are kept in world space. With `ParticleInheritance` ≠ 0 (and not InheritVelocity), each step carries them with a frame: `p = R_now·R_prev⁻¹·(p − framePrev) + frameNow`, the velocity turned too. The frame is 1 the emitter node, 2 its parent, 3 the model's root; 4 and 5 weren't read.

**Random numbers:** MSVC's LCG (`s = s·0x343FD + 0x269EC3; r = (s >> 16) & 0x7FFF`), three global streams (0x00C31994, 0x00C31A6C, 0x00C31FC4), never seeded and shared by every emitter. So the distributions can be matched but not the sequence (checked at 0x0053DF30).

**Time:** dt = min(now − last, 0.1 s) × the time scale (0x008107F0), 0 while paused unless the model animates when paused, × the model's speed. A presimulation runs max(1, trunc(T / 0.0666)) steps of 0.0666 s (0x00AF8A3C), T the model's `MMHEmitterPresimulateTime` (6333); its caller wasn't traced.

### The wisp, end to end

- **Spawn:** `creature_core` applies `vfx_base` 3006 innate. `fxc_wisp_c`'s `CrustHook1` subtree hangs on `cn_wispa`'s `CrustHook1`, under the `Root` bone 2 m up. The dummies `NewDummy_12` (rotation animated) and `NewDummy_14` (translation animated) carry it.
- **While it lives:** the `vfxduration` clip (`dura.ani`, 1 s, looping) plays.
  - `rays` spawn 10 a second, life 1 s, speed 0, ScaleRange 0.5, spin range 0.1.
  - `glow` spawn 7 a second, life 1 s.
  - Both have Inheritance 1, so they follow the moving hook. Each particle is born at its emitter node's origin.
  - Two point lights, radius 2.
  - The particles are sprites drawn by their materials' family (the table above).
- **At death:** `vfxcessation` (`cess.ani`, 3.33 s, once) ramps `rays` 10→0 and `glow` 7→0 between 0.30 and 0.31 s of its length (keys 19660 and 20315 of 65535: 1.0 and 1.03 s of its 3.33, just after the event below; the file section's reading), and fades the colour multipliers and lights.
  - Its `.evt` fires event 16 at 0.97 s naming `expiration` and `expirationsmoke`. That arms those burst emitters (`Object` slot 64, 0x00549800; event 16's route there is inferred), which spawn 1 and 3 particles with Inheritance 0, left where they were born.
  - Then the effect is destroyed.
- **While paused:** the wisp's `PauseAnimate` is 0, so its effect freezes.

### An effect a level places (read 2026-10-20 by lib-agent-2)

Read in the files (ghilana, `tools/gff4.py`) and the GOG exe, for DAOParagonEngine's tower flames. [C] checked, [I] inferred. Nothing seen in play.

- **What they are** [C]: a room's `.rml` `EnvRoomModelList` places effect models as it places meshes. The Circle tower's candle flames are `fxe_fire_cnd_p` and its statue bowls' fires `fxe_fire_small_p`. Nothing in the `.are`, the `.arl` or a script starts them, and the candle and statue meshes (`prp_cand0x_0`, `prp_candlehold01_0`, `prp_elfstatue03_0`, `prp_steshelf01_0`) have no emitters of their own. The chargen stage's `fxe_cha_sel_p` (`dev/CHARGEN.md`) is placed the same way.
- **Counts** [C]: `bhm200ar_tower_level_2` (layout `lak304d`) places 241 `fxe_fire_cnd_p`, 19 `fxe_fire_small_p` and 22 `fxe_sunbeam_w_p`. `bhm700ar_tower_basement` (layout `lak302d`) places 61 `fxe_fire_cnd_p` and 14 `fxe_fire_small_p`, with `fxe_moonbm_s_p`, `fxe_swampcloud_p` and `fxe_caustic_p`.
- **Where** [C]: a lit candle has three `fxe_fire_cnd_p` (scale 1.0 to 1.5) 0.7 to 1.0 m above its origin, e.g. `prp_cand02_0` at (-18.33, -38.89, 0.82) in `lak304d_141` with fires at about (-18.3, -38.9, 1.53 to 1.78). A statue has one `fxe_fire_small_p` about 2 m above its origin, e.g. `prp_elfstatue03_0` at (31.15, -50.91, 1.63) in `lak302d_1103` with its fire at (30.77, -50.98, 3.65). A placed model's level position is the room's position plus its orientation times the model's (`Level.h`'s `InLevel`); only `lak304d_147` is turned. The torch sounds are the area's own `SoundList` entries beside the fires.
- **Who loads them** [C]: the `.rml` handler (0x005CEA80) loops over every record (0x005CEC60 to 0x005CED21), makes each an object (0x005CC5D0) and sets it up with `0x0053A510(scene, object, record)` (at 0x005CECEE), with no filter on the model's name or kind. 0x0053A510 sets the position, orientation and scale (when not 0), loads the model (0x00537F90), applies `EnvModelCutAwayOverride` (record +0x13C: 1 passes 0 and 2 passes 1 to slot 0x148), hands the user-param map to slot 0x138 (0x005462F0, at 0x0053A759), which gives it to each node's slot 12 (0x005413E0, above), and sets the bounds (slot 0x94). Added to the scene, the effect is presimulated (one step of 0.0666 s for these, presim 0.0333 s).
- **Each frame** [C, the visibility's meaning I]: an emitter's update (slot 34, 0x00552CF0) skips when `[0xC31650]` slot 0xF8 answers true, the emitter has `UpdateOnlyWhenVisible` (all of these do) and the object's flags (`Object+0x260`, 0x0054F850) lack 0x20. The flags start as 0x4001 (0x0054FCAE). So vanilla's flames neither age nor spawn while culled, and with lives of 0.4 to 1.0 s they are full a second after they come into view; a port that updates them always looks the same.
- **`fxe_fire_cnd_p`** (patch002.erf) [C], four emitters under `NewDummy_5`:
  - `Flame01` and `flame02`: `dotblur_add`, 5 and 4 a second, life 1.0, linked ribbons (`LinkParticlesTogether`), orientation 3, speed 0.1, spread 2/2, an age map of three keys with colour (0.8, 0.3, 0.1) and alpha 0, 1, 1, scale multipliers 0.5.
  - `glow`: `dotblur_add`, 10 a second, life 0.4, orientation 0, the quad on the wick.
  - `NewEmitter_4`: `Sum_Conj_03`, birth rate 0 in model and clip, so it never emits.
  - Its tree's `vfxduration` clip keys every value as the model has it, so the clip changes nothing visible. No user parameters on its records.
- **`fxe_fire_small_p`** (patch002.erf; no tree, no clip) [C], four emitters under `fire_dummy`:
  - `Glow`: `dotblur_add`, 7 a second, life 0.4, orientation 0, +0.314 in z, age-map colour multiplier alpha 0.1, scale multipliers 1.25.
  - `FireMeat_4`: `fx_fireringfb_add`, a 4 × 4 flipbook at 24 fps, 5 a second, life 1.0, speed 0.5.
  - `Flame`: `fx_firetorchfb_add`, a 4 × 8 flipbook at 24 fps as a linked ribbon, 7 a second, orientation 3.
  - `Distortion_8`: `fx_dis_noise02`, a distortion sprite.
  - The first three name the user parameter `fire`, and every placed `fxe_fire_small_p` record sets `fire` = (1, 1, 1, 1). A value a record lacks defaults to (0, 0, 0, 1), which would tint them black.
- **No light in them** [C for the models]: neither fire model has a light node, so `src/fix/LightFlicker` does nothing to them; a candle's light on the walls is a separate light-only model's.
- **Core** does not yet read a placed model's `EnvModelUserParamList` (3506: `EnvModelUserParamName` 3507, `EnvModelUserParamValue` 3508, a vec4) into `level::Placed`: DAOLib #212.
- Not read: who sets the object's flag 0x20 and what `[0xC31650]` slot 0xF8 answers, so whether a fire is presimulated at room load or only once seen; the record's animation and tree path in 0x0053A510, and slots 0x94 and 0x148; which `EnvModelCutAwayOverride` value (0 and 2 seen) hides a model under the camera's cut-away, and whether fire models obey it; whether a level's rooms load all at once or room by room, and what `EnvRoomVisibilityList` does to emitters beyond the cull; the two flipbook materials' shaders and textures.

### An effect's remote material and its emitters' attachments (the Spirit of Valor's ghost; read 2026-10-20 by lib-agent-2)

[C] read in the GOG exe or the files, [I] inferred. Nothing seen in play. For DAOParagonEngine's Spirit of Valor, the Harrowing's ghost: `vfx_base` 3007 "Fade - Ghost Crust" (`VFX_CRUST_GHOST`), model `fxc_fade_ghost_c`.

#### The remote material [C unless marked]

- **Loaded** by the model blueprint's loader (0x00595E90): Tint (6332) at +0xA0, DecalName (6327) at +0xB0, FresnelFalloff (6328, default 1) at +0x94, Alpha (6331, default 0) at +0xCC, InvertFresnel (6329) at +0x91. A model has a remote material only when it has the Tint field (+0x90 set at 0x00596233).
- **Made** when the effect is instanced (0x00567F80 for a crust, 0x00568590 plain): a 0x40-byte object (constructor 0x00542F70) of tint, decal name, falloff, invert, alpha and its effect object, kept at the effect object's +0x2F4.
- **Laid on the target** by 0x00546350, from the effect manager (0x0079ECA0, ID 19707) through the owner's graphics object's slot 79 (0x00549970): every material instance of the target that has `mml_tRMPDecal` gets the decal as that texture, then the target's +0x2FC = 1 (0x0054645C) and +0x2F8 = the remote material. Character.mat's passes have the parameter (101 of them), and so do eye, face, hairalpha and weapon: body, armour, eyes, face, alpha hair and weapons take it; opaque hair does not. **Removed** by slot 80 (0x00546490): `mml_tRMPDecal` back to `Default_Black` and the pointer cleared. +0x2FC is never cleared.
- **The shader block** (vanilla Ch1.psh and Ch1Alph.psh, in the creature's own pass after lighting and before tint and fog), gated by b4:
  ```
  if (b4) {
      decal = tex2D(s11, uv0)                         // smpRemoteMaterialDecal
      x = saturate(abs(dot(N, V) / c23.y) - c23.z)    // falloff, invert 0 or 1
      w = abs((1 - c23.z) - x) * decal.a * c23.x * c22.w
      colour = lerp(colour, decal.rgb * c22.rgb, w)
  }
  ```
  - Colour only: alpha stays `texture.a × mml_fOpacityMultiplier` (c27). No second pass, no blend or depth change.
  - With invert 0 the weight is a rim, 1 − saturate(|N·V| / falloff): for the ghost's 1.2 that is 0.17 facing the camera and 1 at the silhouette. Invert 1 with a falloff under 1 is a centre glow.
- **The constants** (the provider for usage 0x1F `RemoteMaterialParams`, 0x004F8F50), from the target's +0x2F8:
  - c22 = (tint.rgb × the effect object's +0x2D4 rgb, its w);
  - c23 = (alpha, falloff, invert, 1);
  - with none, (1,1,1,1) and (0,1,0,1), a weight of 0.
  - What +0x2D4 is, inferred to be the effect's creation tint, is not read.
- **b4** (`g_bShaderControlBooleans[4]`, usage 0x15, provider 0x004F8BF0 through the switch 0x004F8A10) is the drawn object's +0x2FC; b5 is the global byte 0x00BF2300.
- **Clip channels 33 to 35 are bound live** (the name table 0x0055D9A0, the lookup 0x00542A50): RemoteMaterialTintColor, FresnelFalloff and Alpha write the remote material's own fields. Many effects have a static alpha of 0 and raise it from a clip. The ghost's clip keys nothing (its `dura` is 0.5 s with no channels, and no events), so its values are the model's: White.dds, falloff 1.2, invert 0, alpha 1, tint (0.95, 1, 0.6).
- **The White.dds the game takes** for it is an add-in's copy (the dao_prc_str and dao_prc_gib texture pack, higher in the order), not core's `gui.erf` copy, a 16 × 16 all-white texture.

#### The emitters' attachments (`emas`, `emat`) [C unless marked]

- **Loaded** by the emitter blueprint's loader (0x0058E9A0 to 0x0058EB18) into 0x24-byte records at the blueprint's +0x270: AttachmentType (6246), SpawnOnSurface (6342), UseNormalForVelocity (6343), Name (6247). The blueprint's +0x130 bit 1 says it has any.
- **Bound once, at the effect's attach** (0x0054DD80, the owner's slot 0xD4, from the attach chain above), for each emitter with records:
  - type 1: every node of the owner that has a physics actor (the owner's list at +0xB0, filled by `PhysicsNodeFunctor`);
  - type 4: a scene-level list (0x00548A70, the manager 0x0053F290's +0x1F8) [inferred: a level-wide source];
  - types 2 and 3 match by Name, which no file sets.
  - Bound by 0x005420F0, a record per shape of the actor whose `MMHShapeAllowEmitterSpawn` (6244) is set, its weight `MMHShapeVolume` (6240) [inferred, from the shape reader 0x00589DB0].
  - With nothing bound, the emitter emits as an unattached one. Detached by 0x0054E0C0.
- **Births** (the sprite emitter's update 0x00552CF0; the same for the linked sprite and the mesh emitter):
  - The count is unchanged: the birth rate, or per metre moved.
  - The count is shared out over the records by volume / total volume, with a fractional carry per record, each birth on a uniformly random shape of that record's actor. Each `emat` adds a full share, so two emats double the births.
  - SpawnOnSurface 1 takes a point on the shape's surface and its outward normal (0x0081D190; for a capsule, pushed out to the radius); 0 a point inside it (0x0081CE60), with no normal.
  - The points are world space, from the shape's current pose. UseNormalForVelocity 1 aims along that normal, 0 along the emitter's +z; with an initial speed of 0, as the ghost's, neither moves the particle.
  - A spawn volume, when the emitter has one, takes priority over the attachment.
- **The creature's own mesh is not touched.** For the ghost the shapes are the armour's ragdoll capsules (`hm_arm_masa_0.phy`: capsules under the chest, hips and collars, volumes 0.0038 to 0.038, AllowEmitterSpawn 1).
- **Core's `.phy` reader does not keep a shape's volume or AllowEmitterSpawn yet: DAOLib #213.**

#### Opacity, and the Valor's script [C unless marked]

- **No effect code writes opacity.** `mml_fOpacityMultiplier` is set only through `SetOpacity(mode, value)` (0x004F4900, the object's +0xB4 block, a mode owning it at a time), from three systems:
  - the object fade (mode 0, 0x00466930);
  - a CCObject's alpha (mode 4: +0x190, `SetAlpha` 0x00695060, applied through AnimBase slot 8, 0x00A61280; set below 1 by a creature's fade-in and hide and show);
  - an exterior chunk (mode 5, 0x005CC770, not read further).
  - Each takes the render-class override (0xB → 1, 0xC → 5, 0xD → 6) first.
- **The Valor's own data is not ghostly:** `bhm600cr_valor.utc` is a Human (`apr_base` 15) in massive armour, `ArmourSkinTint`, opaque. Its script (`bhm600cr_valor.nss`:39-46) applies 3007 permanently at spawn, and `AddItemVisualEffect(weapon and shield, 3007)`. That puts the effect, and its remote material, on each item's own graphics object (the longsword `w_lsw_ls01a_0` has crust hook 1).
- 3007 is also the Harrowing's spirit wolf's, `cir320cr_darkspawn_sp`'s, the Fade demon's in `arl300`, and Wynne's spell's.
- **The ghost's particles:** two emitters on crust hook 1, `Additive` (about 40 a second, life 1.25 s) and `Additive_1` (30 a metre moved, life 0.75 s), material `Sum_Conj_04` (VFX.mat, `ContactSheetAddv`, `smoke_ciga_f4.dds`, an 8 × 8 flipbook), each with one attachment of type 1, on the surface. The semantic's blend is inferred from its name.
- **Nothing found makes a lower body see-through:** no height or gradient term in the shaders' alpha, an opaque decal and opaque armour. The likeliest reading is the rim lerp on a leg, whose surface is mostly side-on to the camera, together with the additive particles [inferred].

#### Not read

- Who calls the slots 79 and 80, and exactly which event lays the material (attach or each frame); the effect object's +0x2D4.
- Attachment type 4's source; the box and sphere surface branches in detail; the random unit vector (0x00A3C9E0, 0x008187E0); where the blueprint's +0x130 bit 1 is set.
- Whether a humanoid's parts' `.phy` nodes all land in the creature's physics list, and so exactly which capsules the Valor has.
- VFX.mat's `ContactSheetAddv` states; the add-in's White.dds pixels; the render-class override pairs' meaning; mode 5's owner; b5's writer; stealth's look.

### Effects, the open questions: read 2026-10-18

Read in the GOG exe and the game's files by lib-agent-2 (with subagents), for DAOParagonEngine #28's nine questions on lib-agent-2's first effects reading (dev/EFFECTS.md). Not seen in play. Three parts, each a subagent's; spot checks noted in each.

#### Effects, open questions 1, 5, 7, 9 (vq1), read 2026-10-18

Read in the GOG exe (`tools\daoexe.py`, capstone sweeps; no Ghidra, no game) and the game files (ghilana, `tools\gff4.py`) by lib-agent-2's vq1 and its forks, following `lib-agent-2-vfx.md`. [C] read in the disassembly or data, [I] inferred. Each section ends with its own "Not read".

##### 1. A crust row whose model has no crust hooks, and abi_base `Crust_ID`

Read 2026-10-18 in the GOG exe (`tools\daoexe.py`, capstone sweeps) and the game's files (ghilana, `tools\gff4.py`) by lib-agent-2 (vq1). [C] read in the disassembly or data, [I] inferred.

###### A crust row is made only on an owner that has crust hooks

- **Create** (0x0079E8A0) [C]: the row's `CrustEffect` (row +0x0E) is read; for a crust row with no explicit crust target (5th argument) the target is the owner's graphics object (owner vtable +0x10C), and its id goes to the effect's +0x454.
- **Model load** (0x007EA550, then 0x007EA1C0 → 0x00A67860) [C]: with `CrustEffect` 1 and no crust target, nothing is loaded and the load returns false (0x007EA853–0x007EA87E). With a target, 0x00A68D50 → `Eclipse::IGraphicsInternal` slot 0x88 (0x005275C0) → `GraphicsStructuresManager` slot 0x14 (0x005733C0, marshalled to the graphics thread when called elsewhere) → slot 0x18, **0x00579EB0**, the crust-object maker. A non-crust row goes through 0x00A68B30 instead.
- **0x00579EB0** [C]: returns null (no object) when the crust target is null **or the target has no crust map** (`Object` +0x274 null; read through the +8 interface's slot 0x6C, 0x0054F800). Otherwise it finds or loads the blueprint by name and instances it through **0x00567F80**, which passes the crust target into the node instancing and sets the new object's **+0x278 = 1** (0x005680DE). That is the only write of 1 to +0x278; the constructor (0x0054F990) clears it, and the plain instancer (0x00568590) leaves it 0. On a null result 0x005275C0 retries once with the name plus `.mmh` (0x00AF6C70), through the same path.
- **The crust map** (`Object` +0x274) is made lazily by 0x0054EA50 (called from 0x005B3250) when the first `crst` node is instanced, a std::map from hook id to a vector of nodes. So an object has one only if its model has at least one `crst` node [C]. Humanoid bodies have them: `hm_arm_nuda_0.mmh` ids 1, 4, 8–11, 14, 15, 18, 21; `hm_glv_nuda_0.mmh` ids 2, 3, 12, 13, 16, 17 [C]. That a humanoid's parts' hooks all land in the one creature object's map is [I] (the abilities' `Crust_ID` 2 projectiles work on humanoid casters).

So **what decides whether a crust row draws is the owner, not the effect's model**: an owner with no `crst` node gets no effect at all from a crust row (nothing drawn, the load false) [C]; an effect model with no `crst` node is fine.

###### The attachment, for fxa_mana_conj_c

`fxa_mana_conj_c.mmh` (patch002.erf) has no `crst` node and no `MMHNodeCrustHookID` field: GOB, then emitters `NewEmitter_0` … at about 1 m from the origin in the XY plane (0, 0.91), (0, −1.01), (−1.08, 0.01), (1.05, 0.01), and under `NewDummy_5` (rotation animated) more of them, all at z 0 [C]. A ring at the feet.

- **0x0079D690** [C]: crust path when `CrustEffect` && the effect object's +0x278 (slot 0x80, 0x0054F810); both hold for a crust row made as above. It calls the owner's slot 0xDC, **0x00543BD0**(effect), and sets the effect's +0x440 = 1.
- **0x00543BD0** [C], `this` the owner:
  1. Only if the owner's crust map **and** the effect's crust map exist: sets the effect object's byte +0xEE = 1 (0x00542960; meaning not read), and calls 0x00562FE0 on the effect's map with the owner.
  2. **Always**, whatever step 1 did: `effect.vtbl[0xF0]` (0x0054D860)(owner, owner's node `GOB` (string 0x00AF83B8), "GOB", 0, 0): an `ObjectAttachDependency`-style attachment (0x50 bytes, 0x00542DD0, then 0x00568BB0) of the whole effect object to the owner's GOB node. Returns 1.
- **0x00562FE0** [C], the hook binding: `M` the effect's map, `N` the owner's (`owner+8` slot 0x6C). First 0x00562E50: for each id in `M` that both maps have, the two vectors must be the same length, else the binding is skipped entirely (return 0, not acted on by the caller). Then for each id of `M`, for each index i: if `N` has the id, a `NodeAttachDependency` (0x30 bytes, vtable 0x00AF837C, +0x2C the effect's node) is made from the owner's node `N[id][i]` and added to the effect's node `M[id][i]` (0x0057B940). An id the owner lacks is skipped, its subtree staying under the effect's GOB.

**For fxa_mana_conj_c on a caster:** the effect's map is null, so no hook is bound; the effect is attached whole at the caster's `GOB` and follows the caster's root transform (position, facing, scale), not any bone [C for the code path; the visual result, a ring at the caster's feet turning with them, I]. It is not a fallback to the plain-child path (0x0054DD80, slot 0xD4), which only non-crust rows (or an object without +0x278) take.

###### `Crust_ID`: the caster's hook a projectile is held at

- **Read once** [C]: the hash 0xC4D9FED5 is in the exe only at 0x006B71E6, in the projectile maker 0x006B6F60 (`CCCreature` vtable index 60, `ProjectileLoad`), and only for a command of type 0x19 (an ability; type 1, an attack, never reads it). It is kept on the projectile at **+0x4B4** (0x006B70CF); the caster's id at +0x4BC (0x006B70C5).
  - Same place: `abi_base.Flags` (0xD97C90C5) bit 0 set makes the ability use the weapon's projectile instead (jumps to the attack path at 0x006B6FF1): the archers' shots (`Flags` 1, `Crust_ID` 0).
- **Used by 0x007E6DA0** [C], the projectile's hold node: finds the caster by +0x4BC and its graphics object; with `Crust_ID` ≠ 0 the node is `caster.vtbl[0x7C]`(id), **0x00547D20**: the first node of that id in the caster's crust map (null when the caster has no map or no such hook); with 0 the caster's node `Prop01` (0x00B23A68, the right-hand weapon node).
  - **At load** (0x007E6F70 → 0x007E6FF7): the projectile's model is attached to that node (slot 0xF0, 0x0054D860), with the scale at +0x4F0; when the caster or the node is missing, it is placed at the position +0x4D0 instead (0x00695540).
  - **At release** (0x007E80E0 → 0x007E817A): the same node is looked up again, the model's world position and orientation are read (slots 0x34, 0x38) and the model is detached from the caster (slot 0xF8), so it flies from where the hook held it.
- **Not on the conjure effect, nor the caster's other effects** [C]: nothing else reads the column (`ConjureVFX` is made by 0x00743F50 without it); the crust effects bind by their own `crst` ids as above.
- The values [C, `abi_base`]: 514 rows 2 (Right Cast), 27 rows 0 (the archers' and traits'), 8 rows 5 (Head: the spits and breaths, `BROODMOTHER_RANGED_SPIT`, `MONSTER_STALKER_SPIT`, `MONSTER_HIGH_DRAGON_FIRE_SPIT`, the archdemon's), 2 rows 3 (Left Cast: `MONSTER_SPIDER_WEB`, `MONSTER_CORRUPTION_BURST`). `crusts.gda`: 2 Right Cast, 3 Left Cast, 5 Head.

###### Not read

- What the crust-target argument does inside the node instancing of 0x00567F80 (blueprint +0x44 slot 8), and the effect object's +0xEE flag.
- Whether a humanoid's part models' hooks are merged into the creature's one object (the map's owner) or kept per part; which hooks a Clothing chest's model (drawn with no gloves model) carries. A caster without the hook holds the projectile, by the code, at +0x4D0 unattached.
- 0x0054D860's dependency update per frame (that the attached effect follows each frame is inferred, as for the crust nodes).

##### 5. A projectile at impact: its model, its effects, its removal

Read 2026-10-18 in the GOG exe (no Ghidra, no game) by vq1's fork. [C] read in the disassembly or the data, [I] inferred.

###### The pieces

- **`CProjectile`** (vtable 0x00B23BB4, 114 slots; slot 113 = 0x007E9630 the per-frame update). The `CProjectileManager` update (vtable 0x00B0D928 slot 3, 0x00679D20) walks its projectiles: when `+0x444` ≠ 0 it calls slot 113 with dt; when `+0x444` = 2 afterwards, it removes it in the same pass (slot 0, 0x00679B90 → the object's scalar-deleting destructor, unless the id is in the manager's second set at +0x2C) [C].
- **The fields** (constructor at 0x007E84F0; set by the maker 0x006B6F60) [C]:
  - `+0x4A4` the `prj_base` row; `+0x4BC` the caster's id; `+0x4C0` the target's id (−1 for a location; replaced by whatever a swept collision hits); `+0x4C4` the event's receiver (the caster when −1).
  - `+0x4B8` = 1 when the command is 25 (use ability), 0 for 1 (attack).
  - `+0x4B4` = `abi_base.Crust_ID` (default 2): only the caster's hook the projectile is held at before release (0x007E6DA0: the owner model's slot 0x7C by the id; with id 0, the node named "Prop01" (0x00B23A68)). Nothing reads it at impact.
  - `+0x4AC` the row of an effect attached to the projectile. For an attack it comes from the launcher's item property type 10 (0x00702E90 on item +0x2E0). For an ability it stays 0, so **a spell's projectile has no attached effect**. `+0x480` is that effect's handle (−1 when there isn't one). It is made at load (0x007E6F70 → `VfxManager` create 0x0079EDC0, with the projectile as owner and a scale of 1).
  - `+0x484` the flying sound, `+0x441` "stuck in the target", `+0x448` the state, `+0x444` the alive/remove flag, `+0x474`/`+0x478`/`+0x47C` the impact and fade timers, all 0 at first.
- **`prj_base` (table 4 "Projectiles", PRJ_)**: its columns, with what the exe reads [C]:

| Column (hash) | Where read | Use |
|---|---|---|
| ModelName 0xFD8403D5 | row +0x0C (row loader 0x007E68C0) | the projectile's model; its tree is the `.anb` of the same name (e.g. `fxa_arc_bolt_p.anb`) [I by name] |
| Acceleration 0xFD480C46, MinSpeed 0xDB26D384, TopSpeed 0xFD919690 | +0x28, +0x2C, +0x30 | flight speed (0x007E9170) |
| ImpactTimeout 0x0796E13B | +0x34 | how long it stays after arrival, and whether it sticks in the target |
| StaticCollisions 0x73E07C85, DynamicCollisions 0x97CA5911 | +0x38, +0x39 | swept collision in flight (0x007E8C10) |
| Simulation 0xA2166177 | +0x3A | ballistic flight path |
| AlphaFade 0x7217A33B | +0x3B | fades the model out before removal |
| Mass 0x90FC93BB, InitialSpeed 0x883650AA | read at launch into +0x460, +0x464 (0x007E80E0) | |
| HideLaunch 0x06484B80 | at load (0x007E8700) | model alpha 0 until it is released |
| SoundFlying 0xE9D96D07 | at launch | the looping flight sound, `+0x484` |
| SoundMatType 0x35DB27AA | at a static hit (0x007E67F0) and at arrival (0x007E911A) | the impact sound's material |
| DamageType 0x5BFAE957, DamageBonus 0x90460F08 | **not in the exe** | (scripts', if anyone's) |

  - **There is no impact-effect or impact-sound column.** `ImpactVFX` and `ImpactSound` are not columns of `prj_base`, and their hashes aren't in the exe [C].

###### The states (`+0x448`, the update 0x007E9630's jump table at 0x007E969C) [C]

- **0 → 1, loaded and held:** at load (0x007E8700 → 0x007E6F70). `+0x444` = 1. Under `HideLaunch` the model's alpha is 0. Then 0x007E7FC0(1) turns the model's weapon trails on, and the attached effect is made if there is one.
- **2, flying** (0x007E9170). The first update after release (`+0x440` = 0) is the launch, 0x007E80E0:
  - The model is detached from the hand and its alpha set to 1.
  - The model's own tree plays anim 501 `vfxduration` (CProjectile slot 44 0x0069B3D0, args 501, 1.0, −1.0, 1).
  - The weapon trails go on, `SoundFlying` starts, and `+0x440` = 1.
  - Each later update moves it and ends the flight (0x007E90B0) when any of these holds:
    - the swept collision (0x007E8C10) hits a wall or an object other than the caster and the target. A static hit also plays the surface's impact sound through `SoundMatType` (0x007E67F0), and a hit object becomes the new target (`+0x4C0`; its attack result goes to 5 unless it is 3);
    - the step was under 1e-6 m (0x00B23A64);
    - (when not `Simulation`) it ends within 0.01 m (0x00B23A4C) of its destination.
  - At over 1000 m travelled (`+0x468` > 1000.0, 0x00AEBC00) it goes straight to removal (0x007E6B40) **with no impact event**.
- **The arrival, 0x007E90B0, in this order:**
  1. **The impact event** (0x007E89F0):
     - If a prepared event is held at `+0x4F4`, that is posted.
     - Otherwise, for an ability (`+0x4B8`), 0x007E71C0 builds a `CScriptEvent` of type **31 `EVENT_TYPE_ABILITY_CAST_IMPACT`**. It holds the receiver, the result's objects, the projectile's position `+0x120` as the impact location, and the objects hit.
     - For an attack, 0x007E87C0 builds type **29 `EVENT_TYPE_ATTACK_IMPACT`**, then posts the held events at `+0x4F8` unless the result is 0, 3 or 5.
     - Each is posted to the caster with `AIMaster::AddEventDeltaTime(0, 0, id, id, 10, event)`, a delay of 0.
     - **The spell's impact visuals come from the scripts handling that event** (`VFX_Impact0/1`, the damage-type effects), as already read.
  2. **Weapon trails off** (0x007E7FC0(0)), only when there is no attached effect (`+0x480` = −1). It collects the model's nodes of type 0x15, which are `ObjectNodeWeapontrail` (the list holds their +0x180 interface; 0x00A676F0 does the same for a creature's weapons). For each it sets +0x1C0 = +0x1C1 = 0, +0x1C2 = 1 and +0x1C3 = 0, and zeroes +0x1D4. So the trail stops growing and is let finish [I: +0x1C2 reads as "fading", from 0x005B1127's use].
  3. **The attached effect is stopped, not destroyed** (0x007E6860 → `VfxManager::Remove` 0x0079EAE0(handle, 0) → Find, then the stop 0x0079E3E0(effect, immediate = 0)) [C]:
     - If it has a `vfxduration` clip, cast (500) and impact (502) are stopped. The duration (501) is eased out through 0x00A5C840 (0.0, 3.0). Then it goes to `vfxcessation` (phase 4), or is destroyed at once when the tree has no cessation clip.
     - With no duration clip but an impact clip (0x007E9AD0 returns 1), nothing is done: the effect plays its impact out by itself.
     - Already ceasing: nothing.
  4. **Only if the target object exists:**
     - **Stuck in the target:** when `ImpactTimeout` > 0, the projectile's model is made a child of the target's model at the hit point (0x007E79D0, the owner's `Object` slot 0xF0, the same attach call the crust uses), and `+0x441` = 1.
     - **The impact sound:** the target's slot 54 (`CCCreature` 0x006A0220) gets the `SoundMatType` value. It reads the target's material from table 89 `materialtypes` (column 0xBB8AD13B), labels from 153 `sound_item_materials` and 154, and plays the sound (0x005B9020 → 0x005BC7F0) [C; how the event name is composed is I].
  5. **The flying sound is stopped** (`+0x484`, 0x005B9330(0)).
  6. **State 3.**
- **3, arrived** (0x007E6C60): `+0x474 += dt`. When `+0x474 ≥ ImpactTimeout`, the removal starts (0x007E6B40). With `ImpactTimeout` 0, as on every spell projectile read, that is the next update. **The model doesn't move after arrival:** only state 2 moves it.
- **The removal start** (0x007E6B40):
  - If the model's tree has anim 503 `vfxcessation` (0x00A5D920), it plays it (slot 44: 503, 1.0, −1.0, 1). `T` (`+0x47C`) = the clip's length in ms (0x00A5F7A0) × 0.001.
  - With `AlphaFade`, `T = max(T, 0.75)` (0x00B23A50).
  - State 4.
- **4, ceasing** (0x007E6CB0), each update `t = +0x478 += dt`:
  ```
  if AlphaFade and T - t <= 0.75:  alpha = (T - min(t, T)) / T      // the model's slot 0x20 (alpha, 0)
  if t >= T:  +0x444 = 2                                             // the manager deletes it in the same pass
  ```
  - As read, the fade divides by `T`, not 0.75. When `T` = 0.75 (an arrow, which has no cessation clip) it is a straight 1 → 0 over 0.75 s. With a longer cessation clip, alpha drops to 0.75/T at `T` − 0.75 s and then falls to 0.
  - With no cessation clip and no `AlphaFade`, `T` = 0, and it is deleted on the next update.
- **The deletion** (CProjectile's destructor 0x007E8600 → 0x007E7680):
  - If stuck, it is detached first (0x007E70E0).
  - Then **every effect whose owner or crust target is the projectile is destroyed at once**: 0x0079EC20(id, 0) → 0x0079DF10, with no cessation. That includes the attached effect if its cessation was still running.
  - The flying sound and the held event are freed. The model goes with the object, and its live particles with it [I: they are the model's].

###### What this means for the look

- **A spell bolt** (Arcane Bolt, `prj_base` 137 `fxa_arc_bolt_p`; ImpactTimeout 0, AlphaFade 0; tree `vfxduration` → `.dura.ani`, `vfxcessation` → `.cess.ani` of 1 s) [C from data]:
  - In flight its `vfxduration` loops.
  - On arrival it stops where it is. The next frame its own `vfxcessation` plays, the projectile's own impact flash. Its `.evt` fires event 16 at 0.03 s on `FireBase_Impact`, `Glow_Impact` and `shockwave_Impact` (burst emitters).
  - The clip ramps `firetrail`/`glow`/`glow_15` birthrates down and fades their colour multipliers and the light `newlight_13`. So the trail's live particles run down for the clip's 1 s.
  - At 1 s the object is deleted, with any particles still alive [I: vanish].
  - Meanwhile the caster gets `ABILITY_CAST_IMPACT` at once, and the scripts lay the impact effects (`VFX_Impact0/1` or the damage-type effect) as separate effects.
- **An arrow** (`prj_base` 1, ImpactTimeout 3, AlphaFade 1, no tree read):
  - On a hit target it sticks into the target's model at the hit point and follows it.
  - It stays 3 s, fades over 0.75 s, and is deleted.
  - With no target object (a location), it stays where it stopped for the same 3 + 0.75 s.
  - Its weapon trail is let finish from arrival. A launcher's enchantment effect (`+0x4AC`) goes to its cessation at arrival and is cut off when the arrow is deleted.

Not read: what the eased stop 0x00A5C840(501, 0.0, 3.0, 0) does to the clip (fade length or blend); the weapon trail's own fade once +0x1C2 is set (0x005B0F60 onward); when AddEventDeltaTime's delay-0 events reach the scripts relative to the frame; the `Simulation` path and the swept-collision ray (0x00648C60, 0x00648FC0) in detail; the miss scatter at load (0x007E76D0); who sets `+0x4F4`; the event name built in 0x006A0220; whether the manager's +0x2C set pools projectiles instead of deleting them.

###### 7. How `vfx_base`'s model is chosen: by the owner creature's `PerSpace`, and the effect scaled to it

**Short answer:** by the size of the creature the effect is on, and nothing else. The size is the owner's `apr_base` `PerSpace`. There is no quality setting, no gender and no random choice. The engine never reads the `ModelName` column; it reads `ModelName1..3` with `Size1..3`. It picks the entry whose `Size` is nearest the owner's `PerSpace`, then scales the model by `PerSpace / Size`. [C]

**The columns** (vfx_base, table 8; column hashes from `gda.column_hash`):

| Column | Hash | Read by the exe |
|---|---|---|
| ModelName | 0xFD8403D5 | **never for table 8** [C]: the hash's four hits are table 2 (`apr_base`, 0x0074AE97), table 0x5D (0x00750177, 0x00752A05) and table 4 (0x007E68E9) |
| ModelName1 / Size1 | 0xE2C28829 / 0x514CC359 | row loader 0x007EA02C / 0x007EA04C |
| ModelName2 / Size2 | 0xC9EFDBEA / 0x7A61909A | 0x007EA08E / 0x007EA0B3, **only when Size1 > 0** |
| ModelName3 / Size3 | 0xD0F4EAAB / 0x637AA1DB | 0x007EA0D9 / 0x007EA0FE, **only when Size1 > 0** |
| AlignToGround 0x4BC7021F, LoadGlobal 0x713FEF95, ShowInPaperDoll 0xF23D2AA1 | | nowhere in `.text` [C] |
| Priority 0x76E5BBCB | | read on demand by 0x007E9CA0(rowId) |

In vanilla's `vfx_base.gda` (993 rows), `ModelName` equals `ModelName1` in 991 rows. The other two rows (0 `INVALID_VISUAL_EFFECT`, 93004) have both empty. So an engine that reads `ModelName1` loses nothing. [C, data]

**The row loader** (0x007E9E00) keeps the three model names in an engine array at row +0xB4 ({ptr, count}, resized to 3 by 0x007CED20, 0x1C-byte MSVC8 `std::string` elements) and the three sizes at row +0xBC ({ptr, count} of floats, 0x00652430). The test `fldz; fcomp [Size1]; test ah,5; jp` skips ModelName2/3 and Size2/3 unless `Size1 > 0`. [C]

**Where the size comes from** (the manager's create 0x0079E8A0) [C]:
- `owner = objectManager[0xC327EC].Find(desc.ownerId)` (0x005D5BE0), then `size = owner->vfunc[0x9C/4 = 39]()`. With no owner object, size = 0.
- Slot 39 is `0x006A21A0` for `CCCreature`: `[this+0x7B0] ? [[this+0x7B0]+0x60] : 0`. That is the creature's `PerSpace` from `apr_base` (dev/TURN_BASED.md: +0x7B0 is the size record, `PerSpace` at +0x60, `Height` at +0x64).
- Every other client class has slot 39 = 0x0083A430 (`fldz; ret`). These are CCObject, CCPlaceable, CCItem, CCAreaOfEffectObject, the triggers, the sound objects, CCWayPoint and CCVisualEffect. So **only creatures have a size**. Effects on placeables, AoE objects or a location get size 0.
- The size is passed to 0x0079DB90, either directly (0x0079EA7C) or through the deferred job (vtable 0x00B21124, kept at +0x2C, run 0x0079ED10). Either way it reaches the model load 0x007EA550 as its third argument.

**The choice** (0x007EA550, 0x007EA690–0x007EA969) [C]. The x87 reverse-op bytes were checked: `DE E2` = FSUBRP st(2) = st0 − st2, and `DE F1` = FDIVRP st(1) = st0 / st1.
```
name = M[0]; chosen = S[0]                      // M = ModelName1..3, S = Size1..3
if size > 0 and S[0] > 0:
    n = number of leading non-empty names in M  // stops at the first empty one (string length +0x14)
    if n > 1:
        if size <= S[0]: keep M[0]
        else for i = 1 .. n-1:
            if size <= S[i]:                    // between S[i-1] and S[i]
                d1 = size - S[i-1]; d2 = S[i] - size
                (name, chosen) = d2 < d1 ? (M[i], S[i]) : (M[i-1], S[i-1])   // a tie takes the smaller
                break
            if i == n-1: (name, chosen) = (M[i], S[i])                      // larger than every size: the last
name = trim(name)                                // 0x004C3310 with " \t" (0x00ADEE0C); the model is loaded by that name
ratio = (size > 0 and chosen > 0) ? size / chosen : 1        // 0x007EAA13-0x007EAA3C
model scale = desc.scale (+8, EFFECT_FLOAT_SCALE) * ratio    // 0x007E9C40 -> the model's vtable +0xAC
```
- **The scaling applies even with a single model.** In vanilla, 31 rows have `Size1` 0.35 and no ModelName2: `fxa_berserk_c` on every monster rage crust, `fxa_ent_par_c`, `fxa_ent_slw_c`, `fxa_ent_dth_c`. So these grow with the creature's `PerSpace`. On a giant spider (0.70) the berserk crust is drawn at 2×; on a child (0.30) at 0.857×. [C code, the sizes inferred from TURN_BASED.md's `PerSpace` values]
- **Rows with three sizes:**
  - The selection rings: 30005/30006 `SR_PTY[t]`, 30011/30012/30025/30026 `SR_EMY*`, at sizes 0.35 or 0.40, 1.3 and 2.1, with `_M` and `_L` models.
  - 4007 "Fire - Torch": `fxe_fire_small_p`, `_medium_p`, `_large_p` at 0.35, 1.3 and 2.5.
  - Two worked cases, with the ring sizes 0.35, 1.3 and 2.1. At PerSpace 0.70: d1 = 0.35, d2 = 0.60, so `SR_PTY` is drawn ×2.0. At PerSpace 1.0: d1 = 0.65, d2 = 0.30, so `SR_PTY_M` is drawn ×0.77. [I, arithmetic on the code above]
- **Inferno 90144** (`fxa_fire_aoe_p`, Size1 0.2, Size2/3 1 and 2 with empty names): n = 1, so only the first model is ever used. It is applied at a location or AoE object, so size is 0 and the ratio is 1 [I: where the scripts apply it].
- **Outside Q7, seen in the same function:** 0x007EA850–0x007EA880 returns false (no model) when the row's `CrustEffect == 1` and the crust-target argument (0x007EA550's second argument, [esp+0x24]) is 0. A crust row created with no crust target loads nothing [C]. That bears on Q1.

**Not read:** what table 0x5D is (it reads a `ModelName`), and whether the model's vtable +0xAC scale is uniform (taken as the object scale +0x5C that the sprite fill squares).

##### 9. The emitter presimulation: its caller, its time, and what it covers

Read in the GOG exe (capstone, `daoexe.py`, direct-call and vtable scans), and the models through ghilana. [C] read in disassembly or data, [I] inferred.

###### The presim is the emitter's own update, run in a loop

- **Every emitter kind's update is the one entry point** [C]. It is vtable slot 34 (+0x88), `Update(bool presim, float T)`, `ret 8`:
  - `Sprite<SpriteParticle>` 0x00552CF0 (also `SpriteLinked<SpriteParticle>`);
  - `Sprite<PhysicsSpriteParticle>` 0x005534C0 (also `SpriteLinked<PhysicsSpriteParticle>`);
  - `Mesh<MeshParticle>` 0x00583F80.
  - All three read the step constant 0x00AF8A3C (float 0.0666) and divide by 0x00AF8A40 (double 0.0666).
- **The step count** [C], at 0x00552D84 to 0x00552DF6:
  ```
  if presim:
      if T < 0: T = model.blueprint+0xD0     // owner Object (+0x154) slot 0xAC -> Object+0x40 -> [ ] -> +0xD0
      steps = max(1, trunc(T / 0.0666))       // fistp with truncation, unsigned compare against 1
      dt    = 0.0666 (float)
  else:
      steps = 1
      dt    = clock(0x008107F0).slot 0x10(!(flags>>13 & 1)) * model speed (+0x2E8, double)
  ```
- **Each step** [C] (loop 0x00552F93 to 0x00553481):
  - the inheritance carry (0x0053EBC0);
  - `UpdateParticles(dt)` (slot 40, 0x005528E0);
  - while the owner's emit bit is set (`Object+0x260 & 1`, read through slot 0xA4 at 0x005530E7), the spawn count (0x0053EC80) and the spawns.
- **The positions are fixed for the whole run.** The previous and current world positions are taken once, before the loop (0x00542440(0) at 0x00552D7F). So a per-metre emitter spawns nothing in a presim [C], and every presimulated particle is born on the segment between the two positions, which here is a single point [I].
- **After a normal update only**, slot 43 (0x00550260) runs [C]. It is the vertex fill (slot 44, 0x00550860, or slot 45 by +0x510). A presim builds no vertices.
- **The gates a presim still passes through** [C], at 0x00552D00 to 0x00552D57:
  - The emitter is skipped (no ageing, no spawns) when the global `[0xC31650]` slot 0xF8 is true, the emitter has `UpdateOnlyWhenVisible` (+0x310→+0x131 bit 2), and the owner's flag 0x20 is clear.
  - Flag 0x20 is most likely "was seen" [I], so an `UpdateOnlyWhenVisible` emitter is probably not presimulated when it is added.
  - It is also skipped when its opacity +0x4E0 ≤ 0. The opacity is initialised to 1.0 (0x00542018) [C].
  - The owner's flags are initialised to 0x4001 (0x0054FCAE, in `Object`'s constructor). So emission is on at add time [C].
- **The animation is not stepped** during the presim [I: nothing in the loop advances the model's clips]. The particles are run with the emitter's live values as they stand when the effect is added: the blueprint's, since no `vfxcast`/`vfxduration` clip has been applied yet.

###### Who turns it on: being added to a scene

- **`Object` +8 vtable (0x00AF8434) slot 85 (+0x154) = 0x00548320**, `PresimulateEmitters()` [C]:
  ```
  T = (Object+8)+0x38 -> blueprint +0xD0          // MMHEmitterPresimulateTime
  if T > 0: for each emitter e in (Object+8)+0x9C..+0xA0: e->Update(true, -1.0f)   // -1 (0x00AE2E00): "use the model's T"
  ```
  - There is no "done" flag. It runs whenever it is called.
  - It covers every emitter in the object's emitter list, of every kind.
- **The only graphics-side caller of slot 85 is the scene's add, 0x0053A260** `(Object* obj, bool flag)`, `ret 8`. It ends at 0x0053A338 with `obj->slot85()` unconditionally, whatever `flag` is [C].
  - The scene's add has no guard against an object already added. Each add presimulates again [C].
  - Its `this` is the scene. `Graphics::ISceneInstance` slot 2 (0x0053A3D0) dispatches event 2 to this add and event 1 to the remove 0x005395B0 [C].
- **A visual effect reaches it at creation, in the attach** [C]:
  - The manager's create (0x0079E8A0) leads to the attach 0x0079D9F0, either directly (0x0079DB90 → 0x0079DD2D) or through the task posted when the effect must wait (0x0079D360; its run 0x0079ED50 → 0x0079EDA1).
  - The attach first calls 0x0079D410, which picks the scene:
    - by default the current one (`[0xC31650]` slot 0x24);
    - otherwise the owner's scene: the owner's model, its `AnimBase` (0x00A5CBA0) or +0x62C.
  - 0x0079D410 then calls `CCVisualEffect` slot 65 (+0x104, 0x007E9AB0). That gets the effect's `AnimBase` (+0x70 subobject slot 0x60) and calls its slot 1.
  - `AnimBaseVFX` slot 1 (0x00A5CB70, `SetScene`) stores the scene at +0x14 and calls `0x0053A260(scene, model+8, 1)`. That runs the presim.
- **The order inside the attach (0x0079D9F0)** [C]:
  1. scene add, and with it the presim (0x0079DA28);
  2. the owner attach, crust or plain child (0x0079D690);
  3. the placement at the owner's position when there is no owner model (0x0079D580);
  4. the phase start (0x007EAC30), skipped for a held effect (+0x448 == 1);
  5. +0x448 = 3.
- **What that order means:**
  - The presim runs once, at creation, before the effect is hung on its owner's hook and before its `vfxcast` phase starts.
  - A held effect (the conjure) is presimulated at creation, not when 0x0079D810 or 0x0079FD00 starts it. A search of direct calls from 0x0079D810, 0x0079FD00, 0x007EAC30 and 0x007EAB50 reaches no scene add and no slot 85 [C for direct calls; virtual calls not followed].
  - Particles presimulated before the crust or child attach are born at the effect's placement at that moment. For a located effect that is the position the create was given (0x007E9A10, set in 0x0079E8A0 before the attach) [I].
  - The node attach (0x0054D860) then resets each emitter's previous position to its current one (0x00542440(1) at 0x0054DA6B and 0x0054DA7E). So inherited particles are not dragged by the jump to the hook [I].
- **Other paths into the scene's add** (direct calls to 0x0053A260; every one presimulates its object's emitters when the model's T > 0) [C for the calls, I for what each is]:
  - **Level loading.** 0x0053A510, called from 0x005CEA80 (which handles `.rml`, the room layout), 0x005CC770 and 0x005D2970. These place the area's props and environment effect models such as `fxe_fire_*_p`, `fxe_sunbeam_*` and `fxe_water_fall_*` [I].
  - **A client object's model added to the scene.** 0x007E36B0, from the per-object state machine 0x007E3D30 (+0x94, eight states), which `GameModeDialog` and `GameModeCutscene` slot 8 call. It uses 0x0053A260 directly on the object's model (+0x154), or `AnimBase` slot 1 on +0x1E0.
  - **`AnimBase` slot 1** in every `AnimBase*` class, and 0x00A5C7F0, which adds a model when its flags are 0 (from 0x00A5D678 and 0x00A673A9, the weapon and gore parts).
  - **A model swap.** 0x0054E180 removes the object (0x005395B0) and adds it again (0x0054E23B), from 0x00A689AB. That presimulates again.
  - **The rest:** `Object` slot 46 (0x00545830, a sub-object at +0x250); 0x005C88E0; 0x00602B70 (a resource-loading path); 0x007BB890 and 0x007C0190 (`CharGenScreenshot`); 0x007F410A.

###### The time

- **`MMHEmitterPresimulateTime`, GFF field 6333**, is a top-level float of the `.mmh`. The model loader keeps it in the blueprint at +0xD0 (0x00596239 → `fstp [ebx+0xD0]` at 0x00596268), and only when the field is present [C].
- **Steps:** `n = max(1, trunc(T / 0.0666))`, each of 0.0666 s. A model with 0 < T < 0.0666 still gets one step of 0.0666 s (`fxe_fire_*_p` at 0.0333) [C].
- **In vanilla's files** (ghilana, 11,590 `.mmh` read; 452 in enciphered add-ins not searched) [C]:
  - 11,149 have no field and 386 have 0.0.
  - 55 have T > 0, all in `patch002.erf` and all effect models:
    - 2.0 (30 steps): `fxe_cloudmirror_p`, `fxe_dust_wnd_p`, `fxe_fade_cloud`, `fxe_forestfuzz_p`, `fxe_moonbm_s_p`/`_w_p`, `fxe_snow_ceiling_p`, `fxe_sunbeam_large`/`_lm_p`/`_orz`/`_s_p`/`_w_p`, `fxe_tree_beam_sharp`, `fxe_water_fall_up`, `fxe_waterfall_redcliffe`.
    - 0.5: `fxe_ash_light_p`, `fxe_snow_light`, `fxe_snow_light_p`.
    - 0.333: `fxe_water_spray`, `water_fall_up`.
    - 0.2667: `fxe_fire_kc_p`.
    - 0.2: `fxe_fire_large_p`.
    - 0.1667: `fxc_lotf_c`, `fxc_withfn_c`, `fxe_fire_medium_p`, `fxe_water_fall_01_p`.
    - 0.1: `fxe_fade_wallfog_p`, `fxe_fde_wallfog_p`, `fxe_snow_windy_p`, `fxe_water_fall_02/03/04/05/08_p`, `fxm_mist_clo_p`, `fxm_storm_clo_p`.
    - 0.0667: `fxa_pri_e_aura_c`, `fxe_rain_heavy_p`, `fxe_sa_torchglow_p`, `fxe_water_fall_06/07/09_p`, `fxm_rain_heavy_p`, `fxm_sa_torchstay_p`, `fxm_torch_side_p`.
    - 0.0333: `fxe_fire_cnd_p`, `fxe_fire_l_ns_p`, `fxe_fire_lrg_p`, `fxe_fire_m_ns_p`, `fxe_fire_med_p`, `fxe_fire_small_p`, `fxe_fire_trch_ost_p`, `fxe_fire_wall_p`, `fxp_fire_barrica_p`, `fxp_loot_c`.
  - So it is almost entirely the environment's ambient emitters (fire, fog, sunbeams, waterfalls, snow and rain), with three crusts (`fxc_lotf_c`, `fxc_withfn_c`, `fxp_loot_c`) and one ability aura (`fxa_pri_e_aura_c`). No spell's conjure, cast or impact effect presimulates.

###### For a port

When a model with T > 0 is first placed (an effect created, or a level's or object's model loaded into the scene), run each of its emitters for `max(1, trunc(T/0.0666))` steps of 0.0666 s:
- at its placement then, emitter not moving;
- with the blueprint's values;
- with emission on;
- with no animation;
- with no vertices built.

Then start normal updates. Skip emitters with `UpdateOnlyWhenVisible` (vanilla does when the object isn't yet seen and the global switch is on).

Not read:
- what the global `[0xC31650]` slot 0xF8 is;
- whether `Object` flag 0x20 is "seen last frame";
- 0x0053A260's `flag` (+0xC8/+0xCC: the spatial structure);
- whether an object's emitter list (+0x9C) holds attached children's emitters;
- the per-object state machine 0x007E3D30's states;
- whether a level's props are added through 0x0053A510 for every room;
- virtual calls from the effect start (0x0079D810, 0x0079FD00).

##### Not read

The sections' own lists above. Across them: what the crust target does inside 0x00567F80's node instancing; whether a humanoid's part models' hooks share one crust map; the per-frame update of the attach dependencies; the eased stop 0x00A5C840; the weapon trail's fade; the virtual calls out of the effect start (0x0079D810, 0x0079FD00).

##### Sprite questions 2, 3 and 8: read 2026-10-18

Read in the GOG exe (`DAOrigins.exe`, base 0x400000) and the game's files (vanilla `vfx_base.gda`, `vfx.mat.xml`, 600 `fx*.mmh` from `packages/core`) by a lib-agent-2 subagent (prefix vq2). Not seen in play. [C] read in the disassembly or data, [I] inferred.

###### 2. The age map's two scales: ScaleX is axis A, ScaleY is axis B

- **The loader** (`amel`, 0x0058D0A0) [C]: 6040 `PercentLifeElapsed` → +0x00; 6041 `ScaleX` → **+0x18**; 6042 `ScaleY` → **+0x1C**; 6043 `Color` (vec4) → +0x08. (+0x04 is the reciprocal gap, filled later.)
- **The multipliers** [C]: the emitter loader (0x00592C60) reads 6280 `AgemapScalexMultiplier` → blueprint +0x1F4 (0x0059336F) and 6281 `AgemapScaleyMultiplier` → +0x1F8 (0x005933A0), both defaulting to 1.0 (`fld1` before each read). The live copy (0x0053F032, also 0x00560BBF/0x00560FC5): +0x1F4 → **+0x3AC**, +0x1F8 → **+0x3B0**. 6279 `AgemapColorMultiplier` (blueprint +0x1E4..0x1F0) → +0x39C..0x3A8.
- **The evaluator** 0x005607C0 is `AgeMap(out rgba, out sX, out sY, map, t, colourMul*, mulX, mulY, size)`, `ret 0x24` [C]:
  ```
  sX = lerp(prev.+0x18, cur.+0x18, f) * mulX * size      // amel ScaleX, live +0x3AC
  sY = lerp(prev.+0x1C, cur.+0x1C, f) * mulY * size      // amel ScaleY, live +0x3B0
  rgba = lerp(prev.Color, cur.Color, f) * colourMul      // live +0x39C
  ```
  `f = (t − prev.t) · cur.recip`; beyond the ends the end element is copied (0x0056093A).
- **The sprite fill** (0x00550860) [C]: the call at 0x00550A9F passes `mulX = E+0x3AC` ([esp+0x68]), `mulY = E+0x3B0` ([esp+0x88]), `size = p+0x28`. Then (0x00550B0A…0x00550BA2) `vecA = p+0x40 · sX·S·S` and `vecB = p+0x50 · sY·S·S` (S the owner's scale, vcall 0xE0 on +0x154). The corners: v0 = P − A − B, v1 = P + A − B, v2 = P + A + B, v3 = P − A + B. **So ScaleX scales the particle's axis at +0x40 (axis A) and ScaleY the axis at +0x50 (axis B).** As in the effects reading, moving along A changes the texture's v, along B its u.
- **What the axes are** (0x0057C7B0, jump table 0x0057CB04 = {0x57C840, 0x57C8FD, 0x57C8FD, 0x57C94C, 0x57C94C}) [C]. The frame it gets is built by 0x0053EBC0 from the camera's four vcalls (0x00552F48–0x00552F84), whose defaults when there is no camera (0x00552E41–0x00552F29) name them:
  - +0x00 position (vcall 0x38, default 0,0,0)
  - +0x10 forward (vcall 0x2C, default 0,1,0)
  - +0x20 up (vcall 0x30, default 0,0,1)
  - +0x30 right (vcall 0x34, default 1,0,0)

  Mode 0 (0x0057C840) writes p+0x40 = −frame+0x20 = **−up** and p+0x50 = frame+0x30 = **right**, then turns both about −forward by the angle (+0x70).
- **So in facing mode 0, ScaleX (with its multiplier) is the sprite's on-screen vertical half-size and ScaleY its horizontal half-size.** With angle 0, v runs top to bottom (P − A = P + up has v = 0) and u left to right, so the texture stands upright and unflipped [C, from the corner table].
- **In the data** [C, 2,927 emitters in 600 `fx*.mmh`; the tally script is `%TEMP%\lib-agent-2-vq2-emit2.py`]. 1,400 emitters have ScaleX ≠ ScaleY somewhere in the age map. Counting which sums larger, X > Y : X < Y:

  | Mode | X > Y | X < Y |
  |---|---|---|
  | 3 (along velocity) | 497 | 63 |
  | 4 (aligned to world Z) | 72 | 1 |
  | 0 | 357 | 253 |
  | 1 | 83 | 51 |
  | 2 | 8 | 15 |

  In modes 3 and 4, axis A is the direction (−D), so X is the length along the motion. The streak emitters bear that out:
  - `fxa_arc_bolt_p` `glow` (mode 3): X 1.0 → 2.0, Y 0.2.
  - `fxa_antim_ward_c` `ray*` (mode 3): X 0 → 0.58, Y 0.5.
  - `fxa_antim_imp_c` `NewEmitter_28` (mode 3): X 4.0, Y 0.5.
  - `fxa_arc_hor_p` `hor_smk_bri` (mode 3): X 3 → 18, Y 5 → 0.5.

  This matches the code's A ↔ X [C for the data; the reading of the artists' intent is I].
  - A mode-0 example with X ≠ Y: `fxa_berserk_c` `energy`, X 0.04 / Y 0.5. In mode 0 that is a thin, wide sprite, 0.5 across by 0.04 high [I: follows from the code; not seen].

###### 3. The emit bit and the two opacities

**The emit bit** is bit 0 of the owner graphics `Object`'s flags (+0x260, or +0x258 through its +8 interface). The update tests it at 0x005530DC–0x005530EF (`[E+0x154]+8`, vcall 0xA4 → 0x0054F850 `return flags`, `test al,1`) before `SpawnCount`/spawn [C].
- **On from creation**: the Object constructor (0x0054F990, ID 8060) writes +0x260 = 0x4001 at 0x0054FCAE [C].
- **Set and clear** are the Object's +8 vtable (0x00AF8434) slot 39 (+0x9C, 0x00549640, `flags |= m`) and slot 40 (+0xA0, 0x00549660, `flags &= ~m`) [C].
- **The effect's phase does not touch it** [C, by the scan below]. Cessation stops births through the clip's channels (the birth rate tag 8, +0x33C, ramped to 0, as the wisp's `cess.ani` does). Bursts (`EmitType` 1) are armed separately (+0x410) by the animation event. `EmitType` itself is not the bit.
- **What clears or sets it** [C, every `push 1` + vcall 0x9C/0xA0 found by `%TEMP%\lib-agent-2-vq2-flags.py`]:
  - **The owner client object's flag `CCObject` +0x23C**, applied to all the effects on it by 0x0069A7A0 (ID 14217). It reads +0x23C (slot 78, 0x006951C0), lists the object's effects through the manager (0x0079E4C0), and sets or clears bit 0 on each effect's graphics Object.
    - +0x23C is written by slot 77 (`CCObject` 0x0069B640, a subclass's 0x0074BDC0), and 0x0069A7A0 runs again from `SetActive` (0x0069A950 at 0x0069AABF) and 0x006BC420.
    - The constructor sets +0x23C = 0, +0x23D = 1 (0x006998B3, `bl` taken as 0 [I]).
  - **A creature whose +0x23D ("automatic") is set** has it recomputed every update (0x006BD4D0, ID 14946, at 0x006BD591–0x006BD5D0) [C]:
    ```
    emit = obj+0x41C != 0 && obj+0x41C->vcall 0x6C() < 0.7   // 0x00B58750 = 0.7
    ```
    +0x41C is set by 0x00695D10. What that vcall returns is not read: [I] a fade or alpha-like value, so a creature's crusts stop emitting while it is faded past 0.3.
    - 0x0067C980 / 0x0067CAD0 (IDs 13641/13642) switch a list of creatures to emit off with automatic cleared, and back to emit on with automatic set. Their caller is not read [I: cutscene or conversation staging].
  - **A projectile with `prj_base` `HideLaunch`**: 0x007E8700 (ID 21121, the effect's start, which also sets phase 1) [C]:
    - It reads table 4's column hash 0x06484B80 (= `HideLaunch`) for the row at +0x4A4 (0x007E875E).
    - If set, it calls the Object's vcall +0x20(0.0, 0) [I: hide] and **clears the emit bit**.
    - Not read: where it is turned back on at release.
  - Two lists not identified: 0x005D2360 (ID 10413, set or clear over an object list) and 0x007D4220 (ID 20833, objects matched by an id at +0x508).
  - Placeable-like objects: 0x007B2D80 sets or clears it on sub-objects 3–4 by a bool, from 0x007B38A0 and 0x007C1390 (IDs 20096, 20298) [C for the call; the owner class is not read].

**+0x4E0, the emitter's opacity**:
- **Its start**: 1.0 at construction (0x00542018) [C].
- **What gates on it** [C]:
  - The update runs only while +0x4E0 > 0 (0x00552D4C). So at 0 the particles neither age nor spawn.
  - The fill draws only while > 0 (0x00550271).
  - The fill multiplies alpha by `+0x4F4 · +0x4E0` (0x005509BE).
- **No animation channel drives it** [C]: no tag in 0x0053DAD0 binds it, and the only writers in 0x538000–0x5A0000 are the constructor and 0x0053D7DC.
- **Its one source is a scene-wide opacity** [C]:
  - 0x004670C0 (ID 2755, `(float a, bool force)`) stores `a` in the global 0x00BF73FC and in this+0x48, or 1.0 if this+0x4C is off.
  - It walks the scene's objects with the visitor 0x00466F40, which acts only on objects whose flags have bit 8 (0x00466FBD).
  - On an emitter node (types 6, 7, 0x13, 0x14) it writes node +0x4E0 (`[entry+0x360]`, the entry being node+0x180) = `a`. On type 0x16 (an emitter group [I]) it does that for every emitter in its list +0x240 (0x0053D790).
  - Its caller: 0x00538060 (ID 7423) ← 0x0068AF30 (ID 13856), which takes the float at `[[0x00C327EC]+0x114]+0xD0` (dirty byte +0xD4) and gives it to the area's scene (+0x62C).
  - Who writes that +0xD0 was not found [not read].
- **The object fade (+0x190, 0x00695A90/0x00695080) does not reach +0x4E0** [C: no path found from it to 0x0053D790/0x004670C0]. [I] It hides effects only by the emit bit above and by hiding the model.

**+0x4F4, a distance (LOD) fade, 0 to 1** [C]:
- **Its start**: 1.0, with the direction +0x4F8 = 0 (0x00542024, 0x00542037).
- **The step** (0x0053F070, `(float step)`):
  ```
  if dir(+0x4F8) > 0:  clear node flag 0x400; a += step; if a >= 1 { a = 1; dir = 0 }
  if dir < 0:          a -= step;  if a <= 0 { a = 0; dir = 0; set node flag 0x400 (hidden) }
  ```
  0x0053DCD0 starts a fade in (dir = 1 if a < 1); 0x0053DCB0 a fade out (dir = −1.0 from 0x00AE2E00, if a > 0).
- **The driver** is 0x00467380 (ID 2758), once a frame over the scene's emitters:
  - `step = min(Δms · 0.001, 0.1)`, so a full fade takes 1 s.
  - It skips nodes with flag 0x200 and steps each fade.
  - For an emitter drawn last frame (+0x4EC, set by the fills at 0x00550D65 and others): if the owner's flag 7 is set (0x00A1C732) it fades in. Otherwise it fades in while `d² < L²`, else fades out (0x00467466–0x00467486). `d²` is the squared distance from the camera (manager +0x30) to the emitter's bounds (+0x1B0, 0x004D2D30). `L = table[0x00BF22CC + 4·lod]` and `lod = E+0x4F0`.
  - An emitter not drawn gets the owner's flag 7 set instead (0x0046757D).
- **`lod`** is 6323 `MMHNodeEmitLod` (u8, default 0) → blueprint +0x2B8 (0x00593847) → live +0x4F0 (0x00560BF9). In the 600 effects: 2,884 emitters LOD 0, 14 LOD 1, 29 LOD 2.
- **The distances** are set by `GraphicsDetailLevel` (0x005E1A36 compares the name; 0x00458EB0 sets them by level 0–3; the exe's static values are 2000/80/30/1 until then) [C]:

  | Detail level | LOD 0 | LOD 1 | LOD 2 | 0x00BF22D8 |
  |---|---|---|---|---|
  | 0 | 100 m | 25 m | 5 m | 0.5 |
  | 1 | 1000 m | 40 m | 15 m | 1.0 |
  | 2 | 1000 m | 60 m | 30 m | 1.2 |
  | 3 | 1000 m | 80 m | 30 m | 1.5 |

  The fourth value is not used by this fade [not read].

###### 8. `mml_vUserColorMultiplier`: two writers, one per kind of source

The constant's name is the wide string at 0x00AF7F00, and both writers set it on the node's material instance (node +0x150, made from the blueprint's material, 0x00560C4A) through 0x004F3F40 [C].
- **Its default is (1, 1, 1, 1)**: every one of the 60 `mml_vUserColorMultiplier` parameters in vanilla `vfx.mat.xml` (`packages/core/data/materials.erf`) has `Default = "1 1 1 1"` [C].

**a) A visual effect's tint, by `UseVariationTint`** [C]: Object (+8) vtable slot 69 (+0x114) = 0x00546700, `SetTint(vec4)`.
- It keeps the colour at Object +0x2CC.
- For every node of types 1, 6, 7, 19, 20, 21 (byte table 0x00546834), whose blueprint's slot 10 is true and which has a material instance, it sets the user colour to it.
  - On an emitter blueprint slot 10 is 0x00561A60, which returns +0x2BC. That is 6340 `MMHUseVariationTint`, read at 0x00593B67–0x00593B80.
  - A mesh's is +0x1AA (0x0057CF00; [I] the same field).
- 69 of the 2,927 emitters have it, e.g. `fxa_fire_swrd_c` `Fire_Base`/`Fire_Whispy`/`Glow`, `fxa_glow_blue_c` `streaks`, the glyphs' `VertGlow`/`Glow`/`Streaks`/`BeamSmall*`.
- **The colour's source**, in the effect's load 0x007EA550, ID 21169 [C]:
  1. **The effect's own**: `EFFECT_INTERGER_USE_TINTING` (int 4) ≠ 0 makes `(EFFECT_VECTOR_TINTING (vector 3).xyz, 1.0)` (0x0079F540 → params +0xC). No vanilla script sets either (`effect_constants_h.nss`:151–155 only).
  2. **Otherwise `vfx_base`'s `Tint` column** (hash 0xF2692253, row +0x2C, 0x007E9EF2). It is a string parsed by `wcstoul(s, 0, 0)` (0x004C0990), so decimal or `0x` hex, as `0xAARRGGBB`. It becomes `(R, G, B, A) / 255` (0x007EA5D5–0x007EA678; 0x00AF6EA8 = 255.0). 0 means none.
  - Either goes to `SetTint` through 0x007E9C70 → vcall +0x114 (0x007EAA64–0x007EAA84). `EFFECT_FLOAT_SCALE` goes by 0x007E9C40 the same way.
  - In vanilla `vfx_base`, 856 rows have 0 and about 140 a colour. Examples:
    - 90091 Heal `fxa_heal_impact_c`: 4283203583 = 0xFF4C7FFF, light blue.
    - 90064 Sunder Armor `fxa_penalty_c`: 0xFFFF7F19, orange.
    - 90023 War Cry: 0xFFCC0000, red.
    - 1067/1110 the flaming weapon `fxa_fire_swrd_c`: `0xFFCC4C19`.
    - 5040 the Collector's Edition spirit flame: `0xFF3366FF`.
- **Not the creature's own tint** [C for this path]: nothing of the owner enters it.

**b) A placed model's named parameter, by `UserParamName`** [C]: the emitter's (+0x180 interface) slot 12 = 0x005413E0.
- It reads the emitter's 6325 `MMHNodeEmitUserParamName` (a string, blueprint +0x2F0, read at 0x00593ABA).
- If the name is non-empty, it looks it up in a map of name → vec4 (0x004C5BF0). If found, it sets the user colour to the value.
- The map comes from Object (+8) slot 78 (+0x138) = 0x005462F0, which hands it to every node's slot 12.
- The level's loader calls it with a placed model's record +0x5C (0x0053A759). The record is read at 0x00A29CE3–0x00A29DF5 from 3506 `EnvModelUserParamList`: 3507 `EnvModelUserParamName` → 3508 `EnvModelUserParamValue` (vec4, default (0, 0, 0, 1)).
- So **a level designer's placed effect is coloured by name**.
  - 254 emitters name a parameter, all seen `color` (e.g. `fxa_heal_impact_c` `HI_cloud`/`HI_glow`, the glyphs, `fxa_ent_aoe_p` `flame_0_1`).
  - A `vfx_base` effect made by a script gets no such map, so path (a) is its only tint [I].
- Another caller passes a map from 0x00C2C078 (at 0x004D6CDB in 0x004D6C50, ID 5059) [not read].

**Which wins** when an emitter has both: whichever ran last, since both write the same parameter [I]. An emitter with `UseVariationTint` 0 and no matched name keeps (1, 1, 1, 1).

###### Not read

- What `CCObject` +0x41C's vcall 0x6C returns (the 0.7 emit threshold), and who calls 0x0067C980/0x0067CAD0, 0x005D2360, 0x007D4220.
- Where `HideLaunch`'s cleared emit bit comes back on.
- Who writes the scene opacity `[[0x00C327EC]+0x114]+0xD0`, and which objects carry Object flag 8.
- The node-type numbers' classes (types 1, 6, 7, 19–22 as used above are by their handling, not their vtables), and the mesh blueprint's +0x1AA field id.
- The other caller of the user-param map (0x004D6CDB).
- The fourth detail-level value (0x00BF22D8).
- Nothing was seen in play.

#### DAOParagonEngine #28: spawn volumes, ribbons and effect lights (lib-agent-2-vq3, read 2026-10-18)

Read in the GOG exe and the game's files; not seen in play. [C] = read in the disassembly or data, [I] = inferred.

##### 4a. Spawn volumes

**The node and its fields** [C]. A spawn volume is an `spnv` child of the emitter's `nemt` node, loaded by 0x00590740 (called from the emitter loader at 0x00593E85) into a `GraphicsStructures::SpawnVolume*` at the blueprint's +0x1FC. Loading it sets the "has a volume" bit (+0x130 bit 6), then:

| Field (GFF id) | Default when absent | Kept |
|---|---|---|
| SpawnWithinVolume 6045 | 0 | +0x130 bit 7 |
| InvertSpawnVolumeNorms 6046 | 0 | +0x131 bit 0 |
| NormalsAsDirection 6291 | **1** | +0x131 bit 4 |
| Radius 6286 | 1 | sphere +4, cylinder +4 |
| CylinderLength 6287 | 1 | cylinder +8 |
| CylinderAxis 6288 (vec3) | (0, 0, 0) | cylinder +0x10 |
| BoxMin 6289, BoxMax 6290 (vec3) | (0, 0, 0) | box +0x10, +0x20 |
| SpawnVolumeType 6285 (u8) | 0 | picks the class |

- The class is picked by type: 1 `SpawnVolumeSphere` (vtable 0x00AFB814, 8 bytes), 2 `SpawnVolumeCylinder` (0x00AFB824, 0x20), 3 `SpawnVolumeBox` (0x00AFB834, 0x30). **Any other value** makes a `SpawnVolumeMesh` (0x00AFB8F4, 0x2C, ctor 0x0058E3A0) [C].
- In vanilla's `modelhierarchies.erf`, the 1,171 `spnv` nodes are 616 cylinders, 332 boxes, 202 spheres and 21 meshes, all of the meshes type 0 [C, scanned]. Fields a type doesn't use are stored as NaN.
- A cylinder's axis is (0, 0, 1) in every sample read. A box's min can be above its max on an axis (`fxa_cnt_aoe_p`: z 5 to 2), and the lerp below takes that as it is.

**The call** [C]. The sampler 0x00560010 calls the volume's slot 2: `Sample(outPos, outNormal, within = +0x130 bit 7, invert = +0x131 bit 0, normalsAsDir = +0x131 bit 4)`, `ret 0x14`. Every volume draws from the same LCG stream as the birth, 0x00C31994. Below, u = rand·(1/32767) in [0, 1] and s = u·2 − 1. The result is in the emitter node's local space.

**What the birth does with it** (0x0053F980) [C]:
- `P = M(+0x60) · (pos, 1)`, divided by w (0x004D3730, 0x004D4650). +0x60 is inferred to be the emitter's world matrix.
- With a volume the point is **not spread between the previous and current place**: there is no `prev + (cur − prev)·rand01` as on the path with no volume.
- `dir = rotate(normal, Q(+0x490))`, +0x490 being the emitter's world rotation (inferred).
- Then SpawnDirectionTracksTarget (+0x131 bit 7) replaces dir with `normalize(target(+0x380) − P)`. SpawnSpreadX turns it about the rotated X axis and SpawnSpreadY about the rotated Y axis, as read before.
- So with NormalsAsDirection off, the normal is (0, 0, 1) and every volume emits along the node's +z.

**Sphere** (0x0053E380) [C]:
```
r = R; if within: r *= u                       // not cbrt(u): within, the points crowd the centre
a = u*360, b = u*180 (degrees)
q = Rz(a) * Rx(b) * Ry(0)                      // 0x004EE200: quaternions about Y, X, Z, Hamilton products (0x004CB640)
n = normalize(rotate((0,0,1), q))              // 0x004CB6D0, then 0x00451A30
pos = n * r
normal = normalsAsDir ? (invert ? -n : n) : (0,0,1)
```
- With the usual signs, n = (sin a·sin b, −cos a·sin b, cos b) [I, for the signs only].
- The polar angle b is uniform, so the directions crowd the poles: the sampling is **not uniform** over the sphere.

**Cylinder** (0x0053E540) [C]:
```
h = s * L * 0.5                                // drawn first
r = R; if within: r *= u                       // the side only, not the caps; within, crowding the axis
theta = u * 2*pi
v = normalize(cross((0,0,1), axis))            // 0x00451A30: |v|^2 <= 1e-6 gives (1,0,0), as for axis (0,0,1)
n = normalize(rotate(v, axisAngle(axis, theta)))
pos = n * r + axis * h                         // the axis isn't normalized: its length scales the height
normal = normalsAsDir ? (invert ? -n : n) : (0,0,1)
```
The cylinder is centred on the node, running from −L/2 to +L/2 along the axis. In the angle and along the axis it is uniform.

**Box** (0x0053E790) [C]:
```
pos = (lerp(min.x, max.x, u), lerp(min.y, max.y, u), lerp(min.z, max.z, u))   // three draws, x, y, z
if !within:
    face = rand % 6                            // uniform per face, not by area
    0: pos.x = min.x, n = -X   1: pos.x = max.x, n = +X
    2: pos.y = min.y, n = -Y   3: pos.y = max.y, n = +Y
    4: pos.z = min.z, n = -Z   5: pos.z = max.z, n = +Z
k = rand                                       // always drawn
if !normalsAsDir: normal = (0,0,1)
elif !within: normal = the face's n            // invert is IGNORED on the surface
else: axis = k % 3; normal = (max - p <= p - min) ? +axis : -axis   // the nearer face of a random axis
      if invert: normal = -normal
```

**Mesh** (`SpawnVolumeMesh`, sampler 0x0053F440; data 0x0058F8E0) [C]:
- **There is no mesh name.** The `spnv` node carries its own geometry as two `data` children (MMHChildren 6999, fourcc 'data', each read by 0x0058F8E0):
  - DataIsIndexStream 6168 = 0: DataCount 6173 vertex positions, field 16 a list of vec4 (16-byte stride), kept at +4 as {data, count, capacity}.
  - DataIsIndexStream 6168 = 1: DataCount 6173 u16 indices (field 8), kept as count/3 six-byte triangles at +0x1C (a vector: begin +0x20, end +0x24).
  - DataType 6169 (2 for the positions, 0 for the indices) isn't read.
  - Examples: `fxa_lit_cone_c`'s `CO_Mesh*` (384 vertices, 384 indices), `fxe_cha_sel_p` (20 vertices, 66 indices).
- A second array at +0x10, read as per-vertex normals, is **never filled**: the loader writes only +4 and +0x1C. Nothing else filling it was found: the other callers of the array's resize (0x0053F400) weren't traced to it [I].
- **Within**, with at least one vertex:
  ```
  i = rand % (n-1); j = rand % (n-1)           // the last vertex is never picked; n = 1 divides by zero
  t = u
  pos = V[j]*(1-t) + V[i]*t                    // a point on the segment between two vertices, not inside the mesh
  normal = normalsAsDir ? ±normalize(V[j] - V[i]) : (0,0,1)
  ```
- **On the surface**, or within with no vertices:
  ```
  tri = rand % T                               // uniform per triangle, not by area
  r1, r2, r3 = u, u, u; S = r1 + r2 + r3
  pos = V[a]*(r1/S) + V[b]*(r2/S) + V[c]*(r3/S) // crowds the centroid; not uniform over the triangle
  normal = normalsAsDir ? ±(N[a]*w1 + N[b]*w2 + N[c]*w3), not normalized, from the unfilled +0x10 : (0,0,1)
  ```
  T = 0 gives pos 0 and normal (0, 0, 1).
- All 21 vanilla mesh volumes have NormalsAsDirection 0, so their normal is always (0, 0, 1). Their within flags are 0 (17) and 1 (4), and invert 1 on 4 of them [C, scanned]. A port need not model the empty normals array.

**Uniformity, in short:** none of the four samplers is uniform over its volume or surface. A port that matches the distributions uses the formulas above. CRT `rand` is not called: every draw is the engine's own LCG at 0x00C31994.

##### 4b. Ribbons (linked particles)

Read 2026-10-18 in the GOG exe (lib-agent-2's vq3 fork). [C] read in the disassembly, [I] inferred.

###### Which emitters are ribbons

- The blueprint's instance maker (0x005609E0) picks the class [C]:
  - blueprint +0x2C0 set (`MeshParticleModelName` 6284) → `ObjectNodeEmitterMesh<MeshParticle>` (0x00560080);
  - else `LinkParticlesTogether` (6033, blueprint +0x130 bit 0x20) → `ObjectNodeEmitterSpriteLinked<SpriteParticle>` (factory 0x005600E0, constructor 0x00586090, vtable 0x00AFADAC);
  - else the plain sprite (0x00560140).
  So a mesh emitter wins over the link flag.
- The constructor [C]: frame accumulator +0x52C = `RandomStartFrame` (blueprint +0x203) ? rand01 (its own LCG stream at [0x00C32234], × 1/32767) : 0; ring head +0x520 = 0, tail +0x524 = 0; UV counter +0x528 = −1; the particle array (+0x500) sized to 0x101 slots of 0x80 bytes, each built by `SpriteParticle`'s constructor (0x0057BD40, every field 0).

###### Overrides against `ObjectNodeEmitterSprite<SpriteParticle>` (vtable 0x00AF87F4) [C]

| Slot | Sprite | Linked | What |
|---|---|---|---|
| 34 update | 0x00552CF0 | the same | shared: spawn count, spawn calls through slot 38, then slot 40, then the fill through slot 43 (not while presimulating) |
| 38 spawn | 0x00552AF0 | 0x00584A90 | below |
| 39 count | 0x00550300 | 0x00584760 | `(tail − head) mod 256` |
| 40 age/move | 0x005528E0 | 0x00584C20 | below |
| 41 clear | 0x00552A90 | 0x00584A50 | head = tail = 0 |
| 42 make the draw batch | 0x00553E50 | 0x005861A0 | always on the 32-byte pool [0x00C31A74] (its vertex buffer +0x28), room for 0x400 vertices and 0x600 indices (256 quads) |
| 43 fill | 0x00550260 | 0x005850E0 | the sprite's dispatches by +0x510 to the 32-byte fill (slot 44) or the 68-byte one (slot 45); the ribbon's is its own, 32-byte only |
| 46 (new) | — | 0x00584C00 | pop the oldest: head = (head + 1) & 0xFF |
| 36 | 0x00550310 | 0x005849F0 | physics (`PhysicsEmitter`) only |

- **There is no 68-byte (tangent) path for ribbons** [C]: the fill only locks [0x00C31A74] and the batch is made on it. A ribbon whose material wants tangents still gets 32-byte vertices [I: what that draws wasn't checked].

###### The particles: a ring of 256 [C]

- Same 0x80-byte `SpriteParticle` (index × 0x80 from +0x500, 0x005502B0). Used: +0x00 position, +0x10 velocity, +0x20 age, +0x24 1/life, +0x28 size, +0x2C spin, +0x40 axis A, +0x50 axis B, +0x60 spin axis, +0x70 angle. +0x30 (the sprite's per-particle frame) is never set or read.
- Head +0x520 is the oldest, tail +0x524 one past the newest; next(i) = (i + 1) & 0xFF (0x00584920). **At most 255 alive**: a spawn when next(tail) == head is dropped.
- **The spawn** (0x00584A90; args position, direction, (unused), and passes `useDirection` 0):
  ```
  if UvDistributionSize N > 0: uvCount(+0x528) = (uvCount + 1) % N     // before the full check: a dropped spawn still advances it
  if next(tail) == head: return
  p = slot[tail]
  invLife = 1 / Life(+0x344); age = 0           // no LifeRange, no guard for Life <= 0
  spin  = RotSpeed(+0x35C) + randS * RotSpeedRange(+0x360)
  size  = 1 + randS * ScaleRange(+0x368)        // randS = r*2/32767 - 1, stream [0x00C32234]
  0x0053F980(p, position, direction, 0)          // position, velocity: as the sprite's birth (volume, spread, speed, target, inherit velocity)
  tail = next(tail)
  ```
  Unlike the sprite's spawn (0x00552AF0) it doesn't call 0x0057C520, so `InitialRotation`/`Range` are ignored, the axes aren't set at birth, and the angle +0x70 is whatever the slot's previous particle left (0 the first time round). It also makes a quaternion of the emitter node's matrix (0x004D33F0) and drops it.
- **Where they are born** (the shared update, 0x00553330–0x005533D8) [C]: with `BirthrateInParticlesPerMeter` (6028, +0x130 bit 4) *and* the link flag, the i-th of n spawned this step is placed at `prev + (cur − prev)·(i+1)/n`, evenly along the emitter's move with the last on its current place; otherwise `prev + (cur − prev)·rand01`. With per-meter birth (the spawn count, 0x0053EC80) a joint is laid about every 1/BirthRate metres the emitter travels.
- **Ageing and death** (0x00584C20, args the update context from 0x0053EBC0 and dt):
  ```
  for i = head .. tail-1:
      if age >= 0: age += dt
      if age < 0 or age*invLife >= 1: pop from head up to and including i      // every older particle goes with it
      else SpriteParticle::Update(ctx, params +0x330, dt)                       // 0x0057BF30: the sprite's motion, inheritance carry included
  for each alive: 0x0057C7B0(ctx, params, dt)   // movement spread, spin, angle, the facing mode's axes
  ```
  No per-particle frame advance and no swap-with-last: the ring stays in birth order.
- **The axes by facing mode** for a ribbon [C for the code; I for what shows]: modes 0, 3 and 4 rewrite axes A/B every frame (0x0057C7B0), so they work. Modes 1 and 2 only turn the axes set at spawn, and the linked spawn never sets them: they stay 0 from the constructor, so the ribbon has **no width** (or a dead particle's stale axes once a slot is reused). Mode 3 (B = normalize(cross(−velocity, P − cam))) gives a camera-facing strip when the particles move along the trail. Mode 0 gives B = the camera's right turned by the angle, whatever way the trail runs.

###### The fill (0x005850E0), once per update [C]

Runs only when the emitter's opacity +0x4E0 > 0, the model's flag (`[+0x154]+8` → vfunc 0xA4) has bit 0x20, and count > 0. It clears +0x50C first and sets it to 1 at the end (the batch is submitted only when it is 1, +0x180 interface slot 5, 0x00584790).

```
count = (tail - head) mod 256                    // quads: count-1 between particles, plus the newest to the emitter
if FlipbookType(+0x200) != 0: frame(+0x52C) += FramesPerSecond(+0x204) * dt / (rows(+0x201)*cols(+0x202))
                                                 // dt = 0x005502D0: the time source × the model's speed; one frame for the whole ribbon, never wrapped
N = UvDistributionSize (blueprint +0x2B0, GFF 6308)
if N == 0: D = count; k = 0
else:      D = N;     k = (uvCount - count%N + N + 1) % N    // = the oldest alive particle's birth number mod N
opacity = E+0x4F4 * E+0x4E0
lock count quads in [0x00C31A74]; batch: base vertex, count*6 indices, count*4 vertices

c = oldest; P = c.pos
rgba, sX, sY = AgeMap(t(c), tint +0x39C, X mul +0x3AC, Y mul +0x3B0, c.size)   // 0x005607C0
W = c.axisB * sY                                 // sY, not sX: the age map's ScaleY × AgemapScaleyMultiplier × size
rgba.a *= opacity; rgba = saturate(rgba); col = ARGB8(rgba*255)
uPrev = 0
loop:
    u0 = N > 0 ? k/D : uPrev;  u1 = (k+1)/D;  uPrev = u1
    v0 = (P + W, TEXCOORD0 (1, u0, frame, 1), col)
    v1 = (P - W, TEXCOORD0 (0, u0, frame, 1), col)
    n = next(c)
    if n != tail:                                // a joint
        W   = normalize((c.axisB + n.axisB) / 2) * sY(c)     // 0x004D47E0, 0x00451A30 (a near-zero vector becomes (1,0,0))
        P   = n.pos
        rgba, sX, sY = AgeMap(t(n), ...)         // no opacity, no saturate
        col = ARGB8(rgba*255)                    // cvttss2si, low byte kept: above 1.0 wraps
    else:                                        // the last segment
        W   = c.axisB * sY(c)
        P   = the emitter's current position (vfunc 0x74)
                                                 // col stays the newest particle's
    v2 = (P - W, (0, u1, frame, 1), col)
    v3 = (P + W, (1, u1, frame, 1), col)
    k = (k+1) % D; c = n; until c == tail
```

- Each quad has its own four vertices (0x80 bytes, written by 0x00550650: float3 position, float4 TEXCOORD0, D3DCOLOR). The shared static index pattern (b, b+1, b+2, b+2, b+3, b) makes the triangles (P+W, P−W, P'−W') and (P'−W', P'+W', P+W). Neighbouring quads meet exactly at a joint: the next quad's v0/v1 are this quad's v3/v2 [C].
- **Width at a joint** is the averaged, renormalised axis B times the *older* particle's sY. The first edge and the last edge use the raw axis B (not renormalised). The owner object's scale isn't applied. Unlike the sprite fill, this fill doesn't update the emitter's bounds (+0x2C0/+0x2D0, +0x4EC) [C].
- **Colour per vertex**: each joint's pair takes its particle's age-map colour × the tint (+0x39C). **Only the oldest particle's alpha is multiplied by the emitter's opacity (+0x4F4·+0x4E0) and clamped.** Every later colour is neither faded by the emitter nor saturated, and a component above 1.0 wraps in its byte [C; as read, a vanilla fault]. So an emitter faded through +0x4E0 fades only the tail end of its ribbon, then vanishes when +0x4E0 reaches 0 [I].
- **u, v**: TEXCOORD0.x runs across the ribbon (1 on the +W edge, 0 on −W). TEXCOORD0.y runs along it: with N = 0, 0 → 1 over the whole ribbon from the oldest particle to the emitter, so the texture stretches as the ribbon grows. With N > 0, each segment spans [k/N, (k+1)/N], repeating every N segments, with k fixed to the particle's birth number so the texture travels with the particles. A spawn dropped on a full ring still advances the counter, so the texture then slips by one segment [C arithmetic; I effect]. TEXCOORD0.w is 1, against 0 for sprites, so an `AlternateDecal` material draws its alternate texture on a ribbon [I from the shader's lerp].
- **Pool limit** [C, 0x00562570]: the lock fails (no ribbon this frame) when the quads don't fit. It scales the count it returns by the pool's percentage (+0x38, ÷100, at least 1), and the batch draws that many. The ribbon loop still writes every particle's quad [I: only matters if that percentage is below 100].

###### Corrections to the earlier reading

- The ribbon's half-width uses the age map's **Y** scale (× `AgemapScaleyMultiplier` +0x3B0 × size), not "scaleA" (X). The X scale is computed and unused [C, 0x005852CC and 0x005607C0's outputs: arg2 = ScaleX·+0x3AC·size, arg3 = ScaleY·+0x3B0·size].
- "Each pair its particle's colour" holds, but the emitter fade and the clamp apply to the oldest particle's colour alone (above).
- "The last to the emitter's current place" is confirmed: one extra quad from the newest particle to the emitter, colour and width the newest particle's.

###### Not read

- What +0x4E0, +0x4F4 and +0x510 are set from; the pool's percentage (+0x38) and who sets it.
- 0x0057BEC0 (mode 3's direction from the velocity) when the velocity is 0.
- The `PhysicsSpriteParticle` variants (vtables 0x00AFAF3C, 0x00AF8984): where they are made.
- How the submitted batch is sorted (the ribbon's bounds aren't refreshed by its fill).

##### 6. An effect's point lights

Read 2026-10-18 in the GOG exe (lib-agent-2's vq3 fork), against DAOLib's `Addresses.h` light rows (#65, #147, #150) and DAOElgaraShaders' `dev/NOTES.md` ("Effects' lights", 2026-10-17, logged in play). [C] = read in the disassembly or data here; [I] = inferred; [E] = elgara's reading in play, not re-checked here.

###### The node and its fields

- **The blueprint** is `GraphicsStructures::ObjectNodePointLightBlueprint` (vtable 0x00AF97EC). The `nplt` loader at 0x00594780 fills it [C]:

| GFF id | Field | Blueprint | How it is read |
|---|---|---|---|
| 6000 | MMHName | +0xC0 | string |
| 6007 | MMHNodePointLightColor | +0x130 | float4; the 4th is passed through to the shader (the wisp's 0.6, 1.4, 2.0, 1.0: over 1, an HDR colour) |
| 6008 | MMHNodePointLightRadius | +0x140 | float |
| 6249 | MMHNodePointLightIntensityVariation | +0x148 | float |
| 6250 | MMHNodePointLightIntensityPeriod | +0x14C | float (s) |
| 6251 | MMHNodePointLightIntensityPeriodDelta | +0x150 | float (s) |
| 6009 | MMHNodePointLightIsStatic | +0x15C | bool: picks the list (below) |
| 6340 | MMHUseVariationTint | +0x15F | bool |
| 6004 | MMHID | +0x154 | int |
| 6339 | MMHLightCanBeOccluded | +0x15E | bool |
| 6345 | MMHNodeLightVersion | +0x15D | byte: the scoring formula |
| 6296 | MMHNodeLightAffectDomain | +0x158 | 0, 1 or 2 (any other value 0) |
| 6999 | MMHChildren | | the child nodes |

  No falloff, shadow, spot or specular field exists on an `nplt` [C]. Its loader reads nothing else.
- **The instance** is `ObjectNodePointLight`. Its vtable is 0x00AF7ACC, with the `ILight` interface at +0x180 (vtable 0x00AF7A6C). It is 0x1C0 bytes, made by the blueprint's slot 2 (0x0055FAC0) [C]:
  - +0x1A0 is the colour, from the blueprint's +0x130 through 0x0053CE10. The same call also sets +0x1B8 = (|r|+|g|+|b|)/3 (0x0053CE60), a brightness used only by version-1 scoring.
  - +0x1B0 is the radius, +0x18C the AffectDomain (which is the `ILight`'s +0xC "kind"), +0x190 the MMHID, +0x195 CanBeOccluded and +0x197 the LightVersion.
  - +0x196 is UseVariationTint, through the blueprint's slot 10, 0x0055F3F0.
  - +0x1B4 is an intensity, set to 1 by the constructor (0x0053CCA0). Its setter is 0x0053C880.
  - +0x1BC is the flicker multiplier, 1 at start.
  - +0x154 is the owning `GraphicsStructures::Object` (the effect's model).
  - +0x60 is the node's world matrix. The position is (m[+0xC], m[+0x1C], m[+0x2C]) / m[+0x3C] (`ILight` slot 2, 0x0053CA70).
- **Flicker** [C]: where IntensityVariation ≠ 0, the instance maker makes an `Animation::RandomWaveController<float>` (0x30 bytes; constructor 0x008155F0, Update 0x008156E0 = ID 22011).
  - It targets +0x1BC and is put on the owning Object's controller list (+0x1AC).
  - Each cycle lasts `period + randS·periodDelta` and moves between two targets, each `base + randS·variation`. The exact scaling is [I]; elgara's data reading says "base × (1 + variation·randS)".
  - It uses the engine's LCG at 0x00C39778.
  - Vanilla's ease is broken: it jumps every cycle. DAOLib's `src/fix/LightFlicker.cpp` (#150) fixes this and stretches cycles to at least 1 s.
- **The colour a shader gets**, `ObjectNodeLight::Color` (0x0053CD90, ID 7605) [C]: `c = +0x1A0`, times the owner's tint float4 at `*(+0x154) + 0x2D4` when +0x196 (UseVariationTint) is set. That tint is inferred to be the effect's tint from `ApplyEffectVisualEffect`/0x0079F540.

###### Animation drives it each frame

- **The node's animation target** (slot 14, 0x0053C890) answers tag **24 → +0x1A0 (colour, float4)** and tag **25 → +0x1B0 (radius, float)**. Any other tag goes to the base node (translation, rotation and so on) [C]. So a clip channel bound to `<light>_color` or `<light>_radius` writes the live colour or radius directly, linearly keyed like the emitters' channels (part 1).
- **The wisp** (patch002's `fxc_wisp_c.mmh`) [C, data]:
  - It has two lights under `NewDummy_14`: `Light_c` (AffectDomain 1) and `Light_l` (AffectDomain 2). Both have colour (0.6, 1.4, 2.0, 1), radius 2, IsStatic 0, no variation, CanBeOccluded 0, UseVariationTint 0 and Version 0.
  - Each exports only `light_c_radius` or `light_l_radius` (tag 25).
  - The clips' `light_c_color` channel names no export in this copy, so it is ignored (EFFECTS.md). In play, the cessation's radius flare (2→4 at 1 s) applies and its colour fade to black does not [I, from the ignore rule].
  - The pair is the usual pattern: one light for figures, one for the level (below).
- **The position is read live** [C]: `LightState::Fill` and the score take it from the node's world matrix when they run, so a light under an animated dummy or a crust hook moves with it.
- Not read: whether a moving light makes a level mesh re-choose its lights. AddLights stamps the manager (`MarkChanged`, 0x00461F40) only on add.

###### Joining the scene's light lists

- **The model's light list** [C]: the instance maker's last step is `Object::AddLight` (0x0054D7C0). It puts the node's `ILight` into the owning Object's list at +0x70, skipping duplicates.
- **The scene** [C]: `Graphics::ISceneInstance` (vtable 0x00AF7838).
  - **Slot 0, 0x0053A340, adds an object.** Unless the scene is deferring (+0xD0, which queues the object at +0xE4), it calls slot 5 on each of its three managers at +0xD8, +0xDC and +0xE0.
  - +0xDC is `Renderer::LightManagerDefault` (vtable 0x00AE7BF0, made by the factory 0x0046C620 as kind 1). Its slot 5 is **AddLights** (0x00464E30, ID 2701).
  - Slot 1 (0x00539710) removes an object, through the manager's RemoveLights (0x00465190, ID 2702).
- **An effect enters its owner's scene** at 0x0079D410 [C]:
  - The scene is the owner model's own (Object vtable +0xA4), else the area's (+0x62C of the owner's area), else the default one (`[0x00C31650]` slot +0x24).
  - The effect's vtable +0x104 (0x007E9AB0) then hands the scene to its model (model interface slot 1).
  - That this ends in `ISceneInstance::AddObject` and so in AddLights is [I]: the scene-add chain was not followed to the call.
- **AddLights sorts each `ILight`** [C]:

| AffectDomain (node +0x18C) | Type (`ILight` slot 4) | IsStatic (bp +0x15C) | List |
|---|---|---|---|
| 1 | point (1) | 1 | +0x6C, figures' "static" |
| 1 | point (1) | 0 | **+0x84, figures' "dynamic"** (no MarkChanged) |
| 0 or 2 | point (1) | 1 | +0x24, level's static (count 0x00C1EEC0 += 1) |
| 0 or 2 | point (1) | 0 | **+0x0C, level's dynamic** (0x00C1EEBC += 1) |
| — | ambient (2) | | +0x54 |
| — | probe (3) | | +0x3C |

  Every vanilla effect light seen has IsStatic 0, so an effect light lands in **+0x84 with domain 1, or +0x0C with domain 0 or 2**. That matches elgara's log [E]: one node in each list per blade for Flaming Weapons, reach 2–3 m. Elgara also found the level's +0x0C nearly empty in the Arl's dungeon (one lamp) before the spell, so in practice the "dynamic" lists hold the effects' lights.
- **On removal** (death, cessation, destroy), RemoveLights takes them out of the same lists (0x00465190, by the same tests, through 0x00464B50) [C].

###### What a draw takes

- **The choice** is made per object, not per draw, into its `LightState` (`LightList::ChooseFor`, 0x00463E40, ID 2691) [C]:
  - **Who chooses from which list:**
    - `ChooseForMoving` (0x00464560, ID 2694; figures, and anything flagged 0x4000) takes +0x84 → group +0, +0x6C → group +0x10, probes +0x3C → group +0x20, and the ambient +0x54 only if asked.
    - `ChooseForLevel` (0x00464310, ID 2693) takes +0x0C → group +0, +0x24 → group +0x10, +0x3C, and the ambient +0x54 only for objects whose flags hold 0x10000.
    - So **a domain-1 effect light lights figures only, and a domain-0/2 light lights level meshes and props only.** The wisp's two lights are one of each [C].
  - **Per light:**
    - A light with CanBeOccluded set and an MMHID missing from the passed sorted id list is skipped (inferred: the room's light visibility list).
    - For the "dynamic" lists, a light is skipped unless its owner Object's flags (vtable +0xA4, on owner+8) have bit 1. That bit is inferred to mean the effect's model is shown.
    - Otherwise the light is scored by its virtual +0x84 (0x0053D280, ID 7628). The object's place is P, its size s (bounding radius) and its room id R:
      ```
      reach = radius(+0x1B0) + s
      if |L - P|^2 > reach^2  or  room test fails: score = 0      // 0x0053D210: passes if R == -1, or the light has no room (+0x188 null), or rooms match
      d = |L - P|
      Version 0: score = |intensity(+0x1B4)| * (reach - d)
      Version 1: score = f(reach) * (reach - d) * |intensity| * brightness(+0x1B8)   // f = 0x004CB490, not read
      ```
    - Effect lights inferred to have no room, so they reach across rooms.
  - **For a level mesh** (read 2026-10-19 by lib-agent-2, for DAOParagonEngine #90; checked unless said):
    - **P and s:** the object handed to `ChooseForLevel` is the mesh node's renderable (node +0x180). P is the node's world matrix (node +0x60) times the `.msh` chunk's `MeshBoundsSphere` centre (8019), divided by w when w is not 0; s is that sphere's radius, not scaled by the matrix. R is the node's room id (+0x168); an owner flagged 0x100000 gives two ids instead.
    - **The room test** (0x0053D210) also passes when the light's room object's list (+0xC0) holds the id. The id list a CanBeOccluded light is tested against is the layout's table of the lights each room sees; no vanilla layout lamp is CanBeOccluded.
    - **A layout lamp's |intensity| is 1.0:** the node's constructor (0x0053CCA0) sets intensity, brightness and flicker to 1.0, and the blueprint's instantiation (0x0055FAC0) copies the radius, domain, MMHID, CanBeOccluded and Version but no intensity.
    - **Ties and held lights:** the sort is MSVC's `std::sort` with the score descending (0x004621D0), so exact ties come out in an order that is fixed but not stable. A light already held keeps its slot; one that drops out fades over about 0.25 s and holds its slot until its fade ends; a newcomer fades in only to take the place of one whose fade has ended. All of it is cleared at once when the layout changes, or when the best light has the flag +0x194.
    - **When it runs:** `ChooseLightsOf` (0x004654E0) runs for each listed object every frame (0x0045BE65). The dynamic list is chosen each time. For a static mesh, the static and probe lists are chosen again only at the area's load or when a light is added or removed, by two counters on the manager.
    - **Correction to a capture's reading** (DAOElgaraShaders' `tower_lights.txt`): [9]/[11] is dynamic light 0 and [10]/[12] light 1, as above. The listing printed [9]–[11] as one light.
    - Not read: f (0x004CB490) and whether any lamp is Version 1; the flag +0x194; a lamp of another Version keeps its last score.
  - **The pick:** sorted by score (0x00463B40). The best non-zero ones up to the group's capacity (the group's +4) are kept, and lights already held are kept.
  - **The fade:** an entry fading in or out is weighted by `min(1, t·4)` or its complement (0x0046A470; the rate 4.0 is at 0x00BF29AC, about 0.25 s). The unit of t was not read.
- **Capacity** [C]:
  - The state's register counts (constructor at 0x0046A4E0) are +0x1F4 = 3 "static" lights and +0x1F8 = 2 "dynamic" lights.
  - Elgara read the lists' own capacity as 2 for the dynamic lists, and 0 when the lighting-quality global (0x00BF22C8) is 0 (0x00462210) [E].
- **The registers** (`LightState::Fill` 0x0046B260, ID 2848; into `g_vLightParams` at state +0x40) [C]:
  - Dynamic group +0, from the tables at 0x00AE7FC8/0x00AE7FD4: light 0 at **[9] position+radius and [11] colour**, light 1 at **[10] and [12]**.
  - Static group +0x10, from 0x00AE7FB0/0x00AE7FBC: [0]/[1], [5]/[6], [7]/[8].
  - [2]/[3] are the sun and [4] the ambient (DAOLib `Lights.h`: zero, plus each chosen ambient light).
  - **The sun, [2] and [3]** (read 2026-10-19, lib-agent-2, for paragon-agent's #78) [C]:
    - It is the area's, not a figure's own: nothing in it is per figure or relative to the camera. `LightManager::Update` (vtable slot 3, 0x00462800) calls `GetSun` (0x004625D0) once per scene and keeps the direction at manager +0xF0, the sun's colour at +0x100 and the figures' colour at +0x110. `ChooseForMoving` is handed +0xF0 and +0x110, `ChooseForLevel` +0xF0 and +0x100 (call sites 0x00465570, 0x00465563). Both go through 0x00462470 to 0x0046BB80, which stores the direction at state +0x60 ([2]) and the colour at +0x70 ([3]). `Fill` never writes them.
    - `GetSun` switches on the layout's +0x7C, which the layout's load (0x005D1290) sets from `EnvAreaChunkIsChunk` (3122):
      - **1, a terrain level:** the direction is the .arl's `EnvAreaSunlightDirection` (3150, layout +0xE0) as stored, w 1.0: the way the light travels, world space, z up, not turned or negated. Figures and level meshes alike read `-c2` as the way to the light (ch1.psh:196-198, static1.psh:196-198 and 271). The colour is `EnvAreaSunlightColorChar` (3149, +0xC4) for figures, a copy of `SunlightColor` where the file lacks it, and `EnvAreaSunlightColor` (3152, +0xB4) for level meshes.
      - **2, a room level:** no sun. Both colours are a static (0,0,0,1) (0x00456320), whatever the .arl holds; the direction comes from the manager's +0xC0. 133 of the game's 213 .arl are room levels, the character creation stage oth999d among them; it is lit by its `_lc.mmh`'s three domain-1 lamps, the key `char_002` (`dev/CHARGEN.md`).
      - **Anything else, or no layout** (inferred for the paperdoll and portrait scenes): the black static at 0x004626E3 for the direction and both colours.
    - On a terrain level the figures' colour is faded per figure (0x00462470): to 0.1 where the layout's `EnvAreaSunlightCanBeOccludedChar` (3148, +0xF0) is set and the figure is in a room whose list passes a test not read, else to 1.0, at 4 a second (0x00BF29AC) or at once. Read as the sun dimmed indoors. The fade scales rgb, not w.
    - `EnvAreaSunlightRotation` (3163), `Enabled` (3151), `ColorMult` (3153) and `ForceCharacterLighting` (3142) are pushed nowhere in `.text`, so inferred never read.
  - Unused slots are zeroed.
  - Each light is written as:
    ```
    pos register    = (L.xyz, radius)
    colour register = (Color().rgb * (+0x1BC flicker) * (+0x1B4 intensity) * fade, Color().a)
    ```
- **What the shaders read of it** [E]:
  - A level or figure shader uses one dynamic light, [9]/[11], with `g_bShaderControlBooleans[0]` set when it has one.
  - The terrain shaders take no point light at all, so an effect never lights the ground out of doors.
  - The level shader's falloff is `cos²(π/2 · d/radius)` (d at least 1 mm) × N·L, plus the shader's highlight.
  - No effect light casts a shadow in vanilla.
  - So a port matching vanilla gives each mesh or figure **the single highest-scoring effect light of its domain**, by the score above. ([10]/[12] are filled, but elgara's per-draw log says shaders read only [9]/[11]; that was not re-checked here.)
- **DAOLib's `Lights.h`** covers only the ambient term ([4], `Fill`'s base plus the +0x30 ambient lights), not point lights. An effect has no ambient-light nodes in the data seen, so effects do not touch it [I].

###### Not read

- The model-side call from the effect's vtable +0x104 (0x007E9AB0, model interface slot 1) through to `ISceneInstance::AddObject`. It is inferred, not traced.
- When `ChooseForMoving` and `ChooseForLevel` run again for an object whose effect light moves or changes radius (the +0xD0 stamp, the state's +0x210/+0x214).
- Version-1 scoring's f (0x004CB490). Whether any vanilla effect has version 1 or AffectDomain 0 (only the wisp was read). The meaning of the owner flag bit 1 and of object flag 0x10000.
- The exact constants of the wave controller's targets. Who calls the intensity setter 0x0053C880 (inferred: the cutscene actions `ActionLightIntensity`/`Color`/`Radius`, vtables 0x00B233D4/0x00B233FC/0x00B233AC).
- The shaders' own falloff and register use for [10]/[12], beyond elgara's notes.

##### Not read (spawn volumes)

- What +0x60 and +0x490 of the emitter instance are, precisely: taken as the node's world matrix and world rotation from their use [I].
- Whether anything fills SpawnVolumeMesh's +0x10 normals array (only the loader's writes were read).
- The signs of 0x004CB6D0's rotation, checked only by its form (the standard matrix from a quaternion).

