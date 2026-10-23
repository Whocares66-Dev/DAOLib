#include "dao/core/MaterialDefinition.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace mat = dao::core::mat;

namespace
{

// A definition as character.mat.xml has one (packages/core/data/materials.erf, 2026-10-17): spaces
// about some `=` and none about others, a comment holding an old pass, a parameter set a pass
// refers to, and a semantic whose two logic blocks give one parameter different defaults.
constexpr std::string_view kCharacter = R"(<?xml version="1.0" encoding="UTF-8" ?>
<Material Name = "Character">
  <ParameterSet Name="Common Character Params">
    <InternalParameter Type = "Matrix4x4f" Name = "aur_mWorldViewProj" Usage = "ModelViewProjectionTrans"/>
    <Parameter Type = "Vector4f" Name = "mml_vFalloffParams" Default = "0.65 10.0 1.5 1.5"/>
    <EffectParameter EffectName="da_distortion" EffectID="da_distortion"/>
  </ParameterSet>
  <Semantic Name ="LerpedTint" RenderClass ="Character,Transparent">
    <Logic Note="Root Logic Block">
      <Logic Note="Shader Model 3">
        <LogicTest ShaderModel="3" Detail="LOW" />
        <Technique Name="DefaultTechnique">
          <!-- <Pass Name="Old" VertexShader="Old.vsh" PixelShader="Old.psh" State="Old.sta"> -->
          <Pass Name="Pass 1" VertexShader="Skin_LOD0.vsh" PixelShader="Skin_LOD0.psh" State="Skin.sta">
            <ParameterSetRef Ref="Common Character Params"/>
            <Parameter Type="Texture" Name="mml_tDiffuse" SamplerName="smpDecal" Default="Default_Missing" Stage="0"/>
            <Parameter Type="Float" Name="mml_fSpecularReflectionMult" Default="1.0"/>
            <Parameter Type="Vector4fArray" Name="mml_vTintMaskColours" Size="6"/>
            <RequiredStreamData Name="POSITION"/>
            <RequiredStreamData Name="TEXCOORD0"/>
          </Pass>
        </Technique>
      </Logic>
      <Logic Note = "Shader Model 2">
        <LogicTest ShaderModel = "2"/>
        <Technique Name="DefaultTechnique">
          <Pass Name="Pass 1" VertexShader="Skin_SM2.vsh" PixelShader="Skin_SM2.psh" State="Skin.sta">
            <Parameter Type="Float" Name="mml_fSpecularReflectionMult" Default="0.5"/>
            <EffectParameter EffectName="da_distortion" EffectID="da_distortion"/>
          </Pass>
        </Technique>
      </Logic>
    </Logic>
  </Semantic>
  <Semantic Name="Default">
    <Logic Note="Default logic block">
      <LogicTest ShaderModel="3"/>
      <Technique Name="DefaultTechnique">
        <Pass Name="test" VertexShader="FontShader.vsh" PixelShader="FontShader.psh" State="FontState.sta"/>
      </Technique>
    </Logic>
  </Semantic>
</Material>
)";

} // namespace

