# The sky as the engine draws it

How the sky dome's shader constants are filled from an area's sky block and what the sky shaders do with them. The block itself is `dao/core/Sky.h`'s and `dao/Atmosphere.h`'s.

## The sky dome's shader parameters: read 2026-10-18

Read in the GOG exe and the sky shaders' disassembly (`C:\project\DAOElgaraShaders\build\scratch\fog\sky.vsh.txt`, `sky.psh.txt`) by lib-agent-2 (with subagents) for DAOParagonEngine #25. Two readings were done independently, the exe's fill and the shader's use, and they agree on each register. The constants were checked in the exe. No values were captured in play.

### Binding

`sky.mat.xml` declares `mml_vSkyParams`, `mml_vFogParams` and `g_vLightParams` by usage: SkyParameters (0x2C), FogParameters (0x2E) and LightsArray (0x11); CloudParameters is 0x2D. The shader-parameter provider (0x004F8A10) hands each the scene's vectors: sky at scene +0x38, fog at +0x18, clouds at +0x48.

The clouds' time is `ApplicationTime` (usage 0x29): the renderer's clock from the game's start, frozen by the pause, running through cutscenes (FRAME_EFFECTS.md, read 2026-10-23 by lib-agent).

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
- There is one disc, the sun's or the moon's as the sky material's CelestialBody texture has it (DefaultSun.dds in the day village's sky box). Its brightness is `moonAlpha` alone and its size `moonScale`.
- It is drawn twice: o3.w is the vertex's w (1) and nothing tests L·view, so the view opposite the light lands on the same uv. Vanilla's second disc is below the horizon; a sun moved low can bring it into view (elgara-agent saw it as a "moon" opposite the sun, 2026-10-21).
- The matrix is worked out again only on a frame the block is flagged (+0x84) or t moves (above), so a direction written alone left the disc where the last flagged frame put it, seen in play 2026-10-21 (DAOLib #222). `dao::atmosphere::SetSunlight` sets the flag.

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

## Water: its fog and its reflection cube (lib-agent-2, read 2026-10-19, for DAOParagonEngine #25)

### mml_vWaterFogParams (waterfog)

#### Short answers
- It is NOT a material value (b, not a). The .mao files of both meshes do not hold it, and Water.mat declares it only as `Usage="WaterFogParameters"` (an engine-provided InternalDataReference, like mml_vFogParams). Checked.
- The engine fills it from the area's sky block, from two GFF fields: `AtmoFogWaterIntensity` (22533) and `AtmoFogWaterCap` (22534) in the area layout (.arl). Checked in the exe and the files.
- Rule: x = (AtmoFogWaterIntensity)^2, y = 1 - AtmoFogWaterCap. If the file lacks a water field the loader falls back to the plain fog one (AtmoFogIntensity, AtmoFogCap). The port's stand-in (intensity^2, 1-cap of the plain fog) is the same rule with the wrong source field: it is wrong whenever the area has its own water values, as the Fade does.
- d is the squared distance from the camera to the vertex, in world space (checked in the listing). The floor y is applied in the pixel shader as a floor on the vertex's fog factor.

#### Pseudo-code
```
// per area, once
wi = layout.atmosphere("AtmoFogWaterIntensity") ?? layout.atmosphere("AtmoFogIntensity")
wc = layout.atmosphere("AtmoFogWaterCap")       ?? layout.atmosphere("AtmoFogCap")
mml_vWaterFogParams = (wi*wi, 1 - wc, unused, unused)

// water0.vsh: world = world-matrix * pos; d2 = dot(cam - world, cam - world)
f_v = min(1, exp(-d2 * x))            // o6.w = 1/exp2(d2*x*1.4427)
// water0.psh / water0nospec.psh:
f   = max(f_v, y)
rgb = fogColour + f * (shaded - fogColour)   // fogColour = mml_vFogParams[0].rgb (c7 / c6)
```
(Fade: the plain fog colour is (0.13, 0.16, 0.06); Wilds: (0.13, 0.16, 0.13).)

#### Values
| Area | AtmoFogWaterIntensity | AtmoFogWaterCap | x | y | plain fog (the stand-in) x, y |
|---|---|---|---|---|---|
| oth000d (Fade) | 0.004842 | 1.0 | 2.3445e-5 | 0.0 | 4.0e-6 (I=0.002), 0.0 |
| ost100d (Wilds) | 0.01962 | 0.95 | 3.8494e-4 | 0.05 | same as water (3.8494e-4, 0.05) |

Both meshes' .mao: Material Water.mat, normal map water_bump_n.dds, mat_vVSHWaterParams and mat_vPSHWaterParams only (Fade: DefaultSemantic NoSpec, PSH `0 0 0 0 | 1 0 500 0.01`; Wilds: Default, PSH `0.25 0.25 0.25 0 | 1 1 50 2`). No fog values. Checked (ghilana, oth000d_water_1442.mao, ost100d_water_581.mao).

So for the Fade the stand-in fog density is 5.9 times too low (4e-6 against 2.34e-5); the Wilds is identical.

#### Evidence
- Listings: C:\project\DAOElgaraShaders\build\scratch\materials\listings\water0.vsh.asm: `mml_vWaterFogParams c23`; lines 70-79: world position over w, `r0.yzw = camera(c22) - world`, `dp3` squared, `mul c23.x`, `mul 1.44269502 (c25.z)`, `exp`, `rcp`, `min o6.w, ., 1` = min(1, exp(-d2*x)). water0.psh.asm:23 `mml_vWaterFogParams c8` and line 102 `max r0.w, v4.w, c8.y`, then line 103 `mad oC0.xyz, r0.w, r0, c7` with `add r0.xyz, r0, -c7` before it: lerp from mml_vFogParams (c7) by the factor. water0nospec.psh.asm: same with c6/c7. Checked.
- Water.mat (in `build\scratch\materials\mat\water.mat.xml`:27, 63, 102, 138 ...): `<InternalParameter Name="mml_vWaterFogParams" Usage="WaterFogParameters"/>`. Checked.
- Provider 0x004F8A10 (the shader-parameter provider named in dev/SKY.md:11): switch on usage-0x11; usage 0x2F goes to 0x004F8AC5, which calls 0x005380B0: returns scene+0x24, size 0x10 (one float4 inline in the scene). Jump table read from the exe bytes (0x4F8CB8 index, 0x4F8C80 targets). Usage 0x2C/0x2D/0x2E go to the sky/cloud/fog. The numeric value 0x2F for WaterFogParameters is inferred from the switch's last entry and the pattern (0x2C Sky, 0x2D Cloud, 0x2E Fog); the usage name strings are not in the exe as plain strings.
- Scene sky update 0x00538FA0, tail at 0x0053956D-0x0053957F: `scene+0x24 = block+0x118` and `scene+0x28 = block+0x11C` (block+0xF0 + 0x2C). The block's +0x110 vec4 is sky[4] = c4 of dev/SKY.md:34 = (moonAlpha, block+0x80, (block+0x1C)^2, 1 - block+0x20), so block+0x118 = (block+0x1C)^2 and block+0x11C = 1 - block+0x20. Checked in the disassembly; the c4 formula is from dev/SKY.md:34 (a prior reading; the 1-cap form fits the numbers below).
- The loader of the layout's fields at 0x005D03A0-0x005D0560: field 0x5802 (22530 fog intensity) into esi+0x12C, 0x5803 (cap) into +0x130, 0x5805 (22533 water intensity) into esi+0x13C, with `fld [esi+0x12C]` as the default when the field is absent (0x005D047D); 0x5806 (22534 water cap) into +0x140, default `[esi+0x130]` (0x005D04D8); 0x5807 (tactical multiplier) into +0x144. Subtracting 0x120 (fog intensity +0x12C = block +0x0C, as in dev SKY Sky.h kSkyFields) gives block +0x1C and +0x20 for water intensity and cap, and +0x24 for the tactical multiplier (SKY.md:66 `k = block +0x24`). Checked in the disassembly; the 0x120 offset between the level struct and the block is inferred from the fields at +0x0C/+0x10 and +0x24, all consistent.
- File values: `python tools/gff4.py --dump <install>\packages\core\env\oth000d.arl` and ost100d.arl, the Atmo* fields (table above). Checked. gff4_fields.csv:2408-2409 names 22533/22534; core Level.cpp:456-457 names them but the paragon crate reads only AtmoFogIntensity/Cap (level.rs:1354-1362 and 1011-1022).

#### Not read
- The usage constant 0x2F by name (no string table); nothing was run in the game, so no captured register value.
- Whether z, w of the vector (scene+0x2C, +0x30) are ever set: the shaders read only x and y.
- The water fog in any area other than the two; areas whose layout lacks the water fields use the fallback above (read at 0x005D047D, 0x005D04D8).
- Whether the area's sky block can also be changed by script (SetAtmosphere has no water-fog parameter in Sky.h's list, so a script cannot change +0x1C/+0x20: inferred from kSkyFields having no entry there).

