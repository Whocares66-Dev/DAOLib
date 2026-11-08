#include "dao/core/MaterialDefinition.h"
#include "dao/core/XmlTags.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
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

// A material of one semantic, "S", holding `logic`.
std::string OneSemantic(std::string_view logic)
{
    return R"(<Material Name="M"><Semantic Name="S">)" + std::string(logic) + "</Semantic></Material>";
}

// A technique called `name` with one pass that is valid.
std::string Tech(std::string_view name)
{
    return R"(<Technique Name=")" + std::string(name) +
           R"("><Pass Name="P" VertexShader="a.vsh" PixelShader="a.psh" State="a.sta"/></Technique>)";
}

// A block with one test and the technique `name`.
std::string Leaf(std::string_view test, std::string_view name)
{
    return "<Logic><LogicTest " + std::string(test) + "/>" + Tech(name) + "</Logic>";
}

// A block with one pass, holding `parameters`, and no test.
std::string WithParameters(std::string_view parameters)
{
    return R"(<Logic><Technique Name="T"><Pass VertexShader="a.vsh" PixelShader="a.psh">)" + std::string(parameters) +
           "</Pass></Technique></Logic>";
}

// The name of the technique a material draws a semantic with, "-" for none.
std::string Chosen(const std::string &xml, int level, bool shaderModel3, std::string_view semantic = "S")
{
    const auto definition = mat::Read(xml);
    REQUIRE(definition);
    const mat::Technique *technique = mat::SelectTechnique(*definition, semantic, level, shaderModel3);
    return technique != nullptr ? technique->name : "-";
}

// The first pass of a one-semantic material, `pass` its text after any `sets` of the material.
mat::Pass PassOf(std::string_view pass, std::string_view sets = "")
{
    const auto definition =
        mat::Read(R"(<Material Name="M">)" + std::string(sets) + R"(<Semantic Name="S"><Logic><Technique Name="T">)" +
                  std::string(pass) + "</Technique></Logic></Semantic></Material>");
    REQUIRE(definition);
    REQUIRE(definition->semantics.size() == 1);
    REQUIRE(definition->semantics[0].logic);
    REQUIRE(definition->semantics[0].logic->technique);
    REQUIRE(!definition->semantics[0].logic->technique->passes.empty());
    return definition->semantics[0].logic->technique->passes[0];
}

std::vector<std::string> Names(const mat::Pass &pass)
{
    std::vector<std::string> names(pass.parameters.size());
    std::ranges::transform(pass.parameters, names.begin(), &mat::Parameter::name);
    return names;
}

// What a folder of definitions holds, and how many semantics a device draws at each detail level.
struct Tally
{
    std::size_t files = 0;
    std::size_t sets = 0;
    std::size_t semantics = 0;
    std::size_t techniques = 0;
    std::size_t passes = 0;
    std::size_t own = 0;          // the parameters passes wrote themselves
    std::size_t drawn[2][4] = {}; // [shader model 3][level]
};

// Counts the techniques of a block and the blocks inside it, and checks each pass has its pixel shader.
void CountBlock(const mat::Block &block, Tally &tally)
{
    if (block.technique)
    {
        ++tally.techniques;
        for (const mat::Pass &pass : block.technique->passes)
        {
            CHECK(!pass.pixelShader.empty());
            ++tally.passes;
            tally.own += static_cast<std::size_t>(std::ranges::count(pass.parameters, false, &mat::Parameter::fromSet));
        }
    }
    for (const mat::Block &child : block.children)
    {
        CountBlock(child, tally);
    }
}

void CountDefinition(const mat::Definition &definition, Tally &tally)
{
    ++tally.files;
    tally.sets += definition.sets.size();
    tally.semantics += definition.semantics.size();
    for (const mat::Semantic &semantic : definition.semantics)
    {
        INFO(semantic.name);
        REQUIRE(semantic.logic);
        CountBlock(*semantic.logic, tally);
        for (int level = 0; level < 4; ++level)
        {
            tally.drawn[0][level] += mat::SelectTechnique(definition, semantic.name, level, false) != nullptr ? 1 : 0;
            tally.drawn[1][level] += mat::SelectTechnique(definition, semantic.name, level, true) != nullptr ? 1 : 0;
        }
    }
}

} // namespace

