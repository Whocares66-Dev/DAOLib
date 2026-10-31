// Dynamic occlusion: Umbra's occlusion culling made cheaper where it pays, and
// left out where it costs more than it saves (DAOLib issue #281, of #279).
//
// Umbra decides what the camera can see from the level's cells and, for the
// objects it is unsure of, occlusion queries: the GPU is asked how many pixels
// of an object's box would be drawn. Two things are done with it, both on the
// update thread, which limits the frame (#279):
//
// - Delay. The engine's commander asks for each query's answer as soon as Umbra
//   wants it, and the update thread spins until the GPU has caught up. When the
//   GPU has not answered, the answer the same box had in the last two frames is
//   given instead; an object that moved, or one with no answer that recent, is
//   waited for as before. Denerim's market, vsync off: 7.26 to 6.56 ms a frame
//   (+11%). Standing still no answer given differed from the GPU's; turning the
//   camera, an object coming into view can be drawn a frame late, less than the
//   level of detail's own pop-in (the user, 2026-10-31: "I can't see any issue").
// - Choice. Culling by occlusion costs its search, an occluder pass and the
//   queries; where little is hidden (the Brecilian Forest, 6.3 ms a frame with
//   it, 4.4 without) that is more than it saves, and where much is (Denerim's
//   market, 6.3 ms with it, 8.0 without) far less. What is hidden is never
//   seen either way, so the picture is the same: a paused frame in Denerim
//   differed only in the HUD's blinking text and the hovered companion. So
//   after each area's load both ways are timed, 60 frames each, and the one
//   with frames 5% shorter kept until the next load, not changed while the
//   party moves about (the user, 2026-10-31); view-frustum culling alone is
//   Umbra's camera given properties 1 for 3.
//
// "Dynamic occlusion culling" on the options screen's Performance section, kept in
// daolib.ini beside the logs, on unless the player turns it off; off, Umbra
// runs as it always did. DAOLIB_DYNAMIC_OCCLUSION=0 in the environment leaves
// it out, the switch too.
#pragma once

#include "dao/Options.h"

#include <optional>

namespace perf
{

// Once, from the loader's way into the game, before the plugins load: every
// game, whatever plugins it has.
// @return the switch for the options screen, or nothing if the part is left out
std::optional<dao::options::Toggle> InstallDynamicOcclusion();

} // namespace perf
