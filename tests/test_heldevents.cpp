#include "dao/core/HeldEvents.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using Catch::Approx;
using dao::core::HeldEvents;
using dao::core::ToWorldDelay;
using dao::core::WorldSeconds;

namespace
{

// Everything due now, by name, with the clock each one was delivered at.
std::vector<std::pair<std::string, double>> Drain(HeldEvents<std::string> &held)
{
    std::vector<std::pair<std::string, double>> out;
    while (auto event = held.Next())
    {
        out.emplace_back(*event, held.Now());
    }
    return out;
}

} // namespace

TEST_CASE("a held event waits until the clock is advanced past it")
{
    HeldEvents<std::string> held;
    held.Add(1.5, "tick");
    CHECK_FALSE(held.Next());
    held.Advance(1.0);
    CHECK(Drain(held).empty());
    CHECK(held.Now() == Approx(1.0));
    held.Advance(1.0);
    const auto out = Drain(held);
    REQUIRE(out.size() == 1);
    CHECK(out[0].first == "tick");
    // Delivered at the time it came due, not at the end of the advance.
    CHECK(out[0].second == Approx(1.5));
    CHECK(held.Now() == Approx(2.0));
}

TEST_CASE("events come due in order, those at one time in the order added")
{
    HeldEvents<std::string> held;
    held.Add(3.0, "c");
    held.Add(1.0, "a");
    held.Add(2.0, "b1");
    held.Add(2.0, "b2");
    held.Advance(4.0);
    const auto out = Drain(held);
    REQUIRE(out.size() == 4);
    CHECK(out[0].first == "a");
    CHECK(out[1].first == "b1");
    CHECK(out[2].first == "b2");
    CHECK(out[3].first == "c");
}

TEST_CASE("an event re-armed as it is delivered counts from its own time")
{
    // A damage-over-time tick every 1.5 s, re-armed by its script each time
    // (effect_dot2_h): four seconds of a round give ticks at 1.5 and 3.0,
    // and the next waits for 4.5, as it would have in real time.
    HeldEvents<std::string> held;
    held.Add(1.5, "tick");
    held.Advance(4.0);
    std::vector<double> at;
    while (auto event = held.Next())
    {
        at.push_back(held.Now());
        held.Add(1.5, "tick");
    }
    REQUIRE(at.size() == 2);
    CHECK(at[0] == Approx(1.5));
    CHECK(at[1] == Approx(3.0));
    held.Advance(4.0);
    at.clear();
    while (auto event = held.Next())
    {
        at.push_back(held.Now());
        held.Add(1.5, "tick");
    }
    REQUIRE(at.size() == 3);
    CHECK(at[0] == Approx(4.5));
    CHECK(at[1] == Approx(6.0));
    CHECK(at[2] == Approx(7.5));
}

TEST_CASE("one already past comes out at once, at the clock")
{
    HeldEvents<std::string> held;
    held.Advance(5.0);
    CHECK_FALSE(held.Next());
    held.AddAt(2.0, "late");
    const auto out = Drain(held);
    REQUIRE(out.size() == 1);
    CHECK(out[0].second == Approx(5.0));
}

TEST_CASE("releasing gives each event the time it has left")
{
    HeldEvents<std::string> held;
    held.Add(1.0, "a");
    held.Add(6.0, "b");
    held.Advance(2.5);
    REQUIRE(held.Next() == "a");
    CHECK_FALSE(held.Next());
    const auto left = held.Release();
    REQUIRE(left.size() == 1);
    CHECK(left[0].first == Approx(3.5));
    CHECK(left[0].second == "b");
    CHECK(held.Size() == 0);
    CHECK(held.Now() == Approx(0.0));
}

TEST_CASE("a delay is written as the world clock writes one")
{
    // Whole days off while the rest is at least a day, then milliseconds.
    CHECK(ToWorldDelay(4.0, 7200).days == 0);
    CHECK(ToWorldDelay(4.0, 7200).ms == 4000);
    CHECK(ToWorldDelay(7200.0, 7200).days == 1);
    CHECK(ToWorldDelay(7200.0, 7200).ms == 0);
    CHECK(ToWorldDelay(7201.5, 7200).days == 1);
    CHECK(ToWorldDelay(7201.5, 7200).ms == 1500);
    CHECK(ToWorldDelay(0.0, 7200).ms == 0);
    CHECK(ToWorldDelay(-1.0, 7200).ms == 0);
}

TEST_CASE("the seconds between two world times")
{
    CHECK(WorldSeconds(3, 1000, 3, 5000, 7200) == Approx(4.0));
    CHECK(WorldSeconds(3, 7199000, 4, 1000, 7200) == Approx(2.0));
    CHECK(WorldSeconds(3, 5000, 3, 1000, 7200) == Approx(-4.0));
}
