#include "dao/core/Gff4.h"
#include "dao/core/Level.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using Catch::Approx;
using dao::core::Point;
namespace gff4 = dao::core::gff4;
namespace level = dao::core::level;
using dao::test::Append;
using dao::test::Builder;
using dao::test::Bytes;
using dao::test::Children;
using dao::test::Float;
using dao::test::Floats;
using dao::test::kGeneric;
using dao::test::Start;
using dao::test::Struct;

namespace
{

// The files here are laid out as the game's are (tools/gff4.py --templates
// on a .msh, a .mmh and a .rml), with the fields the reader takes and no
// others.

// A mesh of one chunk, "Slab": a triangle with corners (0,0,0), (2,0,0) and
// (0,2,0), each vertex its place as three floats, a first texture
// coordinate, and the lightmap's as two half floats: (0,0), (1,0), (0,1). With
// `sets` 1 the chunk declares the first coordinates only, (0.5, 0.5) at every
// vertex, as a terrain's ground patch does.
std::vector<std::byte> MeshFile(std::uint32_t sets = 2, std::uint32_t firstType = 1, std::uint32_t firstAt = 12)
{
    Builder b = Start(
        "MESH", {{"mesh",
                  24,
                  {{2, 14, 0, 0},
                   {8021, 2, gff4::kList | gff4::kStruct | gff4::kReference, 8},
                   {8022, 0, gff4::kList, 12},
                   {8023, 0, gff4::kList, 16}}},
                 {"decl", 24, {{8026, 5, 0, 0}, {8027, 5, 0, 4}, {8028, 4, 0, 8}, {8029, 4, 0, 12}, {8030, 4, 0, 16}}},
                 {"chnk",
                  112,
                  {{2, 14, 0, 48},
                   {8000, 4, 0, 52},
                   {8001, 4, 0, 56},
                   {8002, 4, 0, 60},
                   {8005, 4, 0, 72},
                   {8006, 4, 0, 76},
                   {8009, 4, 0, 88},
                   {8025, 1, gff4::kList | gff4::kStruct, 96}}}});
    const std::size_t root = Struct(b, 24);
    const std::size_t chunk = Struct(b, 112);
    std::vector<std::uint8_t> vertices;
    const std::uint16_t zero = 0x0000;
    const std::uint16_t one = 0x3C00; // 1.0 as a half float
    const auto vertex = [&](float x, float y, std::uint16_t u, std::uint16_t v) {
        Append(vertices, x);
        Append(vertices, y);
        Append(vertices, 0.0F);
        Append(vertices, 0.5F);
        Append(vertices, 0.5F);
        Append(vertices, u);
        Append(vertices, v);
    };
    vertex(0, 0, zero, zero);
    vertex(2, 0, one, zero);
    vertex(0, 2, zero, one);
    b.U32(root + 12, Bytes(b, vertices));
    b.U32(root + 16, Bytes(b, {0, 0, 1, 0, 2, 0}));
    // The chunk's list, of one reference.
    const std::size_t chunks = b.bytes.size();
    b.U32(chunks, 1);
    b.U32(chunks + 4, static_cast<std::uint32_t>(chunk - b.data));
    b.U32(root + 8, static_cast<std::uint32_t>(chunks - b.data));
    b.U32(root, b.String(u"slab"));
    b.U32(chunk + 48, b.String(u"Slab"));
    b.U32(chunk + 52, 24);
    b.U32(chunk + 56, 3);
    b.U32(chunk + 60, 3);
    // Three declarators in place: the place, the first coordinates, the lightmap's.
    const std::size_t declarators = b.bytes.size();
    b.U32(declarators, 1 + sets);
    const std::uint32_t rows[3][4] = {{0, 2, 0, 0}, {firstAt, firstType, 5, 0}, {20, 15, 5, 1}};
    for (std::size_t i = 0; i < 1 + sets; ++i)
    {
        const std::size_t at = declarators + 4 + (24 * i);
        b.U32(at, 0);
        b.U32(at + 4, rows[i][0]);
        b.U32(at + 8, rows[i][1]);
        b.U32(at + 12, rows[i][2]);
        b.U32(at + 16, rows[i][3]);
        b.U32(at + 20, 0);
    }
    b.U32(chunk + 96, static_cast<std::uint32_t>(declarators - b.data));
    return b.bytes;
}

// A model whose mesh file is "slab": a node moved by (10, 0, 0) with a mesh
// node under it, of chunk "Slab" and part "_a", moved by (0, 1, 0) more and
// turned a quarter about z; and beside them a light probe at (1, 2, 3).
std::vector<std::byte> ModelFile()
{
    const std::uint16_t children = gff4::kList | gff4::kReference;
    Builder b = Start("MMH ", {{"mdlh", 8, {{6005, 14, 0, 0}, {6999, kGeneric, children, 4}}},
                               {"node", 8, {{6000, 14, 0, 0}, {6999, kGeneric, children, 4}}},
                               {"mshh", 12, {{6006, 14, 0, 0}, {6004, 14, 0, 4}, {6999, kGeneric, children, 8}}},
                               {"trsl", 16, {{6047, 12, 0, 0}}},
                               {"rota", 16, {{6048, 13, 0, 0}}}});
    const std::size_t root = Struct(b, 8);
    const std::size_t node = Struct(b, 8);
    const std::size_t nodeAt = Struct(b, 16);
    const std::size_t mesh = Struct(b, 12);
    const std::size_t meshAt = Struct(b, 16);
    const std::size_t meshTurn = Struct(b, 16);
    Floats(b, nodeAt, {10, 0, 0, 1});
    Floats(b, meshAt, {0, 1, 0, 1});
    Floats(b, meshTurn, {0, 0, 0.70710678F, 0.70710678F});
    b.U32(root, b.String(u"slab"));
    b.U32(root + 4, Children(b, {{1, node}}));
    b.U32(node, b.String(u"Root"));
    b.U32(node + 4, Children(b, {{3, nodeAt}, {2, mesh}}));
    b.U32(mesh, b.String(u"Slab"));
    b.U32(mesh + 4, b.String(u"_a"));
    b.U32(mesh + 8, Children(b, {{3, meshAt}, {4, meshTurn}}));
    return b.bytes;
}

// A room's layout as the game's are (lak303d_3699_ll.mmh): "GOB" at (1, 0, 0)
// turned a quarter about z, and under it a flickering lamp "Lamp_RT01" at
// (2, 0, 0), two ambient lights, "Ambient0" first, and a probe at (0, 0, 3)
// with its cube, its irradiance's file and red matrix 0 to 15, green 100
// on, blue 200 on.
std::vector<std::byte> LayoutFile()
{
    const std::uint16_t children = gff4::kList | gff4::kReference;
    Builder b = Start(
        "MMH ", {{"mdlh", 4, {{6999, kGeneric, children, 0}}},
                 {"node", 8, {{6000, 14, 0, 0}, {6999, kGeneric, children, 4}}},
                 {"trsl", 16, {{6047, 12, 0, 0}}},
                 {"rota", 16, {{6048, 13, 0, 0}}},
                 {"nplt",
                  64,
                  {{6000, 14, 0, 16},
                   {6004, 5, 0, 20},
                   {6007, 15, 0, 0},
                   {6008, 8, 0, 24},
                   {6009, 0, 0, 44},
                   {6249, 8, 0, 28},
                   {6250, 8, 0, 32},
                   {6251, 8, 0, 36},
                   {6296, 0, 0, 45},
                   {6339, 0, 0, 46},
                   {6340, 0, 0, 48},
                   {6345, 0, 0, 47},
                   {6999, kGeneric, children, 40}}},
                 {"nalt", 32, {{6000, 14, 0, 16}, {6010, 15, 0, 0}, {6340, 0, 0, 24}, {6999, kGeneric, children, 20}}},
                 {"nlpr", 8, {{6000, 14, 0, 0}, {6999, kGeneric, children, 4}}},
                 {"nrfl", 4, {{3104, 14, 0, 0}}},
                 {"nirr", 208, {{6253, 14, 0, 192}, {6336, 16, 0, 0}, {6337, 16, 0, 64}, {6338, 16, 0, 128}}}});
    const std::size_t root = Struct(b, 4);
    const std::size_t gob = Struct(b, 8);
    const std::size_t gobAt = Struct(b, 16);
    const std::size_t gobTurn = Struct(b, 16);
    const std::size_t lamp = Struct(b, 64);
    const std::size_t lampAt = Struct(b, 16);
    const std::size_t ambient = Struct(b, 32);
    const std::size_t second = Struct(b, 32);
    const std::size_t probe = Struct(b, 8);
    const std::size_t probeAt = Struct(b, 16);
    const std::size_t cube = Struct(b, 4);
    const std::size_t irradiance = Struct(b, 208);
    Floats(b, gobAt, {1, 0, 0, 1});
    Floats(b, gobTurn, {0, 0, 0.70710678F, 0.70710678F});
    Floats(b, lampAt, {2, 0, 0, 1});
    Floats(b, probeAt, {0, 0, 3, 1});
    Floats(b, lamp, {1.82F, 0.96F, 0.30F, 1});
    Floats(b, lamp + 24, {8, 0.25F, 0.075F, 0.025F});
    b.U8(lamp + 44, 1);
    b.U8(lamp + 45, 2);
    b.U8(lamp + 47, 1);
    Floats(b, ambient, {0.104F, 0.112F, 0.108F, 1});
    Floats(b, second, {0.5F, 0.5F, 0.5F, 1});
    for (std::size_t i = 0; i < 16; ++i)
    {
        Float(b, irradiance + (4 * i), static_cast<float>(i));
        Float(b, irradiance + 64 + (4 * i), static_cast<float>(100 + i));
        Float(b, irradiance + 128 + (4 * i), static_cast<float>(200 + i));
    }
    b.U32(root, Children(b, {{1, gob}}));
    b.U32(gob, b.String(u"GOB"));
    b.U32(gob + 4, Children(b, {{2, gobAt}, {3, gobTurn}, {4, lamp}, {5, ambient}, {5, second}, {6, probe}}));
    b.U32(lamp + 16, b.String(u"Lamp_RT01"));
    b.U32(lamp + 40, Children(b, {{2, lampAt}}));
    b.U32(ambient + 16, b.String(u"Ambient0"));
    b.U32(second + 16, b.String(u"Ambient1"));
    b.U32(probe, b.String(u"NewLightProbe"));
    b.U32(probe + 4, Children(b, {{2, probeAt}, {7, cube}, {8, irradiance}}));
    b.U32(cube, b.String(u"lak303d_probe_2zz.dds"));
    b.U32(irradiance + 192, b.String(u"lak303d_probe_2zz.mtx"));
    return b.bytes;
}

// A level's layout as the game's are (lak303d.arl, tools/gff4.py --templates), with a few of its
// fields: room 3672 "lak303d_3672" from (-1, -2, -3) to (4, 5, 6), a sun along -z of colour
// (0.5, 0.25, 0.125, 1), da_hsvmatrix with a Brightness of 0.75 held in a generic field, the
// chunks' switch on with 10 rows, the cut-off on, an atmosphere of a sun's intensity and a field
// with no name, and one tree.
std::vector<std::byte> LevelLayoutFile()
{
    Builder b =
        Start("ARL ", {{"AREA",
                        80,
                        {{3011, 5, 0, 0},
                         {3012, 14, 0, 4},
                         {3016, 1, gff4::kList | gff4::kStruct, 8},
                         {3024, 10, 0, 12},
                         {3026, 2, gff4::kList | gff4::kStruct, 24},
                         {3150, 10, 0, 28},
                         {3152, 15, 0, 40},
                         {3122, 1, 0, 56},
                         {3123, 5, 0, 60},
                         {3134, 1, 0, 64},
                         {22500, 4, gff4::kStruct, 68},
                         {3355, 5, gff4::kList | gff4::kStruct, 76}}},
                       {"ROOM", 32, {{3031, 5, 0, 0}, {3033, 14, 0, 4}, {3140, 10, 0, 8}, {3141, 10, 0, 20}}},
                       {"FBE", 12, {{5520, 14, 0, 0}, {5521, 14, 0, 4}, {5522, 3, gff4::kList | gff4::kStruct, 8}}},
                       {"FBEP", 12, {{5523, 14, 0, 0}, {5524, kGeneric, gff4::kReference, 4}}},
                       {"ATMO", 8, {{22520, 8, 0, 0}, {22999, 8, 0, 4}}},
                       {"SPT", 4, {{3353, 14, 0, 0}}}});
    const std::size_t root = Struct(b, 80);
    b.U32(root, 7);
    b.U32(root + 4, b.String(u"Circle Tower"));
    Floats(b, root + 12, {1, 2, 3});
    Floats(b, root + 28, {0, 0, -1});
    Floats(b, root + 40, {0.5F, 0.25F, 0.125F, 1});
    b.U8(root + 56, 1);
    b.U32(root + 60, 10);
    b.U8(root + 64, 1);
    Float(b, root + 68, 19.95F);
    Float(b, root + 72, 3.0F);

    const std::size_t rooms = b.bytes.size();
    b.U32(rooms, 1);
    const std::size_t room = Struct(b, 32);
    b.U32(root + 8, static_cast<std::uint32_t>(rooms - b.data));
    b.U32(room, 3672);
    b.U32(room + 4, b.String(u"lak303d_3672"));
    Floats(b, room + 8, {-1, -2, -3, 4, 5, 6});

    const std::size_t effects = b.bytes.size();
    b.U32(effects, 1);
    const std::size_t effect = Struct(b, 12);
    b.U32(root + 24, static_cast<std::uint32_t>(effects - b.data));
    b.U32(effect, b.String(u"da_hsvmatrix"));
    b.U32(effect + 4, b.String(u"da_hsvmatrix"));
    const std::size_t parameters = b.bytes.size();
    b.U32(parameters, 1);
    const std::size_t parameter = Struct(b, 12);
    b.U32(effect + 8, static_cast<std::uint32_t>(parameters - b.data));
    b.U32(parameter, b.String(u"Brightness"));
    b.U32(parameter + 4, static_cast<std::uint32_t>(gff4::Type::F32)); // the element's type, no flags
    const std::size_t value = b.bytes.size();
    Float(b, value, 0.75F);
    b.U32(parameter + 8, static_cast<std::uint32_t>(value - b.data));

    const std::size_t trees = b.bytes.size();
    b.U32(trees, 1);
    const std::size_t tree = Struct(b, 4);
    b.U32(root + 76, static_cast<std::uint32_t>(trees - b.data));
    b.U32(tree, b.String(u"Lak100d_tre_m_mossy"));
    return b.bytes;
}

// A room at (100, 0, 0) that sees rooms 7 and 9 and opens onto 9 and 12, with the model "slab"
// placed in it at (0, 0, 5), twice the size, its part "_a" in the atlas
// "room.dds" at an offset of (0.5, 0.25) and a scale of (0.25, 0.5), and its user param "fire"
// (1, 1, 1, 1), as the tower's fxe_fire_small_p are placed (DAOLib #212), with its cut-away
// override 2, as most placed effects have it (DAOLib #241).
std::vector<std::byte> RoomFile()
{
    Builder b = Start("RML ", {{"ROOM",
                                48,
                                {{4, 10, 0, 0},
                                 {5, 13, 0, 16},
                                 {3047, 5, gff4::kList, 32},
                                 {3050, 2, gff4::kList | gff4::kStruct, 36},
                                 {3099, 5, gff4::kList, 40},
                                 {3164, 5, gff4::kList, 44}}},
                               {"LMAT", 24, {{3324, 12, 0, 0}, {3323, 14, 0, 16}, {3235, 14, 0, 20}}},
                               {"MDL ",
                                52,
                                {{4, 10, 0, 0},
                                 {5, 13, 0, 16},
                                 {3059, 8, 0, 32},
                                 {3063, 14, 0, 36},
                                 {3326, 1, gff4::kList | gff4::kStruct, 40},
                                 {3506, 3, gff4::kList | gff4::kStruct, 44},
                                 {3109, 0, 0, 48}}},
                               {"UPRM", 20, {{3507, 14, 0, 0}, {3508, 12, 0, 4}}}});
    const std::size_t root = Struct(b, 48);
    Floats(b, root, {100, 0, 0});
    Floats(b, root + 16, {0, 0, 0, 1});
    b.U32(root + 32, b.Numbers({7, 9}));
    b.U32(root + 40, b.Numbers({9, 12}));
    b.U32(root + 44, b.Numbers({0xFFFFFFFF}));
    const std::size_t models = b.bytes.size();
    b.U32(models, 1);
    const std::size_t model = Struct(b, 52);
    b.U32(root + 36, static_cast<std::uint32_t>(models - b.data));
    b.U8(model + 48, 2);
    Floats(b, model, {0, 0, 5});
    Floats(b, model + 16, {0, 0, 0, 1});
    Float(b, model + 32, 2.0F);
    b.U32(model + 36, b.String(u"slab"));
    const std::size_t parts = b.bytes.size();
    b.U32(parts, 1);
    const std::size_t part = Struct(b, 24);
    b.U32(model + 40, static_cast<std::uint32_t>(parts - b.data));
    Floats(b, part, {0.5F, 0.25F, 0.25F, 0.5F});
    b.U32(part + 16, b.String(u"room.dds"));
    b.U32(part + 20, b.String(u"_a"));
    const std::size_t params = b.bytes.size();
    b.U32(params, 1);
    const std::size_t param = Struct(b, 20);
    b.U32(model + 44, static_cast<std::uint32_t>(params - b.data));
    b.U32(param, b.String(u"fire"));
    Floats(b, param + 4, {1, 1, 1, 1});
    return b.bytes;
}

// A room with no models and two trees laid out as the game's SPT structs: tree 3 at (1, 2, 3),
// a quarter turn about z, scale 0.3, tinted (0.25, 0.5, 0.75, 1), drawn at step 2; and tree 0 at
// (-4, 0, 0), unturned, scale 0.5, white, at step 1. Without `drawDistance` the template lacks
// the field, as 20 of the game's files do.
std::vector<std::byte> TreeRoomFile(bool drawDistance = true)
{
    std::vector<dao::test::FieldSpec> tree{
        {4, 10, 0, 0}, {5, 13, 0, 16}, {3356, 8, 0, 52}, {3357, 5, 0, 48}, {3375, 12, 0, 32}};
    if (drawDistance)
    {
        tree.push_back({3379, 1, 0, 56});
    }
    Builder b = Start("RML ", {{"ROOM",
                                40,
                                {{4, 10, 0, 0},
                                 {5, 13, 0, 16},
                                 {3050, 1, gff4::kList | gff4::kStruct, 32},
                                 {3354, 2, gff4::kList | gff4::kStruct, 36}}},
                               {"MDL ", 48, {{4, 10, 0, 0}}},
                               {"SPT ", 64, tree}});
    const std::size_t root = Struct(b, 40);
    Floats(b, root + 16, {0, 0, 0, 1});
    const std::size_t models = b.bytes.size();
    b.U32(models, 0);
    b.U32(root + 32, static_cast<std::uint32_t>(models - b.data));
    const std::size_t trees = b.bytes.size();
    b.U32(trees, 2);
    b.U32(root + 36, static_cast<std::uint32_t>(trees - b.data));
    const float half = 0.70710678F;
    const std::size_t first = Struct(b, 64);
    const std::size_t second = Struct(b, 64);
    Floats(b, first, {1, 2, 3});
    Floats(b, first + 16, {0, 0, half, half});
    Floats(b, first + 32, {0.25F, 0.5F, 0.75F, 1});
    b.U32(first + 48, 3);
    Float(b, first + 52, 0.3F);
    b.U8(first + 56, 2);
    Floats(b, second, {-4, 0, 0});
    Floats(b, second + 16, {0, 0, 0, 1});
    Floats(b, second + 32, {1, 1, 1, 1});
    Float(b, second + 52, 0.5F);
    b.U8(second + 56, 1);
    return b.bytes;
}

} // namespace

