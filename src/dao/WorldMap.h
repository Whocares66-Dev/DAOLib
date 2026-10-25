// The world map: its pins with their states, where the party is, and travel to a pin as a click
// on it travels, refused where the map would refuse the click.
//
//   for (const auto &pin : dao::worldmap::Pins())
//       if (pin.state == dao::worldmap::PinState::Active && pin.name == L"Lothering")
//           dao::worldmap::Travel(pin.index);
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dao::worldmap
{

// A pin's state, as the scripts set it (WM_LOCATION_*) and the map draws it.
enum class PinState : std::int32_t
{
    Inactive = 1,  // not drawn
    Active = 2,    // the only state a click travels to
    GrayedOut = 3, // drawn grey
    Unlocked = 4,  // drawn, not yet travelled to; the map's "unlockable"
    Complete = 5,  // drawn, its quest done
    Destroyed = 6, // drawn with a skull
};

// A pin of the world's map.
struct Pin
{
    std::int32_t index = 0; // its place in the map's list, which Travel takes
    std::wstring tag;       // the pin's own tag
    std::wstring areaTag;   // the area it leads to
    std::wstring name;      // as the map shows it
    PinState state = PinState::Inactive;
};

// Returns the pins of the world's map, in the map's order, whether or not it is shown.
// @return none outside a module, or for a campaign with no world map
//
// Call it, and the rest, on the game thread.
std::vector<Pin> Pins();

// Returns the area tag of the pin the party is at, as the map marks it.
std::optional<std::wstring> Here();

// Why a travel was refused, or None if it went.
enum class Refusal : std::uint8_t
{
    None,
    NotShown,  // the world map is not on screen
    ReadOnly,  // the map is shown only to be looked at, or the party is already travelling
    NoPin,     // the map has no pin at the index
    NotActive, // the pin is not Active: the map draws it, if at all, as one not to click
};

// Travels to a pin as a click on it does: the module's travel script then runs, with its random
// encounters, and the map closes. Refused unless the map is shown for travel and the pin is
// Active. A pin at the party's own area closes the map, as the script does.
Refusal Travel(std::int32_t index);

} // namespace dao::worldmap
