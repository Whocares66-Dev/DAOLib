// The game's options screen, added to as bytes: the part of dao/Options.h
// that needs no game.
//
// Read 2026-10-01 from the game's optionsmenu.gfx (dev/GUI.md):
//
// - The Game tab is one sprite of controls the designer placed in two
//   columns, a row every 35 pixels of the movie's 1024 by 768 stage: at
//   the left two choices and four checkboxes, the last OptLootShimmer_mc
//   at (7, 228); at the right a choice and four checkboxes, the last
//   OptShowTutorials_mc at (483, 166). Below them the tab is empty to the
//   screen's buttons, about 440.
// - A checkbox is the symbol the movie exports as checkbox_mc, placed with
//   a name; its set-up function is found by that name with "_init" after
//   it, on the clip or any clip above it up to the root. The movie makes
//   them all in one function (SetupOptionsMenuInitFunctions), each
//   r2.<name>_init = OptionsUtils.MakeOptionToggleInit(option, label,
//   tooltip), register 2 holding where they are kept.
// - A checkbox made by that helper sends "ClientOptions.
//   SetClientOptionMessage <option> 1" (or 0) when clicked, and is drawn
//   ticked or not by what the token "ClientOptions.<option>" is told to it
//   to be.
// - The tab's own headings ("Gameplay", "Feedback") are each a clip of two
//   things: a gold bar, a shape 437 wide and 48 high, and over it at (24,
//   13) a text clip, the symbol exported as txt_Game (or txt_Feedback),
//   whose text is "$LOC_GAMEPLAY" until the movie's script puts the
//   language's words in. A bar is 15 to the left of the left column's
//   checkboxes and 37 of the right's, and 69 above its first control.
//
// - A choice of a few named values is the movie's slider with named stops,
//   the Video tab's OptTextureQuality_mc and OptGraphicsDetail_mc (one
//   symbol with the volume and zoom sliders), set up by
//   OptionsUtils.MakeSliderPickerInit(option, label, tooltip, new
//   Array(names...)). It asks the option's value at once with
//   ExternalInterface's GetValue, is told it after as a checkbox is, and
//   sends "ClientOptions.SetClientOptionMessage <option> <index>" when it
//   moves. On the tabs whose checkboxes stand at x 7 the sliders stand
//   about 40 to the right of them, 40 to 46 apart: two rows.
//
// - A choice can be a dropdown instead: the symbol the movie exports as
//   Picker, the Game tab's own OptDifficulty_mc, OptPlotAssist_mc and
//   OptSubtitles_mc (read 2026-10-11, guiexport.erf's movie and
//   patch003.erf's alike), set up by OptionsUtils.MakePickerInit(option,
//   label, tooltip, new Array(names...), null,
//   OptionsMenu.PickEntryCallBack): the null is the entries' tooltips, and
//   the callback sends the same SetClientOptionMessage a slider does. It
//   asks its value with GetValue as its button is made, and is told it
//   after as a checkbox is. The tab's own stand 4 to the right of the
//   column's checkboxes and 60 apart: two rows. An open list is a clip of
//   the picker's own, so it is drawn at the picker's depth: the game places
//   its pickers deepest of the tab, the upper deeper than the lower, and an
//   open list covers what is under it.
//
// So a section is added by placing a heading and checkboxes in the Game
// tab's sprite, in the rows it has free, and adding their set-up functions
// before the first of the game's own. The heading is the game's: its bar,
// and its heading text clip with a set-up function that sets the title.
// A bar is higher than a row, so a heading takes two.
//
// (The first heading was the key list's, KeyBindingHeader: plain text
// centred over the column. In play, 2026-10-01, it read as a caption
// beside the game's bars.)
//
// - The Video tab (read 2026-10-11, DAOLib #79) is a clip of two clips, one
//   a column: the left one, 50 lower than the right, ends in the checkbox
//   OptVSync_mc at (3, 302), and the right in OptFBFX_mc at (35, 267). On
//   the screen the columns stand where the Game tab's do, to a pixel, and
//   under the same two bars ("Screen", "Detail"), so a section is placed
//   there as on the Game tab, each column in its own clip. Its sliders
//   stand 41 right of the left column's checkboxes and 37 of the right's,
//   its dropdown (OptResolution_mc) 6. As the game has it the tab is set
//   wide, 99 from the dropdown to the checkbox under it and 106 from the
//   last detail slider to its checkbox, and has five rows free, under the
//   right column. So for a section the tab is laid out again (DAOLib #82):
//   the left column's controls closer, the "Detail" bar, which the main
//   timeline places, under them with the right column's four controls, and
//   the right column, its clip empty, a section's from the bars' height to
//   above Restore Defaults: fourteen rows, a heading and twelve.
//
// - The Controls tab's key list is built in script: AddHeader(text) for a
//   section and AddKeyBinding("TogglePause") for each action, the last of
//   them HideMainInterface. A plugin's actions are listed by the same
//   calls added after that one.
#pragma once