### Water: smpCubemap0 and COLOR0 (prefix watercube)

#### Short answers

1. **smpCubemap0 is the object's light-probe cube, as for Static1; the material names no cube.** [checked: both .mao files, below]. With no probe chosen the engine binds the texture `default_blackcubemap.dds` (`packages/core/data/textures.erf`, 632 bytes) [checked: exe]. Both levels do have probes. The Fade (oth000d) has exactly one, for the whole level, in `oth000d_ll.mmh`: `OTH000d_probe_13s.dds`, 256 px per face. The Wilds (ost100d) has five in `ost100d_ll.mmh`: `fa0`, `fa1`, `fa2`, `fa3`, `9ko`, each 16 px per face. [checked, ghilana]
2. **COLOR0: the premise is wrong. Both water meshes HAVE a COLOR element.** Stream 0, offset 16, Float16x4, usage 10 (Color), index 0, in a 28-byte vertex. No fallback stream or patched declaration is involved. [checked, .msh]
3. **The Fade's water body colour really is black in vanilla.** Its vertices' COLOR rgb is exactly (0,0,0) in all 6920 vertices, and its material has reflection multiplier 0. What the Fade water shows is the fog colour mixed in by distance, plus alpha from the vertex alpha. So the port's black is right near the camera; only the fog mix and alpha have to be right. The Wilds' vertex colour is a dark green (0.18..0.27, 0.22..0.29, 0.15..0.19). [checked]

