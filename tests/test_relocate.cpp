#include "dao/core/Relocate.h"
#include "dao/core/Rtti.h"

#include <catch2/catch_test_macros.hpp>

using dao::core::kPreferredBase;
using dao::core::Relocate;
using dao::core::Rva;
using dao::core::UndecorateClassName;

TEST_CASE("an address at the preferred base is itself")
{
    CHECK(Relocate(0x004E3024, kPreferredBase) == 0x004E3024);
    CHECK(Rva(0x004E3024) == 0xE3024);
}

TEST_CASE("an address follows the base the exe was loaded at")
{
    CHECK(Relocate(0x004E3024, 0x10000000) == 0x100E3024);
    static_assert(Relocate(0x00400000, 0x00500000) == 0x00500000);
}

TEST_CASE("decorated class names undecorate innermost-last")
{
    CHECK(UndecorateClassName(".?AVAbility@@") == "Ability");
    CHECK(UndecorateClassName(".?AVScaleformManager@GUI@@") == "GUI::ScaleformManager");
    CHECK(UndecorateClassName(".?AVActionRotate@Actor@Cutscene@EC@@") == "EC::Cutscene::Actor::ActionRotate");
    CHECK(UndecorateClassName(".?AUPod@@") == "Pod");
    CHECK(UndecorateClassName(".?AW4Mode@@") == "Mode");
}

TEST_CASE("a template's decorated name is returned as it came")
{
    constexpr auto kPause = ".?AV?$MemberFunctionMessageHandler@VGameModeController@@VUserTogglePauseMessage@@"
                            "$H?HandleMessage@1@QAE_NABV2@@ZA@@Private@Messages@@";
    CHECK(UndecorateClassName(kPause) == kPause);
}

TEST_CASE("text that is not a decorated name is returned as it came")
{
    CHECK(UndecorateClassName("").empty());
    CHECK(UndecorateClassName("Ability") == "Ability");
    CHECK(UndecorateClassName(".?AVAbility") == ".?AVAbility");
}
