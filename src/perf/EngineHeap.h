// The engine's memory from the system's heap, not from its own pools
// (DAOLib issue #69).
//
// The engine allocates from pools of its own (dao/Addresses.h, "The
// engine's pools"). They take 850 MiB from the process heap at the first
// allocation and never more, so that is all the memory the engine has
// whatever the machine has; and each allocation walks a free list for the
// best fit, the whole of it unless a block fits exactly: in play, hundreds
// of blocks an allocation for the sizes under 256 bytes that are nearly all
// of them, and over a third of the time of the loading steps that build an
// area list or swap a layout (DAOLib #68, #69).
//
// Everything outside the pool classes reaches them through three functions,
// nothing outside reads a block's header, and no pool is reset or destroyed
// in play (the audit in #69). So the three are replaced whole, before the
// first allocation, with a heap of the loader's own, the system's. The
// engine's memory manager is then never built: its 850 MiB is not taken
// and its ceiling is gone. What the engine's own did without a word, this
// does too: a free or a reallocation of a pointer that is not one of its
// blocks, or of a block already freed, is left alone. Read 2026-10-11.
//
// One more thing the pools did without a word is kept (DAOLib #78): a read a
// little way past the end of a block found memory there, the pool's own.
// Here it can find none, at the end of what the heap has mapped. Such a
// read has its page made readable and goes on, and is said in the log
// with the code that read, to be corrected where it is.
#pragma once

namespace perf
{

// Once, from the loader's way into the game, before the plugins load and
// before the engine's first allocation: every game, whatever plugins it
// has. DAOLIB_ENGINE_HEAP=0 in the environment leaves the engine's own
// pools in place.
void InstallEngineHeap();

} // namespace perf
