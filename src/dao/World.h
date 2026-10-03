// Questions about the world's geometry, as the engine asks them: what a ray
// meets, and whether one point can be seen from another.
//
//   for (const dao::world::Hit &hit : dao::world::Raycast(eye, forward))
//       ...
//   if (dao::world::LineOfSight(archer, target))
//       ...
//
// Read 2026-09-30. An area keeps its physics (Physics::PhysicsScene, over
// PhysX's NxScene) at the +0xCC of the object at its +0x62C (0x00537D60
// reads it). The engine asks that scene two questions:
// - every shape along a ray, to any distance: World::Raycast (0x006497A0,
//   the scene's slot 8, NxScene::raycastAllShapes), which the cursor pick
//   asks 15 times a second (dao/GameModes.h);
// - the nearest shape between two points: the area's LineOfSight
//   (0x0067C3F0, the scene's slot 4, raycastClosestShape over the segment),
//   which the scripts' CheckLineOfSightObject and CheckLineOfSightVector
//   ask, and which lets doors alone among placeables block the view.
// Each takes a mask of collision groups, and a shape is met when its own
// groups (PhysX's NxGroupsMask, first word) share a bit with the mask.
//
// On the game's thread only, where frame callbacks, events and the hooks
// on script commands run: the raycast gathers its hits in a static vector
// (0x00C347B0) that the cursor pick shares. Called on another thread once
// a frame has run (dao::FrameThreadId), each answers nothing: no hits, no
// sight.
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

// Collision groups: a shape's, and a query's mask. Read 2026-09-30 from
// the code that tests them; which bit is which kind of object or ground is
// not yet told apart beyond what is named.
namespace groups
{
// A hit with any of these and an object id is an object; with none of
// them and any of kGround, the ground (0x00648B70).
inline constexpr std::uint32_t kObjects = 0x1 | 0x2 | 0x4 | 0x20000;
inline constexpr std::uint32_t kGround = 0x8 | 0x20;
// Placeables: LineOfSight looks a hit with this up as one.
inline constexpr std::uint32_t kPlaceables = 0x4;
// A creature's body: in play, 2026-09-30, a ray down onto the controlled
// creature met it first with only this group and the creature's id, which
// the engine's own sorting (above) leaves as Nothing.
inline constexpr std::uint32_t kCreatureBodies = 0x10;
// The generous volume an object is picked by, beside its body; Raycast's
// `selectionVolumes` adds it to the mask.
inline constexpr std::uint32_t kSelection = 0x20000;
// Set on ground that cannot be walked: explore mode refuses a move there.
inline constexpr std::uint32_t kNotWalkable = 0x01000000;

// The engine's own masks.
inline constexpr std::uint32_t kCursor = 0x2607; // the cursor pick, 0x0070B590
inline constexpr std::uint32_t kSight = 0x24;    // line of sight: placeables and 0x20
} // namespace groups

// One shape a ray met, as the engine keeps it: 0x30 bytes, read 2026-09-30
// from its constructor (0x00648B10) and World::Raycast, which fills each
// from PhysX's hit (the physics' own, 0x00829900). Explore mode keeps the
// cursor's 32 of these (dao/GameModes.h).
struct Hit
{
    enum class Kind : std::int32_t
    {
        Nothing = 0, // met a shape with neither kind of group
        Ground = 1,  // the area's ground: walkmesh, terrain
        Object = 2,  // a creature, placeable or other object; `object` is its id
    };

    Vector position;
    std::uint32_t padding0 = 0;
    Kind kind = Kind::Nothing;
    ObjectId object = kInvalidObject;
    // groups::kNotWalkable clear: explore mode's walkable test requires it
    // of a ground hit.
    bool walkable = false;
    std::uint32_t unknown1c = 0;
    float unknown20 = 0; // from the shape's owner (its slot 18); not read yet
    std::uint32_t groups = 0;
    std::uint32_t unknown28 = 0;
    std::uint32_t padding2c = 0;

