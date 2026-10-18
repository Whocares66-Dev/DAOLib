// dao/core/Cutscene.h over a cutscene made here, and over the game's in
// game/cutscenes when ghilana's extract has written them there (DAOLib #170).
#include "dao/core/Cutscene.h"
#include "dao/core/Gff4.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace gff4 = dao::core::gff4;
namespace cutscene = dao::core::cutscene;
using dao::test::Builder;
using dao::test::Children;
using dao::test::Float;
using dao::test::Floats;
using dao::test::Start;
using dao::test::Struct;

namespace
{

constexpr std::uint32_t kNone = 0xFFFFFFFF; // a string slot with no string

// A cutscene laid out as bhmcs_intro.cut is (tools/gff4.py --templates), cut down: its header,
// one actor, and in its generic queue an animation with one curve of one vertex and a
// transition, and a spoken line.
std::vector<std::byte> CutsceneFile()
{
    constexpr std::uint16_t kStructList = gff4::kList | gff4::kStruct;
    Builder b =
        Start("CUT ", {{"CUT",
                        40,
                        {{3010, 14, 0, 0},
                         {5000, 8, 0, 4},
                         {5001, 14, 0, 8},
                         {5007, 14, 0, 12},
                         {5016, 4, 0, 16},
                         {5017, 14, 0, 20},
                         {5018, 0, 0, 24},
                         {5019, 0, 0, 25},
                         {5050, 14, 0, 28},
                         {5100, 14, gff4::kList, 32},
                         {5200, 6, kStructList, 36}}},
                       {"VERT", 8, {{5370, 8, 0, 0}, {5371, 8, 0, 4}}},
                       {"TRAN", 20, {{5380, 0, 0, 0}, {5381, 1, gff4::kStruct, 4}, {5382, 1, gff4::kStruct, 12}}},
                       {"CURV", 12, {{5350, 8, 0, 0}, {5351, 1, kStructList, 4}, {5352, 2, kStructList, 8}}},
                       {"AANI",
                        36,
                        {{5300, 0, 0, 0},
                         {5301, 8, 0, 4},
                         {5302, 8, 0, 8},
                         {5303, 3, kStructList, 12},
                         {5304, 0, 0, 16},
                         {5400, 14, 0, 20},
                         {5401, 14, 0, 24},
                         {5402, 8, 0, 28},
                         {5403, 8, 0, 32}}},
                       {"ASLN",
                        32,
                        {{22, 17, 0, 0},
                         {5300, 0, 0, 8},
                         {5301, 8, 0, 12},
                         {5302, 8, 0, 16},
                         {5304, 0, 0, 20},
                         {5562, 14, 0, 24},
                         {5563, 14, 0, 28}}},
                       {"ACTR",
                        36,
                        {{2, 14, 0, 0},
                         {5201, 4, 0, 4},
                         {5202, 14, 0, 8},
                         {5206, dao::test::kGeneric, gff4::kList | gff4::kReference, 12},
                         {5216, 14, 0, 16},
                         {5226, 8, 0, 20},
                         {5214, 10, 0, 24}}}});
    const std::size_t root = Struct(b, 40);
    b.U32(root, b.String(u"bhm500ar_tower_harrowing"));
    Float(b, root + 4, 61.0F);
    b.U32(root + 8, b.String(u"gen00cs_cutscene_end"));
    b.U32(root + 12, b.String(u"bhmcs_intro.cub"));
    b.U32(root + 16, 30);
    b.U32(root + 20, kNone);
    b.U8(root + 24, 0);
    b.U8(root + 25, 255);
    b.U32(root + 28, kNone);
    const std::uint32_t cub = b.String(u"bhmcs_intro.cub");
    const std::uint32_t dof = b.String(u"da_dof.ffx.xml");
    b.U32(root + 32, b.Numbers({cub, dof}));

    const std::size_t actors = b.bytes.size();
    b.U32(actors, 1);
    const std::size_t actor = Struct(b, 36);
    b.U32(root + 36, static_cast<std::uint32_t>(actors - b.data));
    b.U32(actor, b.String(u"Jowan"));
    b.U32(actor + 4, 7);
    b.U32(actor + 8, b.String(u"tools_null"));
    b.U32(actor + 16, b.String(u"bhm_jowan"));
    Float(b, actor + 20, 1.5F);
    Floats(b, actor + 24, {1.0F, 2.0F, 3.0F});

    const std::size_t animation = Struct(b, 36);
    b.U8(animation, 4);
    Float(b, animation + 4, 2.0F);
    Float(b, animation + 8, 1e10F);
    b.U8(animation + 16, 20);
    b.U32(animation + 20, b.String(u"mag_cast"));
    b.U32(animation + 24, b.String(u"bhmcs_intro.cub"));
    Float(b, animation + 28, 0.5F);
    Float(b, animation + 32, 0.25F);
    const std::size_t curves = b.bytes.size();
    b.U32(curves, 1);
    const std::size_t curve = Struct(b, 12);
    b.U32(animation + 12, static_cast<std::uint32_t>(curves - b.data));
    Float(b, curve, -6.5F);
    const std::size_t vertices = b.bytes.size();
    b.U32(vertices, 1);
    const std::size_t vertex = Struct(b, 8);
    b.U32(curve + 4, static_cast<std::uint32_t>(vertices - b.data));
    Floats(b, vertex, {41.5F, -20.25F});
    const std::size_t transitions = b.bytes.size();
    b.U32(transitions, 1);
    const std::size_t transition = Struct(b, 20);
    b.U32(curve + 8, static_cast<std::uint32_t>(transitions - b.data));
    b.U8(transition, 1);
    Floats(b, transition + 4, {41.75F, -20.0F, 42.0F, -10.0F});

    const std::size_t line = Struct(b, 32);
    b.U32(line, 6081234);
    b.U32(line + 4, 0);
    b.U8(line + 8, 23);
    Float(b, line + 12, 3.0F);
    Float(b, line + 16, 5.5F);
    b.U8(line + 20, 255);
    b.U32(line + 24, b.String(u"bhm_jowan_lip"));
    b.U32(line + 28, b.String(u"vo_bhm_jowan"));
    b.U32(actor + 12, Children(b, {{4, animation}, {5, line}}));
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

TEST_CASE("a cutscene's header, an actor, and its queue of actions of two templates", "[cutscene]")
{
    const auto bytes = CutsceneFile();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto read = cutscene::CutsceneOf(*file);
    REQUIRE(read);
    CHECK(read->area == L"bhm500ar_tower_harrowing");
    CHECK(read->runTime == 61.0F);
    CHECK(read->endScript == L"gen00cs_cutscene_end");
    CHECK(read->blendTree == L"bhmcs_intro.cub");
    CHECK(read->fps == 30);
    CHECK(read->stage.empty());
    CHECK(read->playUntilVoCompletes == 0);
    CHECK(read->areaRequired == 255);
    CHECK(read->resources == std::vector<std::wstring>{L"bhmcs_intro.cub", L"da_dof.ffx.xml"});
    REQUIRE(read->actors.size() == 1);
    const auto &actor = read->actors[0];
    CHECK(actor.name == L"Jowan");
    CHECK(actor.id == 7);
    CHECK(actor.model == L"tools_null");
    CHECK(actor.mappingTag == L"bhm_jowan");
    CHECK(actor.scale == 1.5F);
    CHECK(actor.originPosition == std::array<float, 3>{1.0F, 2.0F, 3.0F});
    REQUIRE(actor.actions.size() == 2);

    const auto &animation = actor.actions[0];
    CHECK(animation.kind == "AANI");
    CHECK(animation.type == 4);
    CHECK(animation.start == 2.0F);
    CHECK(animation.stop == 1e10F);
    CHECK(animation.category == 20);
    CHECK(animation.animation == L"mag_cast");
    CHECK(animation.blendTree == L"bhmcs_intro.cub");
    CHECK(animation.speed == 0.5F);
    CHECK(animation.startOffset == 0.25F);
    CHECK(animation.voBank.empty());
    CHECK(animation.actor == -1);
    CHECK_FALSE(animation.fog);
    REQUIRE(animation.curves.size() == 1);
    const auto &curve = animation.curves[0];
    CHECK(curve.base == -6.5F);
    REQUIRE(curve.vertices.size() == 1);
    CHECK(curve.vertices[0].time == 41.5F);
    CHECK(curve.vertices[0].value == -20.25F);
    REQUIRE(curve.transitions.size() == 1);
    CHECK(curve.transitions[0].type == 1);
    CHECK(curve.transitions[0].control1.time == 41.75F);
    CHECK(curve.transitions[0].control2.value == -10.0F);

    const auto &line = actor.actions[1];
    CHECK(line.kind == "ASLN");
    CHECK(line.type == 23);
    CHECK(line.stop == 5.5F);
    CHECK(line.category == 255);
    CHECK(line.text.strref == 6081234);
    CHECK(line.lipsync == L"bhm_jowan_lip");
    CHECK(line.voBank == L"vo_bhm_jowan");
    CHECK(line.animation.empty());
    CHECK(line.curves.empty());
}

TEST_CASE("a file of another type is no cutscene", "[cutscene]")
{
    Builder b = Start("PLTS", {{"PLTS", 4, {{13016, 14, 0, 0}}}});
    Struct(b, 4);
    const auto file = gff4::File::Parse(b.bytes);
    REQUIRE(file);
    CHECK_FALSE(cutscene::CutsceneOf(*file));
}

TEST_CASE("the game's cutscenes in game/cutscenes, when there, each read with its actors' actions", "[cutscene][game]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "cutscenes";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/cutscenes (ghilana's extract of *.cut writes them there)");
    }
    std::size_t cutscenes = 0;
    std::size_t actors = 0;
    std::size_t actions = 0;
    std::size_t curves = 0;
    std::size_t vertices = 0;
    std::size_t lines = 0;
    std::size_t ending = 0;
    std::size_t unlabelled = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        const auto bytes = FromDisk(entry.path());
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const auto read = cutscene::CutsceneOf(*file);
        REQUIRE(read);
        ++cutscenes;
        ending += read->endScript.empty() ? 0 : 1;
        actors += read->actors.size();
        for (const auto &actor : read->actors)
        {
            actions += actor.actions.size();
            for (const auto &action : actor.actions)
            {
                unlabelled += action.kind.empty() ? 1 : 0;
                lines += action.kind == "ASLN" && !action.voBank.empty() ? 1 : 0;
                curves += action.curves.size();
                for (const auto &curve : action.curves)
                {
                    vertices += curve.vertices.size();
                }
            }
        }
        if (entry.path().filename() == "bhmcs_intro.cut")
        {
            CHECK(read->endScript == L"gen00cs_cutscene_end");
            CHECK(read->runTime == Catch::Approx(61.1667F));
            CHECK(read->area == L"bhm500ar_tower_harrowing");
            CHECK(read->actors.size() == 14);
        }
    }
    // 2026-10-18: all 3,158, as tools/gff4.py counts them.
    CHECK(cutscenes == 3158);
    CHECK(ending == 279);
    CHECK(actors == 39803);
    CHECK(actions == 246502);
    CHECK(curves == 372345);
    CHECK(vertices == 320650);
    CHECK(lines == 3555);
    CHECK(unlabelled == 0);
}
