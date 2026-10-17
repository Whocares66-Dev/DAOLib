// Example: a sky's light in the shade (dao/Lights.h).
//
// The level's pieces (its meshes, its terrain, its props) are given an
// ambient light of a faint blue on top of vanilla's, which is nothing for
// most of them: what the sun does not reach takes a little of the sky's
// colour. Figures are left alone, since their ambient light comes from a
// light probe and they are given none here.
//
// What to look for in play, outdoors at day: shade, under eaves and on the
// sides of walls away from the sun, lifted from near black to a dim blue.
// Every ten seconds the log has how many objects' ambient light was worked
// out, by chooser, how many had ambient lights of their own, and the
// brightest vanilla gave.
//
// What it does goes to example_lights.log.
#include "dao/Lights.h"
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Plugin.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>

namespace
{

using namespace dao::lights;
using Clock = std::chrono::steady_clock;

constexpr Color kSky{0.05F, 0.07F, 0.10F, 0.0F}; // added to a level piece's ambient

// Counted on the render thread, read and reset on the game thread.
std::atomic<unsigned> g_level{0};
std::atomic<unsigned> g_moving{0};
std::atomic<unsigned> g_unknown{0};
std::atomic<unsigned> g_withLights{0};
std::atomic<float> g_brightest{0};

void Lit(Ambient &ambient)
{
    const float vanilla = *std::max_element(ambient.color.begin(), ambient.color.begin() + 3);
    switch (ambient.chosen)
    {
    case Chosen::ForLevel:
        ++g_level;
        for (int i = 0; i < 3; ++i)
        {
            ambient.color[i] += kSky[i];
        }
        break;
    case Chosen::ForMoving:
        ++g_moving;
        break;
    case Chosen::Unknown:
        ++g_unknown;
        break;
    }
    if (!ambient.lights.empty())
    {
        ++g_withLights;
    }
    float brightest = g_brightest;
    while (vanilla > brightest && !g_brightest.compare_exchange_weak(brightest, vanilla))
    {
    }
}

void Frame()
{
    static Clock::time_point logged = Clock::now();
    if (Clock::now() - logged < std::chrono::seconds(10))
    {
        return;
    }
    logged = Clock::now();
    spdlog::info("ambient worked out in 10 s: {} level pieces, {} moving, {} unknown; {} with ambient lights; "
                 "brightest {:.3f}",
                 g_level.exchange(0), g_moving.exchange(0), g_unknown.exchange(0), g_withLights.exchange(0),
                 g_brightest.exchange(0));
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_lights");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    OnAmbient(&Lit);
    dao::OnFrame(&Frame);
    return true;
}
