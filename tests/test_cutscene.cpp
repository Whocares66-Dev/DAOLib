// dao/core/Cutscene.h over a cutscene made here, and over the game's in
// game/cutscenes when ghilana's extract has written them there (DAOLib #170).
#include "dao/core/Cutscene.h"
#include "dao/core/Gff4.h"
#include "dao/core/Gff4Writer.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
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

// A list of floats at the data's end: its data offset.
std::uint32_t FloatList(Builder &b, const std::vector<float> &values)
{
    const std::size_t at = b.bytes.size();
    b.U32(at, static_cast<std::uint32_t>(values.size()));
    Floats(b, at + 4, values);
    return static_cast<std::uint32_t>(at - b.data);
}

// A cutscene laid out as bhmcs_intro.cut is (tools/gff4.py --templates), cut down: its header,
// one actor, and in its generic queue an animation with one curve of one vertex and a
// transition, a spoken line, a stage camera by the henchman's place as alistair_main's
// line 1351 has (DAOLib #203), and a head-tracking action laid out as the game's are (DAOLib #245).
std::vector<std::byte> CutsceneFile()
{
    constexpr std::uint16_t kStructList = gff4::kList | gff4::kStruct;
    Builder b = Start(
        "CUT ",
        {{"CUT",
          72,
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
           {5200, 6, kStructList, 36},
           {5011, 3, kStructList, 40},
           {5014, 10, 0, 44},
           {5015, 13, 0, 56}}},
         {"VERT", 8, {{5370, 8, 0, 0}, {5371, 8, 0, 4}}},
         {"TRAN", 20, {{5380, 0, 0, 0}, {5381, 1, gff4::kStruct, 4}, {5382, 1, gff4::kStruct, 12}}},
         {"CURV", 12, {{5350, 8, 0, 0}, {5351, 1, kStructList, 4}, {5352, 2, kStructList, 8}}},
         {"AANI",
          48,
          {{5300, 0, 0, 0},
           {5301, 8, 0, 4},
           {5302, 8, 0, 8},
           {5303, 3, kStructList, 12},
           {5304, 0, 0, 16},
           {5400, 14, 0, 20},
           {5401, 14, 0, 24},
           {5402, 8, 0, 28},
           {5403, 8, 0, 32},
           {5405, 0, 0, 36},
           {5410, 0, 0, 37},
           {5411, 0, 0, 38},
           {5413, 0, 0, 39},
           {5408, 8, gff4::kList, 40},
           {5409, 8, gff4::kList, 44}}},
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
          48,
          {{2, 14, 0, 0},
           {5201, 4, 0, 4},
           {5202, 14, 0, 8},
           {5206, dao::test::kGeneric, gff4::kList | gff4::kReference, 12},
           {5216, 14, 0, 16},
           {5226, 8, 0, 20},
           {5214, 10, 0, 24},
           {5213, 0, 0, 36},
           {5225, 5, 0, 40},
           {5640, 5, 0, 44}}},
         {"AJSC",
          20,
          {{5300, 0, 0, 0}, {5301, 8, 0, 4}, {5302, 8, 0, 8}, {1, 14, 0, 12}, {5570, 0, 0, 16}, {5571, 0, 0, 17}}},
         {"ACHT",
          20,
          {{5300, 0, 0, 16},
           {5301, 8, 0, 0},
           {5302, 8, 0, 4},
           {5304, 0, 0, 17},
           {5620, 4, 0, 8},
           {5621, 8, 0, 12},
           {5628, 0, 0, 18},
           {5629, 0, 0, 19}}},
         // Laid out as the real ones are not (they carry the same fields at other offsets):
         // a shake, a link, a link with nothing but the base fields, gore and the cloth switch.
         {"ASHK",
          36,
          {{5300, 0, 0, 0},
           {5301, 8, 0, 4},
           {5302, 8, 0, 8},
           {5304, 0, 0, 12},
           {5600, 0, 0, 13},
           {5605, 0, 0, 14},
           {5606, 0, 0, 15},
           {5603, 4, 0, 16},
           {5604, 8, 0, 20},
           {5607, 8, 0, 24},
           {5608, 8, 0, 28},
           {5609, 8, 0, 32}}},
         {"ALNK",
          24,
          {{5300, 0, 0, 0},
           {5301, 8, 0, 4},
           {5302, 8, 0, 8},
           {5304, 0, 0, 12},
           {5636, 0, 0, 13},
           {5637, 0, 0, 14},
           {5630, 5, 0, 16},
           {5631, 5, 0, 20}}},
         {"ALNK", 16, {{5300, 0, 0, 0}, {5301, 8, 0, 4}, {5302, 8, 0, 8}, {5304, 0, 0, 12}}},
         {"AGOR", 20, {{5300, 0, 0, 0}, {5301, 8, 0, 4}, {5302, 8, 0, 8}, {5304, 0, 0, 12}, {5750, 8, 0, 16}}},
         {"ACTP", 16, {{5300, 0, 0, 0}, {5301, 8, 0, 4}, {5302, 8, 0, 8}, {5304, 0, 0, 12}, {5700, 0, 0, 13}}}});
    const std::size_t root = Struct(b, 72);
    b.U32(root, b.String(u"bhm500ar_tower_harrowing"));
    Float(b, root + 4, 61.0F);
    b.U32(root + 8, b.String(u"gen00cs_cutscene_end"));
    b.U32(root + 12, b.String(u"bhmcs_intro.cub"));
    b.U32(root + 16, 30);
    b.U32(root + 20, kNone);
    b.U8(root + 24, 0);
    b.U8(root + 25, 255);
    b.U32(root + 28, b.String(u"gen00fl_alistair"));
    const std::uint32_t cub = b.String(u"bhmcs_intro.cub");
    const std::uint32_t dof = b.String(u"da_dof.ffx.xml");
    b.U32(root + 32, b.Numbers({cub, dof}));
    Floats(b, root + 44, {-10.5F, 39.0F, 2.0F});
    Floats(b, root + 56, {0.0F, 0.0F, 0.5F, 0.75F});
    const std::size_t lodCurves = b.bytes.size();
    b.U32(lodCurves, 2);
    const std::size_t lodX = Struct(b, 12);
    const std::size_t lodY = Struct(b, 12);
    b.U32(root + 40, static_cast<std::uint32_t>(lodCurves - b.data));
    Float(b, lodX, 1.5F);
    Float(b, lodY, -2.5F);

    const std::size_t actors = b.bytes.size();
    b.U32(actors, 1);
    const std::size_t actor = Struct(b, 48);
    b.U32(root + 36, static_cast<std::uint32_t>(actors - b.data));
    b.U32(actor, b.String(u"Jowan"));
    b.U32(actor + 4, 7);
    b.U32(actor + 8, b.String(u"tools_null"));
    b.U32(actor + 16, b.String(u"bhm_jowan"));
    Float(b, actor + 20, 1.5F);
    Floats(b, actor + 24, {1.0F, 2.0F, 3.0F});
    b.U8(actor + 36, 0xFF);
    b.U32(actor + 40, 3);
    b.U32(actor + 44, 2);

    const std::size_t animation = Struct(b, 48);
    b.U8(animation, 4);
    Float(b, animation + 4, 2.0F);
    Float(b, animation + 8, 1e10F);
    b.U8(animation + 16, 20);
    b.U32(animation + 20, b.String(u"mag_cast"));
    b.U32(animation + 24, b.String(u"bhmcs_intro.cub"));
    Float(b, animation + 28, 0.5F);
    Float(b, animation + 32, 0.25F);
    b.U8(animation + 36, 255);
    b.U8(animation + 38, 255);
    b.U32(animation + 40, FloatList(b, {0.0F, 0.0F, 0.0F, 0.0F, 0.5F, 1.0F, 2.0F, 3.0F}));
    b.U32(animation + 44, FloatList(b, {0.0F, 0.0F, 0.0F, 0.0F, 1.0F}));
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
    const std::size_t camera = Struct(b, 20);
    b.U8(camera, 24);
    Float(b, camera + 4, 0.0F);
    Float(b, camera + 8, 1e10F);
    b.U32(camera + 12, b.String(u"gen00fl_alistair"));
    b.U8(camera + 16, 0);
    b.U8(camera + 17, 255);
    const std::size_t headtrack = Struct(b, 20);
    Floats(b, headtrack, {1.0F, 4.0F});
    b.U32(headtrack + 8, 3);
    Float(b, headtrack + 12, 0.6348F);
    b.U8(headtrack + 16, 28);
    b.U8(headtrack + 18, 255);
    b.U8(headtrack + 19, 0);
    const std::size_t shake = Struct(b, 36);
    b.U8(shake + 12, 255);
    b.U8(shake, 20);
    Float(b, shake + 8, 1e10F);
    b.U8(shake + 13, 1);
    b.U8(shake + 14, 2);
    b.U8(shake + 15, 255);
    b.U32(shake + 16, 0xFFFFFFFE);
    Floats(b, shake + 20, {0.6F, 0.25F, 0.5F, 0.75F});
    const std::size_t link = Struct(b, 24);
    b.U8(link, 27);
    b.U8(link + 13, 255);
    b.U8(link + 14, 255);
    b.U32(link + 16, 4);
    b.U32(link + 20, 14);
    const std::size_t bareLink = Struct(b, 16);
    b.U8(bareLink, 27);
    const std::size_t gore = Struct(b, 20);
    b.U8(gore, 45);
    Float(b, gore + 16, 0.3F);
    const std::size_t cloth = Struct(b, 16);
    b.U8(cloth, 36);
    b.U8(cloth + 13, 255);
    const std::size_t bareGore = Struct(b, 20);
    b.U8(bareGore, 45);
    b.U32(actor + 12, Children(b, {{4, animation},
                                   {5, line},
                                   {7, camera},
                                   {8, headtrack},
                                   {9, shake},
                                   {10, link},
                                   {11, bareLink},
                                   {12, gore},
                                   {13, cloth},
                                   {12, bareGore}}));
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

