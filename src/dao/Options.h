// A plugin's own settings in the game's options screen: a section on the
// Game tab, with a heading and checkboxes.
//
//   dao::options::AddSection(L"Turn-based", {{
//       .name = L"TurnBasedCombat",
//       .label = L"Turn-based combat",
//       .tooltip = L"Changes outside a fight only",
//       .get = [] { return Enabled(); },
//       .set = [](bool wanted) { return SetEnabled(wanted); },
//   }});
//
// How the game does its own, read 2026-10-01 (dev/GUI.md, dao/core/
// OptionsMovie.h): the Game tab is a clip of controls the designer placed,
// with rows to spare under its two columns; a checkbox is the movie's own
// symbol set up by the movie's own helper, which sends the engine
// "ClientOptions.SetClientOptionMessage <option> 1" (or 0) on a click and
// draws the box by what the engine tells it the option is.
//
// A plugin's section goes the same way. The library patches the options
// movie as the game opens it (dao/Gui.h, OnMovieLoad): the heading and the
// checkboxes are placed in the tab's free rows, and each checkbox is set up
// by the game's helper, so it looks and behaves as its neighbours do,
// tooltip and click sound with them. The library answers for the engine:
// it takes the checkbox's request to be told its option's value, and its
// click, before the engine sees either.
//
// `get` is asked when the screen opens. `set` runs when the player clicks,
// with what the box would become, and answers what the option now is: the
// same to accept, the old value to refuse, and the box is drawn by the
// answer. Both run on the frame's thread (dao/Frame.h), so they may call
// script commands. A change takes effect at the click, not at the screen's
// OK, and Cancel does not undo it; the game saves nothing of it, so the
// plugin keeps its own settings.
//
// There is room for eleven rows: five under the left column and six under
// the right, a heading taking one. Sections are placed in the order they
// were added, each whole or not at all; StateOf says what became of one.
// The rows are shared with every other plugin that adds a section, each
// taking what the ones before it left.
//
// From DAOLib_Load, before the game first opens its options.
//
// Seen in play, 2026-10-01, with examples/options/ beside examples/input/
// (the play tester's session, on 3783dd4):
// - The heading and three boxes under the left column's last box, the
//   boxes and labels in line with the game's own and spaced as they are.
//   The heading is plain text centred over the column, smaller than the
//   game's own gold bars ("Gameplay", "Feedback"): a caption.
// - Each box was asked its value at each opening and drawn by the answer.
// - A click changed a box and it stayed; the box that refuses was empty
//   again 0.3 s after its click, with no flicker caught.
// - Each box had its tooltip, drawn under it over the row below.
// - The game's own boxes worked and did not move. Restore Defaults reset
//   the game's and left the plugin's; after Cancel and a second opening
//   the plugin's were as left and nothing was doubled.
// - In a fight, the screen opened from the main menu with the game paused
//   behind it: the refusing box refused and another changed, each
//   callback run at the click, not when the screen closed.
// The first try, with two plugins patching the movie, showed no section:
// dao/Gui.h, OnMovieLoad, has why.
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::options
{

struct Toggle
{
    // Letters and digits, a letter first, and the plugin's own: the option
    // the checkbox stands for ("TurnBasedCombat").
    std::wstring name;
    std::wstring label;
    std::wstring tooltip;
    std::function<bool()> get;
    std::function<bool(bool wanted)> set;
};

// False, and nothing added, for a section with no toggles, a toggle with a
// name that is not letters and digits or was given before, or one without
// both callbacks.
bool AddSection(std::wstring title, std::vector<Toggle> toggles);

enum class SectionState
{
    Unknown, // no section of that title was added
    Waiting, // the game has not opened its options screen yet
    Shown,   // placed on the Game tab
    NoRoom,  // the tab's free rows were taken by sections added before it
    Failed,  // the options movie was not as read, and was left alone
};

SectionState StateOf(std::wstring_view title);

} // namespace dao::options
