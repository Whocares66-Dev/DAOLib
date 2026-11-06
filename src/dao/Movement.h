// A creature's walk as the engine runs it: its movement state, the movement record that keeps a
// walker waiting or re-pathing for a creature in its way, and each update of a move with what the
// creature-in-the-way pass did in it. Use it to trace why a walker stands, waits or gives up.
//
//   auto watch = dao::movement::OnMoveUpdate([](const dao::movement::MoveUpdate &u) {
//       if (u.inTheWay && u.inTheWay->reacted)
//           spdlog::info("{:#x} {} blockers, now {}", u.creature, u.inTheWay->listed.size(),
//                        u.inTheWay->after.state);
//   });
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace dao::movement
{

// The movement controller's states, as Walker::motion has them.
namespace motion
{
inline constexpr std::int32_t kIdle = 0;
inline constexpr std::int32_t kWalk = 1;
inline constexpr std::int32_t kRun = 2;
inline constexpr std::int32_t kStopping = 3;
inline constexpr std::int32_t kTurning = 4; // turning in place
} // namespace motion

// The movement record's states, as Record::state has them.
namespace record
{
inline constexpr std::int32_t kClear = 0;
inline constexpr std::int32_t kRepath = 2;   // a fresh search asked, for a creature in the way
inline constexpr std::int32_t kSettling = 3; // just after a search; the in-the-way pass waits
inline constexpr std::int32_t kWaiting = 4;  // standing for a creature in the way until the timer runs out
} // namespace record

// The movement record: whether the walker waits or re-paths for a creature in its way.
struct Record
{
    std::int32_t state = record::kClear;
    std::int32_t timerMs = 0; // what is left of a wait or a settle
};

// A creature's movement as it stands.
struct Walker
{
    std::int32_t motion = motion::kIdle; // one of motion::
    std::optional<Record> record;        // nullopt if the creature has none
};

// Returns a creature's movement state and its movement record.
// @return nullopt if the ID is not a creature
//
// Call it on the game thread.
std::optional<Walker> WalkerOf(ObjectId creature);

// What the creature-in-the-way pass did in one move update.
struct InTheWay
{
    bool combat = false; // the in-combat pass, which reacts at once and pushes no one
    // The creatures found in the way, before any was pushed aside; empty if the pass found none.
    std::vector<ObjectId> listed;
    // The out-of-combat pass's answer: 0 none blocks, 1 one blocks (the reaction follows), 2 the
    // walk stops for the controlled creature. Always 1 in combat, where the reaction always runs.
    std::int32_t answer = 0;
    bool reacted = false; // the reaction ran
    Record before, after; // the record around the reaction; the same if it did not run
};

// One update of a move: an attack's approach, a walk to a place, or another command's walk. The ids
// are the game object ids the scripts use.
struct MoveUpdate
{
    ObjectId creature = kInvalidObject;
    std::uint32_t subAction = 0; // the sub-action's type, as dao::commands::SubAction::type; 0 if not found
    // The update's answer: 1 arrived, 0 still going, negative a failure (-4 stuck or no path,
    // -8 done while waiting, -9 a door in the way).
    std::int32_t result = 0;
    // How long the creature has stood while it should walk, in ms. Past 1000, or 2000 unless the
    // creature is in combat, the move fails with -4.
    std::int32_t standingMs = 0;
    Walker walker;                    // after the update
    std::optional<InTheWay> inTheWay; // nullopt if the update ran no in-the-way pass
};

// A plugin's watch on the move updates. It ends when it is destroyed or stopped.
class Watch
{
  public:
    Watch() = default;
    Watch(const Watch &) = delete;
    Watch &operator=(const Watch &) = delete;
    Watch(Watch &&other) noexcept;
    Watch &operator=(Watch &&other) noexcept;
    ~Watch();

    // Ends the watch at once; its callback is not called again once this returns, unless it is
    // running on another thread.
    void Stop() noexcept;
    // Returns true until the watch is stopped or moved from.
    bool Active() const noexcept
    {
        return id_ != 0;
    }

    // The library's own; a plugin gets its watch from OnMoveUpdate.
    explicit Watch(std::uint64_t id) noexcept : id_(id)
    {
    }

  private:
    std::uint64_t id_ = 0;
};

// Calls `observer` after each update of every creature's move, until the watch it returns ends.
// Callbacks run on the game thread, inside the update: read, do not order the creature from there.
// With no watch alive an update is passed straight on.
[[nodiscard]] Watch OnMoveUpdate(std::function<void(const MoveUpdate &update)> observer);

} // namespace dao::movement
