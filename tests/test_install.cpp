// An install's sources and copies (dao/core/Install.h), and the settings
// that list its add-ins (dao/core/AddIns.h), over a folder made in the test
// as the GOG install is laid out.
#include "dao/core/AddInOrder.h"
#include "dao/core/AddIns.h"
#include "dao/core/GdaHash.h"
#include "dao/core/Install.h"
#include "files_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

namespace files = dao::test::files;
namespace install = dao::core::install;

namespace
{

struct Folder
{
    files::TempFolder root;

    Folder()
    {
        const std::vector<std::string> columns{"ID", "Label"};
        const std::vector<std::uint32_t> hashes{dao::core::GdaColumnHash("ID"), dao::core::GdaColumnHash("Label")};
        root.Write(L"game/packages/core/data/2da.erf",
                   files::Erf({{"abi_base.gda", files::Gda(columns, {{1, std::string("a")}}, hashes)},
                               {"Ogre.UTC", files::FromText("ogre")}}));
        root.Write(L"game/packages/core/data/talktables/core_en-us.tlk", files::Tlk({{1, "x"}}));
        root.Write(L"game/packages/core/data/music.fsb", files::FromText("audio"));
        root.Write(L"game/packages/core/override/ogre.utc", files::FromText("override"));
        root.Write(L"game/packages/core/data/broken.erf", files::FromText("not an archive"));
        root.Write(L"game/modules/single player/data/a.rim", files::Erf({{"area.are", files::FromText("area")}}));
        root.Write(L"game/modules/other/data/b.rim", files::Erf({{"b.are", files::FromText("b")}}));
        root.Write(L"game/Addins/dao_prc_cp_1/core/data/2da.erf",
                   files::Erf({{"abi_cp1.gda", files::FromText("ciphered")}}, true, 2U << 4));
        root.Write(L"game/Addins/dao_prc_cp_1/manifest.xml", files::FromText("<Manifest/>"));
        root.Write(L"game/Offers/dao_prc_o/core/x.uti", files::FromText("x"));
        root.Write(L"game/bin_ship/daolib/plugins/mymod/mymod.dll", files::FromText("dll"));
        root.Write(L"game/bin_ship/daolib/plugins/mymod/override/ogre.utc", files::FromText("plugin"));
        root.Write(L"game/bin_ship/daolib/plugins/nofiles/nofiles.dll", files::FromText("dll"));
        // No plugin the loader loads: its DLL is not <name>.dll.
        root.Write(L"game/bin_ship/daolib/plugins/misnamed/mymod.dll", files::FromText("dll"));
        root.Write(L"game/bin_ship/daolib/plugins/misnamed/override/ogre.utc", files::FromText("misnamed"));
        root.Write(L"documents/Settings/AddIns.xml",
                   files::FromText(R"(<AddInsList><AddInItem UID="DAO_PRC_CP_1" Priority="200" Enabled="1">)"
                                   R"(<Title DefaultText="Return to Ostagar"/></AddInItem></AddInsList>)"));
        root.Write(L"documents/AddIns/players_mod/core/override/p.uti", files::FromText("p"));
        root.Write(L"documents/packages/core/override/ogre.utc", files::FromText("documents"));
    }

    std::unique_ptr<install::Index> Build() const
    {
        return install::Index::Build(root.path / L"game", root.path / L"documents");
    }
};

std::string Text(const std::optional<std::vector<std::byte>> &bytes)
{
    return bytes ? std::string(reinterpret_cast<const char *>(bytes->data()), bytes->size()) : "(none)";
}

const install::Source *Named(const install::Index &index, std::string_view id)
{
    const auto at = index.SourceOf(id);
    return at ? &index.Sources()[*at] : nullptr;
}

} // namespace

TEST_CASE("an install's sources, each with its archives and loose files", "[install]")
{
    const Folder folder;
    const auto index = folder.Build();
    std::vector<std::string> ids;
    for (const auto &source : index->Sources())
    {
        ids.push_back(source.id);
    }
    CHECK(ids == std::vector<std::string>{"core", "override:game", "module:other", "campaign", "addin:dao_prc_cp_1",
                                          "offer:dao_prc_o", "plugin:mymod", "addin:players_mod",
                                          "override:documents"});
    const auto *core = Named(*index, "core");
    REQUIRE(core != nullptr);
    CHECK(core->archives == 1); // broken.erf is no archive
    CHECK(core->loose == 2);    // the talk table and the .fsb, not the override folder's
    CHECK(core->files == 4);
    CHECK(index->Measured().unopened == 1);
    const auto *addin = Named(*index, "addin:dao_prc_cp_1");
    REQUIRE(addin != nullptr);
    CHECK(addin->files == 1); // not its manifest.xml
    CHECK(addin->unreadable == 1);
    REQUIRE(addin->settings);
    CHECK(addin->settings->title == "Return to Ostagar");
    CHECK(Named(*index, "addin:players_mod")->base == install::Base::Documents);
    CHECK(Named(*index, "override:documents")->folder == "packages/core/override");
    CHECK(index->AddIns().size() == 1);
}

TEST_CASE("every copy of a resource, by its name in any case, in the game's order, each read", "[install]")
{
    const Folder folder;
    const auto index = folder.Build();
    const auto *copies = index->Find("OGRE.utc");
    REQUIRE(copies != nullptr);
    REQUIRE(copies->size() == 4);
    // The player's override, the install's, the plugin's, the core's archive (dev/FILES.md).
    std::vector<std::string> read;
    std::vector<std::int32_t> priorities;
    for (const auto &copy : *copies)
    {
        read.push_back(Text(index->Read(copy)));
        priorities.push_back(copy.priority);
        CHECK(copy.unused == install::Unused::No);
    }
    CHECK(read == std::vector<std::string>{"documents", "override", "plugin", "ogre"});
    CHECK(priorities == std::vector<std::int32_t>{0, 1, 2, 100005});
    CHECK(index->Where(copies->back()) == "packages/core/data/2da.erf");
    CHECK(index->Resources().contains("abi_base.gda"));
    CHECK(index->Find("nothing.utc") == nullptr);
    CHECK(index->Measured().resources == index->Resources().size());
}

