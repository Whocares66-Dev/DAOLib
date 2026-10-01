#include "dao/core/TalkTable.h"
#include "gff4_builder.h"

#include <catch2/catch_test_macros.hpp>

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
    const std::size_t fields = templates + 2 * 16;
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
    b.U8(first + 8 * strings.size() - 1, 0);
    for (std::size_t i = 0; i < strings.size(); ++i)
    {
        b.U32(first + 8 * i, strings[i].first);
        b.U32(first + 8 * i + 4, b.String(strings[i].second));
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
