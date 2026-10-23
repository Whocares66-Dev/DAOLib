// The probe's check of dao::scene (DAOLib #244): every few seconds, the scene's objects and how long
// reading them took, counted by kind, the rooms' own models that carry emitters (#242's fires), and
// the visual effects RunningEffects lists compared with the scene's objects they own, one by one.
#pragma once

namespace probe
{

// Starts the check on the frame callback. At plugin load only.
void AttachSceneCheck();

} // namespace probe