TEST_CASE("copies of different resources sort into the sources' order, each with its engine source", "[install]")
{
    const Folder folder;
    const auto index = folder.Build();
    const auto first = [&index](std::string_view name) { return index->Find(name)->front(); };
    // Given out of order: the core's table, an offer's item, the player's add-in's item, the
    // player's override.
    std::vector<install::Copy> copies{first("abi_base.gda"), first("x.uti"), first("p.uti"), first("ogre.utc")};
    index->SortCopies(copies);
    std::vector<std::string> where;
    where.reserve(copies.size());
    for (const auto &copy : copies)
    {
        where.push_back(index->Where(copy));
    }
    // Those the game has first, by priority: the player's override, the core's archive; then the
    // player's add-in, which AddIns.xml does not list, and the offer, which no offers.xml does.
    CHECK(where == std::vector<std::string>{"packages/core/override/ogre.utc", "packages/core/data/2da.erf",
                                            "AddIns/players_mod/core/override/p.uti", "Offers/dao_prc_o/core/x.uti"});
    // Two files of one archive are of one source; a loose file's is its folder.
    const auto *ogres = index->Find("ogre.utc");
    const auto inCore = std::ranges::find_if(
        *ogres, [&index](const install::Copy &c) { return index->Where(c) == "packages/core/data/2da.erf"; });
    REQUIRE(inCore != ogres->end());
    CHECK(index->EngineSource(first("abi_base.gda")) == index->EngineSource(*inCore));
    CHECK(index->EngineSource(first("abi_base.gda")) == "game:packages/core/data/2da.erf");
    // The player's override and the install's are two sources, though their paths within their
    // bases are the same.
    REQUIRE(ogres->size() >= 2);
    CHECK(index->EngineSource((*ogres)[0]) == "documents:packages/core/override");
    CHECK(index->EngineSource((*ogres)[1]) == "game:packages/core/override");
    CHECK(index->EngineSource((*ogres)[2]) == "plugins:mymod/override");
}

TEST_CASE("a plugin's override is a source beside its DLL, in the plugins folder the game was started with",
          "[install]")
{
    const Folder folder;
    const auto game = folder.Build();
    const auto *own = Named(*game, "plugin:mymod");
    REQUIRE(own != nullptr);
    CHECK(own->base == install::Base::Plugins);
    CHECK(own->folder == "mymod/override");
    CHECK(game->Folder(install::Base::Plugins) == folder.root.path / L"game" / L"bin_ship" / L"daolib" / L"plugins");
    CHECK(Named(*game, "plugin:misnamed") == nullptr);

    // A build tree's plugins, as DAOLIB_PLUGINS names them: read in place of the game's.
    files::TempFolder tree;
    tree.Write(L"plugins/mymod/mymod.dll", files::FromText("dll"));
    tree.Write(L"plugins/mymod/override/ogre.utc", files::FromText("built"));
    tree.Write(L"plugins/newmod/newmod.dll", files::FromText("dll"));
    tree.Write(L"plugins/newmod/override/n.uti", files::FromText("n"));
    install::Options options;
    options.plugins = tree.path / L"plugins";
    const auto built = install::Index::Build(folder.root.path / L"game", folder.root.path / L"documents", options);
    CHECK(built->Folder(install::Base::Plugins) == options.plugins);
    const auto *mymod = Named(*built, "plugin:mymod");
    REQUIRE(mymod != nullptr);
    CHECK(mymod->files == 1);
    CHECK(Named(*built, "plugin:newmod") != nullptr);
    const auto *ogres = built->Find("ogre.utc");
    REQUIRE(ogres != nullptr);
    std::vector<std::string> read;
    for (const auto &copy : *ogres)
    {
        read.push_back(Text(built->Read(copy)));
    }
    // The game's own plugin's copy is not one: the game does not read that folder.
    CHECK(read == std::vector<std::string>{"documents", "override", "built", "ogre"});

    // A folder that is not there serves no plugin, as the loader loads none from it.
    options.plugins = tree.path / L"nothing";
    const auto none = install::Index::Build(folder.root.path / L"game", folder.root.path / L"documents", options);
    CHECK(Named(*none, "plugin:mymod") == nullptr);
}

TEST_CASE("files of one source sort by name, after their sources' order", "[install]")
{
    files::TempFolder root;
    // One archive with its files out of order by name, and a loose file ahead of it.
    root.Write(L"game/packages/core/data/2da.erf", files::Erf({{"abi_zz.gda", files::FromText("zz")},
                                                               {"abi_b.gda", files::FromText("b")},
                                                               {"abi_m10.gda", files::FromText("m10")},
                                                               {"abi_m9.gda", files::FromText("m9")}}));
    root.Write(L"game/packages/core/override/abi_q.gda", files::FromText("override"));
    const auto index = install::Index::Build(root.path / L"game", {});
    std::vector<install::NamedCopy> listed;
    for (const char *name : {"abi_zz.gda", "abi_m9.gda", "ABI_q.gda", "abi_m10.gda", "abi_b.gda"})
    {
        listed.push_back({name, index->Find(name)->front()});
    }
    index->SortFiles(listed);
    std::vector<std::string> names;
    names.reserve(listed.size());
    for (const auto &file : listed)
    {
        names.push_back(file.name);
    }
    // The override first (priority 1), then the archive's by name, character by character.
    CHECK(names == std::vector<std::string>{"ABI_q.gda", "abi_b.gda", "abi_m10.gda", "abi_m9.gda", "abi_zz.gda"});
}

TEST_CASE("a copy in an enciphered archive is listed and not read", "[install]")
{
    const Folder folder;
    const auto index = folder.Build();
    const auto *copies = index->Find("abi_cp1.gda");
    REQUIRE(copies != nullptr);
    CHECK_FALSE(index->Readable(copies->front()));
    CHECK_FALSE(index->Read(copies->front()));
    CHECK_FALSE(index->Whole("DAO_PRC_CP_1"));
}