TEST_CASE("a cutscene's header, an actor, and its queue of actions of nine templates", "[cutscene]")
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
    CHECK(read->henchmanTag == L"gen00fl_alistair");
    CHECK(read->resources == std::vector<std::wstring>{L"bhmcs_intro.cub", L"da_dof.ffx.xml"});
    REQUIRE(read->lodCurves.size() == 2);
    CHECK(read->lodCurves[0].base == 1.5F);
    CHECK(read->lodCurves[1].base == -2.5F);
    CHECK(read->lodOriginPosition == std::array<float, 3>{-10.5F, 39.0F, 2.0F});
    CHECK(read->lodOriginOrientation == std::array<float, 4>{0.0F, 0.0F, 0.5F, 0.75F});
    REQUIRE(read->actors.size() == 1);
    const auto &actor = read->actors[0];
    CHECK(actor.name == L"Jowan");
    CHECK(actor.id == 7);
    CHECK(actor.model == L"tools_null");
    CHECK(actor.mappingTag == L"bhm_jowan");
    CHECK(actor.scale == 1.5F);
    CHECK(actor.poseHumanoid == 0xFF);
    CHECK(actor.ambientAnim == 3);
    CHECK(actor.crustParent == 2);
    CHECK(actor.originPosition == std::array<float, 3>{1.0F, 2.0F, 3.0F});
    CHECK(actor.inventory == 0xFF); // this file has no field 5217
    REQUIRE(actor.actions.size() == 10);

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
    CHECK(animation.playGad);
    CHECK_FALSE(animation.blendGad);
    CHECK(animation.extendGad);
    CHECK_FALSE(animation.applyFutureGads);
    REQUIRE(animation.gadPositions.size() == 2);
    CHECK(animation.gadPositions[1].time == 0.5F);
    CHECK(animation.gadPositions[1].position == cutscene::Vector3{1.0F, 2.0F, 3.0F});
    REQUIRE(animation.gadOrientations.size() == 1);
    CHECK(animation.gadOrientations[0].orientation == cutscene::Quaternion{0.0F, 0.0F, 0.0F, 1.0F});
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
    CHECK_FALSE(line.stageCameraHenchman);

    const auto &camera = actor.actions[2];
    CHECK(camera.kind == "AJSC");
    CHECK(camera.type == 24);
    CHECK(camera.tag == L"gen00fl_alistair");
    CHECK_FALSE(camera.stageCameraDefault);
    CHECK(camera.stageCameraHenchman);
    CHECK(camera.lookSpeed == 1.0F); // none written, as the engine reads it

    const auto &headtrack = actor.actions[3];
    CHECK(headtrack.kind == "ACHT");
    CHECK(headtrack.type == 28);
    CHECK(headtrack.start == 1.0F);
    CHECK(headtrack.stop == 4.0F);
    CHECK(headtrack.actor == 3);
    CHECK(headtrack.lookSpeed == 0.6348F);
    CHECK(headtrack.realignStart);
    CHECK_FALSE(headtrack.realignContinue);

    // DAOLib #301.
    const auto &shake = actor.actions[4];
    CHECK(shake.kind == "ASHK");
    CHECK(shake.type == 20);
    CHECK(shake.shakeType == 1);
    CHECK(shake.shakeSeed == 0xFFFFFFFE);
    CHECK(shake.shakeFrequency == 0.6F);
    CHECK(shake.shakeNoiseType == 2);
    CHECK(shake.shakeCorrelated);
    CHECK(shake.shakeRoughness == 0.25F);
    CHECK(shake.shakeRampIn == 0.5F);
    CHECK(shake.shakeRampOut == 0.75F);
    CHECK_FALSE(animation.shakeCorrelated);
    CHECK(animation.shakeSeed == 0);

    const auto &link = actor.actions[5];
    CHECK(link.kind == "ALNK");
    CHECK(link.actor == 4);
    CHECK(link.node == 14);
    CHECK(link.linkIsTargetCrust);
    CHECK(link.linkUseOffset);
    CHECK_FALSE(headtrack.linkUseOffset);

    // The exe's constructor (0x007DB900) defaults a link's target and node to 0, not -1.
    const auto &bareLink = actor.actions[6];
    CHECK(bareLink.kind == "ALNK");
    CHECK(bareLink.actor == 0);
    CHECK(bareLink.node == 0);
    CHECK_FALSE(bareLink.linkIsTargetCrust);
    CHECK_FALSE(bareLink.linkUseOffset);

    CHECK(actor.actions[7].gore == 0.3F);
    CHECK(actor.actions[8].clothPhysics);
    CHECK(actor.actions[9].gore == 0.0F);
    CHECK_FALSE(actor.actions[9].clothPhysics);
}

