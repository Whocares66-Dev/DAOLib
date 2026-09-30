// The camera in explore and combat mode: what it follows, where it looks,
// and switching what it follows the way the game does, with its own pan.
//
//   auto *camera = app->GameModes()->Camera();
//   camera->Follow(enemy);   // pans there if within 15 m, else cuts
//
// Read 2026-09-30. The game mode controller's +0x60 is a
// CameraControllerExplore (vftable 0x00B1D954, built by 0x0076F890), whose
// base is CameraControllerBase (0x00B2456C, constructor 0x007EE9B0). The
// base keeps the scene's camera it drives at +0x04 and the id of the
// object it follows at +0x08. Every frame the explore controller's Update
// (slot 7, 0x007714B0, called with the frame's time) puts its focus, +0x70,
// on the followed object's position (the graphics object's +0x120, the
// object found from the id through the application, 0x007EE810) plus an
// offset: the follow is rigid, and a focus written from outside is
// overwritten the next frame. Its slot 8 snaps the camera to that focus at
// once (dao/Messages.h, CenterOnObjectMessage, sends there).
//
// The base's SetTarget (0x007EE840, __thiscall (id), `ret 4`) changes what
// is followed and tells the controller (its slot 0) the new object and the
// old. The explore controller then pans: when the two are within 15 m of
// each other (a float at 0x00C0E04C), it starts an interpolation from the
// old position to the new (0x0076EE00), faster the closer they are (0.6
// times one less the share of 15 m, at least 0.3: 0x00C0E044, 0x00C0E048);
// farther, it cuts. It does so only while its view flag at +0x90 is 0
// (below). Selecting a party member is what normally calls SetTarget;
// Follow does it for any object, an enemy too, without selecting it.
//
// The two views, read 2026-09-30 and matched to a session of zooming the
// same day. The scene camera (below) keeps a zoom from 1, close behind the
// creature, to 0, the tactical view high above; the controller's view
// flag at +0x90 is 1 in the tactical view (its Zoom, 0x007EE7E0, maps the
// scene camera's to -1..1, and the update sets the flag below 0.5). In the
// tactical view a change of target does not move the camera: the
// controller keeps the old focus's offset from the new target at +0x100
// (OnTargetChanged, slot 0), and the focus is the target's position plus
// that and +0xF0 (0x0076EBB0). Easing the offset to nothing pans to the
// target without turning the camera. In the close view it pans itself, as
// above.
//
// The scene camera, the controller's +0x04 (no RTTI), places itself about
// the focus in spherical coordinates each frame (0x006C2C20; the
// conversions 0x004D4990 and 0x004D4A40: x = r sin(pitch) cos(yaw),
// y = r sin(pitch) sin(yaw), z = r cos(pitch)). The yaw at +0x1C (and
// +0x18) is the direction from the focus to the camera, from +x toward +y,
// in radians, and the update keeps it: SetYaw (0x006C2B90, __thiscall
// (float), `ret 4`) turns the camera at once. The pitch at +0x20 is from
// straight above, about the camera's own pivot, which sits above the
// focus. The camera's own position is at +0x80.
//
// Seen in play, 2026-09-30, as the player zoomed, tilted and turned: the
// yaw equal to the direction from the focus to the camera's position
// within 0.02 in both views, running past 2 pi as the player turned (it
// is not wrapped), and kept through a change of view; the pitch 45 degrees
// in the tactical view whatever the turning, and 65 to 125 close in as the
// player tilted; the camera 3.8 m from the focus close in and 21.3 m in
// the tactical view. The turn-based mod, the same day: Follow and SetYaw
// put the camera behind the creature whose turn it was, the yaw set to
// radians(GetFacing) + pi (Eleanor facing 173 degrees, yaw 6.16; Aedan
// facing -176, yaw 0.08), and a screenshot showed the view from behind
// her; so the scripts' facing is measured from the same axis, the same
// way round, as the yaw. The offset's setter is not yet seen.
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <cstddef>
#include <cstdint>

namespace dao
{

// The scene's camera (the header above): where it is, the angles it keeps
// about the focus, and its zoom.
class SceneCamera
{
  public:
    SceneCamera() = delete;
    SceneCamera(const SceneCamera &) = delete;
    SceneCamera &operator=(const SceneCamera &) = delete;
    ~SceneCamera() = delete;

