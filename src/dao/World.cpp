// The world's geometry as the engine asks about it (dao/World.h).
//
// Read 2026-09-30. An area keeps its physics (Physics::PhysicsScene, over PhysX's
// NxScene) at the +0xCC of the object at its +0x62C (0x00537D60 reads it). The engine
// asks that scene two questions:
// - every shape along a ray, to any distance: World::Raycast (0x006497A0, the scene's
//   slot 8, NxScene::raycastAllShapes), which the cursor pick asks 15 times a second
//   (dao/GameModes.h);
// - the nearest shape between two points: the area's LineOfSight (0x0067C3F0, the
//   scene's slot 4, raycastClosestShape over the segment), which the scripts'
//   CheckLineOfSightObject and CheckLineOfSightVector ask, and which lets doors alone
//   among placeables block the view.
// Each takes a mask of collision groups, and a shape is met when its own groups
// (PhysX's NxGroupsMask, first word) share a bit with the mask.
//
// On the game's thread only, where frame callbacks, events and the hooks on script
// commands run: the raycast gathers its hits in a static vector (0x00C347B0) that the
// cursor pick shares. Called on another thread once a frame has run
// (dao::FrameThreadId), each answers nothing: no hits, no sight.

#include "dao/World.h"

#include "dao/RenderCamera.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/Vfx.h"
#include "dao/core/Msvc8.h"
#include "dao/core/Path.h"
#include "dao/generated/ScriptApi.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <mutex>
#include <optional>
#include <utility>

namespace dao::world
{

namespace
{

constexpr std::uint32_t kMaxHits = 32;    // what the engine's own callers ask for
constexpr std::size_t kRaycaster = 0x10C; // in the application: Raycast's `this`

// The engine's vectors are 16 bytes and aligned for SSE.
struct alignas(16) Vector4
{
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 0;

    Vector4() = default;
    explicit Vector4(const Vector &v) : x(v.x), y(v.y), z(v.z)
    {
    }
    Vector ToVector() const
    {
        return {x, y, z};
    }
};

// Off the game's thread, once it is known, a query would race the cursor
// pick for the raycast's static results.
bool OnGameThread()
{
    const std::uint32_t frame = FrameThreadId();
    return frame == 0 || frame == GetCurrentThreadId();
}

float DistanceSquared(const Vector &a, const Vector &b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return (dx * dx) + (dy * dy) + (dz * dz);
}

Sight Ask(CCArea *area, const Vector &from, const Vector &to, ObjectId ignore)
{
    if (area == nullptr || !OnGameThread())
    {
        return {};
    }
    using Fn = bool(__fastcall *)(CCArea *, int, const Vector4 *, const Vector4 *, Vector4 *, ObjectId, std::uint32_t);
    const Vector4 a(from);
    const Vector4 b(to);
    Vector4 blocked;
    const bool clear = reinterpret_cast<Fn>(Module::Address(addr::kCCArea_LineOfSight))(area, 0, &a, &b, &blocked,
                                                                                        ignore, groups::kSight);
    Sight sight;
    sight.clear = clear;
    if (!clear)
    {
        sight.blockedAt = blocked.ToVector();
    }
    return sight;
}

} // namespace

// The module's current area (CCModule's 0x005FE470), which is the controlled
// creature's.
CCArea *CurrentArea()
{
    CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return nullptr;
    }
    CCModule *module = app->GetModule();
    return module != nullptr ? module->CurrentArea() : nullptr;
}

std::vector<Hit> Raycast(const Vector &origin, const Vector &direction, std::uint32_t mask, bool selectionVolumes)
{
    CClientExoApp *app = CClientExoApp::Get();
    const float length = std::sqrt(DistanceSquared(direction, {}));
    if (app == nullptr || length < 1e-6F || !OnGameThread() || CurrentArea() == nullptr)
    {
        return {};
    }
    const auto raycaster = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(app) + kRaycaster);
    const Vector4 from(origin);
    const Vector4 unit(Vector{direction.x / length, direction.y / length, direction.z / length});
    alignas(16) std::array<Hit, kMaxHits> hits{};

    using Fn = std::uint32_t(__fastcall *)(void *, int, Hit *, std::uint32_t, const Vector4 *, const Vector4 *,
                                           std::uint32_t, bool, void *);
    const std::uint32_t count = reinterpret_cast<Fn>(Module::Address(addr::kWorld_Raycast))(
        reinterpret_cast<void *>(raycaster.value_or(0)), 0, hits.data(), kMaxHits, &from, &unit, mask, selectionVolumes,
        nullptr);

    std::vector<Hit> out(hits.begin(), hits.begin() + static_cast<std::ptrdiff_t>(std::min(count, kMaxHits)));
    std::ranges::sort(out, [&](const Hit &a, const Hit &b) {
        return DistanceSquared(a.position, origin) < DistanceSquared(b.position, origin);
    });
    return out;
}

namespace
{

// Renderer::PerspectiveCamera's slots (dao/Addresses.h).
constexpr std::size_t kSceneSlot = 9;      // in the graphics object: the scene being drawn
constexpr std::size_t kSceneCamera = 0xC0; // in the scene: its GraphicsStructures::SceneCamera
constexpr std::size_t kRenderCamera = 0x4; // in the scene's camera: the renderer's camera it wraps
constexpr std::size_t kCameraView = 0x4;   // in the renderer's camera: what answers the view's size
constexpr std::size_t kViewWidthSlot = 5;
constexpr std::size_t kViewHeightSlot = 6;
constexpr std::size_t kPixelToRaySlot = 17;
constexpr std::size_t kPointToPixelSlot = 19;

// The camera the scene is drawn through, null with none.
void *RenderCamera()
{
    if (!OnGameThread())
    {
        return nullptr;
    }
    const auto graphics = TryRead<std::uintptr_t>(Module::Address(addr::kGraphics));
    if (!graphics || *graphics == 0)
    {
        return nullptr;
    }
    using SceneFn = void *(__fastcall *)(void *, int);
    auto *object = reinterpret_cast<void *>(*graphics);
    void *scene = (*reinterpret_cast<SceneFn **>(object))[kSceneSlot](object, 0);
    if (scene == nullptr)
    {
        return nullptr;
    }
    // The scene's camera wraps the renderer's (it passes these slots on to
    // it), and the view's size is the renderer's camera's to ask. Only a
    // perspective camera is one these know: the first try took the wrapper
    // for it, asked the wrong object for the view's size and hung the game
    // (the Eluvian server, 2026-10-01).
    const auto wrapper = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(scene) + kSceneCamera);
    const auto camera = wrapper && *wrapper != 0 ? TryRead<std::uintptr_t>(*wrapper + kRenderCamera) : std::nullopt;
    if (!camera || *camera == 0 ||
        TryRead<std::uintptr_t>(*camera).value_or(0) != Module::Address(addr::kPerspectiveCamera_Vftable))
    {
        return nullptr;
    }
    return reinterpret_cast<void *>(*camera);
}

std::optional<ViewSize> SizeOf(void *camera)
{
    const auto view = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(camera) + kCameraView);
    if (!view || *view == 0)
    {
        return std::nullopt;
    }
    using SizeFn = std::int32_t(__fastcall *)(void *, int);
    auto *object = reinterpret_cast<void *>(*view);
    SizeFn *slots = *reinterpret_cast<SizeFn **>(object);
    const ViewSize size{slots[kViewWidthSlot](object, 0), slots[kViewHeightSlot](object, 0)};
    return size.width > 0 && size.height > 0 ? std::optional(size) : std::nullopt;
}

} // namespace

