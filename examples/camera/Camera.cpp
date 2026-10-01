// Example: moving the camera smoothly (dao/CameraMoves.h, dao/Camera.h).
//
// When the player selects another party member, the game's camera stays
// put in the tactical view and, close in, pans without turning. This
// plugin makes both a glide to the new member: in the tactical view the
// focus slides over with the camera's height and turn kept (PanTo), and
// close in the camera also comes round behind them (EaseYaw to
// YawBehind).
//
// The camera follows the selected member, so a change of what it follows
// is a change of selection (OnTargetChanged). The moves read the camera as
// it is each frame and stop if the player takes it mid-move, so the
// player's own turning and panning are never fought; each move's end, and
// why, is logged (OnMoveEnded). What it does goes to example_camera.log.
#include "dao/CameraMoves.h"
#include "dao/Log.h"
#include "dao/Plugin.h"

#include <spdlog/spdlog.h>

#include <cmath>

namespace
{

constexpr float kSeconds = 0.6F;

void Selected(dao::ObjectId now, dao::ObjectId /*before*/)
{
    const dao::CameraController *camera = dao::camera::Current();
    if (camera == nullptr)
    {
        return;
    }
    dao::camera::PanTo(now, kSeconds);
    const auto behind = dao::camera::YawBehind(now);
    if (!camera->Tactical() && behind)
    {
        dao::camera::EaseYaw(*behind, kSeconds);
    }
    spdlog::info("now following {:#010x}: {}", now, camera->Tactical() ? "gliding over" : "coming round behind");
}

void Ended(dao::camera::Move move, dao::camera::MoveEnd why)
{
    const dao::CameraController *camera = dao::camera::Current();
    const dao::Vector offset = camera != nullptr ? camera->FocusOffset() : dao::Vector{};
    spdlog::info("the {} ended: {}; the focus {:.2f} m from what it follows",
                 move == dao::camera::Move::Pan ? "pan" : "turn", dao::camera::Name(why),
                 std::hypot(offset.x, offset.y));
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_camera");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->expectedExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::camera::OnTargetChanged(&Selected);
    dao::camera::OnMoveEnded(&Ended);
    return true;
}
