// Whether the game's address space is running short: what the fixes that
// keep things loaded ask before they keep more (AreaLists, AreaRooms).
#pragma once

namespace fix
{

// True when no free block of the address space is 512 MiB or more. What a
// fix keeps is paid for there, and is only worth keeping while the game has
// room to load the next area beside it. Cheap: the system is asked for such
// a block and given it straight back.
bool AddressSpaceShort();

} // namespace fix
