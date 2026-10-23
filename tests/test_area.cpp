// dao/core/Area.h over areas built here, and over the game's in game/areas
// when ghilana's extract has written them there (DAOLib #160).
#include "dao/core/Area.h"
#include "dao/core/Gff3.h"
#include "files_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <variant>
#include <vector>

namespace area = dao::core::area;
namespace gff3 = dao::core::gff3;
using Builder = dao::test::files::Gff3;
using Catch::Approx;

namespace
{

constexpr std::uint32_t kByte = 0;
constexpr std::uint32_t kInt = 5;
constexpr std::uint32_t kFloat = 8;
constexpr std::uint32_t kString = 10;
constexpr std::uint32_t kResRef = 11;
constexpr std::uint32_t kLocString = 12;
constexpr std::uint32_t kList = 15;

std::uint32_t Bits(float value)
{
    return std::bit_cast<std::uint32_t>(value);
}

std::uint32_t Int(std::int32_t value)
{
    return std::bit_cast<std::uint32_t>(value);
}

// A variable of the VarTable: its name, its type, and its value's field.
std::uint32_t Variable(Builder &b, std::string_view name, std::uint8_t type, Builder::Value value)
{
    value.label = "Value";
    return b.Struct(0, {{"Name", kString, b.ExoString(name)}, {"Type", kByte, type}, value});
}

// The tower's first level's shape (bhm100ar_tower_level_1.are), cut down to
// one of each kind.
dao::test::files::Bytes TowerArea()
{
    Builder b("ARE");
    const auto top = b.Struct(0xFFFFFFFF, {});
    const auto areaId = Variable(b, "AREA_ID", 1, {.label = "", .type = kInt, .data = 54});
    const auto counter = Variable(b, "AREA_NAME", 3, {.label = "", .type = kString, .data = b.ExoString("tower")});
    const auto creatureVar = Variable(b, "AMBIENT_RANGE", 2, {.label = "", .type = kFloat, .data = Bits(2.5F)});
    const auto creature = b.Struct(4, {{"TemplateResRef", kResRef, b.ResRef("bhm100cr_jowan")},
                                       {"Tag", kString, b.ExoString("bhm100cr_jowan")},
                                       {"XPosition", kFloat, Bits(51.25F)},
                                       {"YPosition", kFloat, Bits(57.5F)},
                                       {"ZPosition", kFloat, Bits(-3.75F)},
                                       {"XOrientation", kFloat, Bits(0)},
                                       {"YOrientation", kFloat, Bits(0)},
                                       {"ZOrientation", kFloat, Bits(-0.6F)},
                                       {"WOrientation", kFloat, Bits(0.8F)},
                                       {"Active", kInt, 0},
                                       {"Importance", kInt, 1},
                                       {"VarTable", kList, b.List({creatureVar})},
                                       {"TeamId", kInt, Int(-1)},
                                       {"Rank", kInt, 4},
                                       {"TreasureCategory", kInt, 2},
                                       {"InteractRadius", kFloat, Bits(1.5F)},
                                       {"IsSelectable", kByte, 0},
                                       {"Trackable", kInt, 1},
                                       {"WeaponsDrawn", kByte, 1}});
    const auto placeable = b.Struct(9, {{"TemplateResRef", kResRef, b.ResRef("genip_chest")},
                                        {"Tag", kString, b.ExoString("bhm100ip_chest")},
                                        {"PickLock", kInt, 30},
                                        {"RoomID1", kInt, 65537},
                                        {"RoomID2", kInt, 65538},
                                        {"Useable", kByte, 0},
                                        {"InitialState", kInt, 2}});
    const auto corner =
        b.Struct(0, {{"XPoint", kFloat, Bits(8.5F)}, {"YPoint", kFloat, Bits(-4.25F)}, {"ZPoint", kFloat, Bits(0)}});
    const auto regionSound = b.Struct(6, {{"Tag", kString, b.ExoString("amb_ext_rumble1_lp")},
                                          {"Sound", kString, b.ExoString("global_amb_glo/amb_ext_rumble1_lp")},
                                          {"Volume", kFloat, Bits(-4.0F)},
                                          {"Occludable", kInt, 1}});
    const auto sounds = b.Struct(1, {{"Tag", kString, b.ExoString("bhm100tr_wind")},
                                     {"Type", kInt, 4},
                                     {"ReverbPreset", kString, b.ExoString("Quarry")},
                                     {"Geometry", kList, b.List({corner})},
                                     {"SoundList", kList, b.List({regionSound})}});
    // A music region as every one is written: an empty script beside the music's states.
    const auto music = b.Struct(1, {{"Tag", kString, b.ExoString("bhm100tr_music")},
                                    {"Type", kInt, 5},
                                    {"ScriptResRefID", kResRef, b.ResRef("")},
                                    {"ScriptDelay", kInt, 0},
                                    {"EnterState", kInt, 2},
                                    {"EnterStateDelay", kInt, 3000},
                                    {"ExitState", kInt, 1},
                                    {"ExitStateDelay", kInt, 0}});
    const auto waypoint = b.Struct(5, {{"Tag", kString, b.ExoString("mn_tower_exit")},
                                       {"HasMapNote", kByte, 1},
                                       {"MapNote", kLocString, b.LocString(377012)},
                                       {"MapNoteType", kInt, 2},
                                       {"MapNoteEnabled", kByte, 1},
                                       {"Group", kInt, 3}});
    const auto stage = b.Struct(12, {{"TemplateResRef", kResRef, b.ResRef("bhm100st_jowan")}});
    const auto sound = b.Struct(6, {{"Tag", kString, b.ExoString("amb_crow")},
                                    {"Sound", kString, b.ExoString("global_amb_glo/amb_ext_crow_sgl")},
                                    {"MaxDistanceMult", kFloat, Bits(1.5F)},
                                    {"Priority", kInt, 225}});
    b.Set(top, {{"Name", kLocString, b.LocString(355401)},
                {"Tag", kString, b.ExoString("bhm100ar_tower_level_1")},
                {"TemplateResRef", kResRef, b.ResRef("bhm100ar_tower_level_1")},
                {"ScriptResRefID", kResRef, b.ResRef("bhm000ar_generic_area")},
                {"Comments", kString, b.ExoString("")},
                {"NoTeleport", kByte, 1},
                {"Map", kString, b.ExoString("")},
                {"North", kInt, 90},
                {"Layout", kString, b.ExoString("bhm100d")},
                {"ReverbPreset", kString, b.ExoString("StoneRoom")},
                {"MusicPreset", kString, b.ExoString("music\\circle_tower")},
                {"VarTable", kList, b.List({areaId, counter})},
                {"CreatureList", kList, b.List({creature})},
                {"ItemList", kList, b.List({})},
                {"PlaceableList", kList, b.List({placeable})},
                {"TriggerList", kList, b.List({sounds, music})},
                {"StoreList", kList, b.List({})},
                {"StageList", kList, b.List({stage})},
                {"WaypointList", kList, b.List({waypoint})},
                {"SoundList", kList, b.List({sound})}});
    return b.Done();
}

std::vector<std::byte> FileBytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(chars.size());
    std::memcpy(bytes.data(), chars.data(), chars.size());
    return bytes;
}

} // namespace

