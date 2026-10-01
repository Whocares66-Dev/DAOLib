// Example: the game's speed (dao/Clock.h).
//
// Fights run at another speed: when the game enters combat mode the game
// runs at kFightSpeed (2, twice as fast; 0.5 would be slow motion), and at
// its own speed again when the fight ends. dao::clock::SetSpeed scales both
// of the game's clocks, the world timer its rules run on and the frame
// timer its animation runs on, so what the player sees keeps pace with the
// AI, effects and regeneration.
//
// The game's mode is read each frame (a frame callback is cheap, and the
// mode changes on the frame the engine switches it); the speed is set only
// when the mode changes. What it does goes to example_speed.log.
#include "dao/ClientExoApp.h"
#include "dao/Clock.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Plugin.h"

#include <spdlog/spdlog.h>

#include <optional>

namespace
{

constexpr float kFightSpeed = 2.0F; // of the game's own speed

void Frame()
{
    static std::optional<dao::GameMode> last;
    const auto mode = dao::CClientExoApp::CurrentMode();
    if (!mode || mode == last)
    {
        return;
    }
    const bool fighting = *mode == dao::GameMode::Combat;
    const bool wasFighting = last == dao::GameMode::Combat;
    last = mode;
    if (fighting != wasFighting)
    {
        const bool took = dao::clock::SetSpeed(fighting ? kFightSpeed : 1.0F);
        spdlog::info("{}: the world at {:.2f} speed{}", fighting ? "a fight" : "the fight is over", dao::clock::Speed(),
                     took ? "" : " (not taken: no world timer, or the frame's hook not placed)");
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_speed");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->expectedExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::OnFrame(&Frame);
    return true;
}
