// Example: drawing in the world (dao/Vfx.h).
//
// While the game is in explore or combat mode, a ring follows the ground
// under the cursor: the game's aiming ring if the point is within kReach of
// the party's controlled creature, the one it shows out of reach if not
// (they looked alike in play; dao/Vfx.h), scaled to kRingRadius. A turn-based mode's movement
// preview, measured as the crow flies: a path's length waits on a path
// search a plugin can ask for (dev/TODO.md, item 5). The ring is placed
// once and moved; it is placed afresh when its colour changes, and taken
// away when the cursor leaves the ground. What it does goes to
// example_preview.log.
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/Vfx.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <optional>

namespace
{

constexpr float kReach = 10.0F;             // metres a move may take
constexpr float kRingRadius = 0.75F;        // metres
constexpr std::uint64_t kFramesBetween = 2; // the ring follows every other frame

dao::ObjectId g_ring = dao::kInvalidObject;
bool g_ringInReach = false;

void TakeRingAway()
{
    if (g_ring != dao::kInvalidObject)
    {
        dao::vfx::Remove(g_ring);
        g_ring = dao::kInvalidObject;
    }
}

float Distance(const dao::Vector &a, const dao::Vector &b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

void Frame()
{
    if (dao::FrameCount() % kFramesBetween != 0)
    {
        return;
    }
    const dao::GameModeExplore *explore = dao::CClientExoApp::CurrentExplore();
    const auto ground = explore != nullptr ? explore->CursorGround() : std::nullopt;
    const dao::CCObject *self = dao::CClientExoApp::Get() != nullptr
                                    ? dao::CClientExoApp::Get()->GetObjectById(dao::script::api::GetMainControlled())
                                    : nullptr;
    if (!ground || self == nullptr)
    {
        TakeRingAway();
        return;
    }
    const float away = Distance(self->Position(), *ground);
    const bool inReach = away <= kReach;
    if (g_ring == dao::kInvalidObject || inReach != g_ringInReach)
    {
        TakeRingAway();
        g_ring = dao::vfx::PlaceRing(*ground, kRingRadius, inReach);
        g_ringInReach = inReach;
        spdlog::info("ring 0x{:x} at ({:.2f}, {:.2f}, {:.2f}), {:.2f} m away: {}", g_ring, ground->x, ground->y,
                     ground->z, away, inReach ? "in reach" : "too far");
    }
    else if (!dao::vfx::Move(g_ring, *ground))
    {
        spdlog::info("ring 0x{:x} is gone", g_ring);
        g_ring = dao::kInvalidObject;
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_preview");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::OnFrame(&Frame);
    return true;
}