TEST_CASE("an area's own fields, its variables and its AREA_ID", "[area]")
{
    const auto bytes = TowerArea();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto read = area::AreaOf(*file);
    REQUIRE(read);
    CHECK(read->name == 355401);
    CHECK(read->tag == "bhm100ar_tower_level_1");
    CHECK(read->script == "bhm000ar_generic_area");
    CHECK(read->noTeleport);
    CHECK(read->north == 90);
    CHECK(read->layout == "bhm100d");
    CHECK(read->musicPreset == "music\\circle_tower");
    REQUIRE(read->variables.size() == 2);
    CHECK(read->variables[1].type == 3);
    CHECK(std::get<std::string>(read->variables[1].value) == "tower");
    CHECK(read->AreaId() == 54);
    CHECK_FALSE(read->Int("AREA_NAME")); // a string
    CHECK_FALSE(read->Int("AREA_COUNTER_1"));
}

TEST_CASE("an area's objects, each with its kind's fields", "[area]")
{
    const auto bytes = TowerArea();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto read = area::AreaOf(*file);
    REQUIRE(read);

    REQUIRE(read->creatures.size() == 1);
    const auto &jowan = read->creatures[0];
    CHECK(jowan.placed.templateResRef == "bhm100cr_jowan");
    CHECK(jowan.placed.at == area::Point{51.25F, 57.5F, -3.75F});
    CHECK(jowan.placed.turn[2] == Approx(-0.6F));
    CHECK(jowan.placed.turn[3] == Approx(0.8F));
    CHECK(jowan.placed.active == 0);
    CHECK(jowan.placed.importance == 1);
    REQUIRE(jowan.placed.variables.size() == 1);
    CHECK(std::get<float>(jowan.placed.variables[0].value) == 2.5F);
    CHECK(jowan.teamId == -1);
    CHECK(jowan.rank == 4);
    CHECK(jowan.treasureCategory == 2);
    CHECK(jowan.interactRadius == 1.5F);
    CHECK_FALSE(jowan.isSelectable);
    CHECK(jowan.weaponsDrawn);

    REQUIRE(read->placeables.size() == 1);
    const auto &chest = read->placeables[0];
    CHECK(chest.pickLock == 30);
    CHECK(chest.roomId1 == 65537);
    CHECK(chest.roomId2 == 65538);
    CHECK_FALSE(chest.useable);
    CHECK(chest.initialState == 2);
    CHECK(chest.placed.turn == area::Quaternion{0, 0, 0, 1}); // none written: unturned

    REQUIRE(read->triggers.size() == 2);
    const auto &wind = read->triggers[0];
    CHECK(wind.type == 4);
    CHECK(wind.reverbPreset == "Quarry");
    REQUIRE(wind.geometry.size() == 1);
    CHECK(wind.geometry[0] == area::Point{8.5F, -4.25F, 0});
    REQUIRE(wind.sounds.size() == 1);
    CHECK(wind.sounds[0].volume == -4.0F);
    CHECK(wind.sounds[0].occludable);
    const auto &music = read->triggers[1];
    CHECK(music.type == 5);
    CHECK(music.script.empty());
    CHECK(music.enterState == 2);
    CHECK(music.enterStateDelay == 3000);
    CHECK(music.exitState == 1);
    CHECK(music.exitStateDelay == 0);
    CHECK_FALSE(music.priority);

    REQUIRE(read->waypoints.size() == 1);
    CHECK(read->waypoints[0].hasMapNote);
    CHECK(read->waypoints[0].mapNote == 377012);
    CHECK(read->waypoints[0].mapNoteType == 2);
    CHECK(read->waypoints[0].group == 3);
    REQUIRE(read->stages.size() == 1);
    CHECK(read->stages[0].templateResRef == "bhm100st_jowan");
    CHECK(read->stores.empty());
    REQUIRE(read->sounds.size() == 1);
    CHECK(read->sounds[0].sound == "global_amb_glo/amb_ext_crow_sgl");
    CHECK(read->sounds[0].maxDistanceMult == 1.5F);
    CHECK(read->sounds[0].priority == 225);
}