TEST_CASE("a mesh's chunk is read: places, the lightmap's coordinates, the triangle", "[level]")
{
    const std::vector<std::byte> bytes = MeshFile();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    const std::vector<level::MeshChunk> chunks = level::Meshes(*file);
    REQUIRE(chunks.size() == 1);
    CHECK(chunks[0].name == L"Slab");
    REQUIRE(chunks[0].positions.size() == 3);
    CHECK(chunks[0].positions[1] == Point{2, 0, 0});
    REQUIRE(chunks[0].lightUv.size() == 3);
    CHECK(chunks[0].lightUv[1] == level::Uv{1, 0});
    CHECK(chunks[0].lightUv[2] == level::Uv{0, 1});
    // The first set is the texture's own, beside the lightmap's.
    REQUIRE(chunks[0].paintUv.size() == 3);
    CHECK(chunks[0].paintUv[1] == level::Uv{0.5F, 0.5F});
    CHECK(chunks[0].indices == std::vector<std::uint32_t>{0, 1, 2});

    // One set of texture coordinates is the lightmap's: a terrain's ground.
    const std::vector<std::byte> ground = MeshFile(1);
    const std::optional<gff4::File> groundFile = gff4::File::Parse(ground);
    REQUIRE(groundFile.has_value());
    const std::vector<level::MeshChunk> patch = level::Meshes(*groundFile);
    REQUIRE(patch.size() == 1);
    REQUIRE(patch[0].lightUv.size() == 3);
    CHECK(patch[0].lightUv[1] == level::Uv{0.5F, 0.5F});
    CHECK(patch[0].positions[1] == Point{2, 0, 0});
    // And the texture's too, being the only one.
    REQUIRE(patch[0].paintUv.size() == 3);
    CHECK(patch[0].paintUv[1] == level::Uv{0.5F, 0.5F});
}

