// A creature's level-up as the level-up screen does it, with no screen open: what it is offered,
// its points, an ability or a specialization taken, an attribute raised, the screen's Auto Level
// Up, and its Done. Each is refused where the screen would refuse it, by the engine's own test.
//
//   for (const auto &offer : dao::levelup::Offers(hero, dao::levelup::Kind::Talents))
//       if (offer.state == dao::levelup::State::Learnable)
//           dao::levelup::Take(hero, offer.ability);
//   dao::levelup::Finish(hero);
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dao::levelup
{

// The kinds of ability the screen offers, each on a page of its own.
enum class Kind : std::int32_t
{
    Talents = 1,
    Spells = 2,
    Skills = 3,
};

// How the screen shows an ability.
enum class State : std::int32_t
{
    Have = 1,      // the creature has it
    Learnable = 2, // it may take it, if it has a point to spend
    Locked = 3,    // its level, a prerequisite or its training is not met
};

// Which screen's offers: the level-up screen's or the creation screen's. The creation screen also
// offers only what its ShowInCharGen allows.
enum class Mode : std::int32_t
{
    CharGen = 1,
    LevelUp = 2,
};

// An ability the screen shows, with how it shows it.
struct Offer
{
    std::int32_t ability = 0; // its row in abi_base
    State state = State::Locked;
};

// Returns the abilities of a kind the screen shows a creature, in the screen's order.
// @return none for a creature with no party data (one that has never been a party member), and
// none of the kind the screen does not show it: spells for a class without mana, talents for one with
//
// Call it on the game thread, in a module. An open level-up or creation screen redraws from it.
std::vector<Offer> Offers(ObjectId creature, Kind kind, Mode mode = Mode::LevelUp);

// Returns the specializations the screen offers a creature's class, in the screen's order, an
// add-in's among them: Have if it has one, Learnable if it is unlocked (a manual or a trainer) and
// the engine's availability test passes, else Locked. Whether it has a point for one is PointsOf's.
// @return none for an object that is no creature
std::vector<Offer> Specializations(ObjectId creature);

// Returns a specialization's name as the screen shows it ("Bard"), its class's in Classes: the
// ability's own name in the abilities' table is a hidden one ("HIDDEN_BARD").
// @return empty for an ability that is no specialization
std::wstring SpecializationName(std::int32_t ability);

// A creature's points to spend.
struct Points
{
    std::int32_t attributes = 0;
    std::int32_t skills = 0;
    std::int32_t talentsOrSpells = 0;
    std::int32_t specializations = 0;
};

// Returns a creature's points to spend. @return nullopt for an object that is no creature
std::optional<Points> PointsOf(ObjectId creature);

// Why a level-up step was refused, or None if it was taken.
enum class Refusal : std::uint8_t
{
    None,
    NoCreature, // the object is no creature, or it has no party data
    NotOffered, // the screen does not show the ability, or the attribute is not one of the six
    Held,       // the creature has the ability already
    Locked,     // the engine's availability test refuses it (CCCreatureStats::IsAbilityAvailable), or a
                // specialization is not unlocked
    NoPoints,   // the creature has no point of the kind it takes
};

// Takes a skill, talent or spell, as a click on its icon does. Refused unless the screen shows it
// learnable and the creature has a point of its kind.
Refusal Take(ObjectId creature, std::int32_t ability, Mode mode = Mode::LevelUp);

// Takes a specialization by its ability (Bard 4015, Champion 4013, ...), as the screen's choice
// does. Refused NotOffered unless it is one of the creature's base class's, Locked unless it is
// unlocked (a manual or a trainer) and the engine's availability test passes, NoPoints unless the
// creature has a point for it.
Refusal TakeSpecialization(ObjectId creature, std::int32_t ability);

// Raises an attribute by some points, as its plus button does.
// @param attribute PROPERTY_ATTRIBUTE_STRENGTH (1) to PROPERTY_ATTRIBUTE_CONSTITUTION (6)
Refusal RaiseAttribute(ObjectId creature, std::int32_t attribute, std::int32_t points);

// Spends the creature's points as the screen's Auto Level Up does, by its class's table. It
// leaves a specialization's point, and may leave points it found nothing for.
void AutoLevelUp(ObjectId creature);

// Ends the level-up as the screen's Done does: the tactics slots, whether it can level up again,
// its regeneration. Call it once the points are spent, and only with no level-up screen open: the
// screen's own Done does the same.
void Finish(ObjectId creature);

} // namespace dao::levelup
