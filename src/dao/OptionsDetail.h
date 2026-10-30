// What of dao::options' plugin controls the options screen's driving (OptionsScreen.cpp) reads.
// Not an API.
#pragma once

#include "dao/Options.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::detail
{

// A plugin's control placed on the screen, as a click on it is sent.
struct OptionControl
{
    std::string option; // its name in the movie: L"DAOLib" and the plugin's name
    options::Kind kind = options::Kind::Toggle;
    std::int32_t values = 0; // a choice's count of values; 2 for a checkbox, 0 for a button
    options::Tab tab = options::Tab::Game;
};

// Returns the control a plugin named so, if its section was placed on the screen.
std::optional<OptionControl> PlacedOption(std::wstring_view name);

// Returns the plugins' controls placed on a tab, whether or not the screen is open.
std::vector<options::EntryState> PluginEntries(options::Tab tab);

// Has the options movie patched as it loads (the plugins' sections and the recorder of the game's
// own controls), once. False where the game is not one the library knows.
bool AttachOptions();

} // namespace dao::detail
