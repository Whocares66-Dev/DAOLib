// dao/core/Collision.h over a level's static physics and a placeable's
// shapes built here, and over the game's in game/collision when ghilana's
// extract has written them there (DAOLib #189).
#include "dao/core/Collision.h"
#include "dao/core/Gff4.h"
#include "dao/core/Grid.h"
#include "dao/core/Install.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using Catch::Approx;
using dao::core::Point;
namespace collision = dao::core::collision;
namespace gff4 = dao::core::gff4;
using dao::test::Builder;
using dao::test::Children;
using dao::test::Floats;
using dao::test::Start;
using dao::test::Struct;

namespace
{

void Word(std::vector<std::byte> &out, std::uint32_t value)
{
    const std::size_t at = out.size();
    out.resize(at + 4);
    std::memcpy(out.data() + at, &value, 4);
}

// A collection framed as an .opf is: the lengths, the stream, then the records.
std::vector<std::byte> Framed(std::string_view stream, const std::vector<std::array<std::int32_t, 4>> &actors,
                              const std::vector<std::array<std::uint32_t, 3>> &shapes)
{
    std::vector<std::byte> out;
    Word(out, 0);
    Word(out, static_cast<std::uint32_t>(stream.size()));
    const std::size_t at = out.size();
    out.resize(at + stream.size());
    std::memcpy(out.data() + at, stream.data(), stream.size());
    Word(out, static_cast<std::uint32_t>(actors.size()));
    for (const auto &record : actors)
    {
        for (const std::int32_t value : record)
        {
            Word(out, static_cast<std::uint32_t>(value));
        }
    }
    Word(out, static_cast<std::uint32_t>(shapes.size()));
    for (const auto &record : shapes)
    {
        for (const std::uint32_t value : record)
        {
            Word(out, value);
        }
    }
    const auto total = static_cast<std::uint32_t>(out.size());
    std::memcpy(out.data(), &total, 4);
    return out;
}

// Two actors as lak302d's are written: a wall turned about z with two boxes, and a prop with a capsule, a sphere
// and a mesh shape; one mesh before them, and an actor of group 0 that is not the first record.
constexpr std::string_view kStream = R"(  <NXUSTREAM2>
    <NxuPhysicsCollection id="Static Physics" userProperties="" sdkVersion="272" nxuVersion="103">
      <NxTriangleMeshDesc id="TriangleMesh_0" userProperties="">
        <NxSimpleTriangleMesh>
          <points>0 0 0  1 0 0  0 1 0
            0 0 1
            </points>
          <triangles>0 1 2  0 2 3
            </triangles>
        </NxSimpleTriangleMesh>
      </NxTriangleMeshDesc>
      <NxSceneDesc id="Scene_0" userProperties="">
        <NxGroupsMask id="groupMask0" bits0="7" bits1="0" bits2="0" bits3="0"/>
        <NxActorDesc id="Actor_0" userProperties="1,0,1" hasBody="false" name="tvi_smwall_01_0.GOB">
          <globalPose>0 1 0  -1 0 0  0 0 1  12 -76 0</globalPose>
          <group>5</group>
          <NxBoxShapeDesc dimensions="4.000959873 0.078416698 2.75">
            <NxShapeDesc userProperties="" name="ColNWBox_TVI_smwall_01_0_1">
              <localPose>1 0 0  0 1 0  0 0 1  1 2 3</localPose>
              <group>5</group>
              <NxGroupsMask id="groupsMask" bits0="2164269088" bits1="0" bits2="0" bits3="0"/>
            </NxShapeDesc>
          </NxBoxShapeDesc>
          <NxBoxShapeDesc dimensions="4 0.574153006 0.0276688">
            <NxShapeDesc userProperties="" name="ColWBox_TVI_smwall_01_0_1">
              <group>5</group>
              <NxGroupsMask id="groupsMask" bits0="2181046304" bits1="1" bits2="2" bits3="3"/>
            </NxShapeDesc>
          </NxBoxShapeDesc>
        </NxActorDesc>
        <NxActorDesc id="Actor_1" userProperties="0,2,3,4" hasBody="true" name="prp_a&amp;b.GOB">
          <globalPose>1 0 0  0 1 0  0 0 1  0 -72 2.5</globalPose>
          <group>0</group>
          <NxCapsuleShapeDesc radius="0.694777012" height="3.040899992">
            <NxCapsuleShapeFlag id="flags">
              <NX_SWEPT_SHAPE>false</NX_SWEPT_SHAPE>
            </NxCapsuleShapeFlag>
            <NxShapeDesc userProperties="" name="Capsule19">
              <group>0</group>
              <NxGroupsMask id="groupsMask" bits0="16781344" bits1="0" bits2="0" bits3="0"/>
            </NxShapeDesc>
          </NxCapsuleShapeDesc>
          <NxSphereShapeDesc radius="0.25">
            <NxShapeDesc userProperties="" name="CollisionSphere03">
              <NxGroupsMask id="groupsMask" bits0="2147549184" bits1="0" bits2="0" bits3="0"/>
            </NxShapeDesc>
          </NxSphereShapeDesc>
          <NxTriangleMeshShapeDesc meshData="TriangleMesh_0">
            <NxShapeDesc userProperties="" name="Box39">
              <NxGroupsMask id="groupsMask" bits0="0" bits1="0" bits2="0" bits3="0"/>
            </NxShapeDesc>
          </NxTriangleMeshShapeDesc>
        </NxActorDesc>
      </NxSceneDesc>
    </NxuPhysicsCollection>
  </NXUSTREAM2>
)";

std::vector<std::byte> Level()
{
    return Framed(kStream, {{-1, 0, 1103, 2}, {-1, 0, 1101, 5}},
                  {{0, 0, 0xBAADF00D}, {1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 11, 12}});
}

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());
    std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

std::filesystem::path GameFolder()
{
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "collision";
}

} // namespace

TEST_CASE("a level's actors are read with their poses, groups, records and rooms", "[collision]")
{
    const std::vector<std::byte> bytes = Level();
    const std::optional<collision::StaticPhysics> physics = collision::StaticPhysicsOf(bytes);
    REQUIRE(physics.has_value());
    CHECK(physics->xml);
    REQUIRE(physics->actors.size() == 2);
    const collision::Actor &wall = physics->actors[0];
    CHECK(wall.id == "Actor_0");
    CHECK(wall.name == "tvi_smwall_01_0.GOB");
    CHECK_FALSE(wall.hasBody);
    CHECK(wall.group == 5);
    CHECK(wall.globalPose.rotation == std::array<float, 9>{0, 1, 0, -1, 0, 0, 0, 0, 1});
    CHECK(wall.globalPose.translation == Point{12, -76, 0});
    CHECK(wall.record == 1);
    CHECK(wall.room == 1101); // its record's, not the first record's
    const collision::Actor &prop = physics->actors[1];
    CHECK(prop.name == "prp_a&b.GOB");
    CHECK(prop.hasBody);
    CHECK(prop.group == 0);
    CHECK(prop.record == 0);
    CHECK(prop.room == 1103);
    REQUIRE(physics->actorRecords.size() == 2);
    CHECK(physics->actorRecords[1].sound == 5);
    CHECK(physics->actorRecords[1].first == -1);
    REQUIRE(physics->shapeRecords.size() == 5);
    CHECK(physics->shapeRecords[0].words[2] == 0xBAADF00D);
}