// The world on the screen: where a point of the world is drawn, and what a point of the
// screen is over. Read 2026-10-01 from the cursor's own pick (dao/Addresses.h): the
// scene's camera turns a pixel into a ray and a point into a pixel, and these ask it.
// Pixels are of the game's view, from its top left: the window's client area. On the
// game's thread, with an area loaded. They answer nothing unless the scene's camera is
// the perspective camera they were read from. (The first try hung the game at
// ScreenSize, which asked the wrong object: dao/Addresses.h.)
//
// Seen in play, 2026-10-01, through the Eluvian server, windowed 1600 by 1000, the
// close view:
// - ScreenSize answered 1600 by 1000, the window's client size.
// - ToScreen of the controlled creature's position: (800, 926), her feet, in front and
//   in view; of a point behind the camera, not in front.
// - PickAt 60 pixels above her feet: her, then the ground. At a cave wall: nothing,
//   the cursor's mask leaving walls out. Outside the view: nothing. A creature is in a
//   pick twice running, its body and its pick volume.
// - AimAt: 243 pixels above her feet; and for a wolf standing behind another creature
//   as the camera saw them, a pixel on the wolf's visible part, where the pick met the
//   wolf first.
// - ToScreen's points drawn by a movie (the turn-based mod's path line): on the
//   creature's feet and the cursor's ground point at four places, one after the camera
//   came round. dev/GUI.md has how a movie's stage lies over the view.
// - Pathfind with the game paused, through the Eluvian server: the same two ends asked
//   running, paused twice and running again answered the same each time to the last
//   waypoint and slice (44.18 m, 29 waypoints, 20 slices). A pause changes nothing.
// - FindPath against the walk, the turn-based mod's preview: ten walks ordered from a
//   path shown, 1 to 8.8 m, round a creature and up a ramp among them, stayed within
//   0.34 m of the path FindPath gave, and fifty searches for a creature at rest took
//   15 ms in all.
// - The real cursor put at an AimAt pixel, by the play tester: the game's own
//   CursorObject was the creature and the click attacked it, at pixels inside it; at
//   one on its edge the game's pick held nothing (AimAt now keeps off edges).
std::optional<ViewSize> ScreenSize()
{
    void *camera = RenderCamera();
    return camera != nullptr ? SizeOf(camera) : std::nullopt;
}

std::optional<ScreenPoint> ToScreen(const Vector &point)
{
    void *camera = RenderCamera();
    if (camera == nullptr)
    {
        return std::nullopt;
    }
    const auto size = SizeOf(camera);
    if (!size)
    {
        return std::nullopt;
    }
    Vector4 world(point);
    world.w = 1.0F;
    ScreenPoint screen;
    using Fn = void(__fastcall *)(void *, int, const Vector4 *, float *, float *, bool *);
    (*reinterpret_cast<Fn **>(camera))[kPointToPixelSlot](camera, 0, &world, &screen.x, &screen.y, &screen.inFront);
    screen.inView = screen.inFront && screen.x >= 0 && screen.y >= 0 && screen.x < static_cast<float>(size->width) &&
                    screen.y < static_cast<float>(size->height);
    return screen;
}

std::vector<Hit> PickAt(std::int32_t x, std::int32_t y)
{
    void *camera = RenderCamera();
    if (camera == nullptr)
    {
        return {};
    }
    const auto size = SizeOf(camera);
    if (!size || x < 0 || y < 0 || x >= size->width || y >= size->height)
    {
        return {};
    }
    Vector4 nearPoint;
    Vector4 farPoint;
    using Fn = void(__fastcall *)(void *, int, std::int32_t, std::int32_t, Vector4 *, Vector4 *);
    (*reinterpret_cast<Fn **>(camera))[kPixelToRaySlot](camera, 0, x, y, &nearPoint, &farPoint);
    const Vector origin = nearPoint.ToVector();
    const Vector end = farPoint.ToVector();
    std::vector<Hit> hits =
        Raycast(origin, {end.x - origin.x, end.y - origin.y, end.z - origin.z}, groups::kCursor, true);
    // As the cursor's own pick is with them: without the effects a plugin
    // left out of it (dao/Vfx.h).
    std::erase_if(hits, [](const Hit &hit) { return hit.object != kInvalidObject && !vfx::IsPickable(hit.object); });
    return hits;
}

namespace
{

// Whether the pick at a pixel meets this object before any other.
bool MeetsFirst(ObjectId object, std::int32_t x, std::int32_t y)
{
    for (const Hit &hit : PickAt(x, y))
    {
        if (hit.kind == Hit::Kind::Object)
        {
            return hit.object == object;
        }
    }
    return false;
}

} // namespace

// A pixel to click an object at: its place on the screen at a height where the pick
// there meets it before any other object, tried at a few heights from its middle
// outward, since an object is picked by its body and not its feet. Of those, the first
// with the pick meeting it at all eight pixels ten away as well, or failing one the
// height with most of them: a pixel inside the object, not on its edge.
//
// Why inside: in play, 2026-10-01, the real cursor put at a pixel on the top edge of a
// spider as drawn was over nothing by the game's own pick (no object, no ground, no
// hits) and the click became a move, while PickAt at that pixel met the spider; 37
// pixels lower the two agreed and the click attacked. The mode's pick is taken 15
// times a second from where the engine has the cursor, and holds no hits when the
// engine gives it no place for the cursor (dao/GameModes.h); at an edge the two can
// differ, so an edge is no place to aim.
std::optional<ScreenPoint> AimAt(ObjectId object)
{
    CClientExoApp *app = CClientExoApp::Get();
    CCObject *target = app != nullptr ? app->GetObjectById(object) : nullptr;
    if (target == nullptr)
    {
        return std::nullopt;
    }
    const Vector base = target->Position();
    // From about a person's middle outward: a body is picked there, a low
    // thing (a chest, a body on the ground) near its base.
    constexpr std::array<float, 6> kHeights = {0.9F, 1.2F, 0.6F, 1.5F, 0.3F, 0.05F};
    constexpr std::int32_t kAway = 10; // pixels, to each of the eight around
    std::optional<ScreenPoint> best;
    for (const float height : kHeights)
    {
        auto screen = ToScreen({base.x, base.y, base.z + height});
        if (!screen || !screen->inView)
        {
            continue;
        }
        const auto x = static_cast<std::int32_t>(screen->x);
        const auto y = static_cast<std::int32_t>(screen->y);
        if (!MeetsFirst(object, x, y))
        {
            continue;
        }
        for (std::int32_t dx = -kAway; dx <= kAway; dx += kAway)
        {
            for (std::int32_t dy = -kAway; dy <= kAway; dy += kAway)
            {
                if ((dx != 0 || dy != 0) && MeetsFirst(object, x + dx, y + dy))
                {
                    ++screen->around;
                }
            }
        }
        if (screen->around == 8)
        {
            return screen;
        }
        if (!best || screen->around > best->around)
        {
            best = screen;
        }
    }
    return best;
}

std::optional<Hit> FirstHit(const Vector &from, const Vector &to, std::uint32_t mask)
{
    const Vector along{to.x - from.x, to.y - from.y, to.z - from.z};
    const std::vector<Hit> hits = Raycast(from, along, mask); // nearest first
    if (hits.empty() || DistanceSquared(hits.front().position, from) > DistanceSquared(to, from))
    {
        return std::nullopt;
    }
    return hits.front();
}

namespace
{
constexpr std::size_t kMovement = 0xA40; // in the creature: its MovementController
constexpr std::size_t kSearch = 0x14;    // in the controller
constexpr std::size_t kWaypoints = 0x70; // in the search: a std::vector of 12-byte points
constexpr std::size_t kNext = 0x88;      // in the search

using core::Point3;

std::vector<Point3> Points(const std::vector<Vector> &waypoints)
{
    std::vector<Point3> out;
    out.reserve(waypoints.size());
    for (const Vector &v : waypoints)
    {
        out.push_back({v.x, v.y, v.z});
    }
    return out;
}
} // namespace

float WalkPath::Length() const
{
    return core::PathLength(Points(waypoints));
}

float WalkPath::Remaining(const Vector &from) const
{
    return core::PathRemaining(Points(waypoints), next, {from.x, from.y, from.z});
}

namespace
{

// The path a controller holds, if it has waypoints.
std::optional<WalkPath> PathOf(std::uintptr_t controller)
{
    const auto search = controller != 0 ? TryRead<std::uintptr_t>(controller + kSearch) : std::nullopt;
    if (!search || *search == 0)
    {
        return std::nullopt;
    }
    const auto points = TryRead<core::VectorRep<Point3>>(*search + kWaypoints);
    const auto next = TryRead<std::int32_t>(*search + kNext);
    if (!points || !next)
    {
        return std::nullopt;
    }
    const auto view = points->View();
    if (view.empty() || view.size() > 4096)
    {
        return std::nullopt;
    }
    WalkPath path;
    for (const Point3 &p : view)
    {
        path.waypoints.push_back({p.x, p.y, p.z});
    }
    path.next = *next < 0 ? 0 : static_cast<std::size_t>(*next);
    return path;
}

std::uintptr_t Movement(const CCCreature *creature)
{
    return creature != nullptr
               ? TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(creature) + kMovement).value_or(0)
               : 0;
}

