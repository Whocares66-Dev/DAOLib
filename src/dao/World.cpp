#include "dao/World.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/core/Msvc8.h"
#include "dao/core/Path.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
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
    return dx * dx + dy * dy + dz * dz;
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
constexpr std::size_t kSceneCamera = 0xC0; // in the scene
constexpr std::size_t kCameraView = 0x4;   // in the camera: what answers the view's size
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
    const auto camera = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(scene) + kSceneCamera);
    return camera ? reinterpret_cast<void *>(*camera) : nullptr;
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
    return Raycast(origin, {end.x - origin.x, end.y - origin.y, end.z - origin.z}, groups::kCursor, true);
}

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
    for (const float height : kHeights)
    {
        const auto screen = ToScreen({base.x, base.y, base.z + height});
        if (!screen || !screen->inView)
        {
            continue;
        }
        for (const Hit &hit : PickAt(static_cast<std::int32_t>(screen->x), static_cast<std::int32_t>(screen->y)))
        {
            if (hit.kind != Hit::Kind::Object)
            {
                continue;
            }
            if (hit.object == object)
            {
                return screen;
            }
            break; // another object is in front here
        }
    }
    return std::nullopt;
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
constexpr int kAhead = 5;               // points ahead of where a search gave up, looked at for a door
constexpr float kAheadStep = 0.5F;      // metres between them
constexpr std::size_t kBarredAt = 0x84; // in the path: a pointer to the barred cell's point, once noted
constexpr auto kLongest = std::chrono::milliseconds(30); // of the game's frame, for one search
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

// The placeable in the way at a point, as CCCreature::PathBlocker finds it
// (dao/Addresses.h).
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
    for (const ObjectId id : objects->View())
    {
        CCPlaceable *placeable = app.GetPlaceableById(id);
        if (placeable == nullptr)
        {
            continue;
        }
        if (DistanceSquared(placeable->Position(), point) <= kReach * kReach && covers(placeable, 0, &at))
        {
            return id;
        }
    }
    return kInvalidObject;
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

