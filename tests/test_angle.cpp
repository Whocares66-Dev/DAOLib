#include "dao/core/Angle.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>

using Catch::Approx;
using dao::core::ShortestTurn;

namespace
{
constexpr float kPi = std::numbers::pi_v<float>;
} // namespace

TEST_CASE("a turn toward a yaw goes the shorter way round")
{
    CHECK(ShortestTurn(0.0F, 1.0F) == Approx(1.0F));
    CHECK(ShortestTurn(1.0F, 0.0F) == Approx(-1.0F));
    // Across the seam: from just short of 2 pi to just past 0 is a small
    // turn forward, not most of a circle back.
    CHECK(ShortestTurn(6.2F, 0.1F) == Approx(0.1F + 2 * kPi - 6.2F).margin(1e-5));
    CHECK(ShortestTurn(0.1F, 6.2F) == Approx(6.2F - 2 * kPi - 0.1F).margin(1e-5));
}

TEST_CASE("a yaw the player has turned past 2 pi is met the same way")
{
    // The camera's yaw reached 9.29 in play; facing 1.0 from there is 1.0
    // plus a whole turn away, and the turn left is the difference mod 2 pi.
    const float from = 9.29F;
    const float to = 1.0F;
    const float turn = ShortestTurn(from, to);
    CHECK(turn > -kPi);
    CHECK(turn <= kPi);
    CHECK(std::remainder(from + turn - to, 2 * kPi) == Approx(0.0F).margin(1e-4));
    CHECK(ShortestTurn(from + 4 * kPi, to) == Approx(turn).margin(1e-4));
}

TEST_CASE("no turn, and a half turn, are what they are")
{
    CHECK(ShortestTurn(2.0F, 2.0F) == Approx(0.0F).margin(1e-6));
    CHECK(ShortestTurn(2.0F, 2.0F + 2 * kPi) == Approx(0.0F).margin(1e-5));
    CHECK(std::abs(ShortestTurn(0.0F, kPi)) == Approx(kPi));
}
