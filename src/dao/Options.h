// Adds a plugin's own settings to the game's options screen, and drives the screen as a player does.
//
// AddSection puts a section on the Game tab of the options screen, or on its Video or Online tab:
// a heading, then checkboxes (Toggle), choices of a few named values (Choice), each a slider with
// stops or a dropdown, and buttons (Button), that look and behave like the game's own. The game
// does not save these settings, so the plugin must store them itself.
//
// Open, ShowTab, Set, Ok and Cancel work the screen through its own buttons, as the player's
// clicks do, for a tool or a test:
//
//   dao::options::Open();                       // from the start menu or Escape's menu
//   // ...frames later, once IsOpen():
//   dao::options::ShowTab(dao::options::Tab::Video);
//   dao::options::Set(L"ElgaraSharpness", 2);
//   dao::options::Ok();
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
#include <optional>
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
// may call script commands. A change takes effect at the click; the screen's Cancel then gives
// `set` back the value the option had as the screen opened, and OK keeps the change.
struct Toggle
{
    // The option's name: letters and digits, starting with a letter. Prefix it with the
    // plugin's name, for example "TurnBasedCombat", so that it cannot clash with another plugin's.
    std::wstring name;
    std::wstring label;                   // the text beside the box
    std::wstring tooltip;                 // the text shown when the mouse is over the box
    std::function<bool()> get;            // returns the option's value; required
    std::function<bool(bool wanted)> set; // applies a click and returns the new value; required
    // The value the screen's Restore Defaults gives it, through set, as a click would; none to
    // leave it as it is.
    std::optional<bool> defaultValue = std::nullopt;
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
// first value before either has run), then switches to what `get` returns. A dropdown's list opens
// downward over the rows under it.
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
    // The index the screen's Restore Defaults gives it, through set, as a change would; none to
    // leave it as it is.
    std::optional<std::int32_t> defaultValue = std::nullopt;
};

// A button in a plugin's section, like the screen's own Restore Defaults.
//
// `pressed` runs on the game thread (dao/Frame.h) each time the player clicks the button, so it
// may call script commands. The screen stays open.
//
//   dao::options::AddSection(L"Multiplayer", {}, {}, dao::options::Column::Either,
//                            dao::options::Tab::Online, {{
//       .name = L"MultiplayerHost",
//       .label = L"Host a game",
//       .tooltip = L"Lets a friend join this game",
//       .pressed = [] { Host(); },
//   }});
struct Button
{
    std::wstring name;             // the button's name, as for a Toggle
    std::wstring label;            // the text on the button
    std::wstring tooltip;          // the text shown when the mouse is over the button
    std::function<void()> pressed; // runs at a click; required
};

// The column of its tab a section would be in.
enum class Column
{
    Either, // the first with room: the left, then the right
    Left,
    Right,
};

// A tab of the options screen.
enum class Tab
{
    Game,     // five free rows in its left column, six in its right
    Video,    // its right column, fourteen rows: a heading, two checkboxes and eight sliders, say
    Online,   // nine free rows in its left column, seven in its right
    Controls, // the key bindings; no room for a section
    Audio,    // no room for a section
};

// Adds a section of settings to a tab of the options screen.
// @param title the section's heading, which is also its key for StateOf
// @param toggles the checkboxes, listed first
// @param choices the sliders and dropdowns, listed after the checkboxes in the order given
// @param column the column the section goes in if that column has the rows for it; it goes in
//  the other if not
// @param tab the tab the section is on
// @param buttons the buttons, listed after the choices in the order given
// @return false, with nothing added, if the section has no toggles, choices or buttons, a name
//  is not letters and digits or is already taken, a callback is missing, a choice has fewer
//  than two values, or the tab is Controls or Audio
//
// Call it from DAOLib_Load, before the game first opens its options screen. A tab's free rows are
// shared by every plugin. A heading takes two rows, a checkbox one and a dropdown two; sliders
// stand as close as the game's own, six of them in seven rows, and buttons two in three; and a
// section takes whole rows of one column. A section on the Video tab has the tab laid out for it: the game's own
// settings closer together and all in the left column, and the right column, a heading and twelve
// rows, for sections. Sections are placed in the order they were added. The first section a tab has no room for
// is left out, and so is every later section on that tab; the plugin's log has a warning for
// each, and StateOf says NoRoom.
bool AddSection(std::wstring title, std::vector<Toggle> toggles, std::vector<Choice> choices = {},
                Column column = Column::Either, Tab tab = Tab::Game, std::vector<Button> buttons = {});

