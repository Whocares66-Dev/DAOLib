#include "dao/core/Exceptions.h"
#include "dao/core/FunctionTable.h"
#include "dao/core/StackText.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

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

namespace
{

// An engine std::wstring as it lies in a frame: a word, the inline units or
// the pointer, the length and the capacity.
std::array<std::uint32_t, 7> StackString(std::wstring_view text, std::uint32_t capacity, std::uint32_t pointer = 0)
{
    std::array<std::uint32_t, 7> words{0xCCCCCCCC, 0, 0, 0, 0, static_cast<std::uint32_t>(text.size()), capacity};
    if (capacity == 7)
    {
        std::array<wchar_t, 8> units{};
        text.copy(units.data(), text.size());
        std::memcpy(&words[1], units.data(), sizeof units);
    }
    else
    {
        words[1] = pointer;
    }
    return words;
}

} // namespace

TEST_CASE("wide text is printable units up to a NUL")
{
    using dao::core::PrintableWide;
    const std::array<wchar_t, 8> name{L'f', L'x', L'_', L'f', L'i', L'r', L'e', L'\0'};
    CHECK(PrintableWide(name) == "fx_fire");
    const std::array<wchar_t, 4> unended{L'a', L'b', L'c', L'd'};
    CHECK_FALSE(PrintableWide(unended).has_value()); // no NUL in what was copied
    const std::array<wchar_t, 4> brief{L'a', L'b', L'\0', L'x'};
    CHECK_FALSE(PrintableWide(brief).has_value()); // shorter than four
    CHECK(PrintableWide(brief, 2) == "ab");
    const std::array<wchar_t, 6> binary{L'a', L'b', 0x4647, L'c', L'd', L'\0'};
    CHECK_FALSE(PrintableWide(binary).has_value());
}

TEST_CASE("an engine string in a frame is found inline or by its pointer")
{
    using dao::core::WStringIn;
    const auto inlined = WStringIn(StackString(L"mmh_x", 7));
    REQUIRE(inlined.has_value());
    CHECK(inlined->text == "mmh_x");
    CHECK(inlined->pointer == 0);

    const auto pointed = WStringIn(StackString(L"fxm_spider_web_long", 31, 0x26B546E0));
    REQUIRE(pointed.has_value());
    CHECK(pointed->size == 19);
    CHECK(pointed->pointer == 0x26B546E0);
    CHECK(pointed->text.empty());
}

TEST_CASE("words that only look like an engine string are not one")
{
    using dao::core::WStringIn;
    CHECK_FALSE(WStringIn(StackString(L"ab", 7)).has_value());             // too short to tell
    CHECK_FALSE(WStringIn(StackString(L"abcdefghij", 31, 0)).has_value()); // no text anywhere
    auto longer = StackString(L"abcde", 7);
    longer[5] = 6; // the length past the NUL
    CHECK_FALSE(WStringIn(longer).has_value());
    auto shrunk = StackString(L"abcdefghij", 31, 0x1000);
    shrunk[6] = 4; // a capacity below the inline 7
    CHECK_FALSE(WStringIn(shrunk).has_value());
    const std::array<std::uint32_t, 7> numbers{1, 2, 3, 4, 5, 9, 8}; // longer than its capacity
    CHECK_FALSE(WStringIn(numbers).has_value());
}

TEST_CASE("a GFF4 file is known by its first eight bytes")
{
    using dao::core::IsGff4;
    const auto bytes = [](std::string_view text) {
        return std::span(reinterpret_cast<const std::byte *>(text.data()), text.size());
    };
    CHECK(IsGff4(bytes("GFF V4.0PC  MMH V0.1")));
    CHECK(IsGff4(bytes("GFF V4.1PC  CONV")));
    CHECK_FALSE(IsGff4(bytes("GFF V3.2")));
    CHECK_FALSE(IsGff4(bytes("GFF V4")));
}
