#include "dao/core/Gff4.h"
#include "dao/core/Model.h"
#include "files_builder.h" // with it, as ghilana's tests include both
#include "model_builder.h"

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
namespace gff4 = dao::core::gff4;
namespace model = dao::core::model;
namespace files = dao::test::files;

TEST_CASE("a mesh's chunks are its slices of the file's buffers, each with its whole declaration", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabMesh();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const std::optional<model::Mesh> mesh = model::MeshOf(*file);
    REQUIRE(mesh);
    CHECK(mesh->name == L"slab.msh");
    REQUIRE(mesh->chunks.size() == 2);
    const model::Chunk &body = mesh->chunks[0];
    CHECK(body.name == L"Body");
    // The end marker is not an element.
    REQUIRE(body.declaration.size() == 7);
    CHECK(body.vertices.size() == 156);
    CHECK(body.Find(model::Usage::Color) == nullptr);

    const model::Element *normal = body.Find(model::Usage::Normal);
    REQUIRE(normal);
    CHECK(normal->format == model::Format::Float16x4);
    CHECK(normal->offset == 28);
    const std::vector<model::Value> normals = model::Floats(body, *normal);
    REQUIRE(normals.size() == 3);
    CHECK(normals[2][2] == Approx(-1));
    CHECK(normals[2][3] == Approx(1));

    const std::vector<model::Value> tangents = model::Floats(body, *body.Find(model::Usage::Tangent));
    const std::vector<model::Value> binormals = model::Floats(body, *body.Find(model::Usage::Binormal));
    REQUIRE(tangents.size() == 3);
    REQUIRE(binormals.size() == 3);
    CHECK(tangents[0][0] == Approx(1));
    CHECK(binormals[0][1] == Approx(-1));

    const model::Element *uv = body.Find(model::Usage::TexCoord);
    REQUIRE(uv);
    CHECK(uv->format == model::Format::Float16x2);
    const std::vector<model::Value> uvs = model::Floats(body, *uv);
    REQUIRE(uvs.size() == 3);
    CHECK(uvs[1][0] == Approx(1));
    CHECK(uvs[2][1] == Approx(1));
    CHECK(uvs[2][2] == Approx(0)); // filled as Direct3D fills it
    CHECK(uvs[2][3] == Approx(1));

    const std::vector<model::Value> weights = model::Floats(body, *body.Find(model::Usage::BlendWeight));
    REQUIRE(weights.size() == 3);
    CHECK(weights[1][0] + weights[1][1] + weights[1][2] + weights[1][3] == Approx(1));

    const std::vector<model::Value> places = model::Floats(body, *body.Find(model::Usage::Position));
    REQUIRE(places.size() == 3);
    CHECK(places[2][0] == Approx(2));
    CHECK(places[2][3] == Approx(1));

    const std::vector<std::array<std::int16_t, 4>> bones = model::Shorts(body, *body.Find(model::Usage::BlendIndices));
    REQUIRE(bones.size() == 3);
    CHECK(bones[2] == std::array<std::int16_t, 4>{2, 1, 2, 0});
    CHECK(model::Shorts(body, *normal).empty());

    const model::Chunk &cap = mesh->chunks[1];
    CHECK(cap.vertexOffset == 160);
    CHECK(model::Floats(cap, cap.declaration[0])[1][1] == Approx(1));
    CHECK(model::Indices(body) == std::vector<std::uint32_t>{0, 1, 2});
    // Its own three indices, from the fourth of the file's, with its base vertex added.
    CHECK(model::Indices(cap) == std::vector<std::uint32_t>{1, 2, 2});

    // Its bounds, as the file keeps them (DAOLib #208).
    REQUIRE(body.bounds);
    CHECK(body.bounds->boxMin == std::array<float, 3>{0, 0, 0});
    CHECK(body.bounds->boxMax == std::array<float, 3>{2, 0, 0});
    CHECK(body.bounds->centre == std::array<float, 3>{1, 0, 0});
    CHECK(body.bounds->radius == Approx(1));
    REQUIRE(cap.bounds);
    CHECK(cap.bounds->boxMax == std::array<float, 3>{5, 2, 0});
}

