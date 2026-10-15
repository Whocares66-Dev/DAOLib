#include "dao/core/Erf.h"
#include "dao/core/MaterialObject.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace mao = dao::core::mao;

namespace
{

// arm_mash.mao as Elgara's override has it (ghilana, 2026-10-15): the Material Editor's export
// with CRLF, then lines Elgara added with LF, a texture after a parameter among them.
constexpr std::string_view kArmMash =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
    "<!-- Exported by Material Editor --><!-- dc:source=file:///C:/BioWareDrive/DragonAge/SourceControl/ArtContent/"
    "PRC/Art/Characters/MATERIAL_PROJECTS/EP1/ARM_MAS.matproj --><MaterialObject Name=\"ARM_MASh\">\r\n"
    "\t<Material Name=\"Character.mat\"></Material>\r\n"
    "\t<DefaultSemantic Name=\"EmissiveNoTint\"></DefaultSemantic>\r\n"
    "\t<Texture Name=\"mml_tDiffuse\" ResName=\"pn_arm_masi_0d.dds\"></Texture>\r\n"
    "\t<Texture Name=\"mml_tEmissiveMask\" ResName=\"Default_Black.dds\"></Texture>\r\n"
    "\t<Texture Name=\"mml_tNormalMap\" ResName=\"pn_arm_masd_0n.dds\"></Texture>\r\n"
    "\t<Texture Name=\"mml_tSpecularMask\" ResName=\"pn_arm_masi_0s.dds\"></Texture>\r\n"
    "\t<Vector4f Name=\"mml_vFalloffParams\" value=\"0.00 1.00 1.50 1.50\"></Vector4f>\r\n"
    "\t<Float Name=\"mml_fSpecularReflectionMult\" value=\"1.00\"></Float>\r\n"
    "\t<Float Name=\"mml_fDaoMetal\" value=\"1.00\"></Float>\n"
    "\t<Texture Name=\"mml_tDaoMetal\" ResName=\"pn_arm_masi_0_m.dds\"></Texture>\n"
    "\t<Int Name=\"mml_iSoundMaterialType\" value=\"7\"></Int>\n"
    "</MaterialObject>";

} // namespace

TEST_CASE("a material object's definition, textures and parameters, in file order", "[mao]")
{
    const auto read = mao::Read(kArmMash);
    REQUIRE(read);
    CHECK(read->name == "ARM_MASh");
    CHECK(read->material == "Character.mat");
    CHECK(read->defaultSemantic == "EmissiveNoTint");
    REQUIRE(read->textures.size() == 5);
    CHECK(read->textures[0].slot == "mml_tDiffuse");
    CHECK(read->textures[0].resource == "pn_arm_masi_0d.dds");
    CHECK(read->textures[4].slot == "mml_tDaoMetal");
    CHECK(read->textures[4].resource == "pn_arm_masi_0_m.dds");
    REQUIRE(read->parameters.size() == 4);
    CHECK(read->parameters[0].type == "Vector4f");
    CHECK(read->parameters[0].name == "mml_vFalloffParams");
    CHECK(read->parameters[0].value == "0.00 1.00 1.50 1.50");
    CHECK(read->parameters[3].type == "Int");
    CHECK(read->ParameterNamed("mml_iSoundMaterialType")->value == "7");
    CHECK(read->TextureIn("mml_tNormalMap")->resource == "pn_arm_masd_0n.dds");
    CHECK(read->TextureIn("mml_tNone") == nullptr);
}

TEST_CASE("a material object's source path and its close", "[mao]")
{
    const auto read = mao::Read(kArmMash);
    REQUIRE(read);
    CHECK(
        read->source ==
        "C:/BioWareDrive/DragonAge/SourceControl/ArtContent/PRC/Art/Characters/MATERIAL_PROJECTS/EP1/ARM_MAS.matproj");
    CHECK(kArmMash.substr(read->end) == "</MaterialObject>");
}

TEST_CASE("a material object with no source, a slot twice, and the close not last", "[mao]")
{
    constexpr std::string_view kText = "<MaterialObject Name=\"a\"><Material Name=\"Terrain.mat\"></Material>"
                                       "<Texture Name=\"t\" ResName=\"one.dds\"></Texture>"
                                       "<Texture Name=\"t\" ResName=\"two.dds\"></Texture>"
                                       "</MaterialObject>\r\n";
    const auto read = mao::Read(kText);
    REQUIRE(read);
    CHECK(read->source.empty());
    CHECK(read->defaultSemantic.empty());
    REQUIRE(read->textures.size() == 2);
    CHECK(read->TextureIn("t")->resource == "one.dds");
    CHECK(read->textures[1].resource == "two.dds");
    CHECK(kText.substr(read->end, 17) == "</MaterialObject>");
}

TEST_CASE("a source path's spaces before its comment's end are not its own", "[mao]")
{
    const auto spaced = mao::Read("<!-- dc:source=file:///C:/a b/P.matproj   --><MaterialObject Name=\"a\">"
                                  "</MaterialObject>");
    REQUIRE(spaced);
    CHECK(spaced->source == "C:/a b/P.matproj");
    const auto tight = mao::Read("<!-- dc:source=file:///C:/P.matproj--><MaterialObject></MaterialObject>");
    REQUIRE(tight);
    CHECK(tight->source == "C:/P.matproj");
}

TEST_CASE("the last close is the one elements go before", "[mao]")
{
    constexpr std::string_view kText = "<!-- an old </MaterialObject> --><MaterialObject Name=\"a\">"
                                       "<Material Name=\"m.mat\"></Material></MaterialObject>";
    const auto read = mao::Read(kText);
    REQUIRE(read);
    CHECK(read->end == kText.size() - 17);
    CHECK(read->material == "m.mat");
}

TEST_CASE("text that is not a material object", "[mao]")
{
    CHECK_FALSE(mao::Read(""));
    CHECK_FALSE(mao::Read("<Material Name=\"x\"></Material>"));
    CHECK_FALSE(mao::Read("</MaterialObject>"));
    // An attribute named like the end of another is not taken for it.
    const auto read = mao::Read("<MaterialObject Name=\"a\"><Texture Name=\"s\" XResName=\"no\" ResName=\"yes\">"
                                "</Texture></MaterialObject>");
    REQUIRE(read);
    CHECK(read->textures[0].resource == "yes");
}

// The GOG install's own material objects, where there is one: each reads, with a definition and
// its name as the file's. Hidden, as it reads the archive: run it by its tag, dao_core_tests
// "[mao-real]".
TEST_CASE("on the GOG install, every material object reads", "[.][mao-real]")
{
    const std::filesystem::path archive =
        L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins/packages/core/data/materialobjects.erf";
    if (!std::filesystem::exists(archive))
    {
        SKIP("no GOG install here");
    }
    const auto erf = dao::core::erf::Archive::Open(archive);
    REQUIRE(erf);
    std::size_t read = 0;
    std::size_t withTextures = 0;
    for (const auto &entry : erf->Entries())
    {
        const auto bytes = erf->Read(entry);
        REQUIRE(bytes);
        const std::string_view text(reinterpret_cast<const char *>(bytes->data()), bytes->size());
        const auto object = mao::Read(text);
        INFO("entry " << read);
        REQUIRE(object);
        CHECK_FALSE(object->material.empty());
        CHECK(text.substr(object->end, 17) == "</MaterialObject>");
        ++read;
        withTextures += object->textures.empty() ? 0 : 1;
    }
    WARN(read << " material objects, " << withTextures << " with textures");
    CHECK(read > 1000);
}
