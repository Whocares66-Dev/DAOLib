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
// A level's own layout, <level>.arl in packages/core/env/ or an add-in's
// core/env/, is GFF4 "ARL V4.0": its rooms, its sun, sky dome, atmosphere,
// clouds and frame buffer effects, trees, wind, terrain chunks, minimap, and
// its walkmesh grid (EnvAreaPathfindExport, not read here). Read 2026-10-17
// over all 213 the game takes (DAOLib #154): every one has the rooms, the
// sun, the atmosphere and the clouds; the atmosphere's later fields (fog on
// water, the moon) are missing from 7, the chunks from all but the 80
// terrain levels, the cut-off's switch from 15. One (orz703d) has an older fog
// of its own (EnvFog*), not read. An effect's parameter value is a generic
// field, always a float in these files. The effects used: da_bloom (208
// levels), da_hsvmatrix (201), da_fade (13), da_dof (1); the 216 empty
// entries beside them are left out.
//
// A room's own layout, <level>_<room>_ll.mmh in the room's .rim, is a model
// like the rest whose nodes are the room's lights. Read 2026-10-17 over all
// 2,581 layouts the game takes (DAOLib #152): each is a node "GOB" at the
// origin, unturned, with as children its point lights ("nplt", 10,673 in
// all), at most one ambient light ("nalt", in 1,083) and its light probes
// ("nlpr", up to seven, in 918), each probe with a reflection cube ("nrfl")
// and its irradiance ("nirr"). A light's place and turn are children of it,
// as a mesh node's are. No layout has a spot light. Every lamp is static,
// can be occluded 0 and affects domain 2; 1,654 flicker at a period of
// 0.075 s give or take 0.025. 26 probes' irradiance is all 0 in the file
// (brc201d's seven, orz703d's eleven, lak519d's one, and test levels':
// combat2, rmtst01, rmtst03).
//
// A room's _lc.mmh (1,448 of them, beside its _ll.mmh) has the same shape and
// LayoutOf reads it the same way: a "GOB" with point lights (3,766) and in a
// few a probe (15), no ambient light. Its lamps are all static and affect
// domain 1, not 2 (amn100d_0_1_0_lc.mmh's light20: radius 8). Read
// 2026-10-17 over all of them; which domain lights what is not read in the
// exe.
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/RayCast.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
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
    // The texture's coordinates, one a vertex: the first set, where the chunk's material paints
    // it; none if the chunk has no texture coordinates, keeps its first set as other than floats
    // or half floats, or its bytes stop short of them. A chunk with one set has it as both this
    // and lightUv.
    std::vector<Uv> paintUv;
    std::vector<std::uint32_t> indices; // three a triangle, into the three above
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

using Colour = std::array<float, 3>; // linear red, green, blue; a lamp's may be above 1

// A point light of a room's layout.
struct Lamp
{
    std::wstring name;
    Point at{}; // in the layout's space
    Colour colour{};
    float radius = 0; // metres
    bool isStatic = false;
    // Its flicker as the file has it: the engine's RandomWaveController takes these three,
    // a light with a variation of 0 steady.
    float variation = 0;
    float period = 0;      // seconds
    float periodDelta = 0; // seconds, the period's spread
    std::uint8_t affectDomain = 0;
    bool canBeOccluded = false;
    bool useVariationTint = false;
    std::uint8_t version = 0;
};

// The ambient light of a room's layout.
struct Ambient
{
    std::wstring name;
    Point at{};
    Colour colour{};
    bool useVariationTint = false;
};

// A light probe of a room's layout.
struct Probe
{
    std::wstring name;
    Point at{};
    std::wstring cube;           // its reflection cube's file, such as lak303d_probe_2zz.dds
    std::wstring irradianceFile; // the .mtx its irradiance was exported to
    // Its irradiance: a 4x4 matrix each for red, green and blue, as dao/core/LightProbes.h's
    // Irradiance keeps them; all 0 if the probe has none, or was never baked.
    std::array<float, 48> irradiance{};
};

// The lights of a room's layout (its _ll.mmh).
struct Layout
{
    std::vector<Lamp> lamps;
    std::optional<Ambient> ambient; // the first, if it has more
    std::vector<Probe> probes;
};

// Returns the lights of a room's layout, each at its place in the layout's space.
Layout LayoutOf(const gff4::File &layout);

// A room of a level, as its layout lists it.
struct LevelRoom
{
    std::int32_t id = 0;
    std::wstring file; // its definition's name, with no extension: lak303d_3672
    Point lower{};     // its bounds' corners, in the level's space
    Point upper{};
};

// A frame buffer effect a level turns on (da_bloom, da_hsvmatrix, da_fade, da_dof), with the
// values it gives the effect's parameters.
struct FrameEffect
{
    std::wstring file;
    std::wstring name;
    std::vector<std::pair<std::wstring, float>> parameters; // "Brightness", 1.0
};

// A value of the atmosphere or the clouds: its field's name ("AtmoSunIntensity") and its floats.
struct NamedFloats
{
    std::string name;
    std::vector<float> floats;
};

struct LocalWind
{
    Point at{};
    std::wstring file; // a .wnd
    float radius = 0;
};

// A terrain level's chunks: a grid of rows by columns, each width by height metres.
struct Chunks
{
    std::int32_t rows = 0;
    std::int32_t columns = 0;
    float width = 0;
    float height = 0;
};

// A level's layout (.arl) but its walkmesh grid, fields as written; a field some files lack is
// optional.
struct LevelLayout
{
    std::int32_t id = 0;
    std::wstring name;       // "Circle Tower (Mage Origin) - Level 1"
    std::wstring layoutName; // "lak303d"
    std::vector<LevelRoom> rooms;
    Point center{};
    std::wstring skyDome; // a model: sb_day, sb_night, sb_newlayout; empty for none
    std::vector<FrameEffect> effects;
    Point sunDirection{};
    std::array<float, 4> sunColour{}; // all 0 where the level has no sun
    std::optional<std::array<float, 4>> sunColourOnCharacters;
    std::optional<bool> sunOccludedOnCharacters;
    bool forceCharacterLighting = false;
    std::vector<NamedFloats> atmosphere; // the sun's scattering, fog and moon, in file order
    std::vector<NamedFloats> clouds;
    std::vector<std::wstring> trees; // SpeedTree files
    std::array<float, 4> treeTint{1, 1, 1, 1};
    std::wstring globalWind; // a .wnd; empty for none
    std::vector<LocalWind> localWinds;
    std::optional<Chunks> chunks; // a terrain level's
    std::wstring startPoint;
    std::optional<std::int32_t> cutoffHeight;
    std::optional<bool> cutoffEnabled;
    Point minimapLower{};
    Point minimapUpper{};
    std::vector<std::vector<std::uint32_t>> lightSubsets; // as written; not yet read in the exe
};

// Returns a level's layout, or nothing for a file that is not one.
std::optional<LevelLayout> LevelLayoutOf(const gff4::File &layout);

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