// A search of this library's own (dao/Addresses.h, "A search apart from any
// creature's"): the engine's records, made here as IsLocationSafe makes
// them on its stack, and the path freed when it goes.
constexpr std::size_t kPathWorld = 0x0C;      // in the controller
constexpr std::size_t kParameters = 0x10;     // in the controller
constexpr std::size_t kParametersHead = 0x10; // of the parameters: id, radius, size class
constexpr std::size_t kAreaNumber = 0x10;     // in the area
constexpr std::size_t kFacing = 0x130;        // in the creature, after its position
constexpr float kNearGoal = 2.0F;             // metres, as RequestPath asks
constexpr float kNearStart = 5.0F;            // metres, as RequestPath asks
constexpr float kSlice = 0.01F;               // seconds of search a step
constexpr std::int32_t kSlices = 200;
// Points ahead of where a search gave up, looked at for a door, and the
// metres between them: 2.5 m in all, in steps finer than a door is thick
// (a 0.5 m step passed over a cell door 2.2 m off, DAOLib #39).
constexpr int kAhead = 25;
constexpr float kAheadStep = 0.1F;
// A closed door this near where a search stopped, whose footprint the
// line there crosses, is taken for what stopped it (DAOLib #39).
constexpr float kDoorNear = 3.0F;
constexpr std::uint8_t kDoorBaseType = 1;      // placeable_basetypes: Door
constexpr std::uint16_t kDoorClosedLocked = 1; // statecnt_door: 0 closed, 1 locked; 2 on open or dead
constexpr std::size_t kBarredAt = 0x84;        // in the path: a pointer to the barred cell's point, once noted
constexpr auto kLongest = std::chrono::milliseconds(40); // of the game's frame, for one search
constexpr int kMostLegs = 6;                             // searches in all, the first and those after
constexpr float kLegProgress = 2.0F;                     // metres nearer a leg must get to search on
constexpr float kArrived = 0.25F;                        // metres from the destination
constexpr std::size_t kActive = 0x00;                    // in the path: a route is being worked out
constexpr std::size_t kWhole = 0x01;                     // in the path: every stretch has been worked out
constexpr std::size_t kGaveUp = 0x48;                    // in the path: stretches it could not work out
// In the path, set by the search's step (0x00A62640): the mover is wider
// than a cell of the grid, so the fine search takes only cells with room
// about them (0x00A461A0), and the mover is a large one near its
// destination (0x007C4B80).
constexpr std::size_t kWide = 0x8D;
constexpr std::size_t kNearGoalFlag = 0x8C;
constexpr std::size_t kWorldStamps = 0xC4;  // in the pathfinding world: which stamps stop the mover
constexpr std::size_t kWorldSize = 0xC8;    // in the pathfinding world: the mover's size
constexpr std::size_t kMoverStamps = 0xC90; // in the creature: not 0 for the other kind of stamp
constexpr std::size_t kSizeSlot = 39;       // the creature's vtable: its size, a float
constexpr std::int32_t kFound = 1;
constexpr std::int32_t kNone = 5;
constexpr std::int32_t kNoMesh = 8;
constexpr std::int32_t kBarred = 6; // a door, or the like, on the way (0x00A4D1F0, a cell of kind 7)

class OwnSearch
{
  public:
    OwnSearch(std::uintptr_t world, std::uintptr_t parameters)
    {
        using ParametersFn = void(__fastcall *)(void *);
        reinterpret_cast<ParametersFn>(Module::Address(addr::kMovementParameters_Constructor))(parameters_.data());
        // The mover's id, radius and size class; the obstacle it stands as
        // stays the creature's own.
        made_ = TryCopy(parameters_.data(), parameters, kParametersHead);
        if (made_)
        {
            using Fn = void *(__fastcall *)(void *, int, std::uintptr_t, void *);
            reinterpret_cast<Fn>(Module::Address(addr::kMovementController_Constructor))(controller_.data(), 0, world,
                                                                                         parameters_.data());
        }
    }
    ~OwnSearch()
    {
        if (made_)
        {
            using Fn = void(__fastcall *)(void *);
            reinterpret_cast<Fn>(Module::Address(addr::kMovementController_Free))(controller_.data());
        }
    }
    OwnSearch(const OwnSearch &) = delete;
    OwnSearch &operator=(const OwnSearch &) = delete;

    bool Made() const
    {
        return made_;
    }
    void *Controller()
    {
        return controller_.data();
    }

  private:
    alignas(16) std::array<std::byte, 0x50> parameters_{};
    alignas(16) std::array<std::byte, 0xB0> controller_{};
    bool made_ = false;
};

// Whether a placeable is a door that is shut: closed or locked.
bool IsShutDoor(const CCPlaceable *placeable)
{
    return placeable->BaseType() == kDoorBaseType && placeable->State() <= kDoorClosedLocked;
}

// The placeable in the way at a point, found as CCCreature::PathBlocker
// finds it (dao/Addresses.h): among the area's placeables within 5 m, one
// whose footprint holds the point. The engine answers the first such and
// asks nothing more of it (read again 2026-10-10: CCPlaceable::Covers is
// the footprint alone), so it names a placeable the story has made
// inactive, which bars nothing, where a shut door beside it does: seen in
// play 2026-10-10, Witch Hunt's Mage Tower basement, a removed barrier
// 1.2 m from the door (DAOLib #53). So here, parting from the engine: an
// inactive placeable is left out, and a shut door wins over any other that
// covers the point, a 6 being a door's barred cells. Seen at that save the
// same day (Eluvian's build, the turn-based mod's session): from 23 m down
// the corridor the search answered 6 with the door, where it had named the
// barrier, and the travel used the door and went through. Not seen: an
// active placeable that is no door and a shut door on the same point, and
// a search that gives up short of an inactive one.
ObjectId Blocker(CClientExoApp &app, const CCArea *area, const Vector &point)
{
    constexpr std::size_t kAreaObjects = 0x690; // in the area: a vector of ids
    constexpr float kReach = 5.0F;
    const auto objects = TryRead<core::VectorRep<ObjectId>>(reinterpret_cast<std::uintptr_t>(area) + kAreaObjects);
    if (!objects || objects->first == nullptr || objects->last < objects->first ||
        objects->last - objects->first > 100000)
    {
        return kInvalidObject;
    }
    const Vector4 at(point);
    using CoversFn = bool(__fastcall *)(CCPlaceable *, int, const Vector4 *);
    const auto covers = reinterpret_cast<CoversFn>(Module::Address(addr::kCCPlaceable_Covers));
    ObjectId first = kInvalidObject;
    for (const ObjectId id : objects->View())
    {
        CCPlaceable *placeable = app.GetPlaceableById(id);
        if (placeable == nullptr || DistanceSquared(placeable->Position(), point) > kReach * kReach ||
            !covers(placeable, 0, &at) || script::api::GetObjectActive(id) == 0)
        {
            continue;
        }
        if (IsShutDoor(placeable))
        {
            return id;
        }
        if (first == kInvalidObject)
        {
            first = id;
        }
    }
    return first;
}

void LiftObstacle(std::uintptr_t controller)
{
    if (controller != 0)
    {
        using Fn = void(__fastcall *)(std::uintptr_t);
        reinterpret_cast<Fn>(Module::Address(addr::kMovementController_LiftObstacle))(controller);
    }
}

void PlaceObstacle(CCCreature *creature)
{
    if (creature != nullptr)
    {
        using Fn = void(__fastcall *)(CCCreature *);
        reinterpret_cast<Fn>(Module::Address(addr::kCCCreature_PlaceObstacle))(creature);
    }
}

} // namespace

