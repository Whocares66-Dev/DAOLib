// Example: the world's speed (dao/Clock.h).
//
// Fights run in slow motion: when the game enters combat mode the world
// runs at half speed, and at full speed again when the fight ends. The
// world's speed is the world timer's own scale, which every object's update
// takes its time from, so the AI, movement, effects and regeneration all
// slow together.
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

constexpr float kFightSpeed = 0.5F;

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
        dao::clock::SetSpeed(fighting ? kFightSpeed : 1.0F);
        spdlog::info("{}: the world at {:.2f} speed", fighting ? "a fight" : "the fight is over", dao::clock::Speed());
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
