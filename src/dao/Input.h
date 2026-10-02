// A plugin's own action: a key registered with the game's own, kept in its
// KeyBindings.ini and listed in its controls screen, and a callback when
// it is pressed.
//
//   dao::input::RegisterAction(
//       {.name = L"MyPluginEndTurn", .label = L"End turn", .key = dao::input::Key::Enter},
//       [] { EndTurn(); });
//
// How the game does its own, read 2026-10-01 (dao/Addresses.h):
//
// - At start-up the game mode controller (0x00641870) registers each action
//   by name with the text the controls screen shows for it
//   (GroupedInputMappings::AddAction), and what its keys send: a text naming
//   a message, "UserTogglePauseMessage" (AddMapping).
// - It then gives the actions their keys: the defaults (ResetKeys, which
//   the controls screen's defaults button calls too), then the player's
//   own from KeyBindings.ini, whose keys are the actions' names
//   (TogglePause_0, TogglePause_1 for an action's two keys).
// - A pressed key's texts are made into messages and sent, but only those
//   the current mode allows (InputModes::Allowed): each mode has a group of
//   texts, and a group that shuts all others out while it is on.
//
// A plugin's action goes the same way, added just after the game's own:
// its name and label, a text no message has ("DAOLibAction <name>",
// dao/core/InputCode.h), that text added to the groups of the modes the
// plugin names, and its default key after the game's defaults. When the
// key is pressed and the mode allows the text, this takes it before the
// engine looks for a message, and the callback runs at the next frame, on
// the frame's thread (dao/Frame.h), where a plugin may call script
// commands and send messages.
//
// So the action is the game's in every way but what it sends: it is listed
// in the engine's actions, its key is saved in KeyBindings.ini with the
// rest, and it is silent where the game's own keys are (a mode not named,
// or while the engine shuts the keys out).
//
// Seen in play, 2026-10-01, with examples/input/ (the play tester's
// session, the real keyboard):
// - The engine took both actions as the game came up, 7.4 s after load.
// - The default key, F11, ran the callback once a press while exploring,
//   and once for a key held two seconds: the trigger is the press.
// - It was silent with the area map, the main menu and the inventory up,
//   and worked again when they closed. It did run with the game paused
//   (Space) while exploring: a pause is not a mode.
// - The game wrote the actions into KeyBindings.ini at that first start,
//   after its own: "DAOLibExamplePing_0=Keyboard::Button_F11",
//   "DAOLibExamplePing_1=(UNASSIGNED)".
// - The controls screen's Restore Defaults left F11 working, and the
//   game's own keys worked throughout.
// - The controls screen did NOT list the actions. That a line changed in
//   KeyBindings.ini is read back for a plugin's action is not yet seen.
//
// The controls screen, read after: its list is written in its movie's
// script, one AddKeyBinding("TogglePause") for each action it shows, so
// no action the script does not name is listed. This now adds the same
// call for each action the engine took, under a header after the game's
// last section (the action's `section`, "Mods" unless the plugin names its
// own), by patching the movie as the game opens it
// (dao/Gui.h, OnMovieLoad; dao/core/GfxMovie.h). A row made that way is
// the game's own row: the label and the two keys from the engine's data,
// rebound as the game's are. Seen in play the same day, with
// examples/input/: the header and both rows at the list's end; a key bound
// to the row with none (F10) ran its callback, and the other rebound (F11
// to F12) answered the new key and not the old; both were in
// KeyBindings.ini and still bound after a restart; opening the screen
// again doubled nothing; Restore Defaults put the first back to F11 and
// emptied the second; and a row of the game's own still rebound.
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

using Key = core::Key;
inline constexpr std::uint32_t kShift = core::kShift;
inline constexpr std::uint32_t kControl = core::kControl;
inline constexpr std::uint32_t kAlt = core::kAlt;

struct Action
{
    // The action's name: its key in KeyBindings.ini and its name among the
    // engine's actions. Letters and digits, a letter first; make it the
    // plugin's own ("MyPluginEndTurn"), since a name the game or another
    // plugin has is refused.
    std::wstring name;
    // What the controls screen shows for it.
    std::wstring label;
    // The key it has until the player binds another; Key::None for no key.
    Key key = Key::None;
    std::uint32_t modifiers = 0; // kShift, kControl, kAlt, or'd
    // The modes its key works in. Loading has no group of keys.
    std::vector<GameMode> modes = {GameMode::Explore, GameMode::Combat};
    // The heading its row goes under in the controls screen: a plugin's
    // own ("Turn-based"), shared by its actions.
    std::wstring section = L"Mods";
};

// Registers an action and the callback for its key. From DAOLib_Load only:
// the game registers its actions once, at start-up, and reads the player's
// keys after. False, and nothing registered, for a name that is not
// letters and digits, one this was given before, or once the game has
// registered its own.
bool RegisterAction(Action action, std::function<void()> pressed);

enum class ActionState
{
    Unknown,    // no action of that name was given to RegisterAction
    Waiting,    // registered here; the game has not registered its actions yet
    Registered, // the engine has it
    NameTaken,  // the engine already had an action of that name; this one is not there
};

ActionState StateOf(std::wstring_view name);

// How often the action's key has been pressed where its mode allowed it.
std::uint64_t Presses(std::wstring_view name);

// The game's idea of where the mouse is, for its world pick, set by the
// plugin: the place in the game's pixels (as dao::world::ToScreen gives
// them) that explore mode casts its cursor ray through, so what it picks
// there (GameModeExplore::CursorObject, CursorGround) is what the player
// would have under a real cursor there, and whatever follows the pick (a
// path preview, a cost caption) follows it. The real mouse is not moved,
// nor read for the pick while this holds; the game's window need not be in
// front. ReleaseCursor gives the pick back to the mouse.
//
// Read 2026-10-02 (dao/Addresses.h, kInput_AxisPair): the pick asks the
// input system for input 43, an axis pair, and this answers it. While it
// holds, the interface's hover test (dao/Gui.h, MouseOverInterface, which
// explore mode asks before it keeps a pick) answers "not over the
// interface" as well, so the pick holds wherever the real mouse is: seen
// in play the same day, through the Eluvian server, the ray followed the
// pointed pixel (four hits at a creature's, none before) but the pick kept
// nothing, the real mouse being off the game's window and the hover test
// saying "over the interface". The Flash interface itself still follows
// the real mouse (its own hovers, the cursor drawn on the screen), and
// MouseOverInterface answers false while this holds. The pick's holding
// at the pointed place is not yet seen.
void PointCursor(std::int32_t x, std::int32_t y);
void ReleaseCursor();

} // namespace dao::input
