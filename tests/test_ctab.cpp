// dao/core/Ctab.h over shader bytecode made here, laid out as the compiler lays it out, and over the
// game's shaders in game/shaders when ghilana's extract has written them there (DAOLib #246).
#include "dao/core/Ctab.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace ctab = dao::core::ctab;

namespace
{

struct Spec
{
    std::string name;
    std::uint16_t set;
    std::uint16_t index;
    std::uint16_t count;
    std::uint16_t kind;
    std::uint16_t type;
    std::uint16_t rows;
    std::uint16_t columns;
    std::uint16_t elements;
};

void Put32(std::vector<std::byte> &bytes, std::size_t at, std::uint32_t value)
{
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}

void Put16(std::vector<std::byte> &bytes, std::size_t at, std::uint16_t value)
{
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}

std::uint32_t PutString(std::vector<std::byte> &table, const std::string &text)
{
    const auto at = static_cast<std::uint32_t>(table.size());
    for (const char c : text)
    {
        table.push_back(static_cast<std::byte>(c));
    }
    table.push_back(std::byte{0});
    return at;
}

// The table's bytes after "CTAB": the header, the constants' infos, their types, then the strings.
std::vector<std::byte> TableBytes(const std::vector<Spec> &constants)
{
    std::vector<std::byte> table(0x1C + (constants.size() * 0x14) + (constants.size() * 0x10));
    Put32(table, 0, 0x1C);
    Put32(table, 8, 0xFFFF0300);
    Put32(table, 12, static_cast<std::uint32_t>(constants.size()));
    Put32(table, 16, 0x1C);
    Put32(table, 20, 0x100);
    for (std::size_t i = 0; i < constants.size(); ++i)
    {
        const Spec &c = constants[i];
        const std::size_t info = 0x1C + (i * 0x14);
        const std::size_t type = 0x1C + (constants.size() * 0x14) + (i * 0x10);
        Put32(table, info, PutString(table, c.name));
        Put16(table, info + 4, c.set);
        Put16(table, info + 6, c.index);
        Put16(table, info + 8, c.count);
        Put32(table, info + 12, static_cast<std::uint32_t>(type));
        Put16(table, type, c.kind);
        Put16(table, type + 2, c.type);
        Put16(table, type + 4, c.rows);
        Put16(table, type + 6, c.columns);
        Put16(table, type + 8, c.elements);
    }
    Put32(table, 24, PutString(table, "ps_3_0"));
    Put32(table, 4, PutString(table, "Microsoft (R) HLSL Shader Compiler 9.19.949.2111"));
    while (table.size() % 4 != 0)
    {
        table.push_back(std::byte{0});
    }
    return table;
}

// A ps_3_0 shader: the version, an unrelated comment, the table's comment, one instruction, the end.
std::vector<std::byte> Shader(const std::vector<std::byte> &table, bool otherCommentFirst = true)
{
    std::vector<std::byte> bytes;
    const auto push = [&](std::uint32_t token) {
        bytes.resize(bytes.size() + 4);
        Put32(bytes, bytes.size() - 4, token);
    };
    push(0xFFFF0300);
    if (otherCommentFirst)
    {
        push((2U << 16) | 0xFFFE);
        push(0x47554244); // "DBUG"
        push(0);
    }
    push((static_cast<std::uint32_t>((table.size() / 4) + 1) << 16) | 0xFFFE);
    push(0x42415443); // "CTAB"
    bytes.insert(bytes.end(), table.begin(), table.end());
    push(0x02000001); // mov
    push(0x800F0800);
    push(0xA0E40000);
    push(0x0000FFFF);
    return bytes;
}

const std::vector<Spec> kConstants = {
    {"g_vLightParams", 2, 4, 6, 1, 3, 1, 4, 6},
    {"mml_mWorld", 2, 0, 4, 2, 3, 4, 4, 1},
    {"g_bShaderControlBooleans", 0, 0, 6, 0, 1, 1, 1, 6},
    {"mml_tDiffuse", 3, 1, 1, 4, 12, 1, 1, 1},
};

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<char> chars((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes(chars.size());
    std::memcpy(bytes.data(), chars.data(), chars.size());
    return bytes;
}

} // namespace

TEST_CASE("a shader's constant table, after another comment, read whole", "[ctab]")
{
    const auto bytes = Shader(TableBytes(kConstants));
    const auto table = ctab::Read(bytes);
    REQUIRE(table);
    CHECK(table->version == 0xFFFF0300);
    CHECK(table->target == "ps_3_0");
    CHECK(table->creator == "Microsoft (R) HLSL Shader Compiler 9.19.949.2111");
    REQUIRE(table->constants.size() == 4);
    const ctab::Constant &lights = table->constants[0];
    CHECK(lights.name == "g_vLightParams");
    CHECK(lights.set == ctab::RegisterSet::Float4);
    CHECK(lights.index == 4);
    CHECK(lights.count == 6);
    CHECK(lights.kind == ctab::Class::Vector);
    CHECK(lights.type == 3);
    CHECK(lights.rows == 1);
    CHECK(lights.columns == 4);
    CHECK(lights.elements == 6);
    CHECK(table->constants[1].kind == ctab::Class::MatrixRows);
    CHECK(table->constants[2].set == ctab::RegisterSet::Bool);
    CHECK(table->constants[3].set == ctab::RegisterSet::Sampler);
    CHECK(table->constants[3].type == 12);
}

TEST_CASE("a register is found in the constant whose registers hold it, in its set", "[ctab]")
{
    const auto table = ctab::Read(Shader(TableBytes(kConstants), false));
    REQUIRE(table);
    CHECK(ctab::At(*table, ctab::RegisterSet::Float4, 0)->name == "mml_mWorld");
    CHECK(ctab::At(*table, ctab::RegisterSet::Float4, 3)->name == "mml_mWorld");
    CHECK(ctab::At(*table, ctab::RegisterSet::Float4, 4)->name == "g_vLightParams");
    CHECK(ctab::At(*table, ctab::RegisterSet::Float4, 9)->name == "g_vLightParams");
    CHECK(ctab::At(*table, ctab::RegisterSet::Float4, 10) == nullptr);
    CHECK(ctab::At(*table, ctab::RegisterSet::Bool, 5)->name == "g_bShaderControlBooleans");
    CHECK(ctab::At(*table, ctab::RegisterSet::Bool, 6) == nullptr);
    CHECK(ctab::At(*table, ctab::RegisterSet::Sampler, 1)->name == "mml_tDiffuse");
    CHECK(ctab::At(*table, ctab::RegisterSet::Sampler, 0) == nullptr);
}

TEST_CASE("bytecode with no table, or a table cut short, gives none", "[ctab]")
{
    const std::vector<std::byte> none;
    CHECK_FALSE(ctab::Read(none));

    // An instruction before any table ends the search.
    std::vector<std::byte> noTable(16);
    Put32(noTable, 0, 0xFFFF0300);
    Put32(noTable, 4, 0x02000001);
    CHECK_FALSE(ctab::Read(noTable));

    const auto whole = Shader(TableBytes(kConstants));
    for (std::size_t cut = 0; cut < whole.size() - 16; cut += 4)
    {
        const std::vector<std::byte> shorter(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(cut));
        CHECK_FALSE(ctab::Read(shorter));
    }
}

TEST_CASE("a table whose name runs past its end, or with more constants than it holds, gives none", "[ctab]")
{
    auto table = TableBytes(kConstants);
    Put32(table, 0x1C, static_cast<std::uint32_t>(table.size())); // the first constant's name
    CHECK_FALSE(ctab::Read(Shader(table)));

    table = TableBytes(kConstants);
    Put32(table, 12, 1000); // the count
    CHECK_FALSE(ctab::Read(Shader(table)));

    table = TableBytes(kConstants);
    Put16(table, 0x1C + 4, 4); // a register set past the sampler
    CHECK_FALSE(ctab::Read(Shader(table)));
}

TEST_CASE("the game's shaders in game/shaders, when there, each read", "[ctab][game][folder]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "shaders";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/shaders (ghilana's extract of *.fxo writes them there)");
    }
    std::size_t shaders = 0;
    std::size_t constants = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        const auto bytes = FromDisk(entry.path());
        const auto table = ctab::Read(bytes);
        INFO(entry.path().filename().string());
        REQUIRE(table);
        const bool pixel = entry.path().filename().string().find(".psh.") != std::string::npos;
        CHECK(table->target.substr(0, 3) == (pixel ? "ps_" : "vs_"));
        ++shaders;
        constants += table->constants.size();
    }
    CHECK(shaders > 0);
    CHECK(constants > shaders);

    const auto face = ctab::Read(FromDisk(folder / "face0.psh.fxo"));
    REQUIRE(face);
    CHECK(face->version == 0xFFFF0300);
    CHECK(face->target == "ps_3_0");
    CHECK(face->constants.size() == 34);
}
