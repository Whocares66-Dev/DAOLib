// The party as the player commands it: which members are selected in the party bar, which
// creature the player controls, a callback for when control passes to another member, and the
// character the full screens (inventory, character record, crafting, the store) act on.
//
//   for (const dao::ObjectId member : dao::party::Selected()) ...
//   dao::party::OnControlChanged([](dao::ObjectId before, dao::ObjectId now) { ... });
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
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

// Returns the character the full screens act on, the one their arrows step through: the controlled
// creature when a screen opens, then whoever its arrows chose.
// @return its id, or kInvalidObject before a game is loaded
//
// Call it, and SetScreenCharacter, on the game thread.
ObjectId ScreenCharacter();

// Makes a party member the character the full screens act on, as their arrows do, stepping through
// the members they show. Control stays where it is. A screen opening chooses the controlled
// creature again, so set it once the screen is open.
// @return false, with the choice as it was, for a creature the arrows never reach: one not in the
// party (or, at camp, not among those there), or not one the screens show
bool SetScreenCharacter(ObjectId creature);

// Returns the engine's party object, for the library's own parts that call its functions.
// @return its address, or 0 before a game is loaded
std::uintptr_t EngineObject();

} // namespace dao::party