TEST_CASE("a material definition's semantics, blocks, passes and parameters, in file order", "[mat]")
{
    const auto read = mat::Read(kCharacter);
    REQUIRE(read);
    CHECK(read->name == "Character");
    REQUIRE(read->sets.size() == 1);
    CHECK(read->sets[0].name == "Common Character Params");
    // The EffectParameter under the set is an element out of place: nothing of it is read.
    REQUIRE(read->sets[0].parameters.size() == 2);
    CHECK(read->sets[0].parameters[0].internal);
    CHECK(read->sets[0].parameters[0].usage == "ModelViewProjectionTrans");

    REQUIRE(read->semantics.size() == 2);
    const mat::Semantic &tint = read->semantics[0];
    CHECK(tint.name == "LerpedTint");
    CHECK(tint.renderClass == "Character,Transparent");
    // The root block holds only blocks: it has no tests and no technique.
    REQUIRE(tint.logic);
    CHECK(tint.logic->tests.empty());
    CHECK(!tint.logic->technique);
    REQUIRE(tint.logic->children.size() == 2);
    const mat::Block &sm3 = tint.logic->children[0];
    const mat::Block &sm2 = tint.logic->children[1];
    CHECK(sm3.note == "Shader Model 3");
    REQUIRE(sm3.tests.size() == 1);
    CHECK(sm3.tests[0].shaderModel == 3);
    CHECK(sm3.tests[0].detail == "LOW");
    CHECK(sm3.tests[0].flags == 0x11);
    CHECK(sm2.tests[0].shaderModel == 2);
    CHECK(sm2.tests[0].detail.empty());
    CHECK(sm2.tests[0].flags == 2);

    // The comment's pass is not one.
    REQUIRE(sm3.technique);
    REQUIRE(sm3.technique->passes.size() == 1);
    const mat::Pass &pass = sm3.technique->passes[0];
    CHECK(pass.name == "Pass 1");
    CHECK(pass.vertexShader == "Skin_LOD0.vsh");
    CHECK(pass.pixelShader == "Skin_LOD0.psh");
    CHECK(pass.state == "Skin.sta");
    CHECK(pass.sets == std::vector<std::string>{"Common Character Params"});
    CHECK(pass.streams == std::vector<std::string>{"POSITION", "TEXCOORD0"});
    // The set's plain parameter, then the pass's own, then the internal ones.
    CHECK(Names(pass) == std::vector<std::string>{"mml_vFalloffParams", "mml_tDiffuse", "mml_fSpecularReflectionMult",
                                                  "mml_vTintMaskColours", "aur_mWorldViewProj"});
    const mat::Parameter *diffuse = mat::Definition::ParameterIn(pass, "mml_tDiffuse");
    REQUIRE(diffuse != nullptr);
    CHECK(diffuse->sampler == "smpDecal");
    CHECK(diffuse->stage == "0");
    CHECK(!diffuse->fromSet);
    const mat::Parameter *tints = mat::Definition::ParameterIn(pass, "mml_vTintMaskColours");
    REQUIRE(tints != nullptr);
    CHECK(tints->size == "6");
    CHECK(!tints->defaultValue.has_value());
    CHECK(mat::Definition::ParameterIn(pass, "aur_mWorldViewProj")->fromSet);
    CHECK(mat::Definition::ParameterIn(pass, "mml_fSpecularReflectionMult")->defaultValue == "1.0");
    CHECK(mat::Definition::ParameterIn(pass, "mml_vFalloffParams")->defaultValue == "0.65 10.0 1.5 1.5");
    CHECK(mat::Definition::ParameterIn(pass, "mml_none") == nullptr);
    REQUIRE(sm2.technique);
    const mat::Pass &fallback = sm2.technique->passes[0];
    CHECK(fallback.effects == std::vector<std::string>{"da_distortion"});
    CHECK(mat::Definition::ParameterIn(fallback, "mml_fSpecularReflectionMult")->defaultValue == "0.5");
    CHECK(mat::Definition::ParameterIn(fallback, "mml_vFalloffParams") == nullptr); // it refers to no set

    // A pass closed in its own tag holds nothing and leaves what follows alone.
    const mat::Semantic *plain = read->SemanticNamed("Default");
    REQUIRE(plain != nullptr);
    REQUIRE(plain->logic);
    CHECK(plain->logic->note == "Default logic block");
    REQUIRE(plain->logic->technique);
    REQUIRE(plain->logic->technique->passes.size() == 1);
    CHECK(plain->logic->technique->passes[0].parameters.empty());
    CHECK(read->SemanticNamed("None") == nullptr);
    CHECK(read->SemanticNamed("default") == plain); // the engine compares names ignoring case
    CHECK(read->DefaultSemantic() == plain);
}

TEST_CASE("of two semantics differing in case the later is found, and of two the same the first", "[mat]")
{
    mat::Definition differing;
    differing.semantics.resize(3);
    differing.semantics[0].name = "Shadow";
    differing.semantics[1].name = "Other";
    differing.semantics[2].name = "SHADOW";
    CHECK(differing.SemanticNamed("shadow") == &differing.semantics[2]);
    CHECK(differing.SemanticNamed("Other") == &differing.semantics[1]);
    CHECK(differing.SemanticNamed("Missing") == nullptr);

    mat::Definition same;
    same.semantics.resize(3);
    same.semantics[0].name = "Shadow";
    same.semantics[1].name = "Other";
    same.semantics[2].name = "Shadow"; // the engine's AddSemantic drops this one
    CHECK(same.SemanticNamed("Shadow") == same.semantics.data());
    CHECK(same.SemanticNamed("SHADOW") == same.semantics.data());
}