TEST_CASE("a level's shapes are read with their kinds, sizes, poses and masks", "[collision]")
{
    const std::vector<std::byte> bytes = Level();
    const std::optional<collision::StaticPhysics> physics = collision::StaticPhysicsOf(bytes);
    REQUIRE(physics.has_value());
    REQUIRE(physics->actors.size() == 2);
    const auto &wall = physics->actors[0].shapes;
    REQUIRE(wall.size() == 2);
    CHECK(wall[0].name == "ColNWBox_TVI_smwall_01_0_1");
    CHECK(wall[0].kind == collision::Kind::Box);
    CHECK(wall[0].dimensions[0] == Approx(4.000959873F));
    CHECK(wall[0].dimensions[2] == Approx(2.75F));
    CHECK(wall[0].localPose.translation == Point{1, 2, 3});
    CHECK(wall[0].group == 5);
    CHECK(wall[0].groupsMask == std::array<std::uint32_t, 4>{0x81002020, 0, 0, 0});
    CHECK(wall[0].record == 0);
    CHECK(wall[1].groupsMask == std::array<std::uint32_t, 4>{0x82002020, 1, 2, 3});
    CHECK(wall[1].localPose.translation == Point{}); // none in the file: the identity
    CHECK(wall[1].record == 1);
    const auto &prop = physics->actors[1].shapes;
    REQUIRE(prop.size() == 3);
    CHECK(prop[0].kind == collision::Kind::Capsule);
    CHECK(prop[0].radius == Approx(0.694777012F));
    CHECK(prop[0].height == Approx(3.040899992F));
    CHECK(prop[0].groupsMask[0] == 0x01001020);
    CHECK(prop[0].record == 2);
    CHECK(prop[1].kind == collision::Kind::Sphere);
    CHECK(prop[1].radius == Approx(0.25F));
    CHECK(prop[2].kind == collision::Kind::Mesh);
    CHECK(prop[2].mesh == 0);
    CHECK(prop[2].record == 4);
    REQUIRE(physics->meshes.size() == 1);
    CHECK(physics->meshes[0].id == "TriangleMesh_0");
    CHECK(physics->meshes[0].points.size() == 4);
    CHECK(physics->meshes[0].points[3] == Point{0, 0, 1});
    CHECK(physics->meshes[0].triangles == std::vector<std::array<std::uint32_t, 3>>{{0, 1, 2}, {0, 2, 3}});
}

TEST_CASE("a shape blocks sight with static geometry or placeables, and the room ray with static geometry",
          "[collision]")
{
    CHECK(collision::Meets(0x81002020, collision::kSightMask));       // a wall
    CHECK(collision::Meets(0x81000004, collision::kSightMask));       // a door or crate
    CHECK(collision::Meets(0x01001020, collision::kSightMask));       // a tree
    CHECK_FALSE(collision::Meets(0x80010000, collision::kSightMask)); // terrain wall alone
    CHECK_FALSE(collision::Meets(0x81000002, collision::kSightMask)); // what stops creatures alone
    CHECK_FALSE(collision::Meets(0x81000004, collision::kRoomMask));  // a placeable is no floor
    CHECK(collision::kSightMask == 0x24);
}

TEST_CASE("a shape's pose by its actor's is its place in the level", "[collision]")
{
    collision::Pose actor;
    actor.rotation = {0, 1, 0, -1, 0, 0, 0, 0, 1}; // -90 degrees about z, by rows
    actor.translation = {10, 20, 0};
    collision::Pose shape;
    shape.translation = {1, 0, 2};
    const collision::Pose placed = collision::Compose(actor, shape);
    CHECK(placed.translation == Point{10, 19, 2});
    CHECK(placed.rotation == actor.rotation);
    CHECK(collision::Apply(actor, {0, 1, 0}) == Point{11, 20, 0});
    collision::Pose turn;
    turn.rotation = {0, -1, 0, 1, 0, 0, 0, 0, 1}; // +90 back
    CHECK(collision::Compose(actor, turn).rotation == std::array<float, 9>{1, 0, 0, 0, 1, 0, 0, 0, 1});
}

TEST_CASE("a binary stream gives the records alone, and bytes framed otherwise nothing", "[collision]")
{
    const std::vector<std::byte> binary = Framed("NXUSTREAM\0\0\x10\x01", {{-1, 0, 1325, 3}}, {{0, 0, 1}});
    const std::optional<collision::StaticPhysics> physics = collision::StaticPhysicsOf(binary);
    REQUIRE(physics.has_value());
    CHECK_FALSE(physics->xml);
    CHECK_FALSE(physics->binary);
    CHECK(physics->actors.empty());
    REQUIRE(physics->actorRecords.size() == 1);
    CHECK(physics->actorRecords[0].room == 1325);
    CHECK(physics->shapeRecords.size() == 1);

    std::vector<std::byte> bytes = Level();
    bytes.push_back(std::byte{0}); // a byte past the records, in the file's length
    const auto longer = static_cast<std::uint32_t>(bytes.size());
    std::memcpy(bytes.data(), &longer, 4);
    CHECK_FALSE(collision::StaticPhysicsOf(bytes));
    bytes = Level();
    const auto total = static_cast<std::uint32_t>(bytes.size() + 1);
    std::memcpy(bytes.data(), &total, 4);
    CHECK_FALSE(collision::StaticPhysicsOf(bytes)); // a length that is not the file's
    bytes = Level();
    bytes.resize(bytes.size() - 1);
    const auto shorter = static_cast<std::uint32_t>(bytes.size());
    std::memcpy(bytes.data(), &shorter, 4);
    CHECK_FALSE(collision::StaticPhysicsOf(bytes)); // the last record cut short
    bytes = Level();
    const std::uint32_t long_stream = 0x7FFFFFFF;
    std::memcpy(bytes.data() + 4, &long_stream, 4);
    CHECK_FALSE(collision::StaticPhysicsOf(bytes)); // a stream longer than the file
    CHECK_FALSE(collision::StaticPhysicsOf({}));
}

