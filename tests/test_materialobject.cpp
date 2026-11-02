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
    // The Int is no kind the engine files (render-19): three parameters, not four.
    REQUIRE(read->parameters.size() == 3);
    CHECK(read->parameters[0].name == "mml_vFalloffParams");
    REQUIRE(read->parameters[0].Get<mao::Vector4>() != nullptr);
    CHECK(*read->parameters[0].Get<mao::Vector4>() == mao::Vector4{0.0F, 1.0F, 1.5F, 1.5F});
    CHECK(read->parameters[0].Get<float>() == nullptr);
    REQUIRE(read->Find<float>("mml_fDaoMetal") != nullptr);
    CHECK(*read->Find<float>("mml_fDaoMetal") == 1.0F);
    CHECK(read->ParameterNamed("mml_iSoundMaterialType") == nullptr);
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

TEST_CASE("an element's name is matched whatever its case, as Clouds.mao's <texture>", "[mao]")
{
    // Clouds.mao's shape (DAOLib #209): the textures written in lower case, single-quoted.
    constexpr std::string_view kClouds = "<MaterialObject Name='Clouds'>\n"
                                         "<Material Name='Clouds.mat' />\n"
                                         "<texture Name='SkyTextureA' ResName='SB03_BCloud' />\n"
                                         "<TEXTURE Name='PerlinNoise' ResName='perlin' />\n"
                                         "<float Name='mml_fCloudSpeed' value='0.5' />\n"
                                         "</MaterialObject>";
    const auto read = mao::Read(kClouds);
    REQUIRE(read);
    CHECK(read->material == "Clouds.mat");
    REQUIRE(read->textures.size() == 2);
    REQUIRE(read->TextureIn("SkyTextureA") != nullptr);
    CHECK(read->TextureIn("SkyTextureA")->resource == "SB03_BCloud");
    CHECK(read->TextureIn("PerlinNoise") != nullptr);
    // A lower-case texture is no parameter, and a lower-case parameter is still one.
    REQUIRE(read->parameters.size() == 1);
    CHECK(read->parameters[0].name == "mml_fCloudSpeed");
    CHECK(*read->parameters[0].Get<float>() == 0.5F);
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

TEST_CASE("each of the nine kinds is kept in its own type", "[mao]")
{
    constexpr std::string_view kText = "<MaterialObject Name=\"all\">"
                                       "<Material Name=\"Character.mat\"/>"
                                       "<DefaultSemantic Name=\"Alpha\"/>"
                                       "<Override Name=\"Other\"/>"
                                       "<Texture Name=\"mml_tDiffuse\" ResName=\"a.dds\"/>"
                                       "<Texture Name=\"mml_tLightmap\" ResName=\"lm.dds\" RequiresID=\"TRUE\"/>"
                                       "<Vector4f Name=\"v\" value=\"1 2 3 4\"/>"
                                       "<Vector4fArray Name=\"va\" value=\"1 2 3 4  5 6 7 8\"/>"
                                       "<Matrix4x4f Name=\"m\" value=\"1 0 0 0 0 1 0 0 0 0 1 0 5 6 7 1\"/>"
                                       "<Float Name=\"f\" value=\"-2.5\"/>"
                                       "<SoundType Name=\"s\" value=\"7\"/>"
                                       "</MaterialObject>";
    const auto read = mao::Read(kText);
    REQUIRE(read);
    CHECK(read->material == "Character.mat");
    CHECK(read->defaultSemantic == "Alpha");
    CHECK(read->overrideName == "Other");
    REQUIRE(read->textures.size() == 2);
    CHECK_FALSE(read->textures[0].requiresId);
    CHECK(read->textures[1].requiresId);
    REQUIRE(read->parameters.size() == 5);
    CHECK(*read->Find<mao::Vector4>("v") == mao::Vector4{1.0F, 2.0F, 3.0F, 4.0F});
    const auto *array = read->Find<mao::Vector4Array>("va");
    REQUIRE(array != nullptr);
    REQUIRE(array->size() == 2);
    CHECK((*array)[1] == mao::Vector4{5.0F, 6.0F, 7.0F, 8.0F});
    const auto *matrix = read->Find<mao::Matrix4x4>("m");
    REQUIRE(matrix != nullptr);
    CHECK((*matrix)[12] == 5.0F);
    CHECK((*matrix)[15] == 1.0F);
    CHECK(*read->Find<float>("f") == -2.5F);
    REQUIRE(read->Find<mao::SoundType>("s") != nullptr);
    CHECK(read->Find<mao::SoundType>("s")->value == 7);
    // File order, whatever the type.
    CHECK(read->parameters[0].name == "v");
    CHECK(read->parameters[4].name == "s");
}

TEST_CASE("the engine files an element only with what it needs", "[mao]")
{
    // A Texture with no ResName, a Float with no value and an element with no Name are not filed
    // (0x005078A2, 0x00507B69 and the flag at +0x20); a short Vector4f leaves the rest zero.
    const auto read = mao::Read("<MaterialObject Name=\"a\"><Texture Name=\"t\"/><Float Name=\"f\"/>"
                                "<Float value=\"1\"/><Material/><Vector4f Name=\"v\" value=\"1 2\"/>"
                                "</MaterialObject>");
    REQUIRE(read);
    CHECK(read->textures.empty());
    CHECK(read->material.empty());
    REQUIRE(read->parameters.size() == 1);
    CHECK(*read->Find<mao::Vector4>("v") == mao::Vector4{1.0F, 2.0F, 0.0F, 0.0F});
}

TEST_CASE("an element is filed at its close, so a later one closes before an earlier open one", "[mao]")
{
    const auto read = mao::Read("<MaterialObject Name=\"a\"><Float Name=\"outer\" value=\"1\">"
                                "<Float Name=\"inner\" value=\"2\"/></Float></MaterialObject>");
    REQUIRE(read);
    REQUIRE(read->parameters.size() == 2);
    CHECK(read->parameters[0].name == "inner");
    CHECK(read->parameters[1].name == "outer");
}

TEST_CASE("the first MaterialObject wins, and attribute names are matched without case", "[mao]")
{
    const auto read = mao::Read("<MaterialObject ID=\"first\"><MATERIAL NAME=\"a.mat\"/>"
                                "<MaterialObject Name=\"second\"><float name=\"f\" VALUE=\"3\"/>"
                                "</MaterialObject></MaterialObject>");
    REQUIRE(read);
    CHECK(read->name == "first");
    CHECK(read->material == "a.mat");
    REQUIRE(read->Find<float>("f") != nullptr);
    CHECK(*read->Find<float>("f") == 3.0F);
}

TEST_CASE("a Vector4fArray's value that ends in white space has one more, zero, vector", "[mao]")
{
    // Read in the engine's code (0x0050845E..0x00508512), not run.
    const auto read = mao::Read("<MaterialObject Name=\"a\"><Vector4fArray Name=\"x\" value=\"1 2 3 4 \"/>"
                                "<Vector4fArray Name=\"y\" value=\"1 2 3 4\"/></MaterialObject>");
    REQUIRE(read);
    CHECK(read->parameters[0].Get<mao::Vector4Array>()->size() == 2);
    CHECK(read->parameters[1].Get<mao::Vector4Array>()->size() == 1);
}

namespace
{

// What the material objects read hold, for the install test's line.
struct Tally
{
    std::size_t read = 0;
    std::size_t withTextures = 0;
    std::size_t floats = 0;
    std::size_t vectors = 0;
    std::size_t arrays = 0;
    std::size_t matrices = 0;
    std::size_t sounds = 0;
    std::size_t overrides = 0;
    std::size_t required = 0;

    void Add(const mao::MaterialObject &object)
    {
        ++read;
        withTextures += object.textures.empty() ? 0 : 1;
        overrides += object.overrideName.empty() ? 0 : 1;
        for (const auto &texture : object.textures)
        {
            required += texture.requiresId ? 1 : 0;
        }
        for (const auto &parameter : object.parameters)
        {
            floats += parameter.Get<float>() != nullptr ? 1 : 0;
            vectors += parameter.Get<mao::Vector4>() != nullptr ? 1 : 0;
            arrays += parameter.Get<mao::Vector4Array>() != nullptr ? 1 : 0;
            matrices += parameter.Get<mao::Matrix4x4>() != nullptr ? 1 : 0;
            sounds += parameter.Get<mao::SoundType>() != nullptr ? 1 : 0;
        }
    }
};

} // namespace

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
    Tally tally;
    for (const auto &entry : erf->Entries())
    {
        const auto bytes = erf->Read(entry);
        REQUIRE(bytes);
        const std::string_view text(reinterpret_cast<const char *>(bytes->data()), bytes->size());
        const auto object = mao::Read(text);
        INFO("entry " << tally.read);
        REQUIRE(object);
        CHECK_FALSE(object->material.empty());
        CHECK(text.substr(object->end, 17) == "</MaterialObject>");
        tally.Add(*object);
    }
    WARN(tally.read << " material objects, " << tally.withTextures << " with textures, " << tally.required
                    << " RequiresID; " << tally.floats << " Float, " << tally.vectors << " Vector4f, " << tally.arrays
                    << " Vector4fArray, " << tally.matrices << " Matrix4x4f, " << tally.sounds << " SoundType, "
                    << tally.overrides << " with an Override");
    CHECK(tally.read > 1000);
}
