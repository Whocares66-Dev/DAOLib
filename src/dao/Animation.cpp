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
