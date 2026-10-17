// The probe's watch on the colour curve: the texture the engine gives the
// frame's last pass, da_hsvmatrix's hslmatrix.psh, as its internal parameter
// ColorCurveTexture on sampler s1 (hslmatrix.mat.xml). No file holds it and
// where the exe fills it was not found (dev/TODO.md), so it is read as the
// device is handed it: every texture one to four texels tall bound to
// sampler 1, its size, format, pool and usage, and its texels whenever they
// change, looked at every two seconds. For a vanilla reading, with the
// gamma and brightness options moved between readings (DAOParagonEngine's
// parity picture).
#pragma once

namespace probe
{

// At plugin load, before the game makes its device.
void AttachColourCurveWatch();

} // namespace probe
