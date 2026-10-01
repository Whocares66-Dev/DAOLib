#include "dao/core/Path.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using Catch::Approx;
using dao::core::PathLength;
using dao::core::PathRemaining;
using dao::core::Point3;

TEST_CASE("a path's length is the sum of its legs")
{
    const std::vector<Point3> path = {{0, 0, 0}, {3, 4, 0}, {3, 4, 12}};
    CHECK(PathLength(path) == Approx(5.0F + 12.0F));
    CHECK(PathLength(std::vector<Point3>{{1, 1, 1}}) == 0.0F);
    CHECK(PathLength({}) == 0.0F);
}

TEST_CASE("what is left of a path is from where the walker is to the next waypoint, then on")
{
    const std::vector<Point3> path = {{0, 0, 0}, {10, 0, 0}, {10, 10, 0}};
    // Halfway along the first leg, heading for the second waypoint.
    CHECK(PathRemaining(path, 1, {5, 0, 0}) == Approx(5.0F + 10.0F));
    // On the last leg.
    CHECK(PathRemaining(path, 2, {10, 4, 0}) == Approx(6.0F));
    // Past the end: nothing left.
    CHECK(PathRemaining(path, 3, {10, 10, 0}) == 0.0F);
}
