#include "dao/core/Gff4.h"
#include "dao/core/Model.h"
#include "files_builder.h" // with it, as ghilana's tests include both
#include "model_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

namespace
{

// A chunk's values as ChunkData takes them: what was read, element by element.
model::ChunkData AsRead(const model::Chunk &chunk)
{
    model::ChunkData data{
        .name = chunk.name, .index = std::nullopt, .declaration = {}, .values = {}, .indices = model::Indices(chunk)};
    for (std::uint32_t &index : data.indices)
    {
        index -= chunk.baseVertex;
    }
    for (const model::Element &element : chunk.declaration)
    {
        if (element.format == model::Format::Short4 || element.format == model::Format::Short2)
        {
            std::vector<model::Value> values;
            for (const auto &shorts : model::Shorts(chunk, element))
            {
                values.push_back({static_cast<float>(shorts[0]), static_cast<float>(shorts[1]),
                                  static_cast<float>(shorts[2]), static_cast<float>(shorts[3])});
            }
            data.values.push_back(std::move(values));
        }
        else
        {
            data.values.push_back(model::Floats(chunk, element));
        }
    }
    return data;
}

std::optional<model::Mesh> Read(const std::vector<std::byte> &bytes, std::optional<gff4::File> &file)
{
    file = gff4::File::Parse(bytes);
    return file ? model::MeshOf(*file) : std::nullopt;
}

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());
    std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

} // namespace

TEST_CASE("an editor leaves a file it set nothing in the same, and its sets read back", "[model][gff4]")
{
    const std::vector<std::byte> bytes = files::SlabMesh();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file);
    gff4::Editor untouched(*file);
    CHECK(untouched.Bytes() == bytes);

    gff4::Editor editor(*file);
    const gff4::Struct chunk = file->Root()->Structs(8021)[0];
    CHECK(editor.SetNumber(chunk, 8001, 7));
    CHECK_FALSE(editor.SetNumber(chunk, 2, 7)); // a string is no number
    CHECK(editor.SetString(chunk, 2, L"Renamed"));
    const std::array<std::byte, 3> three{std::byte{1}, std::byte{2}, std::byte{3}};
    CHECK(editor.SetList(*file->Root(), 8023, three, 3, gff4::Place::InPlaceIfFits));
    CHECK(editor.Bytes().size() > bytes.size()); // only the string was appended

    const std::vector<std::byte> after = editor.Bytes();
    const std::optional<gff4::File> edited = gff4::File::Parse(after);
    REQUIRE(edited);
    const gff4::Struct read = edited->Root()->Structs(8021)[0];
    CHECK(read.Get<std::uint32_t>(8001) == 7U);
    CHECK(read.String(2) == L"Renamed");
    CHECK(edited->Root()->ListBytes(8023, gff4::Type::U8).size() == 3);
}

TEST_CASE("a mesh written with nothing changed, or each chunk as it was read, is the same bytes", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabMesh();
    std::optional<gff4::File> file;
    const std::optional<model::Mesh> mesh = Read(bytes, file);
    REQUIRE(mesh);
    const model::Written none = model::WriteMesh(bytes, {});
    CHECK(none.error.empty());
    CHECK(none.bytes == bytes);

    const std::vector<model::ChunkData> same{AsRead(mesh->chunks[0]), AsRead(mesh->chunks[1])};
    const model::Written again = model::WriteMesh(bytes, same);
    CHECK(again.error.empty());
    CHECK(again.bytes == bytes);
}