// What became of a section given to AddSection.
enum class SectionState
{
    Unknown, // no section with that title was added
    Waiting, // the game has not opened its options screen yet
    Shown,   // placed on its tab
    NoRoom,  // its tab has not the free rows for it, or sections added earlier took them
    Failed,  // the options screen's Flash movie was not as expected, so it was left alone
};

// Returns what became of the section with that title.
SectionState StateOf(std::wstring_view title);

// Why a step on the options screen was refused, or None if it was taken.
enum class Refusal : std::uint8_t
{
    None,
    NotShown,   // the screen is not open; for Open, nothing to open it from; for Set, its tab is not shown
    AlreadySo,  // the screen is open already, or the tab shown already
    Disabled,   // the tab's button is disabled
    NotOffered, // no control of a plugin's by that name is on the screen
    OutOfRange, // a checkbox given other than 0 or 1, or a choice an index it has not
};

// Returns whether the options screen is open.
//
// Call it, and the rest below, on the game thread.
bool IsOpen();

// Opens the options screen as the player does: the start menu's Options button, or Escape's menu's,
// with that menu opened first in the game. The screen comes with the menu's backing panel.
// @return None once it is asked for, NotShown where neither menu can be had (a conversation, a
//  cutscene, a loading screen, another full screen), AlreadySo if it is open
//
// The screen opens a few frames later: IsOpen says when.
Refusal Open();

// One of the screen's tabs.
struct TabState
{
    Tab tab = Tab::Game;
    std::wstring name;    // as its button shows it
    bool enabled = false; // its button takes a click
    bool current = false; // its page is shown
};

// Returns the screen's tabs in their order, or none if it is not open.
std::vector<TabState> Tabs();

// Shows a tab as a click on its button does: the button lit, the page shown, and Restore
// Defaults acting on it.
Refusal ShowTab(Tab tab);

// What a control is.
enum class Kind : std::uint8_t
{
    Toggle, // a checkbox: 0 or 1
    Choice, // a slider with named stops or a dropdown: an index of its values
    Slider, // a slider of numbers from `least` to `most`, the game's own volumes, gamma and mouse
    Button, // a plugin's button
};

// A control on the screen: a plugin's, or the game's own.
struct EntryState
{
    // A plugin's control: the name the plugin gave it. The game's: its option, such as
    // L"GameOptions.AutoPauseCombat" or L"AudioOptions.MusicVolume", which has a dot.
    std::wstring name;
    std::wstring label;
    Kind kind = Kind::Toggle;
    double value = 0;                 // a checkbox's 0 or 1, a choice's index, a slider's number; 0 for a button
    double least = 0;                 // a slider's least
    double most = 0;                  // and most
    std::vector<std::wstring> values; // a choice's values' names
    bool enabled = true;              // takes a change; a volume is not while its sound is disabled
    bool game = false;                // the game's own control
    Tab tab = Tab::Game;
};

// Returns the controls on a tab: this plugin's placed there, and, while the tab is shown, the
// game's own and the other plugins' checkboxes and choices. None if the screen is not open.
std::vector<EntryState> Entries(Tab tab);

// Sets a control as the player's click or drag on it does: a checkbox to 0 or 1, a choice to an
// index of its values, a slider to a number from its least to its most, a plugin's button pressed
// (its value is ignored). A plugin's `set` or `pressed` runs a frame later; the game's own option
// changes as its control's would, and OK keeps it or Cancel puts it back as the game does.
// @return NotShown unless the screen is open with the control's tab shown; Disabled for a control
//  that takes no change now
Refusal Set(std::wstring_view name, double value);

// Presses OK as the player does: the changes kept and the screen closed. A change the game needs
// a restart or a confirmation for opens the game's message box first (dao/Gui.h, CurrentPopup).
Refusal Ok();

// Presses Cancel as the player does: the screen closed, and each plugin's control given back, through
// its `set`, the value it had when the screen opened.
Refusal Cancel();

} // namespace dao::options