TEST_CASE("an enciphered archive's copy reads from its plain copy in the folder the program names", "[install]")
{
    const Folder folder;
    // The archive's full name kept as a folder, the file in another case than the archive's (DAOLib #211).
    folder.root.Write(L"copies/dao_prc_cp_1/core/data/2da.erf/ABI_CP1.gda", files::FromText("plain"));
    const auto index = install::Index::Build(folder.root.path / L"game", folder.root.path / L"documents",
                                             {.archiveCopies = folder.root.path / L"copies"});
    const auto *copies = index->Find("abi_cp1.gda");
    REQUIRE(copies != nullptr);
    CHECK(copies->front().source == index->SourceOf("addin:dao_prc_cp_1"));
    CHECK(index->Readable(copies->front()));
    CHECK(Text(index->Read(copies->front())) == "plain");
    const auto *addin = Named(*index, "addin:dao_prc_cp_1");
    REQUIRE(addin != nullptr);
    CHECK(addin->unreadable == 0);
    CHECK(index->Whole("dao_prc_cp_1"));
    CHECK_FALSE(index->Whole("DAO_PRC_NONE"));
}

TEST_CASE("an install with no Documents folder has no Documents sources", "[install]")
{
    const Folder folder;
    const auto index = install::Index::Build(folder.root.path / L"game", {});
    CHECK_FALSE(index->SourceOf("override:documents"));
    CHECK(index->AddIns().empty());
}

TEST_CASE("AddIns.xml's items, their attributes and titles; offers.xml's", "[addins]")
{
    const auto items = dao::core::ReadAddIns(
        "<AddInsList><AddInItem UID=\"DAO_PRC_CP_1\" Name=\"PRC_CP1_Cailan\" ExtendedModuleUID=\"Single Player\" "
        "Priority=\"200\" Enabled=\"1\" State=\"2\" BioWare=\"1\">\n<Title DefaultText=\"Return &amp; Ostagar\">"
        "</Title></AddInItem>\n<AddInItem UID='B' Enabled=\"0\"></AddInItem><AddInItemX UID=\"no\"/></AddInsList>",
        "AddInItem");
    REQUIRE(items.size() == 2);
    CHECK(items[0].uid == "DAO_PRC_CP_1");
    CHECK(items[0].name == "PRC_CP1_Cailan");
    CHECK(items[0].extends == "Single Player");
    CHECK(items[0].priority == 200);
    CHECK(items[0].enabled == true);
    CHECK(items[0].state == 2);
    CHECK(items[0].bioware);
    CHECK(items[0].title == "Return & Ostagar");
    CHECK(items[1].uid == "B");
    CHECK(items[1].enabled == false);
    CHECK_FALSE(items[1].priority);
    CHECK(items[1].title.empty());

    const auto offers =
        dao::core::ReadAddIns(R"(<OfferList><OfferItem UID="O" Priority="3"></OfferItem></OfferList>)", "OfferItem");
    REQUIRE(offers.size() == 1);
    CHECK_FALSE(offers[0].enabled);
    CHECK(offers[0].priority == 3);
}

TEST_CASE("an attribute is found by its whole name, not inside another's", "[addins]")
{
    // ExtendedModuleUID ends in UID: before UID, it must not be taken for it.
    const auto items = dao::core::ReadAddIns(
        R"(<AddInItem ExtendedModuleUID="DAO_PRC_EP_1" UID="DAO_PRC_DRK_EP1" Name="x"></AddInItem>)", "AddInItem");
    REQUIRE(items.size() == 1);
    CHECK(items[0].uid == "DAO_PRC_DRK_EP1");
    CHECK(items[0].extends == "DAO_PRC_EP_1");
}

namespace
{

// Who each copy of a resource is from, in the order the index gives them.
std::vector<std::string> Order(const install::Index &index, std::string_view name)
{
    std::vector<std::string> out;
    if (const auto *copies = index.Find(name))
    {
        for (const auto &copy : *copies)
        {
            out.push_back(index.Sources()[copy.source].id + (copy.unused == install::Unused::No ? "" : " (unused)"));
        }
    }
    return out;
}

const char *const kThreeAddIns =
    R"(<AddInsList>)"
    R"(<AddInItem UID="A_HIGH" Priority="200" Enabled="1" ExtendedModuleUID="Single Player"/>)"
    R"(<AddInItem UID="B_LOW" Priority="100" Enabled="1" ExtendedModuleUID="DAO_PRC_EP_1"/>)"
    R"(<AddInItem UID="C_DOCS" Priority="300" Enabled="1" ExtendedModuleUID=""/>)"
    R"(</AddInsList>)";

} // namespace

TEST_CASE("add-ins in the game's order: a lower Priority first, a player's in Documents before all", "[install]")
{
    files::TempFolder root;
    root.Write(L"game/packages/core/data/x.gda", files::FromText("core"));
    root.Write(L"game/Addins/a_high/core/data/x.gda", files::FromText("a"));
    root.Write(L"game/Addins/b_low/core/data/x.gda", files::FromText("b"));
    root.Write(L"documents/AddIns/c_docs/core/data/x.gda", files::FromText("c"));
    root.Write(L"game/Addins/a_high/core/data/y.gda", files::FromText("a data"));
    root.Write(L"game/Addins/a_high/core/override/y.gda", files::FromText("a override"));
    root.Write(L"documents/Settings/AddIns.xml", files::FromText(kThreeAddIns));
    const auto index = install::Index::Build(root.path / L"game", root.path / L"documents");
    CHECK(Order(*index, "x.gda") == std::vector<std::string>{"addin:c_docs", "addin:b_low", "addin:a_high", "core"});
    const auto *x = index->Find("x.gda");
    REQUIRE(x != nullptr);
    // Taken highest Priority first, from 9000 down by 7, 10000 more in the game's folder.
    CHECK((*x)[0].priority == 9000);
    CHECK((*x)[1].priority == 19000 - 14);
    CHECK((*x)[2].priority == 19000 - 7);
    CHECK((*x)[0].settled);
    // An add-in's override before its data: its base less 6.
    const auto *y = index->Find("y.gda");
    REQUIRE(y != nullptr);
    CHECK(Text(index->Read(y->front())) == "a override");
    CHECK(y->front().priority == 19000 - 7 - 6);
}

