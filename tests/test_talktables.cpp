// The talk tables the game searches, in its order, and the text it finds
// (dao/core/TalkTables.h), over installs made in the test, and over the GOG
// install against what was seen in play (hidden).
#include "dao/core/Gff4.h"
#include "dao/core/Install.h"
#include "dao/core/TalkTables.h"
#include "files_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <windows.h>

#include <shlobj.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace files = dao::test::files;
namespace install = dao::core::install;

namespace
{

// The core, three add-ins (C_DOCS taken first, then A_HIGH, then B_LOW, by their Priority), an
// offer, and the main campaign, each with tables; A_HIGH and the offer extend the main campaign,
// B_LOW Awakening.
struct Tables
{
    files::TempFolder root;

    Tables()
    {
        Table(L"game/packages/core/data/talktables/core_en-us.tlk", {{1, "core"}, {2, "core"}});
        Table(L"documents/AddIns/c_docs/core/data/talktables/c_docs_c_en-us.tlk", {{3, "c core"}});
        Table(L"game/Addins/a_high/core/data/talktables/a_high_c_en-us.tlk", {{1, "a core"}, {3, "a core"}});
        Table(L"game/Addins/b_low/core/data/talktables/b_low_c_en-us.tlk", {{6, "b core"}});
        Table(L"game/Addins/a_high/module/data/talktables/a_high_en-us.tlk", {{4, "a module"}});
        Table(L"game/Addins/b_low/module/data/talktables/b_low_en-us.tlk", {{4, "b module"}});
        Table(L"game/Offers/o_one/core/data/talktables/o_one_c_en-us.tlk", {{6, "offer core"}});
        Table(L"game/Offers/o_one/module/data/talktables/o_one_en-us.tlk", {{5, "offer module"}});
        Table(L"game/modules/single player/data/talktables/singleplayer_en-us.tlk", {{4, "campaign"}, {5, "campaign"}});
        root.Write(
            L"documents/Settings/AddIns.xml",
            files::FromText(R"(<AddInsList>)"
                            R"(<AddInItem UID="A_HIGH" Priority="200" Enabled="1" ExtendedModuleUID="Single Player"/>)"
                            R"(<AddInItem UID="B_LOW" Priority="100" Enabled="1" ExtendedModuleUID="DAO_PRC_EP_1"/>)"
                            R"(<AddInItem UID="C_DOCS" Priority="300" Enabled="1" ExtendedModuleUID=""/>)"
                            R"(</AddInsList>)"));
        root.Write(
            L"documents/Settings/offers.xml",
            files::FromText(R"(<OfferList><OfferItem UID="O_ONE" Priority="100" ExtendedModuleUID="Single Player"/>)"
                            R"(</OfferList>)"));
    }

    void Table(const std::wstring &path, const std::vector<std::pair<std::uint32_t, std::string>> &strings) const
    {
        root.Write(path, files::Tlk(strings));
    }

    std::unique_ptr<install::Index> Build(const install::Options &options = {}) const
    {
        return install::Index::Build(root.path / L"game", root.path / L"documents", options);
    }
};

std::vector<std::string> Names(const std::vector<install::LoadedTable> &tables)
{
    std::vector<std::string> names;
    names.reserve(tables.size());
    for (const auto &table : tables)
    {
        names.push_back(table.name);
    }
    return names;
}

std::vector<install::LoadedTable> Kinds(std::string_view kinds)
{
    std::vector<install::LoadedTable> tables;
    tables.reserve(kinds.size());
    for (std::size_t i = 0; i < kinds.size(); ++i)
    {
        tables.push_back({std::string(1, static_cast<char>('a' + i)), {}, kinds[i] == 'f'});
    }
    return tables;
}

} // namespace

