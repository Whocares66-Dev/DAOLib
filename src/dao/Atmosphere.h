// The look of the area the player is in, read and set: its sky (the sun's disc, the air, the
// clouds, the fog and the moon) and its sunlight (the light on the ground and the creatures,
// and the way it and their shadows fall). The sky is the script commands SetAtmosphere,
// SetAtmosphereRGB and ResetAtmosphere with the reading they lack; no command reaches the
// sunlight.
//
//   const auto sky = dao::atmosphere::AreaOwnSky();
//   const auto sun = dao::atmosphere::AreaOwnSunlight();
//   if (sky && sun) {
//       dao::atmosphere::SetSky(dao::atmosphere::WithFog(*sky, dao::atmosphere::Preset::Night));
//       auto moonlit = *sun;
//       moonlit.color = moonlit.characterColor = {0.27F, 0.41F, 0.79F, 1};
//       dao::atmosphere::SetSunlight(dao::core::Blend(*sun, moonlit, 0.5F));
//   }
//   dao::atmosphere::ResetSky();
//   dao::atmosphere::ResetSunlight();
//
// Both are the area's: a new area comes with its own, and what was set is gone. A change shows
// in the next frame. The sky is what is drawn above and the haze in the distance, and lights
// nothing. Trees, grass and buildings keep the light their level was made with, whatever the
// sunlight, and lights placed in the level (torches, windows) are not part of either.
#pragma once

#include "dao/core/Sky.h"

#include <cstdint>
#include <optional>

namespace dao::atmosphere
{

// Every value of a sky; dao/core/Sky.h has the members.
using Sky = core::Sky;

// A row of the game's own sky tables (atmosphere, clouds and fog): Ostagar's skies.
enum class Preset : std::int32_t
{
    Default = 0,
    Battle = 1,
    Night = 2,
    Lightning = 3,
};

// Returns the sky as it is drawn now.
// @return nullopt before a game is loaded or with nobody controlled
//
// Call it on the game thread, as with all of these.
std::optional<Sky> CurrentSky();

// Returns the area's own sky, the one ResetSky returns to.
// @return nullopt as for CurrentSky
std::optional<Sky> AreaOwnSky();

// Sets the whole sky.
// @return false, with nothing changed, where CurrentSky would return nullopt
bool SetSky(const Sky &sky);

// Returns the sky to the area's own.
void ResetSky();

// The level's sunlight; dao/core/Sky.h has the members.
using Sunlight = core::Sunlight;

// Returns the sunlight as it falls now.
// @return nullopt as for CurrentSky
std::optional<Sunlight> CurrentSunlight();

// Returns the area's own sunlight, the one ResetSunlight returns to.
// @return nullopt as for CurrentSky
std::optional<Sunlight> AreaOwnSunlight();

// Sets the sunlight.
// @return false, with nothing changed, where CurrentSky would return nullopt
bool SetSunlight(const Sunlight &light);

// Returns the sunlight to the area's own.
void ResetSunlight();

// Returns `sky` with the sun, the air and the moon of a row of the game's atmosphere table.
// @return `sky` unchanged if the table has no such row; a value the row leaves out is kept
Sky WithAir(Sky sky, Preset preset);
// Returns `sky` with the clouds of a row of the game's clouds table. Works like WithAir.
Sky WithClouds(Sky sky, Preset preset);
// Returns `sky` with the fog of a row of the game's fog table. Works like WithAir.
Sky WithFog(Sky sky, Preset preset);

} // namespace dao::atmosphere