TEST_CASE("a file of another type is no cutscene", "[cutscene]")
{
    Builder b = Start("PLTS", {{"PLTS", 4, {{13016, 14, 0, 0}}}});
    Struct(b, 4);
    const auto file = gff4::File::Parse(b.bytes);
    REQUIRE(file);
    CHECK_FALSE(cutscene::CutsceneOf(*file));
}

namespace
{

// What the game's cutscenes hold, counted over all of them.
struct Counts
{
    std::size_t cutscenes = 0;
    std::size_t actors = 0;
    std::size_t actions = 0;
    std::size_t curves = 0;
    std::size_t vertices = 0;
    std::size_t lines = 0;
    std::size_t ending = 0;
    std::size_t unlabelled = 0;
    std::size_t headtracks = 0;
    std::size_t fullSpeed = 0;
    std::size_t realignStarts = 0;
    std::size_t realignContinues = 0;
    std::size_t shakes = 0;
    std::size_t turnShakes = 0;
    std::size_t moveShakes = 0;
    std::size_t correlatedShakes = 0;
    std::size_t links = 0;
    std::size_t crustLinks = 0;
    std::size_t offsetLinks = 0;
    std::size_t gores = 0;
    std::size_t goreSet = 0; // a gore action whose level is not 0
    std::size_t clothSwitches = 0;
    std::size_t fogs = 0;

    // The actions of DAOLib #301: a shake, a link, gore, the cloth switch and fog.
    void AddSmallActions(const cutscene::Action &action)
    {
        if (action.kind == "ASHK")
        {
            ++shakes;
            turnShakes += action.shakeType == 1 ? 1 : 0;
            moveShakes += action.shakeType == 2 ? 1 : 0;
            correlatedShakes += action.shakeCorrelated ? 1 : 0;
        }
        if (action.kind == "ALNK")
        {
            ++links;
            crustLinks += action.linkIsTargetCrust ? 1 : 0;
            offsetLinks += action.linkUseOffset ? 1 : 0;
        }
        gores += action.kind == "AGOR" ? 1 : 0;
        goreSet += action.gore != 0.0F ? 1 : 0;
        clothSwitches += action.kind == "ACTP" ? 1 : 0;
        fogs += action.fog ? 1 : 0;
    }

    void Add(const cutscene::Action &action)
    {
        unlabelled += action.kind.empty() ? 1 : 0;
        lines += action.kind == "ASLN" && !action.voBank.empty() ? 1 : 0;
        if (action.kind == "ACHT")
        {
            ++headtracks;
            fullSpeed += action.lookSpeed == 1.0F ? 1 : 0;
            realignStarts += action.realignStart ? 1 : 0;
            realignContinues += action.realignContinue ? 1 : 0;
        }
        AddSmallActions(action);
        curves += action.curves.size();
        for (const auto &curve : action.curves)
        {
            vertices += curve.vertices.size();
        }
    }

    void Add(const cutscene::Cutscene &read)
    {
        ++cutscenes;
        ending += read.endScript.empty() ? 0 : 1;
        actors += read.actors.size();
        for (const auto &actor : read.actors)
        {
            actions += actor.actions.size();
            for (const auto &action : actor.actions)
            {
                Add(action);
            }
        }
    }
};

} // namespace

