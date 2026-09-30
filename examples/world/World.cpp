// Example: the world's geometry (dao/World.h).
//
// Point the cursor at a creature or a placeable, and the log says whether
// the party's controlled creature can see it, and where the view is cut if
// not, each time the object under the cursor changes. Every two seconds it
// also casts a ray straight down onto the controlled creature and logs
// each shape it met: what kind of thing, and its collision groups, which
// tell which bits the ground and each kind of object carry.
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

#include <cstdint>
#include <optional>

namespace
{

constexpr std::uint64_t kFramesBetweenRays = 60; // about two seconds
constexpr float kAbove = 10.0F;                  // metres above the creature the ray starts

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
