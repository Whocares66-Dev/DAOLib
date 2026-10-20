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

#include "dao/Camera.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/InputDetail.h"
#include "dao/Module.h"
#include "dao/RenderCamera.h"
#include "dao/SafeRead.h"
#include "dao/World.h"
#include "dao/core/Angle.h"
#include "dao/generated/ScriptApi.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <numbers>
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
// tactical view are left alone. The orbit behaviour alone tests no collision, but in the
// close view the scene camera's manager (0x006C4C60) runs a pull-in after it (mode 1 ->
// 0x006C3710 at 0x006C4E1A: a 0.25 m sphere cast from the pivot, mask 0x1024), a
// release (0x006C35A0) and a near-camera fade (0x006C4130), so a wall or the ground
// pulls the camera in (corrected 2026-10-18, lib-agent-2; dev/CAMERA.md). The game's
// own is kept at the first change.
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

// The field of view the scene is drawn with (DAOLib #126, for DAOPhotoCam).
//
// Read 2026-10-15 in GOG's exe: Renderer::PerspectiveCamera (its vtable
// kPerspectiveCamera_Vftable) keeps its vertical field of view in radians at
// +0x140, an aspect's multiplier at +0x144, near and far at +0x130 and
// +0x134. Its slot 41 (0x00A25A70), __thiscall (fov, aspect multiplier,
// near, far), `ret 0x10`, writes all four and sets +0x30, and slot 34
// (0x00465AA0) remakes the projection (+0x40) from them through slot 40
// (0x00A25AD0) when +0x30 is set: 0x004D3A60 (out, fov, the view's aspect
// times +0x144, near, far), D3DXMatrixPerspectiveFovLH's arguments but not
// its matrix (below, DAOLib #204). Slot 33
// (0x00A259F0) answers +0x140. The constructor's default is pi/2. Slot 3
// (0x00A25A50), __thiscall (fov), `ret 4`, writes +0x140 alone and sets
// +0x30; no other code writes +0x140 but the constructor (0x00A25B30). A
// hold through slot 41 alone was not seen in play (2026-10-15, DAOTurnBased's
// test): the game sets the field of view again through slot 3 before each
// frame is drawn.
//
// So a held field of view is written to the scene's camera with +0x30 set,
// and slots 3 and 41 are detoured so that the game's own calls for that
// camera keep it: the game's latest value is noted, to give back when the
// hold ends. The
// detour runs on whatever thread sets the camera; it knows the scene's
// camera by the one the game thread last saw, as dao/World.cpp finds it.