TEST_CASE("a chunk whose first texture coordinates are not floats has none for its texture, and is read", "[level]")
{
    // Direct3D's D3DDECLTYPE_SHORT2 (6) for the first set; the second, the lightmap's, as before.
    const std::vector<std::byte> bytes = MeshFile(2, 6);
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    const std::vector<level::MeshChunk> chunks = level::Meshes(*file);
    REQUIRE(chunks.size() == 1);
    CHECK(chunks[0].paintUv.empty());
    REQUIRE(chunks[0].lightUv.size() == 3);
    CHECK(chunks[0].lightUv[1] == level::Uv{1, 0});
    CHECK(chunks[0].positions[1] == Point{2, 0, 0});

    // And so has one whose first set is declared where the vertices' bytes do not reach.
    const std::vector<std::byte> shortBytes = MeshFile(2, 1, 4000);
    const std::optional<gff4::File> shortFile = gff4::File::Parse(shortBytes);
    REQUIRE(shortFile.has_value());
    const std::vector<level::MeshChunk> cut = level::Meshes(*shortFile);
    REQUIRE(cut.size() == 1);
    CHECK(cut[0].paintUv.empty());
    REQUIRE(cut[0].lightUv.size() == 3);
    CHECK(cut[0].lightUv[2] == level::Uv{0, 1});
    CHECK(cut[0].positions[2] == Point{0, 2, 0});
}

