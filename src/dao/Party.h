// The party as the player commands it: which members are selected in the party bar, which
// creature the player controls, and a callback for when control passes to another member.
//
//   for (const dao::ObjectId member : dao::party::Selected()) ...
//   dao::party::OnControlChanged([](dao::ObjectId before, dao::ObjectId now) { ... });
#pragma once

#include "dao/ObjectId.h"

#include <functional>
#include <vector>

namespace dao::party
{

// Returns the members selected in the party bar, in the order they were selected.
// @return the ids, or an empty vector before a game is loaded or if the selection cannot be read
//
// Call it on the game thread.
std::vector<ObjectId> Selected();

// Returns the creature the player controls, the same one the scripts' GetMainControlled returns.
// @return its id, or kInvalidObject before a game is loaded
//
// Call it on the game thread.
ObjectId Controlled();

// Registers a callback for when the creature the player controls changes.
// @param observer receives the previously controlled creature and the newly controlled one
//
// It runs on the game thread, in the first frame after the change, whatever caused it. Loading a
// game reports a change from kInvalidObject to the loaded hero.
void OnControlChanged(std::function<void(ObjectId before, ObjectId now)> observer);

} // namespace dao::party
