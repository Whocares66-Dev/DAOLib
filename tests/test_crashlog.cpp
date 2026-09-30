#include "dao/core/Exceptions.h"
#include "dao/core/FunctionTable.h"

#include <catch2/catch_test_macros.hpp>

using dao::core::AccessKind;
using dao::core::ExceptionName;
using dao::core::FunctionTable;

TEST_CASE("Ghidra's function list parses, and an address finds its function")
{
    const auto table =
        FunctionTable::Parse("# entry\tend\tname, from Ghidra\n"
                             "005d6740\t005d6a43\tCClientExoApp::SetPaused\n"
                             "006403c0\t006403e6\tGameModeController::HandleMessage_UserTogglePauseMessage\r\n"
                             "0073cd10\t0073cf11\tFUN_0073cd10\n");
    REQUIRE(table.Size() == 3);

    const auto hit = table.Find(0x005D67D4);
    REQUIRE(hit.has_value());
    CHECK(hit->entry == 0x005D6740);
    CHECK(hit->offset == 0x94);
    CHECK(hit->name == "CClientExoApp::SetPaused");

    CHECK(table.Find(0x006403E6)->name == "GameModeController::HandleMessage_UserTogglePauseMessage");
    CHECK(table.Find(0x0073CD10)->offset == 0);
}

TEST_CASE("an address outside every function finds none")
{
    const auto table = FunctionTable::Parse("005d6740\t005d6a43\tA\n0073cd10\t0073cf11\tB\n");
    CHECK_FALSE(table.Find(0x00401000).has_value());
    CHECK_FALSE(table.Find(0x005D6A44).has_value()); // between A and B
    CHECK_FALSE(table.Find(0x0073CF12).has_value()); // past B
    CHECK_FALSE(FunctionTable{}.Find(0x005D6740).has_value());
}

TEST_CASE("lines that do not parse are skipped, not fatal")
{
    const auto table = FunctionTable::Parse("not a line\n"
                                            "zz\t00401010\tBad\n"
                                            "00401020\t00401000\tBackwards\n"
                                            "00401000\t0040100f\tGood\n"
                                            "\n");
    CHECK(table.Size() == 1);
    CHECK(table.Find(0x00401005)->name == "Good");
}

TEST_CASE("exception codes have their names")
{
    CHECK(ExceptionName(0xC0000005) == "EXCEPTION_ACCESS_VIOLATION");
    CHECK(ExceptionName(0xC00000FD) == "EXCEPTION_STACK_OVERFLOW");
    CHECK(ExceptionName(0xE06D7363) == "C++ exception (throw)");
    CHECK(ExceptionName(0x12345678).empty());
    CHECK(AccessKind(0) == "reading");
    CHECK(AccessKind(1) == "writing");
    CHECK(AccessKind(8) == "executing");
}
