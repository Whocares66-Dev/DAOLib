# Effects

A visual effect as the game's files hold it: the table row that names it, the model that is its parts, and the clips and events that run it. The reader is `src/dao/core/Effect.h` (DAOLib #181), for DAOParagonEngine's effects (Paragon #26, `dev/PARAGON_VFX.md`'s first part). The first sections are the files; the last, how the engine plays them, is lib-agent-2's reading in the exe (the request's second part).

Read 2026-10-18 by a subagent of lib-agent over ghilana's extract of what the game takes: the 600 models `fx*.mmh`, their 549 trees `fx*.anb`, 1,091 clips `fx*.ani` with as many `.gad`, 1,070 event files `fx*.evt`, and `vfx_base.gda`'s 993 rows (tools/gff4.py, tools/gda.py, and scripts of the session's own). "By the look of it" marks what the files suggest and the exe has not confirmed.

## There is no effect file

There is no `.vfx` resource. An effect is four kinds of file, all GFF4, named for the effect: `<effect>.mmh`, a model hierarchy (MMH V0.1) whose nodes are the effect's emitters, lights and hooks; `<effect>.anb`, an animation tree (ANBT) whose nodes are its phases; `<effect>.<phase>.ani`, a clip (ANI V0.1) per phase that keys the parts' parameters; `<effect>.<phase>.evt`, the clip's events (EVT V1.0); and `<effect>.<phase>.gad` beside each clip. They are in `packages/core/data/globalvfx.rim` and again in `modelhierarchies.erf`, `modelmeshdata.erf` (an effect's `.msh`) and `anims.erf`; `fxc_wisp_c.mmh` has a newer copy in `patch/patch002.erf`, which the game takes.

## The table: Visual Effects (table 8, `vfx_`)

m2da_base names table 8 "Visual Effects", prefix `vfx_`: `vfx_base.gda` and the add-ins' `vfx_base_*.gda`, `vfx_str.gda` and kin (enciphered, not read). An ID in it is what a creature's appearance (apr_base's Crust_Effect: 3006 for `cn_wispa`) and the scripts' effects name. Its columns: ID, Label, Type, FixedOrientation, AlignToGround, ModelName, ModelName1, Size1, ModelName2, Size2, ModelName3, Size3, BlendTreeName, CrustEffect, SoundImpact, SoundImpactVolume, SoundImpactPitch, SoundDuration, SoundDurationVolume, SoundDurationPitch, SoundCessation, SoundCessationVolume, SoundCessationPitch, Tint, LoadGlobal, PauseAnimate, Priority, ShowInPaperDoll.

- ModelName is the model's file without `.mmh`, BlendTreeName the tree's without `.anb`. They are the same name in 991 rows; 928 rows name one of the 600 `fx*.mmh`, 63 another (`SR_PTY`, the selection rings; `fxm_*`; test rows), 2 none. 899 name an `fx*.anb`, 4 none. One ModelName starts with a space (4024, `" fxe_R_Ripple_aoe_P"`); the reader keeps it as written.
- ModelName1 is ModelName in every row. Seven rows (4007, a fire; 30005 and kin, the rings) name three models with their sizes: 0.35, 1.3 and 2.5 for the fires small, medium and large. By the look of it the engine takes the model whose size suits what the effect is on; not read.
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

The top struct has, besides a model's fields, MMHEmitterPresimulateTime (423 files) and, in 45, a remote material: MMHRemoteMaterialDecalName (`White.dds` in 22), FresnelFalloff, InvertFresnel, Alpha and Tint. By the look of it a material the effect lays over what it is a crust of; not read.

An emitter (`nemt`) has 59 fields of its own, every one in every emitter (the labels' names are xoreos's, `dev/gff4_fields.csv`): its material (MMHMaterialObject, of the library MMHMaterialLibrary, a `.mal`, named with its extension in 2,290 emitters and without it in 637), MMHNodeEmitBirthRate and its range, Life and its range, ScaleRange, InitialSpeed and its range, Acceleration, InitialRotation and its range, InitialRotationSpeed and its range, RotationalAcceleration, MMHNodeInvEmitterMovementSpreadUpdateDelay, SpawnSpreadX and Y (180 and 180 in the wisp's glow: by the look of it degrees, all round), MovementSpreadX and Y, GravityMultiplier, WorldAxisAcceleration (a vector: the wisp's smoke rises at 0.5), ObjectSpaceAcceleration, BirthrateInParticlesPerMeter (1 in 131), ParticlesAffectedByWind (1 in one), ParticlesFollowPath (25), LinkParticlesTogether (29: by the look of it a ribbon), UpdateOnlyWhenVisible, EnableParticleCollisions (0 in all), InheritVelocityInsteadOfPos, Bounciness, Friction, OrientationBehavior (0 in 1,868, 3 in 629, 1 in 256, 2 in 88, 4 in 86), ParticleInheritance (0 to 3), the flipbook (MMHEmitterFlipbookType 0 to 2, FramesPerSecond, Rows and Columns up to 64 and 16, RandomStartFrame), the target (MMHEmitterTargetName, set in 278, TargetAttraction, TargetRadius, SpawnDirectionTracksTarget, KillParticleWhenTargetHit), IsPhysicsEmitter (61), IsPhysicsObjectSpawnEmitter (185), the mesh particle (MMHNodeEmitMeshParticleModelName, a model such as `fx_rock`, and its up and roll axes), MmnNodeEmitVertexFormat (255 in all), UvDistributionSize, IgnoreDistortion (0), CanParticlesSplat (7), Lod (0 to 2), MMHNodeEmitUserParamName (`color`, `fire`, `Drops`: by the look of it the name a tint is given by), MMHUseVariationTint, the age map's multipliers (MMHNodeEmitAgemapColorMultiplier, ScalexMultiplier, ScaleyMultiplier), and MMHNodeEmitType.

MMHNodeEmitType is 0 in 1,887 emitters and 1 in 1,040. 1,026 of the 1s are named by an event of their clips (below), and only 98 of the 0s are; 26 of the 1s export their birth rate for a clip to key, 1,043 of the 0s. The files suggest 0 emits all the time at its birth rate and 1 when an event fires it; the runtime section reads the same in the exe (1 a burst, armed by event 16). The wisp's glow and rays are 0, keyed through its phases; its expiration ring and smoke are 1, fired at the cessation's 0.97 s.

An emitter's structs:

- `amap`, its age map (in every emitter): MMHNodeAgeMapCount and its keys, `amel` structs, each MMHNodeAgeMapElementPercentLifeElapsed (0 at birth, 1 at death, rising), ScaleX, ScaleY and Color, a particle's size and colour at that share of its life. 13,868 keys. The wisp's glow: 0.6 by 0.5 and transparent at birth, 0.56 by 0.47 and opaque at 0.1, 0.2 by 0.15 and transparent at death, blue (0.4, 0.7, 1).
- `spnv`, its spawn volume (1,145): MMHNodeSpawnVolumeType 0 a mesh (20), 1 a sphere (200, its radius), 2 a cylinder (608, its radius, length and axis), 3 a box (317, its corners); the fields of the other shapes are NaN. With SpawnWithinVolume, InvertSpawnVolumeNorms and NormalsAsDirection. A mesh volume holds two `data` structs: the points (MMHDataIsIndexStream 0, a Vector3fList) and the indices into them (1, a Uint16List), as many as MMHDataCount says (20 to 384), by the look of it a triangle list.
- `emas`, its attachments (185), each an `emat`: MMHEmitterEmitterAttachmentType (1 in 153, 4 in 32), Name (empty in all), SpawnOnSurface, UseNormalForVelocity. By the look of it what the particles are born on (the creature's mesh); not read.
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

- What the files hold that the runtime section does not explain: the attachment types 1 and 4 (`emat`), the remote material, the target groups (`emtg`), `nemg`'s own material, and which of ModelName1 to 3 the engine takes.
- What Type `T` and `C`, Tint, LoadGlobal and Priority do.
- No tree of the game's has a `vfxcast` node, the runtime's first phase; one has `vfx` (`.cust.ani` in two), which the phases do not name.
- Where the two readings differ: the runtime section reads WorldAxisAcceleration as a vec4, where the files' field is a Vector3 (type 10) in every emitter.

## As the engine plays them: read 2026-10-18 by lib-agent-2

Read in the GOG exe, the game's files, the scripts and elgara-agent's shader listings by lib-agent-2 (with subagents), for `dev/PARAGON_VFX.md` part 2 (DAOParagonEngine #26). Not seen in play. [C] read in the code or data, [I] inferred; the key claims were checked again in the disassembly.

### What an effect is

- **There is no `.vfx` resource.** An effect is a `vfx_base` row (table 8): a model (`ModelName`, `.mmh`/`.msh`) with an animation tree (`BlendTreeName`, `.anb`) whose emitters, lights and meshes are the model's nodes. `Type` is `D` (duration) or `F` (fire and forget).
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
- **The projectile's look** is `prj_base`'s `ModelName`, with `Crust_ID` at +0x4B4. The engine makes no impact effect when it lands; it queues the impact.
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
  - Half-width `W = axisB · scaleA`, averaged at joints; it faces the camera only as well as axis B does (mode 3 makes a proper ribbon). The object's scale isn't applied.
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
