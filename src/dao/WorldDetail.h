// What other parts of the library read of the current area through dao/World.cpp: its rooms and
// its game objects (dao/Scene.cpp's walk of the scene). Not an API.
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <vector>

namespace dao::detail
{

// Returns the current area's rooms' addresses, in its layout's order; none off the game thread.
std::vector<std::uintptr_t> RoomPointers();

// Returns the ids of the current area's game objects; none off the game thread.
std::vector<ObjectId> AreaObjects();

} // namespace dao::detail
