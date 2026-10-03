// Moving the camera smoothly, the player's own turning and panning taken
// into account: the part every plugin that moves the camera would
// otherwise write, and get wrong in the same places (dao/Camera.h has the
// camera itself).
//
//   dao::camera::PanTo(creature, 0.6f);                    // tactical: glide there, no turning
//   if (const auto behind = dao::camera::YawBehind(creature))
//       dao::camera::EaseYaw(*behind, 0.6f);               // close in: come round behind it
//   dao::camera::EaseZoom(0.7f, 0.8f);                     // pull back a little
//
// - From where the camera is. Each frame reads the camera's yaw and
//   offset as they are, so a player who has turned or panned the camera
//   since the plugin last moved it is where the move starts from.
// - The short way round. The camera's yaw is not wrapped (it ran past 2
//   pi in play), so a turn goes by the smaller angle to the target,
//   whatever multiple of 2 pi either is at.
// - The player wins. A pan stops when the game pans the camera for the
//   player (its own pan from the keys, a dragged mouse button or the
//   cursor at the screen's edge, hooked), and a turn when the camera's yaw
//   is not where the turn left it the frame before. Either leaves the
//   camera theirs.
// - Why it stopped. OnMoveEnded and LastEnd say: there, the player, the
//   camera following something else, replaced, stopped, or no camera.
//
// A pan does not take the game's own changes to the offset for the
// player's: until 2026-09-30 it did, stopping when the offset moved more
// than 5 cm between frames. In the tactical view the game pulls the focus
// back toward the party while no party member is in view (CameraController
// ::PullSpeed), faster each frame, and a pan from a room the party could
// not be seen in was stopped by it within about six frames, twice at the
// start of the hero's turn in the turn-based mod. A pan now holds that
// pull at rest while it runs.
//
// A move eases out: quick at first, slowing into place, near enough there
// after `seconds`. A new move of the same kind replaces the one running.
// Called on the game's thread (a frame callback, a script event); the
// moves run in a frame callback of their own. One plugin's moves only: a
// second plugin's turn would look like the player's, and two pans would
// pull against each other.
#pragma once

#include "dao/Camera.h"
#include "dao/ObjectId.h"

#include <functional>
#include <optional>
#include <string_view>

namespace dao::camera
{

// The camera in explore or combat mode, the modes it moves in; null in any
// other (a menu, a dialogue, a cutscene, loading) or before the game is up.
CameraController *Current();

// When the camera starts following something else: the player selecting
// another party member, or a plugin's Follow or PanTo. `before` is what it
// followed until now. After the game's own handling (in the tactical view
// the camera has stayed where it was; close in, the game's pan has begun),
// on the game's thread. A callback that follows something else in turn is
// called again for that.
void OnTargetChanged(std::function<void(ObjectId now, ObjectId before)> callback);

// Turn the camera, close in, to a yaw (dao/Camera.h, SceneCamera::Yaw).
void EaseYaw(float radians, float seconds);

// Zoom the camera, as the mouse wheel does, to a zoom on the scene
// camera's scale (dao/Camera.h, SceneCamera::Zoom: 1 close behind the
// creature, 0 the tactical view high above). Below 0.5 the game turns to
// the tactical view, where the camera does not follow a change of target
// and pans differently (dao/Camera.h), so a plugin that means to stay in
// the close view keeps to 0.5 and above. The player wins: a zoom the
// player makes with the wheel while it runs stops it. Read 2026-10-02
// (dao/Camera.h, CameraController::ZoomSetting); not yet seen in play.
void EaseZoom(float zoom, float seconds);
// Whether the player has zoomed since the last EaseZoom: its ease ended by
// the player, or the zoom moved since it arrived. False before any.
bool PlayerZoomed();

// Follow a creature and glide the focus onto it. In the tactical view the
// camera keeps its height and its turn, and only slides; close in, the
// game pans by itself to a creature within 15 m (CameraController::
// Follow), and farther it cuts. Selecting that same creature while the pan
// runs (the turn-based mod's order: select, then pan) does not stop it:
// the game's own retargeting is taken as where the pan goes on from, not
// as the player panning. Selecting another creature, or the player
// panning, still does (2026-09-30: a pan stopped by a selection in the
// same frame once left the view on the creature before).
//
// What "arrived" means for a pan: the controller's offset from what it
// follows has been eased to nothing, which is the whole of the move in the
// tactical view. Close in there is no offset to ease, the game's own pan
// carrying the camera over (0x007707D0, from OnTargetChanged), so the move
// ends arrived on its first frame while that pan is still under way and
// CameraController::Focus() is still between the two creatures (in play,
// 2026-09-30, the turn-based mod's: arrived after 25 ms with the focus 3.1
// m from the creature). So to ask whether a pan fell short, read
// FocusOffset() or LastEnd, not the focus against the creature's position.
void PanTo(ObjectId object, float seconds);

// The yaw that puts the camera behind a creature, from its forward vector
// (GetOrientation): atan2(-y, -x), as seen in play. Nothing for an object
// with no orientation.
std::optional<float> YawBehind(ObjectId object);

// Whether a move is still running, and stopping them.
bool Moving();
void Stop();

enum class Move
{
    Yaw,  // EaseYaw
    Pan,  // PanTo
    Zoom, // EaseZoom
};

// Why a move ended.
enum class MoveEnd
{
    Arrived,    // there, near enough (2 cm, or a thousandth of a turn)
    Player,     // the player panned, turned or zoomed the camera
    Retargeted, // the camera began following something else
    Replaced,   // a new move of the same kind
    Stopped,    // Stop()
    NoCamera,   // the camera went: a menu, a dialogue, a cutscene, loading
};
std::string_view Name(MoveEnd why);

// When a move ends, and why, on the game's thread, once the move has let
// go of the camera: a callback may start another.
void OnMoveEnded(std::function<void(Move move, MoveEnd why)> callback);

// Why the last move of a kind ended; nothing while one runs or before the
// first.
std::optional<MoveEnd> LastEnd(Move move);

} // namespace dao::camera
