# Trees and grass: SpeedTree, placed by the rooms

How an area's trees and grass come to be drawn: the models the area lists, the instances each room places, SpeedTree's generated geometry, the level of detail and the wind. Core's `Level` reads the list, not the instances. Read 2026-10-18 in the GOG exe and vanilla's files (shaders through `fxc /dumpbin`) by lib-agent-2 (with subagents), for DAOParagonEngine's side-by-side of the Harrowing's Fade. Not seen in play. The key claims were checked again in the listings, the files or the disassembly.

- **The tree models** are the `.arl`'s `EnvAreaTreecontrollerList` (3355), each `EnvTreeFile` (3353) an `.spt` (0-based ids). `oth000d` has 9: `hydra`, `gra_x_short`, `hair`, `root`, `sporebig`, `gra_x_coral`, `gra_x_sporelw`, `gra_x_thin1`, `rootb`. `EnvTreeColorLevelTint` (3377) is (1, 1, 1, 1). Core's `Level` reads both.
- **The instances are in each room's `.rml`:** `EnvRoomTreenodeList` (3354), each with `Position` (4), `Orientation` (5) in the room's space, `EnvTreeScale` (3356), `EnvAreaTreecontrollerID` (3357), `EnvTreeColorTint` (3375) and `EnvTreeDrawDistance` (3379, an int8). The room reader (0x00A28D50, checked: 0x00A28D62) keeps them as 0x50-byte records at the room's +0x10C. **Core's `level::RoomOf` doesn't read them yet.**
- **`oth000d`** has 343 instances, ids 0–4 only: hydra 22, `gra_x_short` 185, hair 44, root 47, sporebig 45. All are turned about z alone, scaled 0.28–0.40, draw distance 0.
  - Near `bhm600wp_pc_start` (129.30, 165.61, 26.43): a row of dark-tinted (≈0.235, 0.29, 0.18) `sporebig` and `hair` along y ≈ 171, x 116–133, 6–14 m away. That row is likely the missing "stakes and columns" (inferred).
- **The `.spt` files** (SpeedTree 4.x, `__IdvSpt_02_`) hold procedural parameters, not meshes. `speedtreert.dll` (SpeedTreeRT 4.1, 32-bit; the exe imports 61 of its functions) generates the geometry.
  - **The model** (`SpeedTreeLibrary::AddTree` 0x00536D30 → 0x00531B80 → 0x00533DA0): `LoadTree`, `SetTreeSize(−1, −1)`, `Compute(NULL, seed 1, true)`, leaf rocking and drop-to-billboard on. With the seed fixed, every instance of a model shares one geometry.
  - Branch textures are `bk_x_*` in `textures.erf`; leaves and fronds use `oth000d_Diffuse.dds`; billboards use `oth000d_Diffuse_Billboards.dds` and `_Normal_Billboards.dds` (`oth000d.gpu.rim`).
- **An instance** (0x005CBF21):
  - position = the room's position + the room's rotation · the node's;
  - a yaw only, from node ⊗ room (`atan2(−2(xy − wz), 1 − 2(x² + z²))`);
  - a uniform scale;
  - `rand() % 100`, the wind's row offset, one value for nearly a whole area (section 5);
  - the tint;
  - the world's wind, unless the room's +0x14 is −3.
- **Level of detail** (0x0052E260, 0x0052CD60): `s = clamp(r, 2, 30)`, where r is the instance's +0x110 (read at 0x0052E263), the radius of its bounding sphere: |(max − min)/2| of the model's `GetBoundingBox`, not its size token, × the instance's scale (`UpdateBoundingSphere` 0x0052E360, the one writer, the store at 0x0052E47E, from `SetScale`, `CreatePhysicsActor` and `UpdateRenderTransform`). `t = (s − 2)/28`; near = lerp(15, 20, t) and far = lerp(35, 120, t) (the floats 0x00BF73E0..EC), each × the tree's step, the byte at instance +0x116 from the placed tree's +0x24, 0 to 3 (0x005313C0): near × {1, 1.5, 2, 0.001} (0x00AF6E50), far × {1, 1.5, 2, 0.004} (0x00AF6E60) [C]; near² and far² are kept at +0x118 and +0x11C. **A second, global factor g** multiplies them each frame as g² (`SetCameraAndLodFactor` 0x0052E9A0 stores 0x00BF73F8, `UpdateGroupLods` 0x00536010 applies it): g is `g_treeLodFactor` 0x00BF22D8, by the graphics detail 0 to 3: 0.5, 1, 1.2, 1.5 (`SetGraphicsDetailLevel` 0x00458EB0), in Explore and Combat; in any other game mode that × min((π/3)/v, 3), v the camera's slot 0x84 [I: its field of view]; and 1 if the graphics object's slot +0xF8 answers false [I: what that is]. lod = 1 − clamp((d² − near²)/(far² − near²), 0, 1), the billboard alone at 0.
- **The wind** is the `.arl`'s `EnvAreaGlobalwindResRef` (`oth000d_193.wnd`, a GFF4 `WND`: strength 0.25, direction (0, 1, 0), gusts, oscillation and leaf rocking), given to SpeedTree's wind: only `WndStrength`, `WndDirection` and the gusts reach it; the file's matrix count and oscillation values are not used, the engine's five `CSpeedWind`s coming from a constant text in the exe (below; read 2026-10-18 by lib-agent-2).
- **Grass is SpeedTree too** (`gra_x_*`, placed the same way). There is no separate grass system, density map or scatter: the `.rml`s' `EnvScatter*` lists are empty in all 3,526.
- **For a 64-bit port** (inferred): the geometry exists only as `speedtreert.dll`'s output, so a small 32-bit tool calling it once per `.spt` and writing the meshes would serve.

## The trees' geometry: what ships, and the DLL (read 2026-10-18 by lib-agent-2)

Read in the GOG install, the toolset's installer and the exe by lib-agent-2 (with a subagent), for DAOParagonEngine. Not run.

### No geometry ships in any other form

