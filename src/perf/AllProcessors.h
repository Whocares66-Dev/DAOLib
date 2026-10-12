// The game on every processor the machine has (DAOLib issues #89, #92).
//
// GOG's DAOrigins.exe carries a process affinity mask of 3 in its PE load
// configuration (the image's ProcessAffinityMask; EA's and Steam's carry
// none), so Windows starts it on the machine's first two logical
// processors, whatever started it and before any of its code runs. One copy
// of the game then has one core of a machine with ten. Several copies
// (DAOLib #67) share that one core, some sixty threads each, while the rest
// of the machine idles: seen 2026-10-12, a running copy answering nothing
// for 5.7 s while another started, and frame threads stalled for seconds
// in a yield loop on their own task threads.
//
// So the process is given the machine's processors, before the game's code
// runs: what EA's and Steam's builds of the same game have from the start.
// The game's thread manager (Eclipse::DAThreadManager, 0x004017D0) counts
// the processors it has and makes that many worker threads less one, as it
// was written to, and Windows puts each thread on a processor that is free.
// The game pins no thread of its own (its thread class sets no affinity).
#pragma once

namespace perf
{

// Once, from the loader's way into the game, before the game's own code
// runs. DAOLIB_ALL_PROCESSORS=0 in the environment leaves the process on
// the processors it was started on.
void InstallAllProcessors();

} // namespace perf
