// Example: a sky's light in the shade, and the level's lamps dimmed (dao/Lights.h).
//
// The level's pieces (its meshes, its terrain, its props) are given an
// ambient light of a faint blue on top of vanilla's, which is nothing for
// most of them: what the sun does not reach takes a little of the sky's
// colour. Figures are left alone, since their ambient light comes from a
// light probe and they are given none here. The level's own point lights
// (its rooms' layout lights, the lamps and the warm "Hotspots" a day level
// is made with, each room's in its <level>_<x>_<y>_<d>_ll model) are dimmed
// to a quarter, and every other light left as it is.
//
// What to look for in play, outdoors at day: shade, under eaves and on the
// sides of walls away from the sun, lifted from near black to a dim blue.
// Every ten seconds the log has how many objects' ambient light was worked
// out, by chooser, how many had ambient lights of their own, and the
// brightest vanilla gave. Each point light is logged the first time it is
// taken, with its name, its model's and its own colour: by night in
// Redcliffe's village, the glow of the level's fires on the walls about them
// fainter, the party's portraits as bright as ever.
//
// What it does goes to example_lights.log.
#include "dao/Lights.h"
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_set>

namespace
{

using namespace dao::lights;
using Clock = std::chrono::steady_clock;

constexpr Color kSky{0.05F, 0.07F, 0.10F, 0.0F}; // added to a level piece's ambient
constexpr float kLevelLamps = 0.25F;             // what is left of a level's own point light

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

std::mutex g_seenMutex;
std::unordered_set<std::uintptr_t> g_seen; // the point lights logged

void Lamp(PointLight &light)
{
    {
        const std::lock_guard lock(g_seenMutex);
        if (g_seen.insert(light.node).second)
        {
            spdlog::info("point light {} '{}' of {}: its own colour {:.3f} {:.3f} {:.3f}", g_seen.size(),
                         dao::Utf8(light.name), dao::Utf8(light.model), light.own[0], light.own[1], light.own[2]);
        }
    }
    if (light.model.ends_with(L"_ll"))
    {
        for (int i = 0; i < 3; ++i)
        {
            light.color[i] *= kLevelLamps;
        }
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
    OnPointLight(&Lamp);
    dao::OnFrame(&Frame);
    return true;
}