TEST_CASE("the game's cutscenes in game/cutscenes, when there, each read with its actors' actions",
          "[cutscene][game][folder]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "cutscenes";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/cutscenes (ghilana's extract of *.cut writes them there)");
    }
    Counts counts;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        const auto bytes = FromDisk(entry.path());
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const auto read = cutscene::CutsceneOf(*file);
        REQUIRE(read);
        counts.Add(*read);
        if (entry.path().filename() == "bhmcs_intro.cut")
        {
            CHECK(read->endScript == L"gen00cs_cutscene_end");
            CHECK(read->runTime == Catch::Approx(61.1667F));
            CHECK(read->area == L"bhm500ar_tower_harrowing");
            CHECK(read->actors.size() == 14);
        }
    }
    // 2026-10-18: all 3,158, as tools/gff4.py counts them.
    CHECK(counts.cutscenes == 3158);
    CHECK(counts.ending == 279);
    CHECK(counts.actors == 39803);
    CHECK(counts.actions == 246502);
    CHECK(counts.curves == 372345);
    CHECK(counts.vertices == 320650);
    CHECK(counts.lines == 3555);
    CHECK(counts.unlabelled == 0);
    // 2026-10-23, as tools/gff4.py reads them (DAOLib #245).
    CHECK(counts.headtracks == 19384);
    CHECK(counts.fullSpeed == 928); // exactly 1, the most common speed
    CHECK(counts.realignStarts == 825);
    CHECK(counts.realignContinues == 594);
    // 2026-11-02 (DAOLib #301): the templates' counts in the header of Cutscene.h; the rest as this reads them.
    CHECK(counts.shakes == 4647);
    CHECK(counts.links == 818);
    CHECK(counts.gores == 781);
    CHECK(counts.fogs == 7);
    CHECK(counts.clothSwitches == 0); // ACTP is a template no action uses
    CHECK(counts.turnShakes == 4566);
    CHECK(counts.moveShakes == 81);
    CHECK(counts.correlatedShakes == 201);
    CHECK(counts.crustLinks == 436);
    CHECK(counts.offsetLinks == 458);
    CHECK(counts.goreSet == 777); // of 781; the others are the engine's default, 0
}

namespace
{

// A curve through (1, 10), (3, 20) and (5, 0), its two transitions of one type.
cutscene::Curve ThreeVertices(cutscene::TransitionType type)
{
    const auto raw = static_cast<std::uint8_t>(type);
    return {
        .base = -1.0F,
        .vertices = {{1.0F, 10.0F}, {3.0F, 20.0F}, {5.0F, 0.0F}},
        .transitions = {{.type = raw, .control1 = {}, .control2 = {}}, {.type = raw, .control1 = {}, .control2 = {}}}};
}

// A track of three curves, each a constant.
cutscene::Action Track(std::uint8_t type, float x, float y, float z)
{
    cutscene::Action track;
    track.type = type;
    track.stop = 1e10F;
    for (const float value : {x, y, z})
    {
        track.curves.push_back({.base = value, .vertices = {}, .transitions = {}});
    }
    return track;
}

cutscene::Quaternion Product(const cutscene::Quaternion &a, const cutscene::Quaternion &b)
{
    return {(a[3] * b[0]) + (a[0] * b[3]) + (a[1] * b[2]) - (a[2] * b[1]),
            (a[3] * b[1]) - (a[0] * b[2]) + (a[1] * b[3]) + (a[2] * b[0]),
            (a[3] * b[2]) + (a[0] * b[1]) - (a[1] * b[0]) + (a[2] * b[3]),
            (a[3] * b[3]) - (a[0] * b[0]) - (a[1] * b[1]) - (a[2] * b[2])};
}

// Where an orientation sends an axis, q v q*: an orientation is checked by that.
cutscene::Vector3 Turned(const cutscene::Quaternion &q, const cutscene::Vector3 &v)
{
    const auto out = Product(Product(q, {v[0], v[1], v[2], 0.0F}), {-q[0], -q[1], -q[2], q[3]});
    return {out[0], out[1], out[2]};
}

void CheckNear(const cutscene::Vector3 &got, const cutscene::Vector3 &want)
{
    for (std::size_t i = 0; i < 3; ++i)
    {
        CHECK(got.at(i) == Catch::Approx(want.at(i)).margin(1e-4));
    }
}

} // namespace

TEST_CASE("a curve is its base with no vertices, and its end values outside them", "[cutscene]")
{
    CHECK(cutscene::ValueAt({.base = 7.5F, .vertices = {}, .transitions = {}}, 3.0F) == 7.5F);
    const auto curve = ThreeVertices(cutscene::TransitionType::Linear);
    CHECK(cutscene::ValueAt(curve, 0.0F) == 10.0F);
    CHECK(cutscene::ValueAt(curve, 1.0F) == 10.0F);
    CHECK(cutscene::ValueAt(curve, 5.0F) == 0.0F);
    CHECK(cutscene::ValueAt(curve, 9.0F) == 0.0F);
}

TEST_CASE("a curve within 1e-4 s of an inner vertex is that vertex's value", "[cutscene]")
{
    auto curve = ThreeVertices(cutscene::TransitionType::Linear);
    curve.vertices[1].value = 100.0F; // a jump the line comes nowhere near so close to it
    CHECK(cutscene::ValueAt(curve, 3.00005F) == 100.0F);
    CHECK(cutscene::ValueAt(curve, 2.99995F) == 100.0F);
    CHECK(cutscene::ValueAt(curve, 2.999F) < 100.0F);
}

TEST_CASE("a linear transition goes straight from one vertex's value to the next's", "[cutscene]")
{
    const auto curve = ThreeVertices(cutscene::TransitionType::Linear);
    CHECK(cutscene::ValueAt(curve, 2.0F) == Catch::Approx(15.0F));
    CHECK(cutscene::ValueAt(curve, 1.5F) == Catch::Approx(12.5F));
    CHECK(cutscene::ValueAt(curve, 4.5F) == Catch::Approx(5.0F));
}

