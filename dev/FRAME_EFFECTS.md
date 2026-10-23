# Frame effects: the post-process an area runs

How the engine runs an area's frame effects (`da_bloom`, `da_fade`, `da_hsvmatrix`): where they and `areadata.gda`'s override come from, their order, each pass's shaders, states and maths, and what the exe gives them. Read 2026-10-18 in the GOG exe and vanilla's files (shaders through `fxc /dumpbin`) by lib-agent-2 (with subagents), for DAOParagonEngine's side-by-side of the Harrowing's Fade. Not seen in play. The key claims were checked again in the listings, the files or the disassembly. The sky drawn before them is `dev/SKY.md`'s; DAOElgaraShaders reads the same files.

## The Harrowing's Fade, and the effects as vanilla runs them

**What the Fade runs.** `bhm600ar_fade_harrowing.are` (layout `oth000d`, AREA_ID 62) lists in its `.arl`:
- `da_bloom`: Brightness 0.5, Contrast 0.5, GlowIntensity 0.25, HBlurWidth 3, VBlurWidth 3;
- `da_hsvmatrix`: the identity;
- `da_fade`: BlurStrength 5.

`areadata.gda` row 62 overrides the matrix's values (`dev/TODO.md`, 0x0067AFD0): ColorCurves `cc_bhm600`, HSVBrightness 1, HSVContrast 1.02, HSVHue 4, HSVSaturation 0.82. **The sepia is that curve and matrix, not `da_fade`.** `cc_bhm600.dds` (256×1) is a warm lift, red most, every channel reaching white at about 0.81 of its input.

**The files.** An effect is `<name>.ffx.xml` in `postprocesseffects.erf`, its materials `.mat.xml` in `materials.erf`, its states `.sta.xml` in `states.erf`, its shaders in `shaders.erf` (some overridden by `patch002.erf`).

**The order:** by the effect's `layer`, ascending, all at the frame's end: `da_copybackbuffer` (1), `da_bloom` (10), `da_fade` (10), `da_hsvmatrix` (20). The sort is MSVC's `std::sort`, an insertion sort below 33 elements, so the tie keeps its order (inferred). Each effect ends by copying the back buffer to `BackBufferCopy` (BBC). LightMask and Downsampled16 are half size, A8R8G8B8.

**`da_bloom`** (`dabloom.mat.xml`):

| Pass | Shaders | Reads → writes | State |
|---|---|---|---|
| GenerateAppliedLightMask | PassThrough / IlluminationMaskMult | BBC → LightMask | no blend, point, clamp |
| HBlurBloom | HorizontalBlur7 / TwoPassBlurBloom7 (5-tap at low detail and SM2) | LightMask → Downsampled16 | no blend, point |
| VBlurBloom | VerticalBlur7 / TwoPassBlurBloom7 | Downsampled16 → LightMask | no blend, point |
| FinalComp | PassThrough / BloomBlend | LightMask → back buffer | SRCALPHA, ONE; linear, clamp |

```
mask = saturate(c.rgba * (dot(c.rgb, (0.2125, 0.7154, 0.0721)) + Brightness) * (1 + Contrast))   // the 8-bit target clamps
blur: taps at uv + k*BlurWidth/WindowWidth, k = 0, ±1, ±2, ±3, each 1/7    // the full width: a step is BlurWidth/2 texels of the half target
      out = centre.a >= avg.a ? centre : avg                                 // the whole RGBA
blend: dst.rgb += mask.rgb * (mask.a * GlowIntensity)
```

**`da_fade`** (`gaussianblur.mat.xml`): a radial blur, then a distortion.

| Pass | Shaders | Reads → writes | State |
|---|---|---|---|
| RadialBlur | RadialBlur | s0 BBC, s2 `fadesaturatemask.dds` → back buffer | no blend, linear, clamp |
| FadeDistortion | PassThrough / FadeDistortion | s0 BBC, s1 `fadeeffectmask.dds` → back buffer | no blend, linear, clamp |

A copy to BBC follows each pass.
```
// RadialBlur (checked in radialblur.vsh/.psh): not uniform, sharp at the centre, most blur in the corners
n = normalize(uv - 0.5) per vertex, in the .vsh       // add, mul, add, rsq, mul; in uv, no aspect correction
tap k = uv - k * n * BlurStrength * (1/W, 1/H), k = 0..6, computed per vertex and interpolated; weights 0.30, 0.25, 0.15, 0.12, 0.08, 0.06, 0.04
// across the quad n runs from (±0.707, ±0.707) at the corners to 0 at the centre, so the step is (uv - 0.5)·√2·BlurStrength/(W, H)
g = dot(blur, (0.30, 0.59, 0.11)); out = (lerp(g, blur, satMask.a), 1)
// FadeDistortion, t = g_fApplicationTime, m = effectMask.a
vY = frac(uv.y + 0.042 t); vX = frac(uv.x + 0.048 t)
dx = (0.003 cos(2π vY) + 0.002 sin(2π·40 vY)) * (1 - m)
dy = (0.003 sin(2π vX) + 0.002 cos(2π·32 vX)) * (1 - m)
out = (0.5 * tex(uv + (dx, dy)) + 0.5 * tex(uv), 1)
```
- RadialBlur is not uniform. `radialblur.vsh` normalises (uv − 0.5) per vertex (add, mul, add, rsq, mul at its start) and computes the tap coordinates there, and they are interpolated across the full-screen quad. So the displacement runs from (±0.707, ±0.707) at the corners to 0 at the centre: the step is (uv − 0.5)·√2·BlurStrength/(W, H), the centre sharp and the corners blurred most (inferred: the post-process quad is its 4 corner vertices; the game's Fade matches it side by side, paragon-agent's 9ebe3c8; corrected 2026-10-18 by lib-agent-2, the listing checked).
- A positive BlurStrength steps toward the centre (5: taps in the corners out to 30 pixels); a negative one outward (lak511d: −3.5).
- `fadesaturatemask.dds`'s alpha falls from 255 within about 0.2 of the radius to 65 at 1.0 and about 36 in the corners, so **the edges are greyed**, not blurred more.
- `fadeeffectmask.dds`'s alpha is 255 to about 0.4, 121 at 0.8, 0 in the corners, so the ripple and ghosting are at the edges.