TEST_CASE("a model's mesh node has its place and turn through the nodes above it", "[level]")
{
    const std::vector<std::byte> bytes = ModelFile();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    const level::Model model = level::ModelOf(*file);
    CHECK(model.meshFile == L"slab");
    REQUIRE(model.nodes.size() == 1);
    CHECK(model.nodes[0].chunk == L"Slab");
    CHECK(model.nodes[0].part == L"_a");
    CHECK(model.nodes[0].at == Point{10, 1, 0});
    // A quarter about z: x goes to y.
    const Point turned = level::Turned(model.nodes[0].turn, {1, 0, 0});
    CHECK(turned[0] == Approx(0.0F).margin(1e-5));
    CHECK(turned[1] == Approx(1.0F).margin(1e-5));
}

TEST_CASE("a room's layout has its lamps, its ambient light and its probes, placed through the nodes above them",
          "[level]")
{
    const std::vector<std::byte> bytes = LayoutFile();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    const level::Layout layout = level::LayoutOf(*file);

    REQUIRE(layout.lamps.size() == 1);
    const level::Lamp &lamp = layout.lamps[0];
    CHECK(lamp.name == L"Lamp_RT01");
    // (2, 0, 0) turned a quarter is (0, 2, 0), then GOB's (1, 0, 0) added.
    CHECK(lamp.at[0] == Approx(1.0F).margin(1e-5));
    CHECK(lamp.at[1] == Approx(2.0F).margin(1e-5));
    CHECK(lamp.at[2] == Approx(0.0F).margin(1e-5));
    CHECK(lamp.colour == level::Colour{1.82F, 0.96F, 0.30F});
    CHECK(lamp.radius == 8.0F);
    CHECK(lamp.variation == 0.25F);
    CHECK(lamp.period == 0.075F);
    CHECK(lamp.periodDelta == 0.025F);
    CHECK(lamp.isStatic);
    CHECK(lamp.affectDomain == 2);
    CHECK(!lamp.canBeOccluded);
    CHECK(lamp.version == 1);

    REQUIRE(layout.ambient.has_value());
    CHECK(layout.ambient->name == L"Ambient0");
    CHECK(layout.ambient->colour == level::Colour{0.104F, 0.112F, 0.108F});
    CHECK(layout.ambient->at[0] == Approx(1.0F).margin(1e-5));

    REQUIRE(layout.probes.size() == 1);
    const level::Probe &probe = layout.probes[0];
    CHECK(probe.name == L"NewLightProbe");
    CHECK(probe.at[0] == Approx(1.0F).margin(1e-5));
    CHECK(probe.at[1] == Approx(0.0F).margin(1e-5));
    CHECK(probe.at[2] == Approx(3.0F).margin(1e-5));
    CHECK(probe.cube == L"lak303d_probe_2zz.dds");
    CHECK(probe.irradianceFile == L"lak303d_probe_2zz.mtx");
    CHECK(probe.irradiance[0] == 0.0F);
    CHECK(probe.irradiance[15] == 15.0F);
    CHECK(probe.irradiance[16] == 100.0F);
    CHECK(probe.irradiance[47] == 215.0F);
}