    // The direction from the focus to the camera, from +x toward +y, in
    // radians. A camera behind a creature has the yaw of its facing plus
    // pi: radians(GetFacing(creature)) + pi, seen in play.
    float Yaw() const
    {
        return Read<float>(kYaw);
    }
    // Turn the camera about its focus at once; ease by calling it each
    // frame. On the game's thread.
    void SetYaw(float radians)
    {
        using Fn = void(__fastcall *)(SceneCamera *, int, float);
        reinterpret_cast<Fn>(Module::Address(addr::kSceneCamera_SetYaw))(this, 0, radians);
    }
    // From straight above, in radians.
    float Pitch() const
    {
        return Read<float>(kPitch);
    }
    // 1 close behind the creature, 0 the tactical view.
    float Zoom() const
    {
        return Read<float>(kZoom);
    }
    Vector Position() const
    {
        return Read<Vector>(kPosition);
    }

    static constexpr std::size_t kYaw = 0x1C;
    static constexpr std::size_t kPitch = 0x20;
    static constexpr std::size_t kZoom = 0x24;
    static constexpr std::size_t kPosition = 0x80;

  private:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
};

class CameraController
{
  public:
    CameraController() = delete;
    CameraController(const CameraController &) = delete;
    CameraController &operator=(const CameraController &) = delete;
    ~CameraController() = delete;

    // The id of the object the camera follows: the selected party member,
    // unless a plugin said otherwise.
    ObjectId Target() const
    {
        return Read<ObjectId>(kTarget);
    }

    // Follow another object, as selecting a party member does: the camera
    // pans to it if it is within 15 m of the last, and cuts otherwise.
    // Nothing is selected. On the game's thread (a frame callback).
    void Follow(ObjectId object)
    {
        using Fn = void(__fastcall *)(CameraController *, int, ObjectId);
        reinterpret_cast<Fn>(Module::Address(addr::kCameraControllerBase_SetTarget))(this, 0, object);
    }

    // Where the camera looks: the followed object's position and the
    // controller's offset, as of the last update.
    Vector Focus() const
    {
        return Read<Vector>(kFocus);
    }

    // Put the camera on its focus at once, as CenterOnObjectMessage does.
    void SnapToTarget()
    {
        using Fn = void(__fastcall *)(CameraController *, int);
        reinterpret_cast<Fn>(Module::Address(addr::kCameraControllerExplore_SnapToTarget))(this, 0);
    }

    // The tactical view, high above, rather than the close one.
    bool Tactical() const
    {
        return Read<std::int32_t>(kViewFlag) == 1;
    }
    // The controller's zoom, -1 in the tactical view to 1 close in.
    float Zoom() const
    {
        using Fn = float(__fastcall *)(const CameraController *, int);
        return reinterpret_cast<Fn>(Module::Address(addr::kCameraControllerBase_Zoom))(this, 0);
    }

    // How far the focus is from the followed object: in the tactical view,
    // where it was left when the target changed. Setting it moves the
    // focus from the next frame; easing it to nothing pans to the target.
    Vector FocusOffset() const
    {
        const auto kept = Read<Vector>(kKeptOffset);
        const auto other = Read<Vector>(kOtherOffset);
        return {kept.x + other.x, kept.y + other.y, kept.z + other.z};
    }
    void SetFocusOffset(Vector offset)
    {
        Write(kKeptOffset, offset);
        Write(kOtherOffset, Vector{});
    }

    // The scene's camera the controller drives.
    SceneCamera *Scene() const
    {
        return Read<SceneCamera *>(kSceneCamera);
    }

    static constexpr std::size_t kSceneCamera = 0x04;
    static constexpr std::size_t kTarget = 0x08;
    static constexpr std::size_t kFocus = 0x70;
    static constexpr std::size_t kViewFlag = 0x90;
    static constexpr std::size_t kOtherOffset = 0xF0;
    static constexpr std::size_t kKeptOffset = 0x100;

  private:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
    template <class T> void Write(std::size_t offset, const T &value)
    {
        *reinterpret_cast<T *>(reinterpret_cast<std::byte *>(this) + offset) = value;
    }
};

} // namespace dao
