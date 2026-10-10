// Smooth camera moves: turning, panning to a creature and zooming, each eased over a given
// time.
//
// Every move starts from where the camera is now, turns the short way round, and stops as soon
// as the player turns, pans or zooms the camera. Starting a move replaces any running move of
// the same kind. Only one plugin should move the camera, because to each plugin another
// plugin's moves look like the player's. Call these on the game thread.
//
//   dao::camera::PanTo(creature, 0.6f);                    // tactical view: glide, no turning
//   if (const auto behind = dao::camera::YawBehind(creature))
//       dao::camera::EaseYaw(*behind, 0.6f);               // close view: come round behind it
//   dao::camera::EaseZoom(0.7f, 0.8f);                     // pull back a little
#pragma once

#include "dao/Camera.h"
#include "dao/ObjectId.h"

#include <functional>
#include <optional>
#include <string_view>

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
// Below 0.5 the game switches to the tactical view, where the camera does not follow a change
// of target, so stay at 0.5 or above to remain in the close view. Does nothing if there is no
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
// Nothing stops the camera at a wall or the ground, so keep the distance modest in tight
// spaces. Call on the game thread.
void SetCloseDistance(std::optional<float> metres);
// Returns the camera's distance from its pivot at full zoom, in metres, or 0 if the running
// build of the game does not support it.
float CloseDistance();

// Returns why the last move of a kind ended.
// @return nullopt while a move of that kind is running, or before the first one
std::optional<MoveEnd> LastEnd(Move move);

} // namespace dao::camera
