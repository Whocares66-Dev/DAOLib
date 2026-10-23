#include "dao/core/Gff4.h"
#include "dao/core/Morph.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
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
namespace morph = dao::core::morph;

namespace
{

// A morph laid out as hm_arl210cr_chamberlain.mor is: parts "Head", an empty slot, "Lashes"; a
// tint "tint_a"; the bone "brow" moved by (0.1, 0.2, 0.3), and the mesh node "FaceM1" with its two
// vertices at (1, 2, 3, 1) and (4, 5, 6, 1).
std::vector<std::byte> Chamberlain()
{
    dao::test::Builder b = dao::test::Start(
        "MORP", {{"morp",
                  16,
                  {{2, 14, 0, 0},
                   {23000, 14, gff4::kList, 4},
                   {23001, 14, gff4::kList, 8},
                   {23002, 1, gff4::kList | gff4::kStruct, 12}}},
                 {"node", 48, {{2, 14, 0, 16}, {4, 10, 0, 0}, {8013, 4, 0, 24}, {8024, 8, gff4::kList, 20}}}});
    const std::size_t root = dao::test::Struct(b, 16);
    b.U32(root, b.String(u"chamberlain"));
    b.U32(root + 4, b.Numbers({b.String(u"Head"), 0xFFFFFFFF, b.String(u"Lashes")}));
    b.U32(root + 8, b.Numbers({b.String(u"tint_a")}));
    const std::size_t list = b.bytes.size();
    b.U32(list, 2);
    const std::size_t brow = dao::test::Struct(b, 48);
    const std::size_t face = dao::test::Struct(b, 48);
    b.U32(root + 12, static_cast<std::uint32_t>(list - b.data));
    b.U32(brow + 16, b.String(u"brow"));
    dao::test::Floats(b, brow, {0.1F, 0.2F, 0.3F});
    b.U32(brow + 20, 0xFFFFFFFF);
    b.U32(face + 16, b.String(u"FaceM1"));
    b.U32(face + 24, 2);
    std::vector<std::uint32_t> places;
    for (const float f : {1.0F, 2.0F, 3.0F, 1.0F, 4.0F, 5.0F, 6.0F, 1.0F})
    {
        places.push_back(std::bit_cast<std::uint32_t>(f));
    }
    b.U32(face + 20, b.Numbers(places));
    return b.bytes;
}

} // namespace

TEST_CASE("a morph's parts, tints and nodes are read, a mesh node's places by vertex", "[morph]")
{
    const std::vector<std::byte> bytes = Chamberlain();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const std::optional<morph::Morph> read = morph::MorphOf(*file);
    REQUIRE(read);
    CHECK(read->name == L"chamberlain");
    CHECK(read->parts == std::vector<std::wstring>{L"Head", L"", L"Lashes"});
    CHECK(read->tints == std::vector<std::wstring>{L"tint_a"});
    REQUIRE(read->nodes.size() == 2);
    CHECK(read->nodes[0].name == L"brow");
    CHECK(read->nodes[0].position[1] == Approx(0.2));
    CHECK(read->nodes[0].places.empty());
    CHECK(read->nodes[1].name == L"FaceM1");
    REQUIRE(read->nodes[1].places.size() == 2);
    CHECK(read->nodes[1].places[1][2] == Approx(6));
}

TEST_CASE("a file that is no morph is not read as one", "[morph]")
{
    dao::test::Builder b = dao::test::Start("MMH ", {{"mdlh", 4, {{2, 14, 0, 0}}}});
    dao::test::Struct(b, 4);
    const std::optional<gff4::File> file = gff4::File::Parse(b.bytes);
    REQUIRE(file);
    CHECK_FALSE(morph::MorphOf(*file));
}