TEST_CASE("the default semantic is the one named exactly Default, else the first", "[mat]")
{
    mat::Definition definition;
    CHECK(definition.DefaultSemantic() == nullptr);
    definition.semantics.resize(3);
    definition.semantics[0].name = "Shadow";
    definition.semantics[1].name = "default"; // AddSemantic compares with case: not the default
    definition.semantics[2].name = "Shadow";
    CHECK(definition.DefaultSemantic() == definition.semantics.data());
    definition.semantics[2].name = "Default";
    CHECK(definition.DefaultSemantic() == &definition.semantics[2]);
    definition.semantics.push_back(definition.semantics[2]); // a second Default is dropped
    CHECK(definition.DefaultSemantic() == &definition.semantics[2]);
}

TEST_CASE("a parameter set is found by name ignoring case, the first of a name", "[mat]")
{
    const auto read =
        mat::Read(R"(<Material Name="M"><ParameterSet Name="Common"><Parameter Type="Float" Name="a"/></ParameterSet>)"
                  R"(<ParameterSet Name="COMMON"><Parameter Type="Float" Name="b"/></ParameterSet></Material>)");
    REQUIRE(read);
    REQUIRE(read->SetNamed("common") != nullptr);
    CHECK(read->SetNamed("common")->parameters[0].name == "a");
    CHECK(read->SetNamed("other") == nullptr);
}

TEST_CASE("text with no material is no definition", "[mat]")
{
    CHECK(!mat::Read(R"(<MaterialObject Name="ARM_MASh"></MaterialObject>)"));
    CHECK(!mat::Read(""));
}

TEST_CASE("elements and attributes are matched ignoring case, and a name may be an ID", "[mat]")
{
    const auto read = mat::Read(R"(<MATERIAL ID="M"><semantic NAME="S"><logic note="n"><technique id="T">)"
                                R"(<pass name="P" vertexshader="a.vsh" PIXELSHADER="a.psh"/></technique></logic>)"
                                "</semantic></MATERIAL>");
    REQUIRE(read);
    CHECK(read->name == "M");
    REQUIRE(read->semantics.size() == 1);
    CHECK(read->semantics[0].name == "S");
    REQUIRE(read->semantics[0].logic);
    CHECK(read->semantics[0].logic->note == "n");
    REQUIRE(read->semantics[0].logic->technique);
    CHECK(read->semantics[0].logic->technique->name == "T");
    CHECK(read->semantics[0].logic->technique->passes[0].pixelShader == "a.psh");
}

TEST_CASE("every attribute of a tag is visited in the order written", "[mat]")
{
    std::vector<std::pair<std::string, std::string>> seen;
    const auto visit = [&](std::string_view name, const std::string &value) {
        seen.emplace_back(std::string(name), value);
    };
    dao::core::xml::ForEachAttribute(R"(Pass  A = "1" b='x &amp; y'  C=""/)", visit);
    CHECK(seen == std::vector<std::pair<std::string, std::string>>{{"A", "1"}, {"b", "x & y"}, {"C", ""}});
    seen.clear();
    dao::core::xml::ForEachAttribute(R"(Pass A="1" broken)", visit);
    CHECK(seen.size() == 1);
}

TEST_CASE("a test's word is built attribute by attribute, in the order written", "[mat]")
{
    const auto flags = [](std::string_view attributes) {
        const auto read = mat::Read(OneSemantic("<Logic><LogicTest " + std::string(attributes) + "/></Logic>"));
        REQUIRE(read);
        return read->semantics[0].logic->tests[0].flags;
    };
    CHECK(flags(R"(ShaderModel="3")") == 1);
    CHECK(flags(R"(ShaderModel="2")") == 2);
    CHECK(flags(R"(ShaderModel="4")") == 0);
    CHECK(flags(R"(Detail="HIGH")") == 4);
    CHECK(flags(R"(Detail="medium")") == 8);
    CHECK(flags(R"(Detail="LOW")") == 0x10);
    CHECK(flags(R"(Detail="ULTRA")") == 0);
    CHECK(flags(R"(ShaderModel="3" Detail="HIGH")") == 5);
    CHECK(flags(R"(Detail="HIGH" ShaderModel="3")") == 5);
    // Shader model 2 clears a detail before it and refuses one after it.
    CHECK(flags(R"(Detail="HIGH" ShaderModel="2")") == 2);
    CHECK(flags(R"(ShaderModel="2" Detail="HIGH")") == 2);
}

TEST_CASE("the tests of one block are or'ed into one word", "[mat]")
{
    const std::string xml =
        OneSemantic(R"(<Logic><LogicTest Detail="HIGH"/><LogicTest ShaderModel="2"/>)" + Tech("T") + "</Logic>");
    const auto read = mat::Read(xml);
    REQUIRE(read);
    CHECK(read->semantics[0].logic->tests[0].flags == 4);
    CHECK(read->semantics[0].logic->tests[1].flags == 2);
    // Word 6, shader model 2 and HIGH both asked: a shader model 2 device takes it at level 3 and not under.
    CHECK(Chosen(xml, 3, false) == "T");
    CHECK(Chosen(xml, 2, false) == "-");
    CHECK(Chosen(xml, 3, true) == "-");
}

