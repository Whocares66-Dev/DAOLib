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
    // The plugin's key map the action belongs to, or empty for none. An action in a key map and
    // one outside it may have the same key, and the action answers only while its map is on
    // (SetKeymap).
    // An initializer, so a plugin may leave the field out of its designated initializer.
    std::wstring keymap = {}; // NOLINT(readability-redundant-member-init)
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

// Returns whether a key the player bound to the named action is down now, in a game mode the
// action names: for an action held rather than pressed, such as moving a camera.
// @return false for an unknown name, an action the game has not accepted, or one bound to no key
//  of the keyboard
//
// The modifiers the binding names must be down with the key; others may be down too. A binding
// to a mouse button does not count. Call it from a frame callback.
bool Held(std::wstring_view name);

// Turns one of the plugin's key maps on or off, as a mode of its own: a free camera's keys on W,
// A, S and D while it flies, the game's walking on them otherwise.
// @param keymap the name actions give as Action::keymap
//
// While a map is on, its actions answer to their keys, in the game modes each names, and every
// other action with one of those keys does not answer to it, the game's own among them; an
// action's other key still works. Maps are off until turned on. Call it on the game thread.
void SetKeymap(std::wstring_view keymap, bool on);

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

// Hides the cursor and holds it where it is, as the game's right-drag orbit does, or shows it
// there again.
// @param held true to hide and hold it, false to give it back
//
// While it holds, the game's own giving back (its orbit's, its fly camera's) waits for this, and
// MouseDelta still reads the mouse. Call it on the game thread.
void HoldCursor(bool held);

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

// Returns whether a key is down now, as the game's input reads the keyboard, whatever the game
// does with it: withheld (Withhold), unbound or in a menu, and whatever else is held with it.
// @return false before the game reads its input, or with its window in the background
bool Down(Key key);

// A mouse button as the device numbers it, whatever the game's left-handed setting.
enum class MouseButton : std::uint8_t
{
    Left,
    Right,
    Middle,
    Fourth,
    Fifth,
};
// Returns whether a mouse button is down now, as Down(Key) answers for a key.
bool Down(MouseButton button);

// How far the mouse moved: the device's own counts, x to the right and y down, unbounded by the
// window or the screen.
struct MouseMove
{
    std::int32_t x = 0;
    std::int32_t y = 0;
};
// Returns how far the mouse moved during the last frame, whatever the game does with it.
//
// The frame is the one dao::OnFrame runs: read it from a frame callback.
MouseMove MouseDelta();
// Returns the wheel's notches during the last frame, away from the player positive, whatever the
// game does with them. Read it from a frame callback.
std::int32_t WheelNotches();

// What of the game's own reaction to the keys and mouse a plugin may withhold, or'd together.
enum Reaction : std::uint32_t
{
    kMovement = 1,   // moving the controlled creature: the movement keys, steering, run forward
    kCameraTurn = 2, // turning and tilting the camera: the right mouse's orbit, the camera keys
    kCameraZoom = 4, // zooming: the wheel, Page Up and Page Down
    kCameraPan = 8,  // panning: the middle mouse's drag and the camera's pan keys
};
// Withholds the game's own reaction to the keys and mouse, until given back.
// @param reactions those to withhold, or'd; 0 gives all back
//
// What is withheld is the game's reading of its own inputs; a plugin's actions (RegisterAction)
// still fire, and Down, MouseDelta and WheelNotches still answer. Clicks in the world are not
// withheld. A movement a plugin holds (Hold) is withheld with the keys'. Call it from any thread.
void Withhold(std::uint32_t reactions);

} // namespace dao::input
