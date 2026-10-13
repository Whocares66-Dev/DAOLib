// An install's sources and copies (dao/core/Install.h), and the settings
// that list its add-ins (dao/core/AddIns.h), over a folder made in the test
// as the GOG install is laid out.
#include "dao/core/AddIns.h"
#include "dao/core/GdaHash.h"
#include "dao/core/Install.h"
#include "files_builder.h"

#include <catch2/catch_test_macros.hpp>

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
        root.Write(L"game/bin_ship/daolib/plugins/mymod/override/ogre.utc", files::FromText("plugin"));
        root.Write(L"game/bin_ship/daolib/plugins/nofiles/mymod.dll", files::FromText("dll"));
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
    CHECK(core->loose == 1);    // the talk table; neither the .fsb nor the override folder's
    CHECK(core->files == 3);
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

TEST_CASE("every copy of a resource, by its name in any case, each read", "[install]")
{
    const Folder folder;
    const auto index = folder.Build();
    const auto *copies = index->Find("OGRE.utc");
    REQUIRE(copies != nullptr);
    REQUIRE(copies->size() == 4); // the core's archive, its override, the plugin's, Documents'
    CHECK(index->Where((*copies)[0]) == "packages/core/data/2da.erf");
    std::vector<std::string> read;
    for (const auto &copy : *copies)
    {
        read.push_back(Text(index->Read(copy)));
    }
    CHECK(read == std::vector<std::string>{"ogre", "override", "plugin", "documents"});
    CHECK(index->Resources().contains("abi_base.gda"));
    CHECK(index->Find("nothing.utc") == nullptr);
    CHECK(index->Measured().resources == index->Resources().size());
}

TEST_CASE("a copy in an enciphered archive is listed and not read", "[install]")
{
    const Folder folder;
    const auto index = folder.Build();
    const auto *copies = index->Find("abi_cp1.gda");
    REQUIRE(copies != nullptr);
    CHECK_FALSE(index->Readable(copies->front()));
    CHECK_FALSE(index->Read(copies->front()));
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
