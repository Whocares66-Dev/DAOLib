// What the library's parts share of a creature's model: its GraphicsStructures::Object, reached
// as the engine reaches it (dao/World.cpp has the reading). Not an API.
#pragma once

#include <cstdint>

namespace dao
{
class CCObject;
}

namespace dao::detail
{

// Returns a creature's graphics object, or 0 for none. Call on the game thread.
std::uintptr_t GraphicsObjectOf(const CCObject *creature);

} // namespace dao::detail