TEST_CASE("a chunk that runs out of the file's vertices is left out, and a file that is no mesh is none", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabMesh(true);
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const std::optional<model::Mesh> mesh = model::MeshOf(*file);
    REQUIRE(mesh);
    REQUIRE(mesh->chunks.size() == 1);
    CHECK(mesh->chunks[0].name == L"Body");

    const std::vector<std::byte> other = files::SlabModel();
    const std::optional<gff4::File> notMesh = gff4::File::Parse(other);
    REQUIRE(notMesh);
    CHECK_FALSE(model::MeshOf(*notMesh));
    CHECK_FALSE(model::HierarchyOf(*file));
}

TEST_CASE("a model's nodes are in the file's order with their transforms, bones and mesh", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabModel();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const std::optional<model::Hierarchy> tree = model::HierarchyOf(*file);
    REQUIRE(tree);
    CHECK(tree->name == L"slab.mmh");
    CHECK(tree->meshFile == L"slab.msh");
    // The export is no node.
    REQUIRE(tree->nodes.size() == 4);
    const model::Node &gob = tree->nodes[0];
    CHECK(gob.name == L"GOB");
    CHECK(gob.kind == "node");
    CHECK(gob.parent == -1);
    CHECK(gob.children == std::vector<int>{1, 3});
    CHECK(gob.translation[2] == Approx(1));
    CHECK_FALSE(gob.bone);
    // Its export, the name a clip's channel goes by (DAOLib #177).
    REQUIRE(gob.exports.size() == 1);
    CHECK(gob.exports[0].name == L"gob_gobrotation");
    CHECK(gob.exports[0].controllerType == 8);
    CHECK(gob.exports[0].variableType == 4);
    CHECK(gob.exports[0].controllerIndex == 2);

    const model::Node &bone0 = tree->nodes[1];
    CHECK(bone0.bone == 1);
    CHECK(bone0.rotation[2] == Approx(0.70710678F));
    REQUIRE(bone0.exports.size() == 2);
    CHECK(bone0.exports[1].name == L"bone0_rotation");
    CHECK(tree->nodes[2].translation[1] == Approx(1));
    REQUIRE(bone0.scale);
    CHECK(*bone0.scale == Approx(2));
    CHECK(tree->nodes[2].parent == 1);

    const model::Node &mesh = tree->nodes[3];
    CHECK(mesh.kind == "mshh");
    REQUIRE(mesh.mesh);
    CHECK(mesh.mesh->chunk == L"Body");
    CHECK(mesh.mesh->material == L"ogre");
    CHECK(mesh.mesh->part == L"_1");
    CHECK(mesh.mesh->bonesUsed == std::vector<std::int32_t>{1, 0});

    CHECK(tree->bones == std::vector<int>{2, 1, -1});
}