TEST_CASE("a model that is no layout has no lights", "[level]")
{
    const std::vector<std::byte> bytes = ModelFile();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    const level::Layout layout = level::LayoutOf(*file);
    CHECK(layout.lamps.empty());
    CHECK(!layout.ambient.has_value());
    CHECK(layout.probes.empty());
}

TEST_CASE("a level's layout has its rooms, sun, effects with their values, atmosphere and trees", "[level]")
{
    const std::vector<std::byte> bytes = LevelLayoutFile();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    const std::optional<level::LevelLayout> read = level::LevelLayoutOf(*file);
    REQUIRE(read.has_value());
    CHECK(read->id == 7);
    CHECK(read->name == L"Circle Tower");
    CHECK(read->center == Point{1, 2, 3});
    REQUIRE(read->rooms.size() == 1);
    CHECK(read->rooms[0].id == 3672);
    CHECK(read->rooms[0].file == L"lak303d_3672");
    CHECK(read->rooms[0].lower == Point{-1, -2, -3});
    CHECK(read->rooms[0].upper == Point{4, 5, 6});
    CHECK(read->sunDirection == Point{0, 0, -1});
    CHECK(read->sunColour == std::array<float, 4>{0.5F, 0.25F, 0.125F, 1});

    // The parameter's value is a generic field's float, by reference.
    REQUIRE(read->effects.size() == 1);
    CHECK(read->effects[0].name == L"da_hsvmatrix");
    REQUIRE(read->effects[0].parameters.size() == 1);
    CHECK(read->effects[0].parameters[0].first == L"Brightness");
    CHECK(read->effects[0].parameters[0].second == 0.75F);

    REQUIRE(read->atmosphere.size() == 2);
    CHECK(read->atmosphere[0].name == "AtmoSunIntensity");
    CHECK(read->atmosphere[0].floats == std::vector<float>{19.95F});
    CHECK(read->atmosphere[1].name == "22999");
    CHECK(read->clouds.empty());

    REQUIRE(read->chunks.has_value());
    CHECK(read->chunks->rows == 10);
    CHECK(read->chunks->columns == 0);
    CHECK(read->cutoffEnabled == std::optional<bool>(true));
    CHECK(!read->cutoffHeight.has_value());
    CHECK(!read->sunColourOnCharacters.has_value());
    CHECK(read->trees == std::vector<std::wstring>{L"Lak100d_tre_m_mossy"});
    CHECK(read->treeTint == std::array<float, 4>{1, 1, 1, 1});
}