TEST_CASE("the first block whose tests hold gives its technique", "[mat]")
{
    const std::string xml =
        OneSemantic("<Logic>" + Leaf(R"(ShaderModel="3" Detail="HIGH")", "sm3-high") +
                    Leaf(R"(ShaderModel="3" Detail="MEDIUM")", "sm3-medium") +
                    Leaf(R"(ShaderModel="3" Detail="LOW")", "sm3-low") + Leaf(R"(ShaderModel="3")", "sm3") +
                    Leaf(R"(ShaderModel="2")", "sm2") + Leaf(R"(Detail="LOW")", "low") + Leaf("", "any") + "</Logic>");
    // A shader model 3 device: the detail steps down as the level does.
    CHECK(Chosen(xml, 3, true) == "sm3-high");
    CHECK(Chosen(xml, 2, true) == "sm3-medium");
    CHECK(Chosen(xml, 1, true) == "sm3-low");
    // At level 0 shader model 3 is refused whatever the device has; shader model 2 is not asked of it.
    CHECK(Chosen(xml, 0, true) == "sm2");
    // A shader model 2 device takes the shader model 2 block at every level.
    CHECK(Chosen(xml, 3, false) == "sm2");
    CHECK(Chosen(xml, 1, false) == "sm2");
    CHECK(Chosen(xml, 0, false) == "sm2");
}

TEST_CASE("a detail alone admits the levels at and above it, and level 0 none", "[mat]")
{
    const auto at = [](std::string_view detail, int level) {
        return Chosen(OneSemantic("<Logic>" + Leaf(R"(Detail=")" + std::string(detail) + R"(")", "T") + "</Logic>"),
                      level, true);
    };
    CHECK(at("HIGH", 3) == "T");
    CHECK(at("HIGH", 2) == "-");
    CHECK(at("MEDIUM", 3) == "T");
    CHECK(at("MEDIUM", 2) == "T");
    CHECK(at("MEDIUM", 1) == "-");
    CHECK(at("LOW", 3) == "T");
    CHECK(at("LOW", 1) == "T");
    CHECK(at("LOW", 0) == "-");
    CHECK(at("LOW", 4) == "-"); // a level over 3 passes no detail test
}

TEST_CASE("a block's tests hold for the blocks inside it", "[mat]")
{
    const std::string xml =
        OneSemantic(R"(<Logic><Logic><LogicTest ShaderModel="3"/>)" + Leaf(R"(Detail="HIGH")", "inner") + "</Logic>" +
                    Leaf(R"(ShaderModel="2")", "other") + "</Logic>");
    CHECK(Chosen(xml, 3, true) == "inner");
    CHECK(Chosen(xml, 2, true) == "-"); // the outer holds, the inner is HIGH, and the other is for shader model 2
    CHECK(Chosen(xml, 3, false) == "other");
}

TEST_CASE("a block with a technique and blocks inside is invalid, and with it the semantic", "[mat]")
{
    const std::string xml = OneSemantic(R"(<Logic><LogicTest ShaderModel="3"/>)" + Tech("own") + "<Logic>" +
                                        Tech("inner") + "</Logic></Logic>");
    CHECK(Chosen(xml, 3, true) == "-");
}

TEST_CASE("a block's first technique and a semantic's first Logic are the ones kept", "[mat]")
{
    const std::string techniques = OneSemantic("<Logic>" + Tech("first") + Tech("second") + "</Logic>");
    const auto read = mat::Read(techniques);
    REQUIRE(read);
    CHECK(read->semantics[0].logic->technique->name == "first");
    CHECK(Chosen(techniques, 3, true) == "first");

    const std::string logics = OneSemantic("<Logic>" + Tech("first") + "</Logic><Logic>" + Tech("second") + "</Logic>");
    CHECK(Chosen(logics, 3, true) == "first");
    // What a second Logic holds does not count, valid or not.
    const std::string broken =
        OneSemantic("<Logic>" + Tech("first") + R"(</Logic><Logic><Technique Name="second"/></Logic>)");
    CHECK(Chosen(broken, 3, true) == "first");
}

TEST_CASE("an invalid technique anywhere in the tree makes the semantic draw nothing", "[mat]")
{
    const std::string broken = R"(<Logic><LogicTest ShaderModel="3"/><Technique Name="bad"><Pass Name="P" )"
                               R"(VertexShader="a.vsh" State="a.sta"/></Technique></Logic>)";
    // The broken block is never the one chosen on a shader model 2 device, and still the whole is invalid.
    CHECK(Chosen(OneSemantic("<Logic>" + broken + Leaf(R"(ShaderModel="2")", "good") + "</Logic>"), 3, false) == "-");
    CHECK(Chosen(OneSemantic("<Logic>" + Leaf(R"(ShaderModel="2")", "good") + "</Logic>"), 3, false) == "good");
    // No pass, an empty vertex shader, an empty pixel shader.
    CHECK(Chosen(OneSemantic(R"(<Logic><Technique Name="T"/></Logic>)"), 3, true) == "-");
    CHECK(Chosen(OneSemantic(R"(<Logic><Technique Name="T"><Pass PixelShader="a.psh"/></Technique></Logic>)"), 3,
                 true) == "-");
    CHECK(Chosen(OneSemantic(R"(<Logic><Technique Name="T"><Pass VertexShader="a.vsh"/></Technique></Logic>)"), 3,
                 true) == "-");
    // One bad pass of two.
    CHECK(Chosen(OneSemantic(R"(<Logic><Technique Name="T"><Pass VertexShader="a.vsh" PixelShader="a.psh"/>)"
                             R"(<Pass VertexShader="a.vsh"/></Technique></Logic>)"),
                 3, true) == "-");
}

