// dao/core/Plot.h over a plot made here, and over the game's in game/plots
// when ghilana's extract has written them there (DAOLib #169).
#include "dao/core/Gff4.h"
#include "dao/core/Plot.h"
#include "gff4_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace gff4 = dao::core::gff4;
namespace plot = dao::core::plot;
using dao::test::Builder;
using dao::test::Start;
using dao::test::Struct;

namespace
{

// A plot laid out as plt_aoa100pt_darkspawn_gens.plo is (tools/gff4.py --templates), cut down:
// its resref, GUID, script and stored state, and two flags, one stored and one defined.
std::vector<std::byte> PlotFile()
{
    Builder b = Start("PLTS", {{"PLTS", 4, {{13016, 1, gff4::kList | gff4::kStruct, 0}}},
                               {"PLOT",
                                0x20,
                                {{3, 14, 0, 0},
                                 {13000, 2, gff4::kList | gff4::kStruct, 4},
                                 {13007, 14, 0, 8},
                                 {13009, 14, 0, 12},
                                 {13011, 4, 0, 16},
                                 {13012, 4, 0, 20}}},
                               {"FLAG", 12, {{13001, 5, 0, 0}, {13002, 14, 0, 4}, {13005, 0, 0, 8}}}});
    const std::size_t root = Struct(b, 4);
    const std::size_t plots = b.bytes.size();
    b.U32(plots, 1);
    const std::size_t one = Struct(b, 0x20);
    b.U32(root, static_cast<std::uint32_t>(plots - b.data));
    b.U32(one, b.String(u"aoa100pt_darkspawn_gens"));
    b.U32(one + 8, b.String(u"7F0D8B7B630644D3BB8DC50023550AB7"));
    b.U32(one + 12, b.String(u"plot_core"));
    b.U32(one + 16, 0b10U);     // flag 1 stored set
    b.U32(one + 20, 1U << 31U); // flag 63
    const std::size_t flags = b.bytes.size();
    b.U32(flags, 2);
    const std::size_t complete = Struct(b, 12);
    const std::size_t defined = Struct(b, 12);
    b.U32(one + 4, static_cast<std::uint32_t>(flags - b.data));
    b.U32(complete, 1);
    b.U32(complete + 4, b.String(u"AOA_DARKSPAWN_GENS_COMPLETE"));
    b.U8(complete + 8, 255);
    b.U32(defined, 256);
    b.U32(defined + 4, b.String(u"AOA_DARKSPAWN_GENS_CAN_ASK"));
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

TEST_CASE("a plot's GUID, script, stored state and flags, stored and defined", "[plot]")
{
    const auto bytes = PlotFile();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto read = plot::PlotOf(*file);
    REQUIRE(read);
    CHECK(read->resref == L"aoa100pt_darkspawn_gens");
    CHECK(read->guid == L"7F0D8B7B630644D3BB8DC50023550AB7");
    CHECK(read->script == L"plot_core");
    REQUIRE(read->flags.size() == 2);
    const auto *complete = read->Find(1);
    REQUIRE(complete);
    CHECK(complete->name == L"AOA_DARKSPAWN_GENS_COMPLETE");
    CHECK(complete->endsPlot == 255);
    CHECK_FALSE(complete->Defined());
    CHECK(read->Find(256)->Defined());
    plot::Flag edge;
    edge.id = 127;
    CHECK_FALSE(edge.Defined()); // the last stored one
    edge.id = 128;
    CHECK(edge.Defined()); // the engine's first defined one, though no file numbers one so
    CHECK_FALSE(read->Find(2));
    CHECK(read->Stored(1));
    CHECK_FALSE(read->Stored(0));
    CHECK(read->Stored(63)); // the second word's high bit
    CHECK_FALSE(read->Stored(256));
}

TEST_CASE("the game's plots in game/plots, when there, each a GUID and flags stored or defined", "[plot][game]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "plots";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/plots (ghilana's extract of *.plo writes them there)");
    }
    std::size_t plots = 0;
    std::size_t stored = 0;
    std::size_t defined = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        const auto bytes = FromDisk(entry.path());
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const auto read = plot::PlotOf(*file);
        REQUIRE(read);
        CHECK(read->guid.size() == 32);
        ++plots;
        for (const auto &flag : read->flags)
        {
            CHECK((flag.id < 128 || flag.id >= 256)); // none between, as the toolset numbers them
            ++(flag.Defined() ? defined : stored);
        }
    }
    // 2026-10-17: the 1,191 the game takes but the enciphered add-ins'.
    CHECK(plots == 1191);
    CHECK(stored == 9558);
    CHECK(defined == 1695);
}