**`da_hsvmatrix`** (`hslmatrix.mat.xml`; s0 BBC, s1 the area's colour curve):
```
rgb' = Hue(H) * Sat(S) * (B*C*rgb) + 0.5*(1 - C)
Sat[i][j] = (1 - S)*w_j + S*δij, w = (0.2125, 0.7154, 0.0721)
Hue: rotation about the grey axis by H degrees: diagonal cos + (1 - cos)/3; R row ((diag), (1 - cos)/3 + sin/√3, (1 - cos)/3 - sin/√3), the others cycled
out = (curve(rgb'.r, 0.5).r, curve(rgb'.g, 0.5).g, curve(rgb'.b, 0.5).b, 1)
```
For the Fade: R' = 0.8741R + 0.1657G − 0.0198B − 0.01; G' = 0.0060R + 0.9664G + 0.0476B − 0.01; B' = 0.0734R + 0.0983G + 0.8483B − 0.01.

**The exe:**
- An effect runs (`FrameBufferEffect` slot 1, 0x004600B0) when enabled and its `minlod` (+0x84) ≤ the detail setting [0x00BF22C8] (inferred). `da_fade`'s minlod is 1, so it is off at the lowest.
- The `.arl`'s floats are clamped to the file's `range` (0x0045DDE0; BlurStrength −20..20) and copied in by 0x0045F490.
- The engine's own values (0x0045DF70, updated at the frame's start by 0x0045EEE0): back-buffer width and height, `effect_time` (+dt, reset at stop) and the focal plane, which is the scene camera's far plane (answered 2026-10-20: "da_dof" below). `ApplicationTime` (usage 0x29) and `ColorCurveTexture` (0x32) come from the table at 0x00509870; `ApplicationTime`'s source is read 2026-10-23 by lib-agent (below).
  - **Its value** [C]: the float-parameter provider 0x004F88C0 (the dispatcher 0x004F9250 sends float parameters to it; the game-side 0x004F8A10 sends 0x29 to its default, false) loads the double at `g_graphics` (0x00C31650, an `Eclipse::IGraphicsInternal`, vftable 0x00AF6D2C) `+0x40` for usage 0x29 (0x004F88DA). It is handed to the shader as a float.
  - **Its clock** [C]: `IGraphicsInternal` slot 25 (0x0052AE80, `__thiscall(float dt)`, `ret 4`) stores dt at `+0x3C` (the frame effects' `effect_time` adds this one), then adds it to the double at `+0x40` unless the byte at `+0xCA` is set. Slot 27 (0x0052AE10) sets that byte. The one call to slot 25 is EclipseGame's frame update (0x00402690, from EclipseGame::Update 0x00412EB0): dt is the game object's second `EC::ITimer` (`+0xB8`), updated with the fixed rate at `+0x254`, its seconds (slot 2, the microsecond clock's seconds since its last update, or one over the fixed rate) also kept at the game's `+0xC0`. So it is real seconds, a frame at a time.
  - **Where it starts** [C]: the renderer's constructor (0x0052AC20 area) sets `+0x40`, `+0x48` and `+0x3C` to 0. A scan of the exe finds no other store to `+0x40` than slot 25's sum. So it counts from the renderer's creation at the game's start and is not reset by an area load.
  - **What stops it** [C]: `CClientExoApp::SetPaused` (0x005D6740) calls 0x005D5540(paused) at 0x005D67D7. That function pauses or resumes the world timer (`app+0xD4`, 0x00A35DE0 / 0x00A35E20), tells the scene (0x008107F0, slot 5), calls slot 27 with the same flag (0x005D55E3), and keeps it at `app+0x100`. So while the game is paused, ApplicationTime holds. The cutscene mode's enter (0x00692980) pauses the app too (0x005D5540(1) at 0x00692B17), stopping the world timer, but then calls the scene's slot 5 and the renderer's slot 27 with 0 (0x00692B29, 0x00692B5B; ebx is 0 from 0x006929C4). So ApplicationTime keeps running through a cutscene [C]; its leave (0x00692450) calls 0x005D5540(0) unconditionally (edi is 0 from 0x00692490; the push at 0x006925F4) [C] (corrected 2026-10-23 by lib-agent-2: it was noted as the state it kept) (corrected 2026-10-23 by lib-agent: it was noted as holding through a cutscene).
  - **Seen** [S, paragon-agent]: the clouds stop under photo cam's pause, and match a clock that counts from the game's start, in seconds, shortly after a load. That fits: the time since start and the time since the area loaded differ by the load, so "the same clouds shortly after a load" holds only while little time has passed before it [I].

## da_dof: depth of field (read 2026-10-20 by lib-agent-2)

Read in the GOG exe and vanilla's files (`da_dof.ffx.xml`, `dof.mat.xml`, the shaders' fxc listings; patch002's copies have the same code). [C] = checked, [I] = inferred.

