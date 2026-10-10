// The game's modes and their controller (dao/GameModes.h): the lookups that check what they read.
//
// The controller, read 2026-09-30. The application builds it at 0x005DC064 (its constructor,
// 0x00641370, stores GameModeController's four vtables) and keeps it at CClientExoApp+0x114. Its
// current mode is the int at +0x78, which the probe has read in play since 2026-09-29
// (dev/TURN_BASED.md); the modes themselves are objects in a table at +0x58, one per number, each
// a GameMode class (GameModeExplore, GameModeDialog, ...). SetGameMode (0x006414A0) switches: it
// asks the new mode's object (its vtable slot 10) whether to refuse, special-cases loading (9),
// and goes on; the controller's handler for SetGameModeMessage calls it with the message's mode
// and two zeros, and so does SetMode.
//
// Explore mode, read 2026-09-30. Combat mode derives from it (dao::TypeName and
// tools\daoexe.py --bases GameModeCombat). The mode's update (vtable slot 8, 0x00712F90) re-picks
// 15 times a second (a timer at +0x6E4 reset to 1/15 s), or at once when the engine asks: it
// casts a ray from the camera through the cursor (0x0070B590, up to 32 hits at +0xE0, their count
// at +0x6E0, sorted nearest first), then chooses (0x0070F270): the object at +0xCC, or the ground
// hit at +0xD0 with its verdict at +0xD4. An object the GUI names takes precedence (a global at
// 0x00C35FF0, +0xC), and an object hit wins over the ground. What the click handlers read: the
// primary action's (0x007132B0) target is +0xCC, and a move (0x0070C9A0) goes to +0xD0's point if
// +0xD4 allows, recording it at +0xA0 (0x0070BE80) before the creature walks. Each mode keeps its
// own; the one current is the one updated. On the game's thread, as a frame callback runs. Seen in
// play 2026-09-30 (dev/LIBRARY.md). The hits are dao::world::Hit, which dao/World.h's Raycast
// returns too.
//
// When the choice is nothing, read 2026-10-01. The chooser (0x0070EAA0) first asks the interface
// whether the mouse is over one of its movies' elements (the GUI system's slot 71, which asks the
// Scaleform manager, 0x00511420); if so it chooses nothing: no object, no ground, the walkable
// verdict false, though the ray was cast and the hits are there. And with no place for the cursor
// at all (outside the view) the ray is not cast and there are no hits. Otherwise it walks the
// hits: an object that passes its tests is chosen, else the ground hit, whose verdict is
// CanMoveTo's (the point free for the controlled creature, by its own pathfinding parameters, and
// clear of a collision box).
//
// The interface's answer (0x00511420) is yes while the mouse is over any of its movies (a set at
// the Scaleform manager's +0x80) or one of them holds a mouse press (+0xA0, set by the
// button-down handlers, 0x00512340, and cleared by the button-up ones, 0x00512480). Both are
// changed by the mouse's events, not each frame, while the pick is re-taken every 1/15 s whether
// the mouse moves or not (the mode's update, 0x00712F90); so a pick that stays nothing with the
// mouse still is the interface's state left as the last mouse event had it.
//
// Seen in play, 2026-10-01: after the game's window regains the focus, a click at the pixel the
// mouse already rests on is no order and the pick holds no ground until the mouse moves, with no
// plugin drawing anything.
//
// So a plugin's movie that draws under the cursor empties the pick and takes the click. Seen in
// play that day (the turn-based mod's path preview): with a shape of its layer-1 movie on the
// cursor's pixel, CursorGround was nothing in half the reads and six of nine clicks to move were
// lost; with nothing drawn there, never. A visual effect with a shape is an object hit and takes
// the ground's place too (dao/Vfx.h, SetPickable).
#include "dao/GameModes.h"

#include "dao/SafeRead.h"
#include "dao/TypeName.h"

#include <string>

namespace dao
{

// +0x58 points to an array of the mode objects indexed by the mode's number, 13 entries (seen in
// play 2026-09-30, one GameMode class per number: dao::GameMode), as SetGameMode finds them. Not
// a std::vector: +0x5C and +0x60 are other pointers. The current mode is at +0x78 and, by its
// value after a load (9, loading), the previous one at +0x7C. An entry is returned only if its
// class is a GameMode (dao::TypeName).
void *GameModeController::ModeObject(GameMode mode) const
{
    const auto index = static_cast<std::size_t>(mode);
    const auto modes = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(this) + kModes);
    if (!modes || *modes == 0 || index >= kMaxModes)
    {
        return nullptr;
    }
    const auto entry = TryRead<std::uintptr_t>(*modes + (4 * index));
    if (!entry || *entry == 0)
    {
        return nullptr;
    }
    auto *object = reinterpret_cast<void *>(*entry);
    return TypeName(object).starts_with("GameMode") ? object : nullptr;
}

// The camera controller is the controller's +0x60, returned only if its class is
// CameraControllerExplore (dao::TypeName).
CameraController *GameModeController::Camera() const
{
    const auto camera = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(this) + kCamera);
    if (!camera || *camera == 0)
    {
        return nullptr;
    }
    auto *object = reinterpret_cast<CameraController *>(*camera);
    return TypeName(object) == "CameraControllerExplore" ? object : nullptr;
}

} // namespace dao
