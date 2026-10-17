// Example: a day and a night (dao/Atmosphere.h).
//
// Once, when the party is first in an area, that area goes from its own
// look to a night and back over two minutes, without a load, and is then
// left as it was. A look here is two things: the sky (the sun's disc, the
// air, the clouds, the fog, the moon) and the level's sunlight (the light
// on the ground and the creatures, and the way their shadows fall). The
// night is made on the spot: the game's own night table for the sky, and
// the area's sun where it stands, in moonlight's colour. Ten times a
// second the plugin sets the blend of the two that the clock asks for.
//
// What to look for in play, outdoors: the sky darkening to the night
// table's purple and the ground going moonlit blue over a minute, then
// back; the log has the share of night every ten seconds, and a line when
// the area has its own look again. The trees darken and go blue with the
// sunlight; grass and buildings keep the light their level was made with
// (dev/TODO.md). At the start the log has the trees' light beside the
// sunlight, which the area's load lit them by. A plugin that wants
// the whole picture darker or greyer as well has the framebuffer's colour
// effect (dao/Framebuffer.h).
//
// What it does goes to example_atmosphere.log.
#include "dao/Atmosphere.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Plugin.h"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <cmath>
#include <numbers>
#include <optional>

namespace
{

using namespace dao::atmosphere;
using Clock = std::chrono::steady_clock;

constexpr std::chrono::duration<float> kRound = std::chrono::minutes(2); // a day and a night
// Redcliffe's night village's sunlight, as its level has it.
constexpr std::array<float, 4> kMoonlight{0.27F, 0.41F, 0.79F, 1.0F};

std::optional<Clock::time_point> g_noon; // when the round began: the first frame with an area to light
bool g_over = false;                     // the round has been made

// How much of the night it is: 0 at noon, 1 at midnight, half a round on.
float NightAt(Clock::duration sinceNoon)
{
    const float rounds = std::chrono::duration<float>(sinceNoon) / kRound;
    return 0.5F - (0.5F * std::cos(rounds * 2 * std::numbers::pi_v<float>));
}

void Frame()
{
    static Clock::time_point last;
    static Clock::time_point logged;
    const Clock::time_point now = Clock::now();
    if (g_over || now - last < std::chrono::milliseconds(100) ||
        dao::CClientExoApp::CurrentMode() != dao::GameMode::Explore)
    {
        return;
    }
    last = now;
    const auto day = AreaOwnSky();
    const auto sun = AreaOwnSunlight();
    if (!day || !sun)
    {
        return; // no area yet
    }
    if (now - g_noon.value_or(now) >= kRound)
    {
        ResetSky();
        ResetSunlight();
        g_over = true;
        spdlog::info("the round is made: the area has its own sky and sunlight again");
        return;
    }
    if (!g_noon)
    {
        // The area's load lit the trees by its sunlight: the two should read the same here.
        const TreeLight trees = CurrentTreeLight().value_or(TreeLight{});
        spdlog::info(
            "the trees' light {:.3f} {:.3f} {:.3f} from {:.3f} {:.3f} {:.3f}; the sunlight {:.3f} {:.3f} {:.3f} "
            "from {:.3f} {:.3f} {:.3f}",
            trees.diffuse[0], trees.diffuse[1], trees.diffuse[2], trees.direction[0], trees.direction[1],
            trees.direction[2], sun->color[0], sun->color[1], sun->color[2], sun->direction[0], sun->direction[1],
            sun->direction[2]);
    }
    g_noon = g_noon.value_or(now);

    const Sky night = WithFog(WithClouds(WithAir(*day, Preset::Night), Preset::Night), Preset::Night);
    Sunlight moon = *sun;
    moon.color = kMoonlight;
    moon.characterColor = kMoonlight;

    const float share = NightAt(now - *g_noon);
    SetSky(dao::core::Blend(*day, night, share));
    SetSunlight(dao::core::Blend(*sun, moon, share));
    if (now - logged >= std::chrono::seconds(10))
    {
        logged = now;
        spdlog::info("night {:.2f}: the sun's intensity {:.2f} of the area's {:.2f}", share,
                     CurrentSky().value_or(*day).sunIntensity, day->sunIntensity);
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_atmosphere");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::OnFrame(&Frame);
    return true;
}