#### Pseudo-code a port can copy

```
// per vertex (water0.vsh): scatter = sky-scatter rgb computed from the sun, c12-c15; w = 1
o3 (texcoord2) = float4(scatter.rgb, 1) * COLOR0      // the Fade: COLOR0.rgb = 0 -> o3.rgb = 0; o3.w = COLOR0.w
// COLOR0 are float16x4 at byte 16 of the 28-byte vertex; .w is a per-vertex "shore/depth" alpha (Fade -42.8..12.0, Wilds -5.8..5.1)

// per pixel (water0 = semantic Default, water0nospec = semantic NoSpec): v1 = o3
body   = max(N.L, 0) * v1.rgb
refl   = cube(smpCubemap0, reflect(-V, N)).rgb * P.refl      // Default: c5.y = 1 (Wilds). NoSpec: c4.y = 0 (Fade)
f      = fresnel(N.V)                                         // pow(1-N.V, power) * 0.98 + 0.02, 0 if N.V<0
rgb    = lerp(body, refl + spec, f)   // Default adds pow(...)*c3.rgb spec * refl-mult; see the listings
a0     = pow(max(v1.w, 0), P.alphaPow)                        // Default 2, NoSpec 0.01
alpha  = f * (1 - a0) + a0
out    = fogColor + max(v4.w, fogParams.y) * (rgb - fogColor)  // v4.w = exp(-dist * waterFog) from the vs
```