TEST_CASE("a room's layout is no level's layout", "[level]")
{
    const std::vector<std::byte> bytes = LayoutFile();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    CHECK(!level::LevelLayoutOf(*file).has_value());
}

TEST_CASE("a room's models' triangles come out in the level's space with their place in the atlas", "[level]")
{
    const std::vector<std::byte> meshBytes = MeshFile();
    const std::vector<std::byte> modelBytes = ModelFile();
    const std::vector<std::byte> roomBytes = RoomFile();
    const std::optional<gff4::File> mesh = gff4::File::Parse(meshBytes);
    const std::optional<gff4::File> hierarchy = gff4::File::Parse(modelBytes);
    const std::optional<gff4::File> definition = gff4::File::Parse(roomBytes);
    REQUIRE((mesh && hierarchy && definition));
    const std::optional<level::Room> room = level::RoomOf(*definition);
    REQUIRE(room.has_value());
    CHECK(room->at == Point{100, 0, 0});
    CHECK(room->sees == std::vector<std::uint32_t>{7, 9});
    CHECK(room->connects == std::vector<std::uint32_t>{9, 12});
    CHECK(room->lightVis == std::vector<std::int32_t>{-1}); // every room's lights (DAOLib #243)
    REQUIRE(room->models.size() == 1);
    CHECK(room->models[0].file == L"slab");
    CHECK(room->models[0].scale == 2.0F);
    CHECK(room->models[0].cutAwayOverride == 2);
    REQUIRE(room->models[0].userParams.size() == 1);
    CHECK(room->models[0].userParams[0].name == L"fire");
    CHECK(room->models[0].userParams[0].value == std::array<float, 4>{1, 1, 1, 1});
    REQUIRE(room->models[0].parts.size() == 1);
    CHECK(room->models[0].parts[0].atlas == L"room.dds");
    CHECK(room->trees.empty());

    const std::vector<level::MeshChunk> chunks = level::Meshes(*mesh);
    std::vector<level::LitTriangle> triangles;
    std::wstring asked;
    level::AddTriangles(
        *room, room->models[0], level::ModelOf(*hierarchy), chunks,
        [&](const std::wstring &atlas) {
            asked = atlas;
            return 3U;
        },
        triangles);
    REQUIRE(triangles.size() == 1);
    CHECK(asked == L"room.dds");
    CHECK(triangles[0].atlas == 3);
    // The corner at (2, 0, 0): turned a quarter about z to (0, 2, 0), moved
    // by the node to (10, 3, 0), doubled to (20, 6, 0), placed at z 5 in a
    // room at x 100.
    const Point &corner = triangles[0].corners.b;
    CHECK(corner[0] == Approx(120.0F).margin(1e-3));
    CHECK(corner[1] == Approx(6.0F).margin(1e-3));
    CHECK(corner[2] == Approx(5.0F).margin(1e-3));
    // Its lightmap coordinates (1, 0) at the part's offset and scale.
    CHECK(triangles[0].uv[1][0] == Approx(0.75F));
    CHECK(triangles[0].uv[1][1] == Approx(0.25F));

    // A file that is no room's definition gives none.
    CHECK_FALSE(level::RoomOf(*mesh).has_value());
}

