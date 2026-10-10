// Callbacks for items being equipped on and unequipped from a creature, whatever caused it: the
// inventory screen, the scripts' EquipItem and UnequipItem, a creature's own queued equips, or a
// plugin's call.
//
//   dao::equipment::OnEquip([](const dao::equipment::Change &change) { ... });
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <functional>

namespace dao::equipment
{

// One item equipped or unequipped, as reported to a callback.
struct Change
{
    ObjectId creature = kInvalidObject;
    ObjectId item = kInvalidObject;
    // The item's INVENTORY_SLOT_* slot: where it went for an equip, or where it was for an
    // unequip. 255 (INVENTORY_SLOT_INVALID) for none.
    std::int32_t slot = 255;
    bool done = false;      // whether the game did it; false if it refused
    bool displaced = false; // OnUnequip only: pushed out by an item equipped into its slot
};

// Registers a callback for every equip, called once the game has handled it.
// @param observer receives the item, its slot and whether it was equipped
//
// It runs on the game thread. Loading an area equips every creature's ammunition, and each of
// those equips is reported too.
void OnEquip(std::function<void(const Change &change)> observer);

// Registers a callback for every unequip, called once the game has handled it.
// @param observer receives the item, the slot it left and whether it came off
//
// It runs on the game thread. When an equip pushes an item out of its slot, that item is reported
// here with `displaced` set, before OnEquip reports the item that replaced it.
void OnUnequip(std::function<void(const Change &change)> observer);

} // namespace dao::equipment
