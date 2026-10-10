// How long animations, attacks and abilities take (dao/Animation.h).
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
// Seen in play 2026-10-02 (examples/animation, two fights in the GOG game,
// one in the turn-based mod's turns and one in real time, the same figures
// in both; build\playtest-requests6-10-02-animation-results):
// - An animation-timed action's command ends at about four fifths of the
//   animation, the blend: giant spiders' attacks, Length(kAttack) 1.833 s,
//   took a median 1.49 s (with a second group near 1.15 s, another attack
//   animation of theirs); Dirty Fighting's cast, 1.800 s, took 1.51 s;
//   a spider's Web, 2.433 s, took 2.07 s.
// - A humanoid's scripted swing takes the scripts' time: Lyna's daggers,
//   the scripts' 1.000 s, took 1.02 s, 46 times running.
// - Bows: Length(kAttack) 1.500 s says nothing of a shot, which took 1.39 to
//   1.42 s for a follower and 1.58 to 1.62 s for the player.
// - AimDuration read nothing at any time, a bow's shot included.
// - An ability whose row has no CastAnim (Pinning Shot, a spider's
//   Overwhelm) reads nothing here, though it plays something (1.7 s, 2.4 s).
//   Read after: the use-ability step (0x007A7440) sends the ability's
//   script EVENT_TYPE_ABILITY_CAST_START, and then, with a CastAnim, plays
//   it and lets the impact come at its point; with none, it sends
//   EVENT_TYPE_ABILITY_CAST_IMPACT at once and plays nothing. What such an
//   ability takes is what its script does then: an archery talent's is a
//   shot (Pinning Shot's 1.7 s, a shot's 1.4 to 1.6 s and the command's
//   ends), a monster's special its own.
#include "dao/Animation.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Gda.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"

#include <cstddef>

namespace dao::animation
{

namespace
{

constexpr std::size_t kCombat = 0xC60;  // the creature's combat record
constexpr std::size_t kAimMs = 0x60;    // SetAimLoopDuration's milliseconds
constexpr std::size_t kAttackMs = 0x64; // SetAttackDuration's milliseconds
constexpr float kMillisecond = 0.001F;

CCCreature *CreatureOf(ObjectId creature)
{
    CClientExoApp *app = CClientExoApp::Get();
    return app != nullptr ? app->GetCreatureById(creature) : nullptr;
}

std::optional<float> Milliseconds(ObjectId creature, std::size_t field)
{
    const CCCreature *object = CreatureOf(creature);
    if (object == nullptr)
    {
        return std::nullopt;
    }
    const auto record = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(object) + kCombat);
    if (!record || *record == 0)
    {
        return std::nullopt;
    }
    const auto ms = TryRead<std::int32_t>(*record + field);
    if (!ms || *ms <= 0)
    {
        return std::nullopt;
    }
    return static_cast<float>(*ms) * kMillisecond;
}

} // namespace

std::optional<float> Length(ObjectId creature, std::int32_t animation)
{
    CCCreature *object = CreatureOf(creature);
    if (object == nullptr)
    {
        return std::nullopt;
    }
    using HasFn = bool(__fastcall *)(CCCreature *, int, std::int32_t);
    if (!reinterpret_cast<HasFn>(Module::Address(addr::kCCCreature_HasAnimation))(object, 0, animation))
    {
        return std::nullopt;
    }
    using LengthFn = std::int32_t(__fastcall *)(CCCreature *, int, std::int32_t);
    const std::int32_t ms =
        reinterpret_cast<LengthFn>(Module::Address(addr::kCCCreature_AnimationLength))(object, 0, animation);
    if (ms <= 0)
    {
        return std::nullopt;
    }
    return static_cast<float>(ms) * kMillisecond;
}

// AttackDuration: set when an attack is rolled, so it is the last attack's
// until the next. In play it read nothing at an attack's start and the
// scripts' value at its end (1.000 s for daggers, 0.600 s for bows): set
// while the attack is under way, so a cost reckoned ahead of an attack
// takes the scripts' formula instead.
std::optional<float> AttackDuration(ObjectId creature)
{
    return Milliseconds(creature, kAttackMs);
}

std::optional<float> AimDuration(ObjectId creature)
{
    return Milliseconds(creature, kAimMs);
}

namespace
{

// An animation's length, if it is one the creature has; nothing for 0, the
// tables' "none".
std::optional<float> LengthOf(ObjectId creature, std::optional<std::int32_t> animation)
{
    return animation && *animation > 0 ? Length(creature, *animation) : std::nullopt;
}

std::optional<std::int32_t> TransitionOf(std::optional<std::int32_t> animation)
{
    if (!animation || *animation <= 0)
    {
        return std::nullopt;
    }
    return gda::GetInt(script::Table::BaseAnimations, *animation, "TransitionTo");
}

} // namespace

// From the abilities table (TABLE_ABILITIES_SPELLS, ConjureAnim and
// CastAnim) and the animations table's TransitionTo (TABLE_BASE_ANIMATIONS),
// which says what each animation passes into: a spell's conjure start
// (244, "Conjure Direct start") into its loop (264), its cast (246, "Cast
// Direct") into its end (290); a talent has a cast alone (Dirty Fighting's
// 146), a potion its drink (257). How many times the loop plays is the
// caller's to know (the scripts' conjure tables count loops, not seconds).
// Read 2026-10-02 from the tables; the casts seen in play the same day
// (the file's head), no conjure yet.
Ability OfAbility(ObjectId creature, std::int32_t ability)
{
    const auto conjure = gda::GetInt(script::Table::AbilitiesSpells, ability, "ConjureAnim");
    const auto cast = gda::GetInt(script::Table::AbilitiesSpells, ability, "CastAnim");
    // TransitionTo 1 is the idle ("Pause"), not a part of the ability.
    const auto loop = TransitionOf(conjure);
    const auto end = TransitionOf(cast);
    return Ability{.conjure = LengthOf(creature, conjure),
                   .conjureLoop = loop && *loop > 1 ? LengthOf(creature, loop) : std::nullopt,
                   .cast = LengthOf(creature, cast),
                   .castEnd = end && *end > 1 ? LengthOf(creature, end) : std::nullopt};
}

} // namespace dao::animation
