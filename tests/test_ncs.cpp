// dao/core/Ncs.h over scripts made here, and over the game's in
// game/scripts when ghilana's extract has written them there (DAOLib #166).
#include "dao/core/Ncs.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace ncs = dao::core::ncs;

namespace
{

std::vector<std::byte> Bytes(std::initializer_list<int> values)
{
    std::vector<std::byte> out;
    for (const int v : values)
    {
        out.push_back(static_cast<std::byte>(v));
    }
    return out;
}

// "NCS V1.0", "B" and the file's size, as the compiler writes the head.
std::vector<std::byte> Script(std::initializer_list<int> code)
{
    auto out = Bytes({'N', 'C', 'S', ' ', 'V', '1', '.', '0', 'B', 0, 0, 0, 0});
    for (const int v : code)
    {
        out.push_back(static_cast<std::byte>(v));
    }
    const auto size = static_cast<std::uint32_t>(out.size());
    for (int i = 0; i < 4; ++i)
    {
        out[9 + i] = static_cast<std::byte>(size >> (24 - (8 * i)));
    }
    return out;
}

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());
    std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

} // namespace

TEST_CASE("a script's instructions with their operands, big-endian, the code at byte 13", "[ncs]")
{
    // rules_core.ncs's start, then each other layout once.
    const auto bytes = Script({
        0x02, 0x04,                                           // RSADD float
        0x04, 0x04, 0x3E, 0xE6, 0x66, 0x67,                   // CONST float 0.45
        0x01, 0x01, 0xFF, 0xFF, 0xFF, 0xF8, 0x00, 0x04,       // CPDOWNSP -8, 4
        0x1B, 0x00, 0xFF, 0xFF, 0xFF, 0xFC,                   // MOVSP -4
        0x2A, 0x00,                                           // SAVEBP
        0x1E, 0x00, 0x00, 0x00, 0x00, 0x08,                   // JSR +8
        0x04, 0x05, 0x00, 0x03, 'a',  'b',  'c',              // CONST string "abc"
        0x05, 0x00, 0x01, 0x2C, 0x02,                         // ACTION 300, 2 arguments
        0x21, 0x01, 0x00, 0x10, 0xFF, 0xFC, 0x00, 0x04,       // DESTRUCT 16, -4, 4
        0x2C, 0x0E, 0,    0,    0,    8,    0,    0,    0, 4, // STORE_STATE 8, 4, resume +14
        0x2E, 0x00,                                           // WAITFOR
        0x30, 0x01, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0x04,       // CPSPTOAL -16, 4
        0x0B, 0x24, 0x00, 0x0C,                               // EQUAL of two structures of 12 bytes
        0x02, 0x53,                                           // RSADD effect[]
        0x20, 0x00,                                           // RETN
    });
    const auto script = ncs::ScriptOf(bytes);
    REQUIRE(script);
    CHECK(script->whole);
    REQUIRE(script->code.size() == 15);
    const auto &c = script->code;
    CHECK(c[0].offset == 13);
    CHECK(c[1].real == std::bit_cast<float>(0x3EE66667U)); // 0.45 as the compiler rounds it
    CHECK(c[2].value == -8);
    CHECK(c[2].second == 4);
    CHECK(c[3].value == -4);
    CHECK(c[5].opcode == ncs::Op::Jsr);
    CHECK(c[5].Target() == c[5].offset + 8);
    CHECK(c[6].text == "abc");
    CHECK(c[6].length == 7);
    CHECK(c[7].value == 300);
    CHECK(c[7].second == 2);
    CHECK(c[8].value == 16);
    CHECK(c[8].second == -4);
    CHECK(c[8].third == 4);
    CHECK(c[9].value == 8);
    CHECK(c[9].second == 4);
    CHECK(c[9].type == 0x0E);
    CHECK(c[10].opcode == ncs::Op::WaitFor);
    CHECK(c[11].value == -16);
    CHECK(c[12].value == 12);
    CHECK(c[12].length == 4);
    CHECK(ncs::TypeName(c[13].type) == "effect[]");

    const auto text = ncs::Disassemble(*script);
    CHECK(text.find("0000000d  RSADD float\n") == 0);
    CHECK(text.find("JSR +8 -> ") != std::string::npos);
    CHECK(text.find("CONST string \"abc\"") != std::string::npos);
    CHECK(text.find("STORE_STATE resume -> ") != std::string::npos);
    CHECK(text.find("CPSPTOAL -16, 4") != std::string::npos);
}

TEST_CASE("a script stops where an instruction is unknown or runs past its end", "[ncs]")
{
    const auto unknown = ncs::ScriptOf(Script({0x20, 0x00, 0x37, 0x00}));
    REQUIRE(unknown);
    CHECK_FALSE(unknown->whole);
    CHECK(unknown->stopped == 15);
    CHECK(unknown->code.size() == 1);
    const auto cut = ncs::ScriptOf(Script({0x1D, 0x00, 0x00, 0x00}));
    REQUIRE(cut);
    CHECK_FALSE(cut->whole);
    CHECK(ncs::Disassemble(*cut).find("(not decoded)") != std::string::npos);
    CHECK_FALSE(ncs::ScriptOf(Bytes({'N', 'C', 'S', ' ', 'V', '1'})));
    auto other = Script({0x20, 0x00});
    other[7] = static_cast<std::byte>('1'); // V1.1 wants its B
    other[8] = static_cast<std::byte>('X');
    CHECK_FALSE(ncs::ScriptOf(other));
}

TEST_CASE("the game's scripts in game/scripts, when there, each decode to their last byte", "[ncs][game][folder]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "scripts";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/scripts (ghilana's extract of *.ncs writes them there)");
    }
    std::size_t scripts = 0;
    std::size_t instructions = 0;
    std::map<ncs::Op, std::size_t> seen;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".ncs")
        {
            continue;
        }
        const auto bytes = FromDisk(entry.path());
        const auto script = ncs::ScriptOf(bytes);
        REQUIRE(script);
        INFO(entry.path().filename().string());
        CHECK(script->whole);
        ++scripts;
        instructions += script->code.size();
        for (const auto &i : script->code)
        {
            ++seen[i.opcode];
        }
    }
    // 2026-10-17, the game's copies but the enciphered add-ins' (lib-agent-2's count, dev/NCS.md).
    CHECK(scripts == 2764);
    CHECK(instructions > 1'000'000);
    CHECK(seen[ncs::Op::WaitFor] == 2); // combateveryoneimmortal.ncs's
    CHECK(seen[ncs::Op::CpSpToAl] > 0);
    CHECK(seen[ncs::Op::CpAlToPSp] > 0);
    CHECK(seen[ncs::Op::AbortScript] == 0);
}