TEST_CASE("a weapon trail's node has its material, its segment's length and its duration", "[model]")
{
    // As w_lsw_ls01a_0.mmh has it: under GOB, Cylinder04, weapontrail_streaks, 0.925 m, 0.5 s.
    const std::uint16_t children = gff4::kList | gff4::kReference;
    dao::test::Builder b = dao::test::Start(
        "MMH ",
        {{"mdlh", 16, {{6000, 14, 0, 0}, {6005, 14, 0, 4}, {6256, 4, 0, 8}, {6999, dao::test::kGeneric, children, 12}}},
         {"node", 12, {{6000, 14, 0, 0}, {6254, 5, 0, 4}, {6999, dao::test::kGeneric, children, 8}}},
         {"wtrl",
          24,
          {{6000, 14, 0, 0},
           {6001, 14, 0, 4},
           {6292, 8, 0, 8},
           {6293, 8, 0, 12},
           {6340, 0, 0, 20},
           {6999, dao::test::kGeneric, children, 16}}},
         {"trsl", 16, {{6047, 12, 0, 0}}}});
    const std::size_t root = dao::test::Struct(b, 16);
    const std::size_t gob = dao::test::Struct(b, 12);
    const std::size_t trail = dao::test::Struct(b, 24);
    const std::size_t at = dao::test::Struct(b, 16);
    b.U32(root, b.String(u"w_test.mmh"));
    b.U32(root + 4, b.String(u"w_test.msh"));
    b.U32(root + 12, dao::test::Children(b, {{1, gob}}));
    b.U32(gob, b.String(u"GOB"));
    b.U32(gob + 4, 0xFFFFFFFF);
    b.U32(gob + 8, dao::test::Children(b, {{2, trail}}));
    b.U32(trail, b.String(u"Cylinder04"));
    b.U32(trail + 4, b.String(u"weapontrail_streaks"));
    dao::test::Float(b, trail + 8, 0.925F);
    dao::test::Float(b, trail + 12, 0.5F);
    b.U8(trail + 20, 1);
    dao::test::Floats(b, at, {0, 0, 0.35F, 1});
    b.U32(trail + 16, dao::test::Children(b, {{3, at}}));

    const std::optional<gff4::File> file = gff4::File::Parse(b.bytes);
    REQUIRE(file);
    const std::optional<model::Hierarchy> tree = model::HierarchyOf(*file);
    REQUIRE(tree);
    REQUIRE(tree->nodes.size() == 2);
    CHECK_FALSE(tree->nodes[0].trail);
    const model::Node &node = tree->nodes[1];
    CHECK(node.name == L"Cylinder04");
    CHECK(node.kind == "wtrl");
    CHECK(node.parent == 0);
    CHECK(node.translation[2] == Approx(0.35F));
    REQUIRE(node.trail);
    CHECK(node.trail->material == L"weapontrail_streaks");
    CHECK(node.trail->segmentLength == Approx(0.925F));
    CHECK(node.trail->duration == Approx(0.5F));
    CHECK(node.trail->useVariationTint);
}

TEST_CASE("a mesh node is cut away when its MMHMeshCutAway is set", "[model]")
{
    // As the levels' MshFade_ meshes have it: a byte at the mshh struct's +52.
    const std::uint16_t children = gff4::kList | gff4::kReference;
    dao::test::Builder b = dao::test::Start(
        "MMH ",
        {{"mdlh", 16, {{6000, 14, 0, 0}, {6005, 14, 0, 4}, {6256, 4, 0, 8}, {6999, dao::test::kGeneric, children, 12}}},
         {"mshh",
          64,
          {{6000, 14, 0, 0},
           {6001, 14, 0, 4},
           {6006, 14, 0, 12},
           {6193, 0, 0, 52},
           {6999, dao::test::kGeneric, children, 20}}}});
    const std::size_t root = dao::test::Struct(b, 16);
    const std::size_t fade = dao::test::Struct(b, 64);
    const std::size_t wall = dao::test::Struct(b, 64);
    b.U32(root, b.String(u"room.mmh"));
    b.U32(root + 4, b.String(u"room.msh"));
    b.U32(root + 12, dao::test::Children(b, {{1, fade}, {1, wall}}));
    for (const std::size_t mesh : {fade, wall})
    {
        b.U32(mesh + 4, b.String(u"stone"));
        b.U32(mesh + 20, 0xFFFFFFFF);
    }
    b.U32(fade, b.String(u"MshFade_roof"));
    b.U32(fade + 12, b.String(u"Roof"));
    b.U8(fade + 52, 1);
    b.U32(wall, b.String(u"Wall"));
    b.U32(wall + 12, b.String(u"Wall"));

    const std::optional<gff4::File> file = gff4::File::Parse(b.bytes);
    REQUIRE(file);
    const std::optional<model::Hierarchy> tree = model::HierarchyOf(*file);
    REQUIRE(tree);
    REQUIRE(tree->nodes.size() == 2);
    REQUIRE(tree->nodes[0].mesh);
    CHECK(tree->nodes[0].mesh->cutAway);
    REQUIRE(tree->nodes[1].mesh);
    CHECK_FALSE(tree->nodes[1].mesh->cutAway);
}