TEST_CASE("a campaign's files, and an add-in's for it, are the game's only while it is loaded", "[install]")
{
    files::TempFolder root;
    root.Write(L"game/modules/single player/data/z.are", files::FromText("campaign"));
    root.Write(L"game/modules/other/data/z.are", files::FromText("other"));
    root.Write(L"game/Addins/a_high/module/data/z.are", files::FromText("a"));
    root.Write(L"game/Addins/b_low/module/data/z.are", files::FromText("b"));
    root.Write(L"game/Addins/c_docs/module/data/z.are", files::FromText("c"));
    root.Write(L"documents/Settings/AddIns.xml", files::FromText(kThreeAddIns));
    const auto origins = install::Index::Build(root.path / L"game", root.path / L"documents");
    const auto taken = Order(*origins, "z.are");
    REQUIRE(taken.size() == 5);
    CHECK(taken[0] == "addin:a_high");
    CHECK(taken[1] == "campaign");
    const auto none = install::Index::Build(root.path / L"game", root.path / L"documents", {.campaign = ""});
    CHECK(none->Find("z.are")->front().unused == install::Unused::Campaign);
    // A campaign of an add-in's own: its module is the campaign, at the campaign's priorities.
    const auto own = install::Index::Build(root.path / L"game", root.path / L"documents", {.campaign = "C_DOCS"});
    const auto *z = own->Find("z.are");
    REQUIRE(z != nullptr);
    CHECK(own->Sources()[z->front().source].id == "addin:c_docs");
    CHECK(z->front().priority == 50004);
    CHECK((*z)[1].unused == install::Unused::Campaign);
}

namespace
{
// A campaign's .cif whose CampaignCIFPackagesList names these packages.
files::Bytes Cif(const std::vector<std::string> &packages)
{
    constexpr std::uint32_t kPackagesList = 17124;
    constexpr std::uint16_t kString = 14;
    constexpr std::uint16_t kList = 0x8000;
    files::Gff4 cif("CIF ", "V0.1", {{"CIF ", 4, {{kPackagesList, kString, kList, 0}}}});
    const std::uint32_t root = cif.Reserve(4);
    const std::uint32_t list = cif.Reserve(4 + (4 * packages.size()));
    cif.Set32(list, static_cast<std::uint32_t>(packages.size()));
    for (std::size_t i = 0; i < packages.size(); ++i)
    {
        cif.Set32(list + 4 + static_cast<std::uint32_t>(4 * i), cif.String(packages[i]));
    }
    cif.Set32(root, list);
    return cif.bytes;
}
} // namespace

TEST_CASE("a campaign's packages are its sources, behind its own folders and ahead of the core", "[install]")
{
    files::TempFolder root;
    root.Write(L"game/packages/core/data/m.mao", files::FromText("core"));
    root.Write(L"game/packages/core_ep1/data/m.mao", files::FromText("ep1"));
    root.Write(L"game/packages/core_ep1/patch/p.gda", files::FromText("ep1 patch"));
    root.Write(L"game/packages/core_ep1/textures/high/t.dds", files::FromText("ep1 high"));
    root.Write(L"game/packages/core_ep1/textures/medium/t.dds", files::FromText("ep1 medium"));
    root.Write(L"game/packages/second/data/m.mao", files::FromText("second"));
    root.Write(L"game/modules/single player/singleplayer.cif", Cif({}));
    root.Write(L"game/Addins/dao_prc_ep_1/module/dao_prc_ep_1.cif", Cif({"core_ep1", "Second"}));

    // The main campaign loads none: the core's copy, and the package's not the game's.
    const auto origins = install::Index::Build(root.path / L"game", {});
    CHECK(origins->Packages().empty());
    const auto *m = origins->Find("m.mao");
    REQUIRE(m != nullptr);
    CHECK(Text(origins->Read(m->front())) == "core");
    CHECK(Named(*origins, "package:core_ep1")->kind == "package");
    CHECK(std::ranges::count_if(*m, [](const auto &c) { return c.unused == install::Unused::Campaign; }) == 2);

    // Awakening's .cif names two, the first ahead of the second, both ahead of the core.
    const auto awakening = install::Index::Build(root.path / L"game", {}, {.campaign = "DAO_PRC_EP_1"});
    CHECK(awakening->Packages() == std::vector<std::string>{"core_ep1", "second"});
    const auto in = Order(*awakening, "m.mao");
    CHECK(in == std::vector<std::string>{"package:core_ep1", "package:second", "core"});
    const auto *ep1 = awakening->Find("m.mao");
    CHECK((*ep1)[0].priority == 70006);
    CHECK((*ep1)[1].priority == 70106);
    CHECK(awakening->Find("p.gda")->front().priority == 70002);
    const auto *t = awakening->Find("t.dds");
    REQUIRE(t->size() == 2);
    CHECK(Text(awakening->Read(t->front())) == "ep1 high");
    CHECK(t->front().priority == 70005);
    CHECK(t->back().unused == install::Unused::Textures);
}

