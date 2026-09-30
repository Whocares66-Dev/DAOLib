#include "dao/Clock.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Effects.h"
#include "dao/Hook.h"
#include "dao/SafeRead.h"
#include "dao/TypeName.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dao::clock
{

namespace
{

std::atomic<bool> g_held{false};

// Advance, for regeneration: as dao/Effects.cpp does for effects, a
// generation per call, and each creature's stats, by address, remember
// the last one they took.
std::mutex g_lock;
std::vector<std::int32_t> g_advances;
std::unordered_map<const void *, std::size_t> g_seen;
constexpr std::size_t kForgetAfter = 4000;

std::int32_t Advanced(const void *stats)
{
    const std::lock_guard lock(g_lock);
    const std::size_t now = g_advances.size();
    const auto [it, first] = g_seen.try_emplace(stats, now);
    if (first)
    {
        if (g_seen.size() > kForgetAfter)
        {
            g_seen.clear();
            g_seen.emplace(stats, now);
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

// CCCreatureStats::UpdateRegeneration: __thiscall (ms, bool in combat),
// `ret 8`.
using UpdateRegenerationFn = void(__fastcall *)(void *stats, int edx, std::int32_t ms, bool inCombat);
Detour<UpdateRegenerationFn> g_updateRegeneration;

void __fastcall UpdateRegeneration(void *stats, int edx, std::int32_t ms, bool inCombat)
{
    if (stats != nullptr)
    {
        ms = (g_held.load(std::memory_order_relaxed) ? 0 : ms) + Advanced(stats);
    }
    g_updateRegeneration.Original()(stats, edx, ms, inCombat);
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once,
                   [] { g_updateRegeneration.Attach(addr::kCCCreatureStats_UpdateRegeneration, &UpdateRegeneration); });
}

// The world timer (EC::WorldTimer) and its scale.
constexpr std::size_t kWorldTimer = 0xD4; // in the application
constexpr std::size_t kScale = 0x54;      // in the timer

float *Scale()
{
    auto *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return nullptr;
    }
    const auto timer = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(app) + kWorldTimer);
    if (!timer || *timer == 0 || TypeName(reinterpret_cast<const void *>(*timer)) != "EC::WorldTimer")
    {
        return nullptr;
    }
    return reinterpret_cast<float *>(*timer + kScale);
}

} // namespace

void SetSpeed(float scale)
{
    if (float *speed = Scale(); speed != nullptr && scale > 0)
    {
        *speed = scale;
    }
}

float Speed()
{
    const float *speed = Scale();
    return speed != nullptr ? *speed : 1.0F;
}

void Hold(bool held)
{
    Attach();
    g_held = held;
    effects::Hold(held);
}

bool Held()
{
    return g_held;
}

void Advance(float seconds)
{
    Attach();
    {
        const std::lock_guard lock(g_lock);
        g_advances.push_back(static_cast<std::int32_t>(seconds * 1000));
    }
    effects::Advance(seconds);
}

} // namespace dao::clock
