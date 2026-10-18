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