namespace
{

// An empty morph with the game's five templates, as hm_pcc_b01.mor has them (tools/gff4.py --templates):
// a base for Write.
std::vector<std::byte> GameTemplatesBase()
{
    const std::uint16_t inPlace = gff4::kList | gff4::kStruct;
    dao::test::Builder b = dao::test::Start(
        "MORP", {{"morp",
                  16,
                  {{2, 14, 0, 0}, {23000, 14, gff4::kList, 4}, {23001, 14, gff4::kList, 8}, {23002, 4, inPlace, 12}}},
                 {"tprm", 8, {{2, 14, 0, 0}, {23003, 14, 0, 4}}},
                 {"vprm", 8, {{2, 14, 0, 0}, {17, 12, gff4::kList, 4}}},
                 {"fprm", 8, {{2, 14, 0, 0}, {23007, 8, 0, 4}}},
                 {"node",
                  48,
                  {{2, 14, 0, 16},
                   {4, 10, 0, 0},
                   {8013, 4, 0, 24},
                   {8024, 8, gff4::kList, 20},
                   {23004, 1, inPlace, 28},
                   {23005, 2, inPlace, 32},
                   {23006, 3, inPlace, 36}}}});
    const std::size_t root = dao::test::Struct(b, 16);
    b.U32(root, b.String(u"base"));
    b.U32(root + 4, 0xFFFFFFFF);
    b.U32(root + 8, 0xFFFFFFFF);
    b.U32(root + 12, 0xFFFFFFFF);
    return b.bytes;
}

morph::Morph MadeHead()
{
    morph::Morph made;
    made.name = L"hero";
    made.parts = {L"hm_uhm_basa", L"", L"hm_ulm_eyea"};
    made.tints = {L"tint_skin"};
    morph::Node brow;
    brow.name = L"brow";
    brow.position = {0.1F, -0.2F, 0.3F};
    morph::Node face;
    face.name = L"FaceM1";
    face.places = {{1, 2, 3, 1}, {4, 5, 6, 1}, {7, 8, 9, 1}};
    face.textures = {{L"mml_tDiffuse", L"uh_hed_masa_0d"}, {L"mml_tNormalMap", L"uh_hed_masa_0n"}};
    face.vectors = {{L"mml_vTintMaskColours", {{0.5F, 0.25F, 0.125F, 1}, {1, 0, 0, 1}}}};
    face.floats = {{L"mml_fAge", 0.75F}};
    made.nodes = {brow, face};
    return made;
}

} // namespace

TEST_CASE("a morph written into a copy of a game's .mor reads back as it was given", "[morph]")
{
    const std::vector<std::byte> base = GameTemplatesBase();
    const std::optional<gff4::File> file = gff4::File::Parse(base);
    REQUIRE(file);
    const morph::Morph made = MadeHead();
    const auto written = morph::Write(*file, made);
    REQUIRE(written);
    const std::optional<gff4::File> again = gff4::File::Parse(*written);
    REQUIRE(again);
    const std::optional<morph::Morph> read = morph::MorphOf(*again);
    REQUIRE(read);
    CHECK(read->name == L"hero");
    CHECK(read->parts == made.parts);
    CHECK(read->tints == made.tints);
    REQUIRE(read->nodes.size() == 2);
    CHECK(read->nodes[0].name == L"brow");
    CHECK(read->nodes[0].position == made.nodes[0].position);
    CHECK(read->nodes[0].places.empty());
    CHECK(read->nodes[0].textures.empty());
    const morph::Node &face = read->nodes[1];
    CHECK(face.places == made.nodes[1].places);
    REQUIRE(face.textures.size() == 2);
    CHECK(face.textures[1].name == L"mml_tNormalMap");
    CHECK(face.textures[1].texture == L"uh_hed_masa_0n");
    REQUIRE(face.vectors.size() == 1);
    CHECK(face.vectors[0].name == L"mml_vTintMaskColours");
    CHECK(face.vectors[0].values == made.nodes[1].vectors[0].values);
    REQUIRE(face.floats.size() == 1);
    CHECK(face.floats[0].name == L"mml_fAge");
    CHECK(face.floats[0].value == Approx(0.75F));

    // Written again over its own result with fewer nodes, the rest go.
    morph::Morph fewer = made;
    fewer.nodes.resize(1);
    const auto rewritten = morph::Write(*again, fewer);
    REQUIRE(rewritten);
    const std::optional<gff4::File> third = gff4::File::Parse(*rewritten);
    REQUIRE(third);
    const std::optional<morph::Morph> readFewer = morph::MorphOf(*third);
    REQUIRE(readFewer);
    REQUIRE(readFewer->nodes.size() == 1);
    CHECK(readFewer->nodes[0].name == L"brow");
}