TEST_CASE("a step transition holds the first vertex's value to the next vertex", "[cutscene]")
{
    const auto curve = ThreeVertices(cutscene::TransitionType::Step);
    CHECK(cutscene::ValueAt(curve, 1.5F) == 10.0F);
    CHECK(cutscene::ValueAt(curve, 2.9F) == 10.0F);
    CHECK(cutscene::ValueAt(curve, 4.0F) == 20.0F);
}

TEST_CASE("a Bezier transition's controls are absolute (time, value) points", "[cutscene]")
{
    auto curve = ThreeVertices(cutscene::TransitionType::Bezier);
    // Controls symmetric about t = 2: the curve's parameter there is one half, and its value
    // 10/8 + 3/8 30 + 3/8 30 + 20/8.
    curve.transitions[0].control1 = {1.5F, 30.0F};
    curve.transitions[0].control2 = {2.5F, 30.0F};
    CHECK(cutscene::ValueAt(curve, 2.0F) == Catch::Approx(26.25F));
    // Controls a third and two thirds of the way along the line: the curve is the line.
    curve.transitions[1].control1 = {3.0F + (2.0F / 3.0F), 20.0F - (20.0F / 3.0F)};
    curve.transitions[1].control2 = {3.0F + (4.0F / 3.0F), 20.0F - (40.0F / 3.0F)};
    CHECK(cutscene::ValueAt(curve, 3.5F) == Catch::Approx(15.0F).margin(1e-3));
    CHECK(cutscene::ValueAt(curve, 4.7F) == Catch::Approx(3.0F).margin(1e-3));
}

TEST_CASE("a transition of an unknown type is the curve's base", "[cutscene]")
{
    auto curve = ThreeVertices(cutscene::TransitionType::Linear);
    curve.transitions[0].type = 3;
    CHECK(cutscene::ValueAt(curve, 2.0F) == -1.0F);
}

TEST_CASE("the cutscene's clock shows the nearest of its frames", "[cutscene]")
{
    CHECK(cutscene::FrameTime(1.004F, 100) == Catch::Approx(1.0F));
    CHECK(cutscene::FrameTime(1.006F, 100) == Catch::Approx(1.01F));
    CHECK(cutscene::FrameTime(0.05F, 0) == Catch::Approx(2.0F / 30.0F)); // 1.5 frames of 30 round up
}

TEST_CASE("a position track is placed in its actor's origin, then in the stage", "[cutscene]")
{
    const float half = std::sqrt(0.5F);
    cutscene::Actor actor;
    actor.originPosition = {1.0F, 2.0F, 3.0F};
    actor.originOrientation = {0.0F, 0.0F, half, half}; // a quarter turn about z
    const auto track = Track(17, 1.0F, 0.0F, 0.0F);
    CheckNear(cutscene::PositionAt(actor, track, 0.0F), {1.0F, 3.0F, 3.0F});
    const cutscene::Stage stage{.position = {10.0F, 0.0F, 0.0F}, .orientation = {0.0F, 0.0F, 1.0F, 0.0F}};
    CheckNear(cutscene::PositionAt(actor, track, 0.0F, stage), {9.0F, -3.0F, 3.0F});
}

TEST_CASE("an orientation track turns by yaw about z, then pitch about x, then roll about y", "[cutscene]")
{
    const float half = std::sqrt(0.5F);
    const cutscene::Vector3 forward{0.0F, 1.0F, 0.0F};
    const cutscene::Vector3 up{0.0F, 0.0F, 1.0F};
    cutscene::Actor actor;
    actor.originOrientation = {0.0F, 0.0F, 0.0F, 1.0F};
    CheckNear(Turned(cutscene::OrientationAt(actor, Track(18, 90.0F, 0.0F, 0.0F), 0.0F), forward), {-1.0F, 0.0F, 0.0F});
    CheckNear(Turned(cutscene::OrientationAt(actor, Track(18, 0.0F, 90.0F, 0.0F), 0.0F), forward), up);
    CheckNear(Turned(cutscene::OrientationAt(actor, Track(18, 0.0F, 0.0F, 90.0F), 0.0F), up), {1.0F, 0.0F, 0.0F});
    // Yaw, then pitch: up goes to +X. Pitch before yaw would send it to -Y.
    CheckNear(Turned(cutscene::OrientationAt(actor, Track(18, 90.0F, 90.0F, 0.0F), 0.0F), up), {1.0F, 0.0F, 0.0F});
    // The stage's turn and the origin's come before the track's: two quarter turns about z.
    actor.originOrientation = {0.0F, 0.0F, half, half};
    const cutscene::Stage stage{.position = {}, .orientation = {0.0F, 0.0F, half, half}};
    CheckNear(Turned(cutscene::OrientationAt(actor, Track(18, 0.0F, 0.0F, 0.0F), 0.0F, stage), forward),
              {0.0F, -1.0F, 0.0F});
}

TEST_CASE("the mage origin's cutscenes' tracks end at their actors' final places", "[cutscene][game]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "cutscenes";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/cutscenes (ghilana's extract of *.cut writes them there)");
    }
    std::size_t placed = 0;
    for (const char *name : {"jowan_attacks.cut", "bhm500cs_harrowing.cut", "bhmcs_intro.cut"})
    {
        const auto bytes = FromDisk(folder / name);
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const auto read = cutscene::CutsceneOf(*file);
        REQUIRE(read);
        REQUIRE(read->stage.empty()); // no stage: a script's cutscene, in the world's frame
        for (const auto &actor : read->actors)
        {
            const cutscene::Action *position = nullptr;
            const cutscene::Action *orientation = nullptr;
            for (const auto &action : actor.actions)
            {
                position = action.type == 17 ? &action : position;
                orientation = action.type == 18 ? &action : orientation;
            }
            if (position == nullptr && orientation == nullptr)
            {
                continue;
            }
            INFO(name << " actor " << actor.id);
            ++placed;
            if (position != nullptr)
            {
                CheckNear(cutscene::PositionAt(actor, *position, read->runTime), actor.finalPosition);
            }
            if (orientation != nullptr)
            {
                const auto got = cutscene::OrientationAt(actor, *orientation, read->runTime);
                const auto &want = actor.finalOrientation;
                const float dot = (got[0] * want[0]) + (got[1] * want[1]) + (got[2] * want[2]) + (got[3] * want[3]);
                CHECK(std::abs(dot) == Catch::Approx(1.0F).margin(1e-4)); // q and -q are one orientation
            }
        }
    }
    CHECK(placed == 57);
}

