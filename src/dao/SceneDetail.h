// The scene read afresh, with nothing kept, for the probe to check dao::scene's cache against. Not an
// API.
#pragma once

#include "dao/Scene.h"

#include <vector>

namespace dao::detail
{

// Returns what scene::Objects returns, read from the engine whole at this call.
std::vector<scene::Object> FreshSceneObjects();

} // namespace dao::detail