TEST_CASE("a semantic with no name or no logic draws nothing, and a semantic is asked for by name", "[mat]")
{
    CHECK(Chosen("<Material><Semantic><Logic>" + Tech("T") + "</Logic></Semantic></Material>", 3, true, "") == "-");
    CHECK(Chosen(R"(<Material><Semantic Name="S"/></Material>)", 3, true) == "-");
    const std::string two = R"(<Material><Semantic Name="A"><Logic>)" + Tech("a") +
                            R"(</Logic></Semantic><Semantic Name="Default"><Logic>)" + Tech("d") +
                            "</Logic></Semantic></Material>";
    CHECK(Chosen(two, 3, true, "A") == "a");
    CHECK(Chosen(two, 3, true, "a") == "a");
    CHECK(Chosen(two, 3, true, "") == "d");
    CHECK(Chosen(two, 3, true, "None") == "-");
}

TEST_CASE("an element out of place is left out with everything under it", "[mat]")
{
    // A Pass outside a Technique, a Parameter outside a pass or set, a Semantic outside the Material, an
    // unknown element with a technique inside, and a Logic and a Pass inside a pass.
    const auto read =
        mat::Read(R"(<Material Name="M"><Parameter Type="Float" Name="loose"/>)"
                  R"(<Semantic Name="S"><Pass Name="outside" VertexShader="a.vsh" PixelShader="a.psh"/>)"
                  R"(<Logic><Semantic Name="inner"/><Foo><Technique Name="hidden"/></Foo>)" +
                  Tech("T") +
                  R"(<Technique Name="U"><Pass Name="P" VertexShader="a.vsh" PixelShader="a.psh">)"
                  R"(<Logic Note="inside a pass"/><Pass Name="nested"/></Pass></Technique></Logic></Semantic>)"
                  R"(<ParameterSet Name="S"><Semantic Name="inside a set"/></ParameterSet></Material>)");
    REQUIRE(read);
    REQUIRE(read->semantics.size() == 1);
    CHECK(read->sets.size() == 1);
    REQUIRE(read->semantics[0].logic);
    CHECK(read->semantics[0].logic->technique->name == "T");
    CHECK(read->semantics[0].logic->children.empty());
}

TEST_CASE("a parameter set under anything but the Material and an effect parameter under a set are ignored", "[mat]")
{
    const auto read = mat::Read(R"(<Material Name="M"><Semantic Name="S"><ParameterSet Name="inside">)"
                                R"(<Parameter Type="Float" Name="x"/></ParameterSet></Semantic>)"
                                R"(<ParameterSet Name="Set"><EffectParameter EffectName="e"/>)"
                                R"(<Parameter Type="Float" Name="y"/></ParameterSet></Material>)");
    REQUIRE(read);
    REQUIRE(read->sets.size() == 1);
    CHECK(read->sets[0].name == "Set");
    CHECK(read->sets[0].parameters.size() == 1);
    CHECK(read->SetNamed("inside") == nullptr);
}

TEST_CASE("a Meta element is skipped, and so is what comes before the Material", "[mat]")
{
    const auto read =
        mat::Read(R"(<Pass Name="early"/><Material Name="M"><Meta Name="ignored"/><Semantic Name="S"/></Material>)");
    REQUIRE(read);
    CHECK(read->name == "M");
    CHECK(read->semantics.size() == 1);
}

TEST_CASE("a pass with an Effect keeps the effect and its stages and nothing else", "[mat]")
{
    const mat::Pass pass =
        PassOf(R"(<Pass Name="fx" Effect="da_distortion" VertexShader="a.vsh" PixelShader="a.psh" State="a.sta">)"
               R"(<Parameter Type="Float" Name="p"/><RequiredStreamData Name="POSITION"/>)"
               R"(<Stage id="one"/><Stage id="two"/></Pass>)");
    CHECK(pass.effect == "da_distortion");
    CHECK(pass.stages == std::vector<std::string>{"one", "two"});
    CHECK(pass.vertexShader.empty());
    CHECK(pass.pixelShader.empty());
    CHECK(pass.state.empty());
    CHECK(pass.parameters.empty());
    CHECK(pass.streams.empty());
    // It is valid without shaders, so a technique of it is drawn.
    const std::string xml = OneSemantic(R"(<Logic><Technique Name="T"><Pass Effect="fx"/></Technique></Logic>)");
    CHECK(Chosen(xml, 3, true) == "T");
}

