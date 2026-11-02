# Frame effects: the post-process an area runs

How the engine runs an area's frame effects (`da_bloom`, `da_fade`, `da_hsvmatrix`): where they and `areadata.gda`'s override come from, their order, each pass's shaders, states and maths, and what the exe gives them. Read 2026-10-18 in the GOG exe and vanilla's files (shaders through `fxc /dumpbin`) by lib-agent-2 (with subagents), for DAOParagonEngine's side-by-side of the Harrowing's Fade. Not seen in play. The key claims were checked again in the listings, the files or the disassembly. The sky drawn before them is `dev/SKY.md`'s; DAOElgaraShaders reads the same files.

## The Harrowing's Fade, and the effects as vanilla runs them

**What the Fade runs.** `bhm600ar_fade_harrowing.are` (layout `oth000d`, AREA_ID 62) lists in its `.arl`:
- `da_bloom`: Brightness 0.5, Contrast 0.5, GlowIntensity 0.25, HBlurWidth 3, VBlurWidth 3;
- `da_hsvmatrix`: the identity;
- `da_fade`: BlurStrength 5.

`areadata.gda` row 62 overrides the matrix's values (`dev/TODO.md`, 0x0067AFD0): ColorCurves `cc_bhm600`, HSVBrightness 1, HSVContrast 1.02, HSVHue 4, HSVSaturation 0.82. **The sepia is that curve and matrix, not `da_fade`.** `cc_bhm600.dds` (256×1) is a warm lift, red most, every channel reaching white at about 0.81 of its input.

**The files.** An effect is `<name>.ffx.xml` in `postprocesseffects.erf`, its materials `.mat.xml` in `materials.erf`, its states `.sta.xml` in `states.erf`, its shaders in `shaders.erf` (some overridden by `patch002.erf`).

