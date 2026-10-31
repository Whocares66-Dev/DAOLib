// Example: temporary effects' time, read, set, held and advanced (dao/Effects.h).
//
// Once a second the controlled creature's temporary effects are logged with
// their time left, and how many updates aged them in that second, counted
// by the object OnAgeing says each is on. The first time one is seen, its
// time left is halved; the effects' clock is then held for three seconds,
// in which its time left should stand still, and let go with a second's
// Advance, after which it runs on and ends early. Get the controlled
// creature dazed, stunned or slowed in a fight to see it.
//
// What it sees goes to example_effects.log.
#include "dao/Effects.h"
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <unordered_set>

namespace
{

using Clock = std::chrono::steady_clock;

constexpr auto kHeldFor = std::chrono::seconds(3);
constexpr float kAdvance = 1.0F; // seconds, given as the hold ends

std::atomic<dao::ObjectId> g_controlled{dao::kInvalidObject};
std::atomic<std::int32_t> g_aged{0}; // times one of its effects aged, this second
std::unordered_set<std::uint32_t> g_seen;
std::optional<Clock::time_point> g_heldSince;

// The halving, the hold and its end with an Advance, for an effect first seen.
void Shorten(dao::ObjectId creature, const dao::effects::Standing &effect)
{
    const float half = effect.remaining / 2;
    const bool set = dao::effects::SetRemaining(creature, effect.id, half);
    spdlog::info("effect {}: its time left set to {:.2f} s: {}", effect.id, half, set ? "taken" : "not taken");
    if (set && !g_heldSince)
    {
        dao::effects::Hold(true);
        g_heldSince = Clock::now();
        spdlog::info("the effects' clock held");
    }
}

void LogSecond(dao::ObjectId creature)
{
    const auto effects = dao::effects::On(creature);
    spdlog::info("0x{:x}: its effects aged {} times in the last second", creature, g_aged.exchange(0));
    for (const dao::effects::Standing &effect : effects)
    {
        if (effect.durationType != dao::script::EffectDurationType::Temporary)
        {
            continue;
        }
        spdlog::info("  effect {}, type {}, ability {}: {:.2f} s left of {:.2f} s", effect.id,
                     static_cast<std::int32_t>(effect.type), effect.ability, effect.remaining, effect.duration);
        if (g_seen.insert(effect.id).second)
        {
            Shorten(creature, effect);
        }
    }
}

void Frame()
{
    static Clock::time_point last = Clock::now();
    if (g_heldSince && Clock::now() - *g_heldSince >= kHeldFor)
    {
        dao::effects::Advance(kAdvance);
        dao::effects::Hold(false);
        g_heldSince.reset();
        spdlog::info("the effects' clock let go, {:.0f} s advanced", kAdvance);
    }
    if (Clock::now() - last < std::chrono::seconds(1))
    {
        return;
    }
    last = Clock::now();
    const dao::ObjectId creature = dao::script::api::GetMainControlled();
    g_controlled = creature;
    if (creature != dao::kInvalidObject)
    {
        LogSecond(creature);
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_effects");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    // Every temporary effect of every object passes here on every update,
    // so this only counts.
    dao::effects::OnAgeing([](const dao::CGameEffect &, dao::ObjectId owner, std::int32_t ms) {
        if (owner == g_controlled.load(std::memory_order_relaxed))
        {
            g_aged.fetch_add(1, std::memory_order_relaxed);
        }
        return ms;
    });
    dao::OnFrame(&Frame);
    return true;
}
