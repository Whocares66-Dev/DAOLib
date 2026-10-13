// A data table read (dao/core/GdaFile.h), and tables merged as the game
// merges them (dao/core/M2da.h).
#include "dao/core/GdaFile.h"
#include "dao/core/GdaHash.h"
#include "dao/core/M2da.h"
#include "files_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using dao::core::GdaColumnHash;
using dao::core::GdaFile;
namespace files = dao::test::files;
namespace m2da = dao::core::m2da;

namespace
{

std::vector<std::uint32_t> Hashes(const std::vector<std::string> &names)
{
    std::vector<std::uint32_t> out;
    out.reserve(names.size());
    for (const auto &name : names)
    {
        out.push_back(GdaColumnHash(name));
    }
    return out;
}

GdaFile Table(const std::vector<std::string> &columns, const std::vector<std::vector<files::Cell>> &rows)
{
    return GdaFile::Parse(files::Gda(columns, rows, Hashes(columns))).value();
}

std::int64_t Int(const GdaFile::Cell &cell)
{
    return std::get<std::int64_t>(cell);
}

} // namespace

TEST_CASE("a GDA table's columns by hash, and its rows' cells of each column's type", "[gda]")
{
    auto hashes = Hashes({"ID", "Label", "Cost"});
    hashes.push_back(0x12345678);
    const auto table =
        GdaFile::Parse(files::Gda({"ID", "Label", "Cost", "x"},
                                  {{1, std::string("Flurry"), 2.5F, 9}, {2, std::string("Dual"), 3.0F, 8}}, hashes));
    REQUIRE(table);
    REQUIRE(table->Columns().size() == 4);
    CHECK(table->Columns()[1].type == dao::core::gff4::Type::String);
    CHECK(table->Columns()[2].type == dao::core::gff4::Type::F32);
    CHECK(table->ColumnOf("label") == 1U);
    CHECK(table->ColumnOf(0x12345678U) == 3U);
    CHECK_FALSE(table->ColumnOf("Nothing"));
    REQUIRE(table->Rows().size() == 2);
    CHECK(Int(table->Rows()[0][0]) == 1);
    CHECK(std::get<std::wstring>(table->Rows()[0][1]) == L"Flurry");
    CHECK(std::get<double>(table->Rows()[0][2]) == 2.5);
    CHECK(Int(table->Rows()[1][3]) == 8);
    CHECK_FALSE(GdaFile::Parse(files::Tlk({{1, "x"}})));
}

TEST_CASE("worksheets are read from m2da_base's rows, none for an empty prefix", "[gda][m2da]")
{
    const auto base = Table({"ID", "Label", "Worksheet"}, {{1, std::string("Abilities"), std::string("ABI_")},
                                                           {2, std::string("Nothing"), std::string("")}});
    const auto sheets = m2da::Worksheets(base, true);
    REQUIRE(sheets.size() == 1);
    CHECK(sheets[0].id == 1);
    CHECK(sheets[0].label == L"Abilities");
    CHECK(sheets[0].prefix == L"abi_");
    CHECK(sheets[0].merged);
}

TEST_CASE("a file is part of the longest prefix of m2da_base's, else of 2da_base's", "[gda][m2da]")
{
    const std::vector<m2da::Worksheet> sheets{{1, L"Abilities", L"abi_", true},
                                              {9, L"More", L"abi_x_", true},
                                              {7, L"Portraits", L"portraits", true},
                                              {112, L"Ability Base", L"abi_base", false},
                                              {14, L"Ability Types", L"abilitytype", false}};
    CHECK(m2da::WorksheetOf(sheets, L"ABI_base") == 0U);
    CHECK(m2da::WorksheetOf(sheets, L"abi_x_more") == 1U);
    CHECK(m2da::WorksheetOf(sheets, L"portraits_turnbased") == 2U);
    CHECK(m2da::WorksheetOf(sheets, L"abilitytype") == 4U);
    CHECK_FALSE(m2da::WorksheetOf(sheets, L"ab"));
}

TEST_CASE("tables merge by ID: a row parts give alike once, different rows each a conflict", "[gda][m2da]")
{
    const auto core = Table({"ID", "Label"}, {{1, std::string("a")}, {2, std::string("b")}});
    const auto mod = Table({"ID", "Cost"}, {{2, 5.0F}, {3, 6.0F}});
    const auto patch = Table({"ID", "Label"}, {{1, std::string("a")}, {2, std::string("b")}});
    const std::vector<const GdaFile *> parts{&core, &mod, &patch};
    const auto merged = m2da::Merge(parts);
    REQUIRE(merged.columns.size() == 3); // ID, Label, Cost
    REQUIRE(merged.rows.size() == 4);
    CHECK(merged.conflicts == 1);
    CHECK_FALSE(merged.rows[0].conflict);
    CHECK(merged.rows[0].alike == std::vector<std::size_t>{2});
    CHECK(merged.rows[1].conflict);
    CHECK(merged.rows[1].alike == std::vector<std::size_t>{2});
    CHECK(merged.rows[2].conflict);
    CHECK(merged.rows[2].part == 1);
    CHECK(std::get<double>(merged.rows[2].cells[2]) == 5.0);
    CHECK(std::holds_alternative<std::monostate>(merged.rows[2].cells[1]));
    CHECK_FALSE(merged.rows[3].conflict);
    CHECK(merged.rows[3].id == 3);
}

TEST_CASE("copies that agree on every row are no conflict", "[gda][m2da]")
{
    const auto a = Table({"ID", "Label"}, {{1, std::string("a")}});
    const auto b = Table({"ID", "Label"}, {{1, std::string("a")}});
    const std::vector<const GdaFile *> parts{&a, &b};
    const auto merged = m2da::Merge(parts);
    CHECK(merged.conflicts == 0);
    REQUIRE(merged.rows.size() == 1);
    CHECK(merged.rows[0].alike == std::vector<std::size_t>{1});
}

TEST_CASE("a file's own two rows of one ID are no conflict between files, alike or not", "[gda][m2da]")
{
    const auto differ = Table({"ID", "Label"}, {{1, std::string("a")}, {1, std::string("b")}});
    const auto same = Table({"ID", "Label"}, {{1, std::string("a")}, {1, std::string("a")}});
    for (const GdaFile *table : {&differ, &same})
    {
        const std::vector<const GdaFile *> parts{table};
        const auto merged = m2da::Merge(parts);
        CHECK(merged.conflicts == 0);
        CHECK(merged.rows.size() == 2);
        CHECK_FALSE(merged.rows[0].conflict);
    }
}
