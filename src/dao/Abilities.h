// Ability names, cooldowns, the abilities a creature has, and whether it can use one now and at
// what cost. Use it to show an ability by name, to hold or release a cooldown, or to check an
// order before giving it.
//
//   dao::abilities::Name(10003)   // L"Fireball"
//   dao::abilities::Label(10003)  // L"FIREBALL"
//   // Two thirds of the cooldown left: the quickbar's sweep starts two thirds full.
//   dao::abilities::SetCooldown(creature, ability, 2400.0F, 3600.0F);
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::abilities
{

// Returns an ability's label in the ability table, for example L"FIREBALL".
// @param ability the ability's id, a row of the ABI_base table
// @return the label, or nullopt if no row has that id or the tables are not loaded yet
std::optional<std::wstring> Label(std::int32_t ability);

// Returns an ability's name in the player's language, for example L"Fireball".
// @param ability the ability's id, a row of the ABI_base table
// @return the name; the label if the row has no name string; nullopt if no row has that id or the
// tables are not loaded yet
std::optional<std::wstring> Name(std::int32_t ability);

// An ability's cooldown on a creature, in seconds of game time.
struct Cooldown
{
    float remaining = 0; // seconds left; 0 when the ability is ready
    float total = 0;     // the length, in seconds, the cooldown was last set to
    // Returns the fraction of the cooldown still to run, from 0 to 1. This is what the quickbar's
    // sweep shows.
    float Fraction() const
    {
        return total > 0 ? remaining / total : 0.0F;
    }
};

// Returns an ability's cooldown on a creature.
// @param itemTag the tag of the item the ability comes from, like the scripts' sSourceItemTag;
// empty for the creature's own ability
// @return the cooldown, or nullopt if the creature does not exist or does not have the ability
std::optional<Cooldown> GetCooldown(ObjectId creature, std::int32_t ability, std::wstring_view itemTag = {});

// Sets an ability's cooldown to `remaining` seconds left out of `total`.
// @param remaining seconds of game time until the ability can be used again; 0 ends the
// cooldown, and a negative value counts as 0
// @param total the length the quickbar's sweep measures `remaining` against
// @param itemTag as for GetCooldown
// @return false, changing nothing, if the creature does not exist or does not have the ability,
// or if `total` is 0 or less while `remaining` is not 0
//
// Call it on the game thread. The scripts' SetCooldown sets both numbers to the same time; this
// lets the sweep start partly full. OnCooldownSet does not report calls to it.
bool SetCooldown(ObjectId creature, std::int32_t ability, float remaining, float total, std::wstring_view itemTag = {});

// A cooldown running on a creature.
struct RunningCooldown
{
    std::int32_t ability = 0;
    std::wstring itemTag; // the item the ability comes from; empty for the creature's own
    Cooldown cooldown;
};

// Returns every cooldown running on a creature, whatever started it: using the ability, a script,
// an item, or SetCooldown.
// @return the cooldowns, or an empty vector if the creature does not exist or has none running
//
// Call it on the game thread.
std::vector<RunningCooldown> Cooldowns(ObjectId creature);

// A cooldown the scripts' SetCooldown has just set, as reported to OnCooldownSet.
struct CooldownSet
{
    ObjectId creature = kInvalidObject;
    std::int32_t ability = 0;
    std::wstring itemTag; // the item the ability comes from; empty for the creature's own
    float seconds = 0;    // the seconds the script asked for; 0 ends the cooldown
    Cooldown cooldown;    // the cooldown as it reads after the game wrote it
    // Whether the cooldown's total now equals the seconds asked for, that is, whether the game's
    // write took effect. On an item's cooldown it takes while the party holds the item, and
    // may not once the last one is used.
    bool took = false;
};

// Registers a callback for every cooldown the scripts set, which includes every cooldown the game
// starts when an ability is used.
// @param observer receives the cooldown after the game has written it
//
// It runs on the game thread after the game's write, so a cooldown you set from the callback
// replaces the script's. It does not run for this library's own SetCooldown.
void OnCooldownSet(std::function<void(const CooldownSet &)> observer);

// The kinds of ability, numbered like the scripts' ABILITY_TYPE_* constants.
enum class Kind : std::int32_t
{
    All = 0,
    Talent = 1,
    Spell = 2,
    Skill = 3,
    Item = 4, // abilities of usable items, such as a poultice
};

// Returns every ability of one kind that a creature has, or of all kinds: its whole character
// sheet, not only what is on the quickbar.
// @param kind the kind to list; Kind::All for every kind
// @return the ability ids, or an empty vector if the creature does not exist
//
// The list includes hidden abilities, such as the class's own, and a skill appears once for each
// rank the creature has, each with its own id.
std::vector<std::int32_t> Known(ObjectId creature, Kind kind = Kind::All);

// Whether a creature can use an ability now, and if not, why not.
//
//   if (const auto use = dao::abilities::CanUse(jory, ability); use && !*use)
//       spdlog::info("not now: {}", dao::Utf8(use->Reason())); // "Insufficient stamina"
struct Use
{
    bool usable = false;
    std::int32_t reason = -1; // why not, as a talk table string id; -1 if usable or no reason given
    // Returns `usable`.
    explicit operator bool() const
    {
        return usable;
    }
    // Returns the reason in the player's language, for example "Insufficient stamina", or an empty
    // string if no reason was given.
    std::wstring Reason() const;
};

// Checks whether a creature can use an ability now, with the same test the game uses to grey out
// a quickbar button.
// @param itemTag as for GetCooldown
// @return the result, or nullopt if the creature does not exist
//
// Call it on the game thread. It checks the item, active modes, disabling effects, the cost, the
// ability's requirements (such as a weapon or a shield), the cooldown, and whether the ability is
// already being cast. The scripts check the cost again, slightly differently, when the command
// runs, so a sustained (modal) ability this reports as usable can still fail.
std::optional<Use> CanUse(ObjectId creature, std::int32_t ability, std::wstring_view itemTag = {});

// Returns what an ability costs a creature now in mana or stamina, as CanUse calculates it: the
// ability table's cost, raised by the creature's fatigue.
// @return the cost, or nullopt if the creature does not exist or the ability has no row
//
// Compare it with the creature's remaining mana or stamina (CCCreature::ManaStamina).
std::optional<float> Cost(ObjectId creature, std::int32_t ability);

} // namespace dao::abilities