TEST_CASE("a room's trees are read with their place, turn, scale, model, tint and draw distance", "[level]")
{
    const std::vector<std::byte> bytes = TreeRoomFile();
    const std::optional<gff4::File> definition = gff4::File::Parse(bytes);
    REQUIRE(definition.has_value());
    const std::optional<level::Room> room = level::RoomOf(*definition);
    REQUIRE(room.has_value());
    CHECK(room->models.empty());
    CHECK(room->connects.empty()); // no list in the file
    CHECK(room->lightVis.empty());
    REQUIRE(room->trees.size() == 2);
    const level::PlacedTree &first = room->trees[0];
    CHECK(first.at == Point{1, 2, 3});
    CHECK(first.turn[2] == Approx(0.70710678F));
    CHECK(first.turn[3] == Approx(0.70710678F));
    CHECK(first.scale == Approx(0.3F));
    CHECK(first.tree == 3);
    CHECK(first.tint == std::array<float, 4>{0.25F, 0.5F, 0.75F, 1});
    CHECK(first.drawDistance == 2);
    const level::PlacedTree &second = room->trees[1];
    CHECK(second.at == Point{-4, 0, 0});
    CHECK(second.tree == 0);
    CHECK(second.scale == Approx(0.5F));
    CHECK(second.drawDistance == 1);
}

TEST_CASE("a room's tree whose file has no draw distance is drawn at step 0", "[level]")
{
    const std::vector<std::byte> bytes = TreeRoomFile(false);
    const std::optional<gff4::File> definition = gff4::File::Parse(bytes);
    REQUIRE(definition.has_value());
    const std::optional<level::Room> room = level::RoomOf(*definition);
    REQUIRE(room.has_value());
    REQUIRE(room->trees.size() == 2);
    CHECK(room->trees[0].tree == 3);
    CHECK(room->trees[0].drawDistance == 0);
    CHECK(room->trees[1].drawDistance == 0);
}