    bool IsGround() const
    {
        return kind == Kind::Ground;
    }
    // The object the shape belongs to, if any: an Object hit's, and a
    // creature's body, which the engine sorts as Nothing but gives the id.
    std::optional<ObjectId> Object() const
    {
        return object != kInvalidObject ? std::optional<ObjectId>(object) : std::nullopt;
    }
};
static_assert(sizeof(Hit) == 0x30);

// How far above a creature's position the scripts' line of sight looks
// from and to (CheckLineOfSightObject, 0x00668DB0).
inline constexpr float kEyeHeight = 1.8F;

// The area the player is in: the module's current area (CCModule's
// 0x005FE470), or the controlled creature's. Null before a game is loaded.
CCArea *CurrentArea();

// Every shape in the current area along a ray from `origin` in
// `direction`, to any distance, nearest first; at most 32, as the engine
// gathers them. `mask` chooses the groups met. With `selectionVolumes`,
// objects' pick volumes are met too, as the cursor meets them, and an
// object may then appear twice (its body and its volume); without, once.
std::vector<Hit> Raycast(const Vector &origin, const Vector &direction, std::uint32_t mask = groups::kCursor,
                         bool selectionVolumes = false);

// The world on the screen: where a point of the world is drawn, and what a
// point of the screen is over. Read 2026-10-01 from the cursor's own pick
// (dao/Addresses.h): the scene's camera turns a pixel into a ray and a
// point into a pixel, and these ask it. Pixels are of the game's view, from
// its top left: the window's client area. On the game's thread, with an
// area loaded. They answer nothing unless the scene's camera is the
// perspective camera they were read from. (The first try hung the game at
// ScreenSize, which asked the wrong object: dao/Addresses.h.)
//
// Seen in play, 2026-10-01, through the Eluvian server, windowed 1600 by
// 1000, the close view:
// - ScreenSize answered 1600 by 1000, the window's client size.
// - ToScreen of the controlled creature's position: (800, 926), her feet,
//   in front and in view; of a point behind the camera, not in front.
// - PickAt 60 pixels above her feet: her, then the ground. At a cave
//   wall: nothing, the cursor's mask leaving walls out. Outside the view:
//   nothing. A creature is in a pick twice running, its body and its pick
//   volume.
// - AimAt: 243 pixels above her feet; and for a wolf standing behind
//   another creature as the camera saw them, a pixel on the wolf's visible
//   part, where the pick met the wolf first.
// - ToScreen's points drawn by a movie (the turn-based mod's path line):
//   on the creature's feet and the cursor's ground point at four places,
//   one after the camera came round. dev/GUI.md has how a movie's stage
//   lies over the view.
// - Pathfind with the game paused, through the Eluvian server: the same
//   two ends asked running, paused twice and running again answered the
//   same each time to the last waypoint and slice (44.18 m, 29 waypoints,
//   20 slices). A pause changes nothing.
// - FindPath against the walk, the turn-based mod's preview: ten walks
//   ordered from a path shown, 1 to 8.8 m, round a creature and up a ramp
//   among them, stayed within 0.34 m of the path FindPath gave, and fifty
//   searches for a creature at rest took 15 ms in all.
// - The real cursor put at an AimAt pixel, by the play tester: the game's
//   own CursorObject was the creature and the click attacked it, at
//   pixels inside it; at one on its edge the game's pick held nothing
//   (AimAt, below, now keeps off edges).
struct ScreenPoint
{
    float x = 0;
    float y = 0;
    bool inFront = false; // of the camera; behind it, x and y mean nothing
    bool inView = false;  // in front, and within the view's pixels
    // From AimAt only: how many of the eight pixels ten away meet the
    // object first too. 8 is well inside it; a low count is its edge.
    std::int32_t around = 0;
};

struct ViewSize
{
    std::int32_t width = 0;
    std::int32_t height = 0;
};

// The view's size in pixels.
std::optional<ViewSize> ScreenSize();

// Where a point of the world is on the screen.
std::optional<ScreenPoint> ToScreen(const Vector &point);