TEST_CASE("a level's own files, another language's and another quality's are kept, and put after", "[install]")
{
    files::TempFolder root;
    root.Write(L"game/packages/core/env/lvl100/t.dds", files::FromText("level"));
    root.Write(L"game/packages/core/textures/medium/t.dds", files::FromText("medium"));
    root.Write(L"game/packages/core/textures/high/t.dds", files::FromText("high"));
    root.Write(L"game/packages/core/audio/vo/de-de/v.txt", files::FromText("de"));
    root.Write(L"game/packages/core/audio/vo/en-us/v.txt", files::FromText("en"));
    const auto index = install::Index::Build(root.path / L"game", {});
    const auto *t = index->Find("t.dds");
    REQUIRE(t != nullptr);
    REQUIRE(t->size() == 3);
    CHECK(Text(index->Read((*t)[0])) == "high");
    std::vector<install::Unused> why{(*t)[1].unused, (*t)[2].unused};
    std::ranges::sort(why);
    CHECK(why == std::vector<install::Unused>{install::Unused::Level, install::Unused::Textures});
    const auto *v = index->Find("v.txt");
    REQUIRE(v != nullptr);
    CHECK(Text(index->Read(v->front())) == "en");
    CHECK(v->back().unused == install::Unused::Language);
    const auto german = install::Index::Build(root.path / L"game", {}, {.language = "de-de"});
    CHECK(Text(german->Read(german->Find("v.txt")->front())) == "de");
}

TEST_CASE("FMOD's banks and projects by the audio folders' priorities and the language", "[install]")
{
    files::TempFolder root;
    root.Write(L"game/packages/core/audio/sound/gui.fsb", files::FromText("core sound"));
    root.Write(L"game/packages/core/audio/sound/gui.fev", files::FromText("core project"));
    root.Write(L"game/packages/core/audio/vo/en-us/vo/line.fsb", files::FromText("core voice"));
    root.Write(L"game/modules/single player/audio/sound/gui.fsb", files::FromText("campaign sound"));
    root.Write(L"game/modules/single player/audio/vo/en-us/vo/line.fsb", files::FromText("campaign en"));
    root.Write(L"game/modules/single player/audio/vo/de-de/vo/line.fsb", files::FromText("campaign de"));
    root.Write(L"game/Addins/mod/core/audio/sound/gui.fsb", files::FromText("add-in sound"));
    root.Write(L"game/Addins/mod/core/audio/vo/en-us/vo/line.fsb", files::FromText("add-in voice"));
    root.Write(L"documents/Settings/AddIns.xml",
               files::FromText(R"(<AddInsList><AddInItem UID="mod" Priority="1" Enabled="1"/></AddInsList>)"));
    const auto index = install::Index::Build(root.path / L"game", root.path / L"documents");
    const auto read = [&index](std::string_view name) {
        std::vector<std::pair<std::string, std::int32_t>> out;
        for (const auto &copy : *index->Find(name))
        {
            out.emplace_back(copy.unused == install::Unused::No ? Text(index->Read(copy)) : "unused", copy.priority);
        }
        return out;
    };
    // An add-in's sound at its base -1 (19000 in the game's folder), its voice -2.
    using Taken = std::vector<std::pair<std::string, std::int32_t>>;
    CHECK(read("GUI.fsb") == Taken{{"add-in sound", 18999}, {"campaign sound", 50007}, {"core sound", 100008}});
    CHECK(read("line.fsb") ==
          Taken{{"add-in voice", 18998}, {"campaign en", 50006}, {"core voice", 100006}, {"unused", 50006}});
    CHECK(index->Find("line.fsb")->back().unused == install::Unused::Language);
    CHECK(read("gui.fev") == Taken{{"core project", 100008}});
    // A bank is opened by its path, not read whole.
    const auto file = index->FileOf(index->Find("gui.fev")->front());
    REQUIRE(file);
    CHECK(*file == root.path / L"game/packages/core/audio/sound/gui.fev");
    files::TempFolder archived;
    archived.Write(L"game/packages/core/data/a.erf", files::Erf({{"x.gda", files::FromText("x")}}));
    const auto inArchive = install::Index::Build(archived.path / L"game", {});
    CHECK_FALSE(inArchive->FileOf(inArchive->Find("x.gda")->front()));
    const auto german = install::Index::Build(root.path / L"game", root.path / L"documents", {.language = "de-de"});
    CHECK(Text(german->Read(german->Find("line.fsb")->front())) == "campaign de");
}

TEST_CASE("a talk table of another language is not the game's", "[install]")
{
    files::TempFolder root;
    root.Write(L"game/packages/core/data/talktables/core_en-us.tlk", files::FromText("en"));
    root.Write(L"game/packages/core/data/talktables/core_en-us_f.tlk", files::FromText("en, hers"));
    root.Write(L"game/packages/core/data/talktables/core_fr-fr.tlk", files::FromText("fr"));
    root.Write(L"game/packages/core/data/talktables/core_pseudo.tlk", files::FromText("pseudo"));
    root.Write(L"game/packages/core/data/talktables/notes_v2.tlk", files::FromText("no language"));
    const auto english = install::Index::Build(root.path / L"game", {});
    CHECK(english->Find("core_en-us.tlk")->front().unused == install::Unused::No);
    CHECK(english->Find("core_en-us_f.tlk")->front().unused == install::Unused::No);
    CHECK(english->Find("core_fr-fr.tlk")->front().unused == install::Unused::Language);
    CHECK(english->Find("core_pseudo.tlk")->front().unused == install::Unused::Language);
    CHECK(english->Find("notes_v2.tlk")->front().unused == install::Unused::No);
    const auto french = install::Index::Build(root.path / L"game", {}, {.language = "FR-FR"});
    CHECK(french->Find("core_fr-fr.tlk")->front().unused == install::Unused::No);
    CHECK(french->Find("core_en-us.tlk")->front().unused == install::Unused::Language);
}

TEST_CASE("of copies in one folder, a loose file before an archive, the later archive's name first", "[install]")
{
    files::TempFolder root;
    root.Write(L"game/packages/core/patch/p.gda", files::FromText("loose"));
    root.Write(L"game/packages/core/patch/patch000.erf", files::Erf({{"p.gda", files::FromText("000")}}));
    root.Write(L"game/packages/core/patch/patch001.erf", files::Erf({{"p.gda", files::FromText("001")}}));
    const auto index = install::Index::Build(root.path / L"game", {});
    const auto *p = index->Find("p.gda");
    REQUIRE(p != nullptr);
    std::vector<std::string> read;
    for (const auto &copy : *p)
    {
        read.push_back(Text(index->Read(copy)));
    }
    CHECK(read == std::vector<std::string>{"loose", "001", "000"});
}

