// The world map and the Fade's: their pins with their states, where the party is, travel to a pin
// as a click on it travels, refused where the map would refuse the click, and a city's two maps
// swapped.
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

// Which map is on screen.
enum class MapKind : std::uint8_t
{
    None,  // neither
    World, // the world map, or a city's (worldmap.gfx)
    Fade,  // the Fade's, in the Broken Circle (fademap.gfx)
};

// The map on screen, and what the world map's toggle would swap it for.
struct Shown
{
    MapKind kind = MapKind::None;
    std::int32_t type = 0;  // its row of the worldmaps table (WM_*), as the scripts set it primary
    std::int32_t other = 0; // the second map's row, 0 when there is none to swap to
};

// Returns the map on screen.
//
// Call it, and the rest, on the game thread.
Shown ShownMap();

// A pin's state, as the scripts set it (WM_LOCATION_*) and the map draws it.
enum class PinState : std::int32_t
{
    Inactive = 1,  // not drawn
    Active = 2,    // a click travels to it
    GrayedOut = 3, // drawn grey
    Unlocked = 4,  // drawn, not yet travelled to; the map's "unlockable"
    Complete = 5,  // drawn, its quest done; the Fade's map travels to it too
    Destroyed = 6, // drawn with a skull
};

// A pin of a map.
struct Pin
{
    std::int32_t index = 0; // its place in the map's list, which Travel takes
    std::wstring tag;       // the pin's own tag
    std::wstring areaTag;   // the area it leads to
    std::wstring name;      // as the map shows it
    PinState state = PinState::Inactive;
};

// Returns the pins of the map on screen, in its order: the Fade's when that is up, otherwise the
// world's primary map, whether or not it is shown.
// @return none outside a module, or for a campaign with no world map
std::vector<Pin> Pins();

// Returns the area tag of the pin the party is at, on the map Pins reads, as the map marks it.
std::optional<std::wstring> Here();

// Why a travel or a swap was refused, or None if it went.
enum class Refusal : std::uint8_t
{
    None,
    NotShown,  // no map is on screen (for a swap, the world map is not)
    ReadOnly,  // the world map is shown only to be looked at, or the party is already travelling
    NoPin,     // the map has no pin at the index
    NotActive, // the pin is not one the map's click travels to
    NoOther,   // there is no second map to swap to
};

// Travels to a pin as a click on it does: the module's travel script then runs, with its random
// encounters, and the map closes. On the world map, refused unless the map is shown for travel
// and the pin is Active; a pin at the party's own area closes the map, as the script does. On the
// Fade's, refused unless the pin is Active or Complete.
Refusal Travel(std::int32_t index);

// Swaps a city's two maps as the world map's toggle does: the second becomes the one shown, and
// Pins, Here and Travel then read it.
Refusal SwapMaps();

} // namespace dao::worldmap
