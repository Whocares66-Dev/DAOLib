#include "dao/core/Gff4.h"
#include "dao/core/SaveMeta.h"
#include "gff4_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

using dao::core::SaveMeta;
using dao::core::SlotFolderName;
using dao::test::Builder;

namespace
{

// A save's metadata laid out as the game's are (tools/gff4.py --templates
// on CastleCousland.das.met): one template of 0x30 bytes, its label empty,
// four-byte fields at 0 to 44, the strings by their offset in the data.
std::vector<std::byte> MakeMeta(std::string_view type = "SVMT")
{
    Builder b;
    b.Text(0, "GFF V4.0PC  ");
    b.Text(12, type);
    b.Text(16, "V1.1");
    b.U32(20, 1); // templates
    const std::size_t templates = 28;
    const std::size_t fields = templates + 16;
    struct Decl
    {
        std::uint32_t label;
        std::uint32_t type; // 5 a signed 32-bit number, 14 a string
    };
    const std::vector<Decl> decls{
        {SaveMeta::kStory, 14},
        {SaveMeta::kArea, 14},
        {SaveMeta::kTimePlayed, 5},
        {SaveMeta::kLevel, 5},
        {SaveMeta::kClass, 5},
        {SaveMeta::kGender, 5},
        {SaveMeta::kRace, 5},
        {SaveMeta::kBackground, 5},
        {SaveMeta::kCharacter, 14},
        {SaveMeta::kName, 14},
        {16809, 14},
        {16810, 14},
    };
    b.U32(templates + 4, static_cast<std::uint32_t>(decls.size()));
    b.U32(templates + 8, static_cast<std::uint32_t>(fields));
    b.U32(templates + 12, 0x30);
    for (std::size_t i = 0; i < decls.size(); ++i)
    {
        b.U32(fields + (12 * i), decls[i].label);
        b.U32(fields + (12 * i) + 4, decls[i].type);
        b.U32(fields + (12 * i) + 8, static_cast<std::uint32_t>(4 * i));
    }
    b.data = fields + (12 * decls.size());
    b.U32(24, static_cast<std::uint32_t>(b.data));
    b.U8(b.data + 0x30 - 1, 0);
    const std::size_t top = b.data;
    b.U32(top + 0, b.String(u"You are the second child of Teyrn Bryce Cousland."));
    b.U32(top + 4, b.String(u"Castle Cousland"));
    b.U32(top + 8, 1125);
    // Level, class and gender told apart, so each is read from its own field.
    b.U32(top + 12, 7);
    b.U32(top + 16, 2);
    b.U32(top + 20, 1);
    b.U32(top + 24, 3);
    b.U32(top + 28, 5);
    b.U32(top + 32, b.String(u"Aedan"));
    b.U32(top + 36, b.String(u"Castle Cousland"));
    b.U32(top + 40, b.String(u"single player;"));
    b.U32(top + 44, b.String(u"SP;"));
    return b.bytes;
}

} // namespace

TEST_CASE("a save's metadata, as the load screen shows it")
{
    const auto meta = SaveMeta::Parse(MakeMeta());
    REQUIRE(meta);
    CHECK(meta->area == L"Castle Cousland");
    CHECK(meta->character == L"Aedan");
    CHECK(meta->name == L"Castle Cousland");
    CHECK(meta->story == L"You are the second child of Teyrn Bryce Cousland.");
    CHECK(meta->secondsPlayed == 1125);
    CHECK(meta->level == 7);
    CHECK(meta->characterClass == 2);
    CHECK(meta->gender == 1);
    CHECK(meta->race == 3);
    CHECK(meta->background == 5);
}

TEST_CASE("a file that is not a save's metadata is refused")
{
    CHECK_FALSE(SaveMeta::Parse(MakeMeta("TLK ")));
    CHECK_FALSE(SaveMeta::Parse({}));
}

