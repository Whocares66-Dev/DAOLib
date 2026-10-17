#include "fix/LightFlicker.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Log.h"

#include <cstddef>

namespace fix
{

namespace
{

// Animation::RandomWaveController<float>, 0x30 bytes (dao/Addresses.h at
// kRandomWaveController_Update).
struct RandomWave
{
    const void *vftable;
    float *out; // the light's intensity
    float base;
    float variation;
    float period;
    float periodDelta;
    float from;
    float to;
    double cycle; // its length
    double t;     // its time gone
};
static_assert(offsetof(RandomWave, from) == 0x18 && offsetof(RandomWave, to) == 0x1C);
static_assert(offsetof(RandomWave, cycle) == 0x20 && offsetof(RandomWave, t) == 0x28 && sizeof(RandomWave) == 0x30);

// The shortest a light's cycles are on average, in seconds. The data's, nearly
// all 0.075 s give or take 0.025, read as a strobe even eased (the user,
// 2026-10-17: "it's too fast. slow it down", then "try 1 / s"); a light the data
// already has slower keeps its own pace, and the depth of the swing is the data's.
constexpr double kShortestPeriod = 1.0;

using UpdateFn = void(__fastcall *)(RandomWave *wave, int edx, double dt);
dao::Detour<UpdateFn> g_update;

void __fastcall Update(RandomWave *wave, int edx, double dt)
{
    const double t = wave->t + dt;
    // The engine's own test: a new cycle only past this one's length.
    if (t > wave->cycle)
    {
        g_update.Original()(wave, edx, dt);
        // The length the engine drew, period and spread stretched alike.
        if (wave->period > 0.0F && wave->period < kShortestPeriod)
        {
            wave->cycle *= kShortestPeriod / wave->period;
        }
        return;
    }
    wave->t = t;
    // In floats, as the engine works it; a share that is not a number (a cycle of 0) is 0, as before.
    auto x = static_cast<float>(t / wave->cycle);
    if (!(x > 0.0F))
    {
        x = 0.0F;
    }
    else if (x > 1.0F)
    {
        x = 1.0F;
    }
    const float h = x * x * (3.0F - (2.0F * x));
    *wave->out = ((1.0F - h) * wave->from) + (h * wave->to);
}

} // namespace

void InstallLightFlickerFix()
{
    if (!g_update.Attach(dao::addr::kRandomWaveController_Update, &Update))
    {
        spdlog::warn("fixes: flickering lights still jump from one brightness to the next: the detour could not be "
                     "placed (issue #150)");
        return;
    }
    spdlog::info(
        "fixes: flickering lights ease from one brightness to the next, a cycle a second at the fastest (issue #150)");
}

} // namespace fix
