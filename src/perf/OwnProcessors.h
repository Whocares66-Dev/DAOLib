// Each copy of the game on processors of its own (DAOLib issue #89).
//
// GOG's DAOrigins.exe carries a process affinity mask of 3 in its PE load
// configuration (the image's ProcessAffinityMask; EA's and Steam's carry
// none), so Windows starts every copy of it on the machine's first two
// logical processors, whatever started it. The game's thread manager then
// counts the processors it was given and makes that many worker threads
// less one. One copy is as GOG meant it. Several copies (DAOLib #67) share
// those two processors, some sixty threads each, while the rest of the
// machine idles: seen 2026-10-12 on ten cores of two threads, three and
// four copies each at 0x3 with the machine's CPU under 20%, and frame
// threads stalled for seconds in a yield loop waiting on their own task
// threads (seventy hang logs in one instance's day).
//
// So a copy that finds another copy on its processors moves to a group of
// its own: as many processors as the image gave it, the first such group no
// other copy runs on (dao/core/Processors.h). Before the game's code runs,
// so that the thread manager counts what the copy has. A copy alone stays
// where the image put it, and a copy the image did not confine is left
// alone.
#pragma once

namespace perf
{

// Once, from the loader's way into the game, before the game's own code
// runs. DAOLIB_OWN_PROCESSORS=0 in the environment leaves the copy on the
// processors it was started on.
void InstallOwnProcessors();

} // namespace perf
