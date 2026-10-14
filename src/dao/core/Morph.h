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
// The nodes' texture, colour and number parameters are not read.
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/RayCast.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace dao::core::morph
{

struct Node
{
    std::wstring name; // a face bone's, or a mesh node's of a part
    Point position{};  // a face bone's offset
    // A mesh node's vertices placed again, in its chunk's order, each x, y, z, w; empty for a bone.
    std::vector<std::array<float, 4>> places;
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