```
cube_for_water_draw:
    probe = the best-scoring probe of the object's light state (ChooseForLevel, group +0x20); score = 1 / distance, 0 for another room
    return probe ? probe.EnvLightProbeEnvMap : "default_blackcubemap.dds"
```

For the Fade nothing depends on the cube, because P.refl = 0. For the Wilds, bind the probe nearest the water mesh's place, from `ost100d_ll.mmh`, translations (x, y, z): fa1 (-5.5, 107.1, -1.3), fa2 (23.4, -59.2, -1.3), fa0 (-17.8, 66.6, -1.3), 9ko (-124.8, 66.6, -1.3), fa3 (124.8, -121.2, 1.3). The probe nodes sit under the GOB node. The one with the highest 1/dist wins. [inferred: which object place the water mesh is scored from]

#### Evidence

##### Q1, the cube

- Both materials (`MaterialObject`, `Water.mat`) hold only `mat_tNormalMap = water_bump_n.dds`, `mat_vVSHWaterParams` and `mat_vPSHWaterParams`. No cube texture parameter. [checked, ghilana `resource` of `oth000d_water_1442.mao` and `ost100d_water_581.mao`]
  - Fade: `DefaultSemantic NoSpec`; VSH params `0.05 -2.19e-9 0.1 0 | 0.0751754 0.0273616 0.2 0 | 0.00707107 0.00707107 0.1 0`; PSH params `0 0 0 0 | 1 0 500 0.01`.
  - Wilds: `DefaultSemantic Default`; VSH params `0 0.04 0.1 0 | 0.034641 -0.02 0.1 0 | -0.034641 -0.02 0.1 0`; PSH params `0.25 0.25 0.25 0 | 1 1 50 2`.
- The water shaders declare `samplerCUBE smpCubemap0` at s10, the same register as the lit level shaders (`water0.psh.asm:11`, `:25`, `:38`). [checked]
- The engine's per-object cube is `LightState` +0x114, with its three irradiance matrices at +0x130, +0x170 and +0x1B0 (DAOElgaraShaders `dev/NOTES.md:17`; DAOLib `src/dao/Addresses.h:2545-2551`). `ChooseForLevel` 0x00464310 and `ChooseForMoving` take probes from the list at +0x3C into group +0x20 (DAOLib `dev/EFFECTS.md:1049-1050`; the table at `:1040`). A probe's score is 1 / distance, and zero for another room (`dev/NOTES.md:15`).
- **Fill's no-probe path, read here, new** [checked: disassembly of `LightState::Fill` 0x0046B260 from 0x0046B80F]. When the probe group is empty, the code runs only if the state already holds a probe (`[esi+0x110] != 0`). It builds the texture named by the global wide string at 0xC39D14 (call 0x8472c0, then 0x845d80), stores it at `esi+0x114`, and sets the three matrices at +0x130, +0x170 and +0x1B0 from the object returned by 0x4D33C0. It then clears `[esi+0x110]`.
  - The global 0xC39D14 is initialised at 0x00AD3A40 from the wide string at VA 0xB2B2EC, **`default_blackcubemap`**. The constructor at 0x0046B1F0 loads the same global into +0x114.
  - The resource is `default_blackcubemap.dds`. So a state with no probe always has a black cube, never null.
  - Two other texture names are in the exe's data: `bad_cubemap` (wide string at VA 0xAF99E8; DAOElgaraShaders `dev/NOTES.md:174` calls `bad_cubemap.dds` and `default_blackcubemap.dds` the engine's two single-picture cubes) and `Default_Black` (VA 0xAF4194, a 2D texture, not the cube). Not traced to a use.
  - The matrices in the no-probe case come from 0x4D33C0 [inferred: a zero or identity set; its content was not read]. The fallback means no irradiance, a black cube.