namespace
{

// The .phy's flags' offsets in a shap as the game lays it out (tools/gff4.py --templates on a door's).
struct Flag
{
    std::uint32_t label;
    std::uint32_t offset;
};
constexpr Flag kFlags[] = {{6063, 64}, {6064, 65}, {6065, 66}, {6066, 67}, {6067, 68}, {6068, 69}, {6069, 70},
                           {6070, 73}, {6178, 71}, {6179, 72}, {6252, 78}, {6277, 75}, {6295, 76}, {6305, 74}};

dao::test::TemplateSpec ShapTemplate()
{
    dao::test::TemplateSpec shap{"shap",
                                 0x50,
                                 {{6058, 4, 0, 32},
                                  {6060, 13, 0, 0},
                                  {6061, 12, 0, 16},
                                  {6241, 14, 0, 36},
                                  {6240, 8, 0, 40},
                                  {6244, 0, 0, 77},
                                  {6998, dao::test::kGeneric, gff4::kReference, 52},
                                  {6999, dao::test::kGeneric, gff4::kList | gff4::kReference, 60}}};
    for (const Flag &flag : kFlags)
    {
        shap.fields.push_back({flag.label, 0, 0, flag.offset});
    }
    return shap;
}

// A struct's place in the data block, as a reference to it holds.
std::uint32_t Ref(const Builder &b, std::size_t at)
{
    return static_cast<std::uint32_t>(at - b.data);
}

void FlagOn(Builder &b, std::size_t shap, std::uint32_t label)
{
    for (const Flag &flag : kFlags)
    {
        if (flag.label == label)
        {
            b.U8(shap + flag.offset, 1);
        }
    }
}

// A door's model: GOB, its mesh node DoorLow, a cobj and a box under it; and a capsule under GOB's own cobj.
std::vector<std::byte> Door()
{
    Builder b = Start(
        "PHY ",
        {{"mdlh", 8, {{6000, 14, 0, 0}, {6999, dao::test::kGeneric, 0xA000, 4}}},
         {"node", 12, {{6000, 14, 0, 0}, {6999, dao::test::kGeneric, 0xA000, 4}}},
         {"mshh", 8, {{6000, 14, 0, 0}, {6999, dao::test::kGeneric, 0xA000, 4}}},
         {"cobj", 16, {{6056, 8, 0, 0}, {6057, 4, 0, 4}, {6245, 4, 0, 8}, {6999, dao::test::kGeneric, 0xA000, 12}}},
         ShapTemplate(),
         {"boxs", 16, {{6071, 10, 0, 0}}},
         {"caps", 8, {{6072, 8, 0, 0}, {6073, 8, 0, 4}}}});
    const std::size_t root = Struct(b, 8);
    const std::size_t gob = Struct(b, 12);
    const std::size_t mesh = Struct(b, 8);
    const std::size_t meshObject = Struct(b, 16);
    const std::size_t box = Struct(b, 0x50);
    const std::size_t boxSize = Struct(b, 16);
    const std::size_t gobObject = Struct(b, 16);
    const std::size_t capsule = Struct(b, 0x50);
    const std::size_t capsuleSize = Struct(b, 8);
    b.U32(root, b.String(u"plc_lrgwddrr_01_0.mmh"));
    b.U32(root + 4, Children(b, {{1, gob}}));
    b.U32(gob, b.String(u"GOB"));
    b.U32(gob + 4, Children(b, {{2, mesh}, {3, gobObject}}));
    b.U32(mesh, b.String(u"DoorLow"));
    b.U32(mesh + 4, Children(b, {{3, meshObject}}));
    b.U32(meshObject + 4, 1);
    b.U32(meshObject + 12, Children(b, {{4, box}}));
    b.U32(box + 32, 2);
    Floats(b, box, {0, 0, 0, 1});
    Floats(b, box + 16, {1.38554F, 0, 1.9939F, 1});
    b.U32(box + 36, b.String(u"Box21"));
    FlagOn(b, box, 6067); // Placeables
    FlagOn(b, box, 6070); // Nonwalkable
    b.U32(box + 52, 5 | (std::uint32_t{gff4::kStruct} << 16));
    b.U32(box + 56, Ref(b, boxSize));
    Floats(b, boxSize, {1.35812F, 0.125529F, 1.97034F});
    b.U32(gobObject + 4, 2);
    b.U32(gobObject + 12, Children(b, {{4, capsule}}));
    b.U32(capsule + 32, 3);
    Floats(b, capsule, {0, 0, 0.7071F, 0.7071F});
    b.U32(capsule + 36, b.String(u"Capsule07"));
    FlagOn(b, capsule, 6069);                   // StaticGeometry, not fadeable
    FlagOn(b, capsule, 6305);                   // Walkable
    FlagOn(b, capsule, 6178);                   // Effects, which the engine does not read
    dao::test::Float(b, capsule + 40, 0.0343F); // its volume, as hm_rob_tmpa_0's Capsule07
    b.U8(capsule + 77, 1);                      // an emitter may spawn on it
    b.U32(capsule + 52, 6 | (std::uint32_t{gff4::kStruct} << 16));
    b.U32(capsule + 56, Ref(b, capsuleSize));
    Floats(b, capsuleSize, {0.687F, 1.778F});
    return b.bytes;
}

} // namespace

TEST_CASE("a placeable's shapes are read under their nodes with the masks the engine builds", "[collision]")
{
    const std::vector<std::byte> bytes = Door();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    const std::vector<collision::Shape> shapes = collision::ShapesOf(*file);
    REQUIRE(shapes.size() == 2);
    const collision::Shape &box = shapes[0];
    CHECK(box.name == L"Box21");
    CHECK(box.node == L"DoorLow");
    CHECK(box.kind == collision::Kind::Box);
    CHECK(box.objectType == 1);
    CHECK(box.dimensions[0] == Approx(1.35812F));
    CHECK(box.dimensions[2] == Approx(1.97034F));
    CHECK(box.position[0] == Approx(1.38554F));
    CHECK(box.position[2] == Approx(1.9939F));
    CHECK(box.rotation == std::array<float, 4>{0, 0, 0, 1});
    CHECK(box.mask == 0x81000004);
    CHECK_FALSE(box.allowEmitterSpawn);
    CHECK(box.volume == 0.0F);
    const collision::Shape &capsule = shapes[1];
    CHECK(capsule.node == L"GOB");
    CHECK(capsule.kind == collision::Kind::Capsule);
    CHECK(capsule.objectType == 2);
    CHECK(capsule.radius == Approx(0.687F));
    CHECK(capsule.height == Approx(1.778F));
    CHECK(capsule.rotation[3] == Approx(0.7071F));
    CHECK(capsule.mask == (0x80000000 | 0x02000000 | 0x2000 | 0x20));
    CHECK(collision::MaskOf(shapes) == (0x83002024));
    CHECK(capsule.allowEmitterSpawn);
    CHECK(capsule.volume == Approx(0.0343F));
}

TEST_CASE("a robe's shapes, when hm_rob_tmpa_0.phy is in game/models, all take spawns, with their volumes",
          "[collision][game]")
{
    const auto path =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "models" / "hm_rob_tmpa_0.phy";
    if (!std::filesystem::exists(path))
    {
        SKIP("no game/models/hm_rob_tmpa_0.phy (ghilana's extract writes it)");
    }
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(chars.size());
    std::memcpy(bytes.data(), chars.data(), chars.size());
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    const std::vector<collision::Shape> shapes = collision::ShapesOf(*file);
    REQUIRE(shapes.size() == 12);
    for (const collision::Shape &shape : shapes)
    {
        CHECK(shape.allowEmitterSpawn);
        CHECK(shape.volume > 0.0F);
    }
}

