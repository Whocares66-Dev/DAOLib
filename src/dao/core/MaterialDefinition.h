// A material definition (.mat.xml) out of the game's files, over its text (DAOLib #153): its
// semantics, each a choice of passes by shader model and detail, the shaders and render state of
// each pass, and its parameters with their defaults. A material object (dao/core/MaterialObject.h)
// names its definition and a semantic and lists only the parameters it overrides; the rest are
// the definition's. For drawing a material as the game does (DAOParagonEngine) and for tools that
// change materials (DAOElgaraShaders).
//
// Read 2026-10-17 over all 475 in packages/core/data/materials.erf (character.mat.xml,
// static.mat.xml and the rest): a Material with a Name; a few ParameterSets (in 11 files), each a
// Name and parameters; then Semantics, each a Name and in two of three a RenderClass
// ("Character,Transparent"). A semantic holds Logic blocks, each with a Note, one level deep or
// two: a block that draws has LogicTests (ShaderModel 2 or 3; 14 tests also a Detail, LOW or MEDIUM)
// and a Technique of Passes (Name, VertexShader, PixelShader, State). A pass holds Parameters and
// InternalParameters (Type, Name, SamplerName, Default, Usage, Stage, Size), ParameterSetRefs by
// name, RequiredStreamData (a vertex stream's name) and EffectParameters. A parameter is
// repeated in every pass, and its default is not one per file: 797 times it differs between the
// blocks of one semantic (shader model 2's against 3's), so the tree is kept whole. Which block
// the engine takes is not read here.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::mat
{

// A parameter of a pass or a set, as written.
struct Parameter
{
    bool internal = false;                   // an InternalParameter, which the engine fills: a matrix, a shadow map
    std::string type;                        // "Texture", "Float", "Vector4f", "Vector4fArray", "Matrix4x4f", ...
    std::string name;                        // "mml_vFalloffParams"
    std::string sampler;                     // a texture's SamplerName; empty where none
    std::optional<std::string> defaultValue; // "0.65 10.0 1.5 1.5", "Default_Missing"
    std::string usage;                       // an internal one's: "ModelViewProjectionTrans"
    std::string stage;                       // a texture's sampler stage, where given
    std::string size;                        // an array's, where given
};

struct Pass
{
    std::string name;
    std::string vertexShader; // "Skin_LOD0.vsh"
    std::string pixelShader;
    std::string state; // the render state's file, "Skin.sta"
    std::vector<Parameter> parameters;
    std::vector<std::string> sets;    // the parameter sets it refers to, by name
    std::vector<std::string> streams; // the vertex data it requires: "POSITION", "TEXCOORD0"
    std::vector<std::string> effects; // the frame effects whose parameters it takes: "da_distortion"
};

struct Technique
{
    std::string name;
    std::vector<Pass> passes;
};

// When a block is taken: a shader model, and a detail where one is named.
struct Test
{
    int shaderModel = 0;
    std::string detail; // "LOW", "MEDIUM"; empty for any
};

// A logic block that draws: its tests and what it draws with.
struct Block
{
    std::string note; // "Shader Model 3"
    std::vector<Test> tests;
    std::vector<Technique> techniques;
};

struct Semantic
{
    std::string name;        // "LerpedTint"
    std::string renderClass; // "Character,Transparent"; empty where none
    std::vector<Block> blocks;
};

struct ParameterSet
{
    std::string name; // "Common Character Params"
    std::vector<Parameter> parameters;
    std::vector<std::string> effects;
};

// A material definition as its file says it, names as written, everything in file order.
struct Definition
{
    std::string name; // the Material's: "Character"
    std::vector<ParameterSet> sets;
    std::vector<Semantic> semantics;

    // Returns the semantic of a name, or nullptr.
    [[nodiscard]] const Semantic *SemanticNamed(std::string_view wanted) const;
    // Returns the parameter set of a name, or nullptr.
    [[nodiscard]] const ParameterSet *SetNamed(std::string_view wanted) const;
    // Returns a parameter as a pass has it: its own of that name, or else the first of its sets'
    // in the order it refers to them; nullptr if neither has it.
    [[nodiscard]] const Parameter *ParameterIn(const Pass &pass, std::string_view wanted) const;
};

// Reads a material definition from its file's text.
// @return nullopt where the text has no Material element
std::optional<Definition> Read(std::string_view text);

} // namespace dao::core::mat