// Everything under a pixel, nearest first, as the cursor's pick gathers it
// when it is there: the ray the camera gives for the pixel, cast with the
// cursor's mask and the objects' pick volumes. The first object in it is
// what a click there would most likely act on; the game's own choice for
// where the cursor is now is GameModeExplore's (dao/GameModes.h).
std::vector<Hit> PickAt(std::int32_t x, std::int32_t y);

// A pixel to click an object at: its place on the screen at a height where
// the pick there meets it before any other object, tried at a few heights
// from its middle outward, since an object is picked by its body and not
// its feet. Of those, the first with the pick meeting it at all eight
// pixels ten away as well, or failing one the height with most of them:
// a pixel inside the object, not on its edge. Nothing if it is not in view
// at any height, or another object is in front at every one.
//
// Why inside: in play, 2026-10-01, the real cursor put at a pixel on the
// top edge of a spider as drawn was over nothing by the game's own pick
// (no object, no ground, no hits) and the click became a move, while
// PickAt at that pixel met the spider; 37 pixels lower the two agreed and
// the click attacked. The mode's pick is taken 15 times a second from
// where the engine has the cursor, and holds no hits when the engine gives
// it no place for the cursor (dao/GameModes.h); at an edge the two can
// differ, so an edge is no place to aim.
std::optional<ScreenPoint> AimAt(ObjectId object);

// The nearest shape between two points, if any: Raycast, cut at `to`.
std::optional<Hit> FirstHit(const Vector &from, const Vector &to, std::uint32_t mask = groups::kSight);

// Whether `to` can be seen from `from`, as the engine judges it: the
// nearest shape between them among groups::kSight blocks, except a
// placeable that is not a door or of base type 8 (0x0067A670 reads
// GetPlaceableBaseType), and `ignore`. Only the nearest is judged, so a
// chest in front of a wall leaves the view clear, as the scripts see it.
// Converts to bool, true when the view is clear.
struct Sight
{
    bool clear = false;
    std::optional<Vector> blockedAt; // where the view was cut, when it was

    explicit operator bool() const
    {
        return clear;
    }
};
Sight LineOfSight(const Vector &from, const Vector &to, ObjectId ignore = kInvalidObject);

// The path a creature is walking, as its movement found it: the waypoints
// from where the search started to where it ends, and the one it is
// heading for. Read 2026-09-30: a creature's MovementController (its
// +0xA40, 0x006A2570; vftable 0x00B596C0) keeps its search at +0x14, whose
// waypoints are a std::vector of 12-byte points at +0x70 (0x00A47620 reads
// one, bounds-checked by (last - first) / 12) and whose next waypoint's
// index is at +0x88 (0x00A625E0). The move commands' update (0x007A9130)
// asks for the path a slice of time a frame (0x0073EA80, the search
// advanced by 0x00A62640), so a path appears a frame or more after the
// order, not while the game is paused. The search outlives the walk: a
// creature that has arrived, or stood since a load, still answers its last
// path (seen in play, 2026-09-30), so whether it is walking it is its
// current command's to say (a move: CCObject::CurrentCommand). Seen in play.
struct WalkPath
{
    std::vector<Vector> waypoints;
    std::size_t next = 0; // index of the waypoint being walked to

    // Along the waypoints, start to end.
    float Length() const;
    // From `from` (where the creature is) to the next waypoint, then on.
    float Remaining(const Vector &from) const;
};
// Nothing for a creature not walking a path, or not there.
std::optional<WalkPath> CurrentPath(ObjectId creature);