TEST_CASE("a set's reference makes copies, and an own parameter written after it replaces its copy", "[mat]")
{
    const std::string set = R"(<ParameterSet Name="Common"><Parameter Type="Float" Name="a" Default="1"/>)"
                            R"(<Parameter Type="Float" Name="b"/><Parameter Type="Float" Name="c"/>)"
                            R"(<InternalParameter Type="Matrix4x4f" Name="m" Usage="u"/></ParameterSet>)";
    const std::string open = R"(<Pass Name="P" VertexShader="a.vsh" PixelShader="a.psh">)";
    // The reference first: the own b replaces the copy, the last copy taking its place (RemoveAtSwap).
    const mat::Pass after =
        PassOf(open + R"(<ParameterSetRef Ref="common"/><Parameter Type="Float" Name="b" Default="5"/></Pass>)", set);
    CHECK(Names(after) == std::vector<std::string>{"a", "c", "b", "m"});
    CHECK(mat::Definition::ParameterIn(after, "b")->defaultValue == "5");
    CHECK(!mat::Definition::ParameterIn(after, "b")->fromSet);
    CHECK(mat::Definition::ParameterIn(after, "a")->fromSet);
    CHECK(mat::Definition::ParameterIn(after, "m")->internal);

    // The own parameter first: it is not in the pass's map, so the copy stays beside it.
    const mat::Pass before =
        PassOf(open + R"(<Parameter Type="Float" Name="b" Default="5"/><ParameterSetRef Ref="Common"/></Pass>)", set);
    CHECK(Names(before) == std::vector<std::string>{"b", "a", "b", "c", "m"});
    // Two of a name make the pass invalid, and with it the technique.
    const auto drawn = [&](std::string_view body) {
        return Chosen("<Material>" + set + R"(<Semantic Name="S"><Logic><Technique Name="T">)" + open +
                          std::string(body) + "</Pass></Technique></Logic></Semantic></Material>",
                      3, true);
    };
    CHECK(drawn(R"(<Parameter Type="Float" Name="b"/><ParameterSetRef Ref="Common"/>)") == "-");
    CHECK(drawn(R"(<ParameterSetRef Ref="Common"/><Parameter Type="Float" Name="b"/>)") == "T");
    // Two references: the second's copies are beside the first's, and make two of a name.
    CHECK(drawn(R"(<ParameterSetRef Ref="Common"/><ParameterSetRef Ref="Common"/>)") == "-");
}

TEST_CASE("a reference finds the first set of its name read so far", "[mat]")
{
    const std::string pass = R"(<Semantic Name="S"><Logic><Technique Name="T"><Pass VertexShader="a.vsh" )"
                             R"(PixelShader="a.psh"><ParameterSetRef Ref="Common"/></Pass></Technique></Logic>)"
                             "</Semantic>";
    const std::string first = R"(<ParameterSet Name="Common"><Parameter Type="Float" Name="one"/></ParameterSet>)";
    const std::string second = R"(<ParameterSet Name="COMMON"><Parameter Type="Float" Name="two"/></ParameterSet>)";
    const auto both = mat::Read("<Material>" + first + second + pass + "</Material>");
    REQUIRE(both);
    CHECK(Names(both->semantics[0].logic->technique->passes[0]) == std::vector<std::string>{"one"});
    // A set after the pass is not yet read when the reference is met.
    const auto late = mat::Read("<Material>" + pass + first + "</Material>");
    REQUIRE(late);
    CHECK(late->semantics[0].logic->technique->passes[0].parameters.empty());
    CHECK(late->semantics[0].logic->technique->passes[0].sets == std::vector<std::string>{"Common"});
    // A name no set has adds nothing.
    const auto none = mat::Read("<Material>" + pass + "</Material>");
    REQUIRE(none);
    CHECK(none->semantics[0].logic->technique->passes[0].parameters.empty());
}

TEST_CASE("a parameter of a Type the engine makes nothing of is not in the pass", "[mat]")
{
    const mat::Pass pass = PassOf(R"(<Pass VertexShader="a.vsh" PixelShader="a.psh"><Parameter Type="Colour" )"
                                  R"(Name="x"/><Parameter Type="float" Name="y"/><InternalParameter Type="Nope" )"
                                  R"(Name="z"/><InternalParameter Type="Color4f" Name="w"/></Pass>)");
    CHECK(Names(pass) == std::vector<std::string>{"y", "w"});
}

TEST_CASE("a parameter's kind, stage and count are what the engine makes of the Type, Stage and Size", "[mat]")
{
    mat::Parameter parameter;
    CHECK(parameter.KindOf() == mat::Kind::Unknown);
    CHECK(parameter.Count() == 1);
    CHECK(!parameter.StageNumber());
    CHECK(parameter.min == "-10000");
    CHECK(parameter.max == "10000");
    parameter.type = "texture";
    CHECK(parameter.KindOf() == mat::Kind::Texture);
    parameter.type = "Vector4f";
    CHECK(parameter.KindOf() == mat::Kind::Vector4);
    parameter.size = "4";
    CHECK(parameter.KindOf() == mat::Kind::Vector4Array); // the Size, not the Type, makes the array
    parameter.type = "Vector4fArray";
    parameter.size = "1";
    CHECK(parameter.KindOf() == mat::Kind::Vector4);
    parameter.type = "Matrix4x4f";
    CHECK(parameter.KindOf() == mat::Kind::Matrix);
    parameter.stage = "3";
    CHECK(parameter.StageNumber() == 3);
    parameter.stage = "65535"; // 0xFFFF is no stage at all
    CHECK(!parameter.StageNumber());
    parameter.stage = "65539"; // a word
    CHECK(parameter.StageNumber() == 3);
}

