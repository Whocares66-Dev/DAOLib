// Which processors a copy of the game runs on when other copies run beside
// it: the part of perf/OwnProcessors.h that needs no process.
//
// GOG's DAOrigins.exe carries a process affinity mask of 3 in its PE load
// configuration, so Windows starts every copy of it on the first two
// logical processors (dev/BUILDS.md). One copy is as GOG meant it. Several
// copies share those two while the rest of the machine idles, and stall
// each other for seconds (DAOLib #89). So each copy keeps as many
// processors as the image gave it, but takes a group no other copy has.
#pragma once

#include <bit>
#include <cstdint>
#include <span>

namespace dao::core
{

// Returns the processors a copy of the game should run on.
// @param system the machine's processors, a bit each
// @param own the processors this copy was started on
// @param others the processors of each other running copy
// @return `own` if this copy was given every processor or none, or if no other copy has any of
//  its processors; otherwise the first group of as many of the machine's processors, counted
//  from the lowest in groups of that size, that the fewest other copies run on
//
// With every group taken, copies spread evenly over them rather than all landing on the first. A
// copy given every processor has one group to choose from, its own.
constexpr std::uint64_t ChooseProcessors(std::uint64_t system, std::uint64_t own, std::span<const std::uint64_t> others)
{
    const int count = std::popcount(own);
    if (count == 0 || (own & system) != own)
    {
        return own;
    }
    const auto sharers = [&](std::uint64_t group) {
        int sharing = 0;
        for (const std::uint64_t other : others)
        {
            sharing += (other & group) != 0 ? 1 : 0;
        }
        return sharing;
    };
    std::uint64_t best = own;
    int fewest = sharers(own);
    std::uint64_t group = 0;
    int taken = 0;
    for (int bit = 0; bit < 64 && fewest != 0; ++bit)
    {
        const std::uint64_t processor = std::uint64_t{1} << bit;
        if ((system & processor) == 0)
        {
            continue;
        }
        group |= processor;
        if (++taken < count)
        {
            continue;
        }
        if (const int sharing = sharers(group); sharing < fewest)
        {
            best = group;
            fewest = sharing;
        }
        group = 0;
        taken = 0;
    }
    return best;
}

} // namespace dao::core
