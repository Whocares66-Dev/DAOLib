#include "dao/Effects.h"

#include "dao/Addresses.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/Script.h"
#include "dao/TypeName.h"
#include "dao/core/OwedTime.h"
#include "dao/generated/ScriptApi.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace dao::effects
{

namespace
{

std::atomic<bool> g_held{false};

// What each effect is owed of the plugin's advances, by its id, on the
// shared clock or its owner's own (dao/core/OwedTime.h). An effect first
// seen is owed none, so effects applied after an Advance are not aged by
// it.
std::mutex g_lock;
core::OwedTime<std::uint32_t> g_owed;
std::vector<std::function<std::int32_t(const CGameEffect &, std::int32_t)>> g_ageing;
// Effects end and their ids go unused; forgetting them all now and then
// only means an effect seen again waits for the next Advance.
constexpr std::size_t kForgetAfter = 20000;

// The object whose effects are being aged, on this thread: SEffectsHolder::
// AgeEffect, the one caller of AddElapsed, is handed it, as a pointer to
// its effects part inside it (dao/TypeName.h, CompleteObject): until
// 2026-10-02 its +0x10 was read for the id there, which is not the id, so
// no effect was aged by its owner's own clock (the turn-based mod: a
// spider's Overwhelm, advanced on the spider's clock, never ran down).
thread_local ObjectId t_owner = kInvalidObject;

// SEffectsHolder::AgeEffect: __thiscall (owner, effect), `ret 8`.
using AgeEffectFn = std::int32_t(__fastcall *)(void *holder, int edx, void *owner, CGameEffect *effect);
Detour<AgeEffectFn> g_ageEffect;

std::int32_t __fastcall AgeEffect(void *holder, int edx, void *owner, CGameEffect *effect)
{
    const void *object = CompleteObject(owner);
    const auto id = object != nullptr ? TryRead<ObjectId>(reinterpret_cast<std::uintptr_t>(object) + CGameObject::kId)
                                      : std::nullopt;
    const ObjectId outer = std::exchange(t_owner, id.value_or(kInvalidObject));
    const std::int32_t result = g_ageEffect.Original()(holder, edx, owner, effect);
    t_owner = outer;
    return result;
}

// CGameEffect::AddElapsed: __thiscall (ms), `ret 4`.
using AddElapsedFn = void(__fastcall *)(CGameEffect *effect, int edx, std::int32_t ms);
Detour<AddElapsedFn> g_addElapsed;

void __fastcall AddElapsed(CGameEffect *effect, int edx, std::int32_t ms)
{
    if (effect != nullptr)
    {
        std::vector<std::function<std::int32_t(const CGameEffect &, std::int32_t)>> ageing;
        {
            const std::lock_guard lock(g_lock);
            if (g_owed.Items() > kForgetAfter)
            {
                g_owed.ForgetItems();
            }
            ms = (g_held.load(std::memory_order_relaxed) ? 0 : ms) + g_owed.Owed(effect->Id(), t_owner);
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
    std::call_once(once, [] {
        g_ageEffect.Attach(addr::kEffectsHolder_AgeEffect, &AgeEffect);
        g_addElapsed.Attach(addr::kCGameEffect_AddElapsed, &AddElapsed);
    });
}

} // namespace

void Hold(bool held)
{
    Attach();
    g_held = held;
    if (!held)
    {
        const std::lock_guard lock(g_lock);
        g_owed.ShareAll();
    }
}

bool Held()
{
    return g_held;
}

void Advance(float seconds)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_owed.Advance(static_cast<double>(seconds) * 1000);
}

void OwnClock(ObjectId owner, bool own)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_owed.SetOwn(owner, own);
}

void Advance(ObjectId owner, float seconds)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_owed.Advance(owner, static_cast<double>(seconds) * 1000);
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
