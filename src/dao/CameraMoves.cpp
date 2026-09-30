#include "dao/CameraMoves.h"

#include "dao/Camera.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/core/Angle.h"
#include "dao/generated/ScriptApi.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

namespace dao::camera
{

namespace
{

using Clock = std::chrono::steady_clock;

// How far the camera may have moved between frames on its own before it
// counts as the player's doing: more than the orbit's dying spin after the
// mouse stops, less than a player's turn or pan.
constexpr float kYawSlack = 0.01F;    // radians
constexpr float kOffsetSlack = 0.05F; // metres
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
    std::optional<Vector> left;
};
// Only the game's thread touches these: the moves' callers and the frame.
YawMove g_yaw;
PanMove g_pan;
Clock::time_point g_last;

float Length(Vector v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
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
        g_yaw.running = false;
        return;
    }
    const float now = scene->Yaw();
    if (g_yaw.left && std::fabs(now - *g_yaw.left) > kYawSlack)
    {
        g_yaw.running = false; // the player turned it
        return;
    }
    const float d = core::ShortestTurn(now, g_yaw.target);
    const float next = std::fabs(d) < kYawDone ? now + d : now + d * Share(dt, g_yaw.tau);
    scene->SetYaw(next);
    g_yaw.left = next;
    g_yaw.running = std::fabs(d) >= kYawDone;
}

void StepPan(CameraController &camera, float dt)
{
    const Vector now = camera.FocusOffset();
    if (g_pan.left && Length({now.x - g_pan.left->x, now.y - g_pan.left->y, now.z - g_pan.left->z}) > kOffsetSlack)
    {
        g_pan.running = false; // the player panned it
        return;
    }
    const float keep = Length(now) < kOffsetDone ? 0.0F : 1 - Share(dt, g_pan.tau);
    const Vector next{now.x * keep, now.y * keep, now.z * keep};
    camera.SetFocusOffset(next);
    g_pan.left = next;
    g_pan.running = keep > 0;
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
    if (!g_yaw.running && !g_pan.running)
    {
        return;
    }
    CameraController *camera = Current();
    if (camera == nullptr)
    {
        Stop();
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

} // namespace

void OnTargetChanged(std::function<void(ObjectId now, ObjectId before)> callback)
{
    const std::lock_guard lock(g_targetLock);
    g_targetChanged.push_back(std::move(callback));
    if (g_setTarget.Original() == nullptr)
    {
        g_setTarget.Attach(addr::kCameraControllerBase_SetTarget, &SetTarget);
    }
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
    g_yaw = YawMove{true, radians, Tau(seconds), std::nullopt};
}

void PanTo(ObjectId object, float seconds)
{
    Start();
    CameraController *camera = Current();
    if (camera == nullptr)
    {
        return;
    }
    camera->Follow(object);
    g_pan = PanMove{true, Tau(seconds), std::nullopt};
}

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
    return g_yaw.running || g_pan.running;
}

void Stop()
{
    g_yaw.running = false;
    g_pan.running = false;
}

} // namespace dao::camera
