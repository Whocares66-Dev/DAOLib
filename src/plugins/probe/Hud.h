// The probe's HUD watch (DAOLib #136): the HUD's movie records whole, logged when they change, to
// find what holds the interface's fade when the game leaves explore. Every half second on the game
// thread it copies the 0x108 bytes of the records of the HUD's movies (Portraits, Minimap,
// ArmyControl, NavBar, Quickbar, MainUI) and logs, for each that changed, the dwords that differ
// from the last copy, with the game's mode and pause. It changes nothing.
#pragma once

namespace probe
{

// At plugin load.
void AttachHudWatch();

} // namespace probe