TEST_CASE("a chunk's vertices moved are written in place, and its bounds made again", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabMesh();
    std::optional<gff4::File> file;
    const std::optional<model::Mesh> mesh = Read(bytes, file);
    REQUIRE(mesh);
    model::ChunkData body = AsRead(mesh->chunks[0]);
    for (model::Value &place : body.values[0])
    {
        place[0] += 10;
    }
    const std::vector<model::ChunkData> given{body};
    const model::Written written = model::WriteMesh(bytes, given);
    REQUIRE(written.error.empty());
    CHECK(written.bytes.size() == bytes.size());

    std::optional<gff4::File> after;
    const std::optional<model::Mesh> read = Read(written.bytes, after);
    REQUIRE(read);
    REQUIRE(read->chunks.size() == 2);
    const model::Chunk &moved = read->chunks[0];
    CHECK(model::Floats(moved, *moved.Find(model::Usage::Position))[2][0] == Approx(12));
    CHECK(model::Floats(moved, *moved.Find(model::Usage::Normal))[2][2] == Approx(-1));
    const std::optional<gff4::Struct> bounds = after->Root()->Structs(8021)[0].Child(8020);
    REQUIRE(bounds);
    CHECK(bounds->Floats(8017)[0] == Approx(10));
    CHECK(bounds->Floats(8018)[0] == Approx(12));
    CHECK(bounds->Floats(8019)[3] == Approx(1)); // the sphere through the box's corners
    const model::Chunk &cap = read->chunks[1];
    CHECK(model::Floats(cap, cap.declaration[0])[1][1] == Approx(1));
    CHECK(model::Indices(cap) == std::vector<std::uint32_t>{1, 2, 2});
}

TEST_CASE("a chunk given more vertices packs the buffers again, the chunks after it moved", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabMesh();
    std::optional<gff4::File> file;
    const std::optional<model::Mesh> mesh = Read(bytes, file);
    REQUIRE(mesh);
    model::ChunkData body = AsRead(mesh->chunks[0]);
    for (std::vector<model::Value> &values : body.values)
    {
        values.push_back(values[2]);
    }
    body.values[0][3] = {2, 2, 0, 1};
    body.indices = {0, 1, 2, 0, 2, 3};
    const std::vector<model::ChunkData> given{body};
    const model::Written written = model::WriteMesh(bytes, given);
    REQUIRE(written.error.empty());

    std::optional<gff4::File> after;
    const std::optional<model::Mesh> read = Read(written.bytes, after);
    REQUIRE(read);
    REQUIRE(read->chunks.size() == 2);
    const model::Chunk &grown = read->chunks[0];
    CHECK(grown.vertexCount == 4);
    CHECK(model::Indices(grown) == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3});
    CHECK(model::Floats(grown, *grown.Find(model::Usage::Position))[3][1] == Approx(2));
    // Four vertices of 52 bytes end at 208, which the game pads to 224.
    const model::Chunk &cap = read->chunks[1];
    CHECK(cap.vertexOffset == 224);
    CHECK(cap.startIndex == 6);
    CHECK(model::Floats(cap, cap.declaration[0])[2][1] == Approx(2));
    CHECK(model::Indices(cap) == std::vector<std::uint32_t>{1, 2, 2});
}

TEST_CASE("a mesh is not written from values that do not fit it, and says why", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabMesh();
    std::optional<gff4::File> file;
    const std::optional<model::Mesh> mesh = Read(bytes, file);
    REQUIRE(mesh);
    const model::ChunkData good = AsRead(mesh->chunks[0]);

    model::ChunkData unknown = good;
    unknown.name = L"Nothing";
    model::ChunkData short_ = good;
    short_.values[1].pop_back();
    model::ChunkData past = good;
    past.indices = {0, 1, 3};
    model::ChunkData bones = good;
    bones.values[6][0] = {67, 0, 0, 0};
    model::ChunkData overlap = good;
    overlap.declaration = mesh->chunks[0].declaration;
    overlap.declaration[1].offset = 4;
    for (const model::ChunkData &bad : {unknown, short_, past, bones, overlap})
    {
        const std::vector<model::ChunkData> given{bad};
        const model::Written written = model::WriteMesh(bytes, given);
        CHECK_FALSE(written.error.empty());
        CHECK(written.bytes.empty());
    }
    const std::vector<model::ChunkData> twice{good, good};
    CHECK_FALSE(model::WriteMesh(bytes, twice).error.empty());
}

