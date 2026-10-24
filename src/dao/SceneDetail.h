// The scene read afresh, with nothing kept, for the probe to check dao::scene's cache against. Not an
// API.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Scene.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dao::detail
{

// Returns what scene::Objects returns, read from the engine whole at this call.
std::vector<scene::Object> FreshSceneObjects();

// Returns each graphics object of the area's game objects and running effects, with the game object's
// id; for naming the game object a graphics object is of. Empty with no game. On the game thread.
std::unordered_map<std::uintptr_t, ObjectId> GraphicsOwners();

} // namespace dao::detail
