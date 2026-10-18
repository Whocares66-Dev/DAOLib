#include "dao/core/WindowWatch.h"

#include <catch2/catch_test_macros.hpp>

using dao::core::WindowWatch;

TEST_CASE("a close follows an SC_CLOSE of the second before it, and not one before that or none")
{
    WindowWatch watch;
    CHECK_FALSE(watch.CloseFollowsSysClose(5000));
    watch.SysClose(5000);
    CHECK(watch.CloseFollowsSysClose(5000));
    CHECK(watch.CloseFollowsSysClose(6000));
    CHECK_FALSE(watch.CloseFollowsSysClose(6001));
    CHECK_FALSE(watch.CloseFollowsSysClose(4999));
}

TEST_CASE("a key is soon after an activation for two seconds, and not before one")
{
    WindowWatch watch;
    CHECK_FALSE(watch.KeySoonAfterActivation(100));
    watch.Activated(10000);
    CHECK(watch.KeySoonAfterActivation(10000));
    CHECK(watch.KeySoonAfterActivation(12000));
    CHECK_FALSE(watch.KeySoonAfterActivation(12001));
    watch.Activated(20000);
    CHECK(watch.KeySoonAfterActivation(21500));
}