#include "dao/core/GfxMovie.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::options_movie
{

// A checkbox: the option it sends and is told the value of (letters and
// digits), its label, and what it says when the mouse rests on it. UTF-8.
struct Toggle
{
    std::string option;
    std::string label;
    std::string tooltip;
};

// A choice: the option it sends the index of and is told it, its label,
// its tooltip, and its values' names, two or more. UTF-8. It is the game's
// slider with a stop for each value, or with `dropdown` the game's dropdown.
struct Choice
{
    std::string option;
    std::string label;
    std::string tooltip;
    std::vector<std::string> values;
    bool dropdown = false;
};

// The tab a section is on.
enum class Tab : std::uint8_t
{
    Game,
    Video,
};

// A section that takes whichever column has room first.
inline constexpr std::int32_t kEitherColumn = -1;

// A section's rows: its checkboxes, then its choices.
struct Section
{
    std::string title;
    std::vector<Toggle> toggles;
    // `= {}` so that a section written {title, toggles} leaves it out
    // without clang's missing-field-initializers.
    std::vector<Choice> choices = {}; // NOLINT(readability-redundant-member-init)
    // The column the section would be in, 0 the left or 1 the right, if that column has the
    // rows; kEitherColumn for the first that has. `= ...` for the same reason.
    std::int32_t column = kEitherColumn; // NOLINT(readability-redundant-member-init)
    Tab tab = Tab::Game;                 // NOLINT(readability-redundant-member-init)
};

// A row of a tab the game leaves empty: where a checkbox in it goes, in the
// clip its column is.
struct Slot
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t column = 0; // 0 the left, 1 the right

    bool operator==(const Slot &) const = default;
};

// How many rows a heading takes, and a choice.
inline constexpr std::size_t kHeadingRows = 2;
inline constexpr std::size_t kChoiceRows = 2;

// A tab's free rows, in the order they are filled: down the left column
// from under its last checkbox, then down the right, but for rows something
// is placed in already; the Video tab's are the right column's whole, as a
// section finds the tab. Each plugin links its own copy of the library and
// each copy patches the movie the others have patched, so a row another
// plugin's section took is taken. Nothing if the movie's tab is not as
// read.
std::optional<std::vector<Slot>> FreeSlots(std::span<const std::byte> movie, Tab tab = Tab::Game);

// The name a control for an option is placed under, a checkbox or a
// choice, and the name of a heading's text in a row: by the tab and the
// row, so that no two plugins' headings share one.
std::string ControlInstance(std::string_view option);
std::string HeadingInstance(const Slot &slot, Tab tab = Tab::Game);

// The movie with as many of the sections as their tabs have rows for, each
// a heading, its checkboxes and its choices in rows one under the other in
// one column, a section never split or begun without room for all of it.
// With a section for the Video tab among them, that tab's own controls are
// first put in its left column, set closer, which leaves the right column
// to sections; once, however many copies of the library patch the movie.
// `shown` says of each section whether it went in: on a tab the first
// section left out for want of room keeps those after it out too, so that
// sections stand in the order they were given; another tab's are not kept
// out by it. Nothing, and none shown, if the movie is not as read or no
// section fits. The dropdowns are placed deepest of their clip, an upper
// one deeper than a lower, so that an open list covers what these sections
// put under it.
std::optional<gfx::Bytes> AddSections(std::span<const std::byte> movie, std::span<const Section> sections,
                                      std::vector<bool> &shown);

// Actions for the Controls tab's key list, under a heading.
struct KeySection
{
    std::string title;
    std::vector<std::string> actions; // the engine's names for them
};

// The movie with the sections' actions listed after the game's own, each
// section under its heading, in order. A section whose heading the list
// has already (another plugin's copy of the library put it there) has its
// rows put under that heading and no second one made. Nothing if the list
// is not as read or there is no action to add.
std::optional<gfx::Bytes> ListKeys(std::span<const std::byte> movie, std::span<const KeySection> sections);

} // namespace dao::core::options_movie
