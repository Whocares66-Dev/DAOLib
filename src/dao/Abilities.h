// An ability's name, for a person to read: its label in the ability table
// and its name in the player's language. For logs and tools that would
// otherwise show an ability's number.
//
//   dao::abilities::Name(10003)   // L"Fireball"
//   dao::abilities::Label(10003)  // L"FIREBALL"
//
// Read 2026-09-30 from the scripts: an ability's row in TABLE_ABILITIES_SPELLS
// (1, ABI_base, every ability, by its id) has Label and NameStrRef, and the
// ability bar names an ability by NameStrRef through the talk tables
// (ui_h.nss). Both go through dao/Gda.h and dao/Tlk.h; nothing comes back
// for an id with no row, or before the tables are loaded.
//
// And an ability's cooldown on a creature, with the two numbers the
// scripts cannot set apart:
//
//   // two of three turns still to wait: the slot's sweep two thirds full
//   dao::abilities::SetCooldown(creature, ability, 2400.0F, 3600.0F);
//
// Read 2026-09-30 (dao/Addresses.h, "Cooldowns"). A creature has a record
// for each ability it has, which holds the seconds the cooldown was last
// set to, the seconds left, and the share of the one the other is. Each
// frame of game time the engine takes the frame's seconds from what is
// left and sets the share to what is left over what it was set to; the
// record is what the creature's ability list hands the interface, so the
// share is what a quickbar slot's sweep can show. The scripts' SetCooldown
// sets both numbers to the one time, so a long hold set that way shows a
// full sweep that never moves. GetCooldown and SetCooldown here read and
// set the two. Seen in play, 2026-10-01 (examples/cooldown): Powerful
// Swings set to 800 s left of 1200 s read back 800 of 1200, ticked a second
// a second, and its quickbar slot drew a sweep about two thirds dark.
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace dao::abilities
{

std::optional<std::wstring> Label(std::int32_t ability);
// The label when the row names no string.
std::optional<std::wstring> Name(std::int32_t ability);

struct Cooldown
{
    float remaining = 0; // seconds of game time left
    float total = 0;     // seconds it was last set to
    // What is left over what it was set to, 0 to 1: the sweep.
    float Fraction() const
    {
        return total > 0 ? remaining / total : 0.0F;
    }
};

// An ability's cooldown on a creature; nothing for a creature that is not
// there or has no record for the ability (it does not have the ability). An
// ability ready to use reads 0 left. `itemTag` is the tag of the item an
// item's ability comes from, as the scripts' sSourceItemTag.
std::optional<Cooldown> GetCooldown(ObjectId creature, std::int32_t ability, std::wstring_view itemTag = {});

// Sets an ability's cooldown to `remaining` seconds left of `total`: the
// ability cannot be used for `remaining` seconds of game time, and its
// sweep shows remaining over total, running down from there as the time
// passes. 0 left ends the cooldown. False, and nothing done, for a creature
// that is not there or has no record for the ability, or a total of 0 or
// less with time left. On the game's thread.
bool SetCooldown(ObjectId creature, std::int32_t ability, float remaining, float total, std::wstring_view itemTag = {});

} // namespace dao::abilities