// The path a creature would walk to a point, found now by the game's
// pathfinder, the creature left as it is: its own path, its walking and
// its orders are not touched.
//
//   if (const auto path = dao::world::FindPath(creature, enemyPosition, enemy))
//       spdlog::info("{:.1f} m to walk", path->Length());
//
// Read 2026-09-30 (dao/Addresses.h, "A search apart from any creature's"):
// a search is a plain record the engine also makes on the stack of a
// script command (IsLocationSafe), so this makes one of its own, with the
// creature's size, and follows RequestPath's order with it. If the
// creature does not fit at the point, the nearest place within 2 m back
// toward the creature is the destination, as for a click. The creature's
// own obstacle is taken out of the pathfinding world for the search and
// put back after, as the game does each time it asks for a path; `toward`
// names a creature standing at the point, whose obstacle is lifted the same
// way (the game does that for the creature a mover follows), so a path to
// an enemy ends at it rather than failing. A straight walk, where one will
// do, is the two ends; else the search is run to its end, in slices, for at
// most 40 ms. On the game's thread.
//
// The engine's search is made for a walker: it works out only the stretch
// ahead of the waypoint its walker heads for, and the next as the walker
// nears the end of that, so this moves its own search's walker along to
// have the whole path. And it answers "found" for a path it gave up on
// part way, so a path counts here only if it ends at the destination.
//
// Seen in play, 2026-10-01. The first version answered a straight walk
// rightly but, once the party had walked and so stood as obstacles, every
// search came back "found" with the start alone. This version, in the
// Elven Ruins and the forest clearing, the controlled creature walking as
// before throughout:
// - to a companion 0.9 to 3.8 m off, before and after walking: the two
//   ends, no search;
// - to a spider's corpse 3.2 m off round a corner: 4 waypoints, 3.42 m,
//   in 6 slices; to another, 7.14 m straight: 6 waypoints, 7.30 m;
// - outdoors: 35.80 m straight, 26 waypoints and 46.56 m in 21 slices;
//   56.10 m straight, 64 waypoints and 107.62 m in 44 slices;
// - to skeletons 105 to 117 m off in a room never seen: none, the search
//   giving up part way after 17 to 25 slices, 8 to 10 ms each time;
// - to skeletons 67 to 70 m off behind shut doors: none, the search
//   answering 6 at the door. That version went on past the 6 until its 30
//   ms ran out, 130 to 170 slices; this one stops there.
// - and, through the Eluvian server from the ruins' entrance: 40.7 m
//   straight, 29 waypoints and 44.18 m; to creatures 80 to 143 m off past a
//   door, none, the search giving up 53.38 m along, short of the door, with
//   no 6 and so no blocker. Searching on from where it gave up is for that
//   case. In play it changed nothing there: the second search gave up
//   from the same cell. But with the creature standing at that end, 1.2 m
//   from the shut door, the search answered 6 at once ([4, 4, 6]) and, once
//   the engine's own note of the barred cell was read, named the door.
//   Short searches from points further on, where no creature stood, never
//   answered 6 either, and took the call to 47 to 66 ms. So the searching
//   on is gone again: one search, and where it gives up part way, the
//   placeable whose footprint holds a point just ahead is named instead.
//   Seen in play the same day: from the entrance to each of three
//   creatures past the door, the answer 1, 53.38 m of the way, and the
//   blocker the door, in 25 slices; and the walk to the spider in the same
//   room found as before.
// Searched on again, 2026-10-03 (DAOLib #5): a walk of 244 m in the Korcari
// Wilds found nothing, where 90 m to a point on the way and then the rest
// were found. Read: the engine plans a stretch of fine path between each two
// points of a coarse route, and two stretches in a row that fall short of
// their point end the search as found (0x00A4D1F0). A walker's own search
// does the same, a slice of 3 to 6 ms a frame (RequestPath, 0x0073EA80); how
// a click's move reaches the end of so long a walk is not read. So where a
// search gives up part way and no placeable lies ahead, it is searched on
// from where it stopped, up to six searches in all, each getting at least 2
// m nearer, within 40 ms. A door ahead still ends it. Seen in play
// 2026-10-03 through the Eluvian server: from the Tower of Ishal's courtyard
// to the Quartermaster, 229.5 m straight, a path of 276.4 m in one call,
// which a single move then walked to the end. How many searches it took was
// not shown.
// Read after, 2026-10-01: RequestPath tells the pathfinding world its
// mover's size and which stamps stop it before it asks, and the search
// here did not, so its straight-walk check went by the last creature to
// have asked. It tells it now; not yet seen in play.
//
// An earlier FindPath drove the creature's own search (RequestPath until it
// answered) and was taken out after play, 2026-09-30: RequestPath takes new
// ends only when no search is in progress, so its answers came a call late,
// and the search it left stood as the creature's path, after which the
// creature would not walk a click.
struct PathSearch
{
    std::optional<WalkPath> path; // nothing if none was found that reaches the destination
    // With no path, as far as the search got: to the door in the way, or
    // to where it gave up.
    std::optional<WalkPath> partial;
    // With no path, the placeable in the way, if one is found; a shut door
    // in play, kInvalidObject if none. With the answer 6 it is found as the
    // game finds it for a creature's own path: within 5 m of the barred
    // cell the search noted, its footprint holding that point. Where the
    // search gave up part way (the answer 1), by the same test on points up
    // to 2.5 m ahead of where it stopped, the way the path was heading: what
    // lies across the way, not known to be what stopped it.
    ObjectId blocker = kInvalidObject;
    // The pathfinder's last answer: 1 found (or given up part way, when
    // `path` is nothing), 5 none, 6 a door or the like in the way, which a
    // walker must act on to go on (the game ends a move there with
    // COMMAND_FAILED_PATH_ACTION_REQUIRED), 8 no mesh there, 0 not asked (no
    // such creature, or off the game's thread), others a search the time
    // ran out on.
    std::int32_t result = 0;
    std::int32_t slices = 0; // slices of search it took; 0 for a straight walk
    // Searches run: one, or more where a long walk gave up part way in the
    // open and was searched on from where it stopped (FindPath, above).
    std::int32_t legs = 0;
    // For telling why a search failed: each slice's answer in order, the
    // waypoints the engine's path held at the end, whether the creature fit
    // at each end (an end it did not fit at was moved to a free place
    // nearby), the ends the search used, and a line for a log.
    std::vector<std::int32_t> answers;
    std::size_t waypoints = 0;
    bool startFree = false;
    bool goalFree = false;
    Vector start{};
    Vector goal{};
    std::string trace;
};
PathSearch Pathfind(ObjectId creature, const Vector &to, ObjectId toward = kInvalidObject);
// The path alone.
std::optional<WalkPath> FindPath(ObjectId creature, const Vector &to, ObjectId toward = kInvalidObject);