- Probes of the two levels [checked, ghilana]:
  - `oth000d_ll.mmh`: one `nlpr` node, "LightProbe", `EnvLightProbeEnvMap = OTH000d_probe_13s.dds` (ghilana `texture`: 256x256, 262352 bytes in `oth000d.gpu.rim`), irradiance `OTH000d_probe_13s.mtx`. The ambient-light node reads (0.24, 0.29, 0.18). The cube shows a green sky and dark branches.
  - `ost100d_ll.mmh`: five probes, listed above, each a 16x16 cube (1232 bytes, `ost100d.gpu.rim`); ambient (0.134, 0.196, 0.110).
  - Both are terrain levels, one `_ll.mmh` for the whole level (`dev/NOTES.md:27`, "A level is of rooms or of terrain"). The probes are therefore the level's, with no room filter.
- The water mesh's own `.mmh` has `MMHMaterialObject = oth000d_water_1442.mao`, an attribute `BaseLight`, and no probe or cube parameter. The mesh's bbox is (0,0,0)-(1000,1000,0), so the object is large; the scoring place is not read.

##### Q2, COLOR0

- `oth000d_water_1442.msh` and `ost100d_water_581.msh` (GFF4 MESH, read with `tools/gff4.py` and `core`'s element enum): one chunk each, `VertexSize` 28, 6920 and 11194 vertices. The declarator is: [checked]

  | offset | datatype | usage |
  |---|---|---|
  | 0 | 16 (Float16x4) | 0 Position |
  | 8 | 16 (Float16x4) | 3 Normal |
  | 16 | 16 (Float16x4) | **10 Color, index 0** |
  | 24 | 15 (Float16x2) | 5 TexCoord |
  | end marker | | |

- The vertex shader declares `dcl_position v0`, `dcl_color v1` and nothing else (`water0.vsh.asm:42-43`), and uses v1 only at `mul o3, r1, v1` (`:151`). The mesh supplies it, so no default is needed. [checked]
- The colours, read as half floats at byte 16 of each vertex (script `lib-agent-2-watercube-col.py` under %TEMP%): [checked]
  - Fade: rgb = (0, 0, 0) in all 6920 vertices, 3685 distinct values, all in w. w runs -42.8 to 12.0, with 2.0 at 927 vertices and about 3.17 at about 660.
  - Wilds: rgb in 0.18..0.27 / 0.22..0.29 / 0.15..0.19 (the most common (0.264, 0.285, 0.187)), w from -5.8 to 5.1.
- Fade's final colour [checked from `water0nospec.psh.asm:28-111`]: `c3 = (0,0,0)` makes every normal layer's strength 0, so the normal is up (w/ the depth blend). `c4.y = 0` makes the cube's term 0, so `rgb = lerp(N.L * v1.rgb, 0, fresnel)`, and `v1.rgb = scatter * 0 = 0`. Final colour = `fogColor + max(v4.w, c7.y) * (0 - fogColor)`: black up close, fading to the fog colour with distance (`v4.w` = exp of minus distance times `mml_vWaterFogParams.x`, at `water0.vsh.asm:75-80`). The alpha is `f*(1-a) + a`, with `a = max(v1.w,0)^0.01`: about 1 where the vertex alpha is positive, and just the Fresnel term (0.02 + 0.98 (1-N.V)) where it is 0 or negative.
- Fallback streams, patched declarations or shader variants chosen by vertex elements: **not looked for**, because the meshes carry the element. [Not read]

#### Not read

- The scoring place used for the water object (mesh origin, bbox centre or the instance's place) and so which of the Wilds' five probes the engine gives it.
- Whether the water draw really takes its cube from `LightState` +0x114 rather than another binding. Inferred from the shader's s10 being the same register as the lit shaders and from the Wilds' look; the sampler's binding call was not traced.
- What 0x4D33C0 returns (the matrices of the no-probe case).
- Whether the D3D9 runtime supplies (0,0,0,1) for an absent COLOR input and whether any vanilla mesh lacks the element. Not needed for these two meshes.
- The fog colour constants (`mml_vFogParams`) for the Fade; `dev/SKY.md` has the sky block.
