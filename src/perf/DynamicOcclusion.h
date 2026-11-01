// Dynamic occlusion: Umbra's occlusion culling left out where it costs more
// than it saves (DAOLib issues #281, #285 and #286, of #279).
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
// the HUD's blinking text and the hovered companion. What pays depends on what
// the camera sees, which changes as it turns and the party moves, even within one
// room: in Ostagar the spot the party loads in ran 15% faster without occlusion
// culling and the camp a few metres on 15-25% slower. So the choice is made all
// the time (#286): once a second a few frames are run the other way between
// frames run the way in use, the comparisons are smoothed by exponential decay,
// and the other way is taken when it is clearly faster, with the same margin to
// go back, so the way does not flip to and fro. A frame is timed less the
// update thread's wait for the render thread, so under vsync or a driver's
// frame cap, which hold every frame to the same length, the frame's own work is
// still compared. View-frustum culling alone is Umbra's camera given properties
// 1 for 3.
//
// A choice made once an area, from where the party loaded in (#285), lost 6% in
// Ostagar; one made once a room the party stands in still lost 3%, an outdoor
// room being a cell of terrain whose views differ as much. Probing at once when
// the frames changed, with probes in turn, medians and a dwell after a switch,
// did worse than a probe a second: the frame time of a party running moves 10%
// every second or two, so the bursts never stopped (the forest +13% against
// +22%), and Ostagar's switch back took as long (#286's comments).
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
