#include "dao/core/TalkTable.h"
#include "gff4_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using dao::core::TalkTableFile;
using dao::test::Builder;

namespace
{

// A talk table laid out as the game's are (tools/gff4.py --templates on
// core_en-us.tlk): a TLK top struct with the string list, a list of STRN
// structs in place, each an id and a string.
std::vector<std::byte> MakeTalkTable(const std::vector<std::pair<std::uint32_t, std::u16string>> &strings,
                                     std::string_view version = "V0.2")
{
    Builder b;
    b.Text(0, "GFF V4.0PC  TLK ");
    b.Text(16, version);
    b.U32(20, 2); // templates
    const std::size_t templates = 28;
    const std::size_t fields = templates + (2 * 16);
    // TLK: one field, the string list (a list of template 1, in place).
    b.Text(templates, "TLK ");
    b.U32(templates + 4, 1);
    b.U32(templates + 8, static_cast<std::uint32_t>(fields));
    b.U32(templates + 12, 4);
    b.U32(fields, TalkTableFile::kStringList);
    b.U32(fields + 4, 1 | (0xC000U << 16));
    b.U32(fields + 8, 0);
    // STRN: the id, then the string's offset.
    b.Text(templates + 16, "STRN");
    b.U32(templates + 20, 2);
    b.U32(templates + 24, static_cast<std::uint32_t>(fields + 12));
    b.U32(templates + 28, 8);
    b.U32(fields + 12, TalkTableFile::kStringId);
    b.U32(fields + 16, 4);
    b.U32(fields + 20, 0);
    b.U32(fields + 24, TalkTableFile::kString);
    b.U32(fields + 28, 14);
    b.U32(fields + 32, 4);
    b.data = fields + 36;
    b.U32(24, static_cast<std::uint32_t>(b.data));
    // The top struct, then the list: a count and the entries in place.
    const std::size_t list = b.data + 4;
    b.U32(b.data, static_cast<std::uint32_t>(list - b.data));
    b.U32(list, static_cast<std::uint32_t>(strings.size()));
    const std::size_t first = list + 4;
    b.U8(first + (8 * strings.size()) - 1, 0);
    for (std::size_t i = 0; i < strings.size(); ++i)
    {
        b.U32(first + (8 * i), strings[i].first);
        b.U32(first + (8 * i) + 4, b.String(strings[i].second));
    }
    return b.bytes;
}

} // namespace

TEST_CASE("a talk table's strings by id")
{
    const auto bytes = MakeTalkTable({{1, u"End turn"}, {2, u"Too far"}, {7000001, u"Fin du tour"}});
    const auto table = TalkTableFile::Parse(bytes);
    REQUIRE(table);
    CHECK(table->Size() == 3);
    CHECK(table->String(1) == L"End turn");
    CHECK(table->String(2) == L"Too far");
    CHECK(table->String(7000001) == L"Fin du tour");
    CHECK_FALSE(table->String(3));
}

TEST_CASE("a talk table's strings go in their ids' order, whatever the file's")
{
    const auto table = TalkTableFile::Parse(MakeTalkTable({{9, u"nine"}, {2, u"two"}, {5, u"five"}}));
    REQUIRE(table);
    const auto &strings = table->Strings();
    REQUIRE(strings.size() == 3);
    CHECK(strings[0] == std::pair<std::uint32_t, std::wstring>{2, L"two"});
    CHECK(strings[1].first == 5);
    CHECK(strings[2] == std::pair<std::uint32_t, std::wstring>{9, L"nine"});
}

TEST_CASE("a file that is not a version 0.2 talk table is refused")
{
    auto bytes = MakeTalkTable({{1, u"x"}});
    SECTION("version 0.5, Huffman-coded")
    {
        CHECK_FALSE(TalkTableFile::Parse(MakeTalkTable({{1, u"x"}}, "V0.5")));
    }
    SECTION("another type")
    {
        std::memcpy(bytes.data() + 12, "CONV", 4);
        CHECK_FALSE(TalkTableFile::Parse(bytes));
    }
    SECTION("cut short")
    {
        CHECK_FALSE(TalkTableFile::Parse(std::span(bytes).first(30)));
    }
}

