// What of dao/Input.cpp the library's other parts use: the game's fly camera's own inputs
// withheld while dao/Camera.cpp's free view holds the mode, apart from what a plugin withholds
// (dao::input::Withhold). Not an API.
#pragma once

namespace dao::input::detail
{

// Withholds the game's fly camera's own keys and mouse (inputs 28 to 42), or gives them back.
void WithholdFlyCamera(bool withheld);

} // namespace dao::input::detail
