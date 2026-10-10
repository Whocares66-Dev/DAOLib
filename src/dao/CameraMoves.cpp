// Smooth camera moves on the game's camera (dao/Camera.h): the part every plugin that
// moves the camera would otherwise write, and get wrong in the same places.
//
// - From where the camera is. Each frame reads the camera's yaw and offset as they
//   are, so a player who has turned or panned the camera since the plugin last moved
//   it is where the move starts from.
// - The short way round. The camera's yaw is not wrapped (it ran past 2 pi in play),
//   so a turn goes by the smaller angle to the target, whatever multiple of 2 pi
//   either is at.
// - The player wins. A pan stops when the game pans the camera for the player (its
//   own pan from the keys, a dragged mouse button or the cursor at the screen's edge,
//   hooked), and a turn when the camera's yaw is not where the turn left it the frame
//   before. Either leaves the camera theirs.
// - Why it stopped. OnMoveEnded and LastEnd say: there (2 cm, or a thousandth of a
//   turn), the player, the camera following something else, replaced, stopped, or no
//   camera.
//
// A pan does not take the game's own changes to the offset for the player's: until
// 2026-09-30 it did, stopping when the offset moved more than 5 cm between frames. In
// the tactical view the game pulls the focus back toward the party while no party
// member is in view (CameraController::PullSpeed), faster each frame, and a pan from a
// room the party could not be seen in was stopped by it within about six frames, twice
// at the start of the hero's turn in the turn-based mod. A pan now holds that pull at
// rest while it runs.
//
// A move eases out: quick at first, slowing into place, near enough there after
// `seconds`. The moves run in a frame callback of their own, called from the game's
// thread (a frame callback, a script event). One plugin's moves only: a second
// plugin's turn would look like the player's, and two pans would pull against each
// other.

#include "dao/CameraMoves.h"

#include "dao/Camera.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/core/Angle.h"
#include "dao/generated/ScriptApi.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

