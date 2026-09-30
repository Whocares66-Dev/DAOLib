// Moving the camera smoothly, the player's own turning and panning taken
// into account: the part every plugin that moves the camera would
// otherwise write, and get wrong in the same places (dao/Camera.h has the
// camera itself).
//
//   dao::camera::PanTo(creature, 0.6f);                    // tactical: glide there, no turning
//   if (const auto behind = dao::camera::YawBehind(creature))
//       dao::camera::EaseYaw(*behind, 0.6f);               // close in: come round behind it
//
// - From where the camera is. Each frame reads the camera's yaw and
//   offset as they are, so a player who has turned or panned the camera
//   since the plugin last moved it is where the move starts from.
// - The short way round. The camera's yaw is not wrapped (it ran past 2
//   pi in play), so a turn goes by the smaller angle to the target,
//   whatever multiple of 2 pi either is at.
// - The player wins. If the camera is not where the move left it the
//   frame before, the player (or the game) has turned or panned it, and
//   the move stops there, leaving the camera theirs.
//
// A move eases out: quick at first, slowing into place, near enough there
// after `seconds`. A new move of the same kind replaces the one running.
// Called on the game's thread (a frame callback, a script event); the
// moves run in a frame callback of their own. One plugin's moves only:
// two plugins moving the camera at once would each see the other's as the
// player's and stop. Not yet seen in play.
#pragma once

#include "dao/Camera.h"
#include "dao/ObjectId.h"

#include <functional>
#include <optional>

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

// Follow a creature and glide the focus onto it. In the tactical view the
// camera keeps its height and its turn, and only slides; close in, the
// game pans by itself to a creature within 15 m (CameraController::
// Follow), and farther it cuts.
void PanTo(ObjectId object, float seconds);

// The yaw that puts the camera behind a creature, from its forward vector
// (GetOrientation): atan2(-y, -x), as seen in play. Nothing for an object
// with no orientation.
std::optional<float> YawBehind(ObjectId object);

// Whether a move is still running, and stopping them.
bool Moving();
void Stop();

} // namespace dao::camera