TEST_CASE("a disabled add-in's copies are not the game's, and with one the order is not settled", "[install]")
{
    files::TempFolder root;
    root.Write(L"game/Addins/a_high/core/data/x.gda", files::FromText("a"));
    root.Write(L"game/Addins/off/core/data/x.gda", files::FromText("off"));
    root.Write(L"documents/Settings/AddIns.xml",
               files::FromText(R"(<AddInsList><AddInItem UID="A_HIGH" Priority="200" Enabled="1"/>)"
                               R"(<AddInItem UID="OFF" Priority="1" Enabled="0"/></AddInsList>)"));
    const auto index = install::Index::Build(root.path / L"game", root.path / L"documents");
    CHECK(Order(*index, "x.gda") == std::vector<std::string>{"addin:a_high", "addin:off (unused)"});
    CHECK(index->Find("x.gda")->back().unused == install::Unused::Disabled);
    CHECK_FALSE(index->Find("x.gda")->front().settled);
}

TEST_CASE("the game's order of its add-ins: MSVC 2005's sort over AddIns.xml from its end", "[install]")
{
    // AddIns.xml on the GOG install, 2026-10-12, in its order: UID and Priority.
    const std::vector<std::pair<const char *, int>> file{
        {"DAO_PRC_EP_1", 110},      {"DAO_PRC_CP_1", 200},    {"DAO_PRC_CP_2", 100},      {"DAO_PRC_CP_3", 300},
        {"DAO_PRC_CP_7", 302},      {"DAO_PRC_DRK", 500},     {"DAO_PRC_DRK_DAO", 501},   {"DAO_PRC_DRK_EP1", 502},
        {"DAO_PRC_GIB", 595},       {"DAO_PRC_GIB_DAO", 100}, {"DAO_PRC_GIB_EP1", 450},   {"DAO_PRC_LEL", 510},
        {"DAO_PRC_LEL_DAO", 511},   {"DAO_PRC_LEL_EP1", 512}, {"DAO_PRC_NRX_1", 1},       {"DAO_PRC_STR", 100},
        {"DAO_PRC_STR_DAO", 501},   {"DAO_PRC_STR_EP1", 502}, {"DAO_EP1_PROMO_BTK", 100}, {"DAO_EP1_PROMO_GBK", 100},
        {"DAO_EP1_PROMO_PAN", 100}, {"DAO_PRC_PROMO_1", 1},   {"DAO_PRC_PROMO_2", 1},     {"DAO_PRC_PROMO_3", 1},
        {"DAO_PRC_PROMO_4", 1},     {"DAO_PRC_PROMO_5", 1},   {"DAO_PRC_PROMO_BTK", 100}, {"DAO_PRC_PROMO_C2", 1},
        {"DAO_PRC_PROMO_EDG", 1},   {"DAO_PRC_PROMO_EMB", 1}, {"DAO_PRC_PROMO_GBK", 100}, {"DAO_PRC_PROMO_HOD", 1},
        {"DAO_PRC_PROMO_LKS", 1},   {"DAO_PRC_PROMO_LNP", 1}, {"DAO_PRC_PROMO_ME", 1},    {"DAO_PRC_PROMO_PAN", 100},
        {"DAO_PRC_PROMO_VGL", 1},   {"DAO_PRC_PROMO_WAR", 1}, {"DAO_PRC_PROMO_WKD", 1},
    };
    // The order the game took them in, by their folders' priorities in play (dev/FILES.md).
    const std::vector<std::string> seen{
        "DAO_PRC_GIB",       "DAO_PRC_LEL_EP1",   "DAO_PRC_LEL_DAO",   "DAO_PRC_LEL",       "DAO_PRC_DRK_EP1",
        "DAO_PRC_STR_EP1",   "DAO_PRC_DRK_DAO",   "DAO_PRC_STR_DAO",   "DAO_PRC_DRK",       "DAO_PRC_GIB_EP1",
        "DAO_PRC_CP_7",      "DAO_PRC_CP_3",      "DAO_PRC_CP_1",      "DAO_PRC_EP_1",      "DAO_PRC_CP_2",
        "DAO_PRC_PROMO_PAN", "DAO_PRC_PROMO_GBK", "DAO_PRC_PROMO_BTK", "DAO_EP1_PROMO_PAN", "DAO_EP1_PROMO_GBK",
        "DAO_EP1_PROMO_BTK", "DAO_PRC_STR",       "DAO_PRC_GIB_DAO",   "DAO_PRC_PROMO_WAR", "DAO_PRC_NRX_1",
        "DAO_PRC_PROMO_3",   "DAO_PRC_PROMO_4",   "DAO_PRC_PROMO_5",   "DAO_PRC_PROMO_C2",  "DAO_PRC_PROMO_2",
        "DAO_PRC_PROMO_EDG", "DAO_PRC_PROMO_EMB", "DAO_PRC_PROMO_HOD", "DAO_PRC_PROMO_LKS", "DAO_PRC_PROMO_LNP",
        "DAO_PRC_PROMO_ME",  "DAO_PRC_PROMO_1",   "DAO_PRC_PROMO_VGL", "DAO_PRC_PROMO_WKD",
    };
    std::vector<dao::core::AddIn> items;
    items.reserve(file.size());
    for (const auto &[uid, priority] : file)
    {
        dao::core::AddIn item;
        item.uid = uid;
        item.priority = priority;
        item.enabled = true;
        items.push_back(item);
    }
    std::vector<std::string> taken;
    for (const std::size_t i : dao::core::AddInOrder(items, true))
    {
        taken.push_back(items[i].uid);
    }
    CHECK(taken == seen);
}