namespace dao::camera
{

namespace
{

using Clock = std::chrono::steady_clock;

// How far the camera may have turned between frames on its own before it
// counts as the player's doing: more than the orbit's dying spin after the
// mouse stops, less than a player's turn.
constexpr float kYawSlack = 0.01F; // radians
// Near enough to stop.
constexpr float kYawDone = 0.002F;
constexpr float kOffsetDone = 0.02F;

struct YawMove
{
    bool running = false;
    float target = 0;
    float tau = 0;
    std::optional<float> left; // the yaw the last step set
};
struct PanMove
{
    bool running = false;
    float tau = 0;
    ObjectId object = kInvalidObject; // what the camera is panning to
};
struct ZoomMove
{
    bool running = false;
    float target = 0; // on the controller's scale (dao/Camera.h, ZoomSetting)
    float tau = 0;
    std::optional<float> left; // the zoom the last step set
};
// How far the controller's zoom may move between frames on its own before
// it counts as the player's: less than a wheel notch (0.4 on its scale).
constexpr float kZoomSlack = 0.001F;
constexpr float kZoomDone = 0.0005F;
// Only the game's thread touches these: the moves' callers, the frame and
// the camera's own update.
YawMove g_yaw;
PanMove g_pan;
ZoomMove g_zoom;
std::optional<MoveEnd> g_yawEnd;
std::optional<MoveEnd> g_panEnd;
std::optional<MoveEnd> g_zoomEnd;
// Where the last zoom ease left the controller's zoom, kept after it ends.
std::optional<float> g_zoomLeft;

bool &Running(Move move)
{
    switch (move)
    {
    case Move::Yaw:
        return g_yaw.running;
    case Move::Pan:
        return g_pan.running;
    case Move::Zoom:
        break;
    }
    return g_zoom.running;
}

std::optional<MoveEnd> &EndOf(Move move)
{
    switch (move)
    {
    case Move::Yaw:
        return g_yawEnd;
    case Move::Pan:
        return g_panEnd;
    case Move::Zoom:
        break;
    }
    return g_zoomEnd;
}

constexpr Move kMoves[] = {Move::Yaw, Move::Pan, Move::Zoom};
bool g_playerPanned = false;
Clock::time_point g_last;

std::mutex g_endLock;
std::vector<std::function<void(Move, MoveEnd)>> g_ended;

void End(Move move, MoveEnd why)
{
    Running(move) = false;
    EndOf(move) = why;
    std::vector<std::function<void(Move, MoveEnd)>> callbacks;
    {
        const std::lock_guard lock(g_endLock);
        callbacks = g_ended;
    }
    for (const auto &callback : callbacks)
    {
        detail::Guard("a camera move callback", [&] { callback(move, why); });
    }
}

float Length(Vector v)
{
    return std::sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
}

// An ease-out: the share of what is left to cover this frame.
float Share(float dt, float tau)
{
    return tau > 0 ? 1 - std::exp(-dt / tau) : 1;
}

void StepYaw(CameraController &camera, float dt)
{
    SceneCamera *scene = camera.Scene();
    if (scene == nullptr)
    {
        End(Move::Yaw, MoveEnd::NoCamera);
        return;
    }
    const float now = scene->Yaw();
    if (g_yaw.left && std::fabs(now - *g_yaw.left) > kYawSlack)
    {
        End(Move::Yaw, MoveEnd::Player);
        return;
    }
    const float d = core::ShortestTurn(now, g_yaw.target);
    const bool there = std::fabs(d) < kYawDone;
    const float next = there ? now + d : now + (d * Share(dt, g_yaw.tau));
    scene->SetYaw(next);
    g_yaw.left = next;
    if (there)
    {
        End(Move::Yaw, MoveEnd::Arrived);
    }
}

void StepZoom(CameraController &camera, float dt)
{
    const float now = camera.ZoomSetting();
    if (g_zoom.left && std::fabs(now - *g_zoom.left) > kZoomSlack)
    {
        End(Move::Zoom, MoveEnd::Player);
        return;
    }
    const float d = g_zoom.target - now;
    const bool there = std::fabs(d) < kZoomDone;
    const float next = there ? g_zoom.target : now + (d * Share(dt, g_zoom.tau));
    camera.SetZoomSetting(next);
    g_zoom.left = next;
    g_zoomLeft = next;
    if (there)
    {
        End(Move::Zoom, MoveEnd::Arrived);
    }
}

void StepPan(CameraController &camera, float dt)
{
    if (std::exchange(g_playerPanned, false))
    {
        End(Move::Pan, MoveEnd::Player);
        return;
    }
    // The game's own pull back to the party would move the focus as well,
    // and faster each frame (dao/Camera.h, PullSpeed).
    camera.SetPullSpeed(0);
    const Vector now = camera.FocusOffset();
    const bool there = Length(now) < kOffsetDone;
    const float keep = there ? 0.0F : 1 - Share(dt, g_pan.tau);
    camera.SetFocusOffset({now.x * keep, now.y * keep, now.z * keep});
    if (there)
    {
        End(Move::Pan, MoveEnd::Arrived);
    }
}

// CameraControllerExplore::PlayerPan (dao/Addresses.h): the game panning
// the tactical camera for the player, with the input's direction; nothing
// to pan when both are 0.
struct PanInput
{
    std::int32_t x;
    std::int32_t y;
    bool keysOrEdge;
};
using PlayerPanFn = void(__fastcall *)(CameraController *self, int edx, float seconds, const PanInput *input,
                                       void *target, void *party);
Detour<PlayerPanFn> g_playerPan;

void __fastcall PlayerPan(CameraController *self, int edx, float seconds, const PanInput *input, void *target,
                          void *party)
{
    if (input != nullptr && (input->x != 0 || input->y != 0))
    {
        g_playerPanned = true;
    }
    g_playerPan.Original()(self, edx, seconds, input, target, party);
}

// CameraControllerBase::SetTarget (dao/Addresses.h): __thiscall (id),
// `ret 4`; the target it replaces at +0x08. It returns early for the id it
// already follows.
using SetTargetFn = void(__fastcall *)(CameraController *self, int edx, ObjectId id);
Detour<SetTargetFn> g_setTarget;
std::mutex g_targetLock;
std::vector<std::function<void(ObjectId, ObjectId)>> g_targetChanged;

void __fastcall SetTarget(CameraController *self, int edx, ObjectId id)
{
    const ObjectId before = self != nullptr ? self->Target() : kInvalidObject;
    g_setTarget.Original()(self, edx, id);
    if (self == nullptr || self->Target() == before)
    {
        return;
    }
    // Selecting the creature the pan goes to changes nothing here; another
    // takes the camera.
    if (g_pan.running && self->Target() != g_pan.object)
    {
        End(Move::Pan, MoveEnd::Retargeted);
    }
    std::vector<std::function<void(ObjectId, ObjectId)>> callbacks;
    {
        const std::lock_guard lock(g_targetLock);
        callbacks = g_targetChanged;
    }
    for (const auto &callback : callbacks)
    {
        detail::Guard("a camera target callback", [&] { callback(self->Target(), before); });
    }
}

void Step()
{
    const Clock::time_point now = Clock::now();
    const float dt = std::fmin(std::chrono::duration<float>(now - g_last).count(), 0.1F);
    g_last = now;
    if (!g_yaw.running && !g_pan.running && !g_zoom.running)
    {
        return;
    }
    CameraController *camera = Current();
    if (camera == nullptr)
    {
        for (const Move move : kMoves)
        {
            if (Running(move))
            {
                End(move, MoveEnd::NoCamera);
            }
        }
        return;
    }
    if (g_yaw.running)
    {
        StepYaw(*camera, dt);
    }
    if (g_pan.running)
    {
        StepPan(*camera, dt);
    }
    if (g_zoom.running)
    {
        StepZoom(*camera, dt);
    }
}

void Start()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_last = Clock::now();
        OnFrame(&Step);
    });
}