### Targets, passes and widths

- **A target's `.width` and `.height` in the ffx are constants baked at parse** [C]: the filter parser (0x00460590) splits `target.member`, and `width` is `(float)(int)[target+0x2C]` (`fild` at 0x0046091E), `height` [target+0x30], wrapped in a constant node like a literal. The target's own attribute parser (0x0045DBF0, Size 0x0045DA20) sets them from `BACKBUFFER_WIDTH` / `BACKBUFFER_HEIGHT` with one `*n` or `/n`. Whether a resize parses them again is not read.
- **The targets:** BackBufferCopy is the back buffer's size; DOFDownsampledCoC, DOFBlurredA and DOFBlurredB are half its width and height (960 × 540 at 1920 × 1080). `WindowWidth` / `WindowHeight` are runtime values, the full back buffer.
- **The passes, each run once** (one stage `rangeDOF` on EndFrame; no repeat attribute) [C]:
  1. CreateCoCTexture: the back buffer copy and depth into DOFDownsampledCoC, its texel from the full window size.
  2. CoCBlurH into DOFBlurredA, `fWindowWidth` = `DOFBlurredA.width` (960), `fBlurWidth` = WeightBlur.
  3. CoCBlurV into DOFBlurredB, `fWindowHeight` = `DOFBlurredA.height` (540).
  4. ArtifactBlur (SmallBlur) back into DOFBlurredA, its window DOFBlurredB's size, `fBlurWidth` = ArtifactBlur.
  5. FinalDoFComposition onto the back buffer, SRCALPHA / INVSRCALPHA.
  6. Finish: the back buffer copied to BackBufferCopy.
- **Taps** [C]: the blurs' vertex shaders step `s = fBlurWidth / fWindowWidth` (or height), seven taps at k = −3 … 3 with weights 0.05, 0.1, 0.2, 0.3, 0.2, 0.1, 0.05 over rgb and the CoC in alpha, then `a = 2·max(blurred.a, centre.a) − centre.a` (the centre from DOFDownsampledCoC). So at WeightBlur 0.81 a step is 0.81 of a half-resolution texel, 1.62 screen pixels. SmallBlur averages the four corners at ±0.5 step.

### The circle of confusion [C]

- **Downsample** (CoCDownsample): rgb is the average of four point-sampled taps around the pixel at ±1 full-resolution pixel; for each of four depth taps, `d = dot(depth.rgb, (1/65536, 1/256, 1)) × focalplane` and `coc = saturate((FocusDistance − FocalRadius − d) / max(NearFade, 0.001))`, and alpha is their largest: the near CoC alone.
- **Compose** (ComposeDoF): `d` from the full-resolution depth, point sampled; `far = saturate((d − (FocusDistance + FocalRadius)) / max(FarFade, 0.001))`; `b` = DOFBlurredA point-sampled (half resolution); out = (b.rgb, max(b.a, far) × MaxBlur), or the CoC as grey when RenderBlurRadius is 1 (a debug view).
- FocusDistance is the centre of the sharp band and FocalRadius its half-width; NearFade and FarFade are the ramps in metres either side; MaxBlur scales the final alpha.

### The focal plane and the depth target [C]

- **`scene_focalplane`** (runtime kind 3) is the global 0xBF2200, copied each frame by 0x0045EEE0 (0x0045EFB2). 0x00455D50 sets it each frame to the scene camera's far plane, its vtable slot 16 (0x00465760, `[cam+0x134]`; 4000 in play, `src/dao/Camera.cpp`). The scene shaders' `g_fFocalPlane` is the same global, so a far plane a plugin changes moves the packing and the DOF together.
- **The scene's second target (oC1) packs the view distance** (vanilla `static1_fast.psh`'s tail): x = saturate(distance / g_fFocalPlane); f = frac(x · (16777246, 65536, 256, 1)); oC1.rgb = f.yzw − f.xyz / 256; oC1.a = 1. `static_depth.psh` (the depth-only pass with antialiasing) writes the same to oC0. The DOF decodes it as `(r / 65536 + g / 256 + b) × focalplane`, point sampled, clamped.

### Conversations and cutscenes [C unless marked]

- **`conversation_dof_values`** (table 203; `conversation_dof_parameters`, 204, names the columns to apply) is read only by GameModeDialog's constructor (0x0068FA70). It takes the first row whose FocusDistance is negative, ID 8 (FocalRadius 8, WeightBlur 0.5, ArtifactBlur 1.3, NearFade 1, FarFade 20, MaxBlur 1), and sets the dialog's own da_dof instance from it once. Rows 1 to 6 are used by nothing found.
- **The per-frame focus:** the dialog's update sets the speaker's distance from the camera under the key "focus" (0xB0E25C). da_dof has no such parameter (an exact map lookup), so FocusDistance stays at the ffx's 5 [I].
- **When the dialog's instance draws:** each line's start (0x0068E250) takes it out of the effect manager, and puts it back (0x0068E6EB) only for a line with no cut. The dialog's end deletes it. Nothing fades.
- **A cut's own `da_dof`** is its AFBE action (type 19, ActionFrameBufferEffect, vtable 0xB23290): its own instance, added to the manager on its first live frame, every parameter set each frame from its curve or base value with no easing, enabled while its actor is the cutscene's active camera actor [I for that reading of +0x1D8], and removed at once by the action's slot 2 at the cutscene's reset or end. Reaching the action's stop time does not remove it [I].

### Not read

