// An item's requirements as its tooltip shows them, and whether a creature can use it, by the
// engine's own tests. The engine's equip tests neither: a plugin that equips asks first.
//
//   if (!dao::items::CanUse(hero, sword))
//       for (const auto &need : dao::items::Requirements(sword, hero))
//           if (!need.met) ... // need.property, need.value
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <vector>

namespace dao::items
{

// An attribute an item asks of the creature that uses it.
struct Requirement
{
    std::int32_t property = 0; // PROPERTY_ATTRIBUTE_STRENGTH (1) to PROPERTY_ATTRIBUTE_CONSTITUTION (6)
    float value = 0;           // as the tooltip shows it, a whole number
    bool met = false;          // the creature's attribute, with its bonuses, is as high
};

// Returns an item's attribute requirements of a creature, as its tooltip shows them: its base
// item's, raised by its material, and an Arcane Warrior's magic in place of strength when that is
// the higher. @param creature kInvalidObject for the item's own, each then met
// @return none for an item with none, or one that ignores them
//
// Call it, and CanUse, on the game thread.
std::vector<Requirement> Requirements(ObjectId item, ObjectId creature = kInvalidObject);

// Returns whether a creature can use an item, as the inventory greys it out: its attributes, the
// item's own conditions, and the slots the creature has.
bool CanUse(ObjectId creature, ObjectId item);

} // namespace dao::items
