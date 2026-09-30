// Angles as the camera keeps them: radians, not wrapped (the camera's yaw
// ran past 2 pi in play, 2026-09-30, as the player turned it). Here
// because a turn toward a yaw has to find the shorter way round whatever
// multiple of 2 pi either side is at, and that is logic a test can check
// with no game (tests/test_angle.cpp).
#pragma once

#include <cmath>
#include <numbers>

namespace dao::core
{

// The angle to turn from `from` to face `to` by the shorter way, in
// (-pi, pi]; `from + ShortestTurn(from, to)` faces the same way as `to`.
inline float ShortestTurn(float from, float to)
{
    constexpr float kPi = std::numbers::pi_v<float>;
    constexpr float kTurn = 2 * kPi;
    float d = std::fmod(to - from, kTurn);
    if (d > kPi)
    {
        d -= kTurn;
    }
    else if (d <= -kPi)
    {
        d += kTurn;
    }
    return d;
}

} // namespace dao::core
