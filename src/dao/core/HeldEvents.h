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
#include <set>
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
    return (days * dayLength) + (ms / 1000);
}

// The held events and the plugin's clock they wait on, in seconds from
// when it was first held. Events due at one time come out in the order
// they went in. Not thread-safe: dao/Timers.cpp locks around it.
template <typename Event> class HeldEvents
{
  public:
    // The clock: while an event is being delivered, where it came due, so
    // that one it re-arms counts from there; otherwise as far as it has
    // been advanced, so that one sent after an advance counts from its end.
    [[nodiscard]] double Now() const
    {
        return delivering_ ? now_ : target_;
    }

    // Due `delay` seconds from now on this clock.
    void Add(double delay, Event event)
    {
        AddAt(Now() + (delay > 0 ? delay : 0), std::move(event));
    }

    // Due at `due` on this clock; one already past comes out first.
    void AddAt(double due, Event event)
    {
        const double now = Now();
        events_.emplace(due > now ? due : now, std::move(event));
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

    // The next event due by as far as the clock has been advanced, the
    // clock moved to where it came due; nothing once none is.
    std::optional<Event> Next()
    {
        if (events_.empty() || events_.begin()->first > target_)
        {
            now_ = target_;
            delivering_ = false;
            return std::nullopt;
        }
        auto first = events_.begin();
        now_ = first->first > now_ ? first->first : now_;
        delivering_ = true;
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
        const double now = Now();
        for (auto &[due, event] : events_)
        {
            left.emplace_back(due - now, std::move(event));
        }
        events_.clear();
        now_ = 0;
        target_ = 0;
        delivering_ = false;
        return left;
    }

    [[nodiscard]] std::size_t Size() const
    {
        return events_.size();
    }

  private:
    double now_ = 0;
    double target_ = 0;
    bool delivering_ = false;
    std::multimap<double, Event> events_;
};

// Which script events a hold keeps (dao/Timers.h), by their EVENT_TYPE_*
// and the delay they are sent with: a kind left out never, a kind held
// always, and any other kind when its delay is at least `from` seconds (0 or
// less: none by delay). Left out wins, so a kind that must run as it does
// (a fight's end, EVENT_TYPE_DELAYED_GM_CHANGE) stays out of a hold by
// delay.
struct HoldRule
{
    std::set<std::int32_t> held;
    std::set<std::int32_t> leftOut;
    double from = 0;

    bool Holds(std::int32_t type, double delay) const
    {
        if (leftOut.contains(type))
        {
            return false;
        }
        if (held.contains(type))
        {
            return true;
        }
        return from > 0 && delay >= from;
    }

    // Held always (true), or left out always (false).
    void Hold(std::int32_t type, bool hold)
    {
        if (hold)
        {
            held.insert(type);
            leftOut.erase(type);
        }
        else
        {
            held.erase(type);
            leftOut.insert(type);
        }
    }
};

// Held events on a clock per target, the object each is sent to: the
// shared Advance moves every target's clock but those on clocks of their
// own, which only Advance(target, ...) moves. A clock's time counts only
// from its first event, so one is made as it is needed and dropped when
// nothing waits on it.
template <typename Event> class HeldByTarget
{
  public:
    struct Left
    {
        std::uint32_t target = 0;
        double seconds = 0;
        Event event;
    };

    [[nodiscard]] double Now(std::uint32_t target) const
    {
        const auto found = clocks_.find(target);
        return found != clocks_.end() ? found->second.Now() : 0;
    }

    void Add(std::uint32_t target, double delay, Event event)
    {
        clocks_[target].Add(delay, std::move(event));
    }

    void AddAt(std::uint32_t target, double due, Event event)
    {
        clocks_[target].AddAt(due, std::move(event));
    }

    void Advance(double seconds)
    {
        for (auto &[target, clock] : clocks_)
        {
            if (!own_.contains(target))
            {
                clock.Advance(seconds);
            }
        }
    }

    void Advance(std::uint32_t target, double seconds)
    {
        if (const auto found = clocks_.find(target); found != clocks_.end())
        {
            found->second.Advance(seconds);
        }
    }

    void SetOwn(std::uint32_t target, bool own)
    {
        if (own)
        {
            own_.insert(target);
        }
        else
        {
            own_.erase(target);
        }
    }

    [[nodiscard]] bool Own(std::uint32_t target) const
    {
        return own_.contains(target);
    }

    void ShareAll()
    {
        own_.clear();
    }

    // The next event due on any target's clock, with its target; nothing
    // once none is.
    std::optional<std::pair<std::uint32_t, Event>> Next()
    {
        for (auto &[target, clock] : clocks_)
        {
            if (auto event = clock.Next())
            {
                return std::pair<std::uint32_t, Event>{target, std::move(*event)};
            }
        }
        return std::nullopt;
    }

    // Drop the clocks nothing waits on. Not while an event is delivered.
    void Prune()
    {
        std::erase_if(clocks_, [](const auto &entry) { return entry.second.Size() == 0; });
    }

    // Everything still held, by target, each with the seconds it has left;
    // every clock gone and every target back on the shared clock.
    std::vector<Left> Release()
    {
        std::vector<Left> left;
        for (auto &[target, clock] : clocks_)
        {
            for (auto &[seconds, event] : clock.Release())
            {
                left.push_back(Left{target, seconds, std::move(event)});
            }
        }
        clocks_.clear();
        own_.clear();
        return left;
    }

    [[nodiscard]] std::size_t Size() const
    {
        std::size_t size = 0;
        for (const auto &[target, clock] : clocks_)
        {
            size += clock.Size();
        }
        return size;
    }

    [[nodiscard]] std::size_t Size(std::uint32_t target) const
    {
        const auto found = clocks_.find(target);
        return found != clocks_.end() ? found->second.Size() : 0;
    }

  private:
    std::map<std::uint32_t, HeldEvents<Event>> clocks_;
    std::set<std::uint32_t> own_;
};

} // namespace dao::core
