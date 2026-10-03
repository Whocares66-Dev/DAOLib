#include "dao/core/CallSite.h"
#include "dao/core/Pe.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <vector>

using dao::core::EndsWithCall;
using dao::core::FindSection;
using dao::core::Section;

namespace
{

// Seven bytes before an address: filler, then the instruction's bytes last.
std::array<std::byte, 7> Before(std::initializer_list<unsigned> instruction)
{
    std::array<std::byte, 7> out{};
    out.fill(std::byte{0x90});
    std::size_t i = 7 - instruction.size();
    for (unsigned b : instruction)
    {
        out[i++] = static_cast<std::byte>(b);
    }
    return out;
}

} // namespace

TEST_CASE("the call forms the game's code uses end where they should")
{
    // From DAOrigins.exe: call rel32 at 0x006403CF, call edx in a handler's
    // invoke slot, call [edx+0x54] in SetPaused, call [0x...] through the IAT.
    CHECK(EndsWithCall(Before({0xE8, 0x3C, 0x7E, 0x1F, 0x00})));
    CHECK(EndsWithCall(Before({0xFF, 0xD2})));
    CHECK(EndsWithCall(Before({0xFF, 0x52, 0x54})));
    CHECK(EndsWithCall(Before({0xFF, 0x15, 0x44, 0x33, 0xAE, 0x00})));
    CHECK(EndsWithCall(Before({0xFF, 0x92, 0x00, 0x01, 0x00, 0x00})));       // call [edx+0x100]
    CHECK(EndsWithCall(Before({0xFF, 0x54, 0x24, 0x08})));                   // call [esp+8]
    CHECK(EndsWithCall(Before({0xFF, 0x94, 0x81, 0x00, 0x01, 0x00, 0x00}))); // call [ecx+eax*4+0x100]
    CHECK(EndsWithCall(Before({0xFF, 0x10})));                               // call [eax]
    // call [eax*4+0x00C3EE5C], through a table of functions with no base
    // register: the exe has it at 0x008E0DB9.
    CHECK(EndsWithCall(Before({0xFF, 0x14, 0x85, 0x5C, 0xEE, 0xC3, 0x00})));
}

TEST_CASE("other instructions ending at an address are not calls")
{
    CHECK_FALSE(EndsWithCall(Before({})));
    CHECK_FALSE(EndsWithCall(Before({0xC3})));                         // ret
    CHECK_FALSE(EndsWithCall(Before({0xE9, 0x00, 0x00, 0x00, 0x00}))); // jmp rel32
    CHECK_FALSE(EndsWithCall(Before({0xFF, 0xE0})));                   // jmp eax: FF /4
    CHECK_FALSE(EndsWithCall(Before({0xFF, 0x30})));                   // push [eax]: FF /6
    CHECK_FALSE(EndsWithCall(Before({0xFF, 0x52})));                   // call [edx+d8] cut short
}

TEST_CASE("a section is found by name in a PE section table")
{
    // A minimal PE32: DOS header, NT headers at 0x80 with an empty optional
    // header, and two section headers.
    std::vector<std::byte> image(0x200);
    auto put16 = [&](std::size_t at, std::uint16_t v) { std::memcpy(&image[at], &v, 2); };
    auto put32 = [&](std::size_t at, std::uint32_t v) { std::memcpy(&image[at], &v, 4); };
    put16(0, 0x5a4d);
    put32(0x3c, 0x80);
    put32(0x80, 0x4550);
    put16(0x80 + 6, 2);  // two sections
    put16(0x80 + 20, 0); // optional header size
    const std::size_t table = 0x80 + 24;
    std::memcpy(&image[table], ".text", 5);
    put32(table + 8, 0x6d9000);
    put32(table + 12, 0x1000);
    std::memcpy(&image[table + 40], ".rdata", 6);
    put32(table + 40 + 8, 0x116000);
    put32(table + 40 + 12, 0x6da000);

    CHECK(FindSection(image, ".text") == Section{.rva = 0x1000, .virtualSize = 0x6d9000});
    CHECK(FindSection(image, ".rdata") == Section{.rva = 0x6da000, .virtualSize = 0x116000});
    CHECK_FALSE(FindSection(image, ".data").has_value());
    CHECK_FALSE(FindSection(image, ".tex").has_value());
    CHECK_FALSE(FindSection({}, ".text").has_value());
}

TEST_CASE("the game's .text, when there is a copy of the exe")
{
    const auto exe = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "DAOrigins.exe";
    if (!std::filesystem::exists(exe))
    {
        SKIP("no game/DAOrigins.exe in this checkout");
    }
    std::ifstream in(exe, std::ios::binary);
    std::vector<std::byte> head(0x1000);
    in.read(reinterpret_cast<char *>(head.data()), static_cast<std::streamsize>(head.size()));
    CHECK(FindSection(head, ".text") == Section{.rva = 0x1000, .virtualSize = 0x6d86aa});
}