TEST_CASE("talk tables in the order the game loads them: core tables at start, then the campaign's", "[talktables]")
{
    const Tables folder;
    const auto index = folder.Build();
    CHECK(Names(install::LoadOrder(*index)) == std::vector<std::string>{"core_en-us.tlk", "c_docs_c_en-us.tlk",
                                                                        "a_high_c_en-us.tlk", "b_low_c_en-us.tlk",
                                                                        "o_one_c_en-us.tlk", "a_high_en-us.tlk",
                                                                        "o_one_en-us.tlk", "singleplayer_en-us.tlk"});
    // Another campaign's: an add-in's module table for it, and its own, by its name.
    const auto awakening = folder.Build({.campaign = "DAO_PRC_EP_1"});
    const auto names = Names(install::LoadOrder(*awakening));
    CHECK(std::ranges::count(names, "b_low_en-us.tlk") == 1);
    CHECK(std::ranges::count(names, "a_high_en-us.tlk") == 0);
    CHECK(std::ranges::count(names, "singleplayer_en-us.tlk") == 0);
    // No campaign: the core tables alone, as at the main menu.
    CHECK(install::LoadOrder(*folder.Build({.campaign = ""})).size() == 5);
}

TEST_CASE("setting the gender moves its tables ahead of the rest, reversed, each time again", "[talktables]")
{
    auto tables = Kinds("mfmfm");
    install::SetGender(tables, false);
    CHECK(Names(tables) == std::vector<std::string>{"e", "c", "a", "b", "d"});
    install::SetGender(tables, false);
    CHECK(Names(tables) == std::vector<std::string>{"a", "c", "e", "b", "d"});
    install::SetGender(tables, true);
    CHECK(Names(tables) == std::vector<std::string>{"d", "b", "a", "c", "e"});
}

TEST_CASE("an id's text is the first table's that holds it, in the order the hero's gender leaves", "[talktables]")
{
    const Tables folder;
    const auto index = folder.Build();
    // A female hero's: no _f table here, so the load order.
    const install::Strings female(*index, install::TalkTables(*index, true));
    REQUIRE(female.Tables().size() == 8);
    CHECK(female.Lookup(1)->text == L"core");
    CHECK(female.Lookup(3)->text == L"c core");
    CHECK(female.Lookup(4)->text == L"a module");
    CHECK(female.Lookup(6)->text == L"b core");
    const auto all = female.All(4);
    REQUIRE(all.size() == 2);
    CHECK(female.Tables()[all[1].table].name == "singleplayer_en-us.tlk");
    CHECK_FALSE(female.Lookup(7));
    // A male hero's: the plain tables reversed, the campaign's first and the core's last.
    const install::Strings male(*index, install::TalkTables(*index, false));
    CHECK(male.Tables().front().name == "singleplayer_en-us.tlk");
    CHECK(male.Tables().back().name == "core_en-us.tlk");
    CHECK(male.Lookup(1)->text == L"a core");
    CHECK(male.Lookup(4)->text == L"campaign");
    CHECK(male.Lookup(6)->text == L"offer core");
}

TEST_CASE("a table in the language, or in en-us where it has none, and its _f table after it", "[talktables]")
{
    Tables folder;
    folder.Table(L"game/packages/core/data/talktables/core_de-de.tlk", {{1, "Magier"}});
    folder.Table(L"game/packages/core/data/talktables/core_de-de_f.tlk", {{1, "Magierin"}});
    const auto index = folder.Build({.language = "de-de"});
    const auto loaded = install::LoadOrder(*index);
    REQUIRE(loaded.size() == 9);
    CHECK(loaded[0].name == "core_de-de.tlk");
    CHECK_FALSE(loaded[0].female);
    CHECK(loaded[1].name == "core_de-de_f.tlk");
    CHECK(loaded[1].female);
    CHECK(loaded[2].name == "c_docs_c_en-us.tlk");
    CHECK(install::Strings(*index, install::TalkTables(*index, true)).Lookup(1)->text == L"Magierin");
    CHECK(install::Strings(*index, install::TalkTables(*index, false)).Lookup(1)->text == L"a core");
}

