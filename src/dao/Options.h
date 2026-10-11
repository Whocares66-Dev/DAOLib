// Adds a plugin's own settings to the game's options screen.
//
// AddSection puts a section on the Game tab of the options screen: a heading, then checkboxes
// (Toggle) and choices of a few named values (Choice), each a slider with stops or a dropdown, that
// look and behave like the game's own. The game does not save these settings, so the plugin must
// store them itself.
//
//   dao::options::AddSection(L"Turn-based", {{
//       .name = L"TurnBasedCombat",
//       .label = L"Turn-based combat",
//       .tooltip = L"Changes outside a fight only",
//       .get = [] { return Enabled(); },
//       .set = [](bool wanted) { return SetEnabled(wanted); },
//   }});
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::options
{

// A checkbox in a plugin's section.
//
// `get` runs each time the screen opens. `set` runs when the player clicks the box, and returns
// the option's value after the click: `wanted` to accept the change, or the old value to refuse
// it. The box shows whatever `set` returns. Both run on the game thread (dao/Frame.h), so they
// may call script commands. A change takes effect at the click, and the screen's Cancel button
// does not undo it.
struct Toggle
{
    // The option's name: letters and digits, starting with a letter. Prefix it with the
    // plugin's name, for example "TurnBasedCombat", so that it cannot clash with another plugin's.
    std::wstring name;
    std::wstring label;                   // the text beside the box
    std::wstring tooltip;                 // the text shown when the mouse is over the box
    std::function<bool()> get;            // returns the option's value; required
    std::function<bool(bool wanted)> set; // applies a click and returns the new value; required
};

// How a Choice is shown.
enum class ChoiceStyle
{
    Slider,   // a slider with a stop for each value, like the detail sliders on the Video tab
    Dropdown, // a button that opens a list of the values, like Difficulty on the Game tab
};

// A choice of one of a few named values in a plugin's section.
//
// `get` and `set` work as they do for a Toggle, but with the index of a value in `values` in place
// of a bool. `wanted` is always a valid index. Dragging a slider across several stops calls `set`
// once for each. As the screen opens, the choice shows the last value `get` or `set` returned (the
// first value before either has run), then switches to what `get` returns. A choice takes two
// rows. A dropdown's list opens downward over the rows under it.
//
//   dao::options::AddSection(L"Turn-based", {}, {{
//       .name = L"TurnBasedSpeed",
//       .label = L"Turn speed",
//       .tooltip = L"How fast the turns play",
//       .values = {L"0.5x", L"1x", L"1.5x", L"2x"},
//       .get = [] { return SpeedIndex(); },
//       .set = [](std::int32_t wanted) { return SetSpeedIndex(wanted); },
//   }});
struct Choice
{
    std::wstring name;                                    // the option's name, as for a Toggle
    std::wstring label;                                   // the text to the left of the slider or dropdown
    std::wstring tooltip;                                 // the text shown under the label
    std::vector<std::wstring> values;                     // the values' names; at least two
    ChoiceStyle style = ChoiceStyle::Slider;              // a slider, or a dropdown
    std::function<std::int32_t()> get;                    // returns the index of the chosen value
    std::function<std::int32_t(std::int32_t wanted)> set; // applies a change, returns the new index
};

// The column of the Game tab a section would be in.
enum class Column
{
    Either, // the first with room: the left, then the right
    Left,
    Right,
};

// Adds a section of settings to the Game tab of the options screen.
// @param title the section's heading, which is also its key for StateOf
// @param toggles the checkboxes, listed first
// @param choices the sliders and dropdowns, listed after the checkboxes in the order given
// @param column the column the section goes in if that column has the rows for it; it goes in
//  the other if not
// @return false, with nothing added, if the section has no toggles or choices, a name is not
//  letters and digits or is already taken, a callback is missing, or a choice has fewer than
//  two values
//
// Call it from DAOLib_Load, before the game first opens its options screen. The tab has five free
// rows in its left column and six in its right, shared by every plugin. A heading takes two rows,
// a checkbox one and a choice two, and a section stays in one column. Sections are placed in the
// order they were added. The first section that does not fit is left out, and so is every
// section after it.
bool AddSection(std::wstring title, std::vector<Toggle> toggles, std::vector<Choice> choices = {},
                Column column = Column::Either);

// What became of a section given to AddSection.
enum class SectionState
{
    Unknown, // no section with that title was added
    Waiting, // the game has not opened its options screen yet
    Shown,   // placed on the Game tab
    NoRoom,  // sections added earlier took the free rows
    Failed,  // the options screen's Flash movie was not as expected, so it was left alone
};

// Returns what became of the section with that title.
SectionState StateOf(std::wstring_view title);

} // namespace dao::options