// Near enough there after `seconds`: four time constants leave 2%.
float Tau(float seconds)
{
    return std::fmax(seconds, 0.0F) / 4;
}

void AttachTargetHook()
{
    static std::once_flag once;
    std::call_once(once, [] { g_setTarget.Attach(addr::kCameraControllerBase_SetTarget, &SetTarget); });
}

void AttachPanHook()
{
    static std::once_flag once;
    std::call_once(once, [] { g_playerPan.Attach(addr::kCameraControllerExplore_PlayerPan, &PlayerPan); });
}

} // namespace

void OnTargetChanged(std::function<void(ObjectId now, ObjectId before)> callback)
{
    {
        const std::lock_guard lock(g_targetLock);
        g_targetChanged.push_back(std::move(callback));
    }
    AttachTargetHook();
}

CameraController *Current()
{
    auto *app = CClientExoApp::Get();
    auto *modes = app != nullptr ? app->GameModes() : nullptr;
    return modes != nullptr && modes->Explore() != nullptr ? modes->Camera() : nullptr;
}

void EaseYaw(float radians, float seconds)
{
    Start();
    if (g_yaw.running)
    {
        End(Move::Yaw, MoveEnd::Replaced);
    }
    g_yaw = YawMove{true, radians, Tau(seconds), std::nullopt};
    g_yawEnd.reset();
}

// Read 2026-10-02 (dao/Camera.h, CameraController::ZoomSetting). Seen in play the same
// day, the turn-based mod's play test 34: pulled back on each actor's turn by its size
// (an ogre, its victim and another in one frame), and the party's turns back at the
// player's own zoom.
void EaseZoom(float zoom, float seconds)
{
    Start();
    CameraController *camera = Current();
    if (camera == nullptr)
    {
        return;
    }
    if (g_zoom.running)
    {
        End(Move::Zoom, MoveEnd::Replaced);
    }
    // The scene camera's 0..1 onto the controller's scale, as the
    // controller's Zoom maps the other way (dao/Camera.h).
    const float least = camera->ZoomLeast();
    const float most = camera->ZoomMost();
    const float share = std::fmin(std::fmax(zoom, 0.0F), 1.0F);
    g_zoom = ZoomMove{true, least + (share * (most - least)), Tau(seconds), std::nullopt};
    g_zoomEnd.reset();
    g_zoomLeft.reset();
}

