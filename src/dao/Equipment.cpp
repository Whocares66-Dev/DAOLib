// Equipment changes (dao/Equipment.h), by detours on CCCreature's equip and unequip.
//
// Read 2026-10-04 (DAOLib #22). Every path that equips or unequips meets in two of CCCreature's
// functions: the inventory screen's EquipItemMessage, the scripts' EquipItem and UnequipItem, a
// creature's own sub-actions 19 and 20 (an equip and an unequip queued as part of a command), the
// items an equip displaces, a plugin's own call.
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
// ammunition. An area's load equips every creature's ammunition (arrows,
// bolts, a staff's projectile), each told: a line a creature.
#include "dao/Equipment.h"

#include "dao/Addresses.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Objects.h"
#include "dao/generated/ScriptApi.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

namespace dao::equipment
{

namespace
{

std::mutex g_lock;
std::vector<std::function<void(const Change &)>> g_equips;
std::vector<std::function<void(const Change &)>> g_unequips;

std::vector<std::function<void(const Change &)>> Copy(const std::vector<std::function<void(const Change &)>> &from)
{
    const std::lock_guard lock(g_lock);
    return from;
}

ObjectId IdOf(const CCObject *object)
{
    return object != nullptr ? object->Id() : kInvalidObject;
}

void Tell(const std::vector<std::function<void(const Change &)>> &observers, const Change &change)
{
    for (const auto &observer : Copy(observers))
    {
        detail::Guard("an equipment callback", [&] { observer(change); });
    }
}

// The items the unequip told of while an equip ran, on this thread, so the
// equip does not tell of them again as displaced.
thread_local std::vector<ObjectId> t_toldInEquip;
thread_local int t_equipDepth = 0;

// INVENTORY_SLOT_MAIN (0) to INVENTORY_SLOT_DOG_COLLAR (19).
constexpr std::size_t kSlots = 20;

// CCCreature's equip: __thiscall (CCItem *item, slot, weapon set), `ret 0xC`.
// What was in the slot it fills it moves to the inventory itself
// (0x00762FB0, the list at its result's +0x20), without the unequip: seen
// in play 2026-10-04, Dar'Misu put in Lyna's off hand and no unequip for
// the Dagger it held. So the creature's slots are read before and after,
// by the engine's own GetItemInEquipSlot for the set the equip is for (-1
// the active one, as the equip takes it), and an item gone from its slot
// is told of as displaced.
using EquipFn = bool(__fastcall *)(CCObject *, int, CCObject *, std::int32_t, std::int32_t);
Detour<EquipFn> g_equip;

bool __fastcall Equip(CCObject *creature, int edx, CCObject *item, std::int32_t slot, std::int32_t set)
{
    const ObjectId who = IdOf(creature);
    const ObjectId what = IdOf(item);
    const bool watch = who != kInvalidObject && !Copy(g_unequips).empty();
    std::array<ObjectId, kSlots> before{};
    if (watch)
    {
        for (std::size_t i = 0; i < kSlots; ++i)
        {
            before[i] = script::api::GetItemInEquipSlot(static_cast<std::int32_t>(i), who, set);
        }
    }
    if (t_equipDepth++ == 0)
    {
        t_toldInEquip.clear();
    }
    const bool done = g_equip.Original()(creature, edx, item, slot, set);
    --t_equipDepth;
    if (watch && done)
    {
        for (std::size_t i = 0; i < kSlots; ++i)
        {
            const ObjectId was = before[i];
            if (was == kInvalidObject || was == what || std::ranges::find(t_toldInEquip, was) != t_toldInEquip.end() ||
                script::api::GetItemInEquipSlot(static_cast<std::int32_t>(i), who, set) == was)
            {
                continue;
            }
            Change displaced{who, was};
            displaced.slot = static_cast<std::int32_t>(i);
            displaced.done = true;
            displaced.displaced = true;
            Tell(g_unequips, displaced);
        }
    }
    Change change{who, what};
    change.slot = what != kInvalidObject ? script::api::GetItemEquipSlot(what) : slot;
    change.done = done;
    Tell(g_equips, change);
    return done;
}

// CCCreature's unequip: __thiscall (Result *out, CCItem *item), `ret 8`,
// answering out; its first byte the success.
using UnequipFn = std::uint8_t *(__fastcall *)(CCObject *, int, std::uint8_t *, CCObject *);
Detour<UnequipFn> g_unequip;

std::uint8_t *__fastcall Unequip(CCObject *creature, int edx, std::uint8_t *out, CCObject *item)
{
    Change change{IdOf(creature), IdOf(item)};
    change.slot = change.item != kInvalidObject ? script::api::GetItemEquipSlot(change.item) : change.slot;
    std::uint8_t *result = g_unequip.Original()(creature, edx, out, item);
    change.done = result != nullptr && *result != 0;
    if (t_equipDepth > 0)
    {
        t_toldInEquip.push_back(change.item);
    }
    Tell(g_unequips, change);
    return result;
}

// Both detours at once: an item an equip displaces is told to OnUnequip.
void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_equip.Attach(addr::kCCCreature_Equip, &Equip);
        g_unequip.Attach(addr::kCCCreature_Unequip, &Unequip);
    });
}

} // namespace

void OnEquip(std::function<void(const Change &change)> observer)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_equips.push_back(std::move(observer));
}

void OnUnequip(std::function<void(const Change &change)> observer)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_unequips.push_back(std::move(observer));
}

} // namespace dao::equipment