// Read 2026-09-30: a creature's MovementController (its +0xA40, 0x006A2570; vftable
// 0x00B596C0) keeps its search at +0x14, whose waypoints are a std::vector of 12-byte
// points at +0x70 (0x00A47620 reads one, bounds-checked by (last - first) / 12) and
// whose next waypoint's index is at +0x88 (0x00A625E0). The move commands' update
// (0x007A9130) asks for the path a slice of time a frame (0x0073EA80, the search
// advanced by 0x00A62640), so a path appears a frame or more after the order, not
// while the game is paused. The search outlives the walk: a creature that has arrived,
// or stood since a load, still answers its last path (seen in play, 2026-09-30), so
// whether it is walking it is its current command's to say (a move:
// CCObject::CurrentCommand). Seen in play.
std::optional<WalkPath> CurrentPath(ObjectId creature)
{
    CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr || !OnGameThread())
    {
        return std::nullopt;
    }
    return PathOf(Movement(app->GetCreatureById(creature)));
}

namespace
{

// The unit direction from one point toward another, in x and y alone when
// `flat`; as it is where the two are one point.
Vector4 UnitToward(const Vector4 &from, const Vector4 &to, bool flat)
{
    Vector4 direction;
    direction.x = to.x - from.x;
    direction.y = to.y - from.y;
    direction.z = flat ? 0.0F : to.z - from.z;
    const float length =
        std::sqrt((direction.x * direction.x) + (direction.y * direction.y) + (direction.z * direction.z));
    if (length > 1e-6F)
    {
        direction.x /= length;
        direction.y /= length;
        direction.z /= length;
    }
    return direction;
}

// As RequestPath begins (0x0073EB24): the pathfinding world is told which
// creatures' stamps stop this mover and how wide it is, which the
// straight-walk check goes by. Left unset, they are the last creature's to
// ask.
void TellWorldOfMover(std::uintptr_t world, CCCreature *mover)
{
    using SizeFn = float(__fastcall *)(CCCreature *);
    const auto *slots = *reinterpret_cast<SizeFn **>(mover);
    *reinterpret_cast<std::uint8_t *>(world + kWorldStamps) =
        TryRead<std::int32_t>(reinterpret_cast<std::uintptr_t>(mover) + kMoverStamps).value_or(0) != 0 ? 1 : 0;
    *reinterpret_cast<float *>(world + kWorldSize) = slots[kSizeSlot](mover);
}

// One search from a point to the destination (Searcher::Run).
struct Leg
{
    std::optional<WalkPath> found;
    bool arrives = false;
    std::optional<Vector> barredAt; // with the answer 6, the barred cell the engine notes
    std::string flags;
};

// One Pathfind's searches: the mover's place in the pathfinding world, the
// destination as settled, the engine's calls, and the answer they fill in.
class Searcher
{
  public:
    // `facing` and `goal` are Pathfind's own, the goal settled before the
    // first leg; the searcher refers to them, as SSE-aligned members would
    // pad it.
    Searcher(CClientExoApp &app, CCArea *area, std::uintptr_t world, std::uintptr_t parameters, std::int32_t number,
             const Vector4 &facing, const Vector4 &goal, PathSearch &answer)
        : app_(app), area_(area), world_(world), parameters_(parameters), number_(number), facing_(facing), goal_(goal),
          answer_(answer)
    {
    }

    bool IsFree(void *controller, const Vector4 &at) const
    {
        return isFree_(controller, 0, &at, number_, 0, 0);
    }
    // The nearest place within `within` metres of `at` along `toward`
    // where the mover fits; `at` if there is none.
    Vector4 NearestFree(void *controller, const Vector4 &at, const Vector4 &toward, float within) const
    {
        Vector4 nearest = at;
        nearestFree_(area_, 0, &at, &toward, within, controller, 0, &nearest);
        return nearest;
    }
    // The searches' time is counted from here, the start and the goal
    // settled.
    void StartClock()
    {
        started_ = std::chrono::steady_clock::now();
    }
    bool OutOfTime() const
    {
        return std::chrono::steady_clock::now() - started_ >= kLongest;
    }
    float ToGoal(const Vector &at) const
    {
        return std::hypot(goal_.x - at.x, goal_.y - at.y);
    }
    static std::int32_t Word(std::uintptr_t at)
    {
        return TryRead<std::int32_t>(at).value_or(0);
    }

    // A search from `from` to the goal, with a controller of its own: a
    // straight walk if one will do, else the search stepped until it
    // answers, the time runs out or the slices are spent.
    Leg Run(const Vector4 &from, int legNumber)
    {
        Leg leg;
        OwnSearch search(world_, parameters_);
        if (!search.Made())
        {
            return leg;
        }
        void *controller = search.Controller();
        const Vector4 toward = UnitToward(from, goal_, false);
        setStart_(controller, 0, number_, &from, &facing_);
        setGoal_(controller, 0, number_, &goal_, &toward);
        const std::uintptr_t path =
            TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(controller) + kSearch).value_or(0);
        if (direct_(controller, 0, 0, 0))
        {
            answer_.result = kFound;
            leg.found = WalkPath{{from.ToVector(), goal_.ToVector()}, 0};
            leg.arrives = true;
        }
        else
        {
            Step(controller, path, leg);
            Judge(path, leg);
        }
        const auto flag = [path](std::size_t at) {
            return path != 0 ? TryRead<std::uint8_t>(path + at).value_or(0) : std::uint8_t{0};
        };
        leg.flags =
            std::format("leg {}: path active {}, whole {}, gave up {}, wide {}, near {}", legNumber, flag(kActive),
                        flag(kWhole), path != 0 ? Word(path + kGaveUp) : 0, flag(kWide), flag(kNearGoalFlag));
        return leg;
    }

    // Where a search gave up part way, the placeable across the way: the
    // engine answers 6 only to a walker standing in a door's barred cells,
    // and a search that stops short of them gives up with what it has (in
    // play, 1.2 m short of a shut door, 53 m along). So the same test of a
    // placeable's footprint, on points ahead the way the path was heading.
    ObjectId BlockerAhead(const std::vector<Vector> &walked) const
    {
        ObjectId found = kInvalidObject;
        Vector heading{goal_.x - walked.back().x, goal_.y - walked.back().y, 0};
        if (walked.size() >= 2)
        {
            const Vector &before = walked[walked.size() - 2];
            heading = {walked.back().x - before.x, walked.back().y - before.y, 0};
        }
        const float span = std::hypot(heading.x, heading.y);
        for (int ahead = 1; span > 1e-6F && ahead <= kAhead && found == kInvalidObject; ++ahead)
        {
            Vector at = walked.back();
            at.x += heading.x / span * kAheadStep * static_cast<float>(ahead);
            at.y += heading.y / span * kAheadStep * static_cast<float>(ahead);
            found = Blocker(app_, area_, at);
        }
        return found;
    }

    // The placeable that stopped the walk, once the legs are done: about the
    // barred cell the engine noted with a 6, as the game looks; ahead of
    // where a search that found its way part way gave up; else what the
    // legs found already.
    ObjectId Stopper(const Leg &leg, const std::vector<Vector> &walked, const Vector &from) const
    {
        if (answer_.result == kBarred)
        {
            Vector at = from;
            if (leg.barredAt)
            {
                at = *leg.barredAt;
            }
            else if (!walked.empty())
            {
                at = walked.back();
            }
            return Blocker(app_, area_, at);
        }
        if (!answer_.path && answer_.result == kFound && !walked.empty() && answer_.blocker == kInvalidObject)
        {
            const ObjectId ahead = BlockerAhead(walked);
            return ahead != kInvalidObject ? ahead : ClosedDoorNear(walked.back());
        }
        return answer_.blocker;
    }

