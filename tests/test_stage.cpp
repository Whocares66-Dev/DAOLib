// dao/core/Stage.h over a stage made here, laid out as the game's are, and over the game's
// in game/stages when ghilana's extract has written them there (DAOLib #196).
#include "dao/core/Gff4.h"
#include "dao/core/Stage.h"
#include "gff4_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace gff4 = dao::core::gff4;
namespace stage = dao::core::stage;
using dao::test::Builder;
using dao::test::Float;
using dao::test::Floats;
using dao::test::Start;
using dao::test::Struct;

namespace
{

constexpr std::uint16_t kStructList = gff4::kList | gff4::kStruct;

// A stage with the templates of the game's 617 (tools/gff4.py --templates): two places and
// one camera, which shows two places.
std::vector<std::byte> StageFile()
{
    Builder b = Start("STG ", {{"STAG", 8, {{11000, 1, kStructList, 0}, {11001, 2, kStructList, 4}}},
                               {"PLAC", 0x30, {{1, 14, 0, 32}, {4, 10, 0, 0}, {5, 13, 0, 16}, {11004, 14, 0, 36}}},
                               {"CAMR",
                                0x40,
                                {{1, 14, 0, 32},
                                 {4, 10, 0, 0},
                                 {5, 13, 0, 16},
                                 {11002, 14, gff4::kList, 40},
                                 {11003, 8, 0, 36},
                                 {11006, 14, 0, 44},
                                 {11007, 14, 0, 48},
                                 {11008, 14, 0, 52},
                                 {11009, 0, 0, 56}}}});
    const std::size_t root = Struct(b, 8);

    const std::size_t places = b.bytes.size();
    b.U32(places, 2);
    const std::size_t first = Struct(b, 0x30);
    const std::size_t second = Struct(b, 0x30);
    b.U32(root, static_cast<std::uint32_t>(places - b.data));
    Floats(b, first, {-3.0F, 0.25F, 0.5F});
    Floats(b, first + 16, {0.0F, 0.0F, -0.5F, 0.75F});
    b.U32(first + 32, b.String(u"duncan"));
    b.U32(first + 36, b.String(u"group"));
    Floats(b, second, {1.0F, 2.0F, 3.0F});
    Floats(b, second + 16, {0.0F, 0.0F, 0.0F, 1.0F});
    b.U32(second + 32, b.String(u"irving"));
    b.U32(second + 36, b.String(u"cam3_1_1"));

    const std::size_t cameras = b.bytes.size();
    b.U32(cameras, 1);
    const std::size_t camera = Struct(b, 0x40);
    b.U32(root + 4, static_cast<std::uint32_t>(cameras - b.data));
    Floats(b, camera, {0.5F, -7.5F, 1.5F});
    Floats(b, camera + 16, {-0.25F, -0.125F, 0.0625F, 0.5F});
    b.U32(camera + 32, b.String(u"group"));
    Float(b, camera + 36, 45.0F);
    const std::uint32_t duncan = b.String(u"duncan");
    const std::uint32_t irving = b.String(u"irving");
    b.U32(camera + 40, b.Numbers({duncan, irving}));
    b.U32(camera + 44, b.String(u"duncan"));
    b.U32(camera + 48, b.String(u"irving"));
    b.U32(camera + 52, b.String(u"greagoir"));
    b.U8(camera + 56, 3);
    return b.bytes;
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

TEST_CASE("a stage's places and cameras, each field from its own label", "[stage]")
{
    const auto bytes = StageFile();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto read = stage::StageOf(*file);
    REQUIRE(read);
    REQUIRE(read->places.size() == 2);
    CHECK(read->places[0].tag == L"duncan");
    CHECK(read->places[0].position == stage::Vector3{-3.0F, 0.25F, 0.5F});
    CHECK(read->places[0].orientation == stage::Quaternion{0.0F, 0.0F, -0.5F, 0.75F});
    CHECK(read->places[0].defaultCamera == L"group");
    CHECK(read->places[1].tag == L"irving");
    CHECK(read->places[1].position == stage::Vector3{1.0F, 2.0F, 3.0F});
    CHECK(read->places[1].defaultCamera == L"cam3_1_1");

    REQUIRE(read->cameras.size() == 1);
    const auto &camera = read->cameras[0];
    CHECK(camera.tag == L"group");
    CHECK(camera.position == stage::Vector3{0.5F, -7.5F, 1.5F});
    CHECK(camera.orientation == stage::Quaternion{-0.25F, -0.125F, 0.0625F, 0.5F});
    CHECK(camera.placesInShot == std::vector<std::wstring>{L"duncan", L"irving"});
    CHECK(camera.fov == 45.0F);
    CHECK(camera.lookingFrom == L"duncan");
    CHECK(camera.lookingAtPrimary == L"irving");
    CHECK(camera.lookingAtSecondary == L"greagoir");
    CHECK(camera.lookingAtType == 3);
}

TEST_CASE("a place and a camera are found by their tags", "[stage]")
{
    const auto bytes = StageFile();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto read = stage::StageOf(*file);
    REQUIRE(read);
    const stage::Place *place = stage::FindPlace(*read, L"irving");
    REQUIRE(place != nullptr);
    CHECK(place->position[2] == 3.0F);
    CHECK(stage::FindPlace(*read, L"group") == nullptr); // a camera's tag is no place's
    const stage::Camera *camera = stage::FindCamera(*read, L"group");
    REQUIRE(camera != nullptr);
    CHECK(camera->fov == 45.0F);
    CHECK(stage::FindCamera(*read, L"duncan") == nullptr);
}

TEST_CASE("a file of another type is no stage", "[stage]")
{
    Builder b = Start("PLTS", {{"PLTS", 4, {{13016, 14, 0, 0}}}});
    Struct(b, 4);
    const auto file = gff4::File::Parse(b.bytes);
    REQUIRE(file);
    CHECK_FALSE(stage::StageOf(*file));
}

TEST_CASE("the game's stages in game/stages, when there, each read, jowan_attack as it is", "[stage][game][folder]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "stages";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/stages (ghilana's extract of *.stg writes them there)");
    }
    std::size_t stages = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        const auto bytes = FromDisk(entry.path());
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const auto read = stage::StageOf(*file);
        REQUIRE(read);
        for (const auto &place : read->places)
        {
            CHECK_FALSE(place.tag.empty());
        }
        for (const auto &camera : read->cameras)
        {
            CHECK_FALSE(camera.tag.empty());
            CHECK(camera.fov > 0.0F);
        }
        ++stages;
    }
    CHECK(stages > 0);

    const auto bytes = FromDisk(folder / "jowan_attack.stg");
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto read = stage::StageOf(*file);
    REQUIRE(read);
    CHECK(read->places.size() == 14);
    CHECK(read->cameras.size() == 15);
    const stage::Place *duncan = stage::FindPlace(*read, L"duncan");
    REQUIRE(duncan != nullptr);
    CHECK(duncan->position[0] == -3.0298638343811035F);
    CHECK(duncan->orientation[2] == -0.6259233951568604F);
    CHECK(duncan->defaultCamera == L"group");
    const stage::Camera *group = stage::FindCamera(*read, L"group");
    REQUIRE(group != nullptr);
    CHECK(group->position[1] == -7.360930442810059F);
    CHECK(group->fov == 45.0F);
    CHECK(group->lookingAtPrimary == L"greagoir");
    CHECK(group->lookingAtType == 3);
    CHECK(group->placesInShot.empty()); // the Circle's cameras list none
    const stage::Camera *close = stage::FindCamera(*read, L"cam2_3_0");
    REQUIRE(close != nullptr);
    CHECK(close->fov == 25.0F);
    CHECK(close->lookingAtPrimary == L"jowan");
}
