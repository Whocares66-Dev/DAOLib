// The renderer's whole scene, as a plugin sees it: every graphics object the current area draws
// (its rooms' own models, creatures, placeables, items, visual effects), each with its tree of
// nodes (meshes, emitters, lights, crust hooks, bones), as an editor's outliner shows a scene.
// Finding any kind of thing in the world is then a walk of the tree and a filter.
//
//   for (const dao::scene::Object &object : dao::scene::Objects())
//       for (const dao::scene::Node *node : object.Find(dao::scene::Kind::Emitter))
//           ...   // node->name, node->position
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::scene
{

// What a node is.
enum class Kind : std::uint8_t
{
    Node,          // a plain node, a dummy that places its children
    Bone,          // a bone of the model's skeleton
    Crust,         // a crust hook: where an effect on a creature goes
    SnapPoint,     // a snap-to point
    UsePoint,      // a use point
    EmitterTarget, // where an emitter's particles go
    Mesh,          // a mesh, a visual effect's mesh among them
    Emitter,       // a particle emitter: sprites, ribbons or meshes
    EmitterGroup,  // a group of emitters
    Terrain,
    Cloth,
    WeaponTrail,
    Light, // a light: point, ambient, or a light probe
    Other, // a kind this library does not name
};

// One node of a graphics object.
struct Node
{
    std::wstring_view name; // its name in the model, as the .mmh has it; for this frame only
    std::string_view type;  // its class, as the engine's type information names it; kept for the process
    Kind kind = Kind::Other;
    Vector position;               // where it is in the world
    std::array<float, 16> world{}; // its world matrix, row by row; its fourth column is the position
    std::vector<Node> children;
    std::uintptr_t address = 0; // the engine's node, for this frame only
};

// One graphics object of the scene.
struct Object
{
    std::wstring_view model;          // its model's name, with no extension; for this frame only
    Vector place;                     // where it is in the world
    float scale = 1;                  // its uniform scale
    ObjectId owner = kInvalidObject;  // the game object it draws (a creature, placeable, effect...), if any
    std::optional<std::int32_t> room; // the room whose own model it is, by its number, for a room's model
    Node root;                        // its tree of nodes
    std::uintptr_t address = 0;       // the engine's object, for this frame only

    // Returns its nodes of a kind, anywhere in its tree, in the tree's order.
    std::vector<const Node *> Find(Kind kind) const;
};

// Returns every graphics object of the current area's scene, in the scene's order, each with its
// tree of nodes; none before an area is loaded.
//
// Call it on the game thread, and keep nothing of it past the frame: the engine adds and removes
// objects as rooms load and creatures come and go. It reads everything, a millisecond or so in a
// castle's courtyard; to find one kind of thing, ForEach reads only the objects that have one, and
// Walk only what its visitors go into.
std::vector<Object> Objects();

// What a walk does after a visit.
enum class Next : std::uint8_t
{
    Descend, // go on into this one's nodes, or children
    Skip,    // go on, past them
    Stop,    // end the walk
};

// Walks the current area's scene, reading only what the visitors go into.
// @param object gets each object with its fields and no nodes (its root is empty), and answers
//  whether to walk its nodes
// @param node gets each node walked, with no children (they come after it), and its depth below
//  the object's root, and answers whether to walk its children
//
// Call it on the game thread, as Objects.
void Walk(const std::function<Next(const Object &object)> &object,
          const std::function<Next(const Node &node, int depth)> &node);

// Hands every node of a kind in the current area's scene, with the object it is part of (its root
// empty), to `each`, in the scene's order and each object's tree's order. The fastest way to find one
// kind of thing: it reads only the objects that have one.
//
// Call it on the game thread, as Objects.
void ForEach(Kind kind, const std::function<void(const Object &object, const Node &node)> &each);

} // namespace dao::scene
