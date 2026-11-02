// A material object (.mao) out of the game's files, over its text (DAOLib #128, #297): the material
// definition it is an instance of, its textures and parameters, and the material project it was
// exported from. For tools that sort or change materials (DAOElgaraShaders' materials table).
//
// Read 2026-10-15 (ghilana's text of arm_mash.mao and Elgara's reading of 2,632 materials): an
// XML file the toolset's Material Editor writes, a comment `<!-- dc:source=file:///<path> -->`
// before one MaterialObject with a Name; inside, a Material with the definition's Name
// ("Character.mat"), a DefaultSemantic with a Name, Textures each with a Name (the slot) and a
// ResName, and parameters each an element named for its type with a Name and a value as text.
// A slot or parameter may come twice, so all are kept in file order.
//
// The engine's reader (Graphics::MaterialObjectBuilderXML2, 0x00507FE0..0x00508C07, render-19,
// 2026-11-01) files nine kinds of element, each at its closing tag, from the attributes of its
// opening one: Texture, Vector4f, Vector4fArray, Matrix4x4f, Float, SoundType, Material, Override
// and DefaultSemantic. This keeps all nine, each value in its own type. Any other element (the
// `Int` of arm_mash.mao among them) the engine files nothing for, and neither does this.
//
// The .mat's defaults are not applied here. The engine applies them once, when the finished object
// is taken (TakeMaterialObject, 0x00507700, ApplyMaterialDefaults 0x00502BB0): a parameter the .mao
// leaves out takes the .mat's default for it (dao/core/MaterialDefinition.h), and the
// DefaultSemantic the .mao names beats the .mat's own when the material knows it. A caller that
// wants the object as the game draws it applies them itself over what this holds.
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace dao::core::mao
{

// A texture in a slot: "mml_tDiffuse" and "pn_arm_masi_0d.dds", as written.
struct Texture
{
    std::string slot;
    std::string resource;
    // RequiresID="true": the engine files it as a texture request that can be substituted
    // (AddTextureRequest, 0x00500DF0: a resource "lm.dds" becomes Default_Black), not as a plain one
    // (SetTextureParameterByName, 0x00502840).
    bool requiresId = false;
};

// A Vector4f's four floats.
using Vector4 = std::array<float, 4>;

// A Matrix4x4f's sixteen floats, in the order written.
using Matrix4x4 = std::array<float, 16>;

// A SoundType's value, an integer, as against a Float's.
struct SoundType
{
    int value = 0;
};

// A Vector4fArray's vectors, in the order written.
using Vector4Array = std::vector<Vector4>;

// A parameter: its name and its value, whose type is the element it was written as. The variant
// holds a float for a Float, a Vector4 for a Vector4f, a Vector4Array for a Vector4fArray, a
// Matrix4x4 for a Matrix4x4f and a SoundType for a SoundType.
struct Parameter
{
    std::string name;
    std::variant<float, Vector4, Vector4Array, Matrix4x4, SoundType> value;

    // Returns the value as T, or nullptr where it is another type.
    template <class T> [[nodiscard]] const T *Get() const
    {
        return std::get_if<T>(&value);
    }
};

// A material object's file as it says it, names as written.
struct MaterialObject
{
    std::string name;                  // the first MaterialObject's
    std::string material;              // the Material's, the definition's: "Character.mat"
    std::string defaultSemantic;       // "EmissiveNoTint"; empty where there is none
    std::string overrideName;          // the Override's, the engine's object+0x38; empty where there is none
    std::vector<Texture> textures;     // in file order
    std::vector<Parameter> parameters; // Float, Vector4f, Vector4fArray, Matrix4x4f and SoundType, in file order
    // The exporter's dc:source path after "file:///", to its comment's end less the spaces before
    // it; empty where there is none.
    std::string source;
    // Where the last "</MaterialObject>" starts in the text, to put elements before it.
    std::size_t end = 0;

    // Returns the first texture in a slot, or nullptr.
    [[nodiscard]] const Texture *TextureIn(std::string_view slot) const;
    // Returns the first parameter of a name, of whatever type, or nullptr.
    [[nodiscard]] const Parameter *ParameterNamed(std::string_view wanted) const;
    // Returns the first parameter of a name whose value is a T, or nullptr: the engine keeps a
    // Float and a Vector4f of one name apart.
    template <class T> [[nodiscard]] const T *Find(std::string_view wanted) const
    {
        for (const Parameter &parameter : parameters)
        {
            if (parameter.name == wanted)
            {
                if (const T *value = parameter.Get<T>())
                {
                    return value;
                }
            }
        }
        return nullptr;
    }
};

// Reads a material object from its file's text.
// @return nullopt where the text has no MaterialObject element and its close
std::optional<MaterialObject> Read(std::string_view text);

} // namespace dao::core::mao