TEST_CASE("a shape's mask: All sets every bit, a fadeable static shape lacks 0x2000, no flag is 0", "[collision]")
{
    Builder b = Start("PHY ", {ShapTemplate()});
    const std::size_t all = Struct(b, 0x50);
    const std::size_t fadeable = Struct(b, 0x50);
    const std::size_t none = Struct(b, 0x50);
    FlagOn(b, all, 6063);
    FlagOn(b, fadeable, 6069);
    FlagOn(b, fadeable, 6252);
    FlagOn(b, fadeable, 6277); // Water
    FlagOn(b, fadeable, 6295); // TerrainWall
    FlagOn(b, fadeable, 6065); // Items
    FlagOn(b, fadeable, 6068); // Triggers
    FlagOn(b, fadeable, 6066); // Creatures
    FlagOn(b, none, 6064);     // None, read and left
    FlagOn(b, none, 6179);     // Waypoints, not read
    const std::optional<gff4::File> file = gff4::File::Parse(b.bytes);
    REQUIRE(file.has_value());
    CHECK(collision::MaskOf(gff4::Struct(*file, 0, all)) == 0xFFFFFFFF);
    CHECK(collision::MaskOf(gff4::Struct(*file, 0, fadeable)) == 0x8001012B);
    CHECK(collision::MaskOf(gff4::Struct(*file, 0, none)) == 0);
}

TEST_CASE("a file that is no .phy has no shapes", "[collision]")
{
    Builder b = Start("PWK ", {{"mdlh", 8, {{6000, 14, 0, 0}, {6999, dao::test::kGeneric, 0xA000, 4}}}});
    Struct(b, 8);
    const std::optional<gff4::File> file = gff4::File::Parse(b.bytes);
    REQUIRE(file.has_value());
    CHECK(collision::ShapesOf(*file).empty());
}

namespace
{

struct LevelCounts
{
    std::size_t actors = 0;
    std::array<std::size_t, 6> kinds{}; // by Kind
    std::size_t sight = 0;
    std::size_t meshPoints = 0;
    std::size_t meshTriangles = 0;
    std::size_t noRoom = 0;
};

LevelCounts Count(const collision::StaticPhysics &physics)
{
    LevelCounts counts;
    counts.actors = physics.actors.size();
    for (const collision::Actor &actor : physics.actors)
    {
        counts.noRoom += actor.room == -1 ? 1 : 0;
        for (const collision::StaticShape &shape : actor.shapes)
        {
            ++counts.kinds.at(static_cast<std::size_t>(shape.kind));
            counts.sight += collision::Meets(shape.groupsMask[0], collision::kSightMask) ? 1 : 0;
            CHECK(shape.record >= 0);
            if (shape.kind == collision::Kind::Mesh)
            {
                REQUIRE(shape.mesh >= 0);
            }
        }
    }
    for (const collision::TriangleMesh &mesh : physics.meshes)
    {
        counts.meshPoints += mesh.points.size();
        counts.meshTriangles += mesh.triangles.size();
    }
    return counts;
}

std::optional<collision::StaticPhysics> Opf(const std::string &name)
{
    const std::vector<std::byte> bytes = FromDisk(GameFolder() / name);
    return collision::StaticPhysicsOf(bytes);
}

} // namespace

TEST_CASE("lak302d and oth000d in game/collision, when there, read whole, the .arl's records the .opf's",
          "[collision][game]")
{
    if (!std::filesystem::exists(GameFolder() / "lak302d.opf") ||
        !std::filesystem::exists(GameFolder() / "oth000d.opf"))
    {
        SKIP("no game/collision/lak302d.opf and oth000d.opf (ghilana's extract writes them)");
    }
    // As tools/gff4.py and Python's XML reader counted them, 2026-10-18.
    const std::optional<collision::StaticPhysics> lak = Opf("lak302d.opf");
    REQUIRE(lak.has_value());
    CHECK(lak->xml);
    const LevelCounts tower = Count(*lak);
    CHECK(tower.actors == 964);
    CHECK(lak->shapeRecords.size() == 1278);
    CHECK(tower.kinds == std::array<std::size_t, 6>{0, 29, 917, 283, 0, 49}); // spheres, boxes, capsules, meshes
    CHECK(tower.sight == 1278);
    CHECK(tower.noRoom == 0);
    CHECK(lak->meshes.size() == 49);
    CHECK(tower.meshPoints == 818);
    CHECK(tower.meshTriangles == 1206);
    const collision::Actor &wall = lak->actors.front();
    CHECK(wall.name == "tvi_smwall_01_0.GOB");
    CHECK(wall.room == 1101);
    CHECK(wall.globalPose.translation == Point{12, -76, 0});
    CHECK(wall.shapes.front().groupsMask[0] == 0x81002020);

    const std::optional<collision::StaticPhysics> oth = Opf("oth000d.opf");
    REQUIRE(oth.has_value());
    const LevelCounts outside = Count(*oth);
    CHECK(outside.actors == 326);
    CHECK(outside.kinds == std::array<std::size_t, 6>{0, 0, 61, 915, 0, 51});
    CHECK(outside.sight == 1004); // all but the 23 terrain walls
    CHECK(outside.meshPoints == 36874);
    CHECK(outside.meshTriangles == 62618);

    if (std::filesystem::exists(GameFolder() / "lak302d.arl"))
    {
        const std::vector<std::byte> arlBytes = FromDisk(GameFolder() / "lak302d.arl");
        const std::optional<gff4::File> arl = gff4::File::Parse(arlBytes);
        REQUIRE(arl.has_value());
        const std::optional<dao::core::navigation::Grid> grid = dao::core::navigation::GridOf(*arl);
        REQUIRE(grid.has_value());
        const std::optional<collision::StaticPhysics> binary =
            collision::StaticPhysicsOf(std::as_bytes(std::span(grid->collision)));
        REQUIRE(binary.has_value());
        CHECK_FALSE(binary->xml);
        REQUIRE(binary->actorRecords.size() == lak->actorRecords.size());
        CHECK(binary->shapeRecords.size() == lak->shapeRecords.size());
        for (std::size_t i = 0; i < lak->actorRecords.size(); ++i)
        {
            CHECK(binary->actorRecords[i].room == lak->actorRecords[i].room);
            CHECK(binary->actorRecords[i].sound == lak->actorRecords[i].sound);
        }
    }
}

