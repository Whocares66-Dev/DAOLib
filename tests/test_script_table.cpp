#include "dao/core/ScriptValue.h"
#include "dao/generated/ScriptTable.h"

#include <catch2/catch_test_macros.hpp>

#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using dao::core::CompleteArguments;
using dao::core::DefaultKind;
using dao::core::ScriptType;
using dao::core::ScriptValue;
namespace table = dao::script::table;

TEST_CASE("the table finds a command by name, in any case, and by number")
{
    const auto *team = table::Find("GetTeamId");
    REQUIRE(team != nullptr);
    CHECK(team->id == 807);
    CHECK(team->returns == ScriptType::Int);
    REQUIRE(team->params.size() == 1);
    CHECK(team->params[0].type == ScriptType::Object);
    CHECK(team->params[0].defaultKind == DefaultKind::None);
    CHECK(table::Find("getteamid") == team);
    CHECK(table::Find(807) == team);
    CHECK(table::Find("NoSuchCommand") == nullptr);
    CHECK(table::Find(-5) == nullptr);
}

TEST_CASE("every command in the table has a name and a number of its own")
{
    std::set<std::string_view> names;
    std::set<std::int32_t> ids;
    for (const auto &command : table::kCommands)
    {
        CHECK(names.insert(command.name).second);
        CHECK(ids.insert(command.id).second);
        // Defaults trail, as C++'s must: none before a parameter without one.
        bool required = false;
        for (const auto &param : std::views::reverse(command.params))
        {
            if (param.defaultKind == DefaultKind::None)
            {
                required = true;
            }
            else
            {
                CHECK_FALSE(required);
            }
        }
    }
    CHECK(names.size() > 600);
}

TEST_CASE("a default of OBJECT_INVALID is the engine's -1, and constants are resolved")
{
    const auto *remove = table::Find("RemoveEffectsByParameters");
    REQUIRE(remove != nullptr);
    REQUIRE(remove->params.size() == 4);
    const auto creator = remove->params[3].Default();
    REQUIRE(creator);
    CHECK(creator->type == ScriptType::Object);
    CHECK(creator->object == dao::kInvalidObject);
    CHECK(creator->object == 0xFFFFFFFF);
    const auto type = remove->params[1].Default();
    REQUIRE(type);
    CHECK(type->integer == 0); // EFFECT_TYPE_INVALID
}

TEST_CASE("arguments left off take their defaults, and wrong ones are refused")
{
    const auto *remove = table::Find("RemoveEffectsByParameters");
    REQUIRE(remove != nullptr);
    std::string error;

    const std::vector<ScriptValue> one = {ScriptValue::Object(42)};
    const auto full = CompleteArguments(*remove, one, error);
    REQUIRE(full);
    REQUIRE(full->size() == 4);
    CHECK((*full)[0].object == 42);
    CHECK((*full)[3].object == dao::kInvalidObject);

    const std::vector<ScriptValue> wrong = {ScriptValue::Int(42)};
    CHECK_FALSE(CompleteArguments(*remove, wrong, error));
    CHECK(error.find("oTarget") != std::string::npos);

    CHECK_FALSE(CompleteArguments(*remove, {}, error));
    CHECK(error.find("no default") != std::string::npos);

    // One argument too many, every one of the right type: refused for its
    // count, not for a type (a call by name would otherwise drop the last).
    std::vector<ScriptValue> five(full->begin(), full->end());
    five.push_back(ScriptValue::Int(0));
    CHECK_FALSE(CompleteArguments(*remove, five, error));
    CHECK(error.find("takes 4 arguments, 5 given") != std::string::npos);
}

TEST_CASE("an int is taken for a float, and OBJECT_SELF is left to the call")
{
    // SetCreatureProperty(object, int, float, int = PROPERTY_VALUE_TOTAL)
    const auto *set = table::Find("SetCreatureProperty");
    REQUIRE(set != nullptr);
    std::string error;
    const std::vector<ScriptValue> args = {ScriptValue::Object(7), ScriptValue::Int(1), ScriptValue::Int(50)};
    const auto full = CompleteArguments(*set, args, error);
    REQUIRE(full);
    CHECK((*full)[2].type == ScriptType::Float);
    CHECK((*full)[2].number == 50.0F);

    const auto *equipped = table::Find("GetItemInEquipSlot");
    REQUIRE(equipped != nullptr);
    const auto withSelf = CompleteArguments(*equipped, std::vector<ScriptValue>{ScriptValue::Int(1)}, error);
    REQUIRE(withSelf);
    CHECK((*withSelf)[1].self);
}

TEST_CASE("a command with an engine structure in its signature is not called by name")
{
    const auto *apply = table::Find("Engine_ApplyEffectOnObject");
    REQUIRE(apply != nullptr);
    CHECK_FALSE(apply->CallableByName());
    const auto *team = table::Find("GetTeamId");
    REQUIRE(team != nullptr);
    CHECK(team->CallableByName());
    // An array answer is read as its elements.
    const auto *perceived = table::Find("GetPerceivedCreatureList");
    REQUIRE(perceived != nullptr);
    CHECK(perceived->returns == ScriptType::Array);
    CHECK(perceived->CallableByName());
}
