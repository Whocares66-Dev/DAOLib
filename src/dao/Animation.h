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

// The two animations of a shot with a bow, a crossbow or a staff, rows "Aim" and "Fire" of
// anim_base: raising the weapon, then loosing.
inline constexpr std::int32_t kAim = 249;
inline constexpr std::int32_t kFire = 243;

// Returns how long one of a creature's animations lasts.
// @param animation a row of the anim_ tables (anim_combat for attacks)
// @return the length in seconds; nullopt if the creature is not loaded or lacks the animation
//
// An action timed by its animation ends at about four fifths of this length, where it blends
// into the next one. A bow's shot does not follow kAttack's length; OfShot gives a shot's.
std::optional<float> Length(ObjectId creature, std::int32_t animation);

// Returns how long a command gives one of a creature's animations when it plays it as a step at
// normal speed: the animation's length, plus those of the animations it runs on into, up to the
// first that loops.
// @param animation a row of the anim_ tables
// @return the length in seconds; nullopt if the creature is not loaded or lacks the animation
std::optional<float> StepLength(ObjectId creature, std::int32_t animation);

// Returns the attack time the scripts have set for a creature's attack with SetAttackDuration.
// @return seconds; nullopt if none is set or the creature is not loaded
//
// The scripts set it for a humanoid with a weapon once the attack has been rolled, during the
// attack, and the game clears it as the attack ends, so it is set only while an attack is under
// way. A melee attack with it set takes about that long; one without it takes about four fifths
// of its animation's Length. For a shot it is the time of the Aim animation alone (OfShot).
std::optional<float> AttackDuration(ObjectId creature);

// Returns the aim time the scripts have set for a creature's shot with SetAimLoopDuration: how
// long the shot holds its aim before the Fire animation.
// @return seconds; nullopt if none is set or the creature is not loaded
//
// Like the attack time, it is set only while an attack is under way. A staff's has been 0.1 s; a
// bow's can be none.
std::optional<float> AimDuration(ObjectId creature);

// The three parts of a shot with a bow, a crossbow or a staff, in the order they play, in
// seconds.
struct Shot
{
    // Raising the weapon: the Aim animation, sped or slowed to take the scripts' attack time when
    // there is one. 0 for a creature with no Aim animation, such as one holding a staff.
    float aim = 0;
    float aimLoop = 0; // holding the aim: the scripts' aim time, 0 when there is none
    float fire = 0;    // loosing: the Fire animation and what it runs on into

    // Returns the whole shot's time, from the end of the turn to face the target.
    float Total() const
    {
        return aim + aimLoop + fire;
    }
};

// Returns the parts of a creature's shot, timed as the game times them.
// @param attackTime the scripts' attack time for the shot, in seconds; by default the one set
// on the creature now (AttackDuration)
// @param aimTime the scripts' aim time for the shot, in seconds; by default the one set on the
// creature now (AimDuration)
// @return nullopt if the creature is not loaded or lacks the Fire animation
//
// The scripts set both times as a shot begins, so the defaults give a shot's real parts only
// while it is under way. To cost a shot beforehand, pass the times the scripts would set.
std::optional<Shot> OfShot(ObjectId creature, std::optional<float> attackTime = std::nullopt,
                           std::optional<float> aimTime = std::nullopt);

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
