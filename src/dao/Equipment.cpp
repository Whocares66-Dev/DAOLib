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