TEST_CASE("a hierarchy's nodes are changed in place and its boxes made again from the mesh", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabModel();
    CHECK(model::WriteHierarchy(bytes, {}).bytes == bytes);

    const std::vector<std::byte> meshBytes = files::SlabMesh();
    std::optional<gff4::File> meshFile;
    const std::optional<model::Mesh> mesh = Read(meshBytes, meshFile);
    REQUIRE(mesh);
    std::vector<model::NodeEdit> edits(2);
    edits[0].node = 2; // Bone1
    edits[0].translation = dao::core::Point{0, 3, 0};
    edits[1].node = 3; // Mesh1
    edits[1].material = L"ogre_red";
    edits[1].bonesUsed = std::vector<std::int32_t>{0, 1, 2};
    const model::Written written = model::WriteHierarchy(bytes, edits, {}, &*mesh);
    REQUIRE(written.error.empty());

    const std::optional<gff4::File> after = gff4::File::Parse(written.bytes);
    REQUIRE(after);
    const std::optional<model::Hierarchy> tree = model::HierarchyOf(*after);
    REQUIRE(tree);
    CHECK(tree->nodes[2].translation[1] == Approx(3));
    CHECK(tree->nodes[3].mesh->material == L"ogre_red");
    CHECK(tree->nodes[3].mesh->bonesUsed == std::vector<std::int32_t>{0, 1, 2});
    // GOB's box is Body's places, (0, 0, 0) to (2, 0, 0): Mesh1 has no transform, and GOB's own is not in it.
    const gff4::Struct gob = after->Root()->Structs(6999)[0];
    const auto children = gob.Structs(6999);
    const auto box = std::ranges::find_if(children, [](const gff4::Struct &s) { return s.Label() == "bbox"; });
    REQUIRE(box != children.end());
    CHECK(box->Floats(6054) == std::vector<float>{0, 0, 0, 1});
    CHECK(box->Floats(6055) == std::vector<float>{2, 0, 0, 1});
}

TEST_CASE("a hierarchy is not written for a node it lacks, a transform a node does not keep, or a banner", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabModel();
    std::vector<model::NodeEdit> edits(1);
    edits[0].node = 9;
    CHECK_FALSE(model::WriteHierarchy(bytes, edits).error.empty());
    edits[0].node = 0;
    edits[0].scale = 2.0F; // GOB keeps no scale
    CHECK_FALSE(model::WriteHierarchy(bytes, edits).error.empty());
    edits[0].scale.reset();
    edits[0].material = L"x"; // GOB is no mesh
    CHECK_FALSE(model::WriteHierarchy(bytes, edits).error.empty());
    edits[0].material.reset();
    edits[0].node = 3;
    edits[0].bonesUsed = std::vector<std::int32_t>(68, 0);
    CHECK_FALSE(model::WriteHierarchy(bytes, edits).error.empty());
    edits[0].bonesUsed = std::vector<std::int32_t>{3}; // the model has bones 0 to 2
    CHECK_FALSE(model::WriteHierarchy(bytes, edits).error.empty());
    CHECK_FALSE(model::WriteHierarchy(files::SlabModel(true), {}).error.empty());
}

TEST_CASE("an editor adds a struct and lengthens a list, and leaves what was there", "[model][gff4]")
{
    const std::vector<std::byte> bytes = files::SlabMesh();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file);
    gff4::Editor editor(*file);
    const gff4::Struct body = file->Root()->Structs(8021)[0];
    const std::optional<gff4::Struct> copy = editor.Add(body);
    REQUIRE(copy);
    CHECK(editor.SetNumber(*copy, 8001, 9));
    std::vector<std::byte> reference(4);
    const std::uint32_t to = editor.Reference(*copy);
    std::memcpy(reference.data(), &to, 4);
    CHECK(editor.Extend(*file->Root(), 8021, reference, 1));
    CHECK_FALSE(editor.Extend(*file->Root(), 2, reference, 1)); // a string is no list

    const std::vector<std::byte> after = editor.Bytes();
    const std::optional<gff4::File> edited = gff4::File::Parse(after);
    REQUIRE(edited);
    const std::vector<gff4::Struct> chunks = edited->Root()->Structs(8021);
    REQUIRE(chunks.size() == 3);
    CHECK(chunks[0].Get<std::uint32_t>(8001) == 3U);
    CHECK(chunks[2].Get<std::uint32_t>(8001) == 9U);
    CHECK(chunks[2].String(2) == L"Body"); // the copy's string is the original's
}

namespace
{

// A new chunk "Horn": three places (0, 0, 0), (1, 0, 0) and (0, 1, 0) as three floats, one triangle.
model::ChunkData Horn()
{
    return model::ChunkData{
        .name = L"Horn",
        .index = std::nullopt,
        .added = true,
        .declaration = {model::Element{.offset = 0, .format = model::Format::Float3, .usage = model::Usage::Position}},
        .values = {{{0, 0, 0, 1}, {1, 0, 0, 1}, {0, 1, 0, 1}}},
        .indices = {0, 1, 2}};
}

} // namespace

