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
    const model::Written written = model::WriteHierarchy(bytes, edits, &*mesh);
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
    CHECK_FALSE(model::WriteHierarchy(files::SlabModel(true), {}).error.empty());
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