TEST_CASE("the game's rooms in game/rooms, when there, read with every tree turned, scaled and drawn as the files say",
          "[level]")
{
    const std::filesystem::path folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "rooms";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/rooms in this checkout (ghilana's extract of *.rml writes them)");
    }
    std::size_t rooms = 0;
    std::size_t trees = 0;
    std::size_t connected = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".rml")
        {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        std::vector<std::byte> bytes(chars.size());
        std::memcpy(bytes.data(), chars.data(), chars.size());
        const std::optional<gff4::File> file = gff4::File::Parse(bytes);
        INFO(entry.path().filename().string());
        REQUIRE(file.has_value());
        const std::optional<level::Room> room = level::RoomOf(*file);
        REQUIRE(room.has_value());
        // As read in 6,738 of the game's rooms (dao/core/Level.cpp): turns of length 1, scales
        // 0.07 to 0.61, models 0 to 25, draw distances 0 to 3.
        for (const level::PlacedTree &tree : room->trees)
        {
            const auto [x, y, z, w] = tree.turn;
            CHECK(((x * x) + (y * y) + (z * z) + (w * w)) == Approx(1.0F).margin(1e-3));
            CHECK(tree.scale > 0.0F);
            CHECK(tree.scale < 1.0F);
            CHECK(tree.tree >= 0);
            CHECK(tree.drawDistance >= 0);
            CHECK(tree.drawDistance <= 3);
        }
        // As read in the game's rooms (#186): 1 to 35 connections where a room has any.
        CHECK(room->connects.size() <= 35);
        connected += room->connects.empty() ? 0 : 1;
        ++rooms;
        trees += room->trees.size();
    }
    CHECK(rooms > 0);
    CHECK(trees > 0);
    CHECK(connected > 0);
}

TEST_CASE("the game's layouts in game/layouts, when there, read with every lamp lit and every probe named", "[level]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "layouts";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/layouts in this checkout (ghilana's extract of *_ll.mmh writes them)");
    }
    std::size_t layouts = 0;
    std::size_t lamps = 0;
    std::size_t ambients = 0;
    std::size_t probes = 0;
    std::size_t unlit = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".mmh")
        {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        std::vector<std::byte> bytes(chars.size());
        std::memcpy(bytes.data(), chars.data(), chars.size());
        const std::optional<gff4::File> file = gff4::File::Parse(bytes);
        INFO(entry.path().filename().string());
        REQUIRE(file.has_value());
        const level::Layout layout = level::LayoutOf(*file);
        for (const level::Lamp &lamp : layout.lamps)
        {
            CHECK(lamp.radius > 0.0F);
            CHECK(lamp.isStatic);
        }
        for (const level::Probe &probe : layout.probes)
        {
            CHECK(probe.cube.ends_with(L".dds"));
            CHECK(probe.irradianceFile.ends_with(L".mtx"));
            unlit += std::ranges::none_of(probe.irradiance, [](float value) { return value != 0.0F; }) ? 1 : 0;
        }
        ++layouts;
        lamps += layout.lamps.size();
        ambients += layout.ambient ? 1 : 0;
        probes += layout.probes.size();
    }
    CHECK(layouts > 0);
    // Every layout the game takes (DAOLib #152): the counts tools/gff4.py read in them.
    if (layouts == 2581)
    {
        CHECK(lamps == 10673);
        CHECK(ambients == 1083);
        CHECK(probes == 951);
        CHECK(unlit == 26); // the files' own, never baked (dao/core/Level.h)
    }
}

TEST_CASE("the game's level layouts in game/levels, when there, read with their rooms, effects and sky", "[level]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "levels";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/levels in this checkout (ghilana's extract of *.arl writes them)");
    }
    std::size_t levels = 0;
    std::size_t chunked = 0;
    std::size_t bloom = 0;
    std::size_t grade = 0;
    std::size_t gradeBrightness = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".arl")
        {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        std::vector<std::byte> bytes(chars.size());
        std::memcpy(bytes.data(), chars.data(), chars.size());
        const std::optional<gff4::File> file = gff4::File::Parse(bytes);
        INFO(entry.path().filename().string());
        REQUIRE(file.has_value());
        const std::optional<level::LevelLayout> read = level::LevelLayoutOf(*file);
        REQUIRE(read.has_value());
        CHECK(!read->rooms.empty());
        CHECK(read->atmosphere.size() >= 7);
        CHECK(read->clouds.size() == 6);
        for (const level::NamedFloats &value : read->atmosphere)
        {
            CHECK(!value.floats.empty());
        }
        ++levels;
        chunked += read->chunks ? 1 : 0;
        for (const level::FrameEffect &effect : read->effects)
        {
            bloom += effect.name == L"da_bloom" ? 1 : 0;
            if (effect.name == L"da_hsvmatrix")
            {
                ++grade;
                CHECK(effect.parameters.size() == 4);
                const auto brightness = std::ranges::find(effect.parameters, std::wstring(L"Brightness"),
                                                          &std::pair<std::wstring, float>::first);
                gradeBrightness += brightness != effect.parameters.end() && brightness->second == 1.0F ? 1 : 0;
            }
        }
    }
    CHECK(levels > 0);
    // Every level layout the game takes (DAOLib #154): the counts tools/gff4.py read in them.
    if (levels == 213)
    {
        CHECK(chunked == 80);
        CHECK(bloom == 208);
        CHECK(grade == 201);
        CHECK(gradeBrightness == 195); // the others 0.9 (4), 1.1 and 1.2
    }
}