// The rooms of the current area, and whether the game shows each: an area
// is laid out in rooms, and a room the party has never seen is drawn black
// with whatever stands in it.
//
//   if (const auto room = dao::world::RoomOf(enemy); room && !room->Shown())
//       ; // do not send the camera there: the player would see black
//
// Two things decide a room, read 2026-09-30 and 2026-10-01 (dao/Addresses.h,
// "Rooms and their streaming").
//
// Whether it has been seen follows the party, not the camera. When the
// rooms the party's members stand in change, those rooms and some of their
// neighbours are in view (1); a room in view before stays seen (2), which a
// save keeps; and a room never in view (0) is hidden. In play, 2026-10-01:
// beyond a shut door all was black in the tactical view, the room behind
// stayed black with the door open, its four soldiers inactive, and drew
// when the hero walked in, when they woke and the fight began.
//
// Whether it is loaded follows the camera.
// Read 2026-09-30 (dao/Addresses.h, "Rooms and their streaming"). Each
// frame the area's layout takes the scene's camera's position, not the
// party's, and asks its policy how much each room is wanted: now (5)
// within one distance of the camera, in the background (4) within a
// second, or wherever the scene's own check names the room, and not at all
// (3) beyond a third, which unloads it. So what is loaded follows the
// camera: a camera sent toward a dropped room has it loaded in the
// background, some frames later, the game pausing itself to load only when
// the camera jumps more than 30 m in a frame.
//
// Seen in play, 2026-10-01, in Castle Cousland: 61 rooms, 53 wanted, 1
// wanted now and 7 dropped, the controlled creature's own room wanted and
// loaded, and none dropped once the party stood in the hall. A first
// reading took "wanted now" for "drawn"; it is not.
//
// And outdoors, the same day, through the Eluvian server in the Dalish
// forest clearing: the party, two wolves 25 m off and three corpses were
// each in a room, the rooms' numbers 0x30002 to 0x40003 (cells of the
// terrain, by the look of them, not a file's number), every one in view
// (1), and none with its load reading loaded, though all were on screen:
// each read load 2 and state 6, a state the indoor policy never gives, in
// explore mode and in a fight alike. So Shown() goes by what has been seen
// alone, and state and load are an indoor room's.
//
// And indoors, the same day, in the Elven Ruins (through the Eluvian
// server, the turn-based mod going by Shown()): rooms 1574 to 1580, the
// party's own in view and every other never seen, not shown, though
// loaded. The first hall and the entrance tunnel 40 m from it are one
// room. Walking into the next hall put it in view and the room beyond it
// too, unentered. A room behind a shut door stayed never seen, three
// spiders idle in it 13 to 24 m off, and read in view the moment the door
// opened, before anyone walked in; in Castle Cousland a room behind an
// opened door had stayed black until the hero entered, so which neighbours
// a room shows is the area's own. The turn-based mod went by Shown() the
// same night: four spiders in a room never seen, loaded, were left out of
// a fight next door; the door opened, the room read in view at once, and
// they joined, the camera on their turns showing the room drawn.
struct Room
{
    static constexpr std::int32_t kDropped = 3;   // not wanted: unloaded
    static constexpr std::int32_t kWanted = 4;    // kept, loaded in the background if it is not
    static constexpr std::int32_t kWantedNow = 5; // loaded at once, ahead of the others

