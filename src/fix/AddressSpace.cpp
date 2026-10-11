#include "fix/AddressSpace.h"

#include <Windows.h>

#include <cstddef>

namespace fix
{

// Notes on AddressSpaceShort: it was a walk of the whole address space with VirtualQuery,
// looking for a free block of the size. In play, 2026-10-11, with the engine's two to three
// million blocks live in the system's heap and the walk asked once a room, the preload's worst
// frame was 65 to 87 ms, where a run without the walk had 0 to 16; and a door's layout swap
// had 85 to 94 ms inside its update where it had had 56 to 58. The walk itself was not timed.
// The system knows whether it has such a block without being walked: a reservation of the
// size is tried and given straight back. Nothing is committed.
bool AddressSpaceShort()
{
    constexpr std::size_t kRoomWanted = 512U << 20;
    void *room = VirtualAlloc(nullptr, kRoomWanted, MEM_RESERVE, PAGE_NOACCESS);
    if (room == nullptr)
    {
        return true;
    }
    VirtualFree(room, 0, MEM_RELEASE);
    return false;
}

} // namespace fix
