# The sky as the engine draws it

How the sky dome's shader constants are filled from an area's sky block and what the sky shaders do with them. The block itself is `dao/core/Sky.h`'s and `dao/Atmosphere.h`'s.

## The sky dome's shader parameters: read 2026-10-18

Read in the GOG exe and the sky shaders' disassembly (`C:\project\DAOElgaraShaders\build\scratch\fog\sky.vsh.txt`, `sky.psh.txt`) by lib-agent-2 (with subagents) for DAOParagonEngine #25. Two readings were done independently, the exe's fill and the shader's use, and they agree on each register. The constants were checked in the exe. No values were captured in play.

### Binding

`sky.mat.xml` declares `mml_vSkyParams`, `mml_vFogParams` and `g_vLightParams` by usage: SkyParameters (0x2C), FogParameters (0x2E) and LightsArray (0x11); CloudParameters is 0x2D. The shader-parameter provider (0x004F8A10) hands each the scene's vectors: sky at scene +0x38, fog at +0x18, clouds at +0x48.

The scene's sky update (0x00538FA0, every frame):
1. It calls 0x0055BFA0 on the area's sky block (`dao/core/Sky.h`). That recomputes only when the block's changed flag (+0x84) is set or t has moved by 0.001. Its Preetham n, N and pn arguments are never read.
2. It copies the results out: sky[0..4] from the block's +0x90, +0xA0, +0xB0, +0xC0, +0x110; sky[5..8] the 4×4 at +0x120; cloud[0..2]; fog[0..1].

t is the scene camera's +0x13C, 1 by default.

### The nine float4s (g = `hg`)

**The intermediates:**
- **βR** = `rayleighMultiplier` × (6.95e-6, 1.18e-5, 2.44e-5) per metre, for red, green and blue (0x00AF91C8, C4, C0).
- **βM** = `mieMultiplier` × 0.67 × 0.434 × c × 4π³ × (1/λ)².
  - c = (6.544 × `turbidity` − 6.51) × 1e-17.
  - 1/λ = 1538461.5, 1754386.0, 2105263.25 per metre: λ = 650, 570, 475 nm.
  - K = 0.67.

| Register | x, y, z | w | The shader's use |
|---|---|---|---|
| c0 | 1 + g², 2g, `earthReflectance` | `distanceMultiplier` | HG's denominator (1+g²) − 2g·cosθ; .z and .w unused |
| c1 | βR + βM | `sunRed` × `sunIntensity` | extinction exp(−(c1·D)²), and the divisor of in-scatter |
| c2 | βM × (1 − g²) / 4π | `sunGreen` × `sunIntensity` | Mie (HG) numerator |
| c3 | βR × 3 / 16π | `sunBlue` × `sunIntensity` | Rayleigh coefficient |
| c4 | `moonAlpha`, block +0x80, (block +0x1C)², 1 − block +0x20 | | only .x: the disc texture's multiplier |
| c5–c8 | the celestial body's 4×4 | | the vertex shader's projective uv for the disc |

The pixel shader reads c0–c4 and the vertex shader c5–c8, of the one array.

**c5–c8, the disc's matrix:**
- L = normalize(−sun direction (layout +0xE0)), R = normalize((0,0,1) × L), U = normalize(L × R), s = 1 / `moonScale`.
- The rows are (R·s, 0.5), (U·s, 0.5), (L′·s, 0) and (0, 0, 0, 1), multiplied by a rotation about L by the block's +0x7C angle.
- So the disc is projected on the plane facing the light, with border addressing (`sky.sta.xml`, border colour 0).

### The shaders, in short (for the port)

The vertex shader:
- p = view way + camera; clip z = 0.99999988·w, so the dome is drawn just inside the far plane.
- It passes the view way (o2) and the disc's uv, M·view way (o3).

The pixel shader:
```
d = normalize(view);  c = saturate(dot(-L, d));  gamma = acos(dot(-L, d));   // L = g_vLightParams[2].xyz, the sun's light's way
phi = atan(view.z^2 / dot(view.xy, view.xy));                              // as written: of z^2 over x^2 + y^2, unnormalised
D   = 170000 * (0.39 + 0.09*cos(phi) + 1/(1 + 25*gamma^2));                // path length, metres
ext = exp(-(c1.rgb * D)^2);                                                // Gaussian extinction
Lin = (c3.rgb*(2 + 0.5*c*c) + c2.rgb / pow(c0.x - c0.y*c, 1.5)) / c1.rgb * float3(c1.w, c2.w, c3.w) * (1 - ext);
f   = saturate(exp(-d.z^3 / (0.0636620 * F1.z)));                          // F = mml_vFogParams
sky = (F1.z <= 0.0015708) ? Lin : Lin + f * (1 - F1.x) * (F0.rgb - Lin);
t   = tex2D(smpCelestialBody, o3.xy / o3.w);
oC0 = float4(saturate(t.rgb * t.a * c4.x + sky), mml_fOpacityMultiplier);
oC1 = (0.34464, 0.99609375, 0.996084, 1);                                  // far depth, the second target
```
- The cosines in both phase terms are clamped (saturate), so they are 0 away from the sun.
- `sky.psh` takes its sun from `g_vLightParams[2]`, not the block.
- The fog vectors:
  - F0 = (`fogRed`, `fogGreen`, `fogBlue`, (t·(I − k·I) + k·I)²), with I = `fogIntensity` and k = block +0x24;
  - F1 = (1 − `fogCap` (block +0x10), `atmosphereAlpha` (+0x4C), `fogZenith` (+0x14, in degrees) × π/180 (`0x0055c459`: the double 0.017453279 at `0x00af14c0`), block +0x18 (set by no command)). Corrected 2026-10-18, at elgara-agent's question.
  - The shader reads F0.rgb, F1.x and F1.z.
- Clouds (another shader):
  - cloud[0] = (`cloudDensity`, `cloudSharpness`, `cloudDepth`, `cloudRangeMultiplier1`);
  - cloud[1] = (`cloudRed`, `cloudGreen`, `cloudBlue`, `cloudRangeMultiplier2`);
  - cloud[2] = a unit vector from the disc's frame.

### Opacity, AtmoAlpha and what is behind the dome

- **`mml_fOpacityMultiplier` is not `atmosphereAlpha`.** It is a plain material parameter of `sky.mat.xml`, 1.0 by default, written only by the generic object fade (0x004F43C0), which nothing applies to the sky. It goes only to the output alpha.
- **`sky.sta.xml` blends nothing:** depth test on, depth write off, no culling. So the opacity changes nothing on screen.
- **`atmosphereAlpha`** goes to fog[1].y, which `sky.psh` doesn't read.
- **The frame is cleared to black** (0, 0, 0, 1), with its depth (0x00529E6C; the colour from the render target's +0x50, set in 0x0045A845; other colours only under debug flags).
- **The draw order** is terrain, then the sky dome, then the rest (`dadefaultsub.sub.xml`). The dome covers whatever its far-plane depth passes.