TEST_CASE("the game's .phy in game/collision, when there, the door's, crate's and statue's masks 0x81000004",
          "[collision][game][folder]")
{
    if (!std::filesystem::exists(GameFolder() / "plc_lrgwddrr_01_0.phy"))
    {
        SKIP("no game/collision/plc_lrgwddrr_01_0.phy (ghilana's extract writes it)");
    }
    const auto shapesOf = [](const std::string &name) {
        const std::vector<std::byte> bytes = FromDisk(GameFolder() / name);
        const std::optional<gff4::File> file = gff4::File::Parse(bytes);
        REQUIRE(file.has_value());
        return collision::ShapesOf(*file);
    };
    const std::vector<collision::Shape> door = shapesOf("plc_lrgwddrr_01_0.phy");
    REQUIRE(door.size() == 1);
    CHECK(door[0].name == L"Box21");
    CHECK(door[0].node == L"DoorLow");
    CHECK(door[0].dimensions[0] == Approx(1.35812F));
    CHECK(door[0].position[0] == Approx(1.38554F));
    CHECK(collision::MaskOf(door) == 0x81000004);
    const std::vector<collision::Shape> crate = shapesOf("plc_cratelarge_01_0.phy");
    REQUIRE(!crate.empty());
    CHECK(crate[0].name == L"Box12");
    CHECK(crate[0].dimensions[0] == Approx(0.567F).margin(1e-3));
    CHECK(crate[0].position[2] == Approx(0.326F).margin(1e-3));
    CHECK(collision::MaskOf(crate) == 0x81000004);
    const std::vector<collision::Shape> statue = shapesOf("plc_tevstatu03_0.phy");
    REQUIRE(!statue.empty());
    CHECK(statue[0].kind == collision::Kind::Capsule);
    CHECK(statue[0].radius == Approx(0.687F).margin(1e-3));
    CHECK(statue[0].height == Approx(1.778F).margin(1e-3));
    CHECK(collision::MaskOf(statue) == 0x81000004);

    // Every .phy the game takes, where ghilana's extract of *.phy wrote them: the shapes and masks a survey with
    // tools/gff4.py counted, 2026-10-18.
    const std::filesystem::path every = GameFolder() / "phy";
    if (!std::filesystem::exists(every))
    {
        return;
    }
    std::size_t files = 0;
    std::map<collision::Kind, std::size_t> kinds;
    std::map<std::uint32_t, std::size_t> masks;
    for (const auto &entry : std::filesystem::directory_iterator(every))
    {
        if (entry.path().extension() != ".phy")
        {
            continue;
        }
        ++files;
        const std::vector<std::byte> bytes = FromDisk(entry.path());
        const std::optional<gff4::File> file = gff4::File::Parse(bytes);
        INFO(entry.path().filename().string());
        REQUIRE(file.has_value());
        for (const collision::Shape &shape : collision::ShapesOf(*file))
        {
            ++kinds[shape.kind];
            ++masks[shape.mask];
        }
    }
    if (files == 17763)
    {
        CHECK(kinds == std::map<collision::Kind, std::size_t>{{collision::Kind::Sphere, 214},
                                                              {collision::Kind::Box, 3619},
                                                              {collision::Kind::Capsule, 20037},
                                                              {collision::Kind::Mesh, 7005}});
        CHECK(masks[0x81000002] == 19228);
        CHECK(masks[0x82002020] == 5030);
        CHECK(masks[0x81002020] == 3429);
        CHECK(masks[0x80010000] == 2116);
        CHECK(masks[0x81000004] == 656);
    }
}

namespace
{

// What differs between two reads of one level's static physics, by the field, with the first place of each.
class Differences
{
  public:
    void Note(const std::string &field, const std::string &where)
    {
        for (Entry &entry : entries_)
        {
            if (entry.field == field)
            {
                ++entry.count;
                return;
            }
        }
        entries_.push_back({field, 1, where});
    }

    // Notes the field where the two are not equal.
    template <class T> void Equal(const char *field, const T &a, const T &b, const std::string &where)
    {
        if (!(a == b))
        {
            Note(field, where);
        }
    }

    // Notes the field where two lists of numbers differ other than as the .opf's text would: it writes a float with
    // nine decimals (0.00000182 for 1.82e-6, and trailing zeros left off), so a is the XML's number and b is the
    // binary's, which printed that way and read again must give a. A number exactly between two nine-decimal ones
    // (-0.0087890625) is rounded away from zero there, where printf rounds it to even, so both are tried. One that is
    // not b itself is counted in rounded().
    template <class T> void Near(const char *field, const T &a, const T &b, const std::string &where)
    {
        for (std::size_t i = 0; i < a.size() && i < b.size(); ++i)
        {
            const auto value = static_cast<double>(b[i]);
            const double away = std::nextafter(value, value < 0 ? -HUGE_VAL : HUGE_VAL);
            if (a[i] != b[i] && a[i] != Printed(value) && a[i] != Printed(away))
            {
                Note(field, where + " " + std::to_string(a[i]) + " against " + std::to_string(b[i]));
                return;
            }
            rounded_ += a[i] == b[i] ? 0 : 1;
        }
    }

    std::size_t rounded() const
    {
        return rounded_;
    }

    std::size_t Total() const
    {
        std::size_t total = 0;
        for (const Entry &entry : entries_)
        {
            total += entry.count;
        }
        return total;
    }

    std::string Summary() const
    {
        std::string text;
        for (const Entry &entry : entries_)
        {
            text += entry.field + " x" + std::to_string(entry.count) + " (first " + entry.first + ") ";
        }
        return text;
    }

  private:
    // A number written with nine decimals and read again as a float.
    static float Printed(double value)
    {
        std::array<char, 400> text{};
        std::snprintf(text.data(), text.size(), "%.9f", value);
        return std::strtof(text.data(), nullptr);
    }

    struct Entry
    {
        std::string field;
        std::size_t count;
        std::string first;
    };

