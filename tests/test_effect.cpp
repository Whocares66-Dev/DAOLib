// dao/core/Effect.h over an effect made here, laid out as fxc_wisp_c.mmh is
// (tools/gff4.py --templates), and over the game's effects in game/effects
// when ghilana's extract has written them there (DAOLib #181).
#include "dao/core/Clip.h"
#include "dao/core/Effect.h"
#include "dao/core/GdaFile.h"
#include "dao/core/GdaHash.h"
#include "dao/core/Gff4.h"
#include "dao/core/M2da.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using Catch::Approx;
namespace clip = dao::core::clip;
namespace effect = dao::core::effect;
namespace gff4 = dao::core::gff4;
namespace m2da = dao::core::m2da;
using dao::test::Builder;
using dao::test::Children;
using dao::test::Float;
using dao::test::Floats;
using dao::test::kGeneric;
using dao::test::Start;
using dao::test::Struct;

namespace
{

// A label no reader knows, kept as it is.
constexpr std::uint32_t kUnknown = 9999;

// An effect as fxc_wisp_c.mmh is: GOB, with a bounding box, an attribute nothing reads and the
// crust hook 1, which holds the emitter "glow" and the light "Light_c". The emitter exports
// glow_birthrate (tag 8) and has two age keys and a mesh spawn volume of three points, kept as the
// game keeps a Vector3 list, 16 bytes each; the light exports light_c_radius (tag 25).
std::vector<std::byte> EffectFile()
{
    const std::uint16_t children = gff4::kList | gff4::kReference;
    Builder b = Start(
        "MMH ",
        {{"mdlh",
          32,
          {{6000, 14, 0, 0},
           {6005, 14, 0, 4},
           {6256, 4, 0, 8},
           {6999, kGeneric, children, 12},
           {6275, 4, 0, 16},
           {6333, 8, 0, 20},
           {6327, 14, 0, 24},
           {6331, 8, 0, 28}}},
         {"node", 12, {{6000, 14, 0, 0}, {6254, 5, 0, 4}, {6999, kGeneric, children, 8}}},
         {"crst", 12, {{6000, 14, 0, 0}, {6235, 0, 0, 4}, {6999, kGeneric, children, 8}}},
         {"nemt",
          60,
          {{6000, 14, 0, 0},
           {6001, 14, 0, 4},
           {6002, 14, 0, 8},
           {6011, 8, 0, 12},
           {6013, 8, 0, 16},
           {6234, 0, 0, 20},
           {6279, 15, 0, 24},
           {6294, 10, 0, 40},
           {6999, kGeneric, children, 52},
           {kUnknown, 4, 0, 56}}},
         {"nplt",
          32,
          {{6000, 14, 0, 0}, {6007, 15, 0, 4}, {6008, 8, 0, 20}, {6296, 0, 0, 24}, {6999, kGeneric, children, 28}}},
         {"trsl", 16, {{6047, 12, 0, 0}}},
         {"rota", 16, {{6048, 13, 0, 0}}},
         {"xprt", 16, {{6238, 4, 0, 0}, {6052, 14, 0, 4}, {6053, 4, 0, 8}, {6274, 4, 0, 12}}},
         {"amap", 8, {{6039, 4, 0, 0}, {6999, kGeneric, children, 4}}},
         {"amel", 28, {{6040, 8, 0, 0}, {6041, 8, 0, 4}, {6042, 8, 0, 8}, {6043, 15, 0, 12}}},
         {"spnv", 12, {{6285, 0, 0, 0}, {6045, 0, 0, 1}, {6286, 8, 0, 4}, {6999, kGeneric, children, 8}}},
         {"data", 12, {{6168, 0, 0, 0}, {16, 10, gff4::kList, 4}, {8, 2, gff4::kList, 8}}},
         {"bbox", 32, {{6054, 12, 0, 0}, {6055, 12, 0, 16}}},
         {"attr", 8, {{6049, 14, 0, 0}, {6050, 14, 0, 4}}}});
    const std::size_t root = Struct(b, 32);
    const std::size_t gob = Struct(b, 12);
    const std::size_t box = Struct(b, 32);
    const std::size_t gobAt = Struct(b, 16);
    const std::size_t gobTurn = Struct(b, 16);
    const std::size_t attribute = Struct(b, 8);
    const std::size_t hook = Struct(b, 12);
    const std::size_t glow = Struct(b, 60);
    const std::size_t glowRate = Struct(b, 16);
    const std::size_t ageMap = Struct(b, 8);
    const std::size_t born = Struct(b, 28);
    const std::size_t dead = Struct(b, 28);
    const std::size_t volume = Struct(b, 12);
    const std::size_t points = Struct(b, 12);
    const std::size_t indices = Struct(b, 12);
    const std::size_t light = Struct(b, 32);
    const std::size_t lightRadius = Struct(b, 16);

    b.U32(root, b.String(u"fxc_test.mmh"));
    b.U32(root + 4, b.String(u"fxc_test.msh"));
    b.U32(root + 12, Children(b, {{1, gob}}));
    b.U32(root + 16, 2);
    Float(b, root + 20, 0.5F);
    b.U32(root + 24, b.String(u"White.dds"));
    Float(b, root + 28, 0.25F);

    b.U32(gob, b.String(u"GOB"));
    b.U32(gob + 4, 0xFFFFFFFF);
    Floats(b, box, {-1, -1, -1, 1, 1, 1, 1, 1});
    Floats(b, gobAt, {0, 0, 1, 1});
    Floats(b, gobTurn, {0, 0, 0, 1});
    b.U32(attribute, b.String(u"BaseLight"));
    b.U32(attribute + 4, b.String(u"BaseLight"));
    b.U32(gob + 8, Children(b, {{12, box}, {5, gobAt}, {6, gobTurn}, {13, attribute}, {2, hook}}));

    b.U32(hook, b.String(u"CrustHook1"));
    b.U8(hook + 4, 1);
    b.U32(hook + 8, Children(b, {{3, glow}, {4, light}}));

    b.U32(glow, b.String(u"glow"));
    b.U32(glow + 4, b.String(u"dotblur_add"));
    b.U32(glow + 8, b.String(u"fx_miscmatlib.mal"));
    Float(b, glow + 12, 7.0F);
    Float(b, glow + 16, 1.0F);
    b.U8(glow + 20, 1);
    Floats(b, glow + 24, {1, 1, 1, 1});
    Floats(b, glow + 40, {0, 0, 0.5F});
    b.U32(glow + 56, 42);
    b.U32(glowRate, 8);
    b.U32(glowRate + 4, b.String(u"glow_birthrate"));
    b.U32(glowRate + 8, 3);
    b.U32(glowRate + 12, 0);
    b.U32(ageMap, 2);
    Floats(b, born, {0, 0.6F, 0.5F, 0.4F, 0.7F, 1, 0});
    Floats(b, dead, {1, 0.2F, 0.15F, 0.4F, 0.7F, 1, 0});
    b.U32(ageMap + 4, Children(b, {{9, born}, {9, dead}}));
    b.U8(volume, 0);
    b.U8(volume + 1, 1);
    Float(b, volume + 4, std::numeric_limits<float>::quiet_NaN());
    const std::size_t list = b.bytes.size();
    b.U32(list, 3);
    // Three points, each followed by the junk the game's fourth float is.
    Floats(b, list + 4, {-2, 0, 3, 1e-38F, 2, 0, 3, 1e-38F, 0, 1, 2, 1e-38F});
    b.U32(points + 4, static_cast<std::uint32_t>(list - b.data));
    b.U32(points + 8, 0xFFFFFFFF);
    b.U8(indices, 1);
    b.U32(indices + 4, 0xFFFFFFFF);
    const std::size_t shorts = b.bytes.size();
    b.U32(shorts, 3);
    b.U32(shorts + 4, 0x00010002); // 2, 1
    b.U32(shorts + 8, 0);          // 0
    b.U32(indices + 8, static_cast<std::uint32_t>(shorts - b.data));
    b.U32(volume + 8, Children(b, {{11, points}, {11, indices}}));
    b.U32(glow + 52, Children(b, {{7, glowRate}, {8, ageMap}, {10, volume}}));

    b.U32(light, b.String(u"Light_c"));
    Floats(b, light + 4, {0.6F, 1.4F, 2, 1});
    Float(b, light + 20, 2.0F);
    b.U8(light + 24, 1);
    b.U32(lightRadius, 25);
    b.U32(lightRadius + 4, b.String(u"light_c_radius"));
    b.U32(lightRadius + 8, 3);
    b.U32(lightRadius + 12, 1);
    b.U32(light + 28, Children(b, {{7, lightRadius}}));
    return b.bytes;
}

// fxc_wisp_c.cess.evt: the two emitters its cessation fires at 0.97 s.
std::vector<std::byte> EventFile()
{
    Builder b = Start("EVT ", {{"evtl", 4, {{4020, 1, gff4::kList | gff4::kStruct, 0}}},
                               {"evt", 20, {{4016, 9, 0, 0}, {4017, 4, 0, 8}, {4018, 4, 0, 12}, {4019, 14, 0, 16}}}});
    const std::size_t root = Struct(b, 4);
    const std::size_t list = b.bytes.size();
    b.U32(list, 2);
    const std::size_t first = Struct(b, 20);
    const std::size_t second = Struct(b, 20);
    b.U32(root, static_cast<std::uint32_t>(list - b.data));
    for (const auto &[at, name] : {std::pair{first, u"expirationsmoke"}, std::pair{second, u"expiration"}})
    {
        const double time = 0.97;
        std::memcpy(b.bytes.data() + at, &time, sizeof time);
        b.U32(at + 8, 16);
        b.U32(at + 12, 1);
        b.U32(at + 16, b.String(name));
    }
    return b.bytes;
}

// vfx_base's row of the wisp's crust as the game has it, after a row of the same ID a later part
// gives, which the game drops.
m2da::Merged Effects()
{
    m2da::Merged table;
    for (const auto *name : {"ID", "Label", "Type", "ModelName", "ModelName1", "Size1", "BlendTreeName", "CrustEffect",
                             "SoundDuration", "SoundDurationVolume", "SoundDurationPitch"})
    {
        table.columns.push_back({.hash = dao::core::GdaColumnHash(name), .type = std::nullopt});
    }
    const auto row = [&](bool taken, const wchar_t *model) {
        table.rows.push_back(
            {.part = taken ? 0U : 1U,
             .cells = {std::int64_t{3006}, std::wstring(L"Wisp Crust"), std::wstring(L"D"), std::wstring(model),
                       std::wstring(model), 0.5, std::wstring(model), std::int64_t{1},
                       std::wstring(L"vfx_spells/vfx_new/monster/fxc_wisp_c_dur_n"), -3.0, 0.0},
             .id = 3006,
             .conflict = true,
             .taken = taken,
             .alike = {}});
    };
    row(false, L"fxc_dropped");
    row(true, L"fxc_wisp_c");
    return table;
}

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());
    std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