namespace
{

// A clip with travel: playing from start to stop, its keys a position (time, x, y, z) each.
cutscene::Action Clip(float start, float stop, std::vector<cutscene::PositionKey> positions)
{
    cutscene::Action clip;
    clip.kind = "AANI";
    clip.type = 4;
    clip.start = start;
    clip.stop = stop;
    clip.playGad = true;
    clip.gadPositions = std::move(positions);
    return clip;
}

// A quarter turn about z, as a quaternion.
cutscene::Quaternion QuarterTurn()
{
    const float half = std::sqrt(0.5F);
    return {0.0F, 0.0F, half, half};
}

} // namespace

TEST_CASE("a clip's travel is its keys at the time since its start, held outside them", "[cutscene]")
{
    auto clip = Clip(2.0F, 4.0F, {{0.0F, {0.0F, 0.0F, 0.0F}}, {1.0F, {2.0F, 0.0F, 0.0F}}, {2.0F, {2.0F, 4.0F, 0.0F}}});
    clip.gadOrientations = {{0.0F, {0.0F, 0.0F, 0.0F, 1.0F}}, {2.0F, QuarterTurn()}};
    CheckNear(cutscene::TravelAt(clip, 1.0F).position, {0.0F, 0.0F, 0.0F});
    CheckNear(cutscene::TravelAt(clip, 3.0F).position, {2.0F, 0.0F, 0.0F});
    CheckNear(cutscene::TravelAt(clip, 3.5F).position, {2.0F, 2.0F, 0.0F});
    CheckNear(cutscene::TravelAt(clip, 9.0F).position, {2.0F, 4.0F, 0.0F});
    // Half of a quarter turn, and all of it.
    CheckNear(Turned(cutscene::TravelAt(clip, 3.0F).orientation, {0.0F, 1.0F, 0.0F}),
              {-std::sqrt(0.5F), std::sqrt(0.5F), 0.0F});
    CheckNear(Turned(cutscene::TravelAt(clip, 9.0F).orientation, {0.0F, 1.0F, 0.0F}), {-1.0F, 0.0F, 0.0F});
    CheckNear(cutscene::TravelAt(cutscene::Action{}, 1.0F).position, {0.0F, 0.0F, 0.0F});
}

TEST_CASE("an actor is its tracks' place with its live clips' travel added", "[cutscene]")
{
    cutscene::Actor actor;
    actor.originOrientation = {0.0F, 0.0F, 0.0F, 1.0F};
    auto track = Track(17, 1.0F, 0.0F, 0.0F);
    track.start = 0.0F;
    actor.actions.push_back(track);
    actor.actions.push_back(Clip(0.0F, 2.0F, {{0.0F, {0.0F, 0.0F, 0.0F}}, {2.0F, {0.0F, 10.0F, 0.0F}}}));
    CheckNear(cutscene::PlacementAt(actor, 1.0F).position, {1.0F, 5.0F, 0.0F});
    CheckNear(cutscene::PlacementAt(actor, 3.0F).position, {1.0F, 0.0F, 0.0F}); // over: the track alone

    actor.actions[1].extendGad = true;
    CheckNear(cutscene::PlacementAt(actor, 3.0F).position, {1.0F, 10.0F, 0.0F}); // over: its last key stays

    actor.actions[1].blendGad = true;
    CheckNear(cutscene::PlacementAt(actor, 1.0F).position, {1.0F, 0.0F, 0.0F}); // blended, with no weight: nothing
}

TEST_CASE("a live clip's travel is turned by the clips that ended before it, and the stage turns all", "[cutscene]")
{
    cutscene::Actor actor;
    actor.originOrientation = {0.0F, 0.0F, 0.0F, 1.0F};
    auto first = Clip(0.0F, 2.0F, {{0.0F, {0.0F, 0.0F, 0.0F}}, {2.0F, {0.0F, 10.0F, 0.0F}}});
    first.extendGad = true;
    first.applyFutureGads = true;
    first.gadOrientations = {{0.0F, {0.0F, 0.0F, 0.0F, 1.0F}}, {2.0F, QuarterTurn()}};
    actor.actions.push_back(first);
    actor.actions.push_back(Clip(2.0F, 4.0F, {{0.0F, {0.0F, 0.0F, 0.0F}}, {1.0F, {0.0F, 2.0F, 0.0F}}}));
    const auto placed = cutscene::PlacementAt(actor, 3.0F);
    CheckNear(placed.position, {-2.0F, 10.0F, 0.0F}); // (0, 10) from the first, (0, 2) turned to (-2, 0)
    CheckNear(Turned(placed.orientation, {0.0F, 1.0F, 0.0F}), {-1.0F, 0.0F, 0.0F});

    const cutscene::Stage stage{.position = {5.0F, 0.0F, 0.0F}, .orientation = QuarterTurn()};
    const auto staged = cutscene::PlacementAt(actor, 3.0F, stage);
    CheckNear(staged.position, {-5.0F, -2.0F, 0.0F}); // (-2, 10) turned to (-10, -2), then moved by (5, 0)
}

