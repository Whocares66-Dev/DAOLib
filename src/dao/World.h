// Questions about the world's geometry: what a ray hits, whether one point can be seen from
// another, where a point appears on the screen, the paths creatures walk, the size of creature
// models, and the rooms of the current area.
//
// Call these on the game thread. Once the first frame has run (see dao::FrameThreadId), a call
// from any other thread returns an empty result: no hits, no line of sight, no path.
//
//   for (const dao::world::Hit &hit : dao::world::Raycast(eye, forward))
//       ...
//   if (dao::world::LineOfSight(archer, target))
//       ...
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dao
{
class CCArea;
}

namespace dao::world
{

// Notes on the groups, read 2026-09-30 from the code that tests them: which bit is
// which kind of object or ground is not yet told apart beyond what is named. A hit
// with any of kObjects and an object id is an object; with none of them and any of
// kGround, the ground (0x00648B70). kCreatureBodies: in play, 2026-09-30, a ray down
// onto the controlled creature met it first with only this group and the creature's
// id, which the engine's own sorting leaves as Nothing. The cursor pick's mask is
// read at 0x0070B590. LineOfSight looks a hit with kPlaceables up as a placeable, and
// Raycast's `selectionVolumes` adds kSelection to the mask.

// Collision groups. Every shape belongs to some groups, given as bits, and every query has a
// mask of the same bits. A query hits a shape when the two share a bit.
namespace groups
{
// Objects: creatures, placeables and the like.
inline constexpr std::uint32_t kObjects = 0x1 | 0x2 | 0x4 | 0x20000;
// The area's ground: walkmesh and terrain.
inline constexpr std::uint32_t kGround = 0x8 | 0x20;
// Placeables.
inline constexpr std::uint32_t kPlaceables = 0x4;
// A creature's body. The game counts a hit on it as neither an object nor the ground.
inline constexpr std::uint32_t kCreatureBodies = 0x10;
// The larger volume around an object that the cursor selects it by, in addition to its body.
inline constexpr std::uint32_t kSelection = 0x20000;
// Ground that cannot be walked on. Explore mode refuses to move the party there.
inline constexpr std::uint32_t kNotWalkable = 0x01000000;

// The mask the cursor uses to find what is under it.
inline constexpr std::uint32_t kCursor = 0x2607;
// The mask of the game's line-of-sight test: placeables and one of the ground bits (0x20).
inline constexpr std::uint32_t kSight = 0x24;
} // namespace groups

// Notes on Hit: 0x30 bytes, read 2026-09-30 from its constructor (0x00648B10) and
// World::Raycast, which fills each from PhysX's hit (the physics' own, 0x00829900).
// Explore mode keeps the cursor's 32 of these (dao/GameModes.h). `unknown20` comes
// from the shape's owner (its slot 18) and is not read yet.

// One shape that a ray hit, laid out exactly as the game stores it.
struct Hit
{
    // What kind of shape the ray hit.
    enum class Kind : std::int32_t
    {
        Nothing = 0, // a shape in neither kind of group, such as a creature's body
        Ground = 1,  // the area's ground, walkmesh or terrain
        Object = 2,  // a creature, placeable or other object; `object` is its id
    };

    Vector position; // where the ray hit the shape
    std::uint32_t padding0 = 0;
    Kind kind = Kind::Nothing;
    ObjectId object = kInvalidObject; // the shape's object, or kInvalidObject
    // True if the shape is not in groups::kNotWalkable. In explore mode, a click moves the
    // party only to walkable ground.
    bool walkable = false;
    std::uint32_t unknown1c = 0;
    float unknown20 = 0;
    std::uint32_t groups = 0; // the shape's collision groups
    std::uint32_t unknown28 = 0;
    std::uint32_t padding2c = 0;