- The oC1 target's format and any antialiasing resolve; what interpolant the shaders divide as the view distance.
- Whether a resize parses the ffx's baked widths again; Size's `+` and `−`; the case-insensitivity of the names.
- The blurs' sampler filters (GaussianBlurCoC.sta).
- Whether rows 1 to 6 of table 203 are used elsewhere (a list at 0x0086FB26); the "focus" key's fate in play.
- The dialog's other branch at 0x0068E476; what ends a line's cutscene when it finishes by time.

## The back buffer's alpha in vanilla, as the bloom reads it

Read 2026-10-18 by lib-agent-2 in the GOG exe and vanilla files. [checked] = read in a listing, a state file or the exe. [inferred] = not read directly.

Sources: per-shader outputs from `C:\project\DAOElgaraShaders\build\scratch\materials\outputs.txt`, with the tails spot-checked in `listings\*.psh.asm`. States are vanilla `sta\*.sta.xml`, tabulated by `%TEMP%\lib-agent-2-bba-states.py`. Pass to state comes from `matmap.txt`. Post-process shaders are in `%TEMP%\lib-agent-2-fx\asm\`.

**Caution:** `%TEMP%\lib-agent-2-fx\ffx\da_bloom.ffx.xml` and `da_hsvmatrix.ffx.xml` are DAOElgaraShaders' overrides, not vanilla. `DaoAlphaIsShare` and the `DaoSmaa`/`DaoOcclusion` passes are Elgara's. The vanilla `da_bloom.ffx.xml` in `postprocesseffects.erf` (read through ghilana, source core, 4746 bytes) has no DaoAlphaIsShare. `da_copybackbuffer.ffx.xml` exists only in core, so it is vanilla.

The sources were checked against vanilla's `postprocesseffects.erf`, not the installed DAOElgaraShaders overrides, which replace `da_bloom.ffx.xml`, `da_hsvmatrix.ffx.xml` and the illuminationmask, illuminationmaskmult and hslmatrix shaders. The vanilla passes described here are unchanged, `da_fade` and `da_copybackbuffer` are identical, and the listings are vanilla's (compiler 9.19.949.2111).

### The bloom's use of alpha (correction to the brief)

- **IlluminationMaskMult.psh** [checked]: `oC0 = c.rgba * k`, with `k = (dot(c.rgb, (.2125,.7154,.0721)) + fBrightness) * (1 + fContrast)`.
  - There is no saturate in the shader. The 8-bit LightMask target (A8R8G8B8) clamps the result.
  - So `mask.rgb = c.rgb*k` does NOT depend on alpha. Only `mask.a = saturate(a*k)` does.
  - State is PPQuadStateNoBlend: no blend, point sampling.
- **TwoPassBlurBloom5/7** [checked]: `r1.x = centre.a - 0.2*sum.a`, then `oC0 = r1.x >= 0 ? centre : sum/5`.
  - Alpha chooses per texel whether the blur keeps the centre tap or takes the 5-tap mean. This happens in both blur passes.
- **BloomBlend.psh** with BloomBlend.sta [checked]:
  - Shader: `oC0 = (m.rgb, m.a*fGlowIntensity)`.
  - State: SRCALPHA/ONE and no separate alpha blend.
  - Result: `back.rgb += m.rgb * m.a * G` and `back.a += (m.a*G)^2`.
- **So the glow is about `c.rgb * k * saturate(a*k) * G` (before the blur).** Alpha is the blend factor, and the clamp hides alpha below 1 wherever `a*k >= 1`.
  - At the default B = C = 0.5, k runs from 0.75 to 2.25.
  - Example: a pixel of luma 0.5 has k = 1.5, so any alpha of 0.67 or more gives the same glow as 1.

### 1. What the scene leaves in alpha

**Back buffer** [checked]:
- Its format defaults to A8R8G8B8. `D3DGraphicsDriverManager` vtable 0xb2db7c, slot 11 = 0x865420, builds the 0x48-byte creation parameters.
- Adapter format at +8 defaults to 0x15. BackBufferFormat at +0x18 defaults to the adapter format unless the caller supplies one.
- A caller-given format was not traced [inferred, not seen]. DAOElgaraShaders found a forced alpha survives in the scene's target (NOTES.md:218-219).

**Clear** [checked]:
- The render view's constructor (0x45a9f0) sets clear flags 3 (TARGET | ZBUFFER) and colour (0,0,0,1) at +0x50..+0x5c.
- 0x4598a0 issues it: SetClearColor at driver slot 12 (0x868490, floats ×255 to ARGB), then Clear at slot 11 (0x8685f0).
- Three explicit overrides exist:
  - 0x7bba7b sets (0,0,0,1).
  - 0x7c0391 (near GUI::GUIPaperdoll) sets (0,0,0,0).
  - 0x7f4189 sets (0,0,0,0).
- That the main game view keeps the default (0,0,0,1) is [inferred].

**Alpha write mask** [checked]:
- DefaultState.sta has COLORWRITEMASK 15 and SEPARATEALPHABLENDENABLE FALSE. The exe loads it as the material system's default: string at 0xc3102c, used at 0x4fcb1b and 0x4fc7ac.
- No scene state masks alpha off. Only these set COLORWRITEMASK to something other than 15:
  - 0: occlusion queries, contribution tests, HairAlphaPass1 (depth only).
  - 3: PSSMStatic (shadow map).
  - 8: PunchThroughMask (alpha only).
- **The mask is sticky** (read 2026-10-19 by lib-agent-2) [checked]: the state manager applies only the device states a state file lists, and 152 of the 178 states in `states.erf` list no `DS_COLORWRITEMASK`, all the DAMsh* scene states among them. `DefaultState` is the manager's initial state (0x4FCB1B) and the one a pass with no valid state gets (0x4FC7AB, 0x4FC7D8); it is not applied again under each pass. Every state that sets a mask other than 15 is a depth, shadow or query state, and one that sets 15 follows them in practice (DepthPass, SPT_*, PSSM). That nothing leaves 0 or 3 standing into the scene is [inferred], with DAOElgaraShaders' read-back of the picture's alpha (its NOTES.md:218).
- **With antialiasing on,** the depth pre-pass draws the opaque materials with `DepthPass.sta` (mask 15), and the target is then cleared to (0,0,0,1), so the pre-pass leaves nothing in the alpha (DAOElgaraShaders' trace at 8 samples, NOTES.md:218).

**Separate alpha blend** [checked]:
- Only DistortionMask.sta, WeaponTrail.sta and PPQuadState.sta enable it.
- DistortionMask and WeaponTrail draw into the DistortionMask target, not the back buffer (`da_distortion` binds DistortionMask at StaticGeomEnd). WeaponTrail drawing there is [inferred].
- PPQuadState is used only by the unused `GenerateLightMask` and by `dof` passes.
- So **every blended scene pass blends alpha with its colour factors**:
  - SRCALPHA/INVSRCALPHA: `a' = a_s^2 + a_d*(1-a_s)`. Over a_d = 1 this is `1 - a_s + a_s^2`, between 0.75 (at a_s = 0.5) and 1.
  - SRCALPHA/ONE (additive): `a' = a_s^2 + a_d`. It stays 1 over a solid.

