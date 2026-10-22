// Example: effects without a light of their own dimmed, and the area's running effects listed
// (dao/Vfx.h).
//
// The particles of an effect whose model has no point light, a waterfall,
// mist, smoke, dust, are drawn at a third of their colour, as an effect lit
// by a dim night would be; an effect with a light of its own, a fire, a
// brazier, a spell, is left as it is, since it gives its light.
//
// What to look for in play: by night in Redcliffe's village, the mist and
// smoke fainter, the fires as bright as ever. Each emitter is logged the
// first time it is drawn, with its name, its model's, whether the model
// has a light and where it is. Every ten seconds the log counts the area's
// running effects, off screen as well, and names each the first time it is
// listed, with its row, model, owner and place: after the siege, Redcliffe's
// pyre burns as row 4014, fxe_fire_m_ns_p, on the pyre.
//
// What it does goes to example_emitters.log.
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/Vfx.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>

namespace
{

constexpr float kUnlit = 1.0F / 3.0F; // what is left of the colour of an effect with no light

std::mutex g_seenMutex;
std::unordered_set<std::uintptr_t> g_seen; // the emitters logged

void Drawn(dao::vfx::EmitterColor &emitter)
{
    {
        const std::lock_guard lock(g_seenMutex);
        if (g_seen.insert(emitter.node).second)
        {
            spdlog::info("emitter {} '{}' of {}: {}, its colour {:.2f} {:.2f} {:.2f} {:.2f}, at {:.1f} {:.1f} {:.1f}",
                         g_seen.size(), dao::Utf8(emitter.name), dao::Utf8(emitter.model),
                         emitter.modelHasLight ? "has a light" : "no light", emitter.color.r, emitter.color.g,
                         emitter.color.b, emitter.color.a, emitter.position.x, emitter.position.y, emitter.position.z);
        }
    }
    if (!emitter.modelHasLight)
    {
        emitter.color.r *= kUnlit;
        emitter.color.g *= kUnlit;
        emitter.color.b *= kUnlit;
    }
}

std::unordered_set<dao::ObjectId> g_listed; // the running effects logged, on the game thread

void Frame()
{
    using Clock = std::chrono::steady_clock;
    static Clock::time_point logged = Clock::now();
    if (Clock::now() - logged < std::chrono::seconds(10))
    {
        return;
    }
    logged = Clock::now();
    const auto effects = dao::vfx::RunningEffects();
    spdlog::info("{} effects running", effects.size());
    for (const auto &effect : effects)
    {
        if (g_listed.insert(effect.id).second)
        {
            const std::string where = effect.position ? fmt::format("at {:.1f} {:.1f} {:.1f}", effect.position->x,
                                                                    effect.position->y, effect.position->z)
                                                      : "on an item, no place";
            spdlog::info("effect {:#x}: row {}, {} ({}), on {:#x}, {}", effect.id, effect.vfx, dao::Utf8(effect.model),
                         effect.modelHasLight ? "has a light" : "no light", effect.owner, where);
        }
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_emitters");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::vfx::OnEmitterColor(&Drawn);
    dao::OnFrame(&Frame);
    return true;
}
