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
// Not yet read: which view +0x90 names (the update sets it to 1 when the
// zoom is under 0.5 and to 0 otherwise, so it is the zoom's half), what
// the zoom measures (0x007EE7E0: a blend of +0x64 and +0x68 by the scene
// camera's +0x24), and where the yaw and pitch are (the scene camera's
// +0x20 is read as an angle in radians). The example plugin logs them as
// they change, to settle it in play. None of this is seen in play yet.
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <cstddef>
#include <cstdint>

namespace dao
{

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

    // The raw view flag and zoom (the header above says what is and is not
    // known of them).
    std::int32_t ViewFlag() const
    {
        return Read<std::int32_t>(kViewFlag);
    }
    float Zoom() const
    {
        using Fn = float(__fastcall *)(const CameraController *, int);
        return reinterpret_cast<Fn>(Module::Address(addr::kCameraControllerBase_Zoom))(this, 0);
    }

    // The scene's camera the controller drives.
    void *SceneCamera() const
    {
        return Read<void *>(kSceneCamera);
    }

    static constexpr std::size_t kSceneCamera = 0x04;
    static constexpr std::size_t kTarget = 0x08;
    static constexpr std::size_t kFocus = 0x70;
    static constexpr std::size_t kViewFlag = 0x90;

  private:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
};

} // namespace dao