    static constexpr std::int32_t kLoading = 0;
    static constexpr std::int32_t kLoaded = 1;
    static constexpr std::int32_t kNotLoaded = 2;

    static constexpr std::int32_t kNeverSeen = 0; // hidden: drawn black
    static constexpr std::int32_t kInView = 1;    // the party is in it, or in a room that sees it
    static constexpr std::int32_t kSeenBefore = 2;

    std::int32_t number = 0; // the room's, as in its file's name, <area>_<number>.rim
    // How much the game last wanted it: kDropped, kWanted, kWantedNow
    // indoors; 6 for a cell of an outdoor area's terrain.
    std::int32_t state = 0;
    std::int32_t load = kNotLoaded;
    bool loading = false; // a load in the background is under way
    std::int32_t seen = kNeverSeen;
    // How far a point is from the room's bounds, in metres; 0 inside them.
    // Filled by RoomAt and RoomOf for their point, else 0.
    float distance = 0;

    // In memory and in the world, for a room of an indoor area. An outdoor
    // area's rooms are cells of its terrain, whose load does not read
    // kLoaded even under the party's feet (below).
    bool Loaded() const
    {
        return load == kLoaded;
    }
    // Seen by the party now or before: the game draws it and what stands
    // in it, rather than black.
    bool Shown() const
    {
        return seen != kNeverSeen;
    }
};
// Every room of the current area; none before an area is there. On the
// game's thread.
std::vector<Room> Rooms();
// The room a point is in, as the area answers it; for a point in none, the
// nearest by its bounds, `distance` saying how far.
std::optional<Room> RoomAt(const Vector &point);
// The room an object is in: the one the object keeps for itself, which is
// what the game goes by (the party's rooms are its members'). Nothing for
// an object that is not there, or in an area with no rooms.
std::optional<Room> RoomOf(ObjectId object);

// Whether `target` can be seen from `viewer`, as CheckLineOfSightObject
// asks it: from kEyeHeight above one's position to kEyeHeight above the
// other's, in the viewer's area; not if they are in different areas.
Sight LineOfSight(ObjectId viewer, ObjectId target);

} // namespace dao::world