    // The nearest closed door within kDoorNear of where a search stopped
    // whose footprint the straight line from there to it crosses. A locked
    // door's cells are not the barred cells a shut one's are: the engine's
    // search answers "found" with a path that ends where it can get, which
    // may be beside the door or at an angle to it, and notes no barred cell,
    // so nothing of the engine's names it (seen in play 2026-10-06, the
    // Arl of Denerim's dungeon: locked, a one-point path where the mover
    // stood 2.2 m off; unlocked and shut, answer 6 at the door).
    ObjectId ClosedDoorNear(const Vector &stop) const
    {
        constexpr std::size_t kAreaObjects = 0x690; // in the area: a vector of ids
        const auto objects = TryRead<core::VectorRep<ObjectId>>(reinterpret_cast<std::uintptr_t>(area_) + kAreaObjects);
        if (!objects || objects->first == nullptr || objects->last < objects->first ||
            objects->last - objects->first > 100000)
        {
            return kInvalidObject;
        }
        using CoversFn = bool(__fastcall *)(CCPlaceable *, int, const Vector4 *);
        const auto covers = reinterpret_cast<CoversFn>(Module::Address(addr::kCCPlaceable_Covers));
        ObjectId nearest = kInvalidObject;
        float best = kDoorNear * kDoorNear;
        for (const ObjectId id : objects->View())
        {
            CCPlaceable *door = app_.GetPlaceableById(id);
            if (door == nullptr || !IsShutDoor(door) || script::api::GetObjectActive(id) == 0)
            {
                continue;
            }
            const Vector at = door->Position();
            const float distance = DistanceSquared(at, stop);
            if (distance > best || !Crosses(covers, door, stop, at))
            {
                continue;
            }
            best = distance;
            nearest = id;
        }
        return nearest;
    }

  private:
    // Whether the line from `from` to `to` passes through a placeable's
    // footprint, tested every kAheadStep along it.
    template <class Covers>
    static bool Crosses(Covers covers, CCPlaceable *placeable, const Vector &from, const Vector &to)
    {
        const float span = std::hypot(to.x - from.x, to.y - from.y);
        const int steps = std::max(1, static_cast<int>(span / kAheadStep));
        for (int i = 0; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const Vector4 at(Vector{from.x + ((to.x - from.x) * t), from.y + ((to.y - from.y) * t), to.z});
            if (covers(placeable, 0, &at))
            {
                return true;
            }
        }
        return false;
    }

    // The search stepped a slice at a time until it answers, or the slices
    // or the time run out.
    void Step(void *controller, std::uintptr_t path, Leg &leg)
    {
        while (path != 0 && answer_.slices < kSlices && !OutOfTime())
        {
            ++answer_.slices;
            answer_.result = step_(controller, 0, kSlice);
            answer_.answers.push_back(answer_.result);
            leg.found = PathOf(reinterpret_cast<std::uintptr_t>(controller));
            if (answer_.result == kFound || answer_.result == kNone || answer_.result == kNoMesh ||
                answer_.result == kBarred)
            {
                break;
            }
            // The search keeps only two waypoints ahead of the one its
            // walker is heading for (0x00A4C520), and works out the next
            // stretch as the walker nears the end of what it has. No one
            // walks this path, so its walker is put at the last waypoint.
            if (leg.found && !leg.found->waypoints.empty())
            {
                *reinterpret_cast<std::int32_t *>(path + kNext) =
                    static_cast<std::int32_t>(leg.found->waypoints.size() - 1);
            }
        }
    }

    // Whether the leg's path gets there, and where a 6 says it is barred.
    void Judge(std::uintptr_t path, Leg &leg) const
    {
        // A path counts only if it gets there (0x00A4D1F0 ends one it gave
        // up on "found").
        if (leg.found && leg.found->waypoints.size() >= 2)
        {
            const Vector &last = leg.found->waypoints.back();
            const bool there = std::hypot(last.x - goal_.x, last.y - goal_.y) < kArrived;
            const bool whole = TryRead<std::uint8_t>(path + kWhole).value_or(0) != 0;
            leg.arrives = there && (answer_.result == kFound || whole);
        }
        // With a 6 the engine notes the barred cell in the path, for the
        // creature's PathBlocker to look about (0x00A4D1F0).
        if (answer_.result == kBarred)
        {
            if (const auto noted = TryRead<std::uintptr_t>(path + kBarredAt); noted && *noted != 0)
            {
                leg.barredAt = TryRead<Vector>(*noted);
            }
        }
    }

    using IsFreeFn = bool(__fastcall *)(void *, int, const Vector4 *, std::int32_t, std::int32_t, std::int32_t);
    using NearestFn = std::int32_t(__fastcall *)(CCArea *, int, const Vector4 *, const Vector4 *, float, void *,
                                                 std::int32_t, Vector4 *);
    using SetEndFn = bool(__fastcall *)(void *, int, std::int32_t, const Vector4 *, const Vector4 *);
    using DirectFn = bool(__fastcall *)(void *, int, std::int32_t, std::int32_t);
    using StepFn = std::int32_t(__fastcall *)(void *, int, float);

    CClientExoApp &app_;
    CCArea *area_;
    std::uintptr_t world_;
    std::uintptr_t parameters_;
    std::int32_t number_;
    const Vector4 &facing_;
    const Vector4 &goal_;
    PathSearch &answer_;
    std::chrono::steady_clock::time_point started_;
    IsFreeFn isFree_ = reinterpret_cast<IsFreeFn>(Module::Address(addr::kMovementController_IsFree));
    NearestFn nearestFree_ = reinterpret_cast<NearestFn>(Module::Address(addr::kCCArea_NearestFree));
    SetEndFn setStart_ = reinterpret_cast<SetEndFn>(Module::Address(addr::kMovementController_SetStart));
    SetEndFn setGoal_ = reinterpret_cast<SetEndFn>(Module::Address(addr::kMovementController_SetGoal));
    DirectFn direct_ = reinterpret_cast<DirectFn>(Module::Address(addr::kMovementController_Direct));
    StepFn step_ = reinterpret_cast<StepFn>(Module::Address(addr::kMovementController_Step));
};

} // namespace