namespace
{

// CTlkTable's search of one file (0x004E47F0): from the id's hash, one slot on at a time, until
// the id, an empty slot, or the start again.
bool GameFinds(const std::vector<std::uint32_t> &slots, std::uint32_t id)
{
    const std::size_t start = dao::core::TalkTableFile::Hash(id) % slots.size();
    std::size_t i = start;
    do
    {
        if (slots[i] == id)
        {
            return true;
        }
        if (slots[i] == 0xFFFFFFFF)
        {
            return false;
        }
        i = (i + 1) % slots.size();
    } while (i != start);
    return false;
}

std::filesystem::path Documents()
{
    PWSTR path = nullptr;
    std::filesystem::path documents;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path)))
    {
        documents = std::filesystem::path(path) / L"BioWare/Dragon Age";
    }
    CoTaskMemFree(path);
    return documents;
}

} // namespace

// The GOG install with the player's add-ins, against the texts seen in play 2026-10-24 (vanilla
// ids, each one table's alone, from each kind of table), and the game's own search of each
// file finding every id the file holds, as Strings' lookup by id does. Hidden, as it reads ten
// thousand archives twice: run it by its tag, dao_core_tests "[install-real]".
TEST_CASE("on the GOG install, the tables and texts the game had in play", "[.][install-real]")
{
    const std::filesystem::path game = L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins";
    const std::filesystem::path documents = Documents();
    if (!std::filesystem::exists(game / L"bin_ship") || !std::filesystem::exists(documents / L"Settings/AddIns.xml"))
    {
        SKIP("no GOG install and profile here");
    }
    const auto index = install::Index::Build(game, documents);
    const install::Strings female(*index, install::TalkTables(*index, true));
    REQUIRE_FALSE(female.Tables().empty());
    CHECK(female.Tables().front().name == "core_en-us.tlk");
    CHECK(female.Lookup(238912)->text == L"Magi");
    const auto gauntlets = female.Lookup(408421);
    REQUIRE(gauntlets);
    CHECK(gauntlets->text == L"Cailan's Gauntlets");
    CHECK(female.Tables()[gauntlets->table].name == "dao_prc_cp_1_c_en-us.tlk");
    const auto coins = female.Lookup(71419);
    REQUIRE(coins);
    CHECK(coins->text.starts_with(L"I see. Here, then..."));
    CHECK(female.Tables()[coins->table].name == "singleplayer_en-us.tlk");
    CHECK(female.Lookup(447460)->text == L"Purchase opens Downloadable Content menu");
    CHECK(female.Lookup(450687)->text == L"The Darkspawn Chronicles");
    const install::Strings male(*index, install::TalkTables(*index, false));
    CHECK(male.Tables().front().name == "singleplayer_en-us.tlk");
    CHECK(male.Tables().back().name == "core_en-us.tlk");

    std::size_t ids = 0;
    for (const auto &table : female.Tables())
    {
        const auto bytes = index->Read(table.copy);
        REQUIRE(bytes);
        const auto file = dao::core::gff4::File::Parse(*bytes);
        REQUIRE(file);
        std::vector<std::uint32_t> slots;
        for (const auto &entry : file->Root()->Structs(dao::core::TalkTableFile::kStringList))
        {
            slots.push_back(entry.Get<std::uint32_t>(dao::core::TalkTableFile::kStringId).value_or(0xFFFFFFFF));
        }
        for (const std::uint32_t id : slots)
        {
            if (id != 0xFFFFFFFF)
            {
                INFO(table.name << " " << id);
                CHECK(GameFinds(slots, id));
                ++ids;
            }
        }
    }
    WARN(female.Tables().size() << " tables, " << ids << " ids");

    const auto german = install::Index::Build(game, documents, {.language = "de-de"});
    CHECK(install::Strings(*german, install::TalkTables(*german, true)).Lookup(238912)->text == L"Magierin");
    CHECK(install::Strings(*german, install::TalkTables(*german, false)).Lookup(238912)->text == L"Magier");
}