namespace dao::camera
{

namespace
{

constexpr std::size_t kFieldOfView = 0x140; // radians, vertical
constexpr std::size_t kRemake = 0x30;       // the projection is to be made again
constexpr std::size_t kMatrix = 0x40;       // the projection, 16 floats
constexpr std::size_t kNearPlane = 0x130;
constexpr std::size_t kFarPlane = 0x134;
constexpr std::size_t kAspectMultiplier = 0x144;
constexpr std::size_t kSetFieldOfViewSlot = 3;
constexpr std::size_t kSetPerspectiveSlot = 41;

std::atomic<void *> g_camera{nullptr}; // the scene's camera, as the game thread last saw it
std::atomic<float> g_held{NAN};        // radians, NaN when nothing is held
std::atomic<float> g_gameValue{NAN};   // the game's own, while one is held
std::once_flag g_attachOnce;

// The field of view a game's call for the scene's camera sets: the held one while there is one.
float Wanted(void *camera, float fov)
{
    const float held = g_held.load();
    if (std::isnan(held) || camera != g_camera.load())
    {
        return fov;
    }
    g_gameValue.store(fov);
    return held;
}

using SetFieldOfViewFn = void(__fastcall *)(void *camera, int edx, float fov);
Detour<SetFieldOfViewFn> g_setFieldOfView;

void __fastcall SetCameraFieldOfView(void *camera, int edx, float fov)
{
    g_setFieldOfView.Original()(camera, edx, Wanted(camera, fov));
}

using SetPerspectiveFn = void(__fastcall *)(void *camera, int edx, float fov, float aspect, float nearPlane,
                                            float farPlane);
Detour<SetPerspectiveFn> g_setPerspective;

void __fastcall SetPerspective(void *camera, int edx, float fov, float aspect, float nearPlane, float farPlane)
{
    g_setPerspective.Original()(camera, edx, Wanted(camera, fov), aspect, nearPlane, farPlane);
}

void Write(void *camera, float radians)
{
    auto *fields = static_cast<std::byte *>(camera);
    *reinterpret_cast<float *>(fields + kFieldOfView) = radians;
    *reinterpret_cast<std::uint8_t *>(fields + kRemake) = 1;
}

// Each frame on the game thread: the scene's camera noted for the detour, and a held field of
// view kept on it, a camera the game made since among them.
void Keep()
{
    void *camera = detail::RenderCamera();
    void *const before = g_camera.exchange(camera);
    const float held = g_held.load();
    if (camera == nullptr || std::isnan(held))
    {
        return;
    }
    const auto now = TryRead<float>(reinterpret_cast<std::uintptr_t>(camera) + kFieldOfView);
    if (now && *now != held)
    {
        if (camera != before)
        {
            g_gameValue.store(*now);
        }
        Write(camera, held);
    }
}

void Attach()
{
    std::call_once(g_attachOnce, [] {
        const auto *const table = reinterpret_cast<void *const *>(Module::Address(addr::kPerspectiveCamera_Vftable));
        if (table != nullptr)
        {
            g_setFieldOfView.AttachTo(reinterpret_cast<SetFieldOfViewFn>(table[kSetFieldOfViewSlot]),
                                      &SetCameraFieldOfView);
            g_setPerspective.AttachTo(reinterpret_cast<SetPerspectiveFn>(table[kSetPerspectiveSlot]), &SetPerspective);
        }
        OnFrame(&Keep);
    });
}

constexpr float kDegree = std::numbers::pi_v<float> / 180.0F;

} // namespace

std::optional<float> FieldOfView()
{
    void *camera = detail::RenderCamera();
    const auto radians =
        camera != nullptr ? TryRead<float>(reinterpret_cast<std::uintptr_t>(camera) + kFieldOfView) : std::nullopt;
    return radians ? std::optional(*radians / kDegree) : std::nullopt;
}

// The projection (DAOLib #204, for Eluvian #156 and DAOPhotoCam #1): the
// fields #126 read, and the matrix slot 40 makes from them (0x00A25AD0 calls
// 0x004D3A60, read 2026-10-19: 1 / tan(fov / 2) at [5], that over the aspect
// at [0]). The aspect is the view's times +0x144, the view's from 0x00A25AA0
// (1 unless +0x148 is set), so it is taken from the matrix as [5] / [0]
// rather than worked out again.
//
// Seen in play 2026-10-19 by Eluvian's session (instance eluvian-test, 1600 x
// 1000, the Forest Clearing), the matrix in explore read row major as
// (1.082533 0 0 0 / 0 1.732052 0 0 / 0 0 -1.000037 -0.150006 / 0 0 -1 0):
// right-handed, depth 0 to 1, for column vectors, the transpose of
// D3DXMatrixPerspectiveFovRH, not the LH one its arguments suggested. The
// field of view 59.99995, aspect 1.6 (times 1000 is the width), far 4000 in
// every state; near 0.15 exploring, 0.01 in DAOPhotoCam's free view and
// 0.25 after it (the fly camera mode's entry passes 0.25 and 4000 to a
// renderer setter, 0x0076D5D9 to 0x0076D5F6; what sets 0.01 is not read).
// A field of view of 40 held in the free view read 40.
std::optional<Projection> CurrentProjection()
{
    void *camera = detail::RenderCamera();
    const auto size = world::ScreenSize();
    if (camera == nullptr || !size)
    {
        return std::nullopt;
    }
    const auto at = reinterpret_cast<std::uintptr_t>(camera);
    const auto fov = TryRead<float>(at + kFieldOfView);
    const auto nearPlane = TryRead<float>(at + kNearPlane);
    const auto farPlane = TryRead<float>(at + kFarPlane);
    const auto matrix = TryRead<std::array<float, 16>>(at + kMatrix);
    if (!fov || !nearPlane || !farPlane || !matrix || (*matrix)[0] == 0.0F)
    {
        return std::nullopt;
    }
    return Projection{.fieldOfView = *fov / kDegree,
                      .aspect = (*matrix)[5] / (*matrix)[0],
                      .nearPlane = *nearPlane,
                      .farPlane = *farPlane,
                      .width = size->width,
                      .height = size->height,
                      .matrix = *matrix};
}

void SetFieldOfView(std::optional<float> degrees)
{
    void *camera = detail::RenderCamera();
    if (degrees && (*degrees < 1.0F || *degrees > 179.0F || !std::isfinite(*degrees)))
    {
        return;
    }
    Attach();
    if (degrees)
    {
        if (std::isnan(g_held.load()) && camera != nullptr)
        {
            g_gameValue.store(TryRead<float>(reinterpret_cast<std::uintptr_t>(camera) + kFieldOfView).value_or(NAN));
        }
        g_camera.store(camera);
        g_held.store(*degrees * kDegree);
        if (camera != nullptr)
        {
            Write(camera, *degrees * kDegree);
        }
        return;
    }
    g_held.store(NAN);
    const float game = g_gameValue.exchange(NAN);
    if (camera != nullptr && !std::isnan(game))
    {
        Write(camera, game);
    }
}

} // namespace dao::camera