// The Circle's basement's second door between rooms 1325 and 1103, as bhm700ar_tower_basement.are
// has it, and a placeable whose instance has no RoomIDs.
TEST_CASE("a placeable's rooms are its RoomIDs, and -1 where it has none", "[area]")
{
    Builder b("ARE");
    const auto top = b.Struct(0xFFFFFFFF, {});
    const auto door = b.Struct(
        9, {{"Tag", kString, b.ExoString("bhm700ip_seconddoor")}, {"RoomID1", kInt, 1325}, {"RoomID2", kInt, 1103}});
    const auto bare = b.Struct(9, {{"Tag", kString, b.ExoString("genip_chest")}});
    b.Set(top, {{"PlaceableList", kList, b.List({door, bare})}});
    const auto bytes = b.Done();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto read = area::AreaOf(*file);
    REQUIRE(read);
    REQUIRE(read->placeables.size() == 2);
    CHECK(read->placeables[0].roomId1 == 1325);
    CHECK(read->placeables[0].roomId2 == 1103);
    CHECK(read->placeables[1].roomId1 == -1);
    CHECK(read->placeables[1].roomId2 == -1);
}

TEST_CASE("a GFF3 file of another type is no area", "[area]")
{
    Builder b("UTC");
    b.Struct(0xFFFFFFFF, {{"Tag", kString, b.ExoString("ogre")}});
    const auto bytes = b.Done();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    CHECK_FALSE(area::AreaOf(*file));
}

TEST_CASE("the game's areas in game/areas, when there, read whole with an AREA_ID each", "[area][game][folder]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "areas";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/areas (ghilana's extract of *.are writes them there)");
    }
    std::size_t areas = 0;
    std::size_t creatures = 0;
    std::size_t corners = 0;
    std::size_t placeables = 0;
    std::size_t doors = 0;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(folder))
    {
        if (entry.path().extension() != ".are")
        {
            continue;
        }
        const auto bytes = FileBytes(entry.path());
        const auto file = gff3::File::Parse(bytes);
        REQUIRE(file);
        const auto read = area::AreaOf(*file);
        REQUIRE(read);
        CHECK(read->AreaId()); // every one has its row of areadata
        CHECK_FALSE(read->layout.empty());
        ++areas;
        creatures += read->creatures.size();
        for (const auto &trigger : read->triggers)
        {
            corners += trigger.geometry.size();
        }
        for (const auto &placeable : read->placeables)
        {
            ++placeables;
            const bool between = placeable.roomId1 != placeable.roomId2;
            doors += between && placeable.roomId1 != -1 && placeable.roomId2 != -1 ? 1 : 0;
        }
    }
    // As the survey counted them, 2026-10-17: 297 areas, 8,416 creatures, 38,273 corners.
    CHECK(areas == 297);
    CHECK(creatures == 8416);
    CHECK(corners == 38273);
    // And 2026-10-18 (#186): 7,190 placeables, 815 of them between two rooms.
    CHECK(placeables == 7190);
    CHECK(doors == 815);
}
