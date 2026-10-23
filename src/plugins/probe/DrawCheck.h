// The probe's check of dao::render (DAOLib #246): every ten seconds one frame's draws, counted by call,
// with how many have shaders whose constant tables read, how many constants and textures by name
// they carry, one draw's constants laid out, and the armed frame's time beside the frames around it.
#pragma once

namespace probe
{

// At plugin load, before the game makes its device.
void AttachDrawCheck();

} // namespace probe