    // Returns true if the ray hit the area's ground.
    bool IsGround() const
    {
        return kind == Kind::Ground;
    }
    // Returns the object the shape belongs to. This also works for a creature's body, which
    // the game classes as Nothing but still tags with the creature's id.
    // @return nullopt if the shape belongs to no object
    std::optional<ObjectId> Object() const
    {
        return object != kInvalidObject ? std::optional<ObjectId>(object) : std::nullopt;
    }
};
static_assert(sizeof(Hit) == 0x30);

// Notes on kEyeHeight: read from CheckLineOfSightObject (0x00668DB0).

// The height above a creature's position, in metres, that the scripts' line-of-sight test
// looks from and to.
inline constexpr float kEyeHeight = 1.8F;

// Returns the area the player is in.
// @return null before a game is loaded
CCArea *CurrentArea();

// Returns every shape along a ray in the current area, nearest first.
// @param origin where the ray starts
// @param direction the ray's direction, of any non-zero length; the ray has no end
// @param mask the collision groups to hit; groups::kCursor by default
// @param selectionVolumes true to also hit objects' selection volumes, as the cursor does.
// An object may then appear twice, once for its body and once for its volume.
// @return at most 32 hits. None off the game thread, with no area loaded, or for a zero
// direction.
std::vector<Hit> Raycast(const Vector &origin, const Vector &direction, std::uint32_t mask = groups::kCursor,
                         bool selectionVolumes = false);

// A point in the world as drawn on the screen, in pixels from the top left of the game's
// view (the window's client area).
struct ScreenPoint
{
    float x = 0;
    float y = 0;
    bool inFront = false; // true if in front of the camera; if not, x and y are meaningless
    bool inView = false;  // true if the point is in front of the camera and inside the view
    // Set by AimAt only: how many of the eight pixels 10 pixels away also hit the object
    // first. 8 means well inside it; a low count means its edge.
    std::int32_t around = 0;
};

// The size of the game's view, in pixels.
struct ViewSize
{
    std::int32_t width = 0;
    std::int32_t height = 0;
};

// Returns the size of the game's view in pixels, which is the window's client area.
// @return nullopt off the game thread or when no world is drawn
std::optional<ViewSize> ScreenSize();

// Returns where a point in the world appears on the screen.
// @return nullopt off the game thread or when no world is drawn
std::optional<ScreenPoint> ToScreen(const Vector &point);

// Returns everything under a pixel, nearest first, as the cursor would find it there. It
// casts the camera's ray through the pixel with groups::kCursor and objects' selection
// volumes, and skips effects hidden from the cursor with dao::vfx::SetPickable.
// @param x pixels from the left of the view
// @param y pixels from the top of the view
// @return no hits for a pixel outside the view, off the game thread, or when no world is
// drawn
//
// The first object is the one a click there would most likely act on. For what the game
// itself has under the cursor, see GameModeExplore (dao/GameModes.h).
std::vector<Hit> PickAt(std::int32_t x, std::int32_t y);

// Returns a pixel to click to target an object: a point inside the object as drawn, where
// the cursor finds it before any other object.
// @return nullopt if the object is not in view, or another object is in front of it at
// every point tried
//
// It tries heights from about a person's middle outward, and returns the first point whose
// eight neighbouring pixels also hit the object. Failing that, it returns the point with the
// most such neighbours (ScreenPoint::around), because the game's cursor can miss at an
// object's edge.
std::optional<ScreenPoint> AimAt(ObjectId object);

// Returns the nearest shape on the segment between two points.
// @param mask the collision groups to hit; groups::kSight by default
// @return nullopt if nothing lies between them
std::optional<Hit> FirstHit(const Vector &from, const Vector &to, std::uint32_t mask = groups::kSight);

// The result of a line-of-sight test: whether the view is clear, and where it is blocked if
// not. Converts to bool, true when the view is clear.
struct Sight
{
    bool clear = false;
    std::optional<Vector> blockedAt; // where the view is blocked, if it is

    explicit operator bool() const
    {
        return clear;
    }
};
// Returns whether one point can be seen from another, using the same test as the game's
// scripts.
// @param ignore an object that should not block the view, or kInvalidObject
// @return not clear off the game thread or with no area loaded
//
// Only the nearest shape in groups::kSight counts. A placeable blocks the view only if it is
// a door (or of base type 8), so a chest in front of a wall leaves the view clear, just as
// the scripts see it.
Sight LineOfSight(const Vector &from, const Vector &to, ObjectId ignore = kInvalidObject);

// The path a creature walks: its waypoints from start to end, and the one it is heading for.
struct WalkPath
{
    std::vector<Vector> waypoints;
    std::size_t next = 0; // index of the waypoint the creature is walking to