TEST_CASE("a Float is valid between its Min and Max, which the engine gives as -10000 and 10000", "[mat]")
{
    const auto valid = [](std::string_view attributes) {
        const mat::Pass pass =
            PassOf(R"(<Pass><Parameter Type="Float" Name="f" )" + std::string(attributes) + "/></Pass>");
        return pass.parameters[0].Valid();
    };
    CHECK(valid(""));
    CHECK(valid(R"(Default="5000")"));
    CHECK(valid(R"(Default="-10000")"));
    CHECK(!valid(R"(Default="10001")"));
    CHECK(!valid(R"(Default="2" Min="0" Max="1")"));
    CHECK(valid(R"(Default="1" Min="0" Max="1")"));
    CHECK(!valid(R"(Default="-1" Min="0" Max="1")"));
    CHECK(valid(R"(Default="0.f")")); // the leading number is read
    mat::Parameter nameless;
    nameless.type = "Float";
    CHECK(!nameless.Valid());
    // A pass with an invalid Float is invalid.
    CHECK(Chosen(OneSemantic(WithParameters(R"(<Parameter Type="Float" Name="f" Default="2" Min="0" Max="1"/>)")), 3,
                 true) == "-");
}

TEST_CASE("two textures on one stage make a pass invalid, a texture with none does not", "[mat]")
{
    const auto chosen = [](std::string_view first, std::string_view second) {
        return Chosen(
            OneSemantic(WithParameters(R"(<Parameter Type="Texture" Name="a" )" + std::string(first) +
                                       R"(/><Parameter Type="Texture" Name="b" )" + std::string(second) + "/>")),
            3, true);
    };
    CHECK(chosen(R"(Stage="0")", R"(Stage="0")") == "-");
    CHECK(chosen(R"(Stage="0")", R"(Stage="1")") == "T");
    CHECK(chosen(R"(SamplerName="x")", R"(SamplerName="x")") == "T");
    CHECK(chosen(R"(Stage="65535")", R"(Stage="65535")") == "T");
}

TEST_CASE("a Float's value, a vector's groups and a matrix are parsed as the engine's streams do", "[mat]")
{
    mat::Parameter parameter;
    parameter.type = "Float";
    CHECK(parameter.FloatValue() == 0.0F); // no Default: 0, not the 1 of the constructor
    parameter.defaultValue = "2.5 more";
    CHECK(parameter.FloatValue() == 2.5F);
    parameter.defaultValue = "-1";
    CHECK(parameter.FloatValue() == -1.0F);
    parameter.defaultValue = "x";
    CHECK(parameter.FloatValue() == 0.0F);

    using mat::Vector4;
    parameter.type = "Vector4fArray";
    parameter.defaultValue = std::nullopt;
    CHECK(parameter.Vector4Groups().empty());
    parameter.defaultValue = "";
    CHECK(parameter.Vector4Groups().empty());
    parameter.defaultValue = "1 2 3 4";
    CHECK(parameter.Vector4Groups() == std::vector<Vector4>{{1, 2, 3, 4}});
    parameter.defaultValue = "1 2 3 4 5 6 7 8";
    CHECK(parameter.Vector4Groups() == std::vector<Vector4>{{1, 2, 3, 4}, {5, 6, 7, 8}});
    parameter.defaultValue = "1 2 3";
    CHECK(parameter.Vector4Groups() == std::vector<Vector4>{{1, 2, 3, 0}});
    parameter.defaultValue = "1 2 3 4 5";
    CHECK(parameter.Vector4Groups() == std::vector<Vector4>{{1, 2, 3, 4}, {5, 0, 0, 0}});
    parameter.defaultValue = "1 2 3 4 "; // the white space after the last number is one more group
    CHECK(parameter.Vector4Groups() == std::vector<Vector4>{{1, 2, 3, 4}, {0, 0, 0, 0}});
    parameter.defaultValue = "abc";
    CHECK(parameter.Vector4Groups() == std::vector<Vector4>{{0, 0, 0, 0}});
    parameter.defaultValue = "1.5,2";
    CHECK(parameter.Vector4Groups() == std::vector<Vector4>{{1.5F, 0, 0, 0}});
}

