// dao/core/Anb.h over a tree made here, and over the game's trees in
// game/trees when ghilana's extract has written them there (DAOLib #163).
#include "dao/core/Anb.h"
#include "dao/core/GdaFile.h"
#include "dao/core/GdaHash.h"
#include "dao/core/Gff4.h"
#include "dao/core/M2da.h"
#include "gff4_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace anb = dao::core::anb;
namespace gff4 = dao::core::gff4;
namespace m2da = dao::core::m2da;
using dao::test::Builder;
using dao::test::Float;
using dao::test::Start;
using dao::test::Struct;

namespace
{

struct Made
{
    const char16_t *name;
    const char16_t *file;
    std::uint32_t flags;
    std::uint32_t firstChild;
    std::uint32_t children;
    std::int32_t parent;
};

// A tree laid out as humanoid.anb is (tools/gff4.py --templates): its nodes, the root last.
std::vector<std::byte> TreeFile(const std::vector<Made> &made)
{
    Builder b = Start("ANBT", {{"tree", 8, {{4021, 1, gff4::kList | gff4::kStruct, 0}, {4022, 14, 0, 4}}},
                               {"node",
                                28,
                                {{4024, 14, 0, 0},
                                 {4025, 14, 0, 4},
                                 {4026, 8, 0, 12},
                                 {4027, 4, 0, 8},
                                 {4028, 4, 0, 16},
                                 {4029, 4, 0, 20},
                                 {4030, 5, 0, 24}}}});
    const std::size_t root = Struct(b, 8);
    b.U32(root + 4, b.String(u"movement"));
    const std::size_t list = b.bytes.size();
    b.U32(list, static_cast<std::uint32_t>(made.size()));
    std::vector<std::size_t> at;
    at.reserve(made.size());
    for (std::size_t i = 0; i < made.size(); ++i)
    {
        at.push_back(Struct(b, 28));
    }
    b.U32(root, static_cast<std::uint32_t>(list - b.data));
    for (std::size_t i = 0; i < made.size(); ++i)
    {
        b.U32(at[i], b.String(made[i].name));
        b.U32(at[i] + 4, b.String(made[i].file));
        b.U32(at[i] + 8, made[i].flags);
        Float(b, at[i] + 12, 1.0F);
        b.U32(at[i] + 16, made[i].firstChild);
        b.U32(at[i] + 20, made[i].children);
        b.U32(at[i] + 24, static_cast<std::uint32_t>(made[i].parent));
    }
    return b.bytes;
}

// anim_base's rows as the game has them for the pause and its twitch, and a row whose node no
// tree has, replaced by the pause.
m2da::Merged Animations()
{
    m2da::Merged table;
    for (const auto *name : {"ID", "AnimName", "Replacement"})
    {
        table.columns.push_back({.hash = dao::core::GdaColumnHash(name), .type = std::nullopt});
    }
    const auto row = [&](std::int64_t id, const wchar_t *name, std::int64_t replacement) {
        table.rows.push_back({.part = 0,
                              .cells = {id, std::wstring(name), replacement},
                              .id = id,
                              .conflict = false,
                              .taken = true,
                              .alike = {}});
    };
    row(1, L"pause", 1);
    row(640, L"pausetwitch_v1", 1);
    row(900, L"no_such_node", 1);
    row(901, L"nor_this", 0);
    return table;
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

TEST_CASE("a tree's nodes, each its name, clip and place, the root holding the rest", "[anb]")
{
    const auto bytes = TreeFile({{u"pause", u"mh_m.p.ani", 0, 0, 0, 2},
                                 {u"PauseTwitch_v1", u"mh_m.mov_tw_v1.ani", 1, 0, 0, 2},
                                 {u"movement", u"", 0, 0, 2, -1}});
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto tree = anb::TreeOf(*file);
    REQUIRE(tree);
    CHECK(tree->name == L"movement");
    REQUIRE(tree->nodes.size() == 3);
    CHECK(tree->nodes[2].children == 2);
    CHECK(tree->nodes[2].parent == -1);
    CHECK(tree->nodes[1].flags == 1);
    CHECK(tree->nodes[0].weight == 1.0F);
    const auto *twitch = tree->Find(L"pausetwitch_v1"); // in any case
    REQUIRE(twitch);
    CHECK(twitch->file == L"mh_m.mov_tw_v1.ani");
    CHECK_FALSE(tree->Find(L"attk_v1"));
}

TEST_CASE("an animation's clip by its AnimName in the first tree with it, else its replacement's", "[anb]")
{
    const auto bytes = TreeFile({{u"pause", u"mh_m.p.ani", 0, 0, 0, 1}, {u"movement", u"", 0, 0, 1, -1}});
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto movement = anb::TreeOf(*file);
    REQUIRE(movement);
    const auto other =
        TreeFile({{u"pausetwitch_v1", u"mh_m.mov_tw_v1.ani", 1, 0, 0, 1}, {u"humanoid", u"", 0, 0, 1, -1}});
    const auto otherFile = gff4::File::Parse(other);
    REQUIRE(otherFile);
    const auto humanoid = anb::TreeOf(*otherFile);
    REQUIRE(humanoid);
    const anb::Tree *trees[] = {&*humanoid, &*movement};
    const auto animations = Animations();

    const auto twitch = anb::ClipOf(animations, 640, trees);
    REQUIRE(twitch);
    CHECK(twitch->node->file == L"mh_m.mov_tw_v1.ani");
    CHECK(twitch->tree == 0);
    const auto replaced = anb::ClipOf(animations, 900, trees);
    REQUIRE(replaced);
    CHECK(replaced->animation == 1);
    CHECK(replaced->node->file == L"mh_m.p.ani");
    CHECK(replaced->tree == 1);
    CHECK_FALSE(anb::ClipOf(animations, 901, trees));        // no node and no replacement
    CHECK_FALSE(anb::ClipOf(animations, 900, trees, false)); // a placeable's: no fallback
    CHECK(anb::ClipOf(animations, 640, trees, false)->node->file == L"mh_m.mov_tw_v1.ani");
    CHECK_FALSE(anb::ClipOf(animations, 2, trees)); // no row
    CHECK(anb::FemaleTree(L"movement.anb") == L"movement_fem.anb");
}

TEST_CASE("the game's trees in game/trees, when there, each a root last holding children in a row", "[anb][game]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "trees";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/trees (ghilana's extract of *.anb writes them there)");
    }
    std::size_t trees = 0;
    std::size_t notTrees = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        const auto bytes = FromDisk(entry.path());
        const auto file = gff4::File::Parse(bytes);
        const auto tree = file ? anb::TreeOf(*file) : std::nullopt;
        if (!tree)
        {
            ++notTrees;
            continue;
        }
        ++trees;
        REQUIRE_FALSE(tree->nodes.empty());
        const auto &root = tree->nodes.back();
        CHECK(root.parent == -1);
        for (std::size_t i = 0; i < tree->nodes.size(); ++i)
        {
            const auto &node = tree->nodes[i];
            CHECK(node.flags <= 1);
            for (std::uint32_t c = node.firstChild; c < node.firstChild + node.children; ++c)
            {
                REQUIRE(c < tree->nodes.size());
                CHECK(std::cmp_equal(tree->nodes[c].parent, i));
            }
        }
    }
    // 2026-10-17: 1,112 trees, and the 95 level .anb files that are XML.
    CHECK(trees == 1112);
    CHECK(notTrees == 95);
    // The pause a human idles in, and its first twitch, as paragon-agent named them by hand.
    const auto bytes = FromDisk(folder / "humanoid.anb");
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto humanoid = anb::TreeOf(*file);
    REQUIRE(humanoid);
    const auto *pause = humanoid->Find(L"pause");
    REQUIRE(pause);
    CHECK(pause->file == L"mh_m.p.ani");
}