    // Returns the path's total length along its waypoints, in metres.
    float Length() const;
    // Returns the distance still to walk, in metres: from `from` to the next waypoint, then
    // along the rest of the path.
    // @param from the creature's current position
    float Remaining(const Vector &from) const;
};
// Returns the path a creature's movement most recently found.
// @return nullopt if the creature does not exist or has no path
//
// A path appears a frame or more after a move is ordered, and not while the game is paused.
// A creature keeps its last path after it arrives, so check its current command
// (CCObject::CurrentCommand) to tell whether it is walking.
std::optional<WalkPath> CurrentPath(ObjectId creature);

// Returns whether a creature is marked on the pathfinding grid right now. A standing creature
// marks the cells under it as an obstacle, which other creatures' paths avoid and which the
// game checks when it fits attackers around a target (the melee rings of dao/Attack.h).
// @return nullopt if there is no such creature
//
// A creature places its mark during its own update, so while the game is paused a creature
// that has just stopped may not be marked yet. Call on the game thread.
std::optional<bool> ObstaclePlaced(ObjectId creature);

// A creature model's bounding box, the one the game uses for drawing.
//
// The box is in model space, relative to the point where the model stands, unscaled and
// unrotated. It comes from the model's rest pose and is made once, so it does not change as
// the creature animates: a person's box has the arms held out, and a winged creature's
// includes the spread wings. The model's widest horizontal extent is the larger of Size().x
// and Size().y.
struct ModelBox
{
    Vector min; // model space: x and y horizontal, z up from where the model stands
    Vector max;
    float scale = 1; // the model's uniform scale
    // The game's own world-space corners: min and max moved by the model's position,
    // rotation and scale. They rotate with the model, so they are not the corners of an
    // axis-aligned box unless the model faces along an axis.
    Vector worldMin;
    Vector worldMax;

    // Returns the box's size in metres as drawn: (max - min) times the scale.
    Vector Size() const
    {
        return {(max.x - min.x) * scale, (max.y - min.y) * scale, (max.z - min.z) * scale};
    }
};
// Returns a creature's model box.
// @return nullopt if the id is not a creature, its model is not of a kind this can read, or
// the call is off the game thread
//
// Call on the game thread.
std::optional<ModelBox> ModelBoxOf(ObjectId creature);

// The result of a path search by Pathfind.
struct PathSearch
{
    std::optional<WalkPath> path; // the path, if one reaches the destination
    // When there is no path, how far the search got: up to the door in the way, or to where
    // it gave up.
    std::optional<WalkPath> partial;
    // When there is no path, the placeable in the way if one was found, usually a closed
    // door; otherwise kInvalidObject. A placeable that is not active is never named, and a shut
    // door is named before anything else in the same place. If the search gave up part way
    // (`result` 1), this is whatever lies across the path just ahead, which may not be what
    // stopped it.
    ObjectId blocker = kInvalidObject;
    // The pathfinder's last result code:
    // - 1: found, or gave up part way if `path` is empty
    // - 5: no path
    // - 6: a door or something like it is in the way, and the walker must act on it to go on
    //   (the game ends such a move with COMMAND_FAILED_PATH_ACTION_REQUIRED)
    // - 8: no walkmesh there
    // - 0: never asked (no such creature, or called off the game thread)
    // - any other value: the search ran out of time
    std::int32_t result = 0;
    // How many time slices the search took; 0 for a straight walk. The pathfinder works in
    // short slices of time.
    std::int32_t slices = 0;
    // How many searches ran: one, or more when a long walk across open ground gave up part
    // way and the search went on from where it stopped.
    std::int32_t legs = 0;
    // Each slice's result code, in order, to help tell why a search failed.
    std::vector<std::int32_t> answers;
    std::size_t waypoints = 0; // how many waypoints the game's path held at the end
    // Whether the creature fit at the start. An end where it did not fit is moved to a free
    // place nearby.
    bool startFree = false;
    bool goalFree = false; // whether the creature fit at the destination
    Vector start{};        // the start the search used
    Vector goal{};         // the destination the search used
    std::string trace;     // a one-line summary for a log
};

// Finds the path a creature would walk to a point, using the game's pathfinder, without
// affecting the creature: its current path, movement and orders are left alone.
// @param creature the creature that would walk
// @param to the destination. If the creature does not fit there, the nearest free place up
// to 2 m back toward the creature is used, as for a click.
// @param toward a creature standing at the destination, which is not treated as an obstacle,
// so that a path to an enemy reaches it; or kInvalidObject
// @return the search, including the path if one reaches the destination
//
// Where a straight walk works, the path is just its two ends. Otherwise the search runs for
// at most 40 ms. When a long walk across open ground gives up part way, the search goes on
// from where it stopped, up to six times. Call on the game thread.
PathSearch Pathfind(ObjectId creature, const Vector &to, ObjectId toward = kInvalidObject);
// Finds the path a creature would walk to a point, like Pathfind, and returns only the path.
// @return nullopt if no path reaches the destination
//
//   if (const auto path = dao::world::FindPath(creature, enemyPosition, enemy))
//       spdlog::info("{:.1f} m to walk", path->Length());
std::optional<WalkPath> FindPath(ObjectId creature, const Vector &to, ObjectId toward = kInvalidObject);

// A room of the current area. Areas are divided into rooms, and a room the party has never
// seen is drawn black, along with everything in it.
//
// Whether a room has been seen depends on the party, not the camera: the rooms the party
// stands in, and some of their neighbours, are in view, and a room stays seen once it has
// been in view. Whether a room is loaded depends on the camera. An outdoor area's rooms are
// cells of its terrain and never read as loaded, so use Shown() to decide whether a room is
// drawn.
//
//   if (const auto room = dao::world::RoomOf(enemy); room && !room->Shown())
//       ; // don't send the camera there: the player would see only black
struct Room
{
    static constexpr std::int32_t kDropped = 3;   // not wanted, so unloaded
    static constexpr std::int32_t kWanted = 4;    // kept, and loaded in the background if not yet
    static constexpr std::int32_t kWantedNow = 5; // loaded at once, ahead of the others

