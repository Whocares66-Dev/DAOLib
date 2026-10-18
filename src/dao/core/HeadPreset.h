// A head preset (.mop) out of the game's files, over bytes, and the head it
// makes as a morph (DAOLib #171): what the appearance scene offers as a race
// and gender's faces (chargenmorphcfg.xml's heads, dao/core/MorphConfig.h).
// A preset is a recipe for a head, not a morph: no vertices and no bones,
// only the weights of morph targets over the head's base models, material
// values, and the parts and tint files a morph (dao/core/Morph.h) also
// names. ComposeHead blends it into a morph, drawn as any .mor is.
//
//   const auto preset = dao::core::preset::PresetOf(file);
//   // preset->parts[0] == L"HM_UHM_BASa_0", preset->tints[0] == L"t1_skn_001"
//   const auto head = dao::core::preset::ComposeHead(*preset, meshOf);
//
// Read 2026-10-18 over all 84 the game takes (face.erf, and chargen.rim's
// copy of each race and gender's p01, which it takes first), with
// lib-agent-2's reading of the same day: GFF4 "MOP V1.0", one layout in all
// (dev/gff4_fields.csv 23000, 23001 and 23008 to 23022). Each has six parts,
// as a morph's are ordered (face, eyes, hair, beard, neck, lashes; the beard
// in 9, the neck in none), and twelve tint files by slot: 0 skin, 1 lips, 2
// eyes, 3 hair, 4 eye make-up, 5 blush, 6 to 8 tattoo (empty in all), 9
// empty, 10 brows and stubble, 11 crew cut. Its model weights name morph
// targets as model hierarchies ("UH_NOS_30a_0" is uh_nos_30a_0.mmh, its mesh
// uh_nos_30_0.msh, in chargen.rim), 234 in all, 0 to 59 a file, none in the
// *_pcc_b01 and the *1/*2 (hm_hm1) presets. A target's first letters say its
// mesh: UH and UHM the face, UE and UEM the eyes, UL and ULM the lashes; each
// mesh's base, UH_UHM_BASa_0, UH_UEM_BASa_0 or UH_ULM_BASa_0, takes minus the
// sum of its targets' weights (hm_cps_p01's face -7.12), in all 192 meshes of
// the files. Its material values (3 to 15 a file) are FaceM1's and HairM1's
// mml_vTintMaskColours, g_vFacialHairWeights and mml_fAgeBlendWeight, each a
// component of the parameter's vectors by index (vector index / 4, element
// index % 4: a .mor's g_vFacialHairWeights is [[0, 1, 0, 0], [0, 0, 0, 0]]
// where its .mop sets index 1 to 1); its vectors (72 to 78) the tint mask
// colours of Face, Eyes, Lashes, Hair1-3 and Beard1-3 by index 0 to 9, ten as
// a .tnt lays them out, and two meshes' g_vFacialHairColour; its textures (2,
// 4 or 6) FaceM1's diffuse, normal and age maps, and in three the eyes' and
// lashes' diffuse.
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/Morph.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::preset
{

// A morph target's weight: the model hierarchy's name, as written, and how much of it the head takes.
struct ModelWeight
{
    std::wstring model; // "UH_NOS_30a_0", uh_nos_30a_0.mmh
    float weight = 0;
};

// One number a mesh node's material takes: a component of a parameter's vectors, index / 4 the
// vector and index % 4 the element.
struct MaterialValue
{
    std::wstring node;      // "FaceM1"
    std::wstring parameter; // "g_vFacialHairWeights"
    std::int32_t index = 0;
    float value = 0;
};

// One vector a mesh node's material takes, by its parameter's name and its index among the parameter's vectors.
struct MaterialVector
{
    std::wstring node;      // "HairM1"
    std::wstring parameter; // "mml_vTintMaskColours"
    std::int32_t index = 0;
    std::array<float, 4> value{};
};

// A texture a mesh node's material takes in place of its own.
struct Texture
{
    std::wstring node;      // "FaceM1"
    std::wstring parameter; // "mml_tDiffuse"
    std::wstring texture;   // "uh_hed_dwfa_0d", some with ".dds"
};

struct Preset
{
    std::wstring name;               // "hm_cps_p01"; "hm1" for hm_hm1.mop
    std::vector<std::wstring> parts; // face, eyes, hair, beard, neck, lashes; "" for none
    std::vector<std::wstring> tints; // twelve tint files by slot, "" for none
    std::vector<ModelWeight> models; // in file order
    std::vector<MaterialValue> values;
    std::vector<MaterialVector> vectors;
    std::vector<Texture> textures;
};

// Returns a head preset's parts, tints, morph targets and material values, or nothing for a file
// that is no preset.
std::optional<Preset> PresetOf(const gff4::File &file);

// A head mesh's vertices as a composition matches them: each one's position and texture
// coordinate, in its chunk's order.
struct MeshVertices
{
    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 2>> uvs; // as many as positions
};

// Returns the vertices of a model's one mesh chunk by the model's name as a preset writes it
// ("HM_UHM_BASa_0", "UH_NOS_30a_0"); nothing for a model there is none of.
using MeshOfModel = std::function<std::optional<MeshVertices>(std::wstring_view model)>;

// Returns the morph a preset makes: its name, parts and tints, the face's, eyes' and lashes' mesh
// nodes (FaceM1, EyesM1, LashesM1) with their vertices placed, and a node per mesh node its material
// values name, carrying them. No bones. Change the preset's model weights first for a head the
// sliders moved.
// A mesh whose part is empty or not found has no places; a target not found is left out, as the
// engine leaves out one whose model is missing.
morph::Morph ComposeHead(const Preset &preset, const MeshOfModel &meshOf);

} // namespace dao::core::preset