TEST_CASE("a new chunk is put after the original's, packed and bounded as theirs are", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabMesh();
    const std::vector<model::ChunkData> given{Horn()};
    const model::Written written = model::WriteMesh(bytes, given);
    REQUIRE(written.error.empty());

    std::optional<gff4::File> after;
    const std::optional<model::Mesh> read = Read(written.bytes, after);
    REQUIRE(read);
    REQUIRE(read->chunks.size() == 3);
    CHECK(read->chunks[0].declaration.size() == 7); // Body's own declaration, not the new chunk's
    CHECK(model::Indices(read->chunks[1]) == std::vector<std::uint32_t>{1, 2, 2});
    const model::Chunk &horn = read->chunks[2];
    CHECK(horn.name == L"Horn");
    CHECK(horn.vertexSize == 12);
    CHECK(horn.vertexCount == 3);
    CHECK(horn.vertexOffset == 208); // after Cap's 36 bytes at 160, padded
    CHECK(horn.startIndex == 6);
    CHECK(model::Indices(horn) == std::vector<std::uint32_t>{0, 1, 2});
    CHECK(model::Floats(horn, horn.declaration[0])[2][1] == Approx(1));
    const std::optional<gff4::Struct> bounds = after->Root()->Structs(8021)[2].Child(8020);
    REQUIRE(bounds);
    CHECK(bounds->Floats(8018) == std::vector<float>{1, 1, 0, 1});
    const std::optional<gff4::Struct> bodyBounds = after->Root()->Structs(8021)[0].Child(8020);
    CHECK(bodyBounds->Floats(8018) == std::vector<float>{2, 0, 0, 1}); // Body's, as it was
}

TEST_CASE("a new chunk is not written with a name taken, an index, or no place", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabMesh();
    model::ChunkData taken = Horn();
    taken.name = L"Cap";
    model::ChunkData indexed = Horn();
    indexed.index = 0;
    model::ChunkData undeclared = Horn();
    undeclared.declaration.clear();
    model::ChunkData placeless = Horn();
    placeless.declaration[0].usage = model::Usage::Normal;
    for (const model::ChunkData &bad : {taken, indexed, undeclared, placeless})
    {
        const std::vector<model::ChunkData> given{bad};
        CHECK_FALSE(model::WriteMesh(bytes, given).error.empty());
    }
    const std::vector<model::ChunkData> twice{Horn(), Horn()};
    CHECK_FALSE(model::WriteMesh(bytes, twice).error.empty());
}

