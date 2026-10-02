// Example: drawing in the world (dao/Vfx.h).
//
// While the game is in explore or combat mode, a ring follows the ground
// under the cursor: the game's aiming ring, in its own colour if the point
// is within kReach of the party's controlled creature and tinted red if
// not, scaled to kRingRadius. The tint is set after the ring is placed and
// again each time it is looked at, since one given at placing did not show
// in play (dao/Vfx.h says why that may be). A turn-based mode's movement
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
constexpr dao::vfx::Tint kTooFar{1.0F, 0.16F, 0.10F, 1.0F};

dao::ObjectId g_ring = dao::kInvalidObject;
bool g_ringInReach = false;
std::size_t g_ringParts = 0; // what the ring's model had when last looked at
bool g_tintSaid = false;     // the first tint of this ring is in the log

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
    return std::sqrt(dx * dx + dy * dy + dz * dz);
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
        // The same ring for both, so that the tint is all that differs.
        g_ring = dao::vfx::PlaceRing(*ground, kRingRadius);
        g_ringInReach = inReach;
        g_ringParts = dao::vfx::Parts(g_ring);
        g_tintSaid = false;
        spdlog::info("ring 0x{:x} at ({:.2f}, {:.2f}, {:.2f}), {:.2f} m away: {}; its model has {} part(s) now", g_ring,
                     ground->x, ground->y, ground->z, away,
                     inReach ? "in reach, the ring's own colour" : "too far, to be tinted red", g_ringParts);
    }
    else if (!dao::vfx::Move(g_ring, *ground))
    {
        spdlog::info("ring 0x{:x} is gone", g_ring);
        g_ring = dao::kInvalidObject;
    }
    if (g_ring == dao::kInvalidObject)
    {
        return;
    }
    const std::size_t parts = dao::vfx::Parts(g_ring);
    if (parts != g_ringParts)
    {
        spdlog::info("ring 0x{:x}: its model has {} part(s) now, {} before", g_ring, parts, g_ringParts);
        g_ringParts = parts;
    }
    if (!g_ringInReach)
    {
        const bool set = dao::vfx::SetTint(g_ring, kTooFar);
        if (!g_tintSaid && parts != 0)
        {
            spdlog::info("ring 0x{:x}: tinted red after placing, with {} part(s): {}", g_ring, parts, set);
            g_tintSaid = true;
        }
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