namespace dao::camera
{

namespace
{

// The scene camera every mode drives, at a GameMode's +0x58 (dao/Addresses.h, the fly camera):
// its place at +0x70, its orientation at +0x90 (x, y, z, w).
constexpr std::size_t kModeSceneCamera = 0x58;
constexpr std::size_t kEye = 0x70;
constexpr std::size_t kOrientation = 0x90;
constexpr float kDegrees = 180.0F / std::numbers::pi_v<float>;

GameMode g_before = GameMode::Explore;
bool g_free = false;
bool g_flying = false; // the fly camera's mode seen since the free view began: it comes a frame later

// The scene camera's near and far planes when a free view began, given back by the fly camera
// mode's exit, after its own (DAOLib #205; why beside SetFreeView). Empty when no free view of
// the plugin's is in the mode, so the game's own fly camera ends as in vanilla.
struct Planes
{
    float nearPlane = 0.0F;
    float farPlane = 0.0F;
};
std::optional<Planes> g_planes;
std::once_flag g_exitOnce;
constexpr std::size_t kModeExitSlot = 7;

std::optional<Planes> PlanesNow()
{
    void *camera = detail::RenderCamera();
    if (camera == nullptr)
    {
        return std::nullopt;
    }
    const auto at = reinterpret_cast<std::uintptr_t>(camera);
    const auto nearPlane = TryRead<float>(at + kNearPlane);
    const auto farPlane = TryRead<float>(at + kFarPlane);
    return nearPlane && farPlane ? std::optional(Planes{*nearPlane, *farPlane}) : std::nullopt;
}

void GivePlanesBack(const Planes &planes)
{
    void *camera = detail::RenderCamera();
    const auto *const table = reinterpret_cast<void *const *>(Module::Address(addr::kPerspectiveCamera_Vftable));
    if (camera == nullptr || table == nullptr)
    {
        return;
    }
    const auto at = reinterpret_cast<std::uintptr_t>(camera);
    const auto fov = TryRead<float>(at + kFieldOfView);
    const auto multiplier = TryRead<float>(at + kAspectMultiplier);
    if (fov && multiplier)
    {
        reinterpret_cast<SetPerspectiveFn>(table[kSetPerspectiveSlot])(camera, 0, *fov, *multiplier, planes.nearPlane,
                                                                       planes.farPlane);
    }
}

// The exit returns what the base GameMode's (0x006945A0) left in eax, which is passed on as is.
using ModeExitFn = int(__fastcall *)(void *mode, int edx, void *next);
Detour<ModeExitFn> g_flyCamExit;

int __fastcall FlyCamExit(void *mode, int edx, void *next)
{
    const int result = g_flyCamExit.Original()(mode, edx, next);
    if (g_planes)
    {
        GivePlanesBack(*g_planes);
        g_planes.reset();
    }
    return result;
}

GameModeController *Modes()
{
    auto *app = CClientExoApp::Get();
    return app != nullptr ? app->GameModes() : nullptr;
}

void AttachFlyCamExit(const GameModeController *modes)
{
    std::call_once(g_exitOnce, [modes] {
        auto *mode = modes->ModeObject(GameMode::FlyCam);
        const auto vtable =
            mode != nullptr ? TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(mode)) : std::nullopt;
        const auto exit = vtable ? TryRead<std::uintptr_t>(*vtable + (kModeExitSlot * sizeof(void *))) : std::nullopt;
        if (exit && *exit != 0)
        {
            g_flyCamExit.AttachTo(reinterpret_cast<ModeExitFn>(*exit), &FlyCamExit);
        }
    });
}

// Whether a free view went on, or ended without the plugin: a mode other than the fly camera's
// after the game was seen in it, as a script's SetGameMode does (DAOPhotoCam's test, a fight).
bool StillFree(const GameModeController *modes)
{
    if (modes->Mode() == GameMode::FlyCam)
    {
        g_flying = true;
        return true;
    }
    return !g_flying;
}

void EndFree()
{
    input::detail::WithholdFlyCamera(false);
    g_free = false;
    g_flying = false;
}

std::uintptr_t SceneCameraOf(const GameModeController *modes)
{
    const auto mode =
        reinterpret_cast<std::uintptr_t>(modes != nullptr ? modes->ModeObject(GameMode::FlyCam) : nullptr);
    return mode != 0 ? TryRead<std::uintptr_t>(mode + kModeSceneCamera).value_or(0) : 0;
}

void Place(std::uintptr_t camera, const View &view)
{
    alignas(16) std::array<float, 4> eye{view.eye.x, view.eye.y, view.eye.z, 1.0F};
    alignas(16) std::array<float, 4> turn{};
    using FromFn = void(__cdecl *)(float *, float, float, float);
    reinterpret_cast<FromFn>(Module::Address(addr::kQuaternion_FromYawPitchRoll))(turn.data(), view.yaw * kDegrees,
                                                                                  view.pitch * kDegrees, 0.0F);
    using SetFn = void(__fastcall *)(void *, int, const float *, float, std::int32_t);
    auto *self = reinterpret_cast<void *>(camera);
    reinterpret_cast<SetFn>(Module::Address(addr::kSceneCamera_SetPosition))(self, 0, eye.data(), 0.0F, 0);
    reinterpret_cast<SetFn>(Module::Address(addr::kSceneCamera_SetOrientation))(self, 0, turn.data(), 0.0F, 0);
}

} // namespace

