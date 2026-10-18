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
  - `rand() % 100` (inferred: a wind phase);
  - the tint;
  - the world's wind, unless the room's +0x14 is −3.
- **Level of detail** (0x0052E260, 0x0052CD60): `s = clamp(size × scale, 2, 30)`, `t = (s − 2)/28`; near = lerp(15, 20, t) and far = lerp(35, 120, t), each × {1, 1.5, 2, 0.001 or 0.004}[draw distance] (0x00BF73E0). lod = 1 − clamp((d² − near²)/(far² − near²), 0, 1), the billboard alone at 0.
- **The wind** is the `.arl`'s `EnvAreaGlobalwindResRef` (`oth000d_193.wnd`, a GFF4 `WND`: strength 0.25, direction (0, 1, 0), gusts, oscillation and leaf rocking), given to SpeedTree's wind (the mapping inferred).
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
- **The exe's calls:** `LoadTree` from memory (0x00531C56); `SetTreeSize` from its +0x1C0, +0x1C4; `Compute(NULL, 1, true)` (0x00533E19–0x00533E21, checked). The engine's tree class derives from `CSpeedTreeRT` (its constructor on `this` at 0x005355DC), so the class is at most 0x1C0 bytes (inferred).
- **`SetTreeSize(−1, −1)` is most likely refused,** keeping the file's own size: the DLL says "SetTreeSize() is only valid for size values greater than 0.0" (inferred).
- **The exports a baker needs:**
  - `??0CSpeedTreeRT@@QAE@XZ`, `??2CSpeedTreeRT@@SAPAXI@Z`, `??3CSpeedTreeRT@@SAXPAX@Z`;
  - `?LoadTree@CSpeedTreeRT@@QAE_NPBEI@Z` (from memory), `?LoadTree@CSpeedTreeRT@@QAE_NPBD@Z` (from a file);
  - `?SetTreeSize@CSpeedTreeRT@@QAEXMM@Z`, `?Compute@CSpeedTreeRT@@QAE_NPBMI_N@Z`;
  - `??0SGeometry@CSpeedTreeRT@@QAE@XZ`, `?GetGeometry@CSpeedTreeRT@@QAEXAAUSGeometry@1@I@Z`;
  - `GetNum{Branch,Frond,Leaf}LodLevels`, `GetBoundingBox`, `GetNumCollisionObjects`/`GetCollisionObject`, `GetMapBank`, `GetLeafBillboardTable`, `UpdateBillboardGeometry`.
- `SGeometry`'s layout has no public header: it is to be read from the exe's `GetGeometry` callers or the DLL (not read).
- **So the plain route** is a small x86 baker on the player's machine. It loads the game's own DLL and, for each `.spt`, runs `LoadTree`, `SetTreeSize(−1, −1)`, `Compute(NULL, 1, true)` and `GetGeometry` per level of detail, and writes a cache the port reads. With the seed fixed and the size the file's, one bake per file should be exact, instances differing only by their transform (inferred: not yet run).