    std::size_t rounded_ = 0;
    std::vector<Entry> entries_;
};

void CompareShapes(const collision::Actor &a, const collision::Actor &b, Differences &out)
{
    if (a.shapes.size() != b.shapes.size())
    {
        out.Note("shape count", a.id);
        return;
    }
    for (std::size_t i = 0; i < a.shapes.size(); ++i)
    {
        const collision::StaticShape &x = a.shapes[i];
        const collision::StaticShape &y = b.shapes[i];
        const std::string where = a.id + "/" + std::to_string(i);
        out.Equal("shape name", x.name, y.name, where);
        out.Equal("shape kind", x.kind, y.kind, where);
        out.Near("shape dimensions", x.dimensions, y.dimensions, where);
        out.Near("shape radius", std::array{x.radius}, std::array{y.radius}, where);
        out.Near("shape height", std::array{x.height}, std::array{y.height}, where);
        out.Equal("shape mesh", x.mesh, y.mesh, where);
        out.Near("shape pose turn", x.localPose.rotation, y.localPose.rotation, where);
        out.Near("shape pose place", x.localPose.translation, y.localPose.translation, where);
        out.Equal("shape group", x.group, y.group, where);
        out.Equal("shape mask", x.groupsMask, y.groupsMask, where);
        out.Equal("shape record", x.record, y.record, where);
    }
}

Differences Compare(const collision::StaticPhysics &xml, const collision::StaticPhysics &binary)
{
    Differences out;
    out.Equal("mesh count", xml.meshes.size(), binary.meshes.size(), "");
    for (std::size_t i = 0; i < xml.meshes.size() && i < binary.meshes.size(); ++i)
    {
        const std::string &where = xml.meshes[i].id;
        out.Equal("mesh id", xml.meshes[i].id, binary.meshes[i].id, where);
        for (std::size_t p = 0; p < xml.meshes[i].points.size() && p < binary.meshes[i].points.size(); ++p)
        {
            out.Near("mesh points", xml.meshes[i].points[p], binary.meshes[i].points[p], where);
        }
        out.Equal("mesh point count", xml.meshes[i].points.size(), binary.meshes[i].points.size(), where);
        out.Equal("mesh triangles", xml.meshes[i].triangles, binary.meshes[i].triangles, where);
    }
    out.Equal("actor count", xml.actors.size(), binary.actors.size(), "");
    for (std::size_t i = 0; i < xml.actors.size() && i < binary.actors.size(); ++i)
    {
        const collision::Actor &x = xml.actors[i];
        const collision::Actor &y = binary.actors[i];
        out.Equal("actor id", x.id, y.id, x.id);
        out.Equal("actor name", x.name, y.name, x.id);
        out.Equal("actor hasBody", x.hasBody, y.hasBody, x.id);
        out.Near("actor pose turn", x.globalPose.rotation, y.globalPose.rotation, x.id);
        out.Near("actor pose place", x.globalPose.translation, y.globalPose.translation, x.id);
        out.Equal("actor group", x.group, y.group, x.id);
        out.Equal("actor record", x.record, y.record, x.id);
        out.Equal("actor room", x.room, y.room, x.id);
        CompareShapes(x, y, out);
    }
    out.Equal("actor records", xml.actorRecords.size(), binary.actorRecords.size(), "");
    out.Equal("shape records", xml.shapeRecords.size(), binary.shapeRecords.size(), "");
    return out;
}

// A level's static physics from its .arl's EnvStatPhysData.
std::optional<collision::StaticPhysics> ArlPhysics(const std::vector<std::byte> &arlBytes)
{
    const std::optional<gff4::File> arl = gff4::File::Parse(arlBytes);
    const std::optional<dao::core::navigation::Grid> grid = arl ? dao::core::navigation::GridOf(*arl) : std::nullopt;
    if (!grid || grid->collision.empty())
    {
        return std::nullopt;
    }
    return collision::StaticPhysicsOf(std::as_bytes(std::span(grid->collision)));
}

} // namespace

// Every level of the GOG install the index can read, its .arl's binary stream against its .opf's XML where it has
// both (DAOLib #197). Hidden, as it reads the install: dao_core_tests "[collision-real]".
TEST_CASE("on the GOG install, the .arl's binary stream reads as the .opf's XML does", "[.][collision-real]")
{
    const std::filesystem::path game = L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins";
    if (!std::filesystem::exists(game / L"bin_ship"))
    {
        SKIP("no GOG install here");
    }
    const auto index = dao::core::install::Index::Build(game, {});
    std::size_t levels = 0;
    std::size_t compared = 0;
    std::size_t same = 0;
    std::size_t withShapes = 0;
    std::size_t unreadable = 0;
    std::size_t noStream = 0;
    std::size_t actors = 0;
    std::size_t shapes = 0;
    std::size_t triangles = 0;
    std::size_t roundedNumbers = 0;
    for (const auto &[name, copies] : index->Resources())
    {
        if (!name.ends_with(".arl"))
        {
            continue;
        }
        if (copies.empty() || !index->Readable(copies.front()))
        {
            ++unreadable;
            continue;
        }
        INFO(name);
        const std::optional<std::vector<std::byte>> arlBytes = index->Read(copies.front());
        REQUIRE(arlBytes.has_value());
        const std::optional<collision::StaticPhysics> binary = ArlPhysics(*arlBytes);
        if (!binary)
        {
            ++noStream;
            continue;
        }
        ++levels;
        CHECK(binary->binary);
        std::size_t shapeCount = 0;
        for (const collision::Actor &actor : binary->actors)
        {
            shapeCount += actor.shapes.size();
            for (const collision::StaticShape &shape : actor.shapes)
            {
                CHECK(shape.record >= 0);
            }
        }
        // The stream's counts against the records after it, which the engine points userData at.
        CHECK(binary->actors.size() == binary->actorRecords.size());
        CHECK(shapeCount == binary->shapeRecords.size());
        actors += binary->actors.size();
        shapes += shapeCount;
        withShapes += shapeCount > 0 ? 1 : 0;
        for (const collision::TriangleMesh &mesh : binary->meshes)
        {
            triangles += mesh.triangles.size();
        }
        const auto *opf = index->Find(name.substr(0, name.size() - 4) + ".opf");
        if (opf == nullptr || !index->Readable(opf->front()))
        {
            continue;
        }
        const std::optional<std::vector<std::byte>> opfBytes = index->Read(opf->front());
        REQUIRE(opfBytes.has_value());
        const std::optional<collision::StaticPhysics> xml = collision::StaticPhysicsOf(*opfBytes);
        REQUIRE(xml.has_value());
        REQUIRE(xml->xml);
        ++compared;
        const Differences differences = Compare(*xml, *binary);
        same += differences.Total() == 0 ? 1 : 0;
        roundedNumbers += differences.rounded();
        if (differences.Total() != 0)
        {
            WARN(name << ": " << differences.Summary());
        }
        CHECK(differences.Total() == 0);
    }
    WARN(levels << " levels with a stream, " << withShapes << " with shapes read, " << compared
                << " compared with an .opf, " << same << " the same; " << unreadable << " .arl unreadable, " << noStream
                << " without a stream; " << actors << " actors, " << shapes << " shapes, " << triangles
                << " triangles; " << roundedNumbers << " groups of numbers differ only by the .opf's rounding");
}

namespace
{

using Bytes = std::vector<std::byte>;

void Put(Bytes &out, std::uint32_t value)
{
    Word(out, value);
}

void Put(Bytes &out, float value)
{
    std::uint32_t word = 0;
    std::memcpy(&word, &value, 4);
    Word(out, word);
}

// A string as NXU's binary stream has it: its length, then for a length over 0 its characters and a NUL.
void PutString(Bytes &out, std::string_view text)
{
    Put(out, static_cast<std::uint32_t>(text.size()));
    if (!text.empty())
    {
        for (const char c : text)
        {
            out.push_back(static_cast<std::byte>(c));
        }
        out.push_back(std::byte{0});
    }
}

// A pose as the stream has it: each row of the turn, then that row's translation.
void PutPose(Bytes &out, const collision::Pose &pose)
{
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            Put(out, pose.rotation[(row * 3) + column]);
        }
        Put(out, pose.translation[row]);
    }
}

