// A creature's animation tree, <tree>.anb, over bytes: the named nodes the
// engine plays an animation by, each its clip's file. apr_base names a
// creature's trees (ModelTree, CombatModelTree, MovementTree, CombatMoveTree,
// OverlayTree, OverrideTree, AmbientOverride), and an animation's row of the
// animation tables (table 0: anim_base, anim_combat, anim_placeables) names
// its node by its AnimName, with its Replacement the row to play where no
// tree has that node.
//
//   const auto tree = dao::core::anb::TreeOf(file);
//   const auto *pause = tree->Find(L"pause"); // pause->file == L"mh_m.p.ani" in humanoid.anb
//
// The file is GFF4 "ANBT V1.0": a list of nodes (AnimationTree, 4021), each
// its name (4024), its clip's file (4025, empty for a node that only holds
// others), a weight (4026), flags (4027), its children as a first index and
// a count (4028, 4029) and its parent's index (4030, -1 for the root). Read
// 2026-10-17 over all 1,112 the game takes (DAOLib #163): every tree's last
// node is its root (parent -1); a node's children follow one
// another; flags are 0 or 1, 1 on the one-shot clips (an attack, an idle
// twitch) and 0 on the loops (pause, a ready stance) and the nodes that only
// hold others, so by the look of it a clip that plays once (not read in the
// exe); weights 1 or 0. Which of a creature's trees the engine looks in first
// is not read either. A clip's file is named whole, its creature's
// prefix in it (mh., mh_m., mh_c., fh., c_ogre.); nothing is put before it.
// The 95 level .anb files (amn120d_0_2_0.anb and kin) are another format, XML, read by
// RoomTreeOf below.
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/M2da.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::anb
{

struct Node
{
    std::wstring name; // an animation's AnimName: "pause", "attk_v1"
    std::wstring file; // its clip: "mh_m.p.ani"; empty for a node holding others
    float weight = 1;
    std::uint32_t flags = 0; // 1 on a clip that plays once
    std::uint32_t firstChild = 0;
    std::uint32_t children = 0;
    std::int32_t parent = -1;
};

struct Tree
{
    std::wstring name;
    std::vector<Node> nodes;

    // Returns the node of a name, in any case; null for none.
    const Node *Find(std::wstring_view node) const;
};

// Returns an animation tree, or nothing for a file that is not one.
std::optional<Tree> TreeOf(const gff4::File &file);

// Returns the tree a female creature takes in place of one: its name up to its last dot, then
// "_fem.anb" (movement.anb becoming movement_fem.anb), where the game has that file; the engine
// looks for it for gender 2 alone.
std::wstring FemaleTree(std::wstring_view tree);

// An animation's clip as a creature's trees give it.
struct Clip
{
    std::int64_t animation = 0; // the row played: the one asked for, or a replacement of it
    const Node *node = nullptr;
    std::size_t tree = 0; // its place among the trees given
};

// Returns the clip of an animation: the first of the trees with its row's AnimName, else the same
// for its Replacement, and so on.
// @param animations the animation tables merged (table 0)
// @param trees the creature's trees in the order to look in them
// @param replacements false to look up the animation's own node alone, as the engine does for a
// placeable
// @return nothing if the animation has no row, or neither it nor its replacements have a node
std::optional<Clip> ClipOf(const m2da::Merged &animations, std::int64_t animation, std::span<const Tree *const> trees,
                           bool replacements = true);

// A room's animation tree, <room>.anb in its rim: XML, not a creature's GFF4, with the same idea
// of named nodes each a clip. A room's placed model plays one of its nodes from the start
// (level::Placed's defaultAnimation and blendTree).
//
//   const auto tree = dao::core::anb::RoomTreeOf(xml);
//   const auto *wheel = tree->Find(L"fhec_windmillwhl_0.wind"); // its file "fhec_windmillwhl_0.wind.ani"
struct RoomNode
{
    std::wstring name; // what a placed model's default animation names
    std::wstring file; // its clip: "fhec_windmillwhl_0.wind.ani"
    float speed = 1;
    bool noLoop = false;
};

struct RoomTree
{
    std::wstring name;
    std::vector<RoomNode> nodes; // the nodes with a clip, in the file's order

    // Returns the node of a name, in any case; null for none.
    const RoomNode *Find(std::wstring_view node) const;
};

// Returns a room's animation tree from its file's text, or nothing for text that is not one.
std::optional<RoomTree> RoomTreeOf(std::string_view xml);

} // namespace dao::core::anb
