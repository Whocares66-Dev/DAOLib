// A head's morph (.mor) out of the game's files, over bytes (DAOLib #121):
// the head models it is made for, and its nodes, a face bone's offset or a
// mesh node's every vertex placed again. For a tool that changes a head's
// meshes and must know which morphs lean on them (DAOEluvianMCP #132).
//
// Read 2026-10-14 (GFF4 "MORP", tools/gff4.py on hm_arl210cr_chamberlain.mor):
// its name; MorphParts, the head models' names, some empty (a slot with no
// part); MorphTintFileNames; MorphNodes, each a name and a position, and a
// mesh node's (FaceM1, EyesM1, LashesM1) MeshStreamVertexData, four floats a
// vertex, as many as its chunk has and in its order: the morph puts each
// vertex of the part's chunk at its place, by index (DAOLib #119's notes).
// A mesh node's MorphTextureParam, MorphVector4fParam and MorphFloatParam are
// its material's parameters by name, in place of the .mao's: in
// hm_genfl_alistair.mor, FaceM1's diffuse, normal and age maps and its
// mml_vTintMaskColours, ten Vector4s laid out as a .tnt's are
// (DAOLib #158).
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/RayCast.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace dao::core::morph
{

// A texture a mesh node's material takes in place of its own, by the parameter's name.
struct TextureParameter
{
    std::wstring name;    // "mml_tDiffuse"
    std::wstring texture; // "uh_hed_masa_0d", with no extension
};

// Values a mesh node's material takes in place of its own: "mml_vTintMaskColours", ten of them.
struct VectorParameter
{
    std::wstring name;
    std::vector<std::array<float, 4>> values;
};

struct FloatParameter
{
    std::wstring name;
    float value = 0;
};

struct Node
{
    std::wstring name; // a face bone's, or a mesh node's of a part
    Point position{};  // a face bone's offset
    // A mesh node's vertices placed again, in its chunk's order, each x, y, z, w; empty for a bone.
    std::vector<std::array<float, 4>> places;
    // A mesh node's material parameters, in file order; none for a bone.
    std::vector<TextureParameter> textures;
    std::vector<VectorParameter> vectors;
    std::vector<FloatParameter> floats;
};

struct Morph
{
    std::wstring name;
    std::vector<std::wstring> parts; // the head models it is made for, as named, "" for an empty slot
    std::vector<std::wstring> tints; // its tint files
    std::vector<Node> nodes;
};

// Returns a morph file's parts and nodes, or nothing for a file that is no
// morph. A node whose places run out of the file has none.
std::optional<Morph> MorphOf(const gff4::File &file);

} // namespace dao::core::morph
