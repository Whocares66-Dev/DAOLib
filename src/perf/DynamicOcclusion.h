// Dynamic occlusion: Umbra's occlusion culling left out while it costs more
// than it saves for what the camera sees (DAOLib issues #281, #285 and #286, of
// #279). What is hidden is never seen either way, so the picture is the same.
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
