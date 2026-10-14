#include "dao/core/Gff4.h"
#include "dao/core/Morph.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("the chamberlain's morph in game/models, when there, names its parts' mesh nodes", "[morph]")
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
        }
        meshes += node.places.empty() ? 0 : 1;
    }
    CHECK(meshes == 3); // FaceM1, EyesM1, LashesM1
}