// Read 2026-10-15 for DAOLib #125 (dao/Addresses.h, the fly camera): the game's fly camera mode
// moves the scene camera by its own keys (inputs 28 to 42) through SetPosition and
// SetOrientation, its yaw and pitch made into the orientation in degrees. Here the mode is
// entered, its keys withheld (dao/InputDetail.h), and the camera placed by the plugin's view.
//
// Seen in play 2026-10-15 (DAOPhotoCam's session, GOG's game, the Forest Clearing): the mode
// went to 6 a frame after the first call, and the picture was drawn from the view, yaw 0 along
// +y and -90 along +x, pitch 30 the sky; CurrentView on entering matched the close camera's
// eye, yaw -90 and pitch -5; placed each frame the eye moved smoothly and held poses stayed
// put; leaving gave back explore a frame later with its camera exactly as before; from combat
// the same, until a script's SetGameMode took the game from the fly camera to explore and on to
// combat, ending the free view; the pick and ToScreen followed the drawn view; nothing near the
// eye hid, faded or was clipped at 0.15 m.
//
// The near plane (DAOLib #205; read 2026-10-19 in GOG's exe, after Eluvian's session saw it 0.15
// exploring, 0.01 in a free view and 0.25 after): the area's scene camera is made with near 0.15
// and far 4000 (0x0040AE60, from the area's load at 0x006892A7). The fly camera mode's entry
// (slot 6, 0x0076D4A0) sets 0.01 and 4000, and its exit (slot 7, 0x0076D560) 0.25 and 4000,
// fixed values, not those it found: so the game's own fly camera leaves 0.25 standing until the
// next area's camera. Slot 6 is a mode's entry and 7 its exit (GameModeGUI's 0x007187D0 and
// 0x007188C0). A free view gives the camera back as it was, so the mode's exit is detoured
// (slot 7, found through the mode object's vtable) and, after its own, sets the planes found
// when the free view began through the camera's slot 41, whether the plugin or a script ended
// the mode. A first try gave them back a frame after the mode had gone: right when the plugin
// ended it, but a script's SetGameMode had the exit write its 0.25 after (Eluvian's play check,
// 2026-10-19).
std::optional<View> CurrentView()
{
    const std::uintptr_t camera = SceneCameraOf(Modes());
    const auto eye = camera != 0 ? TryRead<Vector>(camera + kEye) : std::nullopt;
    if (!eye)
    {
        return std::nullopt;
    }
    using AngleFn = float(__cdecl *)(const void *);
    const auto *turn = reinterpret_cast<const void *>(camera + kOrientation);
    const float yaw = reinterpret_cast<AngleFn>(Module::Address(addr::kQuaternion_Yaw))(turn);
    const float pitch = reinterpret_cast<AngleFn>(Module::Address(addr::kQuaternion_Pitch))(turn);
    return View{*eye, yaw / kDegrees, pitch / kDegrees};
}

bool SetFreeView(std::optional<View> view)
{
    GameModeController *modes = Modes();
    const std::uintptr_t camera = SceneCameraOf(modes);
    if (!view)
    {
        if (g_free && modes != nullptr && StillFree(modes))
        {
            modes->SetMode(g_before);
        }
        EndFree();
        return true;
    }
    if (camera == 0)
    {
        return false;
    }
    if (g_free && !StillFree(modes))
    {
        EndFree();
        return false;
    }
    if (!g_free)
    {
        const GameMode now = modes->Mode();
        if (now != GameMode::Explore && now != GameMode::Combat)
        {
            return false;
        }
        g_before = now;
        AttachFlyCamExit(modes);
        g_planes = PlanesNow();
        input::detail::WithholdFlyCamera(true);
        modes->SetMode(GameMode::FlyCam);
        g_free = true;
    }
    Place(camera, *view);
    return true;
}

} // namespace dao::camera
