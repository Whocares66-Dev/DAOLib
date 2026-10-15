// Example: moving the camera smoothly (dao/Camera.h).
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
//
// The field of view: a key the player binds ("DAOLib example: wide view", none by default) holds
// it at 100 degrees, another ("narrow view") at 30, and pressing the held one again gives back
// the game's. Each
// press logs the field of view before and after, to see it held through a
// conversation and from one area to the next, and given back exactly.
//
// The free view: another key ("DAOLib example: free view") takes the camera from where it is
// and turns it slowly in place, a sixth of a turn in ten seconds, until pressed again.
#include "dao/Camera.h"
#include "dao/Frame.h"
#include "dao/Input.h"
#include "dao/Log.h"
#include "dao/Plugin.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <optional>
#include <vector>

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

const char *MoveName(dao::camera::Move move)
{
    switch (move)
    {
    case dao::camera::Move::Pan:
        return "pan";
    case dao::camera::Move::Zoom:
        return "zoom";
    case dao::camera::Move::Yaw:
        return "turn";
    }
    return "move";
}

void Ended(dao::camera::Move move, dao::camera::MoveEnd why)
{
    const dao::CameraController *camera = dao::camera::Current();
    const dao::Vector offset = camera != nullptr ? camera->FocusOffset() : dao::Vector{};
    spdlog::info("the {} ended: {}; the focus {:.2f} m from what it follows", MoveName(move), dao::camera::Name(why),
                 std::hypot(offset.x, offset.y));
}

std::optional<float> g_held; // degrees

void Toggle(float degrees)
{
    const auto before = dao::camera::FieldOfView();
    g_held = g_held == degrees ? std::nullopt : std::optional(degrees);
    dao::camera::SetFieldOfView(g_held);
    const auto after = dao::camera::FieldOfView();
    spdlog::info("field of view {}: {:.1f} degrees before, {:.1f} after", g_held ? "held" : "given back",
                 before.value_or(-1.0F), after.value_or(-1.0F));
}

std::optional<dao::camera::View> g_free;

void ToggleFree()
{
    g_free = g_free ? std::nullopt : dao::camera::CurrentView();
    if (g_free && !dao::camera::SetFreeView(g_free))
    {
        spdlog::info("free view refused: the game is not exploring or fighting");
        g_free.reset();
        return;
    }
    if (!g_free)
    {
        dao::camera::SetFreeView(std::nullopt);
    }
    spdlog::info("free view {}", g_free ? "taken" : "given back");
}

void TurnFree()
{
    constexpr float kTurnPerFrame = 0.0035F; // radians: a sixth of a turn in ten seconds at 30 frames
    if (g_free)
    {
        g_free->yaw += kTurnPerFrame;
        if (!dao::camera::SetFreeView(g_free))
        {
            spdlog::info("free view ended by the game");
            g_free.reset();
        }
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_camera");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::camera::OnTargetChanged(&Selected);
    dao::camera::OnMoveEnded(&Ended);
    const std::vector<dao::GameMode> modes{dao::GameMode::Explore, dao::GameMode::Combat, dao::GameMode::Dialog};
    dao::input::RegisterAction(
        {.name = L"ExampleCameraWideView", .label = L"DAOLib example: wide view", .modes = modes},
        [] { Toggle(100.0F); });
    dao::input::RegisterAction(
        {.name = L"ExampleCameraNarrowView", .label = L"DAOLib example: narrow view", .modes = modes},
        [] { Toggle(30.0F); });
    dao::input::RegisterAction({.name = L"ExampleCameraFreeView",
                                .label = L"DAOLib example: free view",
                                .modes = {dao::GameMode::Explore, dao::GameMode::Combat, dao::GameMode::FlyCam}},
                               &ToggleFree);
    dao::OnFrame(&TurnFree);
    return true;
}