// Read 2026-09-30 (dao/Addresses.h, "A search apart from any creature's"): a search is
// a plain record the engine also makes on the stack of a script command
// (IsLocationSafe), so this makes one of its own, with the creature's size, and follows
// RequestPath's order with it. If the creature does not fit at the point, the nearest
// place within 2 m back toward the creature is the destination, as for a click. The
// creature's own obstacle is taken out of the pathfinding world for the search and put
// back after, as the game does each time it asks for a path; `toward` names a creature
// standing at the point, whose obstacle is lifted the same way (the game does that for
// the creature a mover follows), so a path to an enemy ends at it rather than failing.
// A straight walk, where one will do, is the two ends; else the search is run to its
// end, in slices, for at most 40 ms.
//
// The engine's search is made for a walker: it works out only the stretch ahead of the
// waypoint its walker heads for, and the next as the walker nears the end of that, so
// this moves its own search's walker along to have the whole path. And it answers
// "found" for a path it gave up on part way, so a path counts here only if it ends at
// the destination.
//
// Seen in play, 2026-10-01. The first version answered a straight walk rightly but,
// once the party had walked and so stood as obstacles, every search came back "found"
// with the start alone. This version, in the Elven Ruins and the forest clearing, the
// controlled creature walking as before throughout:
// - to a companion 0.9 to 3.8 m off, before and after walking: the two ends, no
//   search;
// - to a spider's corpse 3.2 m off round a corner: 4 waypoints, 3.42 m, in 6 slices;
//   to another, 7.14 m straight: 6 waypoints, 7.30 m;
// - outdoors: 35.80 m straight, 26 waypoints and 46.56 m in 21 slices; 56.10 m
//   straight, 64 waypoints and 107.62 m in 44 slices;
// - to skeletons 105 to 117 m off in a room never seen: none, the search giving up part
//   way after 17 to 25 slices, 8 to 10 ms each time;
// - to skeletons 67 to 70 m off behind shut doors: none, the search answering 6 at the
//   door. That version went on past the 6 until its 30 ms ran out, 130 to 170 slices;
//   this one stops there.
// - and, through the Eluvian server from the ruins' entrance: 40.7 m straight, 29
//   waypoints and 44.18 m; to creatures 80 to 143 m off past a door, none, the search
//   giving up 53.38 m along, short of the door, with no 6 and so no blocker. Searching
//   on from where it gave up is for that case. In play it changed nothing there: the
//   second search gave up from the same cell. But with the creature standing at that
//   end, 1.2 m from the shut door, the search answered 6 at once ([4, 4, 6]) and, once
//   the engine's own note of the barred cell was read, named the door. Short searches
//   from points further on, where no creature stood, never answered 6 either, and took
//   the call to 47 to 66 ms. So the searching on is gone again: one search, and where
//   it gives up part way, the placeable whose footprint holds a point just ahead is
//   named instead. Seen in play the same day: from the entrance to each of three
//   creatures past the door, the answer 1, 53.38 m of the way, and the blocker the
//   door, in 25 slices; and the walk to the spider in the same room found as before.
// Searched on again, 2026-10-03 (DAOLib #5): a walk of 244 m in the Korcari Wilds found
// nothing, where 90 m to a point on the way and then the rest were found. Read: the
// engine plans a stretch of fine path between each two points of a coarse route, and
// two stretches in a row that fall short of their point end the search as found
// (0x00A4D1F0). A walker's own search does the same, a slice of 3 to 6 ms a frame
// (RequestPath, 0x0073EA80); how a click's move reaches the end of so long a walk is
// not read. So where a search gives up part way and no placeable lies ahead, it is
// searched on from where it stopped, up to six searches in all, each getting at least
// 2 m nearer, within 40 ms. A door ahead still ends it. Seen in play 2026-10-03
// through the Eluvian server: from the Tower of Ishal's courtyard to the
// Quartermaster, 229.5 m straight, a path of 276.4 m in one call, which a single move
// then walked to the end. How many searches it took was not shown.
// Read after, 2026-10-01: RequestPath tells the pathfinding world its mover's size and
// which stamps stop it before it asks, and the search here did not, so its
// straight-walk check went by the last creature to have asked. It tells it now; not
// yet seen in play.
//
// An earlier FindPath drove the creature's own search (RequestPath until it answered)
// and was taken out after play, 2026-09-30: RequestPath takes new ends only when no
// search is in progress, so its answers came a call late, and the search it left stood
// as the creature's path, after which the creature would not walk a click.
//
// PathSearch's blocker: with the answer 6 it is found as the game finds it for a
// creature's own path, within 5 m of the barred cell the search noted, its footprint
// holding that point. Where the search gave up part way (the answer 1), by the same
// test on points up to 2.5 m ahead of where it stopped, the way the path was heading.
// A shut door in play.
PathSearch Pathfind(ObjectId creature, const Vector &to, ObjectId toward)
{
    PathSearch answer;
    CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr || !OnGameThread())
    {
        return answer;
    }
    CCCreature *mover = app->GetCreatureById(creature);
    if (mover == nullptr)
    {
        return answer;
    }
    CCArea *area = mover->Area();
    const std::uintptr_t own = Movement(mover);
    const auto world = own != 0 ? TryRead<std::uintptr_t>(own + kPathWorld) : std::nullopt;
    const auto parameters = own != 0 ? TryRead<std::uintptr_t>(own + kParameters) : std::nullopt;
    const auto number =
        area != nullptr ? TryRead<std::int32_t>(reinterpret_cast<std::uintptr_t>(area) + kAreaNumber) : std::nullopt;
    Vector4 from;
    Vector4 facing;
    if (!world || *world == 0 || !parameters || *parameters == 0 || !number ||
        !TryCopy(&from, reinterpret_cast<std::uintptr_t>(mover) + 0x120, sizeof from) ||
        !TryCopy(&facing, reinterpret_cast<std::uintptr_t>(mover) + kFacing, sizeof facing))
    {
        return answer;
    }
    OwnSearch search(*world, *parameters);
    if (!search.Made())
    {
        return answer;
    }
    void *controller = search.Controller();
    TellWorldOfMover(*world, mover);
    Vector4 goal(to);
    goal.w = from.w;
    Searcher searcher(*app, area, *world, *parameters, *number, facing, goal, answer);
    const Vector4 direction = UnitToward(from, goal, false);

    // The destination, as RequestPath settles it, the obstacles still in
    // place: where the mover does not fit, the nearest place within 2 m
    // back toward the start, so a walk to a creature ends beside it.
    answer.goalFree = searcher.IsFree(controller, goal);
    if (!answer.goalFree)
    {
        Vector4 back;
        back.x = -direction.x;
        back.y = -direction.y;
        back.z = -direction.z;
        goal = searcher.NearestFree(controller, goal, back, kNearGoal);
    }

    // The mover's own obstacle, and that of a creature it walks toward, out
    // of the way for the search, as RequestPath lifts them.
    CCCreature *target = toward != kInvalidObject ? app->GetCreatureById(toward) : nullptr;
    const bool startFreeBefore = searcher.IsFree(controller, from);
    LiftObstacle(own);
    LiftObstacle(Movement(target));

    // The start, its own obstacle gone: where the mover still does not fit
    // (others stand against it), the nearest place within 5 m toward the
    // destination. RequestPath then puts the creature there; this only
    // starts the search there.
    answer.startFree = searcher.IsFree(controller, from);
    if (!answer.startFree)
    {
        from = searcher.NearestFree(controller, from, direction, kNearStart);
    }
    answer.start = from.ToVector();
    answer.goal = goal.ToVector();

    searcher.StartClock();
    Leg leg = searcher.Run(from, 1);
    const std::string flags = leg.flags;
    std::vector<Vector> walked;
    if (leg.found)
    {
        walked = leg.found->waypoints;
    }
    bool arrives = leg.arrives;
    answer.legs = 1;

    // A long walk in the open gives up part way with nothing across it
    // (DAOLib #5: 244 m in the Korcari Wilds, 2026-10-03, where 90 m to a
    // point on the way and then the rest were found): the engine plans a
    // stretch of fine path between each two points of its coarse route, and
    // two stretches in a row that do not reach their point end the search
    // (0x00A4D1F0, the path's +0x48). So it is searched on from where it
    // stopped, as long as each leg gets nearer, no placeable lies across the
    // way and time is left. A door stops a search the same each time
    // (2026-10-01), which is why one ahead ends it.
    while (!arrives && answer.result == kFound && walked.size() >= 2 && answer.legs < kMostLegs &&
           !searcher.OutOfTime())
    {
        answer.blocker = searcher.BlockerAhead(walked);
        if (answer.blocker != kInvalidObject)
        {
            break;
        }
        Vector4 onFrom(walked.back());
        onFrom.w = from.w;
        if (!searcher.IsFree(controller, onFrom))
        {
            onFrom = searcher.NearestFree(controller, onFrom, UnitToward(onFrom, goal, true), kNearStart);
        }
        const float before = searcher.ToGoal(walked.back());
        Leg more = searcher.Run(onFrom, answer.legs + 1);
        if (!more.found || more.found->waypoints.size() < 2 ||
            (!more.arrives && searcher.ToGoal(more.found->waypoints.back()) > before - kLegProgress))
        {
            break;
        }
        ++answer.legs;
        walked.insert(walked.end(), more.found->waypoints.begin() + 1, more.found->waypoints.end());
        arrives = more.arrives;
        leg = std::move(more);
    }

    if (arrives)
    {
        answer.path = WalkPath{walked, 0};
        answer.blocker = kInvalidObject;
    }
    else if (!walked.empty())
    {
        answer.partial = WalkPath{walked, 0};
    }
    answer.waypoints = walked.size();

    answer.blocker = searcher.Stopper(leg, walked, from.ToVector());

    // What the search saw, for a log.
    const std::uintptr_t parameters0 = *parameters;
    answer.trace = std::format("start free {} (with its own obstacle {}), destination free {}; mover's id {:#x}, "
                               "radius {:.2f}, size {}; {}; {} leg(s), the last: {}",
                               answer.startFree, startFreeBefore, answer.goalFree,
                               static_cast<std::uint32_t>(Searcher::Word(parameters0)),
                               TryRead<float>(parameters0 + 4).value_or(0.0F), Searcher::Word(parameters0 + 0xC), flags,
                               answer.legs, leg.flags);

    PlaceObstacle(mover);
    PlaceObstacle(target);
    return answer;
}

std::optional<WalkPath> FindPath(ObjectId creature, const Vector &to, ObjectId toward)
{
    return Pathfind(creature, to, toward).path;
}

