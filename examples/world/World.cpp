// Example: the world's geometry (dao/World.h).
//
// Point the cursor at a creature or a placeable, and the log says whether
// the party's controlled creature can see it, and where the view is cut if
// not, each time the object under the cursor changes. Every two seconds it
// also casts a ray straight down onto the controlled creature and logs
// each shape it met: what kind of thing, and its collision groups, which
// tell which bits the ground and each kind of object carry. And while the
// creature walks, the path its movement found: its waypoints and length.
// For the object under the cursor it also asks the pathfinder for the walk
// there (FindPath), and logs its length beside the straight distance (and
// every ten seconds does the same for the six creatures nearest the
// controlled one, so a run with no one at the mouse has them too), and
// the room the object stands in with how much the game wants it and whether
// it is loaded; every two seconds, how many of the area's rooms are in each
// state.
//
// Everything runs in a frame callback, on the game's thread, where the
// world's queries must be asked. What it finds goes to example_world.log.
#include "dao/World.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace
{

constexpr std::uint64_t kFramesBetweenRays = 60;   // about two seconds
constexpr std::uint64_t kFramesBetweenWalks = 300; // about ten
constexpr std::int32_t kNearest = 6;
constexpr float kAbove = 10.0F; // metres above the creature the ray starts

const char *KindName(dao::world::Hit::Kind kind)
{
    switch (kind)
    {
    case dao::world::Hit::Kind::Ground:
        return "ground";
    case dao::world::Hit::Kind::Object:
        return "object";
    default:
        return "nothing";
    }
}

// The room an object stands in, and whether it is loaded.
void LogRoomOf(const char *what, dao::ObjectId object)
{
    if (const auto room = dao::world::RoomOf(object))
    {
        spdlog::info("{} 0x{:x} is in room {} ({:.1f} m from its bounds): seen {} ({}), state {}, load {} ({}){}", what,
                     object, room->number, room->distance, room->seen, room->Shown() ? "shown" : "not shown",
                     room->state, room->load, room->Loaded() ? "loaded" : "not loaded",
                     room->loading ? ", loading" : "");
    }
    else
    {
        spdlog::info("{} 0x{:x} has no room", what, object);
    }
}

// How many rooms the area has in each state.
void LogRooms(dao::ObjectId self)
{
    const std::vector<dao::world::Room> rooms = dao::world::Rooms();
    int now = 0;
    int wanted = 0;
    int dropped = 0;
    int loaded = 0;
    int inView = 0;
    int seenBefore = 0;
    for (const dao::world::Room &room : rooms)
    {
        inView += room.seen == dao::world::Room::kInView ? 1 : 0;
        seenBefore += room.seen == dao::world::Room::kSeenBefore ? 1 : 0;
        now += room.state == dao::world::Room::kWantedNow ? 1 : 0;
        wanted += room.state == dao::world::Room::kWanted ? 1 : 0;
        dropped += room.state == dao::world::Room::kDropped ? 1 : 0;
        loaded += room.Loaded() ? 1 : 0;
    }
    static int lastWanted = -1;
    static int lastLoaded = -1;
    static int lastSeen = -1;
    if (wanted != lastWanted || loaded != lastLoaded || inView + 100 * seenBefore != lastSeen)
    {
        lastWanted = wanted;
        lastLoaded = loaded;
        lastSeen = inView + 100 * seenBefore;
        spdlog::info("{} rooms: {} in view, {} seen before, {} never seen; {} wanted now, {} wanted, {} dropped; {} "
                     "loaded",
                     rooms.size(), inView, seenBefore, static_cast<int>(rooms.size()) - inView - seenBefore, now,
                     wanted, dropped, loaded);
        LogRoomOf("the controlled creature", self);
    }
}

// The walk to an object, found now, the creature left as it is. The
// object's own obstacle is lifted for the search when it is a creature.
void LogWalkTo(dao::ObjectId self, dao::ObjectId seen)
{
    dao::CClientExoApp *app = dao::CClientExoApp::Get();
    const dao::CCObject *from = app->GetObjectById(self);
    const dao::CCObject *to = app->GetObjectById(seen);
    if (from == nullptr || to == nullptr)
    {
        return;
    }
    const dao::Vector a = from->Position();
    const dao::Vector b = to->Position();
    const float straight = std::hypot(b.x - a.x, b.y - a.y, b.z - a.z);
    const dao::world::PathSearch search = dao::world::Pathfind(self, b, seen);
    if (search.path)
    {
        const dao::Vector end = search.path->waypoints.back();
        spdlog::info(
            "a walk from 0x{:x} to 0x{:x}: {} waypoints, {:.2f} m ({:.2f} m straight), ending {:.2f} m from it; "
            "answer {} after {} slice(s)",
            self, seen, search.path->waypoints.size(), search.path->Length(), straight,
            std::hypot(end.x - b.x, end.y - b.y, end.z - b.z), search.result, search.slices);
    }
    else
    {
        spdlog::info("no walk from 0x{:x} to 0x{:x} ({:.2f} m straight): answer {} after {} slice(s){}", self, seen,
                     straight, search.result, search.slices,
                     search.partial ? fmt::format(", {:.2f} m of the way found", search.partial->Length()) : "");
    }
    std::string answers;
    for (const std::int32_t answer : search.answers)
    {
        answers += (answers.empty() ? "" : " ") + std::to_string(answer);
    }
    spdlog::info("  the search: from ({:.2f}, {:.2f}, {:.2f}) to ({:.2f}, {:.2f}, {:.2f}), {} waypoint(s) at the end, "
                 "answers [{}]; {}",
                 search.start.x, search.start.y, search.start.z, search.goal.x, search.goal.y, search.goal.z,
                 search.waypoints, answers, search.trace);
}

// The walks to the creatures nearest the controlled one, with no cursor
// needed.
void LogWalksNearby(dao::ObjectId self)
{
    const dao::script::Array nearest =
        dao::script::api::GetNearestObject(self, dao::script::constants::OBJECT_TYPE_CREATURE, kNearest);
    spdlog::info("the {} creature(s) nearest 0x{:x}:", nearest.Size(), self);
    for (std::size_t i = 0; i < nearest.Size(); ++i)
    {
        if (const dao::ObjectId creature = nearest.Object(i); creature != dao::kInvalidObject && creature != self)
        {
            LogWalkTo(self, creature);
        }
    }
}

void LookAtCursor(dao::ObjectId self, dao::ObjectId seen)
{
    if (seen == self)
    {
        return;
    }
    const dao::world::Sight sight = dao::world::LineOfSight(self, seen);
    if (sight)
    {
        spdlog::info("0x{:x} can see 0x{:x}", self, seen);
    }
    else if (sight.blockedAt)
    {
        spdlog::info("0x{:x} cannot see 0x{:x}: blocked at ({:.2f}, {:.2f}, {:.2f})", self, seen, sight.blockedAt->x,
                     sight.blockedAt->y, sight.blockedAt->z);
    }
    else
    {
        spdlog::info("0x{:x} cannot see 0x{:x} (another area, or no answer)", self, seen);
    }
    LogWalkTo(self, seen);
    LogRoomOf("under the cursor", seen);
}

void LookDown(dao::ObjectId self)
{
    const dao::CCObject *creature = dao::CClientExoApp::Get()->GetObjectById(self);
    if (creature == nullptr)
    {
        return;
    }
    dao::Vector above = creature->Position();
    above.z += kAbove;
    // Every group: what is there, not what the cursor or sight would meet.
    const auto hits = dao::world::Raycast(above, {0, 0, -1}, 0xFFFFFFFF);
    spdlog::info("down from above 0x{:x}: {} hit(s)", self, hits.size());
    for (const dao::world::Hit &hit : hits)
    {
        spdlog::info("  {} 0x{:x} at ({:.2f}, {:.2f}, {:.2f}), groups 0x{:08x}, walkable {}", KindName(hit.kind),
                     hit.object, hit.position.x, hit.position.y, hit.position.z, hit.groups, hit.walkable);
    }
}

// The path the creature is walking, if it is: its length, and what is
// left of it from where it stands.
void LogPath(dao::ObjectId self)
{
    const auto path = dao::world::CurrentPath(self);
    const dao::CCObject *creature = dao::CClientExoApp::Get()->GetObjectById(self);
    if (!path || creature == nullptr)
    {
        return;
    }
    spdlog::info("0x{:x} walks a path of {} waypoints, {:.2f} m, {:.2f} m left (heading for waypoint {})", self,
                 path->waypoints.size(), path->Length(), path->Remaining(creature->Position()), path->next);
}

void Frame()
{
    const dao::GameModeExplore *explore = dao::CClientExoApp::CurrentExplore();
    if (explore == nullptr)
    {
        return;
    }
    const dao::ObjectId self = dao::script::api::GetMainControlled();

    static std::optional<dao::ObjectId> lastSeen;
    const std::optional<dao::ObjectId> seen = explore->CursorObject();
    if (seen != lastSeen)
    {
        lastSeen = seen;
        if (seen)
        {
            LookAtCursor(self, *seen);
        }
    }

    if (dao::FrameCount() % kFramesBetweenRays == 0)
    {
        LookDown(self);
        LogPath(self);
        LogRooms(self);
    }
    if (dao::FrameCount() % kFramesBetweenWalks == 0)
    {
        LogWalksNearby(self);
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_world");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->expectedExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::OnFrame(&Frame);
    return true;
}
