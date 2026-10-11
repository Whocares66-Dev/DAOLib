// What fast load's parts share (perf/FastLoad.h): the player's switch, the
// loading mode as its update finds it, and two helpers.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace perf::fastload
{

// Whether fast load is on: the player's switch in the options screen. A part
// asks at each thing it would do, so a change takes effect from the next
// loading screen, with nothing detoured or put back for it.
bool On();

// The loading mode as an update of it begins. Every part is told before the
// engine's update runs, on the game's update thread.
struct Update
{
    std::uintptr_t mode = 0; // the GameModeLoading
    std::uint16_t state = 0; // its state (dao/Addresses.h, "The loading mode's states")
    std::int64_t now = 0;    // ms, NowMs
    bool first = false;      // the first update of a loading screen: none for a second before it
};

// Milliseconds by a clock that only goes forward.
inline std::int64_t NowMs()
{
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

// A field of one of the engine's objects.
template <class T> T &At(std::uintptr_t object, std::size_t offset)
{
    return *reinterpret_cast<T *>(object + offset);
}

} // namespace perf::fastload