TEST_CASE("a new mesh node is put under its parent, like another, and in the boxes", "[model]")
{
    const std::vector<std::byte> meshBytes = files::SlabMesh();
    const std::vector<model::ChunkData> given{Horn()};
    const model::Written mesh = model::WriteMesh(meshBytes, given);
    REQUIRE(mesh.error.empty());
    std::optional<gff4::File> meshFile;
    const std::optional<model::Mesh> written = Read(mesh.bytes, meshFile);
    REQUIRE(written);

    model::NewNode horn;
    horn.parent = 1; // Bone0, turned a quarter about z and scaled by 2
    horn.name = L"HornMesh";
    horn.translation = dao::core::Point{1, 0, 0};
    horn.mesh = model::MeshRef{.chunk = L"Horn", .material = L"horn", .part = L"_2", .bonesUsed = {1}};
    const std::vector<model::NewNode> added{horn};
    const model::Written out = model::WriteHierarchy(files::SlabModel(), {}, added, &*written);
    REQUIRE(out.error.empty());

    const std::optional<gff4::File> after = gff4::File::Parse(out.bytes);
    REQUIRE(after);
    const std::optional<model::Hierarchy> tree = model::HierarchyOf(*after);
    REQUIRE(tree);
    REQUIRE(tree->nodes.size() == 5);
    const model::Node &made = tree->nodes[3]; // after Bone0's other descendant, Bone1
    CHECK(made.name == L"HornMesh");
    CHECK(made.kind == "mshh");
    CHECK(made.parent == 1);
    CHECK(made.translation == dao::core::Point{1, 0, 0});
    REQUIRE(made.mesh);
    CHECK(made.mesh->chunk == L"Horn");
    CHECK(made.mesh->material == L"horn");
    CHECK(made.mesh->part == L"_2");
    CHECK(made.mesh->bonesUsed == std::vector<std::int32_t>{1});
    CHECK(tree->nodes[4].mesh->bonesUsed == std::vector<std::int32_t>{1, 0}); // Mesh1's, as it was
    // GOB's third child is Bone0, whose last, after its own, is the new node.
    const gff4::Struct node = after->Root()->Structs(6999)[0].Structs(6999)[2].Structs(6999).back();
    CHECK(node.Structs(6999)[0].String(6049) == L"BaseLight"); // Mesh1's attribute

    // Horn's places through its (1, 0, 0) and Bone0's turn and scale: (0, 2, 0), (0, 4, 0), (-2, 2, 0)
    // and its box's fourth corner (-2, 4, 0); with Body's (0, 0, 0) to (2, 0, 0).
    const auto children = after->Root()->Structs(6999)[0].Structs(6999);
    const auto box = std::ranges::find_if(children, [](const gff4::Struct &s) { return s.Label() == "bbox"; });
    REQUIRE(box != children.end());
    CHECK(box->Floats(6054)[0] == Approx(-2));
    CHECK(box->Floats(6054)[1] == Approx(0).margin(1e-5));
    CHECK(box->Floats(6055)[0] == Approx(2));
    CHECK(box->Floats(6055)[1] == Approx(4));
}

TEST_CASE("a new mesh node is not written like no mesh, under no node, or for no chunk", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabModel();
    const std::vector<std::byte> meshBytes = files::SlabMesh();
    std::optional<gff4::File> meshFile;
    const std::optional<model::Mesh> mesh = Read(meshBytes, meshFile);
    REQUIRE(mesh);
    model::NewNode good;
    good.parent = 0;
    good.name = L"Second";
    good.mesh = model::MeshRef{.chunk = L"Cap", .material = L"ogre", .part = L"", .bonesUsed = {0}};
    const std::vector<model::NewNode> fine{good};
    CHECK(model::WriteHierarchy(bytes, {}, fine, &*mesh).error.empty());

    model::NewNode unlike = good;
    unlike.like = 1; // Bone0 is no mesh
    model::NewNode orphan = good;
    orphan.parent = 9;
    model::NewNode chunkless = good;
    chunkless.mesh->chunk = L"Horn";
    model::NewNode boneless = good;
    boneless.mesh->bonesUsed = {3};
    for (const model::NewNode &bad : {unlike, orphan, chunkless, boneless})
    {
        const std::vector<model::NewNode> added{bad};
        CHECK_FALSE(model::WriteHierarchy(bytes, {}, added, &*mesh).error.empty());
    }
}

TEST_CASE("a new bone takes the next bone index and exports, and a new mesh node may use it", "[model]")
{
    model::NewNode bone;
    bone.parent = 1; // Bone0
    bone.name = L"HornBone";
    bone.translation = dao::core::Point{0, 0, 1};
    model::NewNode mesh;
    mesh.parent = 4; // the new bone: the model's nodes are 0 to 3
    mesh.name = L"HornMesh";
    mesh.mesh = model::MeshRef{.chunk = L"Cap", .material = L"horn", .part = L"", .bonesUsed = {3}};
    const std::vector<model::NewNode> added{bone, mesh};
    const model::Written out = model::WriteHierarchy(files::SlabModel(), {}, added);
    REQUIRE(out.error.empty());

    const std::optional<gff4::File> after = gff4::File::Parse(out.bytes);
    REQUIRE(after);
    const std::optional<model::Hierarchy> tree = model::HierarchyOf(*after);
    REQUIRE(tree);
    CHECK(tree->totalBones == 4);
    CHECK(after->Root()->Get<std::uint32_t>(6275) == 5U); // the exports, 3 and the new bone's 2
    REQUIRE(tree->nodes.size() == 6);                     // GOB, Bone0, Bone1, HornBone, HornMesh, Mesh1
    const model::Node &made = tree->nodes[3];
    CHECK(made.name == L"HornBone");
    CHECK(made.parent == 1);
    CHECK(made.bone == 3);
    CHECK(made.translation == dao::core::Point{0, 0, 1});
    CHECK(tree->bones == std::vector<int>{2, 1, -1, 3});
    CHECK(tree->nodes[4].parent == 3);
    CHECK(tree->nodes[4].mesh->bonesUsed == std::vector<std::int32_t>{3});

    const gff4::Struct node = after->Root()->Structs(6999)[0].Structs(6999)[2].Structs(6999).back();
    const std::vector<gff4::Struct> exports = node.Structs(6999);
    REQUIRE(exports.size() == 5); // its translation's export, its rotation's, its transform, HornMesh
    CHECK(exports[0].String(6052) == L"hornbone_translation");
    CHECK(exports[0].Get<std::uint32_t>(6274) == 4U);
    CHECK(exports[1].String(6052) == L"hornbone_rotation");
    CHECK(exports[1].Get<std::uint32_t>(6274) == 3U);
    // Bone0's own, as they were.
    CHECK(after->Root()->Structs(6999)[0].Structs(6999)[2].Structs(6999)[0].String(6052) == L"bone0_translation");
}

