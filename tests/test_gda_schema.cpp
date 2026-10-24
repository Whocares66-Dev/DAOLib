// The worksheets' catalogue and typed rows (dao/core/generated/GdaSchema.h, written by
// tools/gen_gda_schema.py from the install's scan): a worksheet found by its ID with its columns
// and their kinds, a typed row over a merged table made in the test, a column the table lacks read
// as none; and, hidden, every column of the catalogue as the GOG install's merged worksheets have it.
#include "dao/core/GdaHash.h"
#include "dao/core/Install.h"
#include "dao/core/M2da.h"
#include "dao/core/generated/GdaSchema.h"
#include "files_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace files = dao::test::files;
namespace m2da = dao::core::m2da;
namespace tables = dao::core::tables;
using dao::core::GdaFile;

namespace
{

std::vector<std::uint32_t> Hashes(const std::vector<std::string> &names)
{
    std::vector<std::uint32_t> out;
    out.reserve(names.size());
    for (const auto &name : names)
    {
        out.push_back(dao::core::GdaColumnHash(name));
    }
    return out;
}

} // namespace

TEST_CASE("a worksheet is found by its ID, with its columns and what each holds", "[gda][schema]")
{
    const tables::Worksheet *abilities = tables::Find(1);
    REQUIRE(abilities != nullptr);
    CHECK(abilities->label == "Abilities");
    CHECK(abilities->prefix == "abi_");
    CHECK(abilities->name == "AbilitiesTalents"); // the scripts' constant's name, not the label
    const tables::Column *name = tables::ColumnOf(*abilities, "namestrref");
    REQUIRE(name != nullptr);
    CHECK(name->hash == dao::core::GdaColumnHash("NameStrRef"));
    CHECK(name->kind == tables::Kind::StrRef);
    CHECK(tables::ColumnOf(*abilities, "Label")->kind == tables::Kind::Plain);
    CHECK(tables::ColumnOf(*abilities, "no such column") == nullptr);
    CHECK(tables::Find(-5) == nullptr);
    // A worksheet with no TABLE_* constant is named by its label.
    REQUIRE(tables::Find(7) != nullptr);
    CHECK(tables::Find(7)->name == "Portraits");
    CHECK(static_cast<std::int64_t>(tables::Sheet::Portraits) == 7);
    CHECK(std::ranges::is_sorted(tables::Worksheets(), {}, &tables::Worksheet::id));
}

TEST_CASE("a typed row reads each cell as its column's type, and a column the table lacks as none", "[gda][schema]")
{
    // A mod's abilities table, with three of the worksheet's columns.
    const auto table =
        GdaFile::Parse(files::Gda({"ID", "Label", "NameStrRef"},
                                  {{10000, std::string("Flurry"), 371}, {10001, std::string("Arrow"), 372}},
                                  Hashes({"ID", "Label", "NameStrRef"})))
            .value();
    const std::vector<const GdaFile *> parts{&table};
    const m2da::Merged merged = m2da::Merge(parts);
    const auto row = tables::AbilitiesTalents::Of(merged, 10001);
    REQUIRE(row);
    CHECK(row->ID() == 10001);
    CHECK(row->Label() == std::wstring_view(L"Arrow"));
    CHECK(row->NameStrRef() == 372);
    CHECK_FALSE(row->DescStrRef()); // the table has no such column
    CHECK_FALSE(tables::AbilitiesTalents::Of(merged, 5));
    CHECK(tables::AbilitiesTalents::kId == 1);
    // A cell of another kind than its column's: none, not a cast.
    const auto other =
        GdaFile::Parse(files::Gda({"ID", "NameStrRef"}, {{1, std::string("text")}}, Hashes({"ID", "NameStrRef"})))
            .value();
    const std::vector<const GdaFile *> otherParts{&other};
    const m2da::Merged mixed = m2da::Merge(otherParts);
    CHECK_FALSE(tables::AbilitiesTalents::Of(mixed, 1)->NameStrRef());
}

// The GOG install: every worksheet of the catalogue merged from it, each column there with its hash
// and its type. Hidden, as it reads every archive: run it by its tag, dao_core_tests "[install-real]".
TEST_CASE("on the GOG install, every catalogued column is in its worksheet, of its type", "[.][install-real]")
{
    const std::filesystem::path game = L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins";
    if (!std::filesystem::exists(game / L"bin_ship"))
    {
        SKIP("no GOG install here");
    }
    const auto index = dao::core::install::Index::Build(game, {});
    std::size_t columns = 0;
    for (const tables::Worksheet &worksheet : tables::Worksheets())
    {
        INFO(std::string(worksheet.label));
        const m2da::Merged *merged = index->Worksheet(worksheet.id);
        if (merged == nullptr)
        {
            continue; // an add-in's worksheet, with no Documents' AddIns.xml here to enable it
        }
        for (const tables::Column &column : worksheet.columns)
        {
            INFO(std::string(column.name));
            const auto found = std::ranges::find(merged->columns, column.hash, &GdaFile::Column::hash);
            if (found != merged->columns.end())
            {
                CHECK(found->type == column.type);
                ++columns;
            }
        }
    }
    CHECK(columns > 3000);
    WARN(columns << " columns as the catalogue has them");
}