TEST_CASE("a material definition's semantics, blocks, passes and parameters, in file order", "[mat]")
{
    const auto read = mat::Read(kCharacter);
    REQUIRE(read);
    CHECK(read->name == "Character");
    REQUIRE(read->sets.size() == 1);
    CHECK(read->sets[0].name == "Common Character Params");
    REQUIRE(read->sets[0].parameters.size() == 2);
    CHECK(read->sets[0].parameters[0].internal);
    CHECK(read->sets[0].parameters[0].usage == "ModelViewProjectionTrans");
    CHECK(read->sets[0].effects == std::vector<std::string>{"da_distortion"});

    REQUIRE(read->semantics.size() == 2);
    const mat::Semantic &tint = read->semantics[0];
    CHECK(tint.name == "LerpedTint");
    CHECK(tint.renderClass == "Character,Transparent");
    // The root block holds only blocks: it is none itself.
    REQUIRE(tint.blocks.size() == 2);
    CHECK(tint.blocks[0].note == "Shader Model 3");
    REQUIRE(tint.blocks[0].tests.size() == 1);
    CHECK(tint.blocks[0].tests[0].shaderModel == 3);
    CHECK(tint.blocks[0].tests[0].detail == "LOW");
    CHECK(tint.blocks[1].tests[0].shaderModel == 2);
    CHECK(tint.blocks[1].tests[0].detail.empty());

    // The comment's pass is not one.
    REQUIRE(tint.blocks[0].techniques.size() == 1);
    REQUIRE(tint.blocks[0].techniques[0].passes.size() == 1);
    const mat::Pass &pass = tint.blocks[0].techniques[0].passes[0];
    CHECK(pass.name == "Pass 1");
    CHECK(pass.vertexShader == "Skin_LOD0.vsh");
    CHECK(pass.pixelShader == "Skin_LOD0.psh");
    CHECK(pass.state == "Skin.sta");
    CHECK(pass.sets == std::vector<std::string>{"Common Character Params"});
    CHECK(pass.streams == std::vector<std::string>{"POSITION", "TEXCOORD0"});
    REQUIRE(pass.parameters.size() == 3);
    CHECK(pass.parameters[0].sampler == "smpDecal");
    CHECK(pass.parameters[0].stage == "0");
    CHECK(pass.parameters[2].size == "6");
    CHECK(!pass.parameters[2].defaultValue.has_value());
    CHECK(tint.blocks[1].techniques[0].passes[0].effects == std::vector<std::string>{"da_distortion"});

    // A pass closed in its own tag holds nothing and leaves what follows alone.
    const mat::Semantic *plain = read->SemanticNamed("Default");
    REQUIRE(plain != nullptr);
    REQUIRE(plain->blocks.size() == 1);
    CHECK(plain->blocks[0].note == "Default logic block");
    REQUIRE(plain->blocks[0].techniques[0].passes.size() == 1);
    CHECK(plain->blocks[0].techniques[0].passes[0].parameters.empty());
    CHECK(read->SemanticNamed("None") == nullptr);
}

TEST_CASE("a pass's parameter is its own, or its sets', and each block keeps its own default", "[mat]")
{
    const auto read = mat::Read(kCharacter);
    REQUIRE(read);
    const mat::Semantic &tint = read->semantics[0];
    const mat::Pass &sm3 = tint.blocks[0].techniques[0].passes[0];
    const mat::Pass &sm2 = tint.blocks[1].techniques[0].passes[0];
    CHECK(read->ParameterIn(sm3, "mml_fSpecularReflectionMult")->defaultValue == "1.0");
    CHECK(read->ParameterIn(sm2, "mml_fSpecularReflectionMult")->defaultValue == "0.5");
    const mat::Parameter *falloff = read->ParameterIn(sm3, "mml_vFalloffParams");
    REQUIRE(falloff != nullptr);
    CHECK(falloff->defaultValue == "0.65 10.0 1.5 1.5");
    CHECK(read->ParameterIn(sm2, "mml_vFalloffParams") == nullptr); // its pass refers to no set
    CHECK(read->ParameterIn(sm3, "mml_none") == nullptr);
}

TEST_CASE("text with no material is no definition", "[mat]")
{
    CHECK(!mat::Read("<MaterialObject Name=\"ARM_MASh\"></MaterialObject>"));
    CHECK(!mat::Read(""));
}

TEST_CASE("the game's definitions in game/materials, when there, read whole", "[mat][folder]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "materials";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/materials in this checkout (ghilana's extract of *.mat.xml writes them)");
    }
    std::size_t files = 0;
    std::size_t sets = 0;
    std::size_t semantics = 0;
    std::size_t passes = 0;
    std::size_t parameters = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (!entry.path().filename().string().ends_with(".mat.xml"))
        {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        INFO(entry.path().filename().string());
        const auto read = mat::Read(text);
        REQUIRE(read);
        CHECK(!read->name.empty());
        ++files;
        sets += read->sets.size();
        semantics += read->semantics.size();
        for (const mat::Semantic &semantic : read->semantics)
        {
            CHECK(!semantic.blocks.empty());
            for (const mat::Block &block : semantic.blocks)
            {
                for (const mat::Technique &technique : block.techniques)
                {
                    for (const mat::Pass &pass : technique.passes)
                    {
                        CHECK(!pass.pixelShader.empty());
                        ++passes;
                        parameters += pass.parameters.size();
                    }
                }
            }
        }
    }
    CHECK(files > 0);
    // Every definition the game has (DAOLib #153): the counts Python's XML parser read in them.
    if (files == 475)
    {
        CHECK(sets == 11);
        CHECK(semantics == 1495);
        CHECK(passes == 2554);
        CHECK(parameters == 26422);
    }
}