TEST_CASE("a new bone is not written like no bone, like one with no exports, or past the bones", "[model]")
{
    const std::vector<std::byte> bytes = files::SlabModel();
    model::NewNode bone;
    bone.parent = 1;
    bone.name = L"HornBone";
    model::NewNode unlike = bone;
    unlike.like = 3; // Mesh1
    model::NewNode bare = bone;
    bare.like = 2; // Bone1 exports nothing
    model::NewNode later = bone;
    later.parent = 4; // no new node before it
    for (const model::NewNode &bad : {unlike, bare, later})
    {
        const std::vector<model::NewNode> added{bad};
        CHECK_FALSE(model::WriteHierarchy(bytes, {}, added).error.empty());
    }
    model::NewNode mesh;
    mesh.name = L"HornMesh";
    mesh.mesh = model::MeshRef{.chunk = L"Cap", .material = L"horn", .part = L"", .bonesUsed = {4}};
    const std::vector<model::NewNode> past{bone, mesh}; // one new bone makes 3 the last
    CHECK_FALSE(model::WriteHierarchy(bytes, {}, past).error.empty());
}

TEST_CASE("the ogre in game/models, when there, takes a second body as a new chunk and mesh node", "[model]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "models";
    if (!std::filesystem::exists(folder / "cn_bdy_ogr_0.msh") || !std::filesystem::exists(folder / "cn_bdy_ogra_0.mmh"))
    {
        SKIP("no ogre in game/models (ghilana's extract writes the game's meshes there)");
    }
    const std::vector<std::byte> meshBytes = FromDisk(folder / "cn_bdy_ogr_0.msh");
    std::optional<gff4::File> meshFile;
    const std::optional<model::Mesh> mesh = Read(meshBytes, meshFile);
    REQUIRE(mesh);
    REQUIRE(mesh->chunks.size() == 1);
    model::ChunkData second = AsRead(mesh->chunks[0]);
    second.name = L"Second";
    second.added = true;
    second.declaration = mesh->chunks[0].declaration;
    for (model::Value &place : second.values[0]) // the declaration's first element is the place
    {
        place[0] += 2;
    }
    const std::vector<model::ChunkData> given{second};
    const model::Written written = model::WriteMesh(meshBytes, given);
    REQUIRE(written.error.empty());
    std::optional<gff4::File> afterFile;
    const std::optional<model::Mesh> after = Read(written.bytes, afterFile);
    REQUIRE(after);
    REQUIRE(after->chunks.size() == 2);
    CHECK(after->chunks[1].vertexCount == mesh->chunks[0].vertexCount);
    CHECK(model::Indices(after->chunks[1]) == model::Indices(mesh->chunks[0]));

    const std::vector<std::byte> treeBytes = FromDisk(folder / "cn_bdy_ogra_0.mmh");
    const std::optional<gff4::File> treeFile = gff4::File::Parse(treeBytes);
    REQUIRE(treeFile);
    const std::optional<model::Hierarchy> tree = model::HierarchyOf(*treeFile);
    REQUIRE(tree);
    const auto first = std::ranges::find(tree->nodes, "mshh", &model::Node::kind);
    REQUIRE(first != tree->nodes.end());
    model::NewNode node;
    node.parent = first->parent;
    node.name = L"Second";
    node.mesh = *first->mesh;
    node.mesh->chunk = L"Second";
    const std::vector<model::NewNode> added{node};
    const model::Written hierarchy = model::WriteHierarchy(treeBytes, {}, added, &*after);
    REQUIRE(hierarchy.error.empty());
    const std::optional<gff4::File> grown = gff4::File::Parse(hierarchy.bytes);
    REQUIRE(grown);
    const std::optional<model::Hierarchy> read = model::HierarchyOf(*grown);
    REQUIRE(read);
    CHECK(read->nodes.size() == tree->nodes.size() + 1);
    CHECK(std::ranges::count(read->nodes, "mshh", &model::Node::kind) == 2);

    // And a bone under its first, whose exports follow the model's 224.
    model::NewNode bone;
    bone.parent = tree->bones[0];
    bone.name = L"Extra";
    const std::vector<model::NewNode> bones{bone};
    const model::Written boned = model::WriteHierarchy(treeBytes, {}, bones);
    REQUIRE(boned.error.empty());
    const std::optional<gff4::File> boneFile = gff4::File::Parse(boned.bytes);
    REQUIRE(boneFile);
    const std::optional<model::Hierarchy> withBone = model::HierarchyOf(*boneFile);
    REQUIRE(withBone);
    CHECK(withBone->totalBones == tree->totalBones + 1);
    CHECK(boneFile->Root()->Get<std::uint32_t>(6275) == treeFile->Root()->Get<std::uint32_t>(6275).value_or(0) + 2);
    const int extra = withBone->bones.back();
    REQUIRE(extra >= 0);
    CHECK(withBone->nodes[static_cast<std::size_t>(extra)].name == L"Extra");
}