- **Neither the game nor the toolset has baked tree meshes or a SpeedTree cache.** ghilana lists 685 `.spt` files across core and the add-ins, and no `.srt`, `.stf`, `.sdx` or cache files. The only `.msh`/`.mmh` files with tree or grass names are other things (`plc_statmnstre_06_0`, creatures `cn_*_gra`). On disk the only SpeedTree file is `bin_ship\speedtreert.dll`.
- **The toolset's** `packages\core\data_tools\speedtreetools.erf` (180 MB) is the source library: 166 `.spt`, 147 `.dds` and 126 bark `.tga`, with no meshes.
  - Its lightmapper (`tools\lightmapper\LightMapper.py`) reads only `.msh` chunks, so no tree geometry is exported for the bake.
  - `SpeedTreeCAD.exe` (IDV's authoring tool, 4.1.1.0) can export a static mesh (`.obj`, `.X`) by hand, one tree at a time (untested).
  - The toolset's `SpeedTreeRT.dll` is the game's, byte for byte.
- **The level's static physics** (`oth000d.opf`, NxuStream2 XML after an 8-byte prefix) has one capsule per tree instance: 158 actors in group 5, each turned about z, at the instance's world position. The capsule is the `.spt`'s collision object (token 12003) times the instance's scale (hydra: r 2.4957, length 24.465 × 0.294 = r 0.7338, height 7.194). So it gives each tree's position, yaw and scale and its trunk, but no branches.
- **Billboards:** `<level>_diffuse_billboards.dds`/`_normal_billboards.dds` (1024², DXT5) hold side views. Each tree's rectangles in that atlas are its `.spt`'s token 10003, 8 floats each: `hydra` and `hair` 8 angles, `root`, `rootb` and `sporebig` 1. The grass has none. The quad's size isn't stored (it is the tree's bounds from `Compute`).

### The `.spt` format

- spttools (J. Sadeharju, GPL, written for Oblivion; github.com/anahuj/spttools, its `FORMAT` and `sptparser.c`) documents the tokens. A walker built from it parses all 9 of `oth000d`'s files to the last byte.
- The stream is int32 tokens, each followed by a typed value (int, float, byte, or an int-length string). Bézier splines are text: `BezierSpline min max var { n; x y u v w … }`.
- **The sections:**
  - 1000 the header; 1014 the levels;
  - 2000–2007 the bark map, LOD distances, the seed, the size and its variance;
  - 6000–6017 per level: the splines (disturbance, gravity, flexibility, length, radius, start angle) and segment counts;
  - 3xxx/4xxx leaves; 8xxx materials; 9xxx LOD;
  - 10002/10003/10004 the composite UVs of leaves, billboards and fronds;
  - 12xxx collision objects (73xxx a capsule's angles);
  - 13xxx/14xxx fronds; 16xxx flares; 26xxx roughness, gnarl, forks; 40xxx roots; 50xxx texture coordinates; 60xxx/70xxx map layers;
  - 71xxx embedded leaf meshes, which the grass uses; 75xxx fades.
- The trees here have 3 levels; the grass has 5, with blade fronds (13007 = 1).
- **Reproducing `Compute` exactly isn't realistic:** that means the DLL's generator and its random numbers, about 250 KB of code. A stand-in of tubes along the splines and leaf cards would look different.

### The DLL from a small 32-bit tool

- `speedtreert.dll`: x86, 243 exports, importing only MSVCR80, MSVCP80 and KERNEL32. Its manifest asks for `Microsoft.VC80.CRT` 8.0.50608.0, and WinSxS holds 8.0.50727.x.
- **No licence key:** it is "SpeedTreeRT Version 4.1 (Windows PC), Fully Licensed Build", and the exe imports neither `Authorize` nor `IsAuthorized`.
- **The exe's calls:** `LoadTree` from memory (0x00531C56); `SetTreeSize` from its +0x1C0, +0x1C4; `Compute(NULL, 1, true)` (0x00533E19–0x00533E21, checked). The engine's tree class derives from `CSpeedTreeRT` (its constructor on `this` at 0x005355DC), and is itself 0xD60 bytes (`SpeedTreeModel`, allocated at 0x00536D95, constructor 0x005355B0): the `CSpeedTreeRT` base is at most its first 0xF8 bytes, its name is at +0x1C8 (compared ignoring case by `FindModelByName` 0x00532E60) and its instance counter at +0x1E8. The vector of models (+0x1F8, +0x1FC) is `SpeedTreeLibrary`'s (0x21C bytes). This corrects "at most 0x1C0 bytes" (two readings reconciled, 2026-11-01).
- **`SetTreeSize(−1, −1)` is most likely refused,** keeping the file's own size: the DLL says "SetTreeSize() is only valid for size values greater than 0.0" (inferred).
- **The exports a baker needs:**
  - `??0CSpeedTreeRT@@QAE@XZ`, `??2CSpeedTreeRT@@SAPAXI@Z`, `??3CSpeedTreeRT@@SAXPAX@Z`;
  - `?LoadTree@CSpeedTreeRT@@QAE_NPBEI@Z` (from memory), `?LoadTree@CSpeedTreeRT@@QAE_NPBD@Z` (from a file);
  - `?SetTreeSize@CSpeedTreeRT@@QAEXMM@Z`, `?Compute@CSpeedTreeRT@@QAE_NPBMI_N@Z`;
  - `??0SGeometry@CSpeedTreeRT@@QAE@XZ`, `?GetGeometry@CSpeedTreeRT@@QAEXAAUSGeometry@1@I@Z`;
  - `GetNum{Branch,Frond,Leaf}LodLevels`, `GetBoundingBox`, `GetNumCollisionObjects`/`GetCollisionObject`, `GetMapBank`, `GetLeafBillboardTable`, `UpdateBillboardGeometry`.
- `SGeometry`'s layout has no public header: it is to be read from the exe's `GetGeometry` callers or the DLL (not read).
- **So the plain route** is a small x86 baker on the player's machine. It loads the game's own DLL and, for each `.spt`, runs `LoadTree`, `SetTreeSize(−1, −1)`, `Compute(NULL, 1, true)` and `GetGeometry` per level of detail, and writes a cache the port reads. With the seed fixed and the size the file's, one bake per file should be exact, instances differing only by their transform (inferred: not yet run).

## SpeedTree at run time: the geometry structs, billboards, tint and wind (read 2026-10-18 by lib-agent-2)

For DAOParagonEngine #31. Read 2026-10-18 by lib-agent-2 in the GOG exe (`game\DAOrigins.exe`, base 0x400000), `bin_ship\speedtreert.dll` (SpeedTreeRT 4.1, base 0x10000000) and vanilla's `spt_*` shader listings (`C:\project\DAOElgaraShaders\build\scratch\materials\listings\`, fxc 9.19.949.2111). Not run, not seen in play. "Checked" = read in the disassembly, listing or file; "inferred" = reasoned from what was read.

### 1. SGeometry, SIndexed and SLeaf in 4.1's layout

#### SGeometry (0x198 bytes; its constructor 0x10025D50, checked)

| Offset | Field | Evidence |
|---|---|---|
| 0x000 | `SIndexed m_sBranches` (0x60) | ctor; `GetBranchGeometry` 0x1002A390 fills it (mask bit 1) |
| 0x060 | `SIndexed m_sFronds` (0x60) | ctor; `GetFrondGeometry` 0x1002A6B0 (mask bit 2) |
| 0x0C0 | `int m_nNumLeafLods` | `GetLeafGeometry` 0x100245D0 writes `GetNumLeafLodLevels()` |
| 0x0C4 | `SLeaf* m_pLeaves` (array, stride 0x48) | same; the tree's own array at `CSpeedTreeRT+0x10` |
| 0x0C8 | `S360Billboard m_s360Billboard` (0x34) | ctor 0x10023F30; `GetBillboardGeometry` (mask bit 8) |
| 0x0FC | `SHorzBillboard m_sHorzBillboard` (0x9C) | ctor 0x10023F70 |

`GetGeometry(SGeometry&, mask)` 0x1002ABB0: bit 1 branches, 2 fronds, 4 leaves, 8 billboards (checked).

#### SIndexed (0x60 bytes = 24 dwords; ctor 0x10023DA0, filled by 0x1002A390 for branches, checked)

| [i] | Offset | Field | Source in the DLL | The exe |
|---|---|---|---|---|
| 0 | 0x00 | `int m_nNumLods` | `GetNumBranchLodLevels()` | |
| 1 | 0x04 | `const int* m_pNumStrips` | internal +0x5C | index build |
| 2 | 0x08 | `const int** m_pStripLengths` | internal +0x60 | index build |
| 3 | 0x0C | `const int*** m_pStrips` | internal +0x64 | index build |
| 4 | 0x10 | `int m_nNumVertices` | coords vector size / 3 | the vertex count (0x533A6F) |
| 5 | 0x14 | `const unsigned int* m_pColors` | 4-byte vector | read when the lighting method is static |
| 6 | 0x18 | `const float* m_pNormals` (×3) | | NORMAL |
| 7 | 0x1C | `const float* m_pBinormals` (×3) | | BINORMAL |
| 8 | 0x20 | `const float* m_pTangents` (×3) | | TANGENT |
| 9 | 0x24 | `const float* m_pCoords` (×3) | | POSITION |
| 10 | 0x28 | `const float* m_pWindWeights[0]` | float vector internal +0xEC | wind float4 .y |
| 11 | 0x2C | `const float* m_pWindWeights[1]` | float vector +0x10C | wind float4 .w |
| 12 | 0x30 | `const unsigned char* m_pWindMatrixIndices[0]` | byte vector +0xFC | wind float4 .x |
| 13 | 0x34 | `const unsigned char* m_pWindMatrixIndices[1]` | byte vector +0x11C | wind float4 .z |
| **14** | **0x38** | **`const float*` per vertex: the LOD fade hint** | float vector internal +0x12C | **not read** |
| 15 | 0x3C | `float`: the branch (fronds: frond) **LOD fade distance** | `CSpeedTreeRT+0xE4` (fronds +0xE8) | not read |
| 16–23 | 0x40–0x5C | `const float* m_pTexCoords[8]` (float2 each) | internal +0x13C + 0x10·layer | layers 0, 1, 2 and 7 read |

- **[14] (checked as a per-vertex float array; its meaning inferred):** the DLL's only writer is 0x10016FD0, `vec12C[i] = f`, called from three generator loops (0x10007B68, 0x10011C66, 0x1001368A), each storing `1 − k/n` for the vertices of one piece of geometry. A vertex no loop touches keeps the default, −1. So it is the per-vertex LOD fade hint ("the LOD value at which this vertex fades; −1 none"), what SpeedTree 4.x calls `m_pLodFadeHints`. **It is not a second wind index**: the two weights and two indices are [10]–[13]. The peer's −1 everywhere is the default.
- **[15] (checked):** `ParseSupplementalLodInfo` 0x10029B60 reads token 75002 into `CSpeedTreeRT+0xE4` (branches) and 75003 into `+0xE8` (fronds); `GetBranchGeometry` copies +0xE4 to SIndexed+0x3C (0x1002A689), `GetFrondGeometry` +0xE8 to SGeometry+0x9C (0x1002A9BA). spttools' FORMAT names them "LOD/Branches/Fade distance" and "LOD/Fronds/Fade distance". All 9 of `oth000d`'s `.spt`s: 75002 = 0.15, 75003 = 0.2, 75004 (apply fading to extruded fronds) = 0, 75005 (billboard transition / 2) = 0.15.
- **The exe uses neither [14] nor [15]** (checked): the branch/frond vertex filler 0x533F70 reads +0x10 to +0x34 and the texture layers at +0x40; the layout builder 0x5339E0 tests +0x18, +0x1C, +0x20, +0x24, +0x40, +0x44, +0x48; and no code reads the model's branch SIndexed +0x38/+0x3C (`model+0x24C/0x250`) or the frond one's (`model+0x46C`; the +0x470 hits are another class's bounding box). DA:O draws no per-vertex LOD fade on branches or fronds; a port can drop [14] and [15].

**The exe's branch and frond vertex** (layout 0x5339E0, filler 0x533F70, checked). The model keeps the branches' SGeometry at +0x214 (SIndexed +0x214) and the fronds' at +0x3D4 (SIndexed +0x434), each from its own `GetGeometry` call (0x533A57 mask 1, 0x533A37 mask 2). The vertex, in order, each element only if its array is non-null:
- POSITION: FLOAT16_4 (type 0x10), from [9] with w = 1 (0x52BE60 converts float3 to half4);
- COLOR (D3DCOLOR) from [5] if `GetBranchLightingMethod() == 1` (static), else NORMAL, BINORMAL, TANGENT, each FLOAT16_4 from [6], [7], [8];
- TEXCOORD0 FLOAT4, if `GetBranchWindMethod() == 0` (GPU): `(float(idx0[i]), weight0[i], float(idx1[i]), weight1[i])` = ([12], [10], [13], [11]) (0x5340D6–0x534138);
- then FLOAT2 texcoords for layers 0 (diffuse), 2 (normal), 1 (detail) when non-null, and always a last FLOAT2 for layer 7 (shadow), zeros when that layer is null.
The fronds use the branch methods too (the same two calls).

#### SLeaf (0x48 bytes, a vtable at +0; ctor 0x10023E30, copy ctor 0x100014E0)

Read in the exe's leaf-card filler 0x52C5B0 and leaf-mesh filler 0x52C8E0 (checked), the DLL's fill 0x1001D740 and the shader `spt_leafcards.vsh`:

| [i] | Offset | Field | Evidence |
|---|---|---|---|
| 0 | 0x00 | vftable `??_7SLeaf` (virtual destructor) | ctor |
| 1 | 0x04 | `int m_nDiscreteLodLevel` (ctor −1) | the DLL writes the LOD index (0x1001D755, 0x1001BCD2) |
| 2 | 0x08 | `int m_nNumLeaves` | DLL 0x1001D77B; both exe fillers loop to it |
| **3** | **0x0C** | **`float m_fLeafRockScalar`** (ctor 1.0) | DLL 0x1001D785 copies the leaf parameters' +0x3C: **token 21000 "Wind/SpeedWind Support/Leaf rock angle scalar"** |
| **4** | **0x10** | **`float m_fLeafRustleScalar`** (ctor 1.0) | DLL 0x1001D790 copies +0x40: **token 21001 "Leaf rustle angle scalar"** |
| 5 | 0x14 | 4-byte, not float (copied by `mov`); not read by the exe | inferred: a flag or pointer |
| 6 | 0x18 | `const unsigned char* m_pLeafMapIndices` (per leaf: which card) | 0x52C666 |
| 7 | 0x1C | `const float* m_pCenterCoords` (float3 per leaf) | POSITION of a card; TEXCOORD3 of a mesh |
| 8 | 0x20 | per-leaf pointer, not read by the exe | inferred: `m_pColors` |
| 9 | 0x24 | `const float* m_pDimming` (float per leaf) | card TEXCOORD2.w; the shader writes `max(v4.w, 0.75)` |
| 10 | 0x28 | `const float* m_pNormals` (4 corners × float3 per leaf) | card NORMAL |
| 11 | 0x2C | `const float* m_pBinormals` (same) | mesh TEXCOORD0–2 basis |
| 12 | 0x30 | `const float* m_pTangents` (same) | mesh TEXCOORD0–2 basis |
| 13 | 0x34 | `const float* m_pWindWeights[0]` | wind float4 .y |
| 14 | 0x38 | `const float* m_pWindWeights[1]` | wind float4 .w |
| 15 | 0x3C | `const unsigned char* m_pWindMatrixIndices[0]` | wind float4 .x |
| 16 | 0x40 | `const unsigned char* m_pWindMatrixIndices[1]` | wind float4 .z |
| 17 | 0x44 | `const SCard* m_pCards` (stride 0x24) | indexed by [6] |

- **[3]/[4] are the rock and rustle scalars (checked):** `oth000d`'s grass `.spt`s hold 21000 = 0.2, 21001 = 0.4, exactly the peer's grass values; its trees hold 0/0 (no leaf rocking). The exe copies SLeaf[0]'s two to the model's +0xAB0/+0xAB4 (0x52C5E9–0x52C5FD), then writes `(rock, rustle, 0, 0)` to the model's +0xC6C (0x531316–0x531387). The material setup binds +0xC6C to `g_vLeafAngleScalars` by name (0x5301A0–0x5301B5), which is c59 in the leaf shaders. In `spt_leafcards.vsh`: `rock = c59.x · g_vLeafAngles[a].x`, `rustle = c59.y · g_vLeafAngles[a].y`, a = the leaf's angle index (below). They are neither alpha-test values nor cluster indices.
- `SLeaf` has no alpha-test value in 4.1's layout: nothing between +0x0C and +0x44 is read as one.

#### SCard (0x24 bytes; ctor 0x10023EF0 sets +0x00–0x14 to −1.0 and +0x18–0x20 to 0)

| Offset | Field | Use in the exe (0x52C79A–0x52C819) |
|---|---|---|
| 0x00, 0x04 | `m_fWidth, m_fHeight` | card TEXCOORD1.xy |
| 0x08, 0x0C | `m_afPivotPoint[2]` | TEXCOORD1.zw = pivot − 0.5 |
| 0x10, 0x14 | `m_afAngleOffsets[2]` (degrees) | TEXCOORD2.xy = offsets × π/180 (0.0174533, 0xAF14C0) |
| 0x18 | `const float* m_pTexCoords` (4 corners × float2) | TEXCOORD0.xy |
| 0x1C | `const float* m_pCoords` (inferred; not read) | |
| 0x20 | `const SMesh* m_pMesh` | non-null with > 2 indices: the leaf is a mesh, drawn by the mesh path |

**The exe's leaf-card vertex** (stride 0x30, all FLOAT16_4; layout 0x52C0A0 mode 4; filler 0x52C5B0): 6 vertices per card (two triangles, corners {0, 1, 2, 0, 2, 3}), cards whose mesh pointer is null only.
- POSITION = (center[leaf], 1)
- NORMAL = (normals[leaf·4 + corner], 1)
- TEXCOORD0 = (u, v, corner, 1) from the card's texcoords
- TEXCOORD1 = (width, height, pivotX − 0.5, pivotY − 0.5)
- TEXCOORD2 = (angleOffset0 rad, angleOffset1 rad, leaf % GetNumLeafAngleMatrices(), dimming[leaf])
- TEXCOORD3 = (idx0, weight0, idx1, weight1)

### 2. SMesh (0x20 bytes; ctor 0x10023EB0 zeroes 8 dwords)

| Offset | Field | Evidence (exe 0x52C8E0, checked unless marked) |
|---|---|---|
| 0x00 | `int m_nNumVertices` | the vertex loop's bound (0x52CB92) |
| 0x04 | `const float* m_pCoords` (float3) | POSITION |
| 0x08 | `const float* m_pTexCoords` (float2) | TEXCOORD4.xy |
| 0x0C | `const float* m_pNormals` (float3) | NORMAL |
| 0x10 | `const float* m_pBinormals` (inferred; not read) | |
| 0x14 | `const float* m_pTangents` (inferred; not read) | |
| 0x18 | `int m_nNumIndices` | > 2 makes the leaf a mesh (0x52C98B); the index loop |
| 0x1C | `const int* m_pIndices` (triangle list) | `index = m_pIndices[k] + first vertex of this leaf` into the model's index array +0xABC (0x52CBD0) |

**How the exe builds a mesh leaf** (layout 0x52C0A0 mode 8, stride 0x40, all FLOAT16_4; filler 0x52C8E0). For each LOD, each leaf whose card has a mesh, each mesh vertex:
- POSITION = (mesh coords[v], 1), in the mesh's own space;
- NORMAL = (mesh normals[v], 1);
- TEXCOORD0/1/2 = the rows of the leaf's frame, from corner 0 of SLeaf's tangent (+0x30), binormal (+0x2C) and normal (+0x28): TC0 = (T.x, B.x, N.x, 1), TC1 = (T.y, B.y, N.y, 1), TC2 = (T.z, B.z, N.z, 1). The shader turns the mesh into the leaf's orientation with them;
- TEXCOORD3 = (center[leaf], 1), the leaf's position;
- TEXCOORD4 = (u, v, leaf angle index, dimming[leaf]);
- TEXCOORD5 = (idx0, weight0, idx1, weight1).
The counts come first (0x52C170): a mesh leaf adds `m_nNumVertices` to the mesh vertex count (+0x734) and `m_nNumIndices` to the index count (+0xAB8); a card leaf adds 6 vertices (+0x574) and 2 triangles. The two leaf SGeometry copies are the model's +0x594 (cards) and +0x754 (meshes), each from `GetGeometry(…, 4)` (0x52C110).

### 3. The billboards: the image by view angle, and the fade from the 3-D tree

#### The vertex the exe writes (per instance, 4 corners of 0x50 bytes; 0x530A20, checked)

The model's billboard SGeometry is at +0x914, from `GetGeometry(…, 8)` (0x533D6A), so its S360Billboard is at +0x9DC. S360Billboard (0x34 bytes, ctor 0x10023F30, filled by `GetBillboardGeometry` 0x10024650, checked):
- +0x00/+0x04: width and height;
- +0x08: the number of images, one less when the tree has a horizontal image;
- +0x0C: the texcoord table, 8 floats an image;
- +0x10–0x24: pointers, null here;
- +0x28/+0x2C: 255.0;
- +0x30: `CSpeedTreeRT+0xF0`, **token 75005 ("billboard transition / 2"), 0.15** in all 9 of `oth000d`'s trees.

`0x52E160` hands the instance's position (+0x20), yaw (+0x60, radians), scale (+0x64), branch tint (+0x30) and draw distance (+0x116) to the writer. One corner c = 0..3:

| Register | Value |
|---|---|
| v0 (POSITION) | (instance position xyz, c) |
| v1 (TEXCOORD0) | (width, height, 0, **fade**). The batch builder 0x532D6A–0x532D7D first sets .w to 1.0 in all 4 corners; per frame 0x52CE50 copies the instance's +0x70 there |
| v2 (TEXCOORD1) | (**t** = S360Billboard+0x30 = model+0xA0C, **base** = the model's first slot in `g_v360TexCoords` (model+0xFC), **N** = the image count (model+0x9E4), **yaw**) |
| v3 (TEXCOORD2) | (lighting scalars from `GetLightShaderParams`: model+0x19C × +0x1B4, +0x1B0 × +0x19C, +0x1B8, **scale**) |
| v4 (TEXCOORD3) | the instance's tint (+0x30), see section 4 |

**`g_v360TexCoords` (c0–c119)**, filled by 0x530B70: each model takes N consecutive slots from a running count (model+0xFC). Slot i = `(tbl[i][4], tbl[i][5], tbl[i][0], tbl[i][1])` from the DLL's 8-float table at S360Billboard+0x0C (stride 0x20), read as `(u_left, v_bottom, u_right, v_top)` by the corner selectors below.

#### spt_billboards0.vsh and spt_billboards1.vsh (vanilla, fxc 9.19.949.2111; checked line by line)

- **The angle:** θ = `atan2(dy, dx)` of (tree − camera) in the xy plane (`v0.xyz − g_vCameraPosition`, a polynomial atan2), plus the yaw `v2.w`, wrapped into [0, 2π).
- **The image:** `s = θ·N/2π`, `i = floor(s)`, `f = s − i`.
  - billboards1 draws image `i`, slot `base + i`, with weight `w = f`.
  - billboards0 draws image `i+1` (0 when i+1 = N), slot `base + (i+1) mod N`, with weight `w = 1 − f`.
  - Both are drawn and alpha-tested, cross-fading two neighbouring images. With N = 8 (`hydra`, `hair`) the images are 45° apart; with N = 1 (`root`, `rootb`, `sporebig`) the one image is used from every side.
- **The fade term** (the vertex shader's `o1.z`; the pixel shader writes `alpha = tex.a × o1.z` and the alpha test cuts):
  - `p = pow((max(w, t) − t) / (1 − t), 1.7)`;
  - the shader writes `A = lerp(255 − 171·v1.w, 255, p)`, then `o1.z = 1 − (A − 84)/171`, **which is `o1.z = v1.w · (1 − p)`**.
  - So an image is at full weight until its `w` passes t = 0.15 and falls to 0 at w = 1, scaled by the instance's billboard fade `v1.w`.
- **The quad:** the corner offsets are c143–c146 `.yz` = (0.5, 1), (−0.5, 1), (−0.5, 0), (0.5, 0) × (width, height) × scale (v3.w): bottom-centred, height up. They are turned by the camera's azimuth `g_vCameraAngles.x` (c138; the constant is (azimuth, pitch, 0, 0) in radians, the azimuth plus 360 when negative, filled by 0x0052D810: stores at 0x0052D84A to 0x0052D860, the last two the 0 from the `fldz` at 0x0052D822; read 2026-11-01), so the offset is `(−sin a · x, cos a · x, z)`. **Every billboard faces the camera's azimuth, not the line to its own tree.**
  - The corners' texcoords (c139–c142 selectors) are, for corner 0 (right, top), `(e.z, e.w)`; corner 1 (left, top) `(e.x, e.w)`; corner 2 (left, bottom) `(e.x, e.y)`; corner 3 (right, bottom) `(e.z, e.y)`. Here e is `g_v360TexCoords[slot]`.
- The normal map's basis per corner comes from `g_amBBTangents`/`Binormals`/`Normals` (c124–c135), indexed by the corner.

#### Where the billboard comes in within LOD 0..1

The exe's instance LOD (0x52CD60, checked; the LOD value comes from 0x52E260, `dev/TREES.md`):
- `lod ≤ 0`: the instance's +0x6C (draw the billboard) = 1 and +0x70 (the billboard fade) = 1; nothing else.
- Otherwise it calls `GetLodValues(SLodValues&, lod)` and keeps:
  - branch LOD +0x78, alpha +0xEC;
  - frond LOD +0x7C = max(frond LOD − 1, 0), alpha +0xF0;
  - leaf LODs +0x80/+0x84, alphas +0xF4/+0xF8;
  - the billboard fade +0x70 = `m_fBillboardFadeOut`, and +0x6C = (fade > 0).
- Per draw, 0x52CE80 gives the kind's LOD and `alpha / 255`, which becomes the draw's alpha-test reference (`× 255` (the double 255.0 at 0xAF6EA8) → the renderer's state 9 at 0x530D40 and 0x52DE90; that enum 9 is the alpha reference is inferred). The conversion truncates: the x87 control word is set to round toward zero (`or eax, 0xC00`; `fistp` at 0x530D60 and 0x52DEB0), so a whole-number alpha is kept and 126.5 becomes 126 (checked 2026-10-22).
- 0x530C20 and 0x52DE10 clamp the kind's level to the model's level count − 1 and skip the draw for a level below 0 (0x530C20) or of −1 or less (0x52DE10); 0x52DE10 draws the second leaf slot in a second pass (checked 2026-10-22).
- **A whole forest as billboards** (two readings reconciled 2026-11-01; the test read in the exe): `TryDrawStaticBillboards` 0x00533360 runs first in the forest's render slot 0x005370D0. It answers false at once if 0xC31664 or the forest's +0x2E is set. If +0x2D is clear (+0x2D is set only for the chunk of id −3, by `BuildBillboards(flag)`), it calls `SetStaticBillboards` 0x00533080 with whether the camera 0xC316B0 is outside the forest's box +0x4A0 (0x00533378-0x0053339E): that box is each instance's position ± sqrt(far², +0x11C) × `g_treeLodFactor` (`GrowBoundsByInstance` 0x00532160, 0x0053242D-0x00532445), so outside it means beyond every tree's far distance in Explore and Combat, where each tree would be a lone billboard anyway [I for what the player sees: no change there], which rebuilds a prebuilt buffer through `FillStaticBillboards` 0x00532C70 (four 0x50-byte vertices an instance, every fade 1.0). If +0x2D or +0x34 holds, it draws that buffer and answers true, and the render slot skips culling and `DrawModels`. So with the camera outside a forest's bounds, or in a −3 room, every tree of that forest is a billboard at full fade whatever its own level; the instance's fade below applies only with the camera inside.
- **The level itself** (0x536010, per frame and instance, checked 2026-10-22): d² = the camera's (0xC316B0) squared distance to the instance's position (+0x20); level = `1 − clamp((d² − near²)/(far² − near²), 0, 1)` with near² and far² the instance's +0x118 and +0x11C (0x52E260's). When the byte at 0xC31664 is set, every instance takes near² = far² = 2^62 instead (corrected 2026-11-02: 0x5E800000 at 0xADEBC4, which the option stores into 0xBF73F0 and 0xBF73F4 at 0x00404E3D, 0x00404E43; 400 and 3600 are only those floats' startup values), so every tree is at full detail. The byte is set only by the `-capturemovie` command-line option, and it also turns off every billboard draw: `DrawBillboards` 0x532A53 returns at once and `TryDrawStaticBillboards` 0x533360 answers false; it keeps `SetCameraAndLodFactor` from writing 0x00BF73F8, and gives `BuildTreeForests` its flag false (two readings reconciled, 2026-11-01).

**SLodValues** (0x24 bytes; ctor 0x10024030, checked): +0x00 `int` branch LOD, +0x04 `float` branch alpha, +0x08 frond LOD, +0x0C frond alpha, +0x10/+0x14 two leaf LODs, +0x18/+0x1C their alphas (ints −1, floats −1.0), +0x20 `float` billboard fade (0.0).

**`GetLodValues`** (0x10025000, checked; drop-to-billboard on, as the exe sets it):
- the tree's own values:
  - `r` = token 9003 (+0x24): 0.1667 for the trees, 0.125 for the grass;
  - `e` = token 9004 (+0x28): 1.3 for the trees, 1.5 for the grass;
  - `a` = +0x30: **a per-tree float of the .spt**, the main parse's case at 0x1002E1A1 (`fstp [esi+0x30]` at 0x1002E1AF; token 16014, 0x3E8E); the ctor's 0 and method 2's 0.5 (0x1002E426) are only defaults (corrected 2026-10-22 by lib-agent with a subagent, from paragon-agent's samples of the DLL's own GetLodValues (DAOParagonEngine #173); this said a = 0 for method 1, inferred from no other writer found);
  - the opaque alpha = +0x4C = **84** (ctor).
- **W = 2·r·(1 − a)**, each tree its own. Lothering's, fitted exactly to the DLL's samples (squared error under 1e-7 over all 18 of its trees) [I for the values]: r = 0.125 for all; oaks a = 0.2, e = 1.4, W = 0.2; bushes a = 0.15, e = 1.3, W = 0.2125; grass and flowers a = 0.25, e = 1.5, W = 0.1875. The 0.333 and 0.25 this gave for `oth000d` took a = 0; its trees' W wants their own token 16014 (corrected 2026-10-22 by lib-agent with a subagent, from paragon-agent's samples of the DLL's own GetLodValues (DAOParagonEngine #173)).
- **L ≥ 1:** branch and frond LOD 0, alpha 84.
- **W < L < 1:** alpha 84; **billboard fade 0** (no billboard).
- **Branch and frond LOD** = `int(numBranchLods·(1 − L))` (the frond's the same) for every 0 < L < 1, below W too [C] (corrected 2026-10-22 by lib-agent with a subagent, from paragon-agent's samples of the DLL's own GetLodValues (DAOParagonEngine #173); this gave it above W only).
- **0 < L ≤ W:**
  - the 3-D tree's alpha-test value `= 84 + 171·(1 − L/W)^e`: it rises from 84 (opaque) to 255 (gone) as L goes to 0;
  - **billboard fade `= min(1, 1.5·(1 − L/W))`**: it starts at L = W and is full at L = W/3.
  - So for Lothering's oaks (W = 0.2) the billboard fades in over L 0.2 → 0.067, while the tree dissolves by alpha test over L 0.2 → 0. With W from each tree's own parameters these formulas give the DLL's numbers: at L = 129/1024 the fade is 0.555 (oak), 0.611 (bush) and 0.492 (grass), the oak's alpha 126.5.
- **L ≤ 0:** branch and frond LOD −1 (not drawn), billboard fade 1.
- **Leaves** (decoded 2026-10-22 by lib-agent with a subagent, DAOParagonEngine #173) [C unless marked]: slot 2 is −1 and its alpha 255 unless set below; D = +0xD0, A = +0xD8, B = +0xE0 (+0xD4 and +0xDC with drop-to-billboard off), with the tree's a and e:
  - **L < W:** slot 1 is the finest leaf LOD (nLeaf − 1) with alpha `84 + 171·(1 − L/W)^e`, the branches' curve, and is dropped (−1) when that reaches 255 (0x100252B6–0x10025319).
  - **L ≥ W:** L' = L − D, n = int(L'/B), k = max(nLeaf − n − 1, 0), r' = L' − n·B. With r' < A the single LOD k is drawn at alpha 84. With r' ≥ A slots k − 1 (slot 1, the coarser) and k (slot 2, the finer) cross-fade over u = (r' − A)/D: slot 2's alpha is 84 until u = a, then `84 + 171·((u − a)/(1 − a))^e` (0x100253B1–0x100253F9); slot 1's is `84 + 171·(1 − u/(1 − a))^e` until u = 1 − a, then 84 (0x10025417–0x1002545A).
  - D, A and B were not traced to a writer; fitted to the samples [I]: D = 0.25, B = (1 − D)/(nLeaf − 1), A = B − D. So a tree of three leaf LODs (Lothering's bushes, oakbig) cross-fades over L 0.375–0.625 and 0.75–1, and one of two (grass) over 0.75–1, as the DLL's samples show. The same a and e fit both the near fade and these bands.
  - Token 75005 (+0xF0) is not read by GetLodValues; it reaches only the shader, through S360Billboard+0x30.

`ComputeLodCurve` (0x10024DB0) is `a + (b − a)·t^e`, or `b + (a − b)·(1 − t)^e` inverted. `ComputeLodCurveBB` (0x10024E20) is the same over `(t − F)/(1 − a)`, where F = +0xF0 (checked, but not on the path above).

### 4. The instance tint: g_vColorMultiplier

- **Where it comes from (checked):** at instance creation, 0x5CBFD5 calls 0x52E1D0 with the node record's +0x14 (`EnvTreeColorTint`). That copies the one float4 into the instance's **+0x30, +0x40 and +0x50** unchanged. There is no multiply, and the level tint is not involved.
- **Per draw (checked):** 0x530C60 calls 0x52DFA0 for each (instance, geometry kind) draw. 0x52DFA0 writes the model's per-draw constants from the instance, then picks the tint by the kind being drawn (model+0xBA4) and writes it with 0x4D4DF0 to **model+0xC5C**:
  - kind 1 (branches) → +0x30;
  - kind 2 (fronds) → +0x40;
  - kinds 4, 8, 0x100 (leaf cards, leaf meshes, billboards) → +0x50.
- The material setup binds model+0xC5C to `g_vColorMultiplier` by name (0x52FD20, 0x52FF4B, 0x5301EE, 0x53049B for the branch, frond and two leaf materials).
- The same per-draw function writes:
  - +0xD10 = instance +0x68 (`rand() % 100`) → `g_fWindMatrixOffset`;
  - +0xBC8 = scale (+0x64) → `g_fTreeScale`;
  - +0xBCC = (position, 1) → `g_vTreePos`;
  - +0xBD8 = yaw (+0x60);
  - the instance's matrix (+0x90–0xC0) → model+0xCC0.
- **How the shaders use it (vanilla `spt_branch.psh`, c14):**
  - light = `g_vColorMultiplier.rgb + max(N·L, 0.05)·g_vLightDiffuse`, times the shadow term, times the diffuse texture. **The rgb is the ambient colour, not a multiplier on the result.**
  - **A tree group also takes its nearest probe's irradiance** (render-05, read 2026-11-01; DAOParagonEngine #351; checked by lib-agent-r05 in the exe): 0x00531FA0 chooses a probe through `ChooseForPoint` (0x00464200, mask 8) when the light manager's lights-changed stamp (+0xD0) differs from the group's (+0x350), and hands the probe's three matrices (0x0053C8F0, 0x0053C900, 0x0053C910), or the default set (0x004D33C0), to 0x0052D780, which writes 12 vec4 to the globals 0x00C31740..0x00C317F0; 0x0052EA90..0x0052ED26 copies them into the SpeedTree renderer's 12-vec4 array. So a tree's ambient colour is `g_vColorMultiplier.rgb`, the tree's own tint (+0x50). The 12 vec4 do not reach a shader: each model (0x00530DB0, call 0x00530DE8) passes `0x0052EA80` its own vector at model +0xB90, built empty by the constructor (`std::vector<Vector4>::vector(0)`, 0x00535675), and the routine then writes the 12 vec4 to the shared default element 0x00C30438 (0x0052EAA8); `g_mIrradianceParams`, the name the materials bind that vector by, is in no constant table of the 30 `spt_*` and `sm2_spt_*` shaders and in no file of core `shaders.erf` (byte scan of its 532 files) [C]. DAOLib's TODO.md said the trees have no ambient term; that is corrected there.
  - `.w` weights the shadow and self-shadow blend (`mad r0.x, c14.w, …`).
- **Billboards** take the branch tint (+0x30) as the vertex's TEXCOORD3, not the constant. The pixel shader: `color = sat(tex.rgb × (diffuse·max(N·L,0)·lerp(v6.x, v6.y, g_vLightDiffuse.w)·((tint.w − 1)·0.6 + 1) + tint.rgb))` (`lrp r0.w, c4.w, v6.y, v6.x`, re-read in core `shaders.erf`). `g_vLightDiffuse.w` is set per model per frame by 0x00530DB0 at 0x00531077 to `(1 − (L.x·D.x + L.y·D.y)) / 2`, L the model's light direction (+0xBA8) and D the SpeedTree camera's direction output, neither normalised in the dot and z ignored (the three `Normalized` calls at 0x00530FEB, 0x00531028, 0x00531037 write results nothing reads); the billboard material binds the first model's value (0x005344DD..0x005344F3). `v6` is the vertex TEXCOORD2 above.
- **The trees' light** (`g_vLightDir`, `g_vLightDiffuse` rgb): the model copies the globals `SpeedTree::LightDirection` 0x00C31718 and `LightDiffuse` 0x00C31728 each frame (0x00530DC1, 0x00530DD6). The direction has two writers of `SetLightDirection` 0x0052D6D0 (it normalises xyz): `LoadTrees` (0x005C8BEB, the negated `GetSun` direction, or (0.577, 0.577, 0.577) from 0x00AFFDCC without a sun) and `ShadowMaps::Update` (0x00453A26, the negated direction at 0x00BF20F0..F8, when the float at 0x00C1E7E0 is above zero). That second writer never runs as shipped: the test at 0x004539C3..0x004539CE skips it for zero and below, and the float is in .bss with no reference in the file but that test; the direction there is a fixed (0, 0, -1) with no reference but its reads, not the live sun's (corrected 2026-11-04 by lib-agent-2); the colour has one writer, `LoadTrees` (0x005C8C26) [C].
- **`EnvTreeColorLevelTint`** (3377): read at 0x5D000F–0x5D008F into the area's tree-controller-list object at +0x90 (default (0, 0, 0, 1)). No reader of it was found in the tree draw path; it does not reach `g_vColorMultiplier` (checked by the copy above; "unused" is inferred).

### 5. The wind: CSpeedWind from oth000d_193.wnd

- **The `.wnd` is read** at 0xA57760 into a wind object (checked):
  - +0x10/+0x14 = `WndGustMinStrength` 0.05 / `WndGustMaxStrength` 0.25 (0x5BDE60);
  - +0x18/+0x1C = `WndGustMinDuration` 2 / `WndGustMaxDuration` 3 (0xA55D40);
  - +0x20 = `LvlWindSptGustFrequency` / 60 (0xA55D60; 9.96/60 if absent). **`oth000d` has 0: no gusts**;
  - strength +0x34 (0.25) and direction +0x40 ((0, 1, 0)), read by 0xA566F0;
  - `WndTreeNumWindMatrices` (8) is clamped to 1..7 and `WndTreeNumLeafAngles` to at least 1 by 0xA571F0, which stores them at +0x2C/+0x30 and then has each wind instance in [wind+0xD0] (0xA56B10) clamp them the same way and size its matrix arrays and vectors, and the response, exponents, oscillations, rocking and rustling into +0x50… (0xA55D70–0xA55DD0).
- **But the trees' CSpeedWinds don't take most of it (checked).** 0x5369C0 (called at 0x5C8ADC) builds **one fixed SpeedWind text** from constants and `Load`s it (0x536B40) into each of **5 CSpeedWinds** (a 4-byte pimpl each, at manager+0x04…+0x14). The text:

      NumWindMatricesAndLeafAngles 6 8
      WindResponseAndLimit 0.4 0.01
      MaxBendAngle 35
      BranchExponent 2
      LeafExponent 2
      GustStrengthMinMax 0.25 1
      GustDurationMinMax 2 5
      GustFrequency 0.00166667
      BranchOscillationX_LaLsHaHs 0.3 3 6 3
      BranchOscillationY_LaLsHaHs 0.1 4 8 4
      LeafRocking_LaLsHaHs 1 2 2 10
      LeafRustling_LaLsHaHs 3 1 2 20

  Then, from the `.wnd`, per wind:
  - `SetWindStrengthAndDirection(strength, dir.x, dir.y, dir.z)`;
  - `SetGusting(gustMin, max(gustMin, gustMax), frequency, minDur, max(minDur, maxDur))`.

  The exe imports no other CSpeedWind setter, so the `.wnd`'s `WndTree*` values (8 matrices, its rocking and rustling) never reach SpeedWind: for trees `LoadTrees` 0x5C89B0 reads only strength, direction and gust frequency (0xA566F0, 0xA55E00, 0xA55E20) into the text for the five DLL `CSpeedWind`s. The engine has a second wind of its own, `CSpeedTreeWind`/`WindInstance` (`LoadWind` 0x5C8830 creates it with 5 instances from the `.wnd` at area+0x53C and gives it to the scene's `SpeedTreeWorld` at +0x80; `ISceneInstance::UpdateWind` 0x537D40 advances it each frame). Its one consumer found is cloth (`ClothWind::SampleWorldWind` 0x5865F0): it draws no tree (two readings reconciled 2026-11-01; found by a scan, so not a proof of absence). **The trees use 6 wind matrices and 8 leaf angles**, and `SetNumWindMatrices(GetNumWindMatrices())` = 6 is called before `LoadTree` (0x531C1E). A sixth CSpeedWind (manager+0x18) gets the same text with fixed `SetGusting(0.5, 0.8, 0.8, 0.3, 0.6)` and `SetWindStrengthAndDirection(0.8, 1, 0, 0)` (0x536C4C–0x536CD4; its use not traced).
- **Each frame** (0x52ED90; dt is the argument):
  - manager+0x1C += dt;
  - for each of the 5 winds k: `Advance(phase[k] + time, true, false, camDir.x, camDir.y, camDir.z)` (0x52EE41). phase[k] is the vector at manager+0x1C8: **k × 0.2, so 0, 0.2, 0.4, 0.6, 0.8** (the double at 0xAF7608, filled in the manager's setup at 0x005367E3–0x00536812; checked 2026-10-19 by lib-agent-2). The time is manager+0x1C, from 0, plus each frame's dt (0x0052EDB2). Gusting is inside `Advance`;
  - camDir is the live scene camera's, from `CSpeedTreeRT::GetCamera` (0x0052EDDD). Its one `SetCamera` call is at 0x0052EA69, at the end of the per-frame camera update 0x0052E8F0, only while `[0x00C31660]` > 0 (not identified). The position is the camera's own (its vtable +0x38). The direction is the row at +0x20 of the matrix its vtable +0x8C returns, which axis of the camera that is not checked: a port should take those same three floats (checked in outline, 2026-10-19 by lib-agent-2);
  - `GetWindMatrix(m)` for m < `GetNumWindMatrices()` (6) into the wind's vector at manager+(k+2)·16+4, **padded with identity to 7 matrices**;
  - `GetRockAngles`/`GetRustleAngles` (degrees) into vectors at (k+12)·16 and (k+7)·16, zero-padded to 20;
  - packed as **20 float4s `(rock·π/180, rustle·π/180, 0, 0)`** at (k+0x11)·16;
  - also a float4 from a phase advanced by `min(1, GetFinalStrength())·40·dt` (manager+0x160+4k) through sin/cos × 0.1/0.075/0.05 and 0.0015 into manager+0x174+16k (inferred: a sway term; its constant not traced).
- **Into the shaders** (checked from the listings; the binding inferred):
  - the engine semantics `SpeedTreeWindMatrices` (id 0x1C, 0x509C40) and `SpeedTreeWindLeafAngles` (0x509C4C) feed `g_amWindMatrices[7]` (c0–c27) and `g_vLeafAngles[20]` (c28–c47);
  - **an instance's wind is k = (the counter at 0x00C31868)++ % 5**, kept at instance+0x128 by the constructor (0x00531D00, the `% 5` at 0x00531DEE–0x00531DFF): round robin in the order instances are made, across every area of the session, the counter never reset. The providers read it: 0x0052ED30 the leaf angles at manager+(k+0x11)·16, 0x00534E80 the matrices at manager+(k+2)·16 (checked 2026-10-19 by lib-agent-2). 0x00534E80 takes a CSpeedWind of the instance's own at +4 instead when there is one; the constructor zeroes it and no store into it was found, so inferred: never;
  - **corrected 2026-10-19:** the call at 0x5CC005 is not the wind. It is the instance's slot 4 (0x00531E90), `SpeedTreeInstance::AttachToPhysicsScene`, not a render model (corrected 2026-11-01, two readings reconciled): given the area's `Physics::PhysicsScene` (`[ISceneInstance+0xCC]`, read by 0x537D60), it returns false for null or when the scene's byte +0x18B is set; otherwise it asks the scene (slot 1) for a body at the instance's position, releases the old body at +0xE0 and actor at +0xE4, stores the new body and calls `CreatePhysicsActor` 0x005313E0, which builds the actor from the `.spt`'s collision objects (0x0090A49A, 0x0090A494). It is skipped when the room's +0x14 is −3. The manager's sixth wind (+0x18) is still read by nothing found.
- **In the vertex shaders** (`spt_leafcards.vsh`, the same in branch, frond and mesh):
  - matrix row = `4·fmod(idx + g_fWindMatrixOffset, 6)` (c60.w = 6). `g_fWindMatrixOffset` is the instance's `rand() % 100`, so each instance uses the same 6 matrices, rotated. **That offset does not vary from tree to tree** [C, read, not seen in play]: every constructor (0x00531D00) calls `srand(time(NULL))` (0x00531E33-0x00531E3E, seconds) and draws once for a `% 10` value at +0x68 that is never used, and `BuildTreeForests` overwrites +0x68 with the next `rand() % 100` (0x005CBFB7-0x005CBFD2), on the same thread with no `rand` between (the reconciliation's check). So the value is always the second output of the generator seeded with that second: one value for all instances made in one second, in practice one for an area, two if the loop crosses a second, and another on each load. What differs between trees is the wind index k = counter % 5. A port decides whether to copy this or draw a true per-instance random;
  - position = `lerp(p, M[idx0]·p, weight0)`, then `lerp(that, M[idx1]·that, weight1)`, with (idx0, weight0, idx1, weight1) from the wind float4 (sections 1–2);
  - a leaf card's rock and rustle = `g_vLeafAngleScalars.xy × g_vLeafAngles[leaf angle index].xy`.

Core reads the `.wnd` as `dao/core/Wind.h` (`wind::WindOf`: every field of the file, 115 of the game's read 2026-10-18, with only the strength, direction and gusts marked as the ones that reach the engine's wind; DAOLib #196).

## The trees under the area's cutoff (the roof fade) (read 2026-10-22 in the GOG exe by paragon-agent's helper, for DAOParagonEngine #151)

The roof fade of CAMERA.md ("The roof fade", "The cutoff") hands a value to 0x00BF73FC; these are the readings of how the forests are built and drawn under it. [C] read in the code or the files, [I] inferred.

- **Two forests per chunk, not per level** [C, corrected 2026-11-02]: `EC::CExteriorChunk::BuildTreeForests` 0x005CBDE0 is the only maker of `SpeedTreeForest` (constructor 0x00535BB0, the bool argument stored at +0x2C). `LoadStatic` 0x005CA590 calls it, in an area with layout +0x7C == 1, for the chunk of id −3 (`LoadLayoutChunk` 0x005C82C0) and each terrain chunk of the layout's list +0x424; a chunk is one a terrain cell (COMBAT.md). Whether an interior area has another route for trees was not read. A chunk holds:
  - +0x104 with +0x2C = 0;
  - +0x108 with +0x2C = 1.
  - It registers both with the world's cutoff, (area+0x4E0)+0xD4, through 0x004678F0 [C].
- **Which forest a tree goes to** [C]: at 0x005CC007–0x005CC023, a tree whose `CSpeedTreeRT::GetNumCollisionObjects` (0x0090A49A, the DLL's) is above 0 goes to +0x104; the others to +0x108.
- **The model's flags** [C]: `SpeedTreeModel::Compute` 0x00533DA0 sets +0x1BC bit 0x40000000 at 0x00533F0E on the same condition, GetNumCollisionObjects non-zero. It sets +0xD58 (0x00533F42) when the model's name, +0x1C8, contains `_bsh_` (0x00AF74A8) or `_gra_` (0x00AF749C), the bushes and grass, and every billboard path skips such a model (`DrawBillboards` 0x00532AAA, 0x00532B60, `FillStaticBillboards` 0x00532C9A, 0x00532CFE, `BuildBillboards` 0x005342A1).
- **The forest's draw skips those trees** [C]: `SpeedTreeForest::vfunction6` (0x005370D0) calls 0x005333D0. Below 1 (0x00BF73FC < 1.0), unless the forest's +0x2C is set, 0x005333D0 skips all four of its passes: 0x00533140 with kinds 4, 8, 2, 1, each through 0x00530C20.
  - Either way it then calls 0x00532A50(…, 1), the billboard batch [C]. 0x00532A50 writes four corners per tree through 0x0052CE50, the billboard vertex of section 3 [C].
  - So the trees with collision objects lose their branches, fronds and leaves while the cutoff is below 1. Their billboards (the far level of detail) and the trees without collision objects are still drawn [I: the reading of the batch as the billboards is by 0x0052CE50].
- **The hidden trees are collected** [C]: the forest's cull, 0x00536200, takes a flag that is true when 0x00BF73FC < 1 and the forest's +0x2C is clear. Under it, each visible tree is pushed onto the global list 0x00C316F8 (0x00536332–0x00536347).
- **They are drawn by a pass of their own** [C]: `Renderer::SOSMRenderFadedSpeedTrees` (vtable 0x00AE8280, slot 0x0046F590) calls 0x00534CF0.
  - 0x00534CF0 draws each tree of the list with kinds 1, 2, 4 and 8 (materials +0xD28, +0xD2C, +0xD30, +0xD34). Before each, it calls `Graphics::SetMaterialOpacity(2, −1.0)`, which only marks the parameter dirty, and afterwards clears the material's +0xD0 [C].
  - SOSM is not the shadow map (corrected 2026-11-02, this said so, and that the hidden trees therefore cast shadows): SOSM names the family of submission operations (`ISubmissionOperation::Create` 0x00471A60). `SOSMRenderFadedSpeedTrees` is the program's `speedtree renderAllFadedTrees` (the string at 0xAE85C4, `ParseSpeedTree` 0x004736E7), in the group `FadingSpeedTrees` of `dadefaultsub.sub.xml`, after the water and before transparent geometry, so it draws the hidden trees in the main colour pass. The shadow operations are the `Shadows ...` lines.
  - **Trees do not draw into the shadow maps** (read 2026-11-02; [C] where lib-agent read it, the rest [C by the reading]). A shadow group's `ShadowCaster N` tests the partition bits at a node's +4, set only by `MarkNodeInPartition` 0x00454D70 and `MarkObjectInPartition` 0x00454BD0, which need the node's vtable slot +0x20 true: for `SpeedTreeForest` (vtable 0x00AF740C) that slot is `ReturnFalse` 0x005C5E80 [C]; the direct-node route also needs slot +0x84, there 0x004137B0, and the forest's attribute word +0x24 starts as `unsupportedcaster` 0x20000 [C, 0x00535C04], which the shadow groups exclude. And inside a partition `SpeedTreeForest::vfunction6` 0x005370D0 draws no models: `ShadowMaps::StartPartition` 0x00453440 sets 0xC31665, tested at 0x00537113 [C].
  - What is left of tree shadows is dead code: `CShadowCasterCuller` and its case for the forest's type 0xB (0x00A24E80) are never built, since their factory 0x00A24220 is called only from the forest's constructor, with 0, making the generic culler [C, 0x00535E2C-0x00535E37, stored at +0x100]; 0xC31690, the culler a partition would use (0x00536454), is never written; the PSSM slots +0xD14, +0xD18, +0xD1C are read by nothing in .text, and +0x1BC bit 0x20000000, which `DrawInstance` needs to keep a semantic override, is set by nothing, both by byte scans of .text, which a computed store could escape. The `pssmtree*` shaders and `SPT_Branch.mat.xml`'s `PSSM` semantic ship unused. Trees do receive shadows (its default semantic binds the shadow textures at SM3, detail MEDIUM). The `DepthWrite` semantics (+0xD20, +0xD24) are what `DrawInstance` reads, only in Umbra's depth pass (0xC1EBF4).
  - So crown shadows seen on the ground in Lothering's tactical view are not the trees in the sun's maps. Where they come from is not settled (corrected 2026-11-02: this said baked into the terrain, on a misreading of the branch vertex's layer 7 above, which is the `.spt`'s). A terrain chunk (`hrt000d_14_0`, `terrain.mat` HighLod; the far chunk `_1`, BakedLowLod) has no shadow, decal or canopy channel [C by the reading: its vertex is 40 bytes, POSITION float3, NORMAL, BINORMAL and TANGENT float16x4, and one TEXCOORD0 float16x2]. Its only baked light is the room's lightmap atlas (`hrt000d_6_5_000.dds`, the patch by the `.rml`'s offset and scale), at TEXCOORD0 × `mml_vLightmapOffsetAndScale`.zw + .xy: the rgb is added to the lighting and the alpha multiplies the live sun term. It also takes the ground image (palette and masks; `LowLodMap` far away). [I] The dark shapes come from the lightmap or the baked ground: trees line up with the dark of room 6_5's alpha (mean 124 at trees, 141 overall, a dip 4 m downsun) but not in four other rooms, and no crown shape shows in the rgb. Not seen in play. With the trees hidden, a screenshot before and after `ShadowMaps::SetEnabled(false)` 0x00451800 would say whether they are the maps at all.
- **The tree model's alpha multiplier** [C]: each update, 0x0052CC60 (called from 0x00530DB0) runs on a model with +0x1BC bit 0x40000000.
  - It sets +0xACC = max(0x00BF73FC, 0.12). +0xACC is bound to `g_fAlphaMultiplier` on the four materials, e.g. 0x0052FCA2 with the name at 0x00AF72F4 [C].
  - Below 1 it adds the render class "transparent" to the branch material (+0xD28); at 1 or more it removes it. "transparent" is bit 1 in 0x004F6660's name table; 0x004F6AE0 and 0x004F6B00 set and clear it at +0x30/+0x34 [C].
  - Which of the faded pass's draws use that class and multiplier was not read.
  - SPT_Branch.sta has no blending and no alpha test [C, the file]. spt_billboards0.psh does not read g_fAlphaMultiplier [C, the listing]. spt_fronds.psh writes `texture alpha × g_fAlphaMultiplier` (c13) as its alpha [C, the listing].
- **0x00530C20's test of 0x00C1EBF4** [C]: 0x00C1EBF4 is set by `EclipseUmbraCommander` command 0xA0 and cleared by 0xA1 (0x0045CB53, 0x0045CCA7). While it is set and 0x00BF73FC < 1, 0x00530C20 draws nothing.
  - [I] That is Umbra's occlusion depth pass: the trees stop occluding while the cutoff is below 1, with no visible effect.
- **Seen in play** (2026-10-22, through Eluvian, the explore controller's 5.5 m height): in Lothering's tactical view (lot100ar_lothering, around (299, 259)), the trees' crown shadows are drawn on the square and no tree is drawn.
