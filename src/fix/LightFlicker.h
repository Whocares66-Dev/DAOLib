// A flickering light eases from one brightness to the next, as the engine
// meant, rather than jumping (DAOLib issue #150).
//
// A light that flickers (fires, torches: 918 of the game's 9,024 room
// lights) has an Animation::RandomWaveController<float>, whose Update runs
// each frame. A cycle of about 0.05 to 0.1 s goes from one brightness to a
// random next one, and within it Update means to set the light between the
// two by a smoothstep of the cycle's share gone. Its clamp of that share to
// [0, 1] keeps the 0 it loads instead, in every case, so the light holds
// the cycle's first brightness to its end and then jumps to the next: in
// Haven's village house (orz106d) the frame's mean swings 15 to 17 of 255
// within 0.2 s. EA's build has the same code, Steam's is GOG's.
//
// Update is detoured. A frame that starts a new cycle goes to the engine's
// own, so its random draws are the engine's, in its order; within a cycle
// the light is set by the smoothstep the engine meant.
#pragma once

namespace fix
{

// Once, from the loader's way into the game, before the plugins load: every
// game, whatever plugins it has.
void InstallLightFlickerFix();

} // namespace fix
