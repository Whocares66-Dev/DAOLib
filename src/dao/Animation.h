// Measures how long a creature's animations, attacks and abilities take.
//
// Use it to cost an action before the game plays it, for example to fill a turn-based mode's
// time bar. Call these functions on the game thread (dao/Frame.h).
//
//   const auto swing = dao::animation::Length(creature, dao::animation::kAttack);
#pragma once

#include "dao/Structures.h"

#include <cstdint>
#include <optional>

namespace dao::animation
{

// The generic attack animation, row "Attack v1" (attk_v1) of anim_combat. Every other attack
// animation falls back to it, and an attack with no animation of its own plays it.
inline constexpr std::int32_t kAttack = 107;

// Returns how long one of a creature's animations lasts.
// @param animation a row of the anim_ tables (anim_combat for attacks)
// @return the length in seconds; nullopt if the creature is not loaded or lacks the animation
//
// An action timed by its animation ends at about four fifths of this length, where it blends
// into the next one. A bow's shot does not follow kAttack's length.
std::optional<float> Length(ObjectId creature, std::int32_t animation);

// Returns the attack time the scripts last set for a creature with SetAttackDuration.
// @return seconds; nullopt if none is set or the creature is not loaded
//
// The scripts set it for a humanoid with a weapon once the attack has been rolled, during the
// attack, so it is not known before an attack. It then keeps the last attack's value until the
// next one. An attack with it set takes about that long; one without it takes about four fifths
// of its animation's Length.
std::optional<float> AttackDuration(ObjectId creature);

// Returns the aim time the scripts last set for a creature's bow with SetAimLoopDuration.
// @return seconds; nullopt if none is set or the creature is not loaded
//
// So far it has always returned nullopt, even for creatures using bows.
std::optional<float> AimDuration(ObjectId creature);

// The lengths of an ability's animations for one creature, in seconds. A field is nullopt where
// the ability has no such animation or the creature lacks it.
struct Ability
{
    std::optional<float> conjure;     // the start of a spell's conjure, its wind-up before the cast
    std::optional<float> conjureLoop; // one pass of the conjure's looping part
    std::optional<float> cast;        // the cast itself, or drinking a potion
    std::optional<float> castEnd;     // the animation the cast blends into
};

// Returns the lengths of a talent's or spell's animations for a creature.
// @param ability a row of the abilities table (TABLE_ABILITIES_SPELLS)
//
// The caller decides how many times the conjure loop plays, because the scripts' conjure tables
// count loops, not seconds. An ability with no cast animation, such as Pinning Shot or a
// spider's Overwhelm, has none here, though its script may still play something.
Ability OfAbility(ObjectId creature, std::int32_t ability);

} // namespace dao::animation