namespace
{

// The area's layout and its rooms (dao/Addresses.h, "Rooms and their
// streaming").
constexpr std::size_t kLayout = 0x658;      // in the area
constexpr std::size_t kRooms = 0x424;       // in the layout: an array of room pointers
constexpr std::size_t kRoomCount = 0x428;   // in the layout
constexpr std::size_t kRoomNumber = 0x14;   // in a room
constexpr std::size_t kRoomBounds = 0x120;  // in a room
constexpr std::size_t kRoomLoad = 0x264;    // in a room
constexpr std::size_t kRoomSeen = 0x100;    // in a room
constexpr std::size_t kRoomState = 0x268;   // in a room
constexpr std::size_t kRoomLoading = 0x371; // in a room
constexpr std::size_t kObjectRoom = 0x41C;  // in an object: its room
constexpr std::uint32_t kMaxRooms = 4096;

// The rooms' addresses, in the layout's order.
std::vector<std::uintptr_t> RoomPointers()
{
    std::vector<std::uintptr_t> rooms;
    const CCArea *area = OnGameThread() ? CurrentArea() : nullptr;
    const auto layout =
        area != nullptr ? TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(area) + kLayout) : std::nullopt;
    if (!layout || *layout == 0)
    {
        return rooms;
    }
    const auto first = TryRead<std::uintptr_t>(*layout + kRooms);
    const auto count = TryRead<std::uint32_t>(*layout + kRoomCount);
    if (!first || *first == 0 || !count || *count > kMaxRooms)
    {
        return rooms;
    }
    for (std::uint32_t i = 0; i < *count; ++i)
    {
        const auto room = TryRead<std::uintptr_t>(*first + (i * sizeof(std::uintptr_t)));
        if (room && *room != 0)
        {
            rooms.push_back(*room);
        }
    }
    return rooms;
}

std::optional<Room> ReadRoom(std::uintptr_t at)
{
    const auto number = TryRead<std::int32_t>(at + kRoomNumber);
    const auto load = TryRead<std::int32_t>(at + kRoomLoad);
    const auto state = TryRead<std::int32_t>(at + kRoomState);
    const auto loading = TryRead<std::uint8_t>(at + kRoomLoading);
    const auto seen = TryRead<std::int32_t>(at + kRoomSeen);
    if (!number || !load || !state || !loading || !seen)
    {
        return std::nullopt;
    }
    Room room;
    room.number = *number;
    room.state = *state;
    room.load = *load;
    room.loading = *loading != 0;
    room.seen = *seen;
    return room;
}

} // namespace

// Two things decide a room, read 2026-09-30 and 2026-10-01 (dao/Addresses.h, "Rooms
// and their streaming").
//
// Whether it has been seen follows the party, not the camera. When the rooms the
// party's members stand in change, those rooms and some of their neighbours are in
// view (1); a room in view before stays seen (2), which a save keeps; and a room never
// in view (0) is hidden. In play, 2026-10-01: beyond a shut door all was black in the
// tactical view, the room behind stayed black with the door open, its four soldiers
// inactive, and drew when the hero walked in, when they woke and the fight began.
//
// Whether it is loaded follows the camera. Read 2026-09-30 (dao/Addresses.h, "Rooms and
// their streaming"). Each frame the area's layout takes the scene's camera's position,
// not the party's, and asks its policy how much each room is wanted: now (5) within one
// distance of the camera, in the background (4) within a second, or wherever the
// scene's own check names the room, and not at all (3) beyond a third, which unloads
// it. So what is loaded follows the camera: a camera sent toward a dropped room has it
// loaded in the background, some frames later, the game pausing itself to load only
// when the camera jumps more than 30 m in a frame.
//
// Seen in play, 2026-10-01, in Castle Cousland: 61 rooms, 53 wanted, 1 wanted now and 7
// dropped, the controlled creature's own room wanted and loaded, and none dropped once
// the party stood in the hall. A first reading took "wanted now" for "drawn"; it is
// not.
//
// And outdoors, the same day, through the Eluvian server in the Dalish forest
// clearing: the party, two wolves 25 m off and three corpses were each in a room, the
// rooms' numbers 0x30002 to 0x40003 (cells of the terrain, by the look of them, not a
// file's number), every one in view (1), and none with its load reading loaded, though
// all were on screen: each read load 2 and state 6, a state the indoor policy never
// gives, in explore mode and in a fight alike. So Shown() goes by what has been seen
// alone, and state and load are an indoor room's.
//
// And indoors, the same day, in the Elven Ruins (through the Eluvian server, the
// turn-based mod going by Shown()): rooms 1574 to 1580, the party's own in view and
// every other never seen, not shown, though loaded. The first hall and the entrance
// tunnel 40 m from it are one room. Walking into the next hall put it in view and the
// room beyond it too, unentered. A room behind a shut door stayed never seen, three
// spiders idle in it 13 to 24 m off, and read in view the moment the door opened,
// before anyone walked in; in Castle Cousland a room behind an opened door had stayed
// black until the hero entered, so which neighbours a room shows is the area's own.
// The turn-based mod went by Shown() the same night: four spiders in a room never seen,
// loaded, were left out of a fight next door; the door opened, the room read in view at
// once, and they joined, the camera on their turns showing the room drawn.
std::vector<Room> Rooms()
{
    std::vector<Room> rooms;
    for (const std::uintptr_t at : RoomPointers())
    {
        if (const auto room = ReadRoom(at))
        {
            rooms.push_back(*room);
        }
    }
    return rooms;
}

std::optional<Room> RoomAt(const Vector &point)
{
    const Vector4 at(point);
    // The area's own answer, the one each object's room comes from.
    if (CCArea *area = OnGameThread() ? CurrentArea() : nullptr)
    {
        using RoomAtFn = std::uintptr_t(__fastcall *)(CCArea *, int, const Vector4 *);
        const std::uintptr_t room = reinterpret_cast<RoomAtFn>(Module::Address(addr::kCCArea_RoomAt))(area, 0, &at);
        if (auto found = room != 0 ? ReadRoom(room) : std::nullopt)
        {
            return found;
        }
    }
    // A point in no room: the nearest, by the measure the streaming goes
    // by, the squared distance from a point to a room's bounds.
    using Fn = float(__fastcall *)(std::uintptr_t bounds, int, const Vector4 *);
    const auto distanceSquared = reinterpret_cast<Fn>(Module::Address(addr::kBounds_DistanceSquared));
    std::optional<Room> nearest;
    for (const std::uintptr_t pointer : RoomPointers())
    {
        auto room = ReadRoom(pointer);
        if (!room)
        {
            continue;
        }
        room->distance = std::sqrt(std::max(distanceSquared(pointer + kRoomBounds, 0, &at), 0.0F));
        if (!nearest || room->distance < nearest->distance)
        {
            nearest = room;
        }
    }
    return nearest;
}

std::optional<Room> RoomOf(ObjectId object)
{
    CClientExoApp *app = CClientExoApp::Get();
    const CCObject *found = app != nullptr && OnGameThread() ? app->GetObjectById(object) : nullptr;
    if (found == nullptr)
    {
        return std::nullopt;
    }
    // The room the object keeps for itself, which the game goes by.
    const auto own = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(found) + kObjectRoom);
    if (auto room = own && *own != 0 ? ReadRoom(*own) : std::nullopt)
    {
        return room;
    }
    return RoomAt(found->Position());
}

// The nearest shape between the points among groups::kSight blocks, except a placeable
// that is not a door or of base type 8 (0x0067A670 reads GetPlaceableBaseType), and
// `ignore`. Only the nearest is judged, so a chest in front of a wall leaves the view
// clear, as the scripts see it.
Sight LineOfSight(const Vector &from, const Vector &to, ObjectId ignore)
{
    return Ask(CurrentArea(), from, to, ignore);
}

Sight LineOfSight(ObjectId viewer, ObjectId target)
{
    CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr || !OnGameThread())
    {
        return {};
    }
    CCObject *from = app->GetObjectById(viewer);
    CCObject *to = app->GetObjectById(target);
    if (from == nullptr || to == nullptr)
    {
        return {};
    }
    CCArea *area = from->Area();
    if (area == nullptr || area->Id() != to->AreaId())
    {
        return {};
    }
    Vector eye = from->Position();
    Vector seen = to->Position();
    eye.z += kEyeHeight;
    seen.z += kEyeHeight;
    return Ask(area, eye, seen, kInvalidObject);
}

