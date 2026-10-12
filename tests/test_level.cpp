#include "dao/core/Gff4.h"
#include "dao/core/Level.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

using Catch::Approx;
using dao::core::Point;
namespace gff4 = dao::core::gff4;
namespace level = dao::core::level;
using dao::test::Builder;

namespace
{

// The files here are laid out as the game's are (tools/gff4.py --templates
// on a .msh, a .mmh and a .rml), with the fields the reader takes and no
// others.

struct FieldSpec
{
    std::uint32_t label;
    std::uint16_t type;
    std::uint16_t flags;
    std::uint32_t offset;
};

struct TemplateSpec
{
    const char *label;
    std::uint32_t size;
    std::vector<FieldSpec> fields;
};

constexpr std::uint16_t kGeneric = 0xFFFF;

// The header and the templates; the data starts where this ends.
Builder Start(const char *type, const std::vector<TemplateSpec> &templates)
{
    Builder b;
    b.Text(0, std::string("GFF V4.0PC  ") + type + "V1.0");
    b.U32(20, static_cast<std::uint32_t>(templates.size()));
    std::size_t fields = 28 + (16 * templates.size());
    for (std::size_t i = 0; i < templates.size(); ++i)
    {
        const std::size_t at = 28 + (16 * i);
        b.Text(at, templates[i].label);
        b.U32(at + 4, static_cast<std::uint32_t>(templates[i].fields.size()));
        b.U32(at + 8, static_cast<std::uint32_t>(fields));
        b.U32(at + 12, templates[i].size);
        for (const FieldSpec &f : templates[i].fields)
        {
            b.U32(fields, f.label);
            b.U32(fields + 4, f.type | (std::uint32_t{f.flags} << 16));
            b.U32(fields + 8, f.offset);
            fields += 12;
        }
    }
    b.data = fields;
    b.U32(24, static_cast<std::uint32_t>(b.data));
    return b;
}

void Float(Builder &b, std::size_t at, float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, 4);
    b.U32(at, bits);
}

void Floats(Builder &b, std::size_t at, const std::vector<float> &values)
{
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        Float(b, at + (4 * i), values[i]);
    }
}

// Room for a struct at the data's end: where it is.
std::size_t Struct(Builder &b, std::size_t size)
{
    const std::size_t at = b.bytes.size();
    b.U8(at + size - 1, 0);
    return at;
}

// A list of bytes at the data's end: its data offset.
std::uint32_t Bytes(Builder &b, const std::vector<std::uint8_t> &bytes)
{
    const std::size_t at = b.bytes.size();
    b.U32(at, static_cast<std::uint32_t>(bytes.size()));
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        b.U8(at + 4 + i, bytes[i]);
    }
    return static_cast<std::uint32_t>(at - b.data);
}

// A generic list of references to structs, each (template, place): its data offset.
std::uint32_t Children(Builder &b, const std::vector<std::pair<std::uint16_t, std::size_t>> &structs)
{
    const std::size_t at = b.bytes.size();
    b.U32(at, static_cast<std::uint32_t>(structs.size()));
    for (std::size_t i = 0; i < structs.size(); ++i)
    {
        b.U32(at + 4 + (8 * i), structs[i].first | (std::uint32_t{gff4::kStruct | gff4::kReference} << 16));
        b.U32(at + 8 + (8 * i), static_cast<std::uint32_t>(structs[i].second - b.data));
    }
    return static_cast<std::uint32_t>(at - b.data);
}

template <class T> void Append(std::vector<std::uint8_t> &to, const T &value)
{
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(&value);
    to.insert(to.end(), bytes, bytes + sizeof(T));
}

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
                               {"rota", 16, {{6048, 13, 0, 0}}},
                               {"nlpr", 8, {{6000, 14, 0, 0}, {6999, kGeneric, children, 4}}}});
    const std::size_t root = Struct(b, 8);
    const std::size_t node = Struct(b, 8);
    const std::size_t nodeAt = Struct(b, 16);
    const std::size_t mesh = Struct(b, 12);
    const std::size_t meshAt = Struct(b, 16);
    const std::size_t meshTurn = Struct(b, 16);
    const std::size_t probe = Struct(b, 8);
    const std::size_t probeAt = Struct(b, 16);
    Floats(b, nodeAt, {10, 0, 0, 1});
    Floats(b, meshAt, {0, 1, 0, 1});
    Floats(b, meshTurn, {0, 0, 0.70710678F, 0.70710678F});
    Floats(b, probeAt, {1, 2, 3, 1});
    b.U32(root, b.String(u"slab"));
    b.U32(root + 4, Children(b, {{1, node}, {5, probe}}));
    b.U32(node, b.String(u"Root"));
    b.U32(node + 4, Children(b, {{3, nodeAt}, {2, mesh}}));
    b.U32(mesh, b.String(u"Slab"));
    b.U32(mesh + 4, b.String(u"_a"));
    b.U32(mesh + 8, Children(b, {{3, meshAt}, {4, meshTurn}}));
    b.U32(probe, b.String(u"NewLightProbe"));
    b.U32(probe + 4, Children(b, {{3, probeAt}}));
    return b.bytes;
}

// A room at (100, 0, 0) that sees rooms 7 and 9, with the model "slab"
// placed in it at (0, 0, 5), twice the size, its part "_a" in the atlas
// "room.dds" at an offset of (0.5, 0.25) and a scale of (0.25, 0.5).
std::vector<std::byte> RoomFile()
{
    Builder b = Start(
        "RML ",
        {{"ROOM",
          40,
          {{4, 10, 0, 0}, {5, 13, 0, 16}, {3047, 5, gff4::kList, 32}, {3050, 2, gff4::kList | gff4::kStruct, 36}}},
         {"LMAT", 24, {{3324, 12, 0, 0}, {3323, 14, 0, 16}, {3235, 14, 0, 20}}},
         {"MDL ",
          48,
          {{4, 10, 0, 0},
           {5, 13, 0, 16},
           {3059, 8, 0, 32},
           {3063, 14, 0, 36},
           {3326, 1, gff4::kList | gff4::kStruct, 40}}}});
    const std::size_t root = Struct(b, 40);
    Floats(b, root, {100, 0, 0});
    Floats(b, root + 16, {0, 0, 0, 1});
    b.U32(root + 32, b.Numbers({7, 9}));
    const std::size_t models = b.bytes.size();
    b.U32(models, 1);
    const std::size_t model = Struct(b, 48);
    b.U32(root + 36, static_cast<std::uint32_t>(models - b.data));
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

TEST_CASE("a model's mesh node has its place and turn through the nodes above it, and a layout its probe", "[level]")
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

    const std::optional<Point> probe = level::ProbeOf(*file);
    REQUIRE(probe.has_value());
    CHECK(*probe == Point{1, 2, 3});
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
    REQUIRE(room->models.size() == 1);
    CHECK(room->models[0].file == L"slab");
    CHECK(room->models[0].scale == 2.0F);
    REQUIRE(room->models[0].parts.size() == 1);
    CHECK(room->models[0].parts[0].atlas == L"room.dds");

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
