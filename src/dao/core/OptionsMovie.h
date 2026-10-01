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
// under its last checkbox, then down the right. Nothing if the movie's
// Game tab is not as read.
std::optional<std::vector<Slot>> FreeSlots(std::span<const std::byte> movie);

// The name a checkbox for an option is placed under, and a heading's.
std::string ToggleInstance(std::string_view option);
std::string HeadingInstance(std::size_t section);

// The movie with as many of the sections as there are rows for, each a
// heading and its checkboxes, a section never split or begun without room
// for all of it; `shown` is how many went in, from the first. Nothing, and
// `shown` 0, if the movie is not as read or no section fits.
std::optional<gfx::Bytes> AddSections(std::span<const std::byte> movie, std::span<const Section> sections,
                                      std::size_t &shown);

} // namespace dao::core::options_movie