// The obstacle placed: the byte at the movement parameters' +0x40, which
// MeleeRingSpot reads before lifting it. A stamp marks the cells under it (kind 6),
// which the melee rings' fit test and the straight-walk check refuse; a creature
// places it in its own update, so while the world is held a creature that has just
// stopped may stand unstamped (DAOLib #19).
std::optional<bool> ObstaclePlaced(ObjectId creature)
{
    // The movement controller's parameters (+0x10), their +0x40 the
    // obstacle's placed flag.
    constexpr std::uintptr_t kPlaced = 0x40;
    CClientExoApp *app = CClientExoApp::Get();
    const std::uintptr_t movement = app != nullptr ? Movement(app->GetCreatureById(creature)) : 0;
    const auto parameters = movement != 0 ? TryRead<std::uint32_t>(movement + kParameters) : std::nullopt;
    if (!parameters || *parameters == 0)
    {
        return std::nullopt;
    }
    return TryRead<std::uint8_t>(*parameters + kPlaced).value_or(0) != 0;
}

// DAOLib #21. Read 2026-10-04: the model is a GraphicsStructures::Object, reached as
// CCCreature::HeadTransform reaches it (the animation set at the creature's +0x154, its
// +0x18, that one's +4, whose slot 19 answers the object). Its Graphics::IRenderable
// (at +4) answers the box by its slot 5 (0x00549B80, __thiscall (Vector4 *min,
// Vector4 *max, bool world), `ret 0xC`): the box cached at the object's +0x140 and
// +0x150, made on the first ask (0x00549140) from the model's meshes, each mesh's own
// box at its node's place less the object's; with `world`, its two corners put through
// the object's place, rotation and uniform scale (the interface's +0x58). So the box is
// in model space, about the model's place on the ground, unscaled and unturned, and
// made once: the model's rest box, its meshes' own bounds, not following the
// creature's pose as it animates (a creature's skinned meshes sit at the model's
// root). A creature that changes its gear keeps the box it had (not yet seen either
// way). Its largest extent across is the larger of Size().x and Size().y, the box
// being in the model's own axes.
//
// Seen in play 2026-10-04 (the turn-based mod, examples/world, Ortan Thaig, all at
// scale 1): a bronto 1.37 by 3.80 and 2.44 high, a giant spider 1.76 by 2.30 and 1.19,
// a deepstalker 0.82 by 2.54 and 0.90, the long bodies as drawn. A person's is its rest
// pose's, arms out: 1.05 to 1.24 m across, 1.60 to 1.83 m high, its head node 0.45 to
// 0.5 m under the top (a bone at the neck). An ogre's, 4.00 by 1.19 and 4.60 high, is
// its upright arms-out rest pose, where it stands about 3 m as drawn. So across is a
// body's length for a creature that is not a person, and twice a person's shoulders
// for a person. The high dragon's (seen 2026-10-05, its body on the mountain top, the
// save after its fight) is 31.07 by 21.75 and 14.82 high, its wings spread, where the
// body a player looks at is 8 to 10 m: for a winged model the box is the wings'.
std::optional<ModelBox> ModelBoxOf(ObjectId creature)
{
    constexpr std::uintptr_t kAnimationSet = 0x154; // in the creature
    constexpr std::uintptr_t kHolder = 0x18;        // in the animation set
    constexpr std::uintptr_t kModel = 0x4;          // in that: the object's +8
    constexpr std::size_t kObjectSlot = 19;         // answers the object
    constexpr std::uintptr_t kRenderable = 0x4;     // in the object
    constexpr std::size_t kBoxSlot = 5;
    constexpr std::uintptr_t kScale = 0x58; // in the renderable
    CClientExoApp *app = CClientExoApp::Get();
    CCObject *object = app != nullptr ? app->GetObjectById(creature) : nullptr;
    if (object == nullptr || object->Type() != core::ObjectType::Creature || !OnGameThread())
    {
        return std::nullopt;
    }
    const auto set = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(object) + kAnimationSet);
    const auto holder = set && *set != 0 ? TryRead<std::uintptr_t>(*set + kHolder) : std::nullopt;
    const auto model = holder && *holder != 0 ? TryRead<std::uintptr_t>(*holder + kModel) : std::nullopt;
    const auto modelTable = model && *model != 0 ? TryRead<std::uintptr_t>(*model) : std::nullopt;
    if (!modelTable || *modelTable == 0)
    {
        return std::nullopt;
    }
    // __thiscall, no arguments: the object the interface is part of.
    using ObjectFn = std::uintptr_t(__fastcall *)(std::uintptr_t, int);
    const auto objectOf = TryRead<std::uintptr_t>(*modelTable + (kObjectSlot * sizeof(void *)));
    const std::uintptr_t graphics = objectOf && *objectOf != 0 ? reinterpret_cast<ObjectFn>(*objectOf)(*model, 0) : 0;
    const std::uintptr_t renderable = graphics != 0 ? graphics + kRenderable : 0;
    // Only GraphicsStructures::Object's own interface: the slot read.
    const auto table = renderable != 0 ? TryRead<std::uintptr_t>(renderable) : std::nullopt;
    if (!table || *table != Module::Address(addr::kGraphicsStructures_Object_RenderableVftable))
    {
        return std::nullopt;
    }
    using BoxFn = void(__fastcall *)(std::uintptr_t, int, Vector4 *, Vector4 *, bool);
    const auto box = reinterpret_cast<const BoxFn *>(*table)[kBoxSlot];
    Vector4 low;
    Vector4 high;
    box(renderable, 0, &low, &high, false);
    ModelBox answer;
    answer.min = low.ToVector();
    answer.max = high.ToVector();
    answer.scale = TryRead<float>(renderable + kScale).value_or(1.0F);
    box(renderable, 0, &low, &high, true);
    answer.worldMin = low.ToVector();
    answer.worldMax = high.ToVector();
    return answer;
}

// The distance within which a creature is shown (dao/Addresses.h, "Where a
// creature's animation events come from"; DAOLib #88): a float the game
// reads each time it goes through the area's objects. The game writes it
// too, when it applies the graphics detail (0x00458EB0: 30, 35, 50 or 60 by
// the level; read by the address library's session, 2026-10-12), so a
// distance a plugin asks for is held: put back each frame, with what the
// game last wrote kept to give back. Seen in play 2026-10-12 with the
// probe, which holds it the same way: at 60, a fight 82 m from the camera
// had no impact raised in 14 s; at 400, 63.
namespace
{

std::mutex g_showMutex;
std::optional<float> g_showHeld; // the distance a plugin holds
float g_showGames = 0.0F;        // what the game had there, to give back
bool g_showWatched = false;

void HoldCreatureShow()
{
    auto *distance = At<float>(addr::kCreatureShowDistance);
    const std::lock_guard lock(g_showMutex);
    if (distance != nullptr && g_showHeld && *distance != *g_showHeld)
    {
        g_showGames = *distance; // the game has applied its own again
        *distance = *g_showHeld;
    }
}

} // namespace

std::optional<float> CreatureShowDistance()
{
    const auto *distance = At<float>(addr::kCreatureShowDistance);
    return distance != nullptr ? std::optional<float>(*distance) : std::nullopt;
}

bool SetCreatureShowDistance(std::optional<float> metres)
{
    auto *distance = At<float>(addr::kCreatureShowDistance);
    if (distance == nullptr || (metres && !(*metres > 0.0F)))
    {
        return false;
    }
    bool watch = false;
    {
        const std::lock_guard lock(g_showMutex);
        if (!g_showHeld)
        {
            g_showGames = *distance;
        }
        g_showHeld = metres;
        *distance = metres.value_or(g_showGames);
        watch = metres.has_value() && !std::exchange(g_showWatched, true);
    }
    if (watch)
    {
        OnFrame(&HoldCreatureShow);
    }
    return true;
}

} // namespace dao::world

namespace dao::detail
{

void *RenderCamera()
{
    return world::RenderCamera();
}

} // namespace dao::detail
