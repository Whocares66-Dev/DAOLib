# What Paragon needs for effects (VFX)

Paragon (paragon-agent, 2026-10-18) draws no effect of the game's yet, and for the mage origin that is the largest gap the user sees: the Fade's wisps, the enemy of the Harrowing, are invisible. A wisp's model (cn_wispa) is a placeholder: cn_wisp_0.msh is an ordinary list of 24 triangles about 0.00007 m across, and the wisp is seen through its crust effect (corrected 2026-10-18 by lib-agent-2; it is not a point mesh with VFX.mat's ContactSheetBlend). Its look is its appearance's Crust_Effect (apr_base row 14: 3006). Every spell's conjure, cast, projectile and impact, the Fade's glows, the Rage demon's fire, fires, torches and auras are effects too. Paragon issue #26.

## 1. Core: an effect file's reader (`.vfx`, and what it names)

- The format: GFF4 (or what it is), its timeline, its emitters (sprites, ribbons, meshes, lights, decals, sounds?), their parameters over time (curves), attach points or bones, looping, duration.
- A reader in core returning a plain description Paragon can simulate and draw, like Cutscene.h's.
- The tables naming effects: Crust_Effect, ConjureVFX, VFX_Impact0/1, AppearVFX, the projectiles table (prj_base's model or effect), the effect tables (vfx_base?), and which column names the file.

## 2. The exe: how an effect plays (lib-agent-2)

- How an emitter spawns and moves its particles (rates, lifetimes, velocity, gravity, the curves' sampling), how sprites face the camera, how a ribbon trails, and the blend and depth each uses.
- How a crust effect attaches to a creature (bones or nodes), and when it starts and stops (spawn, death, activity).
- How a spell's effects are timed against its conjure, cast, projectile and impact.

## 3. The shaders

VFX.mat's families (ContactSheetBlend, the additive ones, distortion): elgara-agent's listings, which Paragon ports as it does the others.

The first use is the wisp: its crust effect drawn on the creature, so the Harrowing's enemy can be seen.
