// Example: effects without a light of their own dimmed (dao/Vfx.h).
//
// The particles of an effect whose model has no point light, a waterfall,
// mist, smoke, dust, are drawn at a third of their colour, as an effect lit
// by a dim night would be; an effect with a light of its own, a fire, a
// brazier, a spell, is left as it is, since it gives its light.
//
// What to look for in play: by night in Redcliffe's village, the mist and
// smoke fainter, the fires as bright as ever. Each emitter is logged the
// first time it is drawn, with its name, its model's and whether the model
// has a light.
//
// What it does goes to example_emitters.log.
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/Vfx.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
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
            spdlog::info("emitter {} '{}' of {}: {}, its colour {:.2f} {:.2f} {:.2f} {:.2f}", g_seen.size(),
                         dao::Utf8(emitter.name), dao::Utf8(emitter.model),
                         emitter.modelHasLight ? "has a light" : "no light", emitter.color.r, emitter.color.g,
                         emitter.color.b, emitter.color.a);
        }
    }
    if (!emitter.modelHasLight)
    {
        emitter.color.r *= kUnlit;
        emitter.color.g *= kUnlit;
        emitter.color.b *= kUnlit;
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
    return true;
}
