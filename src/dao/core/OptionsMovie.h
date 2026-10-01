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
// - A section's heading in the key list is the exported symbol
//   KeyBindingHeader, whose text clip, named KeyBindingHeader_mc, has its
//   text set by its own set-up function.
//
// So a section is added by placing a heading and checkboxes in the Game
// tab's sprite, in the rows it has free, and adding their set-up functions
// before the first of the game's own.
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

// A row of the Game tab the game leaves empty.
struct Slot
{
    std::int32_t x = 0;
    std::int32_t y = 0;
};

// The free rows, in the order they are filled: down the left column from
// under its last checkbox, then down the right, but for rows something is
// placed in already. Each plugin links its own copy of the library and
// each copy patches the movie the others have patched, so a row another
// plugin's section took is taken. Nothing if the movie's Game tab is not
// as read.
std::optional<std::vector<Slot>> FreeSlots(std::span<const std::byte> movie);

// The name a checkbox for an option is placed under, and the name of a
// heading in a row: by the row, so that no two plugins' headings share one.
std::string ToggleInstance(std::string_view option);
std::string HeadingInstance(const Slot &slot);

// The movie with as many of the sections as there are rows for, each a
// heading and its checkboxes, a section never split or begun without room
// for all of it; `shown` is how many went in, from the first. Nothing, and
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