**Fog** [checked]: never touches alpha. Every lit shader ends `oC0.xyz = lerp(fogColour, colour, fog)` and writes oC0.w separately.

**Per material (oC0.w, then state):**

| Surface | oC0.w | State | Alpha left in the back buffer |
|---|---|---|---|
| Terrain1/2/3 | literal 1 (`c32.z`) | DATerrain: no blend | 1 [checked] |
| Level and room meshes: Static1, _fast, _relief, Static2, Static3, BakedLowLOD, Vista3 | `mml_fOpacityMultiplier` | DAMshOpqe*: no blend | the opacity multiplier, 1 where the .mao does not set it (below) [checked] |
| Characters: Ch1/Ch2 (non-`alph`), Face0/2, Weapon1/2, Prop1/2, Cloth1 | `mml_fOpacityMultiplier` (Cloth1/Prop2: × texture alpha) | Character, Face, Weapon, Prop: no blend | the opacity multiplier, 1 where the .mao does not set it [checked]; Cloth1 and Prop2 carry their texture alpha |
| Eye0 | vertex colour alpha `v0.w` | Character | v0.w, presumably 1 [inferred] |
| Cut-outs: Static1_Punch, Prop1_Punch | `decal.a * opacity`, texkill below 0.3 | DAMshPnch*: no blend, alpha test > 127 | **the texture alpha, 0.5 to 1** [checked] |
| SpeedTree leaf cards, fronds, billboards | `tex.a * g_fAlphaMultiplier` (billboards: `* v0.z`) | SPT_*: no blend, test > 90 or > 84 | **the texture alpha, at least about 0.35** [checked] |
| SpeedTree branches | `max(r0.x, g_fAlphaMultiplier)` | no blend | 1 or more, clamped [inferred] |
| Character cut-outs and blends: Ch*Alph* | `tex.a * opacity` | Character_alpha / CharacterAddvPT2S: SRC/INVSRC, test ≥ 128 | about 0.75 to 1 over a solid [checked] |
| Character cut-outs: CharacterPT | `tex.a * opacity` | no blend, test ≥ 128 | 0.5 to 1 |
| Hair, LOD1 (HairAlpha passes) | `hairTex * v0.w` | pass 2: no blend, CWM 15; passes 3-4: SRC/INVSRC | the hair's alpha, then blended [checked] |
| Hair, LOD2 (Hair2 @ Character.sta) | same | no blend | the hair's alpha, written raw [checked] |
| Sky | `mov oC0.w, c10` = `mml_fOpacityMultiplier` | Sky.sta: no blend, CWM 15 | the multiplier, 1 per the brief. The area's "atmosphere alpha" is the fog lerp `c9.x` on rgb, not oC0.w [checked] |
| Clouds | `cloud * opacity` | SRC/INVSRC | `1 - a + a^2` over the sky |
| Water0, WaterFlowing0 | computed `mad` / `max` | Water: SRC/INVSRC | `1 - a + a^2`, from 0.75 to 1 |
| VFX0, alt, atm, dba | `tex.a * vertex alpha` | DAEmtNorm*: SRC/INVSRC | 0.75 to 1 over a solid |
| VFX0 under DAMshOpqe1S | same | no blend | **the raw particle alpha** (rare) |
| VFX0_Addv (additive) | `min(a, max(rgb))` | DAEmtAddv*: SRCALPHA/ONE | stays 1 over a solid; raised where below 1 |
| Static1_Alpha, PropAlpha1 | `tex.a * opacity` | DAMshNorm1S: SRC/INVSRC | `1 - a + a^2` |
| PunchThroughMask.psh | `saturate(tex.r + c0.x)` | ONE/ZERO, CWM 8 (alpha only) | writes alpha outright [checked]; never drawn, since no .mao uses it (2026-10-19) [checked] |