TEST_CASE("a talk table written reads back its strings, and is refused an id twice")
{
    const std::vector<std::pair<std::uint32_t, std::wstring>> strings{{900002, L"Red Ogre"}, {900001, L"Élan"}};
    const auto table = dao::core::TalkTableFile::Parse(dao::core::TalkTableFile::Write(strings));
    REQUIRE(table);
    CHECK(table->Size() == 2);
    CHECK(table->String(900002) == L"Red Ogre");
    CHECK(table->String(900001) == L"Élan");
    const std::vector<std::pair<std::uint32_t, std::wstring>> twice{{1, L"a"}, {1, L"b"}};
    CHECK(dao::core::TalkTableFile::Write(twice).empty());
    const std::vector<std::pair<std::uint32_t, std::wstring>> empty{{TalkTableFile::kEmptySlot, L"a"}};
    CHECK(dao::core::TalkTableFile::Write(empty).empty());
}

namespace
{

constexpr std::size_t kList = 100; // the data at 96, the top struct, then the list's count

std::uint32_t U32(const std::vector<std::byte> &bytes, std::size_t at)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + at, sizeof(value));
    return value;
}

} // namespace

// The ids of the game's core_de-de_f.tlk (its texts aside), in the slots the file has them in.
TEST_CASE("a table written puts each id in the slot the game's own table has it in")
{
    const std::vector<std::uint32_t> slots{TalkTableFile::kEmptySlot,
                                           0x5CF65,
                                           0x4F194,
                                           0x3A540,
                                           0x5FBE4,
                                           TalkTableFile::kEmptySlot,
                                           TalkTableFile::kEmptySlot,
                                           0x5FC33,
                                           0x3A53B,
                                           0x3A53D,
                                           0x3A54C,
                                           0x3A53C,
                                           0x5FC30,
                                           0x60E84,
                                           TalkTableFile::kEmptySlot,
                                           0x3A53A};
    std::vector<std::pair<std::uint32_t, std::wstring>> strings;
    for (const std::uint32_t id : slots)
    {
        if (id != TalkTableFile::kEmptySlot)
        {
            strings.emplace_back(id, std::to_wstring(id));
        }
    }
    const auto bytes = TalkTableFile::Write(strings);
    REQUIRE(U32(bytes, kList) == slots.size());
    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        INFO("slot " << i);
        CHECK(U32(bytes, kList + 4 + (8 * i)) == slots[i]);
    }
}

TEST_CASE("a text written once for the ids that share it, an empty one as none, each on four bytes after 0xFF")
{
    const std::vector<std::pair<std::uint32_t, std::wstring>> strings{{1, L"ab"}, {2, L"ab"}, {3, L""}, {4, L"cd"}};
    const auto bytes = TalkTableFile::Write(strings);
    const std::uint32_t count = U32(bytes, kList);
    REQUIRE(count == 5);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> words; // id, offset
    words.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        words.emplace_back(U32(bytes, kList + 4 + (8 * i)), U32(bytes, kList + 8 + (8 * i)));
    }
    const auto offset = [&](std::uint32_t id) {
        return std::ranges::find(words, id, &std::pair<std::uint32_t, std::uint32_t>::first)->second;
    };
    CHECK(offset(1) == offset(2));
    CHECK(offset(3) == 0xFFFFFFFF);
    CHECK(offset(TalkTableFile::kEmptySlot) == 0xFFFFFFFF);
    // "ab" and its NUL take ten bytes, so the next text starts two bytes of 0xFF later.
    const std::uint32_t first = std::min(offset(1), offset(4));
    const std::uint32_t second = std::max(offset(1), offset(4));
    CHECK(second == first + 12);
    CHECK(bytes[96 + first + 10] == std::byte{0xFF});
    CHECK(bytes[96 + first + 11] == std::byte{0xFF});
    CHECK(bytes.size() == 96 + second + 10);
    const auto table = TalkTableFile::Parse(bytes);
    REQUIRE(table);
    CHECK(table->Size() == 4);
    CHECK(table->String(2) == L"ab");
    CHECK(table->String(3) == L"");
    CHECK_FALSE(table->String(TalkTableFile::kEmptySlot));
}

TEST_CASE("a table of no strings has no list")
{
    const auto bytes = TalkTableFile::Write({});
    REQUIRE(bytes.size() == 100);
    CHECK(U32(bytes, 96) == 0xFFFFFFFF);
    const auto table = TalkTableFile::Parse(bytes);
    REQUIRE(table);
    CHECK(table->Size() == 0);
}