TEST_CASE("the ogre's body, when it is in game/models, reads whole", "[model]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "models";
    const auto read = [&](const char *name) {
        std::ifstream in(folder / name, std::ios::binary);
        std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        std::vector<std::byte> out(chars.size());
        std::memcpy(out.data(), chars.data(), chars.size());
        return out;
    };
    if (!std::filesystem::exists(folder / "cn_bdy_ogr_0.msh") || !std::filesystem::exists(folder / "cn_bdy_ogra_0.mmh"))
    {
        SKIP("no game/models/cn_bdy_ogr_0.msh and cn_bdy_ogra_0.mmh in this checkout (ghilana's extract writes them)");
    }
    const std::vector<std::byte> meshBytes = read("cn_bdy_ogr_0.msh");
    const std::vector<std::byte> treeBytes = read("cn_bdy_ogra_0.mmh");
    const std::optional<gff4::File> meshFile = gff4::File::Parse(meshBytes);
    const std::optional<gff4::File> treeFile = gff4::File::Parse(treeBytes);
    REQUIRE(meshFile);
    REQUIRE(treeFile);
    const std::optional<model::Mesh> mesh = model::MeshOf(*meshFile);
    const std::optional<model::Hierarchy> tree = model::HierarchyOf(*treeFile);
    REQUIRE(mesh);
    REQUIRE(tree);
    REQUIRE(mesh->chunks.size() == 1);
    const model::Chunk &chunk = mesh->chunks[0];
    CHECK(chunk.vertexCount == 9652);
    CHECK(chunk.declaration.size() == 7);
    CHECK(model::Indices(chunk).size() == 29505);
    // Every place is in the chunk's box and its sphere.
    REQUIRE(chunk.bounds);
    const model::Bounds &bounds = *chunk.bounds;
    const std::vector<model::Value> places = model::Floats(chunk, *chunk.Find(model::Usage::Position));
    const auto outside = std::ranges::count_if(places, [&](const model::Value &place) {
        float squared = 0;
        bool inBox = true;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            inBox = inBox && place[axis] >= bounds.boxMin[axis] - 1e-3F && place[axis] <= bounds.boxMax[axis] + 1e-3F;
            squared += (place[axis] - bounds.centre[axis]) * (place[axis] - bounds.centre[axis]);
        }
        return !inBox || squared > (bounds.radius + 1e-3F) * (bounds.radius + 1e-3F);
    });
    CHECK(outside == 0);
    const std::vector<model::Value> weights = model::Floats(chunk, *chunk.Find(model::Usage::BlendWeight));
    REQUIRE(weights.size() == 9652);
    CHECK(weights[0][0] + weights[0][1] + weights[0][2] + weights[0][3] == Approx(1).epsilon(0.01));

    CHECK(tree->totalBones == 99);
    const auto meshNode =
        std::ranges::find_if(tree->nodes, [](const model::Node &node) { return node.mesh.has_value(); });
    REQUIRE(meshNode != tree->nodes.end());
    CHECK(meshNode->mesh->chunk == chunk.name);
    CHECK(meshNode->mesh->material == L"CN_BDY_OGRa");
    CHECK(meshNode->mesh->bonesUsed.size() == 64);
    std::int16_t highest = 0;
    for (const auto &indices : model::Shorts(chunk, *chunk.Find(model::Usage::BlendIndices)))
    {
        highest = std::max({highest, indices[0], indices[1], indices[2], indices[3]});
    }
    CHECK(highest < 64);
    for (const std::int32_t bone : meshNode->mesh->bonesUsed)
    {
        REQUIRE(bone >= 0);
        CHECK(tree->bones.at(static_cast<std::size_t>(bone)) >= 0);
    }
}
