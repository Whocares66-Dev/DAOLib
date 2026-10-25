// The loot panel: the container it shows, its Take All and its Close, as its buttons do them.
//
//   if (dao::loot::Container())
//       dao::loot::TakeAll();
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>

namespace dao::loot
{

// Returns the container the loot panel shows: a chest, a body's bag, a pile of bones.
// @return nullopt when no loot panel is shown
//
// Call it, and the rest, on the game thread.
std::optional<ObjectId> Container();

// What a button did.
enum class Result : std::uint8_t
{
    Closed,       // the panel closed; the game mode goes back on its next update
    BackpackFull, // Take All took what fitted, the panel stays shown with items left
    NotShown,     // no loot panel is shown, so nothing was done
};

// Closes the panel as its Close button does: a container that takes nothing and is empty is
// left, as the engine leaves an emptied one, no longer to be used.
Result Close();

// Takes everything into the party's backpack as the Take All button does, then closes the panel,
// unless the backpack filled with items left.
Result TakeAll();

} // namespace dao::loot