    static constexpr std::int32_t kLoading = 0;
    static constexpr std::int32_t kLoaded = 1;
    static constexpr std::int32_t kNotLoaded = 2;

    static constexpr std::int32_t kNeverSeen = 0; // hidden, so drawn black
    static constexpr std::int32_t kInView = 1;    // the party is in it or in a room that sees it
    static constexpr std::int32_t kSeenBefore = 2;

    std::int32_t number = 0; // the room's number, as in its file name, <area>_<number>.rim
    // How much the game last wanted the room: kDropped, kWanted or kWantedNow indoors; 6 for
    // a terrain cell outdoors.
    std::int32_t state = 0;
    std::int32_t load = kNotLoaded; // kLoading, kLoaded or kNotLoaded
    bool loading = false;           // true while a background load is under way
    std::int32_t seen = kNeverSeen; // kNeverSeen, kInView or kSeenBefore
    // How far the point asked about lies outside the room's bounds, in metres; 0 inside
    // them. Set by RoomAt and RoomOf, otherwise 0.
    float distance = 0;

    // Returns true if the room is loaded. Only meaningful indoors: an outdoor area's rooms
    // never read as loaded, even the one under the party's feet.
    bool Loaded() const
    {
        return load == kLoaded;
    }
    // Returns true if the party sees the room now or has seen it before, so the game draws it
    // and everything in it instead of black.
    bool Shown() const
    {
        return seen != kNeverSeen;
    }
};
// Returns every room of the current area.
// @return none before an area is loaded, or off the game thread
std::vector<Room> Rooms();
// Returns the room that contains a point, using the area's own lookup. For a point in no
// room, returns the nearest room by its bounds, with `distance` set to how far away it is.
// @return nullopt if the area has no rooms, or off the game thread
std::optional<Room> RoomAt(const Vector &point);
// Returns the room an object is in: the room the object records for itself, which is the one
// the game goes by.
// @return nullopt if there is no such object, or its area has no rooms
std::optional<Room> RoomOf(ObjectId object);

// Returns whether `target` is in line of sight of `viewer`, using the same test as the
// scripts' CheckLineOfSightObject: from kEyeHeight above one's position to kEyeHeight above
// the other's.
// @return not clear if either object is missing, they are in different areas, or the call is
// off the game thread
Sight LineOfSight(ObjectId viewer, ObjectId target);

// Returns how far from the camera the game shows a creature, in metres, as it stands now. The
// game's own goes by the graphics detail the player chose: 30, 35, 50 or 60.
// @return nullopt in a build of the game the library does not know
//
// A creature farther from the camera than this is faded out and taken out of the scene, and its
// animations stop with it. An attack's impact is an event of its animation, so such a creature's
// commands still run and end, but its blows never land.
std::optional<float> CreatureShowDistance();

// Holds the distance from the camera within which the game shows a creature at `metres`, or with
// nullopt gives it back to the game.
// @param metres more than 0, or nullopt for the game's own
// @return false, with nothing changed, for a distance that is not more than 0, or in a build of
//  the game the library does not know
//
// The game sets the distance itself whenever it applies the graphics detail, so the library puts
// the plugin's back each frame for as long as it is held. It is one distance for every creature
// of the game, not one creature's, and it is not saved. Farther creatures drawn and animated cost
// frame time. The area's rooms are still loaded only around the camera, to some 120 m.
bool SetCreatureShowDistance(std::optional<float> metres);

} // namespace dao::world