TEST_CASE("a morph is not written over a file that is no morph, or one whose nodes lack a field it needs", "[morph]")
{
    dao::test::Builder b = dao::test::Start("MMH ", {{"mdlh", 4, {{2, 14, 0, 0}}}});
    dao::test::Struct(b, 4);
    const std::optional<gff4::File> model = gff4::File::Parse(b.bytes);
    REQUIRE(model);
    CHECK_FALSE(morph::Write(*model, MadeHead()));
    // The chamberlain-like file's nodes have no parameters' lists.
    const std::vector<std::byte> bytes = Chamberlain();
    const std::optional<gff4::File> bare = gff4::File::Parse(bytes);
    REQUIRE(bare);
    CHECK_FALSE(morph::Write(*bare, MadeHead()));
}

TEST_CASE("hm_pcc_b01.mor in game/heads, when there, written over itself reads back the same", "[morph]")
{
    const std::filesystem::path path =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "heads" / "hm_pcc_b01.mor";
    if (!std::filesystem::exists(path))
    {
        SKIP("no hm_pcc_b01.mor in game/heads (ghilana's extract writes it there)");
    }
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes(chars.size());
    std::memcpy(bytes.data(), chars.data(), chars.size());
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const std::optional<morph::Morph> before = morph::MorphOf(*file);
    REQUIRE(before);
    const auto written = morph::Write(*file, *before);
    REQUIRE(written);
    const std::optional<gff4::File> again = gff4::File::Parse(*written);
    REQUIRE(again);
    const std::optional<morph::Morph> after = morph::MorphOf(*again);
    REQUIRE(after);
    CHECK(after->name == before->name);
    CHECK(after->parts == before->parts);
    CHECK(after->tints == before->tints);
    REQUIRE(after->nodes.size() == before->nodes.size());
    for (std::size_t i = 0; i < before->nodes.size(); ++i)
    {
        const morph::Node &a = after->nodes[i];
        const morph::Node &b = before->nodes[i];
        CHECK(a.name == b.name);
        CHECK(a.position == b.position);
        CHECK(a.places == b.places);
        CHECK(a.textures.size() == b.textures.size());
        CHECK(a.vectors.size() == b.vectors.size());
        CHECK(a.floats.size() == b.floats.size());
    }
}

TEST_CASE("the chamberlain's morph in game/models, when there, names its parts' mesh nodes and their materials' values",
          "[morph]")
{
    const std::filesystem::path path =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "models" / "hm_arl210cr_chamberlain.mor";
    if (!std::filesystem::exists(path))
    {
        SKIP("no hm_arl210cr_chamberlain.mor in game/models (ghilana's extract writes it there)");
    }
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(chars.size());
    std::memcpy(bytes.data(), chars.data(), chars.size());
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const std::optional<morph::Morph> read = morph::MorphOf(*file);
    REQUIRE(read);
    CHECK(read->parts.size() == 6);
    CHECK(read->parts[0] == L"HM_UHM_BASa_0");
    CHECK(read->parts[5] == L"hm_ulm_basa_0"); // after two empty slots
    std::size_t meshes = 0;
    for (const morph::Node &node : read->nodes)
    {
        if (node.name == L"FaceM1")
        {
            CHECK(node.places.size() == 2282); // HM_UHM_BAS_FaceM1's vertices
            // Its material's textures and values in place of the .mao's (DAOLib #158).
            REQUIRE(node.textures.size() == 4);
            CHECK(node.textures[0].name == L"mml_tDiffuse");
            CHECK(node.textures[0].texture == L"uh_hed_olda_0d");
            REQUIRE(node.vectors.size() == 5);
            CHECK(node.vectors[1].name == L"mml_vTintMaskColours");
            REQUIRE(node.vectors[1].values.size() == 10);
            CHECK(node.vectors[1].values[1][3] == Approx(1.464F));
            CHECK(node.vectors[0].values[0] == std::array<float, 4>{0.08F, 0.04F, 0.05F, 1.0F}); // g_vFacialHairColour
        }
        if (node.name == L"LashesM1")
        {
            REQUIRE(node.vectors.size() == 1);
            CHECK(node.vectors[0].values.size() == 8);
            CHECK(node.textures.empty());
        }
        meshes += node.places.empty() ? 0 : 1;
    }
    CHECK(meshes == 3); // FaceM1, EyesM1, LashesM1
}
