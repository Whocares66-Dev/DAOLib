// A plugin's own action: a key the player can rebind in the game's controls
// screen, and a callback when it is pressed.
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
// Not yet seen in play: none of this has run in the game. What play has to
// say: that the action shows in the controls screen and can be rebound
// there, that the callback runs once per press (the trigger is the one the
// game's screenshot and console keys have, taken to be the press), and
// that a rebound key comes back from KeyBindings.ini at the next start.
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
    std::uint32_t modifiers = 0; // kShift, kControl
    // The modes its key works in. Loading has no group of keys.
    std::vector<GameMode> modes = {GameMode::Explore, GameMode::Combat};
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

} // namespace dao::input
