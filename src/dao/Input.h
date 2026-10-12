// Gives a plugin key bindings of its own, and lets it choose what the game treats as under the
// cursor.
//
// RegisterAction adds an action to the game's key bindings. The player can rebind it in the
// controls screen, the game saves the binding in KeyBindings.ini, and the plugin's callback runs
// when the key is pressed.
//
// PointCursor moves the game's world pick, which decides what object or ground is under the
// cursor, to a pixel of the plugin's choosing without moving the mouse.
//
//   dao::input::RegisterAction(
//       {.name = L"MyPluginEndTurn", .label = L"End turn", .key = dao::input::Key::Enter},
//       [] { EndTurn(); });
#pragma once

#include "dao/GameModes.h"
#include "dao/core/InputCode.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::input
{

using Key = core::Key; // a keyboard key, identified by its DirectInput scan code

inline constexpr std::uint32_t kShift = core::kShift;     // Shift, for Action::modifiers
inline constexpr std::uint32_t kControl = core::kControl; // Ctrl, for Action::modifiers
inline constexpr std::uint32_t kAlt = core::kAlt;         // Alt, for Action::modifiers

// An action for RegisterAction: its name, its label, its default key, and the game modes where
// the key works.
struct Action
{
    // The action's name in KeyBindings.ini and inside the game. Use letters and digits, starting
    // with a letter. Prefix it with the plugin's name, for example "MyPluginEndTurn": a name that
    // the game or another plugin already uses is refused.
    std::wstring name;
    std::wstring label;          // the text shown in the controls screen
    Key key = Key::None;         // the default key, or Key::None for no default
    std::uint32_t modifiers = 0; // the default key's modifiers: kShift, kControl and kAlt, or'd
    // The game modes in which the key works. GameMode::Loading is ignored.
    std::vector<GameMode> modes = {GameMode::Explore, GameMode::Combat};
    // The heading the controls screen lists the action under. Actions with the same section
    // share a heading.
    std::wstring section = L"Mods";
};

// Registers an action and the callback its key runs.
// @param pressed runs once per key press, on the game thread (dao/Frame.h), so it may call
//  script commands
// @return false, with nothing registered, if the name is not valid or this plugin has already
//  registered it, if `pressed` is empty, or if the game has already registered its own actions
//
// Call it from DAOLib_Load, because the game registers its actions only once, at start-up. Call
// StateOf later to find out whether the game accepted the action.
bool RegisterAction(Action action, std::function<void()> pressed);

// Whether the game has accepted an action given to RegisterAction.
enum class ActionState
{
    Unknown,    // no action of that name was given to RegisterAction
    Waiting,    // the game has not registered its own actions yet
    Registered, // the game has added the action
    NameTaken,  // the game already had an action of that name, so this one was not added
};

// Returns whether the game has accepted the named action.
ActionState StateOf(std::wstring_view name);

// Returns how many times the action's key has been pressed in a game mode where it works; 0 for
// an unknown name.
std::uint64_t Presses(std::wstring_view name);

// Makes the game treat a pixel as the one under the mouse, without moving the mouse.
// @param x the pixel's column, in the screen pixels that dao::world::ToScreen returns
// @param y the pixel's row
//
// In explore mode the game then picks the object or ground at that pixel, and anything that
// depends on what is under the cursor, such as a path preview, follows it. This works even when
// the game's window is not in front. The Flash interface still follows the real mouse. The pixel
// stays in effect until ReleaseCursor. Call it from any thread.
void PointCursor(std::int32_t x, std::int32_t y);

// Hands the world pick back to the real mouse after PointCursor.
void ReleaseCursor();

// One of the four ways the game's movement keys steer the controlled creature.
enum class Move : std::uint8_t
{
    Forward,
    Backward,
    Left,
    Right,
};

// Holds one of the game's movement keys down, or lets it go, with no key pressed.
// @param direction the key
// @param held true to hold it down, false to let it go
//
// While a direction is held the game steers the controlled creature as it does by the keyboard:
// by the way the camera faces, running or walking as the player has it set, with the game's own
// drive command, and only where the game takes movement keys at all (not in a conversation or a
// menu). It stops when nothing is held. A real key pressed at the same time counts as well.
// This works even when the game's window is not in front. A direction stays held until it is
// let go, so let go of what was held. Call it from any thread.
void Hold(Move direction, bool held);

} // namespace dao::input
