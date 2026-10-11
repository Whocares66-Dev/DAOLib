// A level's geometry out of the game's files, over bytes: a room's
// definition, the models placed in it, their meshes, and the triangles of
// it all in the level's space with their place in the room's lightmaps.
// Read 2026-10-11 with tools/gff4.py and checked by drawing three rooms of
// the mage origin's Circle from above by their lightmaps alone: the pillars'
// shadows fall where the pillars stand (DAOLib #65).
//
// All three files are GFF4 (dao/core/Gff4.h; dev/gff4_fields.csv names the
// fields):
//
//   <level>_<room>.rml, in <level>.rim: the room's place and turn, the rooms
//     seen from it, and its models: each a place, a turn, a scale, the
//     model's name, and for each lightmapped part of it the atlas it is in
//     and an offset and scale into that atlas.
//   <model>.mmh, in modelhierarchies.erf: a tree of nodes; a mesh node names
//     the mesh's chunk it draws and a part id the room's list goes by, and
//     has its translation and rotation as children.
//   <model>.msh, in modelmeshdata.erf: the vertices and indices of every
//     chunk, each chunk with a Direct3D vertex declaration. Positions are
//     three or four floats or half floats; the second set of texture
//     coordinates is the lightmap's.
//
// A room's own layout, <level>_<room>_ll.mmh in the room's .rim, is a model
// like the rest whose nodes are the room's lamps and its light probe.
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/RayCast.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace dao::core::level
{

using Quaternion = std::array<float, 4>; // x, y, z, w
using Uv = std::array<float, 2>;

// Returns a point turned by a quaternion of length 1.
Point Turned(const Quaternion &turn, const Point &point);
// Returns the turn that is `inner` and then `outer`.
Quaternion Then(const Quaternion &inner, const Quaternion &outer);

// One chunk of a mesh file.
struct MeshChunk
{
    std::wstring name;
    std::vector<Point> positions;
    // The lightmap's coordinates, one a vertex: the second set of texture coordinates, or the
    // only one; none if the chunk has no texture coordinates.
    std::vector<Uv> lightUv;
    std::vector<std::uint32_t> indices; // three a triangle, into the two above
};

// Returns the chunks of a mesh file (.msh). A chunk whose vertices or
// indices run out of the file is left out.
std::vector<MeshChunk> Meshes(const gff4::File &mesh);

// A mesh node of a model: the chunk it draws and where, in the model's space.
struct MeshNode
{
    std::wstring chunk;
    std::wstring part; // what a room's lightmap list knows it by
    Point at{};
    Quaternion turn{0, 0, 0, 1};
};

struct Model
{
    std::wstring meshFile; // the .msh its chunks are in, with no extension
    std::vector<MeshNode> nodes;
};

// Returns a model's mesh nodes (.mmh).
Model ModelOf(const gff4::File &hierarchy);

// Returns where the light probe of a room's layout (its _ll.mmh) is, in the
// layout's space, or nothing if it has none.
std::optional<Point> ProbeOf(const gff4::File &layout);

// A lightmapped part of a model placed in a room.
struct LightmapPart
{
    std::wstring part;
    std::wstring atlas;                           // the texture's file, such as lak303d_Room369900.dds
    std::array<float, 4> offsetScale{0, 0, 1, 1}; // u and v offset, u and v scale
};

struct Placed
{
    std::wstring file; // the model's name, with no extension
    Point at{};
    Quaternion turn{0, 0, 0, 1};
    float scale = 1;
    std::vector<LightmapPart> parts;
};

struct Room
{
    Point at{};
    Quaternion turn{0, 0, 0, 1};
    std::vector<std::uint32_t> sees; // the numbers of the rooms seen from it
    std::vector<Placed> models;
};

// Returns a room's definition (.rml), or nothing for a file that is not one.
std::optional<Room> RoomOf(const gff4::File &definition);

// Returns a point of a placed model's space in the level's.
Point InLevel(const Room &room, const Placed &placed, const Point &point);

// A triangle of a level with where it is in a lightmap.
struct LitTriangle
{
    Triangle corners;
    std::array<Uv, 3> uv{}; // in the atlas, 0 to 1
    std::uint32_t atlas = 0;
};

// Adds the lightmapped triangles of one model placed in a room, in the
// level's space.
// @param chunks the model's mesh file's
// @param atlasOf gives the number a triangle keeps for an atlas's file name
void AddTriangles(const Room &room, const Placed &placed, const Model &model, std::span<const MeshChunk> chunks,
                  const std::function<std::uint32_t(const std::wstring &)> &atlasOf, std::vector<LitTriangle> &out);

} // namespace dao::core::level