TEST_CASE("the game's meshes in game/models, when there, come back the same bytes as read", "[model]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "models";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/models in this checkout (ghilana's extract writes the game's meshes there)");
    }
    std::size_t meshes = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".msh")
        {
            continue;
        }
        const std::vector<std::byte> bytes = FromDisk(entry.path());
        std::optional<gff4::File> file;
        const std::optional<model::Mesh> mesh = Read(bytes, file);
        REQUIRE(mesh);
        std::vector<model::ChunkData> same;
        for (std::size_t i = 0; i < mesh->chunks.size(); ++i)
        {
            same.push_back(AsRead(mesh->chunks[i]));
            same.back().index = i; // a few meshes repeat a chunk's name (fxa_ent_rt_c.msh: four "root")
        }
        INFO(entry.path().filename().string());
        const model::Written written = model::WriteMesh(bytes, same);
        CHECK(written.error.empty());
        CHECK(written.bytes == bytes);
        ++meshes;
    }
    CHECK(meshes > 0);
}

TEST_CASE("a model's files are renamed: a hierarchy's name and mesh file, a mesh's name", "[model]")
{
    const model::Written tree = model::Renamed(files::SlabModel(), L"red.mmh", L"red.msh");
    REQUIRE(tree.error.empty());
    const std::optional<gff4::File> treeFile = gff4::File::Parse(tree.bytes);
    REQUIRE(treeFile);
    const std::optional<model::Hierarchy> hierarchy = model::HierarchyOf(*treeFile);
    REQUIRE(hierarchy);
    CHECK(hierarchy->name == L"red.mmh");
    CHECK(hierarchy->meshFile == L"red.msh");
    CHECK(hierarchy->nodes.size() == 4);

    const model::Written mesh = model::Renamed(files::SlabMesh(), L"red.msh");
    REQUIRE(mesh.error.empty());
    std::optional<gff4::File> meshFile;
    const std::optional<model::Mesh> read = Read(mesh.bytes, meshFile);
    REQUIRE(read);
    CHECK(read->name == L"red.msh");
    CHECK(read->chunks.size() == 2);

    CHECK_FALSE(model::Renamed(files::SlabMesh(), L"").error.empty());
    const std::vector<std::byte> notModel(16, std::byte{0});
    CHECK_FALSE(model::Renamed(notModel, L"x.msh").error.empty());
}