**A material's Float that the .mao does not set is the definition's `Default` at draw** (read 2026-10-19 by lib-agent-2): the `.mat` definition's parse (0x0050A4A0, 0x005093D0) keeps the Default on the definition's parameter, the .mao's own map (+0xD0) holds only the `<Float>`s it names, and `MaterialArtFloat`'s slot 5 (0x0050F180) falls back to the Default. DAOElgaraShaders saw it in play (its NOTES.md:192 and :194: a Default of 1 made every figure metal, 0 none). So `mml_fOpacityMultiplier` is 1 on a mesh whose .mao lacks it (`static.mat.xml:30`), not 0.

There is **no glow or bloom mask written into alpha.** No vanilla scene shader writes an emissive term into oC0.w; the `em` character variants write `mml_fOpacityMultiplier` like the rest. The second target's alpha (oC1.w, a literal 1) is the distance target and is not the back buffer.

### 2. da_copybackbuffer

- It is a plain copy [checked]. Layer 1, at StaticGeomEnd and at EndFrame: `copy BACKBUFFER to BackBufferCopy filter NONE`.
- `FBICopyBackbufferToTarget` (vtable 0xae77ac, slot 1 = 0x45d7b0) calls `D3DGraphicsDriver` slot 51 (0x869900):
  - `GetBackBuffer(0, 0, MONO)`, then
  - `IDirect3DDevice9::StretchRect(backbuffer, NULL, target, NULL, D3DTEXF_NONE)`.
- No shader and no state. Both surfaces are A8R8G8B8, so alpha copies through unchanged. With MSAA on, StretchRect resolves, so alpha is averaged at edges.
- Between the scene and the bloom, two things rewrite the back buffer, both carrying or blending alpha:
  - `da_distortion` (layer 3) writes the copy's RGBA back with no blend (Distortion.psh `oC0 = texld(BBC)`; PPQuadNoBlendMirror).
  - `da_dof` (layer 5), when active, composes with SRC/INVSRC (ComposeDoF.sta), giving `1 - a + a^2`. When DOF runs (conversations, cinematics) is [inferred].

### 3. A typical frame, as the bloom's first pass reads it

