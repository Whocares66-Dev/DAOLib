// A creature's equipment changing, by any path: the inventory screen's
// EquipItemMessage, the scripts' EquipItem and UnequipItem, a creature's
// own sub-actions 19 and 20 (an equip and an unequip queued as part of a
// command), the items an equip displaces, a plugin's own call. An area's
// load equips every creature's ammunition (arrows, bolts, a staff's
// projectile), each told: seen in play 2026-10-04, a line a creature.
//
//   dao::equipment::OnEquip([](const dao::equipment::Change &change) { ... });
//
// Read 2026-10-04 (DAOLib #22). Every path meets in two of CCCreature's
// functions:
// - the equip (0x006BC7F0), __thiscall (CCItem *item, slot, weapon set),
//   `ret 0xC`, success in al: EquipItemMessage's handler (0x006BE330)
//   calls it with the message's item and slot and set -1, the scripts'
//   EquipItem with theirs. A slot of 255 lets the item go to its own.
// - the unequip (0x006B7690), __thiscall (Result *out, CCItem *item),
//   `ret 8`, answering out, whose first byte is the success. The equip
//   calls it for ammunition that no longer fits (three places); what was
//   in the slot it fills it moves to the inventory itself, without the
//   unequip (seen in play 2026-10-04), so the creature's slots are read
//   before and after an equip, and an item gone is told to OnUnequip as
//   displaced. Either way OnUnequip hears of it before OnEquip.
// One script command function serves both EquipItem and UnequipItem
// (0x0065CDA0), branching on the command's id: the equip for EquipItem,
// else the unequip, whose result's first byte it answers before
// destroying the result (0x007616C0). The sub-actions are the sub-action
// table's 19 (0x007A25C0) and 20 (0x007A2650), and five more of the
// engine's functions call each (read by the address library's session,
// 2026-10-04).
// An observer is told after the engine's call, on the game's thread.
// Seen in play 2026-10-04: an off hand's Dagger told as displaced before
// Dar'Misu went on, a plain unequip, a refused two-hander, and the load's
// ammunition.
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <functional>

namespace dao::equipment
{

struct Change
{
    ObjectId creature = kInvalidObject;
    ObjectId item = kInvalidObject;
    // The item's slot (an INVENTORY_SLOT_* constant), as GetItemEquipSlot
    // answers it: after an equip, the slot it went to; before an unequip,
    // the one it left. 255 (INVENTORY_SLOT_INVALID) for none.
    std::int32_t slot = 255;
    bool done = false; // whether the engine did it
    // OnUnequip: taken off by an equip that put something in its place,
    // not by the unequip.
    bool displaced = false;
};

void OnEquip(std::function<void(const Change &change)> observer);
void OnUnequip(std::function<void(const Change &change)> observer);

} // namespace dao::equipment
