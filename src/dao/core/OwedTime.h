// Time owed to things that age on a plugin's clock (dao/Effects.h's
// effects, dao/Clock.h's regeneration): the shared clock, which moves for
// every owner but those on clocks of their own, and each owner's own. The
// engine asks for each thing's time as its owner updates; what a plugin
// has advanced since it last asked is added. Here because what is owed to
// whom, and what is carried between small steps, is logic a test can check
// with no game (tests/test_owedtime.cpp).
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace dao::core
{

template <typename Key> class OwedTime
{
  public:
    // Move the shared clock on, for every owner not on a clock of its own.
    void Advance(double ms)
    {
        if (ms > 0)
        {
            shared_ += ms;
        }
    }

    // Move one owner's own clock on, whether or not it is on it: on its own
    // clock, its things age by this alone.
    void Advance(std::uint32_t owner, double ms)
    {
        if (ms > 0)
        {
            owners_[owner].total += ms;
        }
    }

    // Put an owner on a clock of its own (true), which the shared clock
    // passes by, or back on the shared one (false).
    void SetOwn(std::uint32_t owner, bool own)
    {
        if (own)
        {
            owners_[owner].own = true;
        }
        else if (const auto found = owners_.find(owner); found != owners_.end())
        {
            found->second.own = false;
        }
    }

    [[nodiscard]] bool Own(std::uint32_t owner) const
    {
        const auto found = owners_.find(owner);
        return found != owners_.end() && found->second.own;
    }

    // Every owner back on the shared clock.
    void ShareAll()
    {
        for (auto &[owner, clock] : owners_)
        {
            clock.own = false;
        }
    }

    // The whole milliseconds owed to `item`, of `owner`, since it was last
    // asked about: what the shared clock moved, unless the owner is on its
    // own, and what the owner's own moved. A fraction is kept for the next
    // time. An item first asked about is owed nothing.
    std::int32_t Owed(Key item, std::uint32_t owner)
    {
        const auto found = owners_.find(owner);
        const double ownTotal = found != owners_.end() ? found->second.total : 0;
        const bool own = found != owners_.end() && found->second.own;
        const auto [seen, first] = items_.try_emplace(item, Seen{shared_, ownTotal});
        if (first)
        {
            return 0;
        }
        const double owed = (own ? 0 : shared_ - seen->second.shared) + (ownTotal - seen->second.own);
        const double whole = owed > 0 ? std::floor(owed) : 0;
        // What is given is taken off, the fraction left owed; the shared
        // time that passed while on its own clock is not owed later.
        if (own)
        {
            seen->second.shared = shared_;
            seen->second.own += whole;
        }
        else
        {
            const double fromShared = shared_ - seen->second.shared;
            const double givenShared = whole < fromShared ? whole : fromShared;
            seen->second.shared += givenShared;
            seen->second.own += whole - givenShared;
        }
        return static_cast<std::int32_t>(whole);
    }

    // Forget every item, as if each were first asked about next: for when
    // so many have been seen that most must be gone.
    void ForgetItems()
    {
        items_.clear();
    }

    [[nodiscard]] std::size_t Items() const
    {
        return items_.size();
    }

  private:
    struct Owner
    {
        double total = 0;
        bool own = false;
    };
    struct Seen
    {
        double shared = 0;
        double own = 0;
    };
    double shared_ = 0;
    std::unordered_map<std::uint32_t, Owner> owners_;
    std::unordered_map<Key, Seen> items_;
};

} // namespace dao::core
