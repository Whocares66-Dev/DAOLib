// What the engine's physics knows of a level and of a placeable, out of the
// game's files (DAOLib #189): the shapes a ray between two creatures' eyes
// meets, each with the mask the ray tests, and the room of each piece of the
// level, for a reimplementation of perception (DAOParagonEngine,
// dev/PARAGON_PERCEPTION.md).
//
// A level's static physics is <level>.opf in packages/core/env/, a PhysX
// collection: a u32 of the file's length, a u32 of the stream's, the stream
// (NxuStream2 XML, one NxActorDesc an actor), then a u32 count and as many
// 16-byte actor records, a u32 count and as many 12-byte shape records. The
// engine loads the same collection from the .arl's EnvStatPhys (3744)
// EnvStatPhysData (3745), dao/core/Grid.h's Grid::collision, framed the same
// way but with the stream in NXU's binary form, which this reads the records
// of and not the stream. An actor's userProperties are "a,b,c...": its
// record's index, then each of its shapes' in turn.
//
// A placeable's shapes are in its model's <model>.phy, GFF4 "PHY " in
// modelhierarchies.erf: a tree of nodes (MMHChildren) in which each "cobj"
// holds its "shap" shapes. The engine builds each shape's mask from its
// MMHShapeCollisionMask* flags, and keeps the OR of every shape's at the
// placeable's +0x6D8 (a door's 0x81000004).
//
// A shape meets a ray when (its bits0 & the ray's mask) != 0: the eye-to-eye
// ray's mask is 0x24, the ray down that finds a creature's room 0x20.
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/RayCast.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace dao::core::collision
{

// The bits of a shape's mask (bits0), as the engine builds them from a .phy's MMHShapeCollisionMask* flags.
inline constexpr std::uint32_t kItems = 0x1;
inline constexpr std::uint32_t kCreatures = 0x2;
inline constexpr std::uint32_t kPlaceables = 0x4;
inline constexpr std::uint32_t kTriggers = 0x8;
inline constexpr std::uint32_t kStaticGeometry = 0x20;
inline constexpr std::uint32_t kWater = 0x100;
inline constexpr std::uint32_t kNotFadeable = 0x2000; // static geometry that is not MMHShapeFadeable
inline constexpr std::uint32_t kTerrainWall = 0x10000;
inline constexpr std::uint32_t kNonwalkable = 0x01000000;
inline constexpr std::uint32_t kWalkable = 0x02000000;
inline constexpr std::uint32_t kAny = 0x80000000; // set with any other bit
inline constexpr std::uint32_t kAll = 0xFFFFFFFF;

// The mask of the ray between two creatures' eyes (0x0067C530): what blocks sight.
inline constexpr std::uint32_t kSightMask = kPlaceables | kStaticGeometry;
// The mask of the ray cast down from above a creature to find its room.
inline constexpr std::uint32_t kRoomMask = kStaticGeometry;

// Returns whether a shape of mask `bits0` meets a ray of mask `rayMask`, as the engine's filter tests it.
constexpr bool Meets(std::uint32_t bits0, std::uint32_t rayMask)
{
    return (bits0 & rayMask) != 0;
}

// A shape's kind: MMHShapeType's value in a .phy, and the same for an .opf's shape element.
enum class Kind : std::uint32_t
{
    Unknown = 0,
    Sphere = 1,  // NxSphereShapeDesc, sphs
    Box = 2,     // NxBoxShapeDesc, boxs
    Capsule = 3, // NxCapsuleShapeDesc, caps
    Convex = 4,  // cvxs; seen in no file
    Mesh = 5,    // NxTriangleMeshShapeDesc, mshs
};

// A pose as the .opf has it: a turn as a 3x3 matrix by rows, then a translation. A point p is at turn * p +
// translation.
struct Pose
{
    std::array<float, 9> rotation{1, 0, 0, 0, 1, 0, 0, 0, 1};
    Point translation{};
};

// Returns `local` placed by `outer`: a shape's localPose by its actor's globalPose is the shape's pose in the level.
Pose Compose(const Pose &outer, const Pose &local);

// Returns a point placed by a pose.
Point Apply(const Pose &pose, const Point &point);

// A level's triangle mesh, an NxTriangleMeshDesc's NxSimpleTriangleMesh: its points and its triangles' indices.
struct TriangleMesh
{
    std::string id; // what a shape's meshData names
    std::vector<Point> points;
    std::vector<std::array<std::uint32_t, 3>> triangles;
};

// One shape of a level's actor.
struct StaticShape
{
    std::string name;
    Kind kind = Kind::Unknown;
    Point dimensions{};                        // a box's half extents
    float radius = 0;                          // a sphere's or a capsule's
    float height = 0;                          // a capsule's, between its caps' centres along its y
    std::int32_t mesh = -1;                    // a mesh shape's mesh: its index in StaticPhysics::meshes
    Pose localPose;                            // in its actor's space
    std::uint32_t group = 0;                   // the file's; the engine puts every one in group 5 at load
    std::array<std::uint32_t, 4> groupsMask{}; // bits0 to bits3; bits0 is what a ray tests
    std::int32_t record = -1;                  // its record's index, from its actor's userProperties
};

// One actor of a level: a model placed in it, with its shapes.
struct Actor
{
    std::string id;   // "Actor_0"
    std::string name; // the model's, "tvi_smwall_01_0.GOB"
    bool hasBody = false;
    Pose globalPose;
    std::uint32_t group = 0;
    std::vector<StaticShape> shapes;
    std::int32_t record = -1; // its record's index, from its userProperties
    std::int32_t room = -1;   // its record's room id: the .arl's EnvRoomID; -1 without a record
};

// An actor's record after the stream: what the engine points the actor's userData at.
struct ActorRecord
{
    std::int32_t first = -1; // -1 in every file
    std::int32_t second = 0; // 0 in every file
    std::int32_t room = -1;  // the room's EnvRoomID
    std::int32_t sound = 0;  // 0 to 15; inferred to be the floor's sound material
};

// A shape's record after the stream, as its three words; what they mean is not read.
struct ShapeRecord
{
    std::array<std::uint32_t, 3> words{};
};

// A level's static physics.
struct StaticPhysics
{
    bool xml = false; // the stream is XML, which the meshes and actors are read from; false for a binary stream
    std::vector<TriangleMesh> meshes;
    std::vector<Actor> actors;
    std::vector<ActorRecord> actorRecords;
    std::vector<ShapeRecord> shapeRecords;
};

// Returns a level's static physics from an .opf's bytes or EnvStatPhysData's; nothing if they are not so framed.
// A binary stream gives the records alone.
std::optional<StaticPhysics> StaticPhysicsOf(std::span<const std::byte> bytes);

// One shape of a placeable's model.
struct Shape
{
    std::wstring name; // MMHShapeName
    std::wstring node; // the MMHName of the nearest node above it: "GOB", or a door's mesh node
    Kind kind = Kind::Unknown;
    Point dimensions{};                        // MMHShapeBoxDim, a box's half extents
    float radius = 0;                          // MMHShapeRadius
    float height = 0;                          // MMHShapeHeight
    std::array<float, 4> rotation{0, 0, 0, 1}; // MMHShapeRotation: x, y, z, w
    Point position{};                          // MMHShapePosition
    std::uint32_t mask = 0;                    // bits0, built from the flags as the engine builds it
    bool fadeable = false;                     // MMHShapeFadeable
    std::uint32_t objectType = 0;              // its cobj's MMHNodeCollisionObjType: 1 or 2
};

// Returns a placeable model's shapes, in the file's order; none for a file that is no .phy.
std::vector<Shape> ShapesOf(const gff4::File &phy);

// Returns the OR of shapes' masks: what the engine keeps at a placeable's +0x6D8.
std::uint32_t MaskOf(std::span<const Shape> shapes);

// Returns the mask the engine builds from a .phy shape's MMHShapeCollisionMask* flags (0x00589DB0).
std::uint32_t MaskOf(const gff4::Struct &shap);

} // namespace dao::core::collision