std::optional<WalkPath> CurrentPath(ObjectId creature)
{
    CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr || !OnGameThread())
    {
        return std::nullopt;
    }
    return PathOf(Movement(app->GetCreatureById(creature)));
}

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

    // As RequestPath begins (0x0073EB24): the pathfinding world is told
    // which creatures' stamps stop this mover and how wide it is, which
    // the straight-walk check goes by. Left unset, they are the last
    // creature's to ask.
    {
        using SizeFn = float(__fastcall *)(CCCreature *);
        const auto *slots = *reinterpret_cast<SizeFn **>(mover);
        *reinterpret_cast<std::uint8_t *>(*world + kWorldStamps) =
            TryRead<std::int32_t>(reinterpret_cast<std::uintptr_t>(mover) + kMoverStamps).value_or(0) != 0 ? 1 : 0;
        *reinterpret_cast<float *>(*world + kWorldSize) = slots[kSizeSlot](mover);
    }

    Vector4 goal(to);
    goal.w = from.w;
    Vector4 direction;
    direction.x = goal.x - from.x;
    direction.y = goal.y - from.y;
    direction.z = goal.z - from.z;
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (length > 1e-6F)
    {
        direction.x /= length;
        direction.y /= length;
        direction.z /= length;
    }

    using IsFreeFn = bool(__fastcall *)(void *, int, const Vector4 *, std::int32_t, std::int32_t, std::int32_t);
    using NearestFn = std::int32_t(__fastcall *)(CCArea *, int, const Vector4 *, const Vector4 *, float, void *,
                                                 std::int32_t, Vector4 *);
    const auto isFree = reinterpret_cast<IsFreeFn>(Module::Address(addr::kMovementController_IsFree));
    const auto nearestFree = reinterpret_cast<NearestFn>(Module::Address(addr::kCCArea_NearestFree));

    // The destination, as RequestPath settles it, the obstacles still in
    // place: where the mover does not fit, the nearest place within 2 m
    // back toward the start, so a walk to a creature ends beside it.
    answer.goalFree = isFree(controller, 0, &goal, *number, 0, 0);
    if (!answer.goalFree)
    {
        Vector4 back;
        back.x = -direction.x;
        back.y = -direction.y;
        back.z = -direction.z;
        Vector4 nearest = goal;
        nearestFree(area, 0, &goal, &back, kNearGoal, controller, 0, &nearest);
        goal = nearest;
    }

    // The mover's own obstacle, and that of a creature it walks toward, out
    // of the way for the search, as RequestPath lifts them.
    CCCreature *target = toward != kInvalidObject ? app->GetCreatureById(toward) : nullptr;
    const bool startFreeBefore = isFree(controller, 0, &from, *number, 0, 0);
    LiftObstacle(own);
    LiftObstacle(Movement(target));

    // The start, its own obstacle gone: where the mover still does not fit
    // (others stand against it), the nearest place within 5 m toward the
    // destination. RequestPath then puts the creature there; this only
    // starts the search there.
    answer.startFree = isFree(controller, 0, &from, *number, 0, 0);
    if (!answer.startFree)
    {
        Vector4 nearest = from;
        nearestFree(area, 0, &from, &direction, kNearStart, controller, 0, &nearest);
        from = nearest;
    }
    answer.start = from.ToVector();
    answer.goal = goal.ToVector();

    using SetEndFn = bool(__fastcall *)(void *, int, std::int32_t, const Vector4 *, const Vector4 *);
    using DirectFn = bool(__fastcall *)(void *, int, std::int32_t, std::int32_t);
    using StepFn = std::int32_t(__fastcall *)(void *, int, float);
    const auto setStart = reinterpret_cast<SetEndFn>(Module::Address(addr::kMovementController_SetStart));
    const auto setGoal = reinterpret_cast<SetEndFn>(Module::Address(addr::kMovementController_SetGoal));
    const auto direct = reinterpret_cast<DirectFn>(Module::Address(addr::kMovementController_Direct));
    const auto step = reinterpret_cast<StepFn>(Module::Address(addr::kMovementController_Step));
    const auto word = [](std::uintptr_t at) { return TryRead<std::int32_t>(at).value_or(0); };

    // One search from a point to the destination, with a controller of its
    // own: a straight walk if one will do, else the search stepped until it
    // answers, the time runs out (`timed`) or `most` slices are spent.
    struct Leg
    {
        std::optional<WalkPath> found;
        bool arrives = false;
        std::optional<Vector> barredAt; // with the answer 6, the barred cell the engine notes
        std::string flags;
    };
    const auto started = std::chrono::steady_clock::now();
    const auto runLeg = [&](const Vector4 &legFrom, std::int32_t most, bool timed, int number0) {
        Leg leg;
        OwnSearch legSearch(*world, *parameters);
        if (!legSearch.Made())
        {
            return leg;
        }
        void *leg0 = legSearch.Controller();
        Vector4 toward0;
        toward0.x = goal.x - legFrom.x;
        toward0.y = goal.y - legFrom.y;
        toward0.z = goal.z - legFrom.z;
        const float span = std::sqrt(toward0.x * toward0.x + toward0.y * toward0.y + toward0.z * toward0.z);
        if (span > 1e-6F)
        {
            toward0.x /= span;
            toward0.y /= span;
            toward0.z /= span;
        }
        setStart(leg0, 0, *number, &legFrom, &facing);
        setGoal(leg0, 0, *number, &goal, &toward0);
        const std::uintptr_t path =
            TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(leg0) + kSearch).value_or(0);
        if (direct(leg0, 0, 0, 0))
        {
            answer.result = kFound;
            leg.found = WalkPath{{legFrom.ToVector(), goal.ToVector()}, 0};
            leg.arrives = true;
        }
        else
        {
            std::int32_t spent = 0;
            while (path != 0 && spent < most && answer.slices < kSlices &&
                   (!timed || std::chrono::steady_clock::now() - started < kLongest))
            {
                ++spent;
                ++answer.slices;
                answer.result = step(leg0, 0, kSlice);
                answer.answers.push_back(answer.result);
                leg.found = PathOf(reinterpret_cast<std::uintptr_t>(leg0));
                if (answer.result == kFound || answer.result == kNone || answer.result == kNoMesh ||
                    answer.result == kBarred)
                {
                    break;
                }
                // The search keeps only two waypoints ahead of the one its
                // walker is heading for (0x00A4C520), and works out the
                // next stretch as the walker nears the end of what it has.
                // No one walks this path, so its walker is put at the last
                // waypoint.
                if (leg.found && !leg.found->waypoints.empty())
                {
                    *reinterpret_cast<std::int32_t *>(path + kNext) =
                        static_cast<std::int32_t>(leg.found->waypoints.size() - 1);
                }
            }
            // A path counts only if it gets there (0x00A4D1F0 ends one it
            // gave up on "found").
            if (leg.found && leg.found->waypoints.size() >= 2)
            {
                const Vector &last = leg.found->waypoints.back();
                const bool there = std::hypot(last.x - goal.x, last.y - goal.y) < kArrived;
                const bool whole = TryRead<std::uint8_t>(path + kWhole).value_or(0) != 0;
                leg.arrives = there && (answer.result == kFound || whole);
            }
            // With a 6 the engine notes the barred cell in the path, for
            // the creature's PathBlocker to look about (0x00A4D1F0).
            if (answer.result == kBarred)
            {
                if (const auto noted = TryRead<std::uintptr_t>(path + kBarredAt); noted && *noted != 0)
                {
                    leg.barredAt = TryRead<Vector>(*noted);
                }
            }
        }
        const auto flag = [path](std::size_t at) {
            return path != 0 ? TryRead<std::uint8_t>(path + at).value_or(0) : std::uint8_t{0};
        };
        leg.flags =
            std::format("leg {}: path active {}, whole {}, gave up {}, wide {}, near {}", number0, flag(kActive),
                        flag(kWhole), path != 0 ? word(path + kGaveUp) : 0, flag(kWide), flag(kNearGoalFlag));
        return leg;
    };

    Leg leg = runLeg(from, kSlices, true, 1);
    const std::string flags = leg.flags;
    std::vector<Vector> walked;
    if (leg.found)
    {
        walked = leg.found->waypoints;
    }
    if (leg.arrives)
    {
        answer.path = WalkPath{walked, 0};
    }
    else if (!walked.empty())
    {
        answer.partial = WalkPath{walked, 0};
    }
    answer.waypoints = walked.size();

    if (answer.result == kBarred)
    {
        // About the barred cell the engine noted, as the game looks.
        const Vector at = leg.barredAt ? *leg.barredAt : !walked.empty() ? walked.back() : from.ToVector();
        answer.blocker = Blocker(*app, area, at);
    }
    else if (!answer.path && answer.result == kFound && !walked.empty())
    {
        // Given up part way. The engine answers 6 only to a walker standing
        // in a door's barred cells; a search that stops short of them gives
        // up with what it has (in play, 1.2 m short of a shut door, 53 m
        // along). So the same test of a placeable's footprint, on points
        // ahead the way the path was heading.
        Vector heading{goal.x - walked.back().x, goal.y - walked.back().y, 0};
        if (walked.size() >= 2)
        {
            const Vector &before = walked[walked.size() - 2];
            heading = {walked.back().x - before.x, walked.back().y - before.y, 0};
        }
        const float span = std::hypot(heading.x, heading.y);
        for (int ahead = 1; span > 1e-6F && ahead <= kAhead && answer.blocker == kInvalidObject; ++ahead)
        {
            Vector at = walked.back();
            at.x += heading.x / span * kAheadStep * static_cast<float>(ahead);
            at.y += heading.y / span * kAheadStep * static_cast<float>(ahead);
            answer.blocker = Blocker(*app, area, at);
        }
    }

    // What the search saw, for a log.
    const std::uintptr_t parameters0 = *parameters;
    answer.trace = std::format(
        "start free {} (with its own obstacle {}), destination free {}; mover's id {:#x}, radius {:.2f}, size {}; {}",
        answer.startFree, startFreeBefore, answer.goalFree, static_cast<std::uint32_t>(word(parameters0)),
        TryRead<float>(parameters0 + 4).value_or(0.0F), word(parameters0 + 0xC), flags);

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
        const auto room = TryRead<std::uintptr_t>(*first + i * sizeof(std::uintptr_t));
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

} // namespace dao::world
