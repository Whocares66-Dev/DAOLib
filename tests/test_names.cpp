#include "dao/core/CommandWord.h"
#include "dao/generated/ScriptConstants.h"

#include <catch2/catch_test_macros.hpp>

using dao::script::EffectType;
using dao::script::EventType;

TEST_CASE("a constant's script name, from its enum value")
{
    CHECK(Name(EventType::CommandComplete) == "EVENT_TYPE_COMMAND_COMPLETE");
    CHECK(Name(EffectType::Root) == "EFFECT_TYPE_ROOT");
    CHECK(Name(dao::script::PlaceableAction::Destroy) == "PLACEABLE_ACTION_DESTROY");
    // A value no constant names has none.
    CHECK(Name(static_cast<EffectType>(987654)).empty());
    // From 2da_constants_h.nss: what EVENT_TYPE_DAMAGED's integer 0 carries.
    CHECK(Name(dao::script::DamageType::Fire) == "DAMAGE_TYPE_FIRE");
}

TEST_CASE("a command type's name")
{
    using dao::core::CommandType;
    CHECK(Name(CommandType::UseAbility) == "UseAbility");
    CHECK(Name(CommandType::MoveToLocation) == "MoveToLocation");
    CHECK(Name(static_cast<CommandType>(200)).empty());
}
