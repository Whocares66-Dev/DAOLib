// Which processors a copy of the game takes beside other copies
// (dao/core/Processors.h).
#include "dao/core/Processors.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

using dao::core::ChooseProcessors;

namespace
{

constexpr std::uint64_t kTwenty = 0xFFFFF; // ten cores of two threads
constexpr std::uint64_t kGogs = 0x3;       // what GOG's exe is started on

std::uint64_t Choose(std::uint64_t own, const std::vector<std::uint64_t> &others, std::uint64_t system = kTwenty)
{
    return ChooseProcessors(system, own, others);
}

} // namespace

TEST_CASE("a copy alone keeps the processors it was started on", "[processors]")
{
    CHECK(Choose(kGogs, {}) == kGogs);
    // And so does one whose own are free though others run elsewhere.
    CHECK(Choose(kGogs, {0xC, 0x30}) == kGogs);
}

TEST_CASE("a copy beside others takes the first group of its size that is free", "[processors]")
{
    // Three copies as GOG's exe starts them, all on the first two.
    CHECK(Choose(kGogs, {kGogs}) == 0xC);
    CHECK(Choose(kGogs, {kGogs, kGogs, kGogs}) == 0xC);
    // The second of this library's copies beside the first and an old one.
    CHECK(Choose(kGogs, {kGogs, 0xC}) == 0x30);
    // A gap left by a copy that closed is taken before a higher group.
    CHECK(Choose(kGogs, {kGogs, 0x30}) == 0xC);
    // As many processors as it was given: a copy started on four takes four.
    CHECK(Choose(0xF, {0xF}) == 0xF0);
}

TEST_CASE("with every group taken, copies spread evenly", "[processors]")
{
    // Four processors, two groups, both taken: the group with the fewer.
    CHECK(Choose(kGogs, {kGogs, kGogs, 0xC}, 0xF) == 0xC);
    CHECK(Choose(kGogs, {kGogs, 0xC, 0xC}, 0xF) == kGogs);
    // Evenly taken: its own.
    CHECK(Choose(kGogs, {kGogs, 0xC}, 0xF) == kGogs);
}

TEST_CASE("a copy not confined is left as it is", "[processors]")
{
    // EA's and Steam's exes are started on every processor.
    CHECK(Choose(kTwenty, {kGogs, kGogs}) == kTwenty);
    CHECK(Choose(0, {kGogs}) == 0);
    // Processors the machine has not: nothing to choose among.
    CHECK(Choose(0x300000, {0x300000}) == 0x300000);
}

TEST_CASE("groups are counted over the processors the machine has", "[processors]")
{
    // A machine whose processors are 0, 1, 4 and 5: the second group is 4 and 5.
    CHECK(Choose(kGogs, {kGogs}, 0x33) == 0x30);
    // An odd processor left over makes no group: three processors, one group.
    CHECK(Choose(kGogs, {kGogs}, 0x7) == kGogs);
}