bool PlayerZoomed()
{
    if (g_zoomEnd == MoveEnd::Player)
    {
        return true;
    }
    const CameraController *camera = Current();
    return camera != nullptr && !g_zoom.running && g_zoomLeft &&
           std::fabs(camera->ZoomSetting() - *g_zoomLeft) > kZoomSlack;
}

// Selecting that same creature while the pan runs (the turn-based mod's order: select,
// then pan) does not stop it: the game's own retargeting is taken as where the pan goes
// on from, not as the player panning. Selecting another creature, or the player
// panning, still does (2026-09-30: a pan stopped by a selection in the same frame once
// left the view on the creature before).
//
// What "arrived" means for a pan: the controller's offset from what it follows has
// been eased to nothing, which is the whole of the move in the tactical view. Close in
// there is no offset to ease, the game's own pan carrying the camera over (0x007707D0,
// from OnTargetChanged), so the move ends arrived on its first frame while that pan is
// still under way and CameraController::Focus() is still between the two creatures (in
// play, 2026-09-30, the turn-based mod's: arrived after 25 ms with the focus 3.1 m from
// the creature). So to ask whether a pan fell short, read FocusOffset() or LastEnd, not
// the focus against the creature's position.
void PanTo(ObjectId object, float seconds)
{
    Start();
    CameraController *camera = Current();
    if (camera == nullptr)
    {
        return;
    }
    AttachTargetHook();
    AttachPanHook();
    if (g_pan.running)
    {
        End(Move::Pan, MoveEnd::Replaced);
    }
    camera->Follow(object);
    g_pan = PanMove{true, Tau(seconds), object};
    g_panEnd.reset();
    g_playerPanned = false;
}

// From the object's forward vector (GetOrientation): atan2(-y, -x), as seen in play
// (dao/Camera.h has the measurements).
std::optional<float> YawBehind(ObjectId object)
{
    const Vector forward = script::api::GetOrientation(object);
    if (forward.x == 0 && forward.y == 0)
    {
        return std::nullopt;
    }
    return std::atan2(-forward.y, -forward.x);
}

bool Moving()
{
    return g_yaw.running || g_pan.running || g_zoom.running;
}

void Stop()
{
    for (const Move move : kMoves)
    {
        if (Running(move))
        {
            End(move, MoveEnd::Stopped);
        }
    }
}

std::string_view Name(MoveEnd why)
{
    switch (why)
    {
    case MoveEnd::Arrived:
        return "arrived";
    case MoveEnd::Player:
        return "the player";
    case MoveEnd::Retargeted:
        return "retargeted";
    case MoveEnd::Replaced:
        return "replaced";
    case MoveEnd::Stopped:
        return "stopped";
    case MoveEnd::NoCamera:
        return "no camera";
    }
    return {};
}

void OnMoveEnded(std::function<void(Move move, MoveEnd why)> callback)
{
    const std::lock_guard lock(g_endLock);
    g_ended.push_back(std::move(callback));
}

std::optional<MoveEnd> LastEnd(Move move)
{
    return EndOf(move);
}

namespace
{
// The game's close distance, kept as it was before a plugin first set one.
std::optional<float> g_gameClose;
} // namespace

// Read 2026-10-04 (DAOLib #17, dao/Addresses.h "The camera's distances"): the orbit's
// radius each frame is zoom x close + (1 - zoom) x far, the two from globals the
// explore controller's update copies into the orbit every frame. So a larger close
// distance pulls the camera back at the same zoom, and the zoom's view flag and the
// tactical view are left alone. Nothing stops the camera at a wall or the ground (the
// orbit tests no collision). The game's own is kept at the first change.
void SetCloseDistance(std::optional<float> metres)
{
    auto *close = At<float>(addr::kCameraControllerExplore_CloseDistance);
    if (close == nullptr)
    {
        return;
    }
    if (!g_gameClose)
    {
        g_gameClose = *close;
    }
    *close = metres.value_or(*g_gameClose);
}

float CloseDistance()
{
    const float *close = At<float>(addr::kCameraControllerExplore_CloseDistance);
    return close != nullptr ? *close : 0.0F;
}

} // namespace dao::camera