TEST_CASE("the game's order of its offers: offers.xml's from its end, by Priority", "[install]")
{
    // offers.xml on the GOG install, 2026-10-12, and the order the game took them in.
    const std::vector<std::pair<const char *, int>> file{
        {"DAO_PRC_OCP_13", 100}, {"DAO_PRC_DRK_O", 503},     {"DAO_PRC_GIB_O", 100},
        {"DAO_PRC_LEL_O", 513},  {"DAO_PRC_OPROMO_ME", 512}, {"DAO_PRC_STR_O", 503},
        {"DAO_PRC_ONRX_1", 1},   {"DAO_PRC_OCP_1", 199},     {"DAO_PRC_OCP_2", 99}};
    const std::vector<std::string> seen{"DAO_PRC_LEL_O",  "DAO_PRC_OPROMO_ME", "DAO_PRC_STR_O",
                                        "DAO_PRC_DRK_O",  "DAO_PRC_OCP_1",     "DAO_PRC_GIB_O",
                                        "DAO_PRC_OCP_13", "DAO_PRC_OCP_2",     "DAO_PRC_ONRX_1"};
    std::vector<dao::core::AddIn> items;
    items.reserve(file.size());
    for (const auto &[uid, priority] : file)
    {
        dao::core::AddIn item;
        item.uid = uid;
        item.priority = priority;
        items.push_back(item);
    }
    std::vector<std::string> taken;
    for (const std::size_t i : dao::core::AddInOrder(items, false))
    {
        taken.push_back(items[i].uid);
    }
    CHECK(taken == seen);
}

namespace
{

// All an index says, as lines: its sources, its archives and every copy in its order.
std::vector<std::string> Summary(const install::Index &index)
{
    std::vector<std::string> lines;
    for (const auto &source : index.Sources())
    {
        lines.push_back(source.id + " " + source.folder + " " + std::to_string(source.archives) + " " +
                        std::to_string(source.loose) + " " + std::to_string(source.files) + " " +
                        std::to_string(source.unreadable));
    }
    for (const auto &archive : index.Archives())
    {
        lines.push_back(archive.path + " " + std::to_string(archive.header.count) + " " +
                        std::to_string(archive.header.flags) + " " + std::to_string(archive.files));
    }
    for (const auto &[name, copies] : index.Resources())
    {
        for (const auto &copy : copies)
        {
            lines.push_back(name + " " + index.Where(copy) + " " + std::to_string(copy.entry) + " " +
                            std::to_string(copy.size) + " " + std::to_string(copy.priority));
        }
    }
    return lines;
}

} // namespace

TEST_CASE("archives' directories kept in a cache folder are read again only when changed", "[install]")
{
    const Folder folder;
    const files::TempFolder cache;
    install::Options options;
    options.cache = cache.path;
    const auto build = [&folder, &options] {
        return install::Index::Build(folder.root.path / L"game", folder.root.path / L"documents", options);
    };
    const auto none = folder.Build();
    const std::size_t archives = none->Archives().size();
    REQUIRE(archives >= 4);

    const auto first = build();
    CHECK(first->Measured().read == archives);
    CHECK(first->Measured().kept == 0);
    CHECK(std::filesystem::exists(cache.path / L"archives.bin"));
    const auto second = build();
    CHECK(second->Measured().read == 0);
    CHECK(second->Measured().kept == archives);
    CHECK(second->Measured().unopened == none->Measured().unopened); // a file that is no archive is tried again
    CHECK(Summary(*second) == Summary(*none));
    CHECK(Text(second->Read(second->Find("abi_base.gda")->front())) ==
          Text(none->Read(none->Find("abi_base.gda")->front())));

    // One archive changed, its size alone (its time put back): only it is read again, and the
    // index is as one built without.
    const std::filesystem::path b = folder.root.path / L"game/modules/other/data/b.rim";
    const auto time = std::filesystem::last_write_time(b);
    folder.root.Write(L"game/modules/other/data/b.rim",
                      files::Erf({{"b.are", files::FromText("b")}, {"c.are", files::FromText("cc")}}));
    std::filesystem::last_write_time(b, time);
    const auto changed = build();
    CHECK(changed->Measured().read == 1);
    CHECK(changed->Measured().kept == archives - 1);
    CHECK(Summary(*changed) == Summary(*folder.Build()));
    CHECK(build()->Measured().read == 0);

    // Its time alone, the same bytes: read again too.
    std::filesystem::last_write_time(b, time + std::chrono::seconds(10));
    CHECK(build()->Measured().read == 1);
    CHECK(build()->Measured().read == 0);

    // One gone: the rest are kept, and so is a file without it.
    std::filesystem::remove(folder.root.path / L"game/modules/other/data/b.rim");
    const auto fewer = build();
    CHECK(fewer->Measured().read == 0);
    CHECK(fewer->Measured().kept == archives - 1);
    CHECK(install::LoadKept(cache.path).size() == archives - 1);

    // A cache that does not read is none: every archive is read, and the file made again.
    folder.root.Write(L"cache-bad/archives.bin", files::FromText("not a cache"));
    options.cache = folder.root.path / L"cache-bad";
    CHECK(build()->Measured().read == archives - 1);
    CHECK(install::LoadKept(options.cache).size() == archives - 1);
}

