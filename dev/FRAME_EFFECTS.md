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
- Whether the engine re-applies DefaultState under each pass's state, or leaves the last value standing, is [not traced].

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
| Level and room meshes: Static1, _fast, _relief, Static2, Static3, BakedLowLOD, Vista3 | `mml_fOpacityMultiplier` | DAMshOpqe*: no blend | the opacity multiplier, normally 1 [value inferred] |
| Characters: Ch1/Ch2 (non-`alph`), Face0/2, Weapon1/2, Prop1/2, Cloth1 | `mml_fOpacityMultiplier` (Cloth1/Prop2: × texture alpha) | Character, Face, Weapon, Prop: no blend | the opacity multiplier, normally 1 [value inferred]; Cloth1 and Prop2 carry their texture alpha |
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
| PunchThroughMask.psh | `saturate(tex.r + c0.x)` | ONE/ZERO, CWM 8 (alpha only) | writes alpha outright [checked]. Which target it draws to is [not traced] |

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
- **A peer port that keeps alpha at 1 everywhere** matches vanilla on solids, sky and additive effects. It differs only on cut-outs and hair (vanilla weaker where `a*k < 1`) and slightly under blended effects. It must keep the blur's `centre.a >= mean.a` selection to match vanilla's bloom shape.