**The order:** by the effect's `layer`, ascending, all at the frame's end: `da_copybackbuffer` (1), `da_bloom` (10), `da_fade` (10), `da_hsvmatrix` (20). The sort is MSVC's `std::sort`, but the tie does not keep the order: it comes out reversed, `da_fade` before `da_bloom` (corrected 2026-10-23 by lib-agent-2; it was inferred to keep the `.arl` order), see "How the list is built" below. Each effect ends by copying the back buffer to `BackBufferCopy` (BBC). LightMask and Downsampled16 are half size, A8R8G8B8.
- **How the list is built** (read 2026-10-23 by paragon-agent for DAOParagonEngine #206, checked by lib-agent-2) [C]:
    - **Adding:** an effect is added by the manager's add 0x00457400(effect, sort). It pushes the pointer onto a plain vector (0x0049D170) and re-sorts the whole vector (0x00457250 → 0x00457060, MSVC 8's `std::sort`) only when `sort` is set.
    - **A file's effects:** the loader 0x004582B0 reads a GFF `V4.0` file. A single effect (field 0xBCF) is added with a sort. A list (field 0xBD2) is added in the file's order with no sort (0x00458400), then sorted once (0x0045840E).
    - **Draw time:** 0x00456C10 walks the vector forward, skipping an effect whose byte +0x88 is 0, unless the global 0x00C1E8B5 is up (corrected 2026-11-01, render-07 and 0x00456C10 read: +0x88 is the effect's `availability` flag, not a "disabled" flag; see "The .ffx.xml grammar" below). There is no second ordering.
  - **The sort's tie rule** [C]:
    - At 32 elements or fewer, `std::sort` is the insertion sort 0x00456900, on the dword at effect +0x80 (the layer [C: the parser stores the `layer` attribute at effect +0x80, 0x0046A0C0..0x0046A0E3, checked 2026-11-01; render-07]).
    - **Ties come out reversed.** A new element not greater than the first goes to the front (`cmp; ja` at 0x0045692A). Otherwise it is moved back past every element it is not greater than (0x00456950–0x0045695C). So it lands in front of its equals.
  - **So in the Fade** [C for the mechanism, I for the list it runs on]:
    - The `.arl` lists `da_bloom`, `da_hsvmatrix`, `da_fade`. One sort puts `da_fade` in front of `da_bloom`, whether or not `da_copybackbuffer` is already in the vector.
    - Each further sorted add to the same vector would reverse the pair again [I]. (answered in part 2026-11-01, render-01; DAOParagonEngine #351) The area's effects are in neither manager 0x00C1EAB0 nor 0x00C1EAC4 but in a third list, `area+0x5F4`, so the sort above acts on that list's own vector; see "Run order of the three lists" below. What the two managers hold (da_init's effects are the likely ones [I]) is still unread.
  - **Seen in play** (DAOParagonEngine #206; Eluvian's frame_capture of the Fade) [S]: `da_fade` (RadialBlur, then FadeDistortion) is drawn before `da_bloom`.

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
- An effect runs (`FrameBufferEffect` slot 1, 0x004600B0) when enabled and its `minlod` (+0x84) ≤ the detail setting [0x00BF22C8] [C: `cmp eax, [0xbf22c8]; jg` at 0x004600B9..0x004600C5, signed, checked 2026-11-01 (render-02/04, read 2026-11-01; DAOParagonEngine #351); was inferred]. `da_fade`'s minlod is 1, so it is off at the lowest.
- The `.arl`'s floats are clamped to the file's `range` (0x0045DDE0; BlurStrength −20..20) and copied in by 0x0045F490.
- The engine's own values (0x0045DF70, updated at the frame's start by 0x0045EEE0): back-buffer width and height, `effect_time` (+dt, reset at start and at stop, never clamped (settled 2026-11-01, render-03, two readings reconciled); [C] 0x0045EEE0) and the focal plane, which is the scene camera's far plane (answered 2026-10-20: "da_dof" below). `ApplicationTime` (usage 0x29) and `ColorCurveTexture` (0x32) come from the table at 0x00509870; `ApplicationTime`'s source is read 2026-10-23 by lib-agent (below).
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

- **`scene_focalplane`** (runtime kind 3) is the global 0xBF2200, copied each frame by 0x0045EEE0 (0x0045EFB2). 0x00455D50 sets it each frame to the scene camera's far plane, its vtable slot 16 (0x00465760, `[cam+0x134]`; 4000 in play, `src/dao/Camera.cpp`). The scene shaders' `g_fFocalPlane` is the same global, so a far plane a plugin changes moves the packing and the DOF together. [C] 0x00455D50 stores it with `fstp [0xBF2200]` at 0x00455D71 after the camera's slot 16 (+0x40) call, every frame; 1000.0 is only the file's initial value (settled 2026-11-01, render-03, two readings reconciled).
- **The scene's second target (oC1) packs the view distance** (vanilla `static1_fast.psh`'s tail): x = saturate(distance / g_fFocalPlane); f = frac(x · (16777246, 65536, 256, 1)); oC1.rgb = f.yzw − f.xyz / 256; oC1.a = 1. `static_depth.psh` (the depth-only pass with antialiasing) writes the same to oC0. The DOF decodes it as `(r / 65536 + g / 256 + b) × focalplane`, point sampled, clamped.

### Conversations and cutscenes [C unless marked]

- **`conversation_dof_values`** (table 203; `conversation_dof_parameters`, 204, names the columns to apply) is read only by GameModeDialog's constructor (0x0068FA70). It takes the first row whose FocusDistance is negative, ID 8 (FocalRadius 8, WeightBlur 0.5, ArtifactBlur 1.3, NearFade 1, FarFade 20, MaxBlur 1), and sets the dialog's own da_dof instance from it once. Rows 1 to 6 are used by nothing found.
- **The per-frame focus:** the dialog's update sets the speaker's distance from the camera under the key "focus" (0xB0E25C). da_dof has no such parameter (an exact map lookup), so FocusDistance stays at the ffx's 5 [I].
- **When the dialog's instance draws:** each line's start (0x0068E250) takes it out of the effect manager, and puts it back (0x0068E6EB) only for a line with no cut. The dialog's end deletes it. Nothing fades.
- **A cut's own `da_dof`** is its AFBE action (type 19, ActionFrameBufferEffect, vtable 0xB23290): its own instance, added to the manager on its first live frame, every parameter set each frame from its curve or base value with no easing, enabled while its actor is the cutscene's active camera actor [I for that reading of +0x1D8], and removed at once by the action's slot 2 at the cutscene's reset or end. (corrected 2026-10-31 by paragon-agent's readers, two readings reconciled, and checked in the exe; DAOParagonEngine #351): **it is removed at its stop**, which this note had as [I] 'does not remove it'. The update (0x007D4500) computes `dur = (float)(stop - start)` (0x007D47B3-0x007D47B9) and, when `dur <= tLocal` (`fcompp; test ah, 0x41; jp` at 0x007D47C1-0x007D47C8: the jump skips the removal only when dur > tLocal), removes the instance from the manager (`0x004E9640(0x00C1EAC4, ...)` at 0x007D47DB) and clears its added flag (+0x30); the next live update would add it again [C]. The player's step always runs an action at its stored stop on the frame whose interval holds it (0x007D8B00 gathers every start and stop with prev <= x <= t; 0x007DA0F0 appends the frame's own t only past the last + 1e-4), with a tLocal bit-identical to the update's `(float)(stop - start)`, so the removal always fires [reported]. An effect is therefore drawn on the frames with **start - 1e-4 <= t < stop** (more exactly `(float)(t - start) < (float)(stop - start)`), and on none with t >= stop; an action whose start and stop fall in one frame's interval is added and removed in that frame and never drawn. A port that draws while `t <= stop` draws one frame too many whenever the cut's quantised clock lands on the stop. Its parameters on a drawn frame are sampled at that frame's t. (The same holds for a link's attach: it is seen only while t < stop.) The more the readers found (read 2026-10-31 by paragon-agent's readers, two readings reconciled; DAOParagonEngine #351): the gate has a second term, **enabled = actor +0x24 != 0 or player +0x1D8 == actor** (`cmp byte [eax+0x24], 0` / `cmp [ecx+0x1D8], eax` at 0x007D4536-0x007D4548, read) [C], where actor +0x24 is field 5223 `CutsceneActorMaster` (the actor constructor, `push 0x1467` at 0x007DFFA4 [reported]), so an effect on the cut's master actor applies whatever the camera, one on a camera actor only while that camera is active (this confirms the [I] reading of +0x1D8 above), and one on any other actor sits in the manager disabled. There is no 'latest started wins': every live action has its own instance, each enabled by its own actor, and the manager draws all the enabled ones. Float parameters (12-byte entries `{param*, base, curve index}`) are set each live update to the base or to the curve at `start + tLocal` (`SetFloat` 0x0045ED90); a vec4 parameter's component written is the one numbered by the **curve index byte**, not the loop's k (`movsx edi, byte [esp+ebp+0x38]` at 0x007D4703, read; `FSTP [esp+edi*4+0x28]` at 0x007D471D [reported]), correct only when a file's curve index equals its component; the parameter named `Time` gets the action's tLocal. Reset (0x007D4800) removes the instance from the manager and clears +0x30; the destructor (0x007D96D0) removes and deletes it.

### Not read

- The oC1 target's format and any antialiasing resolve; what interpolant the shaders divide as the view distance.
- Whether a resize parses the ffx's baked widths again; the case-insensitivity of the names. Size's `+` and `−` are ignored, see "The resources and instructions" below (settled 2026-11-01, render-03, two readings reconciled).
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
- **Read by `dao::core::sta` (`src/dao/core/MaterialState.h`, #295; 2026-11-01).** A state file is `<name>.xml` in states.erf; `sta::Read` gives its `DeviceState` and `SamplerState` entries (a sampler of a stage, or of a texture parameter, found at shader bind), `sta::Effective(defaults, pass)` the engine's rule: DefaultState.sta's values with the pass's named entries over them, every other value back at the default (0x004FBBD0 LoadDefaults, 0x004FBFF0 ApplyState step 7, so nothing is inherited from the previous pass), and `sta::FileNameFor` which file a name loads (more than 32 characters with `.xml`, or a character outside `0-9 a-z A-Z _ - ( ) .`: DefaultState.sta.xml, 0x004FC720). SAMP_MAXANISOTROPY is refused (0x004F9EB8), so DefaultState's anisotropy line for stage 7 never takes effect. The depth pre-pass states are DepthPass.sta, ColorDepthPass.sta and ColorDepthPassTwoSided.sta; `Renderer::SetDepthPassColorWrite` 0x0045C200 switches their colour write mask between 0xF and 0.
- No scene state masks alpha off. Only these set COLORWRITEMASK to something other than 15:
  - 0: occlusion queries, contribution tests, HairAlphaPass1 (depth only).
  - 3: PSSMStatic (shadow map).
  - 8: PunchThroughMask (alpha only).
- **The mask is sticky** (read 2026-10-19 by lib-agent-2) [checked]: the state manager applies only the device states a state file lists, and 152 of the 178 states in `states.erf` list no `DS_COLORWRITEMASK`, all the DAMsh* scene states among them. `DefaultState` is the manager's initial state (0x4FCB1B) and the one a pass with no valid state gets (0x4FC7AB, 0x4FC7D8); it is not applied again under each pass. Every state that sets a mask other than 15 is a depth, shadow or query state, and one that sets 15 follows them in practice (DepthPass, SPT_*, PSSM). That nothing leaves 0 or 3 standing into the scene is [inferred], with DAOElgaraShaders' read-back of the picture's alpha (its NOTES.md:218). **Corrected by the state manager's reading (render-14, #295):** ApplyState restores the default of every value the previous state set and this one does not name (0x004FC1B6-0x004FC1EF), so a mask is not sticky from state to state; the conclusion that nothing leaves 0 or 3 standing holds for that reason.
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

Passes, in the stage `EndFrame`, effect layer 15 (after da_fade/da_bloom at 10, before da_hsvmatrix at 20; at medium or lower detail the SM2 material differs, see below):

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
- **0x0064C420(mode)** is the state setter (ret 4): mode 0 = fade in: if state is not already 0, `SetEnabled(blur, true)` (0x0045FA10) and `FadeDirection = 0`; mode 1 = fade out: `FadeOutStart = current Time` and `FadeDirection = 1`, enabled stays; mode 2 = `SetEnabled(blur, false)`. The state is stored last. Read again (gui-17, read 2026-11-01; DAOParagonEngine #351; two readings reconciled; [C] marked checked in the exe): this is `ScreenFade::SetBlurState` (`this` = `[app+0x154]`, the struct `ScreenFade::ScreenFade` 0x0064C3F0 makes); with no effect in +0 only the state is stored [C 0x0064C425]; a parameter of kind 3 is set through `Parameter::SetFloat` 0x0045ED90, of kind 5 directly at +0x30. Callers: `DragonAgeGUISystem::SetShowBookBack` (0x0060B1EF, mode `on ? 0 : 1`) and `GUIBookback::Close` (0x007170A5, mode 2). Paragon's blur fade follows this three-state machine.
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
- `effect_time`'s behaviour past its declared range (0 6): it does not clamp. 0x0045EEE0 adds dt to the value each frame and never applies the range, so `Time - Start` keeps growing and the fade-out does run past 6 s (settled 2026-11-01, render-03, two readings reconciled); [C] 0x0045EF94-0x0045EF9F (`fadd [esi+0x30]`, `fstp`), no compare with +0x34/+0x38 anywhere in the function.
- What the flag object at GUISystem+0x268 and the member at BookBack+0xC0 are (slot 0x54 targets).
- The BookBack outro's length in seconds (frame labels and rate), so how long the blur lasts after a screen closes.
- Who else enables `da_blurdesaturate` by script: the FB_SetEffectEnabled command can (the toolset scripts do not mention it), not searched in module scripts.
- The sampler state of s2 (the original) in pass 3 (GaussianBlur/HSVMatrixBlend states set stages 0 and 1 only), so whether it is point or linear (same result at 1:1).
- Whether the world's own rendering (shadows, particles) changes while a full-screen screen is up; the game is paused by `GameModeGUI` (GUI.md), so the picture is static apart from effects that run on real time.
- Scratch: `%TEMP%\lib-agent-2-guiblur-x\` (twopassblur, horizontal/verticalblur, hslmatrix) and `-x2\` (hslmatrixfade, states), extracted with ghilana (`all_copies`), dumped with `fxc /dumpbin`.

## The vignette (read 2026-10-23 by paragon-agent for DAOParagonEngine #25, checked by lib-agent-2)

GOG 1.05 addresses. [C] read in the code or the files, [I] inferred, [S] seen in play.

- **What it is:** the frame's two top corners are darkened by a vignette, drawn after the frame effects and before the interface. It is a full-height strip at each side, made by the renderer and gated by the GUI system's visible flag [C].
- **Made at the renderer's start** [C]:
  - StartRenderer 0x00529C70 (ID 7142, slot 1 of IGraphicsInternal's vftable 0x00AF6D2C) looks up the material object `VignetteGUI` (0x00AF6CB8, through 0x004F79E0) and keeps it at `+0x10C`.
  - It builds the MixGUI quad (0x00529530) and the vignette's mesh (0x005297B0), at 0x00529FF6 and 0x00529FFD.
- **The material** [C, vanilla files, one copy each in the core .erf archives]:
  - `vignettegui.mao` names `MixGUI.mat`, the semantic `LinearTextureInterpolation` and `da_vignette.dds` as `InputTexture` (sampler `smpGUI`).
  - That semantic is `PassThrough.vsh` with `MixGUI.psh`, under `MixGUILin.sta`.
  - The state blends SRCALPHA/INVSRCALPHA, filters linearly and clamps U, V and W. It has no z test or z write, and no alpha test or stencil.
  - `MixGUI.psh` is `texld r0, t0, s0; mov oC0, r0`: the texel, with nothing multiplied in. `PassThrough.vsh` passes the position and TEXCOORD0 through.
- **The mesh, 0x005297B0** [C]:
  - It has 8 vertices of stride 0x18 (x, y, z, w, u, v) in clip space, and 12 indices (two quads), kept at `+0x110`.
  - **The left quad:** x −1 to −0.5, with u 0 to 1.
  - **The right quad:** x 0.5 to 1, with u 1 to 0, so the texture is mirrored.
  - **Both:** y 1 to −1, the full height, with v 0 at the top. 0.5 is the float at 0x00AE7104.
- **The texture** [C, decoded]:
  - `da_vignette.dds` is 256×512 DXT5. Its RGB is black; the shape is all in alpha.
  - The alpha is 201/255 at texel (0,0), peaking at 213 at (0,1), so about 0.21 of the frame is left at the corner.
  - It falls to 0 by about x = 225 along the top and y = 352 down the left edge. The lower part and the right edge are 0.
  - With u 0 at the screen's edge, only the two top corners darken, over the outer quarter of the width.
- **Drawn at the frame's end** [C]:
  - EndFrame 0x00528BB0 (ID 7124, slot 22) calls 0x005277E0 when `[this+0x114]` is set and the GUI system's byte `+0x7C` is set (0x00528D92: `mov eax, [0x00C31604]; cmp byte [eax+0x7C], 0`).
  - 0x00C31604 is the GUI system's singleton, the primary object (vftable 0x00AF6744). It is stored by the constructor 0x00524240 at 0x005243B2.
- **The flag `+0x7C`** [C]:
  - **Set to 1** by the constructors 0x00524240 (0x005242D9) and 0x00524710 (ID 6975, 0x005247A1).
  - **Its only other writer** is `GUISystem::SetGuiVisible` 0x00523900 (ID 6941). That is slot 11 (+0x2C) of GUISystem's vftable 0x00AF6744 and DragonAgeGUISystem's 0x00B03504.
  - **What SetGuiVisible does:** it stores its argument at `+0x7C`. With a Scaleform manager at `+0x120`, it tail-jumps to 0x005118C0. That stores the argument at `[mgr+0x15]` and sets each movie record's byte `+0x28` (0x00518CB0, over the list `mgr+0x70`). So it shows or hides every movie, and the vignette with them.
  - **Its callers:** none was found [I]. There is no E8/E9 call and no `call [r+0x2C]` in .text. None of the 146 calls through a +0x2C entry loads the singleton or the app's `+0x158` GUI system. Calls through the getter 0x0053CBF0 were not enumerated.
  - Nothing maps it to a script command or a message handler. `ToggleMainInterfaceMessage` hides the HUD through the scene's HideScreen (GUI.md:114) and leaves the flag alone.
  - So, as far as was read, the flag stays 1 and the vignette is drawn on every frame [I].
- **For a plugin** [I]: clearing `[0x00C31604]+0x7C` drops the vignette alone. Calling SetGuiVisible(false) would hide every movie too.
- **Seen at Redcliffe by day** (Eluvian frame_capture, no photo cam) [S]:
  - It is drawn right after the grade's HSLMatrix draw (cc_arl100) and before the first Scaleform draw, as a MixGUI.psh draw of `da_vignette.dds`: 4 triangles, 8 vertices, SRCALPHA/INVSRCALPHA, sampled through `smpGUI`.
  - Hiding the HUD (ToggleMainInterfaceMessage) leaves it drawn.
- **Open** [I]: whether it shows over the start menu, movies and cutscenes.

## Run order of the three lists, the shadow maps, the default colour curve (render-01, read 2026-11-01; DAOParagonEngine #351)

Tags: [C] checked in the disassembly by lib-agent-r01 (2026-11-01), [C by the reading] what the reading says it checked, [I] inferred; "two readings reconciled" is paragon-agent's A and B readers agreeing.

### Run order of the three lists

- **The trigger** `Renderer::FrameBufferEffects::TriggerEvent` (0x00456E70, the body of SOFBEffectEventTrigger 0x0046F440) runs the event (0x00456C10) in the manager 0x00C1EAB0, then in the object in the global 0x00C1E8B0 if it is not null, then in the manager 0x00C1EAC4 [C: `mov ecx, 0xC1EAB0` / `call 0x456C10` at 0x00456EAB, `mov ecx, [0xC1E8B0]` / `test` at 0x00456EB5, `mov ecx, 0xC1EAC4` at 0x00456EC6]. (render-01, read 2026-11-01; DAOParagonEngine #351)
- **The global 0x00C1E8B0 is the area's own list.** 0x0067B1B0 sets it to `area+0x5F4` when an area is made current and to null otherwise [C: `mov eax, [ecx+0x5F4]` / `mov [0xC1E8B0], eax` at 0x0067B1B7, the null store at 0x0067B1E5]. It is the object `Framebuffer.cpp` asks for an effect by name (FB_SetEffectResource, FB_SetEffectEnabled), so the plugin API already works on the area's list and assumes neither manager (checked: src/dao/Framebuffer.h and .cpp name only 0x00C1E8B0, no 0x00C1EAB0, 0x00C1EAC4 or 0x5F4). (render-01, read 2026-11-01; DAOParagonEngine #351)
- 0x00456C10 calls each effect's slot 1 with the event when the list is enabled (+0x10) and the effect is active (byte +0x88) or 0x00C1E8B5 is up [C by the reading]. That the .arl's da_bloom, da_fade and da_hsvmatrix are what `area+0x5F4` holds is [I]. The event 2 branch before the three runs rebinds the device surface (0x00459660, 0x00465600) [C by the reading]; that event 2 is EndFrame is [I].
- **Still open:** what the managers 0x00C1EAB0 and 0x00C1EAC4 hold. The cutscene's da_dof and the GUI blur are added to 0x00C1EAC4 (above), so the area's effects run before them; da_init's effects are probably in one of the two [I]. Startup (0x00458B10) loads `da_init` when no effect file is named and adds it through 0x00457AA0 and 0x004578D0, which manager gets it was not read [C by the reading for the name].

### The sun's shadow maps (Renderer::ShadowMaps; render-01, read 2026-11-01; DAOParagonEngine #351)

Two partitions, near (index 0, 0x00BF2098) and far (1, 0x00BF209C), each a render target of a square size, drawn by the render script's scene ops SOSMStartPartition, SOSMStopPartition, SOSMBlurPartition and SOSMBindBackbuffer. Flags at 0x00C1E7E4: bit 0 created, bit 1 enabled, bit 2 frame set up, bit 3 a partition being drawn [C by the reading].

- **Create** (0x00455580), from StartRenderer (0x00529DD1) and from SetDetailLevel, once the frame-buffer effects have started (0x00C1E8B6) [C by the reading]. The size by the detail `[0x00BF20E0]` (initially 2): 0 makes no maps, 1 and 2 give 0x200, 3 gives 0x400 (the table at 0x004556CC). The partitions are `{10.0, size, format}` and `{30.0 (sqrt of 900), size, format}`, so **the first Create always uses reaches of 10 and 30 whatever the level**. Both maps are put on the list of targets cleared each frame (0x00457B30 into 0x00C1EAD8), with clear colour (1, 1, 1, 1). Two readings reconciled.
- **SetDetailLevel** (0x004556E0), from SetGraphicsDetailLevel (0x00458FF7): with no maps it only stores the level; 0 or 3 destroys (0x00453AB0) and recreates; 1 or 2 only when the old level was neither 1 nor 2. Then, when the descriptions exist, the near reach is {0, 5, 7, 10}[level] and the far {0, 15, 20, 30}[level], and 0x00BF20B4 is the far reach squared. A level above 3 reads past the tables. Detail 1's 5 and 15 therefore apply only after a change to 1 while running. Two readings reconciled.
- **Update** (0x00453900), each frame from IGraphicsInternal::EndFrame (0x00528D36): with the maps made and enabled it advances a frame counter (0x00C1E808, wrapping at 50000), sets the camera, takes the sun, sets the light direction, updates the splits and resets the matrices, and resets each partition's two blur widths to 0.35. Two readings reconciled.
- **The blur** (BlurPartition 0x004518E0, the body of SOSMBlurPartition 0x0046F5C0): it runs only when the graphics detail `[0x00BF22C8]` is 2 or more, the index is 0 and the shadows are on (flags & 3). **Only the near map is blurred, and not at detail 0 or 1**, whatever the render script passes. CreateBlurFilters (0x00454510) builds, per partition, a "Filtering stage" of two GaussianBlur filters, horizontal (HBlurShadow) into a temporary target and vertical (VBlurShadow) back, fBlurWidth 0.35. Two readings reconciled; the gate by the reading [C by the reading].
- **The sun's fallback.** The sun comes from the scene's light manager (0x00537F00(scene, 1) is the `LightManagerDefault`, scene +0xDC; its three floats at +0xC0, the shadow direction, copied by 0x00462290 with w = 1) (corrected 2026-11-01, render-05, two readings reconciled; checked 0x00537F00..0x00537F30 returns scene +0xDC for 1 [C], and slot 1 (0x00462360) writes +0xC0 each frame from the layout [C]; the earlier reading took the result for a light and +0xC0 for its field; EFFECTS.md, "The light manager"); with no manager the vector is **(0, 0, -1, 0)** [C: fldz, `fst` x and y, `fld [0xAE2E00]` (-1.0), `fstp` z, `fst` w at 0x004539A7-0x004539BB, so w is the 0 left on the stack]. The reading's second reader had w = -1 and lost to this. The manager's +0xC0 being the direction of the sun's shadows is settled by 0x00462360 and 0x00462290 (EFFECTS.md, "The light manager"; render-05, read 2026-11-01; DAOParagonEngine #351): for a terrain level the .arl's sun direction, for a room level the direction from the scene's shadow light.
- A SpeedTree light-direction branch (the float 0x00C1E7E0 > 0) is dead as shipped: nothing Ghidra sees writes 0x00C1E7E0 or the floats it reads [I].

### The default colour curve (render-01, read 2026-11-01; DAOParagonEngine #351)

- 0x004560D0 returns the area's curve (global 0x00C1EB18) if it is not null, else the default at 0x00C1EB20 [C: `cmp dword [0xC1EB18], 0` / `mov eax, 0xC1EB20` / `je` / `mov eax, 0xC1EB18`].
- The default is **`da_defaultcc`**, loaded by Startup (0x00458B10, at 0x00458CDA-0x00458D13); a 256x1 texture in core textures.erf, 1152 bytes [C by the reading, by ghilana]. Whether it is exactly the identity was not read [I]; a look at its 256 texels settles it.
- SetAreaColorCurve (0x00458990) keeps the area's texture in 0x00C1EB18; 0x0067B1B0 calls it with `area+0x6B8` when `area+0x6CC` is not empty, and ClearAreaColorCurve (0x00458A10) otherwise [C: 0x0067B1C2-0x0067B1DA]. That `area+0x6B8` is the areadata's ColorCurves, the `ColorCurveTexture` above, is [I]. Shutdown releases both curves. Who consumes 0x004560D0 was not followed.

## The resources and instructions (render-03, read 2026-11-01; DAOParagonEngine #351)

Tags: [C] checked in the disassembly by lib-agent-r03 (2026-11-01), [C by the reading] what the reading says it checked, [I] inferred; "two readings reconciled" is paragon-agent's A and B readers agreeing. The reading covers 0x0045D890 to 0x0045F310, the middle of the frame-buffer effect system, from the effect files' side of what this note describes above.

- **effect_time** (`FBEclipseResource::OnEvent`, 0x0045EEE0, slot 2) [C]: event 2 (each frame) adds the frame's dt (`g_graphics+0x3C`) to the value at +0x30 for kind 2; events 0 (start) and 4 (stop) set it to 0 (`fldz` at 0x0045EFD2 and 0x0045F069); the range at +0x34/+0x38 is never read, so it is seconds since the effect was switched on, unclamped. Kind 3 (`scene_focalplane`) copies the global 0x00BF2200 each frame (0x0045EFB2); kinds 0 and 1 re-read the back buffer's size on events 2 and 0. Nothing is done while the byte at +0x3C is set [C]. Events 0, 2 and 4 as start, frame and stop are [I]. (settled 2026-11-01, render-03, two readings reconciled)
- **scene_focalplane** [C]: the global 0x00BF2200, rewritten every frame by 0x00455D50 (0x00455D71) from a graphics sub-object's slot 16; the same function copies `g_graphics+0x3C` to 0x00C1E8B8, the effects' dt. 1000.0 is the file's initial value only. That the sub-object is the scene camera is the note's earlier reading, not re-checked.
- **A size** (`IFBGenericSurfaceResource::ParseSize`, 0x0045DA20) [C by the reading]: a text shorter than 11 characters is a plain number (`_wtoi`); otherwise the first token (separators " 	/+*-") is `backbuffer_width` or `backbuffer_height` (anything else gives base 0) with one `*n` or `/n` after it (`/n` truncates toward zero; `/0` gives the base). `+` and `-` are searched for and the results thrown away, so they are ignored. A text of exactly "BACKBUFFER" (10 characters) would be read as the number 0. The scratch buffer is static (127 characters), not thread-safe. A target with no width or height is back-buffer sized, format A8R8G8B8 (0x0045D9D0).
- **Formats** (0x0045DBF0, names compared without case) [C by the reading]: a16b16g16r16 0xB, a16b16g16r16f 0x21, a32b32g32r32f 0x24, g16r16 9, g16r16f 0x20, g32r32f 0x23, r32f 0x22, r8g8b8 2, l8 0xC, anything else 3. A render target's flags: 1 format given, 2 own depth, 4 the back buffer's depth, 0x10 shared, 0x20/0x40 width/height track the back buffer, 0x80 autoclear with a colour; `properties=<name>` takes another record's size and format (0x0045EC30).
- **A float's range** (`FBFloat32Resource::SetAttribute`, 0x0045DDE0) [C by the reading]: `value` is clamped to the range only when the range was parsed before it (min and max are 0 until then, and a range narrower than 1e-6 does not clamp). `da_fade.ffx.xml` has `value` before `range`, so the default (-3.5) is clamped only if the builder passes attributes in the file's order, which is [I]. This is the same fact as "clamped to the file's `range`" in the parameters list above, with its order dependence. `expose` is true, numeric, slider or anything else; `range` is read as "%f %f"; `animatable` is true or not.
- **`<requires effectid=...>`** (the builder, 0x00469AC7) [C by the reading]: lists the effects this one requires, handed to 0x00457AA0. `EnableDependencies` (0x0045F310) switches on every one not yet on, recursively, so enabling an effect enables what it requires. That 0x00457AA0 appends to the vector at +0x74 is [I].
- **Parameter blending.** `FBFloat32Resource::BlendFrom` (0x0045EDC0, slot 4) [C by the reading]: given another float resource and a duration, if the two values differ by more than 1e-6 it starts a linear ramp (`FloatRamp::Begin` 0x0045E0C0, `Advance` 0x0045F080: t = elapsed/duration, no easing) from the other's value to its own; `GetValue` (0x0045EDB0) returns the ramp's current value while it runs. The effects update the ramp with the frame's dt on event 2 (0x0045F4B0). `SetValue` (0x0045ED90) sets the value at once and stops a ramp. The ramp is shared with `EC::ChunkTile::SetDetail` and `CameraControllerExplore::BeginFocusEase`. Who calls BlendFrom, and with what duration, is not read. This is the resource-level BlendFrom; the effect-level one that calls it for each resource is 0x0045F8A0 (render-04, below), and 0x0045F4B0 is the resource's OnEvent, which calls the ramp's Advance 0x0045F080: the two notes do not disagree, they name different functions [C: 0x0045EDC0 ends in a call to 0x0045E0C0, Begin; 0x0045F4B0 calls 0x0045F080; 0x0045F8A0 calls slot +0x10 of each resource].
- **The instructions** of a filter's body [C by the reading]: `bind target|backbuffer|null [at N] [viewport original|recalculate|stored]` is `FBIBindTarget` (type 0xA, 0x0045E190; slot N, render target set through 0x00455DA0, `null` unbinds) or `FBIBindBackBuffer` (type 9, 0x0045E820; the main target through 0x00456000). The viewport word is 0 `original` (the target's own viewport), 1 `recalculate` (redone on every Execute) or 2 `stored` (made when the instruction is built, redone on Refresh). A made viewport is the game viewport's share of the target: x = width * [0x00C1E8C4] / main width, y likewise with [0x00C1E8C8], width and height scaled by [0x00BF2204] and [0x00BF2208] (0 and 1.0 in the files, so the full target, until `FrameBufferEffects::OnResize` 0x00456590 stores them [C, 0x00456680, 0x00456692]: its third and fourth arguments over the main target's width and height, the target's slots +4 and +8 [C by the reading]. It keeps its first two arguments at 0xC1E8C4 and 0xC1E8C8 [C, 0x0045667A, 0x00456686], and `CreateMainRenderTarget` 0x004569D0 calls it with the old pair and the new target's size; 0xC1E8A4 is that main target object). `run semantic X` is `FBIRunSemantic` (type 0xB, Execute 0x0045D890): it draws the full-screen quad with the semantic's material if the effect is enabled (0x00456040), then touches the bound target's texture. `FBIDumpRenderTarget` (type 0xD) writes a target to a file and is a debug instruction.

## The effect's objects, Run, the line parser, the cross-fade, and the renderer's settings (render-02/04, read 2026-11-01; DAOParagonEngine #351)

Tags: [C] checked in the disassembly by lib-agent-r0204 (2026-11-01), [C by the reading] what the readings say they checked, [I] inferred; "two readings reconciled" is paragon-agent's A and B readers agreeing. The readings found nothing in this note wrong. render-04 covers 0x0045F380 to 0x00461990, render-02 the instructions' end and the renderer's start-up (0x00458D60 to 0x0045D6F0).

### The objects (render-04)

- **`FrameBufferEffect`** (0x8C bytes, `new` at 0x00455940, constructor 0x00461400) [C by the reading]: +0 vftable 0xAE7B24 (slot 0 scalar deleting destructor 0x00461840, slot 1 Run 0x004600B0); +4 enabled byte; +8 name; +0x24 vector of stages; +0x34 `std::map<wstring, FrameBufferResource*>` (the effect owns its resources and deletes them, 0x0045FD50); +0x60 vector of the effects that require this one; +0x70 vector of the effects this one requires; +0x80 layer, +0x84 minlod, +0x88 a byte (the manager's "disabled" skip). +0x7C is not a field but the end pointer of the vector at +0x70. The meanings of +0x80 (`layer`) and +0x88 (`availability`, not "disabled") are settled in "The .ffx.xml grammar" below. The destructor (0x004614B0) releases and detaches its requirements but does not tell its own dependents, so one that requires it keeps a dangling pointer [C by the reading; I that it matters, since required effects such as da_copybackbuffer are shared and are probably never destroyed first].
- **`FrameBufferStage`**: +0 vftable 0xAE7138, +4 name, +0x20 the event id it answers, +0x24 "has a filter" byte, +0x28 vector of filters. `FrameBufferStage::Run` (0x0045FCD0) runs each filter when the event equals +0x20 [C by the reading].
- **`FrameBufferFilter`** (constructor 0x00460430): +4 loaded, +5 owns its material, +8 name, +0x24 vector of instructions, +0x34 material instance. A filter with no material is loaded with +0x34 null (the copy-only filters such as `Finish`); one whose material fails to load stays unloaded and `AddFilter` (0x00460400) drops it [C by the reading]. Execute (0x0045FBD0) runs the instructions in order.
- **Resources**: the factory 0x00461990 makes kind 0 render target (0x78 bytes), 1 back buffer (0x30), 2 texture (0x50), 3 Float32 (0x58), 4 Vector4f (0x3C), 5 Eclipse (0x40); above 5 gives null [C by the reading]. Resource vftable slots: 0 ParseAttribute, 1 Create, 2 OnEvent, 3 scalar deleting destructor, 4 BlendFrom. Only a Float32 reacts to BlendFrom and, for event 2, to OnEvent; the others use the no-op 0x00738C90. A texture resource's id is the file name it loads.
- **Requires and required-by.** Enabling an effect enables what it requires, recursively (`UpdateDependencies` bit 1, 0x0045F870 to 0x0045F310). Disabling it disables a requirement only when no other dependent of that requirement is enabled, a scan of the dependents' enabled bytes (0x0045FAE0, 0x0045FB60..0x0045FB91), so da_copybackbuffer stays on while any effect that requires it is on [C by the reading].
- **Event ids** sent to an effect: 0 on enable (and by the two places that pick a live effect up, 0x004717AA and 0x00505DDE), 1 on disable, 2 each frame, 11 the shadows' filtering stage. That 2 is the end of the frame is [I]; the senders are [C by the reading].

### Run (0x004600B0; render-04)

Slot 1, `__thiscall(this, int event)` [C]. Returns at once unless the byte at +4 is set (0x004600B3) and `minlod` (+0x84) is not above the detail setting at 0x00BF22C8 (`cmp eax, [0xbf22c8]; jg`, signed, 0x004600B9..0x004600C5): this settles FRAME_EFFECTS.md's earlier [I], an effect is gated against the detail setting, so da_fade (minlod 1) is off at detail 0 and da_dof (minlod 0) always runs. Then `BroadcastEvent` (0x0045F770) sends the event to every resource, then every stage runs (0x0045FCD0). `SetEnabled` (0x0045FA10) stores the enabled byte first, then calls Run(0) on enabling and Run(1) on disabling; as Run returns at once with +4 clear, **a stage that answers the disable event never runs**, and one that answers the enable event does [C by the reading for the mechanism; the event names are [I]]. The viewport setter (0x00456590) calls `FrameBufferEffect::Refresh` (0x0045F820) on every effect of the managers and the area's list when the game viewport changes, which recomputes each bind's viewport.

### The line parser (render-04)

`FBParseFilterLine` (0x00461570) tokenises a `<filter>` line on space and tab and sends `copy`, `bind`, `run` and `dump` (case-insensitive) to their parsers; any other first word goes to the assignment parser 0x00460590. A failed line puts the loader in its error state [C by the reading]. The quirks that decide what the vanilla files mean [C by the reading, two readings reconciled]:

- **A number must start with a digit** (`iswdigit` at 0x00460653 and 0x004607ED). `x = -3.5`, `x = .5` and `x = +1` are dropped without an error; a negative value reaches a filter only through a float resource (da_fade's `fBlurSize = BlurStrength`, BlurStrength -3.5). Every `=` token is skipped, so `a = b` and `a b` mean the same.
- **The right-hand side is read by what it is**: a render target or back buffer resource binds its texture to the filter's parameter once, at parse time (0x0045D2F0); a Float32 resource copies its current value on each Execute (0x0045F490, the ramp's value while one runs); a Vector4f or an Eclipse resource likewise; `target.width` and `target.height` are constants taken at parse time. A texture resource on the right fails the line.
- **A clear after bind is ignored.** The words after the index are matched only against `viewport`, so da_bssrdf's `bind BSSRDFProfile1 at 0 clear R183G160B143A0` clears nothing through this parser (whether the target's own autoclear does is not read).
- **`viewport <mode>` takes effect only after `at N` or after a back-buffer resource.** In `bind BACKBUFFER viewport recalculate` the first `viewport` leads to a state that waits for a second one, so the mode stays `original`. The modes are `original` 0, `recalculate` 1, `stored` 2; words after the mode are not read.
- **`copy A to B [filter point|linear]`**: the filter is 6 for `point`, 7 for `linear`, 5 for anything else (da_fade's `NONE`). `copy backbuffer to T` makes FBICopyBackbufferToTarget. A missing source token crashes, and so does `dump` with an unknown target.
- **A disable-time stage never runs** (see Run above).

### The cross-fade (render-04)

`FrameBufferEffect::BlendFrom` (0x0045F8A0) takes another effect of the same name and a duration, walks the two resource maps in step (name order) and calls each resource's slot 4 with its counterpart. For a Float32 that is `FBFloat32Resource::BlendFrom` (0x0045EDC0): a ramp from the other's current value to its own, begun by 0x0045E0C0. Its one caller, 0x00456EE0, finds each effect of a list in a manager by name and blends the new one from the one found, so a new instance of an effect, such as an area's da_bloom replacing another's, eases from the old values instead of snapping. **The two BlendFroms are two functions, and neither note is wrong**: 0x0045F8A0 is the effect's, 0x0045EDC0 the Float32 resource's. The ramp is 0x0045E0C0 (Begin: start, end and duration stored, elapsed zeroed) and 0x0045F080 (Advance), driven from the resource's OnEvent 0x0045F4B0 [C: all four addresses in the exe, 2026-11-01]. **Ramps advance only while the effect runs**: OnEvent(2) advances an active ramp by the dt in 0x00C1E8B8 (the global 0x00455D50 copies `g_graphics+0x3C` to), and the only per-frame sender is Run, so a disabled effect, or one under its minlod, freezes its ramps [C by the reading; other senders of event 2 were not seen]. Which transitions pass a duration (area change, cutscene, the fade setter 0x0064C420) is not read.

### The renderer's settings (render-02)

- **The instructions' Execute** [C by the reading]: slot 1 of each instruction. `FBIAssignFloat32` (type 0, 0x0045D4D0) writes a constant into the filter's float; `FBIAssignEclipseParameter` (type 2, 0x0045D5D0) copies an engine value's +0x30, so **the engine's values (back-buffer size, `effect_time`, `scene_focalplane`, from the section above) reach every effect's constants each frame through this copy**; `FBIAssignFloat32ToResource` (type 3) sets a Float32 resource; `FBIAssignVector4fFromResource` (type 4) copies 16 bytes; `FBICopyTargetToTarget` (type 7) is the driver's stretch copy (slot +0xE0); `FBICopyBackbufferToTarget` (type 8) the driver's slot +0xCC, which is how `BackBufferCopy` is refreshed after every effect; `FBIBindTarget` (0xA) and `FBIRunSemantic` (0xB) as above. Shared render targets (flag 0x10) live in a global map at 0x00C1EA80 by name (0x00458E50 registers, 0x00458E80 fetches), so two effects naming the same target share one texture.
- **Antialiasing follows the option, off by default** [C by the reading]: the level is the global 0x00C1EB48, set by 0x00458E90 from the video options' antialiasing attribute (options +0x4D0) when the options are applied or loaded; the first-run defaults write 0 to it. **The depth path follows it** (`SelectDepthPath`, 0x00459230): with the level non-zero, or an option byte, or a capability byte (+0x14 of the driver's caps) set, `DepthPass.sta`, `ColorDepthPass.sta` and `ColorDepthPassTwoSided.sta` get a full colour write mask (0xF), the scene's depth is packed into colour by a separate pass, and `da_depth` is disabled; otherwise the mask is 0, `da_depth` is enabled and the second target (oC1) carries the packed distance. What `da_depth` itself does is not read.
- **The first-run detail** (`AutoDetectVideoSettings`, 0x004590B0) [C: 0x004590E3..0x004590FC, 2026-11-01]: runs when the ini holds no video settings (the byte at options +0x93C is 0). A value the driver manager returns (video memory in megabytes is [I]) is compared with 0x100 and 0x200: below 256, texture quality 0, detail 0 and frame-buffer effects off; exactly 256, 1 and 1, on; above 256, 2 and 2; **above 512, detail 3** (`cmp 0x200, eax; sbb esi, esi; neg esi; add esi, 2`, an unsigned compare). The antialiasing default written beside them is 0.
- Also read: the low-end mode 0x004591C0 forces detail 0 (that the capability it tests is a shader model is [I]); `AddFrameBufferEffect` (0x00459610) re-sorts by layer on each add, `RemoveFrameBufferEffect` (0x00459D90) does not; the main surface and an overlay renderer for debug text are made by 0x0045ADB0 and 0x0045AF20; 0x004597B0 is a renderer's clear colour (0,0,0,1 by default; the portrait renderer sets 0,0,0,0).

## The .ffx.xml grammar (render-07, read 2026-11-01; DAOParagonEngine #351)

Tags: [C] checked by lib-agent-r07 in the disassembly (2026-11-01), [C by the reading] what the reading says it checked, "two readings reconciled" is paragon-agent's A and B readers agreeing. A core reader of this grammar is being built as DAOLib #292. The filter lines' own grammar is render-04's line parser above and is not repeated here.

`FBEffectBuilderXML2` (0x004698E0..0x0046A430) is the expat state machine `FrameBufferEffect::LoadEffectFile` (0x0045F1D0) runs over a `<name>.ffx.xml` (render-07, read 2026-11-01; DAOParagonEngine #351) [C by the reading]. Its object is 0x38 bytes: +4 the expat handle, +8 an error code, +0xC the state, +0x10 the state stack, +0x20 the output vector, +0x28 the effect being built, +0x2C the resource, +0x30 the stage, +0x34 the filter. The states are 0 the document, 1 in `<Effect>`, 3 in `<Resources>`, 4 in a resource element, 5 in `<implementation>`, 6 in `<stage>`, 7 in `<filter>`, 10 an error (every handler then returns at once) and 11 a skipped effect.

```
<Effect id=.. [layer=int] [domain=cinematics] [minlod=int] [availability=int]>
  <Resources>
    <target|float|vector4f|runtime|backbuffer id=.. attr=..*/>
    <requires id=.. effectid=../>
  </Resources>
  <implementation>
    <stage id=.. event=..>
      <filter id=.. material=..>
        one filter line per text chunk, '#' starts a comment
      </filter>
    </stage>
  </implementation>
</Effect>
```

- **The Effect's attributes.** `id` must be the first attribute (else error 4) and makes the `FrameBufferEffect` (0x8C bytes). Then, over the pairs from the third on: `layer` is stored at effect +0x80 [C 0x0046A0C0..0x0046A0E3: `_wcsicmp` against the name at 0x00AE7F84, the value through 0x008E643E, stored to +0x80], which confirms the earlier note that +0x80 is the sort key; `minlod` at +0x84 [C 0x0046A123..0x0046A146; Run gates it against the detail setting, above]; `availability` at the byte +0x88 as `value > 0` [C 0x0046A14C..0x0046A174: `test eax, eax; setg al; mov [ecx+0x88], al`]; `domain` equal to `cinematics` is the skip below. The constructor (0x00461400) clears +0x80, +0x84 and +0x88 with one register [C 0x00461482..0x0046148E], so an Effect with no `layer` has layer 0 and **one with no `availability` has +0x88 = 0** [I: the register's value was not traced, it is stored to all three].
- **0xC1E8B5 is a switch, `g_frameEffectsEnabled`** [C by the reading, 2026-11-02]: `SetFrameEffectsEnabled` 0x00528050, `ApplyChangedOptions` 0x005E1A23, `AutoDetectVideoSettings` and `Startup` write it, and `EndFrame` saves, clears and restores it around a frame.
- **What +0x88 means to the draw walk** [C: 0x00456C10 read 2026-11-01]. The walk (0x00456C14: the list's enabled byte +0x10; 0x00456C60: `bl = [0xC1E8B5]`) calls each effect's slot 1 with the event when `[0xC1E8B5]` is non-zero, and otherwise only when `[ecx+0x88] != 0` (`cmp byte ptr [ecx+0x88], bl; je skip`, bl being 0 on that path, 0x00456C70..0x00456C76). So **+0x88 = 1 means the effect is available and is walked; +0 means it is skipped**, and an effect with `availability="0"` or none is never run by the walk unless the flag at 0x00C1E8B5 is up. The flag is a different byte from the cinematics flag below (0x00C1E8B5 against 0x00C1E8B7), and what raises it was not followed [I]. The earlier wording "skipping a disabled effect" read the byte backwards.
- **Resources.** `<Resources>` only in state 1 with an effect made. The kinds by element: `target` 0, `backbuffer` 1, `float` 3, `vector4f` 4, `runtime` 5; no element makes kind 2 (texture). A resource's `id` must be its first attribute (else error 3) and the other pairs go to the resource's ParseAttribute. `requires` takes `effectid` as its second attribute and queues the other effect's load. An unknown element inside `<Resources>`, a `texture` for one, is error 2 and ends the parse. A duplicate resource id is dropped (`AddResource` inserts only when absent) [C by the reading].
- **Stages.** `<stage id=.. event=..>` takes `event` as its second attribute. The event names, case-insensitive: start 0, stop 1, beginframe 2, endframe 3, staticgeomstart or staticgeom_start 5, staticgeomend or staticgeom_end 6, motiongeometrystart 7, motiongeometryend 8, distortionmaskend 9, userdefined 10, anything else 11 (a stage that answers no event the manager sends [I]). A stage with no filter is error 6 at its end.
- **Filters.** `<filter id=.. material=..>` looks for `material` as the second or third pair only; absent gives an empty material (a copy-only filter). Each text chunk is one line: leading tab, LF and space are skipped, at most 255 characters are kept (the rest is cut silently), `#` starts a comment, an empty line is refused, and `FBParseFilterLine` (render-04's line parser, above) does the rest. A refused line is error 5.
- **Other elements** outside `<Resources>` (`header`, `description`, `author`) are ignored [C by the reading].

The quirks (render-07, read 2026-11-01; DAOParagonEngine #351):

- **Element names match ignoring case**: `_wcsicmp` at 0x008E687B folds A to Z by 0x20 [C by the reading, two readings reconciled]. Attribute names are compared the same way (the calls at 0x0046A0C8, 0x0046A154 and 0x0046A0F1 pass through it) [C].
- **A broken effect silently disappears.** Only `</Effect>` adds an effect to the output, and an error leaves the one in progress out of it; nothing reads the error code (+8) in `LoadEffectFile` [C by the reading; I that no other reader of +8 exists].
- **`domain="cinematics"` stays empty without the command-line flag at 0x00C1E8B7** (set from `EclipseGame::ProcessCommandLineOption`, 0x00404DC9 [C by the reading]): the loop pushes state 11 [C 0x0046A0E9..0x0046A11E], so the resources, stages and filters are all skipped, and `</Effect>` still adds the empty effect. **It also leaves the parser in state 1**: `</Effect>` is matched before the state-11 test (0x00469D36 then 0x00469D41), pops once, which removes 11 and leaves the Effect's state 1 on the stack, and a second `<Effect>` needs state 0 (0x0046A1EF) and fails with error 2, **so every effect after a skipped one in the same file is lost** [C by the reading, two readings reconciled; I: whether a shipped .ffx.xml holds more than one Effect was not checked]. That the flag is off in normal play is [I].
- **A number must start with a digit** in a filter line (`iswdigit`, render-04's line parser, above) [C by the reading, two readings reconciled]. The Effect's own integers go through 0x008E643E (`_wtoi` [I]) and are not tested for digits; `availability` is only `> 0`, so 0, a negative or a non-number leaves the effect unavailable.
- **`id` is first, `effectid` second, `event` second**, and `material` second or third: a file with the attributes in another order fails with error 3 or 4, or loses its material.

The reader of this grammar is `src/dao/core/FrameEffect.{h,cpp}` (`dao::core::ffx::Read`, DAOLib #292, tested in `tests/test_frameeffect.cpp`): the engine's quirks kept, each with the address it was read at, and the 14 shipped files read whole.

## Material states (.sta): the default state and its overrides, and the material managers (render-14, read 2026-11-01; DAOParagonEngine #351)

How a draw's render state is set, and the three managers around it. Two readings reconciled (marks: [C] read in the GOG exe, [C by the reading] what the reading says it checked, [I] inferred). This is the rule the per-file notes above and in CHARACTERS.md, EFFECTS.md and SKY.md rest on: they name what one .sta sets, and this says what the rest of it is. A core reader of the .sta format is being built as DAOLib #295.

### The rule [C]

**Every pass draws under `DefaultState.sta` plus only what its own .sta names, and the defaults come back when the next state does not name them.** `MaterialStateManager` (singleton 0x00C30A08, 0x5F4 bytes, vftable 0x00AF4018) loads `DefaultState.sta` (name at 0x00AF4024) as the defaults in `LoadDefaults` (0x004FBBD0): a table of 51 device values and one of 16 stages x 12 sampler types (index stage*12 + type), all 0x7FFFFFFF (unset) first, then filled from the default state's pairs. A sampler entry that names a texture parameter instead of a stage (below) does not set a default. `ApplyState` (0x004FBFF0, from `SetState` 0x004FC2C0 = `BeginState` 0x004FBB30 then apply, called by `MaterialState::SetMaterialObject` 0x004FA8C0) does this:

1. A state with the same serial (+0x4C) and generation (+0x50) as the current one returns at once. Otherwise the flag buffers of "what the previous state changed" and "what this one changes" are swapped and the new one cleared (51 bytes for devices, 192 for samplers).
2. Every device pair (index, value) of the state is sent to the driver, even one equal to the default, and flagged as changed when it differs from the default.
3. Every sampler entry is sent the same way, by stage, or by the unit the shader gave its texture parameter.
4. Restore: every device index and every sampler slot that the previous state changed, that has a default and that this state did not change, is sent its default again.

So a .sta never needs to repeat a default, and a state that names a value equal to the default after one that changed it still sends it. The state's self handle and the manager's current-state handle are weak (below), so the manager does not keep a state alive.

A debug byte at +0x5F0, set by nothing found, would force SEPARATEALPHABLENDENABLE 1, BLENDOPALPHA MAX, SRCBLENDALPHA ONE and DESTBLENDALPHA ONE after every apply (the alpha of every draw becomes max(src, dst)). The constructor clears it and no absolute reference to 0x00C30FF8 exists [C], so it is off in play; its setter is unknown [I].

### The file's vocabularies [C]

A .sta (`states.erf`, XML) holds `DeviceState` elements, a `DS_` name and a value, and `SamplerState` elements, a stage (or a texture parameter), a `SAMP_` type and a value. `MaterialState::AddDeviceState` (0x004FA430) takes the name through 0x004F93B0 and the value through 0x004F98B0 and drops the pair when either gives -1, so a misspelt name or value is dropped silently. Names compare without case. A value that is no name is parsed as an integer, then, if that gives 0, as a float whose bit pattern is stored (0x004F9DA6-0x004F9DCD). These are the engine's codes; the driver maps them to Direct3D's [I: not read here].

**The 51 device states** (index, name without the `DS_` prefix): 0 ZENABLE, 1 FILLMODE, 2 SHADEMODE, 3 ZWRITEENABLE, 4 ALPHATESTENABLE, 5 SRCBLEND, 6 DESTBLEND, 7 CULLMODE, 8 ZFUNC, 9 ALPHAREF, 10 ALPHAFUNC, 11 ALPHABLENDENABLE, 12 FOGENABLE, 13 FOGCOLOR, 14 FOGTABLEMODE, 15 FOGSTART, 16 FOGEND, 17 FOGDENSITY, 18 RANGEFOGENABLE, 19 STENCILENABLE, 20 STENCILFAIL, 21 STENCILZFAIL, 22 STENCILPASS, 23 STENCILFUNC, 24 STENCILREF, 25 STENCILMASK, 26 STENCILWRITEMASK, 27 CLIPPING, 28 FOGVERTEXMODE, 29 CLIPPLANEENABLE, 30 MULTISAMPLEANTIALIAS, 31 MULTISAMPLEMASK, 32 COLORWRITEMASK, 33 BLENDOP, 34 SCISSORTESTENABLE, 35 SLOPESCALEDEPTHBIAS, 36 TWOSIDEDSTENCILMODE, 37 CCW_STENCILFAIL, 38 CCW_STENCILZFAIL, 39 CCW_STENCILPASS, 40 CCW_STENCILFUNC, 41 COLORWRITEMASK1, 42 COLORWRITEMASK2, 43 COLORWRITEMASK3, 44 BLENDFACTOR, 45 SRGBWRITEENABLE, 46 DEPTHBIAS, 47 SEPARATEALPHABLENDENABLE, 48 SRCBLENDALPHA, 49 DESTBLENDALPHA, 50 BLENDOPALPHA. The name strings are at 0x00AF370C..0x00AF306C. An unknown name gives -1.

**The value codes** (0x004F98B0):

| Codes | Names |
|---|---|
| 0, 1, 2 | `ZBUFFERENABLE_FALSE`, `ZBUFFERENABLE_TRUE`, `ZBUFFER_USEW` |
| 3, 4, 5 | `FILL_POINT`, `FILL_WIREFRAME`, `FILL_SOLID` |
| 6, 7 | `SHADE_FLAT`, `SHADE_GOURAUD` |
| 8 to 18 | `BLEND_ZERO`, `ONE`, `SRCCOLOR`, `INVSRCCOLOR`, `SRCALPHA`, `INVSRCALPHA`, `DESTALPHA`, `INVDESTALPHA`, `DESTCOLOR`, `INVDESTCOLOR`, `SRCALPHASAT` |
| 21, 22 | `BLEND_BLENDFACTOR`, `BLEND_INVBLENDFACTOR` (19 and 20, Direct3D's both-source-alpha pair, have no name) |
| 23, 24, 25 | `CULL_NONE`, `CULL_CW`, `CULL_CCW` |
| 26 to 33 | `CMP_NEVER`, `LESS`, `EQUAL`, `LESSEQUAL`, `GREATER`, `NOTEQUAL`, `GREATEREQUAL`, `ALWAYS` |
| 34 to 37 | `FOG_NONE`, `FOG_EXP`, `FOG_EXP2`, `FOG_LINEAR` |
| 38 to 45 | `STENCILOP_KEEP`, `ZERO`, `REPLACE`, `INCRSAT`, `DECRSAT`, `INVERT`, `INCR`, `DECR` |
| 46 to 50 | `BLENDOP_ADD`, `SUBTRACT`, `REVSUBTRACT`, `MIN`, `MAX` |
| 1, 0 | `TRUE` (shares `_TRUE`'s return, 0x004F98DB), `FALSE` |

**The 12 sampler types** (0x004F9DE0, index stage*12 + type): 0 `SAMP_ADDRESSU`, 1 `ADDRESSV`, 2 `ADDRESSW`, 3 `MAGFILTER`, 4 `MINFILTER`, 5 `MIPFILTER`, 6 `MIPMAPLODBIAS`, 7 `MAXMIPLEVEL`, 8 `MAXANISOTROPY` (refused, below), 9 `SRGBTEXTURE`, 10 `ELEMENTINDEX`, 11 `BORDERCOLOR`.

**The sampler values** (0x004F9F10): `TEXADD_WRAP` 0, `TEXADD_MIRROR` 1, `TEXADD_CLAMP` 2, `TEXADD_BORDER` 3, `TEXADD_MIRRORONCE` 4, `TEXFILT_NONE` 5, `TEXFILT_POINT` 6, `TEXFILT_LINEAR` 7, `TEXFILT_ANISOTROPIC` 8, `TEXFILT_PYRAMIDALQUAD` 9, `TEXFILT_GAUSSIANQUAD` 10. Anything else is an integer, then float bits, as above (0x004FA025-0x004FA051).

### Quirks

- **`SAMP_MAXANISOTROPY` is refused.** The type matcher compares with the string at 0x00AF3D44 and on a match jumps to the `or eax,-1` return (`jne 0x004F9F05` at 0x004F9EB8) [C, disassembled 2026-11-01]. One reading listed it as type 8, and was wrong. No .sta can set anisotropy; the engine sets it elsewhere.
- **A sampler state can name a texture parameter instead of a stage.** `AddSamplerState` (0x004FB8B0; stage, type, value, texture) with a texture name stores byTexture = 1 and stage 0, and the name in a narrow string; the entry (0x44 bytes) has its bound unit at +0x10. The unit is found when the shader is bound, and `ApplyState` skips the entry when it is 0xFFFF (the texture is not in the shader) [C]. Such entries set no default.
- A sampler entry is added only when both its type and its stage are not -1 (0x004FB9CF-0x004FB9E7).
- Resource names are checked by `IsValidResourceName` (0x004F7AA0): every character is `0-9 a-z A-Z _ - ( ) .`, an empty name is valid. It is called by the material, state and shader loaders (0x004F81DD, 0x004FC7C1, 0x004FE2CC, 0x004FE45C), and the shader loader also requires a length of at most 0x20 [C by the reading].

### MaterialState (0x9C bytes, vftable 0x00AF4004) [C]

Constructor 0x004FB6B0 (device, mostDerived). +8 name (wstring), +0x24 byte 1, +0x28 device-state vector of 8-byte (index, value) pairs, +0x38 sampler vector of 0x44-byte entries, +0x48 device, +0x4C serial (the global counter 0x00C309AC, post-incremented; its only writer), +0x50 generation word, +0x54 second-interface handle, +0x5C self handle, +0x64 the virtual base `SynchSafePointerListBase`. `Clone` (0x004FBA20) copies the name and both vectors, gets a new serial and holds its own self handle weak; its caller 0x004F53F0 gives each pass of a material a state of its own [I]. A sampler entry is +0 stage, +4 type, +8 value, +0xC byTexture, +0x10 bound unit, +0x14 a std::set, +0x28 the narrow texture name. Ghidra's structs for this class and the manager sit low (its `offset_0x40` is +0x48, its `offset_0x5d0` is +0x5F0).

### MaterialStateManager's layout (0x5F4 bytes) [C]

+4 vector of state handles; +0x14 default state handle; +0x1C current state handle; +0x24 current generation (-1 at first); +0x28 driver (0x005278C0: the graphics driver manager [0x00C31650] +0xBC on the thread in 0x00C2B590, else +0xB4); +0x2C and +0x5F the two device flag buffers (0x33 each); +0x94 current and +0x98 previous flag pointers; +0x9C deviceDefault[51]; +0x168 samplerDefault[16][12]; +0x468 and +0x528 the two sampler flag buffers (0xC0 each); +0x5E8 current and +0x5EC previous; +0x5F0 the debug flag. Getter 0x004FCB70 (Ghidra labels it `_AfxAygshellState`).

### MaterialManager: the .mat libraries, and the standard list [C unless marked]

`MaterialManager` (vftable 0x00AF305C, +4 a vector of material handles, +0x14 a mutex; constructor 0x004F7FF0) keeps the loaded material libraries (.mat). `LoadStandardMaterials` (0x004F8500, from `Eclipse::IGraphicsInternal::StartRenderer` 0x00529C70 at 0x00529DDD) loads a fixed list when the renderer starts: **40** pointers to wide strings at 0x00BF5598-0x00BF5634, ended by 0x00BF5638 -> 0x00ADEF14 (an empty string) [C, bytes read in the GOG exe 2026-11-01]. The first entry is `Default_BadStream.mat` (0x00BF5598 -> 0x00AF302C), the last `Character.mat` (0x00BF5634 -> 0x00AF2BC0), and `GUIMaterial_MinAlpha.mat` (0x00BF562C -> 0x00AF2BF8) stands between `GUIMaterial_MaxAlpha.mat` and `HairAlpha.mat`. In order: `Default_BadStream`, `Default_Missing`, `Black`, `Canopy`, `Cloth`, `Clouds`, `DABloom`, `DADistortion`, `DADistortionMask`, `dof`, `DistortionMaskDiffuse`, `Eye`, `Face`, `Fade`, `GeneralGui`, `HSLMatrix`, `Lava`, `LightProbeProj`, `MixGUI`, `Prop`, `Sky`, `Static`, `Terrain`, `VFX`, `Vista`, `Water`, `Weapon`, `Weapontrail`, `SPT_Billboards0`, `SPT_Fronds`, `SPT_Branch`, `SPT_LeafCards`, `SPT_LeafMesh`, `ScreenSpace`, `GaussianBlur`, `GUIMaterial`, `GUIMaterial_MaxAlpha`, `GUIMaterial_MinAlpha`, `HairAlpha`, `Character` (each with `.mat`). `GetMaterial` (0x004F8370) finds a name in the vector by the material's name (no case), else loads it (0x004F8150), and if that fails loads the fallback named at 0x00C30974 [I: `Default_Missing.mat`].

- `ReloadShaders` (5890, 0x004F7B20): under the lock it rebuilds the shader manager's two lists (singleton 0x00C31084, getter 0x004FE090) [I: vertex and pixel shaders] and re-resolves every material's techniques (0x005032D0). Its callers are `Renderer::ResetDisplay` 0x004594D0 (at 0x0045952F) and `Renderer::vfunction7` 0x00459460 (at 0x004594B9), each right after `TextureManager::ReloadTextures(true)`.
- `RebindMaterials` (5891, 0x004F7BF0): under the lock it recreates each shader entry's device object (0x004F6AB0: `entry[0x24] = 0x005073C0(entry[0x20], device)`) and then runs 0x005032D0. **Its only caller is the detail-level path**, `SetGraphicsDetailLevel` 0x00458EB0 at 0x00458FD4, not a device reset.
- `PurgeExpired` (0x004F7E40, slot 2) erases the null handles from the vector; `ReleaseAll` (0x004F8060, from the renderer shutdown 0x00527C70 at 0x00527D84) clears the vector and drops the default state.

### MaterialObjectManager: the .mao objects [C unless marked]

Singleton 0x00C308C0 (`GetInstance` 0x004F77C0, 26 callers; Ghidra has no function there and the call sites name it `_AfxAygshellState`), vftable 0x00AF2B90, +4 a map by wide name, +0x34 a mutex. `GetObject` (0x004F78E0): a locked lookup (0x004F7230); if absent, load (0x004F72D0): append `.mao` if the name lacks it, open `<name>.xml` first (`.xml` at 0x00AF2B74 appended at 0x004F742A, opened at 0x004F7470), and only if that fails the plain name (0x004F7485..0x004F74A6) [C] (corrected 2026-11-01: this had the plain name first), parse the XML and build a 0xF0-byte material object (0x00508110); if still absent and the name is not the fallback at 0x00C30918 [I: `Default_Miss01.mao`, the string at 0x00AF2B98], recurse on the fallback. **A missing material object is drawn with `Default_Miss01.mao`, not skipped.** `CreateInstance` (0x004F79E0, 24 callers) returns a new `MaterialObjectInstance` (0xE0 bytes, 0x004F5080) or 0.

### The material object's parameters and the .mao reader (render-16, read 2026-11-01; DAOParagonEngine #351)

- **A .mao value always beats the .mat default.** `ApplyParamDefaults` (0x005028C0) asks the object's `Has*Parameter` tests first (texture 0x004FF330, float 0x004FF3F0, vector4 0x004FF370, vector4 array 0x004FF3B0, matrix 0x004FF430, by the definition parameter's type 0, 1, 2, 3, 5) and fills a parameter from the definition's default only where the object lacks it [C by the reading; two readings reconciled; the callers are the three xrefs of 0x004FF330]. The object keeps one map per kind (textures +0x78, vector4 arrays +0x90, vector4s +0xB0, floats +0xD0, sound types +0xE4, matrices +0xF8), keyed by the interned parameter id (`MaterialParameterNames::GetOrAddId` 0x00506D40), each with a default value beside it.
- **`DefaultSemantic` is looked up by `Material::FindSemantic` (slot 6, 0x00503140), last to first and case-insensitive, and a miss keeps the material's own default.** [C] The loop walks the material's semantic list (+0xC/+0x10) from its last entry down (`sub esi,1` at 0x00503163, the entry at `[list+esi*4]`) and compares names with `EqualsNoCase` (0x00503187..0x0050318E), checked in the exe. The name is kept by the builder (+0x64) and resolved when `MaterialObject` closes (`SetSemantic` 0x004FE940, which stores only a non-null result into +0x74); `SetMaterial` (0x00501540) had already set +0x74 to the material's default semantic (`Material` +0x18) [C by the reading]. So a `.mat` with two semantics of one name gives the later one.
- **The .mao reader's element names are case-insensitive** (`__wcsicmp` and `ECString::EqualsNoCase` in the builder's end-element handler 0x005077F0) [C by the reading]; `dev/PARAGON_MAO_CASE.md` agrees.
- A `Texture` element with the builder's flag +0x28 set goes to the deferred record (0x00500DF0), where `lm.dds` becomes `Default_Black` (`dev/SKY.md`).
- **Core's `MaterialObject` keeps all nine kinds the builder files** (Texture with RequiresID, Vector4f, Vector4fArray, Matrix4x4f, Float, SoundType, Material, Override, DefaultSemantic), each at its closing tag in its own type, and the first `<MaterialObject>` only; the .mat's defaults are the caller's to apply (DAOLib #297, render-19).

### Weak handles [C]

The engine's handle is an 8-byte `General::SafePointer {T* p; u8 weak}`. 0x004FA0E0 makes a handle **weak**: it sets byte +4 and calls 0x004D7AE0, which **decrements the target's strong count (+0x14) under its +0x18 lock at 0x004D7B29** (`add dword ptr [esi+0x14], -1`, read in the GOG exe 2026-11-01) and, at zero, hands the object to its disposal (0x004CAFD0/0x004CB040). 0x00572810 makes it strong again (the byte to 0; the increment in 0x004D7BA0 is [I]). Weak are the object manager's map entries (`Register` 0x004F7830, so the manager does not keep material objects alive), a cloned state's self handle, and the state manager's current state. `GetObject` ends by making the handle it returns strong, so the caller owns the reference. The other operations: 0x00534A70 constructs from a raw pointer, 0x004F4710 assigns a handle, 0x00458420 a raw pointer, 0x00538F30 releases and nulls, 0x004621C0 gets p, 0x004F71A0 copy-constructs.

### Marks

[C] The 5890/5891 callers, the weak decrement at 0x004D7B29, the refusal at 0x004F9EB8 and the table's end at 0x00BF5638 were read in the GOG exe 2026-11-01. [C by the reading] The apply loop's step addresses (0x004FC0D0-0x004FC25C), the sampler-value parse and the object manager's steps. Two readings reconciled on the 40-entry table (one had 39 starting at `Default.mat`; both missed `GUIMaterial_MinAlpha.mat`), the refusal of `SAMP_MAXANISOTROPY`, the weak direction of 0x004FA0E0 and 5891's role.
