// The area lists the party leaves are kept, and handed back when it returns
// (DAOLib issue #64). A part of fast load (perf/FastLoad.h).
//
// The game's areas come in lists, each a file of its own, and a door or a
// map trip to an area of another list is the long loading screen: the
// engine saves each area of the list it leaves, deletes the list, makes the
// new one, reads which areas it holds and builds them one a frame
// (dao/Addresses.h, "The loading mode's states" and "The module's area
// list"). Most doors are such: of the single-player module's 190 lists, 140
// hold one area. In play a swap to a list that was there already took 0.8
// to 1.3 s where the engine's own took 1.8 to 2.1.
//
// So the lists left are kept, the newest first, eight at the most and fewer
// when the address space runs short (the largest free block under 512 MiB
// has the oldest deleted):
//
// - At the swap's step 5 the engine asks the module to drop its list. The
//   list is taken out of the module instead and kept, with the RIM tree the
//   module's resources had loaded for it.
// - At step 6, before the engine makes the new list, the lists kept are
//   asked for the area the party is going to (the engine's own FindArea).
//   One that has it goes back into the module, where the engine finds a
//   list made and makes none; its read is answered as done, and the
//   engine's build finds every area built. The list just left is kept, and
//   the oldest deleted, as the engine would have, when there is one too
//   many.
// - At step 7 the engine unloads the RIM tree of the list left, and waits
//   ten seconds for a tree a kept list's areas never let go. So a kept
//   list's tree stays loaded, and is unloaded when its list is deleted in a
//   swap, once the engine has loaded the new one. The tree of the area
//   asked for is made the manager's current one at every swap, since the
//   engine does nothing where it finds that tree loaded, and the next
//   transition takes the current tree for the list the party is in.
// - A list being made that holds an area of a kept one has that one
//   deleted first: the engine refuses an area it already has.
// - A game starting or a save loading, or the module dropping its list for
//   any reason but a swap, deletes what is kept.
//
// What it leaves as it is: a kept list's objects stay in the module's table
// of tags, so a script's search by tag can meet one (in play the same
// character standing in three kept lists was spoken to in the right one);
// and a tree whose list is deleted outside a swap stays loaded. Read
// 2026-10-10 and 11; in play 2026-10-11, 27 loading screens over six lists
// with four kept.
#pragma once

#include "perf/fastload/Shared.h"

namespace perf::fastload::lists
{

// Places the detours. DAOLIB_AREA_LISTS=<n> in the environment keeps n lists
// at the most in place of eight; 0 keeps none, the engine as it was.
void Attach();

// Before and after an update of the loading mode: what the mode is doing is
// what says which of the module's calls is a swap's.
void BeforeUpdate(const Update &update);
void AfterUpdate();

} // namespace perf::fastload::lists
