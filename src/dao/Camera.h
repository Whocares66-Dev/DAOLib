// The camera in explore and combat mode: read where it is and what it looks at, make it
// follow any object, turn, tilt and zoom it, move it smoothly, and hold its field of view.
//
//   auto *camera = app->GameModes()->Camera();
//   camera->Follow(enemy);   // pans there if within 15 m, otherwise cuts
//
// Smooth moves (dao::camera): turning, panning to a creature and zooming, each eased over a
// given time. Every move starts from where the camera is now, turns the short way round, and
// stops as soon as the player turns, pans or zooms the camera. Starting a move replaces any
// running move of the same kind. Only one plugin should move the camera, because to each
// plugin another plugin's moves look like the player's. Call these on the game thread.
//
//   dao::camera::PanTo(creature, 0.6f);                    // tactical view: glide, no turning
//   if (const auto behind = dao::camera::YawBehind(creature))
//       dao::camera::EaseYaw(*behind, 0.6f);               // close view: come round behind it
//   dao::camera::EaseZoom(0.7f, 0.8f);                     // pull back a little

// Notes on the camera, read 2026-09-30. dev/CAMERA.md has the explore camera as
// the engine runs it (read 2026-10-18): the zooms, distance, pitch, controls, the
// reset on a load and the close view's pull-in. The game mode controller's +0x60 is a
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
// the tactical view. The turn-based mod, the same day, used Follow and
// SetYaw to bring the camera round behind the creature whose turn it was.
// The scripts' facing is not the yaw's measure: GetOrientation's forward
// vector against GetFacing, for three creatures, gave facing = -(atan2(fy,
// fx) in degrees) - 90 ((-0.40, 0.92) facing 156; (-0.39, -0.92) facing
// 23; (0.34, -0.94) facing -20). So behind a creature is the yaw
// atan2(-fy, -fx), or radians(90 - facing). A first try with
// radians(facing) + pi left the camera at the creature's side, though one
// screenshot, everyone facing about 180, had seemed to agree. The offset's
// setter is not yet seen.
//
// One yaw, read 2026-09-30 for a camera that did not stay behind a
// creature once the player had turned it. The scene camera's manager
// (0x006C4C60) runs a list of camera behaviours each frame; the orbit
// (type 2, the update above) is the one that places it. It keeps one yaw,
// +0x1C, mirrored at +0x18 and in the orbit behaviour's +0x34, and
// SetYaw writes all three. The player's turning is not a second yaw: the
// controller puts the mouse's rate in +0x44 each frame, the manager
// clears it after, and the orbit turns it into a yaw speed (the
// behaviour's +0x80) that dies away within a quarter of a second once the
// mouse stops. The update has a second mode, a byte at +0x00 armed by a
// timer at +0x04, in which the yaw is taken from where the camera stands
// and SetYaw's yaw is ignored; the constructor sets the timer to -1, and
// nothing found sets it again, so it should not arise. Nothing else was
// found turning the camera but SetYaw, and the controller's reset
// (TurnBehind, below).
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace dao
{

// The game's scene camera, which the CameraController drives: its position, its angles
// around the point it looks at (the focus), and its zoom. Get it from
// CameraController::Scene.
class SceneCamera
{
  public:
    SceneCamera() = delete;
    SceneCamera(const SceneCamera &) = delete;
    SceneCamera &operator=(const SceneCamera &) = delete;
    ~SceneCamera() = delete;

    // Returns the direction from the focus to the camera, in radians, measured from +x
    // toward +y. The value is not wrapped, so it can grow past 2 pi as the player turns.
    //
    // To put the camera behind a creature, use atan2(-fy, -fx) of the creature's forward
    // vector (GetOrientation), or radians(90 - GetFacing). radians(GetFacing) + pi does
    // not work.
    float Yaw() const
    {
        return Read<float>(kYaw);
    }
    // Turns the camera around its focus immediately. To ease a turn, call it every frame,
    // or use dao::camera::EaseYaw.
    // @param radians the new yaw, measured as Yaw is
    //
    // Call on the game thread.
    void SetYaw(float radians)
    {
        using Fn = void(__fastcall *)(SceneCamera *, int, float);
        reinterpret_cast<Fn>(Module::Address(addr::kSceneCamera_SetYaw))(this, 0, radians);
    }
    // Returns the camera's tilt in radians, measured from straight above.
    float Pitch() const
    {
        return Read<float>(kPitch);
    }

    // Notes on SetPitch: it does what the engine's own SetPitch does, writing the
    // orbit's kept pitch (+0x20 and +0x50). The close view's clamp is the explore
    // controller's (0x0076F1B0, its globals at 0x00C0E050 and 0x00C0E054). Not yet
    // seen in play (2026-10-04).

    // Tilts the camera immediately. The tilt holds until the player tilts the camera.
    // @param radians the angle from straight above; smaller values look down more steeply
    //
    // In the close view (Zoom 0.75 and above), the game clamps the tilt to between 30 and 150
    // degrees every frame. Read the player's pitch first and restore it afterwards. Call
    // on the game thread. Not yet tested in the game.
    void SetPitch(float radians)
    {
        using Fn = void(__fastcall *)(SceneCamera *, int, float);
        reinterpret_cast<Fn>(Module::Address(addr::kSceneCamera_SetPitch))(this, 0, radians);
    }
    // Notes on Zoom: the controller's view flag, +0x90, is set while its Zoom() = 2s - 1
    // is below 0.5 (0x0076FBF4, 0x0076FC92-0x0076FCAD), so on this 0..1 scale the
    // tactical view is below 0.75, not 0.5 (corrected 2026-10-18 by lib-agent-2;
    // dev/CAMERA.md).

    // Returns the zoom: 1 is close behind the creature, 0 is the tactical view high above.
    // Below 0.75 the camera is in the tactical view.
    float Zoom() const
    {
        return Read<float>(kZoom);
    }
    // Returns the camera's position in the world.
    Vector Position() const
    {
        return Read<Vector>(kPosition);
    }

    static constexpr std::size_t kYaw = 0x1C;      // offset of the yaw
    static constexpr std::size_t kPitch = 0x20;    // offset of the pitch
    static constexpr std::size_t kZoom = 0x24;     // offset of the zoom
    static constexpr std::size_t kPosition = 0x80; // offset of the position

  private:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
};

// The controller of the camera in explore and combat mode: what the camera follows, where
// it looks, and the zoom the player has set. Get it from GameModes::Camera.
class CameraController
{
  public:
    CameraController() = delete;
    CameraController(const CameraController &) = delete;
    CameraController &operator=(const CameraController &) = delete;
    ~CameraController() = delete;

    // Returns the id of the object the camera follows: the selected party member,
    // unless a plugin has called Follow.
    ObjectId Target() const
    {
        return Read<ObjectId>(kTarget);
    }

    // Makes the camera follow an object, as selecting a party member does, but without
    // selecting it. Any object works, including an enemy.
    //
    // In the close view, the camera pans to the object if it is within 15 m of the previous
    // one, and cuts to it otherwise. In the tactical view the camera stays where it is (see
    // SetFocusOffset). Call on the game thread, for example from a frame callback.
    void Follow(ObjectId object)
    {
        using Fn = void(__fastcall *)(CameraController *, int, ObjectId);
        reinterpret_cast<Fn>(Module::Address(addr::kCameraControllerBase_SetTarget))(this, 0, object);
    }

    // Returns the point the camera looked at in the last update: the followed object's
    // position plus the controller's offset.
    //
    // This is not where the camera is heading: while the game pans between creatures it is
    // a point between them, and after SetFocusOffset it lags a frame behind.
    Vector Focus() const
    {
        return Read<Vector>(kFocus);
    }

    // Moves the camera onto its focus immediately, as CenterOnObjectMessage does.
    void SnapToTarget()
    {
        using Fn = void(__fastcall *)(CameraController *, int);
        reinterpret_cast<Fn>(Module::Address(addr::kCameraControllerExplore_SnapToTarget))(this, 0);
    }

    // Notes on TurnBehind: it is what the controller's reset does for the object it
    // follows (its slot 9). The yaw is the object's heading, read from its orientation
    // (the graphics object's +0x130), plus 270 degrees, normalised. Not yet seen in
    // play.

    // Turns the camera immediately to look along an object's heading, putting it behind
    // the object, the same way the game resets the camera. Not yet tested in the game.
    void TurnBehind(ObjectId object)
    {
        using Fn = void(__fastcall *)(CameraController *, int, ObjectId);
        reinterpret_cast<Fn>(Module::Address(addr::kCameraControllerBase_TurnBehind))(this, 0, object);
    }

    // Returns true in the tactical view, high above, and false in the close view.
    bool Tactical() const
    {
        return Read<std::int32_t>(kViewFlag) == 1;
    }
    // Returns the scene camera's zoom mapped to -1 (the tactical view) to 1 (close in).
    float Zoom() const
    {
        using Fn = float(__fastcall *)(const CameraController *, int);
        return reinterpret_cast<Fn>(Module::Address(addr::kCameraControllerBase_Zoom))(this, 0);
    }

    // Notes on the zoom setting, read 2026-10-02: +0x48, from its least (+0x64, -1, the
    // tactical view) to its most (+0x68, 1, close in), which the mouse wheel moves
    // (each notch times +0x4C, 0.4, added through 0x007EDBD0). The controller's update
    // (0x007EEB70) maps it to the scene camera's 0..1 (0x007EE770) and hands it to
    // SceneCamera::SetZoom (0x006C2BF0), toward which the scene camera eases its own
    // zoom (at most 2 a second, quicker as it goes), and below 0.75 there it is the
    // tactical view (0.5 on the controller's -1..1; corrected 2026-10-18, lib-agent-2).

    // Returns the zoom the player has set with the mouse wheel, from ZoomLeast (-1, the
    // tactical view) to ZoomMost (1, close in). The scene camera eases toward it.
    float ZoomSetting() const
    {
        return Read<float>(kZoomSetting);
    }
    // Sets the zoom as the mouse wheel does; the camera then eases toward it.
    // @param zoom from ZoomLeast to ZoomMost
    //
    // To ease the setting itself over time, use dao::camera::EaseZoom.
    void SetZoomSetting(float zoom)
    {
        Write(kZoomSetting, zoom);
    }
    // Returns the least zoom setting, the tactical view (-1).
    float ZoomLeast() const
    {
        return Read<float>(kZoomLeast);
    }
    // Returns the greatest zoom setting, close in (1).
    float ZoomMost() const
    {
        return Read<float>(kZoomMost);
    }

    // Returns the focus's offset from the followed object. In the tactical view, this
    // keeps the focus where it was when the target changed.
    Vector FocusOffset() const
    {
        const auto kept = Read<Vector>(kKeptOffset);
        const auto other = Read<Vector>(kOtherOffset);
        return {kept.x + other.x, kept.y + other.y, kept.z + other.z};
    }
    // Sets the focus's offset from the followed object; the focus moves on the next frame.
    // Easing the offset to zero pans to the target without turning the camera.
    void SetFocusOffset(Vector offset)
    {
        Write(kKeptOffset, offset);
        Write(kOtherOffset, Vector{});
    }

    // Notes on the pull: dao/Addresses.h, PullToParty.

    // Returns the speed, in metres per second, at which the tactical view pulls the focus
    // toward the nearest party member while neither that member nor the followed object is
    // on screen. The speed grows by 30 each second until one of them is; it is 0 when
    // nothing is pulling.
    float PullSpeed() const
    {
        return Read<float>(kPullSpeed);
    }
    // Sets the tactical view's pull speed. Setting it to 0 restarts the pull from rest.
    // @param metresPerSecond the speed, in metres per second
    void SetPullSpeed(float metresPerSecond)
    {
        Write(kPullSpeed, metresPerSecond);
    }

    // Returns the scene camera that the controller drives.
    SceneCamera *Scene() const
    {
        return Read<SceneCamera *>(kSceneCamera);
    }

    static constexpr std::size_t kSceneCamera = 0x04; // offset of the scene camera
    static constexpr std::size_t kTarget = 0x08;      // offset of the followed object's id
    static constexpr std::size_t kZoomSetting = 0x48; // offset of the zoom setting
    static constexpr std::size_t kZoomLeast = 0x64;   // offset of the least zoom setting
    static constexpr std::size_t kZoomMost = 0x68;    // offset of the greatest zoom setting
    static constexpr std::size_t kFocus = 0x70;       // offset of the focus
    static constexpr std::size_t kViewFlag = 0x90;    // offset of the view flag, 1 when tactical
    static constexpr std::size_t kPullSpeed = 0xEC;   // offset of the pull speed
    static constexpr std::size_t kOtherOffset = 0xF0; // offset of the focus's second offset
    static constexpr std::size_t kKeptOffset = 0x100; // offset of the focus's kept offset

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

namespace dao::camera
{

// Returns the camera controller in explore or combat mode, the only modes in which the camera
// moves.
// @return null in any other mode (a menu, a conversation, a cutscene, loading) or before the
// game has started
CameraController *Current();

// Registers a callback for when the camera starts following a different object: the player
// selecting another party member, or a plugin calling Follow or PanTo.
// @param callback called with the object followed now and the one followed before
//
// The callback runs on the game thread after the game has handled the change: in the
// tactical view the camera has stayed where it was, and in the close view the game's pan has
// begun. If the callback makes the camera follow something else, it is called again for that
// change.
void OnTargetChanged(std::function<void(ObjectId now, ObjectId before)> callback);

// Turns the camera to a yaw, in the close view.
// @param radians the yaw to turn to, measured as SceneCamera::Yaw is
// @param seconds how long the turn takes to all but arrive; it starts quickly and slows into
// place
void EaseYaw(float radians, float seconds);

// Zooms the camera, as the mouse wheel does.
// @param zoom the zoom to reach, on SceneCamera::Zoom's scale from 1 (close behind the
// creature) to 0 (the tactical view high above); clamped to 0..1
// @param seconds how long the zoom takes to all but arrive
//
// Below 0.75 the game switches to the tactical view, where the camera does not follow a change
// of target, so stay at 0.75 or above to remain in the close view. Does nothing if there is no
// camera (see Current).
void EaseZoom(float zoom, float seconds);
// Returns true if the player has zoomed since the last EaseZoom, either during it or after it
// arrived. Returns false before the first EaseZoom.
bool PlayerZoomed();

// Makes the camera follow an object and glides the focus onto it.
// @param object the object to follow, usually a creature
// @param seconds how long the glide takes to all but arrive
//
// In the tactical view the camera only slides, keeping its height and yaw. In the close view
// the game itself pans to an object within 15 m and cuts to one farther away, so the move
// reports Arrived on its first frame while the game's own pan is still running. Selecting the
// same creature during the pan does not stop it; selecting another does. To tell whether a
// pan fell short, check CameraController::FocusOffset or LastEnd, not Focus. Does nothing if
// there is no camera (see Current).
void PanTo(ObjectId object, float seconds);

// Returns the yaw that puts the camera behind an object, for EaseYaw.
// @return nullopt if the object has no orientation
std::optional<float> YawBehind(ObjectId object);

// Returns true while any move is running.
bool Moving();
// Stops every running move where it is; each one ends with MoveEnd::Stopped.
void Stop();

// A kind of camera move.
enum class Move
{
    Yaw,  // EaseYaw
    Pan,  // PanTo
    Zoom, // EaseZoom
};

// Why a move ended.
enum class MoveEnd
{
    Arrived,    // it got close enough to its target
    Player,     // the player panned, turned or zoomed the camera
    Retargeted, // the camera started following something else
    Replaced,   // a new move of the same kind replaced it
    Stopped,    // Stop was called
    NoCamera,   // the camera is gone: a menu, a conversation, a cutscene or loading
};
// Returns a short lower-case name for a reason, for logs, such as "arrived" or "the player".
std::string_view Name(MoveEnd why);

// Registers a callback for when a move ends.
// @param callback called with the kind of move and why it ended
//
// The callback runs on the game thread after the move has released the camera, so it may
// start another move.
void OnMoveEnded(std::function<void(Move move, MoveEnd why)> callback);

// Sets the camera's distance from its pivot at full zoom (1); the game's default is 3.0. A
// larger distance pulls the camera back at the same zoom without switching to the tactical
// view, which helps frame a creature too big for the default.
// @param metres the distance, or nullopt to restore the game's own
//
// In the close view the game pulls the camera in toward its pivot when a wall or the ground
// is in the way. Call on the game thread.
void SetCloseDistance(std::optional<float> metres);
// Returns the camera's distance from its pivot at full zoom, in metres, or 0 if the running
// build of the game does not support it.
float CloseDistance();

// Returns why the last move of a kind ended.
// @return nullopt while a move of that kind is running, or before the first one
std::optional<MoveEnd> LastEnd(Move move);

// Returns the field of view the scene is drawn with: the vertical angle, top to bottom, in
// degrees. The horizontal one follows from the window's shape.
// @return nullopt with no scene drawn, or off the game thread
std::optional<float> FieldOfView();

// Holds the field of view the scene is drawn with, until it is given back.
// @param degrees the vertical angle, top to bottom, from 1 to 179; nullopt gives back the game's
//
// It holds through the game's own changes to it, conversations' and cutscenes' among them, and
// from one area to the next. Call on the game thread.
void SetFieldOfView(std::optional<float> degrees);

// A view of the world: where the eye is and which way it looks.
struct View
{
    Vector eye; // in the world, in metres
    // The heading, in radians: 0 looks along +y, -pi/2 along +x, so x = -sin(yaw) and
    // y = cos(yaw) are the way the view faces on the ground.
    float yaw = 0.0F;
    float pitch = 0.0F; // the tilt, in radians, from -pi/2 to pi/2, up positive
};

// Returns the view the scene is drawn from now: where a free view starts.
// @return nullopt with no scene camera
//
// Call on the game thread.
std::optional<View> CurrentView();

// Draws the scene from a view of the plugin's own, until given back: the game's fly camera, the
// camera loose from the party, placed where the view says each time this is called.
// @param view the view, or nullopt to give the camera back to the mode the game was in
// @return false, with nothing changed, if the game is not exploring or fighting when a free view
//  begins, or has no camera; false too once the game has left the free view on its own
//
// Call it each frame from a frame callback while the view holds. The game's mode becomes
// GameMode::FlyCam a frame after the first call, and its own fly camera keys are withheld; a
// plugin moves the view itself, for example from dao::input's readings. The world's pick and
// dao::world::ToScreen follow the drawn view. A script that changes the game's mode ends the
// free view: the game goes to that mode, and the next call returns false. Given back, the game's
// camera returns to where it was. Call on the game thread.
bool SetFreeView(std::optional<View> view);

} // namespace dao::camera
