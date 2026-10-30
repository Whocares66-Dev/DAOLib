// What of dao::options' plugin controls the options screen's driving (OptionsScreen.cpp) reads.
// Not an API.
#pragma once

#include "dao/Options.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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

} // namespace dao::detail