- **Opaque level geometry, terrain, characters, faces, weapons:** 1. At 1 the glow is `c*k*saturate(k)*G`.
- **Sky:** `mml_fOpacityMultiplier`, 1 per the brief (the sky shader writes it straight, with no blend). Under clouds, 0.75 to 1.
- **Foliage, fences, cut-out props, hair, LOD2 hair:** their texture alpha, about 0.35 to 1 for trees, 0.5 to 1 for punch-through. This lowers their bloom only where `a*k < 1`, mostly dim texels.
- **Under alpha-blended effects (water, particles, glass, character alpha cards):** `1 - a + a^2`, from 0.75 to 1, never lower over a solid.
- **Under additive effects (spells, fire, glows):** stays 1 (an additive pass only raises alpha).
- **Pixels nothing draws:** 1, from the (0,0,0,1) clear (the main view's clear is [inferred]); their rgb is 0, so no glow.
- **Open, from play:** DAOParagonEngine's frame work measured vanilla at 8 samples (the Circle tower and the Fade, 2026-10-19). There, `da_bloom` adds nothing visible on opaque level geometry, the terrain included, but does on the sky.
  - **Alpha is not the reason** (elgara-agent's capture the same day, vanilla, Redcliffe, at 8 samples and at none). Opaque geometry's alpha is about 1 before `da_bloom`, `mml_fOpacityMultiplier` is 1.0 on every draw, and `COLORWRITEENABLE` is 15.
  - **Nor is the shader.** The bloom's first pass, captured as hash f96e4b9d285176af, is the FNV-1a 64 of `illuminationmaskmult.psh.fxo` from `patch002.erf`, the copy the game takes. Disassembled with `fxc /dumpbin`, it is the same code as `shaders.erf`'s (only the debug path differs): k = (luma + fBrightness)(1 + fContrast), oC0 = c·k.
  - **Still open: the inputs,** the `fBrightness`, `fContrast` and `fGlowIntensity` vanilla applies per area, against what Paragon applies.
- **A peer port that keeps alpha at 1 everywhere** matches vanilla on solids, sky and additive effects. It differs only on cut-outs and hair (vanilla weaker where `a*k < 1`) and slightly under blended effects. It must keep the blur's `centre.a >= mean.a` selection to match vanilla's bloom shape.

## The blur behind full-screen GUI screens (guiblur; lib-agent-2, 2026-10-19)

GOG exe, base 0x400000. [C] = checked (listing, files), [I] = inferred.

### Short answers

1. **It is a frame effect: `da_blurdesaturate`** (`postprocesseffects.erf`, layer 15, minlod 0). Its own header says: "blur and desaturate the 3D scene when the in game GUI panel is brought up (inventory, character, journal, etc)". Not da_dof, not the scene's `b` bit 2 [C]. No snapshot, no img:// texture, no world-rendering switch: the world keeps rendering and the effect reworks the live back buffer at the frame's end, before the Flash movies are drawn over it [C for the effect's passes; I for "before the movies" from FRAME_EFFECTS.md].
2. **The lead was wrong: `b` bit 2 is a sound.** 0x00A55830 `b & 2` calls singleton getter 0x00510CA0 (global 0xC315EC, 0x58-byte object, vtable 0xAF6588 `GUI::GUIManager`), then 0x00522C30(2, L"Click"). That is the GUI sound row 2 "open book" (column "Click", string 0xB05DAC), already in dev/AUDIO.md:384 (caller 0x00A5589A). It plays a click and does nothing visual [C].
3. **The switch is `b` bit 8** (the book frame), not bit 2. GUIScene Show 0x00A55830: `b & 8` calls `DragonAgeGUISystem` slot 25 (+0x64, 0x0060B1A0) with 1, then ShowScene(0x3B BookBack). Hide 0x00A55910: `b & 8` calls the same slot with 0. Slot 25 turns the effect on (arg 1: fade in) or starts its fade out (arg 0) [C]. Scenes with bit 8: Inventory (8, also the loot panel), CharacterRecord (0), Journal (9), Map (0xF), WorldMap (0x10), Tactics (0x17), Store (0x20), Crafting (0x26), LevelUp (0x2E), and the movieless Skills (0xD) and SpellsTalents (0xA) [C, table at GUI.md:120-182]. Not blurred: ItemUpgrade, CharGen, PartyPicker, Options, MainMenu, SaveLoad, Conversation (b = 0).

### Numbers a port can copy [C unless marked]

Parameters (`da_blurdesaturate.ffx.xml`, no engine or area override; the exe sets none of them): HBlurWidth 3, VBlurWidth 3 (range 0-50), Hue 1.0 (degrees: a 1 degree rotation, practically identity), Saturation 0.5, Brightness 1.0, Contrast 1.0, TimeToFade 0.25, FadeDirection 1 (default; the engine writes it), FadeOutStart 0.

Targets (all A8R8G8B8, shared with bloom): BackBufferCopy (BBC) full size; LightMask and Downsampled16 half size (BACKBUFFER_WIDTH/2 x HEIGHT/2).

Passes, in the stage `EndFrame`, effect layer 15 (after da_bloom/da_fade at 10, before da_hsvmatrix at 20; at medium or lower detail the SM2 material differs, see below):

| # | Material semantic | Shaders | Reads -> writes | State |
|---|---|---|---|---|
| 1 | GaussianBlur `HBlur` | HorizontalBlur.vsh, TwoPassBlur.psh | BBC (full) -> LightMask (half) | no depth, no blend, linear, clamp (GaussianBlur.sta) |
| 2 | GaussianBlur `VBlur` | VerticalBlur.vsh, TwoPassBlur.psh | LightMask -> Downsampled16 (half) | same |
| 3 | HSLMatrix `HSLMatrixFade` | HSLMatrix.vsh, HSLMatrixFade.psh | Downsampled16 (s0) + BBC (s2) -> back buffer | HSVMatrixBlend.sta: no blend; s0 linear clamp |
| 4 | copy | | back buffer -> BBC | |

Kernel (TwoPassBlur.psh ps_2_0, identical in `data/shaders.erf` and `patch002.erf`; the vertex shaders compute the taps): 7 taps, weights **0.05, 0.10, 0.20, 0.30, 0.20, 0.10, 0.05** (sum 1; alpha forced to 1; rgb only).
- Horizontal: `t = HBlurWidth / backbuffer_width` (UV units), taps at `u + k*t`, k = -3..3, same v. Vertical: `t = VBlurWidth / backbuffer_height`, taps at `v + k*t`.
- So the step is **3 full-resolution pixels** per tap, 1.5 texels of the half-size target; the support is +-9 px; the 1-D standard deviation is sqrt(2.1)*3 = **4.35 full-resolution pixels** per axis. It is a fixed pixel size, not a fraction of the screen: it looks weaker at 4K than at 1080p [I for "looks"].
- Pass 1 samples the full-size BBC with a bilinear fetch into the half-size target, so it also downsamples 2x.

Pass 3 (HSLMatrixFade.psh ps_3_0 and HSLMatrix.vsh, vanilla copies of `shaders.erf`):
```
// per frame: Time = effect_time (seconds since the effect was enabled), Dir = FadeDirection, Start = FadeOutStart, TTF = TimeToFade (0.25)
a = saturate(Time - Start)
x = (1 - a) * TTF * Dir + Time * (1 - Dir)
t = saturate(x / TTF)
M = Hue(1 deg) * Sat(0.5) * (Brightness 1 * Contrast 1), offset 0.5*(1 - Contrast) = 0    // the matrix from HSLMatrix.vsh; formula in FRAME_EFFECTS.md "da_hsvmatrix"
blurred = Downsampled16(uv)  (linear)         // upsampled from half size
orig    = BBC(uv)
rgb = orig + t * (M * blurred.rgb - orig)     // oC0.xyz; alpha = 1
Sat(S)[i][j] = (1 - S)*w_j + S*delta_ij,  w = (0.2125, 0.7154, 0.0721)   // S = 0.5: halfway to grey
```
- Fade in (Dir 0): t = saturate(Time / 0.25), a quarter-second ramp.
- Fade out (Dir 1): t = 1 - saturate(Time - Start), a ramp over **1.0 second**, not TTF, because the shader does not divide the `1 - a` term by anything but TTF*Dir/TTF = 1 [C in the listing]. Start is the effect's current Time when the fade-out is asked for.
- The pixel shader does not read the colour curve (s1), so the area's colour grade does not enter here [C]. DAOElgaraShaders overrides `hslmatrixfade.psh.fxo` (1,484 bytes against vanilla's 704) and `hslmatrix.mat.xml`, so a player with it installed sees its version of pass 3; the numbers above are vanilla's.
- At shader model 2 the material uses SM2_HSLMatrix.psh (no fade term, uses the colour curve) [C from the .mat.xml and listing]; ignore for a port.

Result: the world at full resolution is replaced, within 0.25 s, by a 4.35 px (per axis) gaussian-ish blur, half-desaturated (saturation 0.5), brightness and contrast unchanged. Even over the whole screen, not radial [C].

### Where it is turned on and off [C]

The effect lives in a 0x20-byte plain struct at `CClientExoApp+0x154` (made at 0x005DBFC4-0x005DBFE2 by the ctor 0x0064C3F0; freed at 0x005D750A). Fields: +0x00 the `da_blurdesaturate` effect, +0x04 its `Time` value, +0x08 `FadeOutStart`, +0x0C `FadeDirection`, +0x10 state (0 in, 1 out, 2 off; starts 2), +0x14 the `da_colorfade` effect (a separate fade-to-colour), +0x18 its `Time`, +0x1C its fade value.
- **0x0064C630** looks the effects up by name on first use (wide string 0xB06B7C `da_blurdesaturate`, 0xB06BF0 `da_colorfade`; value names `Time` 0xB06BE4, `FadeOutStart` 0xB06B60, `FadeDirection` 0xB06B44), disables the blur effect, and adds it to the effect manager (0x00457400 on 0x00C1EAC4). Called from 0x005DBFF1 at start-up.
- **0x0064C420(mode)** is the state setter (ret 4): mode 0 = fade in: if state is not already 0, `SetEnabled(blur, true)` (0x0045FA10) and `FadeDirection = 0`; mode 1 = fade out: `FadeOutStart = current Time` and `FadeDirection = 1`, enabled stays; mode 2 = `SetEnabled(blur, false)`. The state is stored last.
- **0x0060B1A0 = `DragonAgeGUISystem` slot 25** (vtable 0xB03504 + 0x64; the only direct caller is the vtable). With arg `on`: sets a flag object at GUISystem+0x268 (`slot 0x54(on)`); if `on` is 0 it also gets scene 0x3B and calls its +0xC0 member slot 0x54 with 0 (inferred: starts the book's outro); then tail-jumps to 0x0064C420 with mode `on ? 0 : 1` (0x0060B1E9-0x0060B1EF).
- **On**: GUIScene Show 0x00A55830, 0x00A558D4-0x00A558FD (`b & 8`): slot 25(1), then ShowScene(0x3B).
- **Fade out begins**: GUIScene Hide 0x00A55910, 0x00A55975-0x00A55987 (`b & 8`): slot 25(0).
- **Off for good**: `GUIBookback` (BookBack, 0x3B) Hide, vtable 0xB17068 slot 6 = 0x00717090: base Hide 0xA55910, then `0x0064C420(2)` at 0x007170A5. BookBack's movie plays its `outro` label and at frame 51 sends `GUIBookback.HideBookBack` (as_bookback/scripts/frame_51/DoAction.as:2), which hides the scene [C for the message in the AS; I for the chain from the message to Hide]. So the blur ends when the book's outro ends, not when the panel closes; if the outro is longer than the 1 s fade-out, the effect is at t = 0 (visibly unblurred) before it is disabled.
- A scan of the code for `mov reg,[reg+0x158]` followed by a call of slot 0x64 found only those two (Show and Hide); no other code triggers it [C for the scan, which covers this addressing form only].

`da_dof` is not involved (it is the conversation/cutscene depth of field; no reference from this path).

### Does a movie draw a snapshot? No [C]
Grep of the decompiled movies under `%TEMP%\lib-agent-2-guicmd-x\allgfx\` for snapshot, backbuffer and blur finds nothing outside the shared `StickyButton.as` library text. `bookback.gfx` (`as_bookback`) is the book frame art (`BookBack.as` plays `intro`/`outro`; a token `GUIBookback.ShowBlack` shows an optional black full-screen `BlackBack` clip). The blurred world is the engine's, drawn into the back buffer before the movies.

### For a port
```
on screen open (scene.b & 8):  blur.enabled = true; blur.time = 0; dir = 0
on screen close (scene.b & 8): dir = 1; start = blur.time        // fade out over 1 s
when the book (BookBack 0x3B) hides: blur.enabled = false
each frame while enabled (after the world, before the UI):
  blur.time += dt
  H = blurH(backbuffer -> half-size, taps k*3px/W, w = [.05 .1 .2 .3 .2 .1 .05])
  V = blurV(H, taps k*3px/H, same weights)
  t = dir == 0 ? saturate(time/0.25) : 1 - saturate(time - start)
  out = lerp(world, SatMatrix(0.5)*V, t)      // Hue 1 degree ignored
```

### Not read
- `effect_time`'s exact behaviour past its declared range (0 6): whether it clamps at 6 s, which would hold `Time - Start` at 0 if a screen was open longer than 6 s (the fade-out would then not advance). The update is 0x0045EEE0 (not read through).
- What the flag object at GUISystem+0x268 and the member at BookBack+0xC0 are (slot 0x54 targets).
- The BookBack outro's length in seconds (frame labels and rate), so how long the blur lasts after a screen closes.
- Who else enables `da_blurdesaturate` by script: the FB_SetEffectEnabled command can (the toolset scripts do not mention it), not searched in module scripts.
- The sampler state of s2 (the original) in pass 3 (GaussianBlur/HSVMatrixBlend states set stages 0 and 1 only), so whether it is point or linear (same result at 1:1).
- Whether the world's own rendering (shadows, particles) changes while a full-screen screen is up; the game is paused by `GameModeGUI` (GUI.md), so the picture is static apart from effects that run on real time.
- Scratch: `%TEMP%\lib-agent-2-guiblur-x\` (twopassblur, horizontal/verticalblur, hslmatrix) and `-x2\` (hslmatrixfade, states), extracted with ghilana (`all_copies`), dumped with `fxc /dumpbin`.