namespace
{

// A clip of the travel (0, 0, 0) to `end` over 0 to 2 s that is blended with a weight, as a percentage.
cutscene::Action Blended(float weight, cutscene::Vector3 end)
{
    auto clip = Clip(0.0F, 2.0F, {{0.0F, {0.0F, 0.0F, 0.0F}}, {0.1F, end}});
    clip.blendGad = true;
    cutscene::Curve curve;
    curve.base = weight;
    clip.curves.push_back(curve);
    return clip;
}

cutscene::Actor Still()
{
    cutscene::Actor actor;
    actor.originOrientation = {0.0F, 0.0F, 0.0F, 1.0F};
    return actor;
}

} // namespace

TEST_CASE("blended clips share the actor by their weights, whatever the sum", "[cutscene]")
{
    auto actor = Still();
    actor.actions.push_back(Blended(30.0F, {10.0F, 0.0F, 0.0F}));
    actor.actions.push_back(Blended(70.0F, {0.0F, 20.0F, 0.0F}));
    CheckNear(cutscene::PlacementAt(actor, 1.0F).position, {3.0F, 14.0F, 0.0F}); // 0.3 p1 + 0.7 p2

    // Weights of 3 and 7 are the same shares, and a weight over 100 is held at 100.
    actor.actions[0].curves[0].base = 3.0F;
    actor.actions[1].curves[0].base = 7.0F;
    CheckNear(cutscene::PlacementAt(actor, 1.0F).position, {3.0F, 14.0F, 0.0F});
    actor.actions[0].curves[0].base = 200.0F;
    actor.actions[1].curves[0].base = 100.0F;
    CheckNear(cutscene::PlacementAt(actor, 1.0F).position, {5.0F, 10.0F, 0.0F});
}

TEST_CASE("a lone blended clip is applied whole at any weight above 1e-6, and one of weight 0 is not", "[cutscene]")
{
    auto actor = Still();
    actor.actions.push_back(Blended(0.001F, {10.0F, 0.0F, 0.0F}));
    CheckNear(cutscene::PlacementAt(actor, 1.0F).position, {10.0F, 0.0F, 0.0F});
    actor.actions[0].curves[0].base = 0.0F;
    CheckNear(cutscene::PlacementAt(actor, 1.0F).position, {0.0F, 0.0F, 0.0F});
    actor.actions[0].curves[0].base = -5.0F; // below 0 is held at 0
    CheckNear(cutscene::PlacementAt(actor, 1.0F).position, {0.0F, 0.0F, 0.0F});
    actor.actions[0].curves[0].base = 0.00001F; // a sum of 1e-7 is under the engine's 1e-6
    CheckNear(cutscene::PlacementAt(actor, 1.0F).position, {0.0F, 0.0F, 0.0F});
}

TEST_CASE("blended clips turn the actor by a slerp from nothing to their turn by their share", "[cutscene]")
{
    auto actor = Still();
    auto first = Blended(50.0F, {0.0F, 0.0F, 0.0F});
    first.gadOrientations = {{0.0F, QuarterTurn()}};
    actor.actions.push_back(first);
    actor.actions.push_back(Blended(50.0F, {0.0F, 0.0F, 0.0F})); // a share with no turn of its own
    // Half a quarter turn.
    CheckNear(Turned(cutscene::PlacementAt(actor, 1.0F).orientation, {0.0F, 1.0F, 0.0F}),
              {-std::sqrt(0.5F), std::sqrt(0.5F), 0.0F});

    // The blend's turn goes on the left of a direct clip's: the quarter turn about x first, and the blend after.
    auto direct = Clip(0.0F, 2.0F, {{0.0F, {0.0F, 0.0F, 0.0F}}});
    const float half = std::sqrt(0.5F);
    direct.gadOrientations = {{0.0F, {half, 0.0F, 0.0F, half}}};
    actor.actions.push_back(direct);
    CheckNear(Turned(cutscene::PlacementAt(actor, 1.0F).orientation, {0.0F, 1.0F, 0.0F}), {0.0F, 0.0F, 1.0F});
}

TEST_CASE(
    "a clip's keys are read at the time since its start, whatever its StartOffset, and an ended one leaves its last",
    "[cutscene]")
{
    auto actor = Still();
    auto clip =
        Clip(1.0F, 2.0F, {{0.0F, {0.0F, 0.0F, 0.0F}}, {1.0F, {0.0F, 10.0F, 0.0F}}, {3.0F, {0.0F, 30.0F, 0.0F}}});
    clip.startOffset = 25.25F;
    clip.extendGad = true;
    actor.actions.push_back(clip);
    // Live at 1.5 s: half a second in, whatever the offset.
    CheckNear(cutscene::PlacementAt(actor, 1.5F).position, {0.0F, 5.0F, 0.0F});
    // Ended at 5 s: the last key (3 s), not the key at the clip's length (1 s).
    CheckNear(cutscene::PlacementAt(actor, 5.0F).position, {0.0F, 30.0F, 0.0F});
    CheckNear(cutscene::TravelAt(clip, 1.5F).position, {0.0F, 5.0F, 0.0F});
}

TEST_CASE("an ended blended clip stays in the blend with its weight at its stop, after the live ones", "[cutscene]")
{
    auto actor = Still();
    auto ended = Blended(30.0F, {10.0F, 0.0F, 0.0F});
    ended.extendGad = true;
    ended.stop = 1.0F;
    actor.actions.push_back(ended);
    auto live = Blended(70.0F, {0.0F, 20.0F, 0.0F});
    live.start = 1.5F;
    live.stop = 3.0F;
    actor.actions.push_back(live);
    // The clip began at 1.5 s, so the keys run from there: the key at 0.1 s is 1.6 s.
    CheckNear(cutscene::PlacementAt(actor, 2.0F).position, {3.0F, 14.0F, 0.0F});
    // Alone once the live one is over, the ended one is applied whole.
    actor.actions[1].stop = 1.8F;
    CheckNear(cutscene::PlacementAt(actor, 5.0F).position, {10.0F, 0.0F, 0.0F});
}

