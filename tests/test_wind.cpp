// dao/core/Wind.h over a wind file made here, laid out as the game's are, and over the
// game's in game/wind when ghilana's extract has written them there (DAOLib #196).
#include "dao/core/Gff4.h"
#include "dao/core/Wind.h"
#include "gff4_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace gff4 = dao::core::gff4;
namespace wind = dao::core::wind;
using dao::test::Float;
using dao::test::Floats;
using dao::test::Start;
using dao::test::Struct;

namespace
{

// A wind file with the template of the game's 115 (tools/gff4.py --templates), each field
// a different value: a float's is its offset over 4, so that a field read from another's
// label shows.
std::vector<std::byte> WindFile()
{
    auto b =
        Start("WND ", {{"WIND", 0xC0, {{3730, 8, 0, 116},  {22003, 8, 0, 96},  {22004, 10, 0, 0},  {22005, 8, 0, 100},
                                       {22010, 8, 0, 104}, {22011, 8, 0, 108}, {22012, 8, 0, 112}, {22013, 8, 0, 120},
                                       {22020, 1, 0, 180}, {22021, 1, 0, 181}, {22022, 8, 0, 124}, {22023, 8, 0, 128},
                                       {22025, 8, 0, 132}, {22026, 8, 0, 136}, {22027, 12, 0, 16}, {22028, 12, 0, 32},
                                       {22029, 12, 0, 48}, {22030, 12, 0, 64}, {22031, 8, 0, 140}, {22032, 8, 0, 144},
                                       {22033, 8, 0, 148}, {22034, 8, 0, 152}, {22035, 8, 0, 156}, {22036, 8, 0, 160},
                                       {22037, 8, 0, 164}, {22038, 8, 0, 168}, {22039, 8, 0, 172}, {22040, 8, 0, 176},
                                       {22041, 10, 0, 80}}}});
    const std::size_t root = Struct(b, 0xC0);
    for (std::size_t at = 96; at <= 176; at += 4)
    {
        Float(b, root + at, static_cast<float>(at) / 4.0F);
    }
    Floats(b, root, {0.0F, 1.0F, 0.5F});
    Floats(b, root + 16, {1.0F, 2.0F, 3.0F, 4.0F});
    Floats(b, root + 32, {5.0F, 6.0F, 7.0F, 8.0F});
    Floats(b, root + 48, {9.0F, 10.0F, 11.0F, 12.0F});
    Floats(b, root + 64, {13.0F, 14.0F, 15.0F, 16.0F});
    Floats(b, root + 80, {0.25F, 0.5F, 0.75F});
    b.U8(root + 180, 8);
    b.U8(root + 181, 7);
    return b.bytes;
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

TEST_CASE("a wind file's every field is read from its own label", "[wind]")
{
    const auto bytes = WindFile();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto read = wind::WindOf(*file);
    REQUIRE(read);
    CHECK(read->strength == 24.0F);
    CHECK(read->direction == std::array<float, 3>{0.0F, 1.0F, 0.5F});
    CHECK(read->radiusFalloff == 25.0F);
    CHECK(read->gustMinStrength == 26.0F);
    CHECK(read->gustMaxStrength == 27.0F);
    CHECK(read->gustMinDuration == 28.0F);
    CHECK(read->gustFrequency == 29.0F);
    CHECK(read->gustMaxDuration == 30.0F);
    CHECK(read->treeResponse == 31.0F);
    CHECK(read->treeResponseLimit == 32.0F);
    CHECK(read->treeBranchExponent == 33.0F);
    CHECK(read->treeLeafExponent == 34.0F);
    CHECK(read->clothResponse == 35.0F);
    CHECK(read->clothResponseLimit == 36.0F);
    CHECK(read->clothStrength == 37.0F);
    CHECK(read->clothGustStrengthMin == 38.0F);
    CHECK(read->clothGustStrengthMax == 39.0F);
    CHECK(read->clothGustDurationMin == 40.0F);
    CHECK(read->clothGustDurationMax == 41.0F);
    CHECK(read->clothGustIntervalMin == 42.0F);
    CHECK(read->clothGustIntervalMax == 43.0F);
    CHECK(read->clothGustDirChange == 44.0F);
    CHECK(read->treeNumWindMatrices == 8);
    CHECK(read->treeNumLeafAngles == 7);
    CHECK(read->treeBranchOscillationX == std::array<float, 4>{1.0F, 2.0F, 3.0F, 4.0F});
    CHECK(read->treeBranchOscillationY == std::array<float, 4>{5.0F, 6.0F, 7.0F, 8.0F});
    CHECK(read->treeLeafRocking == std::array<float, 4>{9.0F, 10.0F, 11.0F, 12.0F});
    CHECK(read->treeLeafRustling == std::array<float, 4>{13.0F, 14.0F, 15.0F, 16.0F});
    CHECK(read->clothGustAxisRatio == std::array<float, 3>{0.25F, 0.5F, 0.75F});
}

TEST_CASE("a file of another type is no wind", "[wind]")
{
    auto b = Start("PLTS", {{"PLTS", 4, {{13016, 14, 0, 0}}}});
    Struct(b, 4);
    const auto file = gff4::File::Parse(b.bytes);
    REQUIRE(file);
    CHECK_FALSE(wind::WindOf(*file));
}

TEST_CASE("the game's winds in game/wind, when there, each read, the Fade's as it is", "[wind][game][folder]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "wind";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/wind (ghilana's extract of *.wnd writes them there)");
    }
    std::size_t winds = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        const auto bytes = FromDisk(entry.path());
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const auto read = wind::WindOf(*file);
        REQUIRE(read);
        ++winds;
    }
    CHECK(winds > 0); // a few have a gust minimum above the maximum, as written

    const auto bytes = FromDisk(folder / "oth000d_193.wnd");
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto fade = wind::WindOf(*file);
    REQUIRE(fade);
    CHECK(fade->strength == 0.25F);
    CHECK(fade->direction == std::array<float, 3>{0.0F, 1.0F, 0.0F});
    CHECK(fade->gustFrequency == 0.0F);
    CHECK(fade->gustMaxStrength == 0.25F);
    CHECK(fade->gustMinDuration == 2.0F);
    CHECK(fade->gustMaxDuration == 3.0F);
    CHECK(fade->treeNumWindMatrices == 8);
    CHECK(fade->treeNumLeafAngles == 8);
    CHECK(fade->treeLeafRocking == std::array<float, 4>{2.0F, 4.0F, 5.0F, 20.0F});
    CHECK(fade->treeLeafRustling == std::array<float, 4>{7.0F, 3.0F, 4.0F, 40.0F});
    CHECK(fade->clothGustAxisRatio[2] == 0.2F);
}
