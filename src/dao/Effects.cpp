#include "dao/Effects.h"

#include "dao/Addresses.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Script.h"
#include "dao/generated/ScriptApi.h"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dao::effects
{

namespace
{

std::atomic<bool> g_held{false};

// Advance: each call is a generation; an effect, by its id, remembers the
// last generation it was aged to, and takes the milliseconds of every
// generation since. An effect first seen takes none, so effects applied
// after an Advance are not aged by it.
std::mutex g_lock;
std::vector<std::int32_t> g_advances; // milliseconds, one per generation
std::unordered_map<std::uint32_t, std::size_t> g_seen;
std::vector<std::function<std::int32_t(const CGameEffect &, std::int32_t)>> g_ageing;
// Effects end and their ids go unused; forgetting them all now and then
// only means an effect seen again waits for the next Advance.
constexpr std::size_t kForgetAfter = 20000;

std::int32_t Advanced(const CGameEffect &effect)
{
    const std::lock_guard lock(g_lock);
    const std::size_t now = g_advances.size();
    const auto [it, first] = g_seen.try_emplace(effect.Id(), now);
    if (first)
    {
        if (g_seen.size() > kForgetAfter)
        {
            g_seen.clear();
            g_seen.emplace(effect.Id(), now);
        }
        return 0;
    }
    std::int32_t ms = 0;
    for (std::size_t i = it->second; i < now; ++i)
    {
        ms += g_advances[i];
    }
    it->second = now;
    return ms;
}

// CGameEffect::AddElapsed: __thiscall (ms), `ret 4`.
using AddElapsedFn = void(__fastcall *)(CGameEffect *effect, int edx, std::int32_t ms);
Detour<AddElapsedFn> g_addElapsed;

void __fastcall AddElapsed(CGameEffect *effect, int edx, std::int32_t ms)
{
    if (effect != nullptr)
    {
        ms = (g_held.load(std::memory_order_relaxed) ? 0 : ms) + Advanced(*effect);
        std::vector<std::function<std::int32_t(const CGameEffect &, std::int32_t)>> ageing;
        {
            const std::lock_guard lock(g_lock);
            ageing = g_ageing;
        }
        for (const auto &decide : ageing)
        {
            detail::Guard("an effect ageing callback", [&] { ms = decide(*effect, ms); });
        }
    }
    g_addElapsed.Original()(effect, edx, ms);
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] { g_addElapsed.Attach(addr::kCGameEffect_AddElapsed, &AddElapsed); });
}

} // namespace

void Hold(bool held)
{
    Attach();
    g_held = held;
}

bool Held()
{
    return g_held;
}

void Advance(float seconds)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_advances.push_back(static_cast<std::int32_t>(seconds * 1000));
}

void OnAgeing(std::function<std::int32_t(const CGameEffect &effect, std::int32_t ms)> ageing)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_ageing.push_back(std::move(ageing));
}

std::vector<Standing> On(ObjectId object)
{
    std::vector<Standing> out;
    const script::Array effects = script::api::GetEffects(object);
    out.reserve(effects.Size());
    for (std::size_t i = 0; i < effects.Size(); ++i)
    {
        const CGameEffect *effect = effects.Effect(i);
        if (effect == nullptr)
        {
            continue;
        }
        Standing standing{.id = effect->Id(),
                          .type = effect->Type(),
                          .durationType = effect->DurationType(),
                          .ability = effect->AbilityId(),
                          .creator = effect->Creator()};
        if (standing.durationType == script::EffectDurationType::Temporary)
        {
            standing.duration = effect->Duration();
            standing.remaining = effect->Remaining();
        }
        out.push_back(standing);
    }
    return out;
}

} // namespace dao::effects