TEST_CASE("an ended clip's turn goes on the left, and turns later travel only if it applies future gads", "[cutscene]")
{
    auto actor = Still();
    auto turner = Clip(0.0F, 1.0F, {{0.0F, {0.0F, 0.0F, 0.0F}}});
    turner.extendGad = true;
    turner.gadOrientations = {{0.0F, QuarterTurn()}};
    actor.actions.push_back(turner);
    // A live clip, which turns the actor about x and moves it up y.
    auto live = Clip(1.0F, 3.0F, {{0.0F, {0.0F, 2.0F, 0.0F}}});
    const float half = std::sqrt(0.5F);
    live.gadOrientations = {{0.0F, {half, 0.0F, 0.0F, half}}};
    actor.actions.push_back(live);
    // ext * (track * live): the forward axis (0, 1, 0) goes about x to (0, 0, 1), then about z, which leaves it.
    CheckNear(Turned(cutscene::PlacementAt(actor, 2.0F).orientation, {0.0F, 1.0F, 0.0F}), {0.0F, 0.0F, 1.0F});
    // (track * live) * ext would take (0, 1, 0) round z to (-1, 0, 0), then about x: not that.
    // Without ApplyFutureGads the live travel is not turned by the ended clip's quarter turn.
    CheckNear(cutscene::PlacementAt(actor, 2.0F).position, {0.0F, 2.0F, 0.0F});
    actor.actions[0].applyFutureGads = true;
    CheckNear(cutscene::PlacementAt(actor, 2.0F).position, {-2.0F, 0.0F, 0.0F});
}

TEST_CASE("a blend-extended clip does not turn the clips after it, and the product puts the later on the left",
          "[cutscene]")
{
    auto actor = Still();
    auto blended = Blended(100.0F, {0.0F, 0.0F, 0.0F});
    blended.extendGad = true;
    blended.applyFutureGads = true;
    blended.stop = 1.0F;
    blended.gadOrientations = {{0.0F, QuarterTurn()}};
    actor.actions.push_back(blended);
    auto live = Clip(1.0F, 3.0F, {{0.0F, {0.0F, 2.0F, 0.0F}}});
    actor.actions.push_back(live);
    CheckNear(cutscene::PlacementAt(actor, 2.0F).position, {0.0F, 2.0F, 0.0F});

    // Two ended clips that apply future gads: a quarter turn about z, then one about x, turn the
    // third's (0, 0, 1) by `x * z` (the later on the left): z leaves it, x sends it to (0, -1, 0),
    // where `z * x` would send it to (1, 0, 0).
    auto z = Clip(0.0F, 1.0F, {{0.0F, {0.0F, 0.0F, 0.0F}}});
    z.extendGad = z.applyFutureGads = true;
    z.gadOrientations = {{0.0F, QuarterTurn()}};
    auto x = z;
    const float half = std::sqrt(0.5F);
    x.stop = 1.5F;
    x.start = 1.0F;
    x.gadOrientations = {{0.0F, {half, 0.0F, 0.0F, half}}};
    auto third = Clip(2.0F, 4.0F, {{0.0F, {0.0F, 0.0F, 1.0F}}});
    auto both = Still();
    both.actions = {x, z, third}; // the file's order is not the order they ended
    CheckNear(cutscene::PlacementAt(both, 3.0F).position, {0.0F, -1.0F, 0.0F});
}

TEST_CASE("jowan_attacks' clips carry their actors metres from FinalPos, which is the tracks' alone",
          "[cutscene][game]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "cutscenes";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/cutscenes (ghilana's extract of *.cut writes them there)");
    }
    const auto bytes = FromDisk(folder / "jowan_attacks.cut");
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto read = cutscene::CutsceneOf(*file);
    REQUIRE(read);
    std::size_t carried = 0;
    for (const auto &actor : read->actors)
    {
        bool gad = false;
        for (const auto &action : actor.actions)
        {
            gad = gad || (action.playGad && !action.blendGad && !action.gadPositions.empty());
        }
        const auto placed = cutscene::PlacementAt(actor, read->runTime);
        const double away =
            std::hypot(placed.position[0] - actor.finalPosition[0], placed.position[1] - actor.finalPosition[1]);
        INFO("actor " << actor.id);
        if (!gad)
        {
            CHECK(away < 1e-3); // no travel: the tracks, which end at FinalPos
        }
        carried += away > 0.3 ? 1 : 0;
    }
    // 2026-10-18: five actors of jowan_attacks end 0.45 to 3 m from FinalPos (lib-agent-2: FinalPos excludes the GADs).
    // 2026-11-02 (#306): one of them, actor 7, ends some 80 m away: its BlendGad clip (cs_duncan.hn_frwll01__1, a
    // constant weight of 100, StartOffset 25.25) has keys that jump from (4.4, 2.7) to (78.7, -30.0) between 2.0 and
    // 2.4 s into the clip, and the cut stops at 3.1 s into it. A lone blend clip is applied whole, so the actor
    // follows the keys, as the engine's arithmetic does; it has not been seen in play.
    CHECK(carried == 5);
}

TEST_CASE("a cutscene's unwritten FPS and AreaRequired read as the load reads them, a written FPS of 0 kept",
          "[cutscene]")
{
    {
        gff4::Writer out("CUT ", "V1.0");
        out.Root("CUT ", {{5021, gff4::Type::F32}});
        const std::vector<std::byte> bytes = out.Save();
        const auto read = cutscene::CutsceneOf(*gff4::File::Parse(bytes));
        REQUIRE(read);
        CHECK(read->fps == 30);
        CHECK(read->areaRequired == 1);
    }
    gff4::Writer out("CUT ", "V1.0");
    const gff4::StructAt root = out.Root("CUT ", {{5016, gff4::Type::U32}});
    out.U32(root, 5016, 0);
    const std::vector<std::byte> bytes = out.Save();
    const auto read = cutscene::CutsceneOf(*gff4::File::Parse(bytes));
    REQUIRE(read);
    CHECK(read->fps == 0);
}
