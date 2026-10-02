// How long a creature's animations take as the engine times them, and the
// attack's timing the scripts hand the engine: what a turn-based mode's
// time bar needs to cost an attack as the game will play it.
//
//   const auto swing = dao::animation::Length(creature, dao::animation::kAttack);
//
// Read 2026-10-02. A creature's animation set (its +0x154) answers how long
// an animation is, in milliseconds (0x00694E50, __thiscall (id), `ret 4`),
// and whether it has one (0x00694E70). An animation's id is a row of the
// anim_ tables (anim_combat for an attack's).
//
// An attack's swing (0x006A41A0, the step that plays it): the scripts set
// its time for a humanoid with a weapon, after the attack is rolled
// (combat_h.nss, SetAttackDuration with core_h.nss's CalculateAttackTiming;
// SetAimLoopDuration for a bow's aim), and the engine keeps it in the
// creature's combat record (+0xC60) in milliseconds, +0x64 the attack's and
// +0x60 the aim's. With one set the swing's animation is sped or slowed to
// take that time, after a blend of a fifth of it; with none (a creature
// that is not humanoid, or no weapon) it takes the animation's own length,
// less a blend of a fifth of that. An attack that has no animation of its
// own plays kAttack.
//
// From the frame's thread. Not yet seen in play.
#pragma once

#include "dao/Structures.h"

#include <cstdint>
#include <optional>

namespace dao::animation
{

// anim_combat's "Attack v1" (attk_v1): the attack animation every other
// falls back to (the table's Replacement column), and the one the swing
// plays when it is given none it has.
inline constexpr std::int32_t kAttack = 107;

// How long a creature's animation is, in seconds. Nothing if the creature
// is not there or has no such animation.
std::optional<float> Length(ObjectId creature, std::int32_t animation);

// The attack time the scripts last set for a creature (SetAttackDuration),
// and a bow's aim (SetAimLoopDuration), in seconds. Nothing if none is set
// or the creature is not there. Set when an attack is rolled, so it is the
// last attack's until the next.
std::optional<float> AttackDuration(ObjectId creature);
std::optional<float> AimDuration(ObjectId creature);

// A talent's or spell's animations for a creature, each one's length in
// seconds, nothing where the ability has none or the creature lacks it.
// From the abilities table (TABLE_ABILITIES_SPELLS, ConjureAnim and
// CastAnim) and the animations table's TransitionTo (TABLE_BASE_ANIMATIONS),
// which says what each animation passes into: a spell's conjure start
// (244, "Conjure Direct start") into its loop (264), its cast (246, "Cast
// Direct") into its end (290); a talent has a cast alone (Dirty Fighting's
// 146), a potion its drink (257). How many times the loop plays is the
// caller's to know (the scripts' conjure tables count loops, not seconds).
// Read 2026-10-02 from the tables. Not yet seen in play.
struct Ability
{
    std::optional<float> conjure;     // the conjure's start
    std::optional<float> conjureLoop; // one pass of its loop
    std::optional<float> cast;
    std::optional<float> castEnd;
};
Ability OfAbility(ObjectId creature, std::int32_t ability);

} // namespace dao::animation