TEST_CASE("a parameter with no override holds the Default as the engine fills it", "[mat]")
{
    using mat::Vector4;
    mat::Parameter vector;
    vector.type = "Vector4f";
    CHECK(vector.Vector4Value() == Vector4{0, 0, 0, 0});
    vector.defaultValue = "1 2 3 4";
    CHECK(vector.Vector4Value() == Vector4{1, 2, 3, 4});
    vector.defaultValue = "1 2 3 4 5 6 7 8"; // more than one group: zeros
    CHECK(vector.Vector4Value() == Vector4{0, 0, 0, 0});

    mat::Parameter array;
    array.type = "Vector4fArray";
    array.size = "2";
    CHECK(array.Vector4ArrayValue() == std::vector<Vector4>{{0, 0, 0, 0}, {0, 0, 0, 0}});
    array.defaultValue = "1 2 3 4"; // fewer than the Size: the Default is lost
    CHECK(array.Vector4ArrayValue() == std::vector<Vector4>{{0, 0, 0, 0}, {0, 0, 0, 0}});
    array.defaultValue = "1 2 3 4 5 6 7 8 9 10 11 12"; // more: cut to the Size
    CHECK(array.Vector4ArrayValue() == std::vector<Vector4>{{1, 2, 3, 4}, {5, 6, 7, 8}});

    mat::Parameter matrix;
    matrix.type = "Matrix4x4f";
    CHECK(!matrix.MatrixDefault());
    CHECK(matrix.MatrixValue() == mat::Matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
    matrix.defaultValue = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16";
    REQUIRE(matrix.MatrixDefault());
    CHECK(matrix.MatrixValue() == mat::Matrix{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
    matrix.defaultValue = "";
    CHECK(!matrix.MatrixDefault());
}

TEST_CASE("the game's definitions in game/materials, when there, read whole", "[mat][folder]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "materials";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/materials in this checkout (ghilana's extract of *.mat.xml writes them)");
    }
    Tally tally;
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
        CountDefinition(*read, tally);
    }
    CHECK(tally.files > 0);
    // Every definition the game has (DAOLib #153, #298): the counts Python's XML parser read in them, with the
    // parameters each pass wrote itself (the sets' copies are more); a shader model 2 device, and any device at
    // level 0, draws the 1457 semantics that have a shader model 2 block, and a shader model 3 device all 1495.
    if (tally.files == 475)
    {
        CHECK(tally.sets == 11);
        CHECK(tally.semantics == 1495);
        CHECK(tally.techniques == 2518);
        CHECK(tally.passes == 2554);
        CHECK(tally.own == 26422);
        for (int level = 0; level < 4; ++level)
        {
            INFO("level " << level);
            CHECK(tally.drawn[0][level] == 1457);
            CHECK(tally.drawn[1][level] == (level == 0 ? 1457U : 1495U));
        }
    }
}

TEST_CASE("a technique that does not load is passed over for the next block", "[mat]")
{
    const auto definition = mat::Read(
        OneSemantic("<Logic>" + Leaf(R"(ShaderModel="3")", "A") + Leaf(R"(ShaderModel="3")", "B") + "</Logic>"));
    REQUIRE(definition);
    const auto loads = [](const mat::Technique &technique) { return technique.name != "A"; };
    const mat::Technique *chosen = mat::SelectTechnique(*definition, "S", 3, true, loads);
    REQUIRE(chosen != nullptr);
    CHECK(chosen->name == "B");
    CHECK(mat::SelectTechnique(*definition, "S", 3, true)->name == "A"); // with no callback every one loads
}

TEST_CASE("a material is valid where one of its semantics is", "[mat]")
{
    const std::string semantics =
        R"(<Semantic Name="Broken"/><Semantic Name="Good">)" + Leaf(R"(ShaderModel="3")", "T") + "</Semantic>";
    const auto definition = mat::Read(R"(<Material Name="M">)" + semantics + "</Material>");
    REQUIRE(definition);
    const auto states = mat::Semantics(*definition, 3, true);
    REQUIRE(states.size() == 2);
    CHECK_FALSE(states[0].valid); // no logic
    CHECK(states[0].technique == nullptr);
    CHECK(states[1].valid);
    CHECK(mat::Valid(*definition, 3, true));
    const auto none = mat::Read(R"(<Material Name="M"><Semantic Name="Broken"/></Material>)");
    REQUIRE(none);
    CHECK_FALSE(mat::Valid(*none, 3, true));
}

TEST_CASE("an object's defaults are the last semantic's, pass's and parameter's of each name", "[mat]")
{
    const auto pass = [](std::string_view value) {
        return R"(<Logic><Technique Name="T"><Pass VertexShader="a.vsh" PixelShader="a.psh" State="a.sta">)"
               R"(<Parameter Type="Float" Name="x" Default=")" +
               std::string(value) + R"("/></Pass></Technique></Logic>)";
    };
    const auto definition = mat::Read(R"(<Material Name="M"><Semantic Name="A">)" + pass("1") +
                                      R"(</Semantic><Semantic Name="B">)" + pass("2") + "</Semantic></Material>");
    REQUIRE(definition);
    const auto defaults = mat::Defaults(*definition, 3, true);
    REQUIRE(defaults.size() == 1);
    CHECK(defaults[0]->name == "x");
    CHECK(defaults[0]->FloatValue() == 2.0F);
}
