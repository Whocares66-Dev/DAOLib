// Delayed events kept off the engine's queue while a plugin holds the clock
// (dao/Timers.h), and the world clock's way of writing a delay. The engine
// queues an event due at an absolute world time; a held one is due instead
// at a time on the plugin's own clock, which moves only when it is
// advanced. Here because the order they come due in, and the clock each
// one sees as it is delivered, is logic a test can check with no game
// (tests/test_heldevents.cpp).
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace dao::core
{

// A delay as the world clock keeps one: whole days of `dayLength` seconds,
// and milliseconds into the next. DelayEvent writes its seconds so through
// two of the world timer's helpers (0x00A35D80 for the days, by taking
// whole days off while the rest is at least one; 0x00A35CF0 for the rest,
// times 1000, truncated), read 2026-10-02.
struct WorldDelay
{
    std::uint32_t days = 0;
    std::uint32_t ms = 0;
};

inline WorldDelay ToWorldDelay(double seconds, std::uint32_t dayLength)
{
    WorldDelay delay;
    if (seconds <= 0)
    {
        return delay;
    }
    if (dayLength > 0)
    {
        while (seconds >= dayLength)
        {
            seconds -= dayLength;
            ++delay.days;
        }
    }
    delay.ms = static_cast<std::uint32_t>(seconds * 1000);
    return delay;
}

// How many seconds from (fromDay, fromMs) to (toDay, toMs) on the world
// clock; less than 0 for a time already past.
inline double WorldSeconds(std::uint32_t fromDay, std::uint32_t fromMs, std::uint32_t toDay, std::uint32_t toMs,
                           std::uint32_t dayLength)
{
    const double days = static_cast<double>(toDay) - static_cast<double>(fromDay);
    const double ms = static_cast<double>(toMs) - static_cast<double>(fromMs);
    return days * dayLength + ms / 1000;
}

// The held events and the plugin's clock they wait on, in seconds from
// when it was first held. Events due at one time come out in the order
// they went in. Not thread-safe: dao/Timers.cpp locks around it.
template <typename Event> class HeldEvents
{
  public:
    // The clock: where the last event delivered came due, or as far as it
    // has been advanced once nothing more is due.
    [[nodiscard]] double Now() const
    {
        return now_;
    }

    // Due `delay` seconds from now on this clock.
    void Add(double delay, Event event)
    {
        AddAt(now_ + (delay > 0 ? delay : 0), std::move(event));
    }

    // Due at `due` on this clock; one already past comes out first.
    void AddAt(double due, Event event)
    {
        events_.emplace(due > now_ ? due : now_, std::move(event));
    }

    // Move the clock on by `seconds`: what falls due in them comes out of
    // Next, one at a time.
    void Advance(double seconds)
    {
        if (seconds > 0)
        {
            target_ += seconds;
        }
    }

    // The next event due by the clock's target, the clock moved to where it
    // came due, so that one added while it is delivered is due from there;
    // nothing once none is, the clock then at its target.
    std::optional<Event> Next()
    {
        if (events_.empty() || events_.begin()->first > target_)
        {
            now_ = target_;
            return std::nullopt;
        }
        auto first = events_.begin();
        now_ = first->first > now_ ? first->first : now_;
        Event event = std::move(first->second);
        events_.erase(first);
        return event;
    }

    // Everything still held, each with the seconds it has left, in the
    // order they come due; the clock starts again from 0.
    std::vector<std::pair<double, Event>> Release()
    {
        std::vector<std::pair<double, Event>> left;
        left.reserve(events_.size());
        for (auto &[due, event] : events_)
        {
            left.emplace_back(due - now_, std::move(event));
        }
        events_.clear();
        now_ = 0;
        target_ = 0;
        return left;
    }

    [[nodiscard]] std::size_t Size() const
    {
        return events_.size();
    }

  private:
    double now_ = 0;
    double target_ = 0;
    std::multimap<double, Event> events_;
};

} // namespace dao::core
