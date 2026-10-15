// A material object (.mao) out of the game's files, over its text (DAOLib #128): the material
// definition it is an instance of, its textures and parameters, and the material project it was
// exported from. For tools that sort or change materials (DAOElgaraShaders' materials table).
//
// Read 2026-10-15 (ghilana's text of arm_mash.mao and Elgara's reading of 2,632 materials): an
// XML file the toolset's Material Editor writes, a comment `<!-- dc:source=file:///<path> -->`
// before one MaterialObject with a Name; inside, a Material with the definition's Name
// ("Character.mat"), a DefaultSemantic with a Name, Textures each with a Name (the slot) and a
// ResName, and parameters each an element named for its type (Float, Vector4f, Int) with a Name
// and a value as text. A slot or parameter may come twice, so all are kept in file order.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::mao
{

// A texture in a slot: "mml_tDiffuse" and "pn_arm_masi_0d.dds", as written.
struct Texture
{
    std::string slot;
    std::string resource;
};

// A parameter: its element's name for its type ("Float", "Vector4f", "Int"), its name, and its
// value as written ("0.00 1.00 1.50 1.50").
struct Parameter
{
    std::string type;
    std::string name;
    std::string value;
};

// A material object's file as it says it, names as written.
struct MaterialObject
{
    std::string name;                  // the MaterialObject's
    std::string material;              // the definition's: "Character.mat"
    std::string defaultSemantic;       // "EmissiveNoTint"; empty where there is none
    std::vector<Texture> textures;     // in file order
    std::vector<Parameter> parameters; // in file order
    // The exporter's dc:source path after "file:///", to its comment's end less the spaces before
    // it; empty where there is none.
    std::string source;
    // Where the last "</MaterialObject>" starts in the text, to put elements before it.
    std::size_t end = 0;

    // Returns the first texture in a slot, or nullptr.
    [[nodiscard]] const Texture *TextureIn(std::string_view slot) const;
    // Returns the first parameter of a name, or nullptr.
    [[nodiscard]] const Parameter *ParameterNamed(std::string_view wanted) const;
};

// Reads a material object from its file's text.
// @return nullopt where the text has no MaterialObject element and its close
std::optional<MaterialObject> Read(std::string_view text);

} // namespace dao::core::mao