TEST_CASE("a slot's kind and number from its folder's name")
{
    const auto manual = SlotFolderName::Parse(L"Slot_12");
    REQUIRE(manual);
    CHECK(manual->kind == 0);
    CHECK(manual->number == 12);
    const auto autosave = SlotFolderName::Parse(L"AutoSave_4");
    REQUIRE(autosave);
    CHECK(autosave->kind == 1);
    CHECK(autosave->number == 4);
    const auto quick = SlotFolderName::Parse(L"QuickSave_1");
    REQUIRE(quick);
    CHECK(quick->kind == 2);
    CHECK(quick->number == 1);
    CHECK_FALSE(SlotFolderName::Parse(L"Slot_"));
    CHECK_FALSE(SlotFolderName::Parse(L"Slot_1a"));
    CHECK_FALSE(SlotFolderName::Parse(L"Screenshots"));
    CHECK_FALSE(SlotFolderName::Parse(L"Slot_1234567890"));
}

TEST_CASE("a metadata file written unchanged is the same bytes", "[savemeta][write]")
{
    const auto bytes = MakeMeta();
    const auto meta = SaveMeta::Parse(bytes);
    REQUIRE(meta);
    const auto out = SaveMeta::Write(bytes, *meta);
    REQUIRE(out);
    CHECK(out->bytes == bytes);
    CHECK(out->unwritten.empty());
}

TEST_CASE("each changed field of the metadata reads back changed and nothing else moves", "[savemeta][write]")
{
    const auto bytes = MakeMeta();
    const auto was = *SaveMeta::Parse(bytes);
    const auto write = [&](auto change) {
        auto meta = was;
        change(meta);
        const auto out = SaveMeta::Write(bytes, meta);
        REQUIRE(out);
        CHECK(out->unwritten.empty());
        const auto back = SaveMeta::Parse(out->bytes);
        REQUIRE(back);
        return *back;
    };
    const auto same = [](const SaveMeta &a, const SaveMeta &b) {
        return a.area == b.area && a.character == b.character && a.name == b.name && a.story == b.story &&
               a.secondsPlayed == b.secondsPlayed && a.level == b.level && a.characterClass == b.characterClass &&
               a.gender == b.gender && a.race == b.race && a.background == b.background;
    };

    auto got = write([](SaveMeta &m) { m.secondsPlayed = 99999; });
    CHECK(got.secondsPlayed == 99999);
    got.secondsPlayed = was.secondsPlayed;
    CHECK(same(got, was));

    got = write([](SaveMeta &m) { m.level = 20; });
    CHECK(got.level == 20);
    got.level = was.level;
    CHECK(same(got, was));

    got = write([](SaveMeta &m) { m.area = L"Denerim, the Alienage"; });
    CHECK(got.area == L"Denerim, the Alienage");
    got.area = was.area;
    CHECK(same(got, was));

    got = write([](SaveMeta &m) { m.name = L"Before the Landsmeet"; });
    CHECK(got.name == L"Before the Landsmeet");
    got.name = was.name;
    CHECK(same(got, was));

    got = write([](SaveMeta &m) { m.character = L"Lyna"; });
    CHECK(got.character == L"Lyna");
    got.character = was.character;
    CHECK(same(got, was));

    got = write([](SaveMeta &m) { m.story = L""; });
    CHECK(got.story.empty());
    got.story = was.story;
    CHECK(same(got, was));

    got = write([](SaveMeta &m) {
        m.characterClass = 3;
        m.gender = 2;
        m.race = 1;
        m.background = 4;
    });
    CHECK(got.characterClass == 3);
    CHECK(got.gender == 2);
    CHECK(got.race == 1);
    CHECK(got.background == 4);
    CHECK(got.level == was.level);
}

TEST_CASE("the metadata's unnamed fields and its story ride along", "[savemeta][write]")
{
    const auto bytes = MakeMeta();
    auto meta = *SaveMeta::Parse(bytes);
    meta.level = 3;
    meta.area = L"Somewhere else";
    const auto out = SaveMeta::Write(bytes, meta);
    REQUIRE(out);
    const auto file = dao::core::gff4::File::Parse(out->bytes);
    REQUIRE(file);
    const auto root = file->Root();
    REQUIRE(root);
    CHECK(root->String(16809).value_or(L"") == L"single player;");
    CHECK(root->String(16810).value_or(L"") == L"SP;");
    CHECK(root->String(SaveMeta::kStory).value_or(L"") == meta.story);
}

TEST_CASE("only a save's metadata is written", "[savemeta][write]")
{
    const SaveMeta meta;
    CHECK_FALSE(SaveMeta::Write(MakeMeta("TLK "), meta));
    CHECK_FALSE(SaveMeta::Write({}, meta));
}
