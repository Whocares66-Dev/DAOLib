// A creature's damage per hand, the character record's figure (dao/Damage.h; DAOLib #108, the
// turn-based mod's issue 112).
//
// Read 2026-10-13. The engine computes none of it: rows 49 (Damage_OffHand) and 50
// (Damage_MainHand) of properties.gda are plain attributes with EngineLink 0. The script
// RecalculateDisplayDamage(oCreature, nSlot) (core_h.nss) writes them, with CalculateWeaponDamage
// (the same file) for an armed hand: the weapon's mean of base and maximum damage, plus the
// attribute bonus times the weapon's factor, times the rank's fDamageScale, plus
// PROPERTY_ATTRIBUTE_DAMAGE_BONUS, times the attack speed by a style's magic number; staves have
// their own formula. Unarmed, the main hand is fists' damage plus the attribute bonus and the
// damage bonus. The off hand gets a figure only when dual wielding; otherwise -99 (an item that is
// not a weapon, or one not dual wielded), -97 (empty). The table's Min is 0, so those may arrive
// clamped to 0: anything not above 0 is no off-hand weapon.
//
// Two scripts call it. rules_core, on EVENT_TYPE_EQUIP and UNEQUIP, for party members alone, which
// is why an enemy's properties read 0 (seen 2026-10-13 by the turn-based mod: an armed cultist, a
// dragonling). module_core, on module event 95 (EVENT_TYPE_CHAR_RECORD_OPENED), for event object 0,
// with no party check, so that event is sent here. The same case also runs
// STATS_HandlePercentDamageDealt (nothing for a creature outside the party: it checks the hero's and
// the followers' tags), STATS_SetGameTimePlayed (the hero's play-time stat, rewritten with the play
// time) and ACH_CalculatePercentageComplete (counts achievements already had and sets the
// completion percentage; grants nothing). No script reads properties 49 and 50.
//
// Not yet seen in play: that SetCreatureProperty takes 49 and 50 on a creature outside the party,
// that the event is handled before SignalEvent returns, and Awakening's and the DLCs' module
// scripts passing event 95 on to module_core.
#include "dao/Damage.h"

#include "dao/ScriptHooks.h"
#include "dao/generated/ScriptApi.h"
#include "dao/generated/ScriptConstants.h"

#include <Windows.h>

#include <cstdint>

namespace dao::damage
{

namespace
{
constexpr std::int32_t kCharRecordOpened = 95; // EVENT_TYPE_CHAR_RECORD_OPENED
constexpr std::int32_t kDamageOffHand = 49;    // properties.gda
constexpr std::int32_t kDamageMainHand = 50;
} // namespace

std::optional<SheetDamage> ForSheet(ObjectId creature)
{
    if (!script::CommandsReady() || GetCurrentThreadId() != script::ScriptThreadId())
    {
        return std::nullopt;
    }
    namespace api = script::api;
    if (api::GetObjectType(creature) != script::constants::OBJECT_TYPE_CREATURE)
    {
        return std::nullopt;
    }
    auto event = api::Event(kCharRecordOpened);
    if (event)
    {
        event = api::SetEventObject(*event.get(), 0, creature);
    }
    const ObjectId module = api::GetModule();
    if (!event || module == kInvalidObject)
    {
        return std::nullopt;
    }
    api::SignalEvent(module, *event.get(), 1);

    SheetDamage damage;
    damage.mainHand = api::GetCreatureProperty(creature, kDamageMainHand);
    if (const float off = api::GetCreatureProperty(creature, kDamageOffHand); off > 0)
    {
        damage.offHand = off;
    }
    return damage;
}

} // namespace dao::damage
