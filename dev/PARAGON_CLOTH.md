# Cloth: what Paragon needs from core (paragon-agent helper, 2026-10-24, DAOParagonEngine #265 item 3)

Banners and flags are missing in Paragon (Redcliffe's castle gate: `prp_bannerred_04_0`, 8 placed in lak200d; `prp_flagchantry_01_0`, 17 placed). Their cloth is not a mesh node: it is a node of kind `nclt` in the `.mmh`, with its mesh inline in the hierarchy, and core's `Model` reader (`Walk` in `Model.cpp`) does not look at it (it makes the node, with no `mesh`, since `nclt` has no `MMHMeshGroupName`).

## What is in the file (read with ghilana `resource prp_bannerred_04_0.mmh`, `.phy`, `model`)

`prp_bannerred_04_0.mmh` (GOG core, `patch002.erf` copy, 8178 bytes): `GOB` with children `xprt` x2, `bbox`, `trsl`, `rota`, then the `nclt` node `Plane28`, then three `mshh` (the pole and rope, which core reads). The `bbox` is the pole's only (z 0.02 to 0.18), the cloth is outside it.

The `nclt` struct (labels from `dev/gff4_fields.csv`):

- `MMHName` 6000 = "Plane28", `MMHMaterialObject` 6001 = "Banners" (a `.mao`), `MMHID` 6004 = "_37m", the collision masks and `MMHMeshCastRuntimeShadow` 0.
- The cloth's parameters, 6195 to 6224: `MMHClothThickness` 0.1, `Density` 1, `BendingStiffness` 0.1, `StretchingStiffness` 1, `DampingCoefficient` 0.001, `Friction` 0, `Pressure` 1, `TearFactor` 1.5, `CollisionResponseCoefficient` 0.2, `AttachmentResponseCoefficient` 0.2, `AttachmentTearFactor` 1.5, `SolverIterations` 5, `ExternalAcceleration` (0,0,0), `WakeUpCounter` 10, `SleepLinearVelocity` 0.01, and the flags (`Gravity`, `Bending`, `Damping` 1, `DisableCollision` 1, the rest 0). These are the fields of PhysX 2.x's `NxClothDesc` (the exe stores them through `NXU::NxClothDesc::load` 0x008B0AF0 / `store` 0x008AB980).
- `MMHClothCookedDataStream` 6232: a list of bytes, a PhysX 2.x cooked cloth mesh (`NXS` `CLTH` header, version 3), 1,000 bytes or so; it holds the rest vertices and the topology. The exe cooks with `NxCookClothMesh` (`Physics::PhysicsManager::CookClothMesh` 0x008196F0) and `PhysicsObject::CreateCloth` 0x00839F10 makes the `NxCloth` from it.
- `MMHClothMeshGroupStruct` 6233: a struct of a mesh group with every field at its null (0xFFFFFFFF): unused in this file.
- children: `attr` (BaseLight), `trsl` (0.0207, 0.0989, -1.9728), `rota` (identity), `catl` holding an `att` (`MMHClothAttachmentType` 6225 = 1, two-way flag 1, shape name "", vertex id 0xFFFFFFFF), and an `msgr` struct.
- the `msgr` struct (a mesh group, the labels of a `.msh` chunk: `MeshChunkVertexSize` 8000 = 72, `VertexCount` 8001 = 16, `IndexCount` 8002 = 42, `PrimitiveType` 8003 = 0, `IndexFormat` 8004 = 1, `MeshChunkBounds`, `MeshVertexData` and `MeshIndexData` as byte lists, and `MeshChunkVertexDeclarator` 8025: Float4 position at 0, Float2 texcoord at 16, tangent at 24, binormal at 40, normal at 56, all Float4 and not half floats as the `.msh` chunks of this model are). The vertices are the cloth at rest, in the node's own frame (here x about 0, y -0.607 to 0.569, z -2.387 to 2.081; with the node's `trsl` z -1.97 the top edge lies at z 0.11, under the pole at z 0.10 to 0.13 and the banner hangs 4.4 down). 16 vertices, 14 triangles, the indices 32-bit.

The model's `.phy` (`prp_bannerred_04_0.phy`, GFF4 `phy`) has only a capsule collision shape on `GOB`; the cloth is not in it.

`prp_flagchantry_01_0.mmh` has the same: `Plane47` of kind `nclt` beside two `mshh`. Creature capes and the like are likely the same kind (not checked).

## What Paragon asks of core

1. In `Hierarchy`/`Node`: an `nclt` node's cloth, as `WeaponTrail` is for `wtrl`: `std::optional<Cloth>` with the material (6001), the id (6004), the parameters above (the `NxClothDesc` ones), the attachments (`catl` children `att`), and the rest mesh as a `Chunk` with its `Element`s (the declaration is in the struct, the vertex bytes in `MeshVertexData` 8022-like labels; the `msgr` is read like a `.msh` chunk, so the existing `Floats` and `Indices` should serve if the chunk can be made from a struct and not from a mesh file).
2. A word on whether the cooked stream (6232) is needed at all for a static draw: it is not (the rest pose is the `msgr`); it is the sim's. No reader of it is asked for.
3. Not asked yet: the wind (`dev` says `Wind` has the cloth response and strength from a level's `.wnd`; the nodes themselves carry `MMHClothWind*` 6257 to 6276 in other models, absent in this one).

## The exe, as far as read (GOG 1.05)

- The scene node is `GraphicsStructures::ObjectNodeCloth` (constructor 0x00564xxx, vftable 0x00AF9C04; its size 0x5C0 bytes) made by `GraphicsStructures::ObjectNodeClothBlueprint` (vftable 0x00AFEA6C); the blueprint's instantiate is `vfunction3` 0x005B36C0: it makes the node, passes the blueprint's cloth description (the `+0x64` struct, floats at +0x70 to +0xD6) and the mesh prototype (`+0x68`, a `Physics::ClothMeshPrototype`, vftable 0x00AFC09C) and then makes the blueprint's children. Read from the decompilation; the functions behind the setters (0x00586400 to 0x00586590) are not read.
- Not read: how the simulated vertices reach the draw each frame, the material choice for an `nclt` node ("Banners" is a `.mao` of the model's own; what shader it names is for Paragon to look up), and whether the node draws before the sim has run in the rest pose. Marked inferred in Paragon: the rest pose is what is drawn at first.

Paragon will draw the rest pose through the material's family once core gives the cloth, and no simulation; a later step may sway it by the level's wind (inferred until read).