void Append(Bytes &out, const Bytes &more)
{
    out.insert(out.end(), more.begin(), more.end());
}

// A block: its head (0x00ABCDEF, the type, the length with the head, 0x01234567), its fields, the blocks it holds.
Bytes BlockOf(std::uint32_t type, const Bytes &fields, const std::vector<Bytes> &children = {})
{
    Bytes body = fields;
    for (const Bytes &child : children)
    {
        Append(body, child);
    }
    Bytes out;
    Put(out, std::uint32_t{0x00ABCDEF});
    Put(out, type);
    Put(out, static_cast<std::uint32_t>(16 + body.size()));
    Put(out, std::uint32_t{0x01234567});
    Append(out, body);
    return out;
}

// The block types of the classes the reader takes, which move with the SDK's version.
struct Numbers
{
    std::uint32_t sdk;
    std::uint32_t shapeDesc;
    std::uint32_t box;
    std::uint32_t sphere;
    std::uint32_t capsule;
    std::uint32_t mesh;
    std::uint32_t actor;
};
constexpr Numbers kSdk272{272, 31, 32, 34, 35, 38, 41};
constexpr Numbers kSdk281{281, 32, 33, 35, 36, 39, 42};

// An NxShapeDesc as the stream has it: the fields before its groups mask, the mask in a block of its own.
Bytes ShapeDescOf(const Numbers &n, std::string_view name, const collision::Pose &pose, std::uint32_t group,
                  std::uint32_t bits0)
{
    Bytes fields;
    PutString(fields, "");
    PutString(fields, name);
    PutPose(fields, pose);
    for (const float x : {0.0F, 0.0F, 0.0F, 1.0F})
    {
        Put(fields, x);
    }
    Put(fields, std::uint32_t{8}); // the shape flags
    Put(fields, group);
    fields.resize(fields.size() + 16); // the material, the densities and the skin width
    Bytes mask;
    for (const std::uint32_t bits : {bits0, 0U, 0U, 0U})
    {
        Put(mask, bits);
    }
    return BlockOf(n.shapeDesc, fields, {BlockOf(17, mask)});
}

collision::Pose Turned(float x, float y, float z)
{
    collision::Pose pose;
    pose.rotation = {0, 1, 0, -1, 0, 0, 0, 0, 1};
    pose.translation = {x, y, z};
    return pose;
}

// A mesh as the stream has it: a block with its name and a block of the points, the triangles and the flags.
Bytes MeshOf(std::string_view id, bool indices16)
{
    Bytes head;
    PutString(head, id);
    head.resize(head.size() + 40); // the description's other fields
    Bytes mesh;
    Put(mesh, std::uint32_t{4});
    for (const float x : {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F})
    {
        Put(mesh, x);
    }
    Put(mesh, std::uint32_t{2});
    for (const std::uint32_t index : {0U, 1U, 2U, 3U, 2U, 1U})
    {
        if (indices16)
        {
            mesh.push_back(static_cast<std::byte>(index));
            mesh.push_back(std::byte{0});
        }
        else
        {
            Put(mesh, index);
        }
    }
    Put(mesh, std::uint32_t{indices16 ? 2U : 0U});
    return BlockOf(9, head, {BlockOf(7, mesh)});
}

// A level as lak302d's stream is laid out: a collection holding the SDK's description, the meshes and a scene of
// actors; the first a wall of a box and a sphere turned about z, the second a prop of a capsule and a mesh.
Bytes BinaryStream(const Numbers &n, bool indices16 = false, std::uint32_t boxBlockType = 0)
{
    Bytes wall;
    PutString(wall, "Actor_0");
    PutString(wall, "1,0,1");
    Put(wall, std::uint32_t{0});
    PutString(wall, "tvi_smwall_01_0.GOB");
    PutPose(wall, Turned(12, -76, 0));
    for (const float x : {0.0F, 0.0F, 0.0F, 1.0F})
    {
        Put(wall, x);
    }
    Put(wall, 10.0F);
    Put(wall, std::uint32_t{0x40});
    Put(wall, std::uint32_t{5});
    wall.resize(wall.size() + 2);
    Bytes box;
    for (const float x : {4.0F, 0.5F, 2.75F})
    {
        Put(box, x);
    }
    Bytes sphere;
    Put(sphere, 0.25F);
    collision::Pose raised;
    raised.translation = {0, 0, 2.5F};
    const Bytes wallBlock = BlockOf(
        n.actor, wall,
        {BlockOf(boxBlockType != 0 ? boxBlockType : n.box, box, {ShapeDescOf(n, "ColNWBox", raised, 5, 0x81002020)}),
         BlockOf(n.sphere, sphere, {ShapeDescOf(n, "Sphere", {}, 5, 0x80002020)})});

    Bytes prop;
    PutString(prop, "Actor_1");
    PutString(prop, "0,2,3");
    Put(prop, std::uint32_t{0});
    PutString(prop, "prp_a.GOB");
    PutPose(prop, {});
    for (const float x : {0.0F, 0.0F, 0.0F, 1.0F})
    {
        Put(prop, x);
    }
    Put(prop, 10.0F);
    Put(prop, std::uint32_t{0x40});
    Put(prop, std::uint32_t{0});
    prop.resize(prop.size() + 8);
    Bytes capsule;
    Put(capsule, 0.5F);
    Put(capsule, 3.0F);
    Put(capsule, std::uint32_t{0});
    Bytes meshShape;
    PutString(meshShape, "TriangleMesh_1");
    meshShape.resize(meshShape.size() + 8);
    const Bytes propBlock = BlockOf(n.actor, prop,
                                    {BlockOf(n.capsule, capsule, {ShapeDescOf(n, "Capsule", {}, 5, 0x01001020)}),
                                     BlockOf(n.mesh, meshShape, {ShapeDescOf(n, "Mesh", {}, 5, 0x80010000)})});

    Bytes collectionFields;
    PutString(collectionFields, "Physics");
    Put(collectionFields, std::uint32_t{0});
    Put(collectionFields, n.sdk);
    Put(collectionFields, std::uint32_t{103});
    Bytes description;
    for (const std::uint32_t x : {65536U, 256U, 2048U})
    {
        Put(description, x);
    }
    Bytes sceneFields(44, std::byte{0});
    const Bytes collection = BlockOf(n.sdk == 272 ? 99 : 106, collectionFields,
                                     {BlockOf(n.sdk == 272 ? 98 : 105, description), MeshOf("TriangleMesh_0", false),
                                      MeshOf("TriangleMesh_1", indices16),
                                      BlockOf(n.sdk == 272 ? 95 : 102, sceneFields, {wallBlock, propBlock})});

    Bytes stream;
    for (const char c : std::string_view("NXUSTREAM\0\0", 11))
    {
        stream.push_back(static_cast<std::byte>(c));
    }
    Put(stream, n.sdk);
    Put(stream, std::uint32_t{103});
    Append(stream, BlockOf(0, {}, {collection}));
    Append(stream, BlockOf(n.sdk == 272 ? 101 : 108, {}));
    return stream;
}

