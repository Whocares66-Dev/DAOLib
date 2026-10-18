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
- The engine's own values (0x0045DF70, updated at the frame's start by 0x0045EEE0): back-buffer width and height, `effect_time` (+dt, reset at stop) and the focal plane. `ApplicationTime` (usage 0x29) and `ColorCurveTexture` (0x32) come from the table at 0x00509870; `g_fApplicationTime`'s source wasn't traced (inferred: the application's clock in seconds).
