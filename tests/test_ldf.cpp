// dao/core/Ldf.h over a script.ldf made here, and over the toolset's copy in
// toolset/extract/ldf when tools/toolset_scripts.py has extracted it (DAOLib #167).
#include "dao/core/Ldf.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace ldf = dao::core::ldf;

TEST_CASE("script.ldf's defines, constants and declarations, the comments left out", "[ldf]")
{
    const auto read = ldf::LdfOf(R"(// SCRIPT.LDF
#define ENGINE_NUM_STRUCTURES   6
#define ENGINE_STRUCTURE_1  location

int    TRUE                     = 1;
float  PI                       = 3.141592f;
int RESISTANCE_CHECK_SUCCESS = TRUE;
int GM_INVALID = -1;
string sLanguage = "nwscript; not a statement's end";

/** @brief Pauses the game. (Not a declaration = 5;)
*/
void ToggleGamePause(int bPause = TRUE) = 188;

string GetM2DAString( int n2DA, string sColumn,
                      int nRow, string s2DA = "" ) = 9;
effect[] GetEffects(object oObject, int nType = 0) = 300; // the array kind
command CommandUseAbility( int nAbilityId, vector vTarget=[0.0,0.0,0.0], float nConjureTime = -1.0 ) = 301;
int GetHour() = 15;
)");
    REQUIRE(read.defines.size() == 2);
    CHECK(read.defines[1] == std::pair<std::string, std::string>{"ENGINE_STRUCTURE_1", "location"});
    REQUIRE(read.constants.size() == 5);
    CHECK(read.ConstantOf("PI")->value == "3.141592f");
    CHECK(read.ConstantOf("sLanguage")->value == "\"nwscript; not a statement's end\"");
    CHECK(read.IntOf("TRUE") == 1);
    CHECK(read.IntOf("RESISTANCE_CHECK_SUCCESS") == 1); // through TRUE
    CHECK(read.IntOf("GM_INVALID") == -1);
    CHECK_FALSE(read.IntOf("PI"));
    REQUIRE(read.functions.size() == 5);
    const auto *pause = read.FunctionOf(188);
    REQUIRE(pause);
    CHECK(pause->returns == "void");
    CHECK(pause->name == "ToggleGamePause");
    REQUIRE(pause->parameters.size() == 1);
    CHECK(pause->parameters[0].type == "int");
    CHECK(pause->parameters[0].name == "bPause");
    CHECK(pause->parameters[0].defaultValue == "TRUE");
    const auto *m2da = read.FunctionOf(9);
    REQUIRE(m2da);
    REQUIRE(m2da->parameters.size() == 4);
    CHECK(m2da->parameters[2].name == "nRow");
    CHECK(m2da->parameters[2].defaultValue.empty());
    CHECK(m2da->parameters[3].defaultValue == "\"\"");
    CHECK(read.FunctionOf(300)->returns == "effect[]");
    CHECK(read.FunctionOf(15)->parameters.empty());
    const auto *use = read.FunctionOf(301);
    REQUIRE(use);
    REQUIRE(use->parameters.size() == 3); // the vector's commas are its own
    CHECK(use->parameters[1].defaultValue == "[0.0,0.0,0.0]");
    CHECK_FALSE(read.FunctionOf(5));
}

TEST_CASE("the toolset's script.ldf, when there, is 674 functions and 481 constants", "[ldf][game]")
{
    const auto path =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "toolset" / "extract" / "ldf" / "script.ldf";
    if (!std::filesystem::exists(path))
    {
        SKIP("no toolset/extract/ldf/script.ldf (tools/toolset_scripts.py extracts it)");
    }
    std::ifstream in(path, std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const auto read = ldf::LdfOf(text);
    CHECK(read.functions.size() == 674);
    CHECK(read.constants.size() == 481);
    CHECK(read.IntOf("ENGINE_NUM_STRUCTURES") == std::nullopt); // a #define, not a constant
    REQUIRE(read.defines.size() == 10);
    CHECK(read.FunctionOf(188)->name == "ToggleGamePause");
    CHECK(read.FunctionOf(569)->name == "CommandPlayAnimation");
    CHECK(read.IntOf("EVENT_TYPE_SPAWN") == 3);
    CHECK(read.FunctionOf(337)->returns == "object []"); // as written
}