const effect::Raw *RawOf(const effect::Item &item, std::uint32_t label)
{
    for (const effect::Raw &raw : item.fields)
    {
        if (raw.label == label)
        {
            return &raw;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("an effect's row names its model and tree, the row the game takes of its ID", "[effect]")
{
    const m2da::Merged effects = Effects();
    const auto row = effect::RowOf(effects, 3006);
    REQUIRE(row);
    CHECK(row->model == L"fxc_wisp_c");
    CHECK(row->ModelFile() == L"fxc_wisp_c.mmh");
    CHECK(row->TreeFile() == L"fxc_wisp_c.anb");
    CHECK(row->type == L"D");
    CHECK(row->crust);
    CHECK(row->duration.name == L"vfx_spells/vfx_new/monster/fxc_wisp_c_dur_n");
    CHECK(row->duration.volume == Approx(-3.0F));
    REQUIRE(row->sizes.size() == 1);
    CHECK(row->sizes[0].size == Approx(0.5F));
    CHECK(row->impact.name.empty());
    CHECK_FALSE(effect::RowOf(effects, 3007));
    CHECK(effect::Row{}.ModelFile().empty());
}

TEST_CASE("an effect's model is its nodes, each read by its kind", "[effect]")
{
    const auto bytes = EffectFile();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto read = effect::EffectOf(*file);
    REQUIRE(read);
    CHECK(read->name == L"fxc_test.mmh");
    CHECK(read->presimulateTime == Approx(0.5F));
    CHECK(read->totalExports == 2);
    REQUIRE(read->remoteMaterial);
    CHECK(read->remoteMaterial->decal == L"White.dds");
    CHECK(read->remoteMaterial->alpha == Approx(0.25F));
    REQUIRE(read->items.size() == 4);

    const effect::Item &gob = read->items[0];
    CHECK(gob.node.kind == "node");
    REQUIRE(gob.bounds);
    CHECK((*gob.bounds)[1][0] == Approx(1.0F));
    REQUIRE(gob.parts.size() == 1); // the attribute, which nothing reads
    CHECK(gob.parts[0].label == "attr");
    CHECK(gob.parts[0].fields.size() == 2);

    const effect::Item &hook = read->items[1];
    CHECK(hook.node.kind == "crst");
    CHECK(hook.node.parent == 0);
    CHECK(hook.crustHook == std::optional<std::uint8_t>{1});

    const effect::Item *glow = read->Find(L"GLOW");
    REQUIRE(glow != nullptr);
    CHECK(glow->node.parent == 1);
    CHECK_FALSE(glow->light);
    REQUIRE(glow->emitter);
    const effect::Emitter &emitter = *glow->emitter;
    CHECK(emitter.material == L"dotblur_add");
    CHECK(emitter.materialLibrary == L"fx_miscmatlib.mal");
    CHECK(emitter.birthRate == Approx(7.0F));
    CHECK(emitter.life == Approx(1.0F));
    CHECK(emitter.type == 1);
    CHECK(emitter.worldAxisAcceleration[2] == Approx(0.5F));
    CHECK(emitter.ageMapScaleXMultiplier == Approx(1.0F)); // not in the file: the multiplier's own
    REQUIRE(emitter.ageMap.size() == 2);
    CHECK(emitter.ageMap[0].lifeElapsed == Approx(0.0F));
    CHECK(emitter.ageMap[0].scaleX == Approx(0.6F));
    CHECK(emitter.ageMap[0].colour[3] == Approx(0.0F));
    CHECK(emitter.ageMap[1].lifeElapsed == Approx(1.0F));
    CHECK(emitter.ageMap[1].scaleY == Approx(0.15F));
    REQUIRE(emitter.spawnVolume);
    CHECK(emitter.spawnVolume->type == 0);
    CHECK(emitter.spawnVolume->spawnWithinVolume);
    CHECK(std::isnan(emitter.spawnVolume->radius));
    REQUIRE(emitter.spawnVolume->points.size() == 3);
    CHECK(emitter.spawnVolume->points[1] == effect::Vector{2, 0, 3});
    CHECK(emitter.spawnVolume->points[2] == effect::Vector{0, 1, 2});
    CHECK(emitter.spawnVolume->indices == std::vector<std::uint16_t>{2, 1, 0});
    CHECK(glow->parts.empty());
    const effect::Raw *unknown = RawOf(*glow, kUnknown);
    REQUIRE(unknown != nullptr);
    CHECK(std::get<std::int64_t>(unknown->value) == 42);
    const effect::Raw *birthRate = RawOf(*glow, 6011);
    REQUIRE(birthRate != nullptr);
    CHECK(std::get<std::vector<float>>(birthRate->value) == std::vector<float>{7.0F});

    const effect::Item *light = read->Find(L"light_c");
    REQUIRE(light != nullptr);
    REQUIRE(light->light);
    CHECK(light->light->colour[1] == Approx(1.4F));
    CHECK(light->light->radius == Approx(2.0F));
    CHECK(light->light->affectDomain == 1);
}

TEST_CASE("a clip's channel drives the parameter its export names", "[effect]")
{
    const auto bytes = EffectFile();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto read = effect::EffectOf(*file);
    REQUIRE(read);
    const auto rate = read->Drives(L"Glow_Birthrate");
    REQUIRE(rate);
    CHECK(read->items[rate->item].node.name == L"glow");
    CHECK(rate->parameter == effect::Parameter::BirthRate);
    REQUIRE(rate->exported != nullptr);
    CHECK(rate->exported->controllerType == 3);
    const auto radius = read->Drives(L"light_c_radius");
    REQUIRE(radius);
    CHECK(radius->parameter == effect::Parameter::Radius);
    CHECK(radius->exported->controllerIndex == 1);
    // The wisp's clips key a trail no node of its model has.
    CHECK_FALSE(read->Drives(L"trail_birthrate"));
}

TEST_CASE("a clip's events, each its time, id, target and what it names", "[effect]")
{
    const auto bytes = EventFile();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto events = clip::EventsOf(*file);
    REQUIRE(events);
    REQUIRE(events->size() == 2);
    CHECK((*events)[0].time == Approx(0.97));
    CHECK((*events)[0].id == 16);
    CHECK((*events)[0].target == 1);
    CHECK((*events)[0].text == L"expirationsmoke");
    CHECK((*events)[1].text == L"expiration");
    CHECK_FALSE(effect::EffectOf(*file));
    const auto model = EffectFile();
    const auto modelFile = gff4::File::Parse(model);
    REQUIRE(modelFile);
    CHECK_FALSE(clip::EventsOf(*modelFile));
}

TEST_CASE("the game's effects in game/effects, when there, read whole", "[effect][game]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "effects";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/effects (ghilana's extract of fx*.mmh and fx*.evt writes them there)");
    }
    std::size_t models = 0;
    std::size_t emitters = 0;
    std::size_t events = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        const auto bytes = FromDisk(entry.path());
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        if (entry.path().extension() == ".evt")
        {
            const auto read = clip::EventsOf(*file);
            REQUIRE(read);
            events += read->size();
            continue;
        }
        if (entry.path().extension() != ".mmh")
        {
            continue;
        }
        const auto read = effect::EffectOf(*file);
        REQUIRE(read);
        ++models;
        for (const effect::Item &item : read->items)
        {
            CHECK(item.emitter.has_value() == (item.node.kind == "nemt"));
            if (item.emitter)
            {
                ++emitters;
                CHECK(!item.emitter->material.empty());
                if (const auto &volume = item.emitter->spawnVolume; volume && volume->type == 0)
                {
                    CHECK(!volume->points.empty());
                    for (const std::uint16_t index : volume->indices)
                    {
                        CHECK(index < volume->points.size());
                    }
                }
            }
        }
        if (entry.path().filename() == "fxc_wisp_c.mmh")
        {
            const effect::Item *glow = read->Find(L"glow");
            REQUIRE(glow != nullptr);
            CHECK(glow->emitter->birthRate == Approx(7.0F));
            CHECK(read->items[1].crustHook == std::optional<std::uint8_t>{1});
        }
    }
    // The counts read 2026-10-18 in the 600 effect models and 1,070 event files the game takes.
    CHECK(models == 600);
    CHECK(emitters == 2927);
    CHECK(events == 2333);
}
