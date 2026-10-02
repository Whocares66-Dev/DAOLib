#include "dao/core/OwedTime.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using dao::core::OwedTime;

TEST_CASE("an item is owed what the shared clock moved since it was last asked")
{
    OwedTime<std::uint32_t> owed;
    // First asked: owed nothing, whatever was advanced before.
    owed.Advance(4000);
    CHECK(owed.Owed(1, 100) == 0);
    owed.Advance(4000);
    CHECK(owed.Owed(1, 100) == 4000);
    CHECK(owed.Owed(1, 100) == 0);
}

TEST_CASE("an owner on its own clock is passed by the shared one")
{
    OwedTime<std::uint32_t> owed;
    CHECK(owed.Owed(1, 100) == 0); // the creature's effect
    CHECK(owed.Owed(2, 200) == 0); // a bystander's
    owed.SetOwn(100, true);
    owed.Advance(4000);
    CHECK(owed.Owed(1, 100) == 0);
    CHECK(owed.Owed(2, 200) == 4000);
    owed.Advance(100, 1500);
    CHECK(owed.Owed(1, 100) == 1500);
    CHECK(owed.Owed(2, 200) == 0);
}

TEST_CASE("a fraction of a millisecond is carried to the next time")
{
    OwedTime<std::uint32_t> owed;
    owed.SetOwn(100, true);
    CHECK(owed.Owed(1, 100) == 0);
    for (int i = 0; i < 3; ++i)
    {
        owed.Advance(100, 0.4);
    }
    CHECK(owed.Owed(1, 100) == 1);
    owed.Advance(100, 0.4);
    owed.Advance(100, 0.4);
    CHECK(owed.Owed(1, 100) == 1);
}

TEST_CASE("time the shared clock moved while an owner had its own is not owed later")
{
    OwedTime<std::uint32_t> owed;
    CHECK(owed.Owed(1, 100) == 0);
    owed.SetOwn(100, true);
    owed.Advance(4000);
    CHECK(owed.Owed(1, 100) == 0);
    owed.ShareAll();
    CHECK_FALSE(owed.Own(100));
    CHECK(owed.Owed(1, 100) == 0);
    owed.Advance(2000);
    CHECK(owed.Owed(1, 100) == 2000);
}

TEST_CASE("an owner's own advance counts on the shared clock too")
{
    // Not on its own clock, an owner advanced alone ages by both.
    OwedTime<std::uint32_t> owed;
    CHECK(owed.Owed(1, 100) == 0);
    owed.Advance(1000);
    owed.Advance(100, 500);
    CHECK(owed.Owed(1, 100) == 1500);
}