// The GOG install, where there is one: the index's choice against what the game itself said
// it took (examples/resources's where.txt, 2026-10-12). Hidden, as it reads ten thousand
// archives: run it by its tag, dao_core_tests "[install-real]".
TEST_CASE("on the GOG install, the copy the index puts first is the one the game took", "[.][install-real]")
{
    const std::filesystem::path game = L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins";
    if (!std::filesystem::exists(game / L"bin_ship"))
    {
        SKIP("no GOG install here");
    }
    const auto index = install::Index::Build(game, {});
    const auto *rules = index->Find("rules_core.ncs");
    REQUIRE(rules != nullptr);
    CHECK(index->Where(rules->front()) == "packages/core/data/designerscripts.rim");
    CHECK(rules->front().priority == 100004);
    const auto *base = index->Find("2da_base.gda");
    REQUIRE(base != nullptr);
    CHECK(index->Where(base->front()) == "packages/core/patch/patch003.erf");
    CHECK(base->front().priority == 100000);
    // FMOD's files, by the resource manager's order (DAOLib #191): read in the exe, not seen.
    const auto *gui = index->Find("gui.fsb");
    REQUIRE(gui != nullptr);
    CHECK(index->Where(gui->front()) == "packages/core/audio/sound/gui.fsb");
    CHECK(gui->front().priority == 100008);
    const auto *jowan = index->Find("bhm100_jowan.fsb");
    REQUIRE(jowan != nullptr);
    REQUIRE(jowan->size() == 5);
    CHECK(index->Where(jowan->front()) == "modules/single player/audio/vo/en-us/vo/bhm100_jowan.fsb");
    CHECK(jowan->front().priority == 50006);
    CHECK(std::ranges::count(*jowan, install::Unused::Language, &install::Copy::unused) == 4);
}

// The GOG install's archives kept and read again from a cache, with the times each took. Hidden
// as above: dao_core_tests "[install-real]".
TEST_CASE("on the GOG install, an index built from its cache is the one built without", "[.][install-real]")
{
    const std::filesystem::path game = L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins";
    if (!std::filesystem::exists(game / L"bin_ship"))
    {
        SKIP("no GOG install here");
    }
    const files::TempFolder cache;
    install::Options options;
    options.cache = cache.path;
    const auto read = install::Index::Build(game, {}, options);
    const auto kept = install::Index::Build(game, {}, options);
    CHECK(kept->Measured().read == 0);
    CHECK(kept->Measured().kept == read->Measured().read);
    CHECK(Summary(*kept) == Summary(*read));
    WARN("read " << read->Measured().ms << " ms, from the cache " << kept->Measured().ms << " ms, "
                 << kept->Measured().kept << " archives");
}

TEST_CASE("a table is parsed once and kept, and a worksheet merged from its files in the game's order", "[install]")
{
    files::TempFolder root;
    const std::vector<std::uint32_t> baseHashes{dao::core::GdaColumnHash("ID"), dao::core::GdaColumnHash("Label"),
                                                dao::core::GdaColumnHash("Worksheet")};
    root.Write(
        L"game/packages/core/override/m2da_base.gda",
        files::Gda({"ID", "Label", "Worksheet"}, {{7, std::string("Abilities"), std::string("ABI_")}}, baseHashes));
    const std::vector<std::uint32_t> hashes{dao::core::GdaColumnHash("ID"), dao::core::GdaColumnHash("Label")};
    root.Write(L"game/packages/core/override/abi_base.gda",
               files::Gda({"ID", "Label"}, {{1, std::string("one")}, {2, std::string("two")}}, hashes));
    root.Write(L"game/packages/core/override/abi_more.gda",
               files::Gda({"ID", "Label"}, {{3, std::string("three")}}, hashes));
    const auto index = install::Index::Build(root.path / L"game", {});

    // Kept: the same table for a name in any case (DAOLib #207).
    const auto table = index->Table("abi_base.gda");
    REQUIRE(table != nullptr);
    CHECK(table->Rows().size() == 2);
    CHECK(index->Table("ABI_BASE.GDA") == table);
    CHECK(index->Table("none.gda") == nullptr);

    const auto *merged = index->Worksheet(7);
    REQUIRE(merged != nullptr);
    CHECK(merged->rows.size() == 3);
    CHECK(dao::core::m2da::Taken(*merged, 3) != nullptr);
    CHECK(index->Worksheet(7) == merged);
    CHECK(index->Worksheet(8) == nullptr);
}

// A table only another campaign has is not the loaded campaign's: Darkspawn Chronicles'
// portraits_drk.gda had come into the Single Player campaign's portraits (DAOLib #249).
TEST_CASE("a worksheet takes no file the game does not have, such as another campaign's", "[install]")
{
    files::TempFolder root;
    const std::vector<std::uint32_t> baseHashes{dao::core::GdaColumnHash("ID"), dao::core::GdaColumnHash("Label"),
                                                dao::core::GdaColumnHash("Worksheet")};
    root.Write(L"game/packages/core/override/m2da_base.gda",
               files::Gda({"ID", "Label", "Worksheet"}, {{7, std::string("Portraits"), std::string("portraits")}},
                          baseHashes));
    const std::vector<std::uint32_t> hashes{dao::core::GdaColumnHash("ID"), dao::core::GdaColumnHash("Label")};
    root.Write(L"game/packages/core/override/portraits.gda",
               files::Gda({"ID", "Label"}, {{1, std::string("default")}}, hashes));
    root.Write(L"game/modules/single player/data/portraits_sp.gda",
               files::Gda({"ID", "Label"}, {{2, std::string("ours")}}, hashes));
    root.Write(L"game/modules/other/data/portraits_drk.gda",
               files::Gda({"ID", "Label"}, {{3, std::string("player")}}, hashes));

    const auto index = install::Index::Build(root.path / L"game", {});
    REQUIRE(index->Find("portraits_drk.gda") != nullptr);
    CHECK(index->Find("portraits_drk.gda")->front().unused == install::Unused::Campaign);
    const auto *merged = index->Worksheet(7);
    REQUIRE(merged != nullptr);
    CHECK(merged->rows.size() == 2);
    CHECK(dao::core::m2da::Taken(*merged, 2) != nullptr);
    CHECK(dao::core::m2da::Taken(*merged, 3) == nullptr);

    // The other campaign loaded: its table is the game's, and the Single Player campaign's is not.
    const auto other = install::Index::Build(root.path / L"game", {}, {.campaign = "OTHER"});
    const auto *theirs = other->Worksheet(7);
    REQUIRE(theirs != nullptr);
    CHECK(dao::core::m2da::Taken(*theirs, 3) != nullptr);
    CHECK(dao::core::m2da::Taken(*theirs, 2) == nullptr);
}
