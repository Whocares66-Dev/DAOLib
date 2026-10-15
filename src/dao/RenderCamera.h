// The renderer's camera the scene is drawn through, shared by dao/World.cpp (the world on the
// screen) and dao/FieldOfView.cpp (its field of view). Not an API.
#pragma once

namespace dao::detail
{

// Returns the scene's Renderer::PerspectiveCamera, null with none or off the game thread.
void *RenderCamera();

} // namespace dao::detail
