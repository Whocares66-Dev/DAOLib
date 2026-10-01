#include "dao/Clock.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Effects.h"
#include "dao/Hook.h"
#include "dao/Module.h"
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

// The frame's clock: the Eclipse game object (a global) keeps an
// EC::ITimer at +0xB4, which its update ticks each frame; the timer keeps
// the frame's seconds at +0x10 and the total at +0x18 (a double).
constexpr std::size_t kFrameTimer = 0xB4;
constexpr std::size_t kFrameSeconds = 0x10;
constexpr std::size_t kTotalSeconds = 0x18;
std::atomic<float> g_speed{1.0F};

const void *FrameTimer()
{
    const auto game = TryRead<std::uintptr_t>(Module::Address(addr::kEclipseGame));
    if (!game || *game == 0)
    {
        return nullptr;
    }
    const auto timer = TryRead<std::uintptr_t>(*game + kFrameTimer);
    return timer ? reinterpret_cast<const void *>(*timer) : nullptr;
}

// EC::ITimer::Update: __thiscall (float fixed frames per second, 0 for
// none), `ret 4`. For the frame's timer, the frame's seconds are scaled
// after it has taken them, and the total with them.
using TimerUpdateFn = void(__fastcall *)(void *timer, int edx, float fixedFps);
Detour<TimerUpdateFn> g_timerUpdate;

void __fastcall TimerUpdate(void *timer, int edx, float fixedFps)
{
    g_timerUpdate.Original()(timer, edx, fixedFps);
    const float speed = g_speed.load(std::memory_order_relaxed);
    if (speed == 1.0F || timer == nullptr || timer != FrameTimer())
    {
        return;
    }
    auto *bytes = static_cast<std::byte *>(timer);
    auto &seconds = *reinterpret_cast<float *>(bytes + kFrameSeconds);
    auto &total = *reinterpret_cast<double *>(bytes + kTotalSeconds);
    const float scaled = seconds * speed;
    total -= static_cast<double>(seconds - scaled);
    seconds = scaled;
}

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

bool SetSpeed(float scale)
{
    if (scale <= 0)
    {
        return false;
    }
    static std::once_flag once;
    static std::atomic<bool> hooked{false};
    std::call_once(once, [] { hooked = g_timerUpdate.Attach(addr::kITimer_Update, &TimerUpdate); });
    g_speed = scale;
    float *world = Scale();
    if (world != nullptr)
    {
        *world = scale;
    }
    return world != nullptr && hooked;
}

float Speed()
{
    return g_speed;
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
