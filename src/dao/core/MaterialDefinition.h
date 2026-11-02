// A material definition (.mat.xml) out of the game's files, over its text (DAOLib #153): its
// semantics, each a tree of logic blocks that picks a technique by shader model and detail, the
// passes of each technique with their shaders and render state, and the parameters of each pass
// with their defaults. A material object (dao/core/MaterialObject.h) names its definition and a
// semantic and lists only the parameters it overrides; the rest are the definition's. For drawing
// a material as the game does (DAOParagonEngine) and for tools that change materials
// (DAOElgaraShaders).
//
// The reader keeps what the engine's builder keeps (dev/MATERIALS.md, DAOLib #298): an element
// out of place, with everything under it, is left out; a semantic has one logic tree and a block
// one technique, the first; a pass's parameters are the ones it has after its references to
// parameter sets are made. SelectTechnique says which technique the engine draws with.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::mat
{

using Vector4 = std::array<float, 4>;
using Matrix = std::array<float, 16>;

// What a parameter holds, as the engine makes it from the Type and the Size.
enum class Kind
{
    Unknown, // a Type the engine makes nothing of; the reader leaves such a parameter out of a pass
    Texture,
    Float,
    Vector4,
    Vector4Array, // a Vector4f or a Vector4fArray with a Size over 1
    Matrix,
};

// A parameter of a pass or a set, as written.
struct Parameter
{
    bool internal = false;                   // an InternalParameter, which the engine fills: a matrix, a shadow map
    bool fromSet = false;                    // a copy of one a ParameterSetRef took from its set
    std::string type;                        // "Texture", "Float", "Vector4f", "Vector4fArray", "Matrix4x4f", ...
    std::string name;                        // "mml_vFalloffParams"
    std::string sampler;                     // a texture's SamplerName; empty where none
    std::optional<std::string> defaultValue; // "0.65 10.0 1.5 1.5", "Default_Missing"
    std::string usage;                       // an internal one's: "ModelViewProjectionTrans"
    std::string stage;                       // a texture's sampler stage, where given
    std::string size;                        // an array's, where given
    std::string min = "-10000";              // a Float's lowest value, the engine's when not written
    std::string max = "10000";               // a Float's highest

    // Returns what the Type and Size make of it: Vector4f is an array when its Size is over 1.
    // Meant for a plain parameter; an internal one's Color4f and InternalDataReference are Unknown.
    [[nodiscard]] Kind KindOf() const;
    // Returns the sampler stage if one is written and is not 0xFFFF (unassigned); the engine then
    // finds a texture's stage by its SamplerName in the pixel shader.
    [[nodiscard]] std::optional<int> StageNumber() const;
    // Returns the Size, 1 where not written.
    [[nodiscard]] int Count() const;
    // Returns whether the engine takes the parameter as usable: it has a name, and a Float lies
    // in [Min, Max]. A pass with one that is not is invalid.
    [[nodiscard]] bool Valid() const;
    // Returns a Float's Default as a number, 0 where it has none.
    [[nodiscard]] float FloatValue() const;
    // Returns the groups of four numbers of a Default, one more all zero for white space after the last
    // number, and a group cut short with zeros after it.
    [[nodiscard]] std::vector<Vector4> Vector4Groups() const;
    // Returns a Matrix4x4f's Default, 16 numbers in file order; nothing where it has none.
    [[nodiscard]] std::optional<Matrix> MatrixDefault() const;
    // Returns the value a Vector4f has with no override: the Default's one group, else zeros.
    [[nodiscard]] Vector4 Vector4Value() const;
    // Returns the value a Vector4fArray has with no override: Count() groups of the Default, or Count()
    // zeros where it has fewer.
    [[nodiscard]] std::vector<Vector4> Vector4ArrayValue() const;
    // Returns the value a Matrix4x4f has with no override: its Default, else the identity.
    [[nodiscard]] Matrix MatrixValue() const;
};

struct Pass
{
    std::string name;
    std::string vertexShader; // "Skin_LOD0.vsh"
    std::string pixelShader;
    std::string state; // the render state's file, "Skin.sta"
    // The parameters the pass has: its own, and the sets' it refers to, as they stand after the last
    // is made (a set's copy is replaced by an own parameter written after the reference). The plain
    // ones come first, then the internal.
    std::vector<Parameter> parameters;
    std::vector<std::string> sets;    // the parameter sets it refers to, by name
    std::vector<std::string> streams; // the vertex data it requires: "POSITION", "TEXCOORD0"
    std::vector<std::string> effects; // the frame effects whose parameters it takes: "da_distortion"
    // A pass with an Effect runs that frame-buffer effect's stages and has no shaders, state,
    // parameters or streams; they are not kept. It runs every stage of the effect, whichever it lists.
    std::string effect;
    std::vector<std::string> stages; // the Stage ids it lists
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
    std::string detail;      // "HIGH", "LOW", "MEDIUM"; empty for any
    std::uint32_t flags = 0; // the engine's word: 1 shader model 3, 2 shader model 2, 4 HIGH, 8 MEDIUM, 0x10 LOW
};

// A logic block: tests that hold for it and the blocks inside it, or the technique it draws with.
struct Block
{
    std::string note; // "Shader Model 3"
    std::vector<Test> tests;
    std::optional<Technique> technique; // the first one; a block with one has no blocks inside it
    std::vector<Block> children;
};

struct Semantic
{
    std::string name;           // "LerpedTint"
    std::string renderClass;    // "Character,Transparent"; empty where none
    std::optional<Block> logic; // the first Logic of the semantic, the root of its tree
};

struct ParameterSet
{
    std::string name; // "Common Character Params"
    std::vector<Parameter> parameters;
};

// A material definition as its file says it, names as written, everything in file order.
struct Definition
{
    std::string name; // the Material's: "Character"
    std::vector<ParameterSet> sets;
    std::vector<Semantic> semantics;

    // Returns the semantic of a name, matched ignoring case, or nullptr, as the engine finds it: of two
    // with exactly the same name the first counts, and of two differing in case the later.
    [[nodiscard]] const Semantic *SemanticNamed(std::string_view wanted) const;
    // Returns the semantic the engine takes where none is named: the one named exactly "Default", else the first.
    [[nodiscard]] const Semantic *DefaultSemantic() const;
    // Returns the parameter set of a name, matched ignoring case, the first of that name; or nullptr.
    [[nodiscard]] const ParameterSet *SetNamed(std::string_view wanted) const;
    // Returns a parameter of a pass by name, or nullptr; the pass's own and its sets' are in it.
    [[nodiscard]] static const Parameter *ParameterIn(const Pass &pass, std::string_view wanted);
};

// Reads a material definition from its file's text.
// @return nullopt where the text has no Material element
std::optional<Definition> Read(std::string_view text);

// Returns the technique the engine draws a semantic with, or nullptr where it draws nothing: the first
// block, in file order and outermost first, whose tests hold and that has a technique. It draws nothing
// if the semantic has no name or no logic, any block in its tree has
// both a technique and blocks inside, any technique has no pass, or any pass is invalid (a shader
// missing, a parameter not valid, two of a name, two textures on one stage). The files a pass names
// are not looked at: a caller that finds a shader or a state missing treats the technique as the
// engine does, as invalid, and does not go on to the next block.
// @param semantic the semantic's name; empty for the default
// @param detailLevel the graphics detail level: 0 lowest, 3 highest
// @param shaderModel3 whether the device has shader model 3; otherwise it is taken to have shader model 2
const Technique *SelectTechnique(const Definition &definition, std::string_view semantic, int detailLevel,
                                 bool shaderModel3);

} // namespace dao::core::mat