Bytes BinaryLevel(const Bytes &stream)
{
    return Framed(std::string_view(reinterpret_cast<const char *>(stream.data()), stream.size()),
                  {{-1, 0, 1103, 1}, {-1, 0, 1325, 3}}, {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}});
}

void CheckBinaryLevel(const collision::StaticPhysics &physics)
{
    CHECK_FALSE(physics.xml);
    CHECK(physics.binary);
    REQUIRE(physics.meshes.size() == 2);
    CHECK(physics.meshes[1].id == "TriangleMesh_1");
    CHECK(physics.meshes[0].points.size() == 4);
    CHECK(physics.meshes[0].points[1] == Point{1, 0, 0});
    REQUIRE(physics.meshes[0].triangles.size() == 2);
    CHECK(physics.meshes[0].triangles[1] == std::array<std::uint32_t, 3>{3, 2, 1});
    CHECK(physics.meshes[1].triangles == physics.meshes[0].triangles);

    REQUIRE(physics.actors.size() == 2);
    const collision::Actor &wall = physics.actors[0];
    CHECK(wall.id == "Actor_0");
    CHECK(wall.name == "tvi_smwall_01_0.GOB");
    CHECK_FALSE(wall.hasBody);
    CHECK(wall.group == 5);
    CHECK(wall.globalPose.rotation == std::array<float, 9>{0, 1, 0, -1, 0, 0, 0, 0, 1});
    CHECK(wall.globalPose.translation == Point{12, -76, 0});
    CHECK(wall.record == 1);
    CHECK(wall.room == 1325);
    REQUIRE(wall.shapes.size() == 2);
    CHECK(wall.shapes[0].kind == collision::Kind::Box);
    CHECK(wall.shapes[0].name == "ColNWBox");
    CHECK(wall.shapes[0].dimensions == Point{4, 0.5F, 2.75F});
    CHECK(wall.shapes[0].localPose.translation == Point{0, 0, 2.5F});
    CHECK(wall.shapes[0].group == 5);
    CHECK(wall.shapes[0].groupsMask == std::array<std::uint32_t, 4>{0x81002020, 0, 0, 0});
    CHECK(wall.shapes[0].record == 0);
    CHECK(wall.shapes[1].kind == collision::Kind::Sphere);
    CHECK(wall.shapes[1].radius == 0.25F);
    CHECK(wall.shapes[1].groupsMask[0] == 0x80002020);
    CHECK(wall.shapes[1].record == 1);

    const collision::Actor &prop = physics.actors[1];
    CHECK(prop.record == 0);
    CHECK(prop.room == 1103);
    CHECK(prop.group == 0);
    REQUIRE(prop.shapes.size() == 2);
    CHECK(prop.shapes[0].kind == collision::Kind::Capsule);
    CHECK(prop.shapes[0].radius == 0.5F);
    CHECK(prop.shapes[0].height == 3.0F);
    CHECK(prop.shapes[0].groupsMask[0] == 0x01001020);
    CHECK(prop.shapes[0].record == 2);
    CHECK(prop.shapes[1].kind == collision::Kind::Mesh);
    CHECK(prop.shapes[1].mesh == 1);
    CHECK(prop.shapes[1].groupsMask[0] == 0x80010000);
    CHECK(prop.shapes[1].record == 3);
    CHECK(physics.actorRecords.size() == 2);
    CHECK(physics.shapeRecords.size() == 4);
}

} // namespace

TEST_CASE("a binary stream's meshes, actors, shapes, poses and masks are read as the XML's are", "[collision]")
{
    const Bytes bytes = BinaryLevel(BinaryStream(kSdk272));
    const std::optional<collision::StaticPhysics> physics = collision::StaticPhysicsOf(bytes);
    REQUIRE(physics.has_value());
    CheckBinaryLevel(*physics);
}

TEST_CASE("a binary stream of the later SDK has its shapes and actors under other block types", "[collision]")
{
    const Bytes bytes = BinaryLevel(BinaryStream(kSdk281));
    const std::optional<collision::StaticPhysics> physics = collision::StaticPhysicsOf(bytes);
    REQUIRE(physics.has_value());
    CheckBinaryLevel(*physics);
}

TEST_CASE("a mesh's indices are 16 bits wide when its flags say so", "[collision]")
{
    const Bytes bytes = BinaryLevel(BinaryStream(kSdk272, true));
    const std::optional<collision::StaticPhysics> physics = collision::StaticPhysicsOf(bytes);
    REQUIRE(physics.has_value());
    CheckBinaryLevel(*physics);
}

TEST_CASE("a binary stream it cannot read whole leaves the records and no shapes", "[collision]")
{
    SECTION("a version of the SDK it has no block types for")
    {
        Bytes stream = BinaryStream(kSdk272);
        const std::uint32_t other = 300;
        std::memcpy(stream.data() + 11, &other, 4);
        const std::optional<collision::StaticPhysics> physics = collision::StaticPhysicsOf(BinaryLevel(stream));
        REQUIRE(physics.has_value());
        CHECK_FALSE(physics->binary);
        CHECK(physics->actors.empty());
        CHECK(physics->meshes.empty());
        CHECK(physics->actorRecords.size() == 2);
    }
    SECTION("a shape of a kind it does not read")
    {
        const std::optional<collision::StaticPhysics> physics =
            collision::StaticPhysicsOf(BinaryLevel(BinaryStream(kSdk272, false, 37)));
        REQUIRE(physics.has_value());
        CHECK_FALSE(physics->binary);
        CHECK(physics->actors.empty());
        CHECK(physics->shapeRecords.size() == 4);
    }
    SECTION("a mesh whose points and triangles do not fill its block")
    {
        Bytes stream = BinaryStream(kSdk272);
        // The first mesh's point count, 4 after its name block's head: raise it past what the block holds.
        const std::array<std::uint8_t, 4> marker{0x04, 0, 0, 0};
        const auto at =
            std::search(stream.begin() + 100, stream.end(), reinterpret_cast<const std::byte *>(marker.data()),
                        reinterpret_cast<const std::byte *>(marker.data()) + 4);
        REQUIRE(at != stream.end());
        *at = std::byte{0x40};
        const std::optional<collision::StaticPhysics> physics = collision::StaticPhysicsOf(BinaryLevel(stream));
        REQUIRE(physics.has_value());
        CHECK_FALSE(physics->binary);
        CHECK(physics->meshes.empty());
    }
    SECTION("a stream cut short")
    {
        Bytes stream = BinaryStream(kSdk272);
        stream.resize(stream.size() / 2);
        const std::optional<collision::StaticPhysics> physics = collision::StaticPhysicsOf(BinaryLevel(stream));
        REQUIRE(physics.has_value());
        CHECK_FALSE(physics->binary);
        CHECK(physics->actors.empty());
        CHECK(physics->actorRecords.size() == 2);
    }
}
