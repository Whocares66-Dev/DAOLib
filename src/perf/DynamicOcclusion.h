// Dynamic occlusion: Umbra's occlusion culling left out where it costs more
// than it saves (DAOLib issues #281 and #285, of #279).
//
// Umbra decides what the camera can see from the level's cells and, for the
// objects it is unsure of, occlusion queries: the GPU is asked how many pixels
// of an object's box would be drawn. That costs its search, an occluder pass
// and the queries; where little is hidden (the Brecilian Forest, 6.3 ms a frame
// with it, 4.4 without) that is more than it saves, and where much is
// (Denerim's market on an RTX 3090, 6.3 ms with it, 8.0 without) far less. On a
// slower GPU it costs more: the 3090 locked to 600 MHz, Denerim's market too ran
// faster without it (9.9 ms against 7.8). What is hidden is never seen either
// way, so the picture is the same: a paused frame in Denerim differed only in
// the HUD's blinking text and the hovered companion. So after each area's load
// both ways are timed, 60 frames each, and the one with frames 5% shorter kept
// until the next load, not changed while the party moves about (the user,
// 2026-10-31); view-frustum culling alone is Umbra's camera given properties 1
// for 3.
//
// #281 also gave a query the GPU had not yet answered the same box's answer from
// the frame before, so as not to wait for it. It was taken out (#285): measured
// with the party running a route, it gained nothing, on the 3090 (Denerim,
// six rounds, 5.78 ms against vanilla's 5.80) or at 600 MHz (Orzammar, 91 fps
// against 92), since the update thread seldom waited on the GPU.
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
