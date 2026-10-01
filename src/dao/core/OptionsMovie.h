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

struct Section
{
    std::string title;
    std::vector<Toggle> toggles;
};

// A row of the Game tab the game leaves empty: where a checkbox in it goes.
struct Slot
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t column = 0; // 0 the left, 1 the right

    bool operator==(const Slot &) const = default;
};

// How many rows a heading takes.
inline constexpr std::size_t kHeadingRows = 2;

// The free rows, in the order they are filled: down the left column from
// under its last checkbox, then down the right, but for rows something is
// placed in already. Each plugin links its own copy of the library and
// each copy patches the movie the others have patched, so a row another
// plugin's section took is taken. Nothing if the movie's Game tab is not
// as read.
std::optional<std::vector<Slot>> FreeSlots(std::span<const std::byte> movie);

// The name a checkbox for an option is placed under, and the name of a
// heading's text in a row: by the row, so that no two plugins' headings
// share one.
std::string ToggleInstance(std::string_view option);
std::string HeadingInstance(const Slot &slot);

// The movie with as many of the sections as there are rows for, each a
// heading and its checkboxes in rows one under the other in one column, a
// section never split or begun without room for all of it; `shown` is how
// many went in, from the first. Nothing, and
// `shown` 0, if the movie is not as read or no section fits.
std::optional<gfx::Bytes> AddSections(std::span<const std::byte> movie, std::span<const Section> sections,
                                      std::size_t &shown);

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
