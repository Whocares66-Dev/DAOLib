// The renderer's scene as a tree (dao/Scene.h).
//
// Read 2026-10-22 (dev/SCENE.md, DAOLib #244). The current area's scene is a
// Graphics::ISceneInstance at the area's +0x62C (the area's constructor
// stores it at 0x006892AE; the layout keeps the same pointer at +0x4E0).
// Its +0x90 is a std::list of every GraphicsStructures::Object it draws:
// MSVC 2005's, its head node at +0x94 and its size at +0x98, each node
// next (+0), prev (+4) and the Object (+8). AddObject (0x0053A260) pushes
// the Object's base; RemoveObject (0x005395B0) erases it. An Object's model
// name is its blueprint's (+0x40) +0x8, its place a Vector4 at +0x100, its
// uniform scale a float at +0x5C. Its node tree hangs from the node at
// +0x58, climbed to the top by the parent at node +0x170 as the Object's
// own code does (0x00543E70); a node's children are a std::vector at node
// +0x10 (begin +0x14, end +0x18) that AddChild (0x0055F060) appends to,
// setting the child's +0x170; its name is its blueprint's (+0x174) +0xC0
// (dao/GraphicsNodes.h), its world matrix 16 floats at +0x60, row by row.
//
// What a node is comes from its class, as RTTI names it: every node class
// derives from GraphicsStructures::ObjectNode and is named for what it is
// (ObjectNodeMesh, ObjectNodeEmitterSprite<...>, ObjectNodeCrust,
// ObjectNodePointLight...). Reading the name works in every build with no
// address of a vtable, and it is read once per class (the cache below).
//
// The scene has no link back from an Object to the game object it draws;
// the other way, a game object's +0x154 leads to its Object
// (detail::GraphicsObjectOf). So the owners are found by walking the
// area's game objects and its running visual effects, and a room's own
// models by walking each room's models (a std::vector at the room's +0x2A8
// of the Objects' +8 interfaces, begin +0x2AC, end +0x2B0, an entry null
// once its room has unloaded it).
//
// The engine adds and removes Objects on the game thread with no lock
// (rooms loading, creatures coming and going), so this reads only there,
// every read guarded, and keeps no pointer past the call.
#include "dao/Scene.h"
#include "dao/SceneDetail.h"

#include "dao/ClientExoApp.h"
#include "dao/GraphicsNodes.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/TypeName.h"
#include "dao/World.h"
#include "dao/WorldDetail.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace dao::scene
{

namespace
{

constexpr std::size_t kSceneList = 0x94;    // in the scene: its object list's head node
constexpr std::size_t kSceneCount = 0x98;   // in the scene: the list's size
constexpr std::size_t kListNext = 0x0;      // in a list node
constexpr std::size_t kListValue = 0x8;     // in a list node: the Object
constexpr std::size_t kObjectRoot = 0x58;   // in an Object: a node of its tree
constexpr std::size_t kObjectScale = 0x5C;  // in an Object: a float
constexpr std::size_t kObjectPlace = 0x100; // in an Object: a Vector4
constexpr std::size_t kNodeChildren = 0x14; // in a node: begin and end of its children
constexpr std::size_t kNodeParent = 0x170;  // in a node
constexpr std::size_t kNodeWorld = 0x60;    // in a node: 16 floats, row by row
constexpr std::size_t kRoomNumber = 0x14;   // in a room
constexpr std::size_t kRoomModels = 0x2AC;  // in a room: begin and end of its models' +8 interfaces
constexpr std::size_t kObjectInterface = 8; // in an Object: the interface the rooms hold

// Bounds against a list or tree that does not read as one.
constexpr std::uint32_t kMostObjects = 20000;
constexpr std::size_t kMostChildren = 4096;
constexpr int kDeepest = 64;

// A node's class name and kind, by its vtable.
struct Class
{
    std::string type;
    Kind kind = Kind::Other;
};

// Read once per class and kept for the process, so a node's type can point into it: an unordered_map's
// elements do not move.
std::mutex g_classesLock;
std::unordered_map<std::uintptr_t, Class> g_classes;

// The classes met in one call, so that a call takes the lock once per class and not per node.
using Seen = std::unordered_map<std::uintptr_t, const Class *>;
using NodeVisitor = std::function<Next(const Node &, int)>;

// The kind a node's class name says. The names are the engine's, from RTTI (dev/SCENE.md's table);
// a template's name stays decorated, so each is matched by the part that names it.
Kind KindOf(std::string_view type)
{
    const auto has = [&](std::string_view part) { return type.find(part) != std::string_view::npos; };
    if (has("ObjectNodeEmitterGroup"))
    {
        return Kind::EmitterGroup;
    }
    if (has("ObjectNodeEmitterTarget"))
    {
        return Kind::EmitterTarget;
    }
    if (has("ObjectNodeEmitter"))
    {
        return Kind::Emitter;
    }
    if (has("ObjectNodeMesh") || has("ObjectNodeVFXMesh"))
    {
        return Kind::Mesh;
    }
    if (has("ObjectNodeBone"))
    {
        return Kind::Bone;
    }
    if (has("ObjectNodeCrust"))
    {
        return Kind::Crust;
    }
    if (has("ObjectNodeSnaptoPoint"))
    {
        return Kind::SnapPoint;
    }
    if (has("ObjectNodeUsePoint"))
    {
        return Kind::UsePoint;
    }
    if (has("ObjectNodeTerrain"))
    {
        return Kind::Terrain;
    }
    if (has("ObjectNodeCloth"))
    {
        return Kind::Cloth;
    }
    if (has("ObjectNodeWeapontrail"))
    {
        return Kind::WeaponTrail;
    }
    if (has("Light") || has("LightProbe"))
    {
        return Kind::Light;
    }
    if (type.ends_with("ObjectNode"))
    {
        return Kind::Node;
    }
    return Kind::Other;
}

const Class *ClassOf(std::uintptr_t node, Seen &seen)
{
    static const Class kNone;
    const auto table = TryRead<std::uintptr_t>(node);
    if (!table || *table == 0)
    {
        return &kNone;
    }
    if (const auto found = seen.find(*table); found != seen.end())
    {
        return found->second;
    }
    const std::lock_guard lock(g_classesLock);
    auto found = g_classes.find(*table);
    if (found == g_classes.end())
    {
        Class read;
        read.type = TypeName(reinterpret_cast<const void *>(node));
        read.kind = KindOf(read.type);
        found = g_classes.emplace(*table, std::move(read)).first;
    }
    seen.emplace(*table, &found->second);
    return &found->second;
}

// What moves of a node: its world matrix and the position in it, and its name.
void ReadPlace(Node &node)
{
    if (!TryCopy(node.world.data(), node.address + kNodeWorld, sizeof(float) * node.world.size()))
    {
        return;
    }
    const float w = node.world[15] != 0.0F && std::isfinite(node.world[15]) ? node.world[15] : 1.0F;
    node.position = {node.world[3] / w, node.world[7] / w, node.world[11] / w};
    if (const auto blueprint = TryRead<std::uintptr_t>(node.address + detail::kNodeBlueprint);
        blueprint && *blueprint != 0)
    {
        node.name = detail::NodeName(node.address);
    }
}

// A node read without its children.
Node NodeAt(std::uintptr_t at, const Class &kind)
{
    Node node;
    node.address = at;
    node.type = kind.type;
    node.kind = kind.kind;
    ReadPlace(node);
    return node;
}

// Where a node's children are: the engine's vector of node pointers, begin and end; none past the depth
// or for a vector that does not read as one.
struct Range
{
    std::uintptr_t begin = 0;
    std::uintptr_t end = 0;
    std::size_t Count() const
    {
        return (end - begin) / sizeof(std::uintptr_t);
    }
};

Range ChildrenOf(std::uintptr_t at, int depth)
{
    std::array<std::uintptr_t, 2> range{};
    if (depth >= kDeepest || !TryCopy(range.data(), at + kNodeChildren, sizeof(range)) || range[0] == 0 ||
        range[1] < range[0] || (range[1] - range[0]) / sizeof(std::uintptr_t) > kMostChildren)
    {
        return {};
    }
    return {range[0], range[1]};
}

Node ReadNode(std::uintptr_t at, int depth, Seen &seen)
{
    Node node = NodeAt(at, *ClassOf(at, seen));
    const Range children = ChildrenOf(at, depth);
    node.children.reserve(children.Count());
    for (std::uintptr_t entry = children.begin; entry < children.end; entry += sizeof(std::uintptr_t))
    {
        if (const auto child = TryRead<std::uintptr_t>(entry); child && *child != 0)
        {
            node.children.push_back(ReadNode(*child, depth + 1, seen));
        }
    }
    return node;
}

// The top of an Object's tree: its +0x58 climbed by the parents.
std::uintptr_t RootOf(std::uintptr_t object)
{
    auto node = TryRead<std::uintptr_t>(object + kObjectRoot);
    if (!node || *node == 0)
    {
        return 0;
    }
    std::uintptr_t top = *node;
    for (int i = 0; i < kDeepest; ++i)
    {
        const auto parent = TryRead<std::uintptr_t>(top + kNodeParent);
        if (!parent || *parent == 0)
        {
            break;
        }
        top = *parent;
    }
    return top;
}

// Which game object each Object draws, and the room each of a room's own models is part of.
struct Links
{
    std::unordered_map<std::uintptr_t, ObjectId> owners;
    std::unordered_map<std::uintptr_t, std::int32_t> rooms;
};

std::unordered_map<std::uintptr_t, ObjectId> ReadOwners(CClientExoApp &app)
{
    std::unordered_map<std::uintptr_t, ObjectId> owners;
    const auto own = [&](ObjectId id) {
        CCObject *object = app.GetObjectById(id);
        const std::uintptr_t graphics =
            object != nullptr ? detail::GraphicsObjectOf(reinterpret_cast<std::uintptr_t>(object)) : 0;
        if (graphics != 0)
        {
            owners.emplace(graphics, id);
        }
    };
    for (const ObjectId id : detail::AreaObjects())
    {
        own(id);
    }
    for (const ObjectId id : detail::RunningEffectIds())
    {
        own(id);
    }
    return owners;
}

std::unordered_map<std::uintptr_t, std::int32_t> ReadRooms()
{
    std::unordered_map<std::uintptr_t, std::int32_t> rooms;
    for (const std::uintptr_t room : detail::RoomPointers())
    {
        const auto number = TryRead<std::int32_t>(room + kRoomNumber);
        const auto begin = TryRead<std::uintptr_t>(room + kRoomModels);
        const auto end = TryRead<std::uintptr_t>(room + kRoomModels + 4);
        if (!number || !begin || !end || *begin == 0 || *end < *begin ||
            (*end - *begin) / sizeof(std::uintptr_t) > kMostObjects)
        {
            continue;
        }
        for (std::uintptr_t entry = *begin; entry < *end; entry += sizeof(std::uintptr_t))
        {
            const auto model = TryRead<std::uintptr_t>(entry);
            if (model && *model != 0)
            {
                rooms.emplace(*model - kObjectInterface, *number);
            }
        }
    }
    return rooms;
}

void ReadLinks(CClientExoApp &app, Links &links)
{
    links.owners = ReadOwners(app);
    links.rooms = ReadRooms();
}

Object ReadObject(std::uintptr_t at, const Links &links)
{
    Object object;
    object.address = at;
    object.model = detail::ObjectModelName(at);
    std::array<float, 3> place{};
    if (TryCopy(place.data(), at + kObjectPlace, sizeof(place)))
    {
        object.place = {place[0], place[1], place[2]};
    }
    object.scale = TryRead<float>(at + kObjectScale).value_or(1.0F);
    if (const auto owner = links.owners.find(at); owner != links.owners.end())
    {
        object.owner = owner->second;
    }
    if (const auto room = links.rooms.find(at); room != links.rooms.end())
    {
        object.room = room->second;
    }
    return object;
}

// --- The cache of the scene's structure (DAOLib #244) --------------------
//
// The scene's structure, which objects it holds and each one's tree of nodes
// with their classes, changes only when the engine adds or removes an object
// (AddObject, RemoveObject) or gives a node a child (AddChild), so it is read
// once and kept, and those three are detoured to say what changed: the next
// call reads the list again and the trees of the objects new or changed, and
// keeps the rest. What moves is not kept: an object's place and scale and a
// node's world matrix are read at each call. The links to the game objects
// and the rooms are read at each call too, since a game object can take its
// Object after the Object joined the scene. The user, 2026-10-22: cache the
// structure; a fresh walk (detail::FreshSceneObjects) is kept for the probe to
// check the cache against.

// A node as kept: the engine's node, its class, and its children by their
// places in the object's list.
struct CachedNode
{
    std::uintptr_t address = 0;
    const Class *kind = nullptr;
    std::vector<std::uint32_t> children;
};

// An object's tree as kept, its root first and in the tree's order; empty for one with no tree.
using CachedTree = std::vector<CachedNode>;

// An object as kept: its tree and the kinds of node in it, a bit a kind.
struct Kept
{
    CachedTree tree;
    std::uint32_t kinds = 0;
};

constexpr std::uint32_t Bit(Kind kind)
{
    return 1U << static_cast<unsigned>(kind);
}

std::recursive_mutex g_cacheLock;                         // a visitor may call the scene again
std::uintptr_t g_scene = 0;                               // whose structure is kept
bool g_changed = true;                                    // the list or a tree changed since it was read
std::unordered_set<std::uintptr_t> g_dirty;               // objects whose tree is to be read again
std::unordered_map<std::uintptr_t, Kept> g_kept;          // by the Object
std::vector<std::uintptr_t> g_order;                      // the scene's order
std::unordered_map<std::uintptr_t, std::int32_t> g_rooms; // the rooms' own models' rooms

std::uint32_t Cache(std::uintptr_t at, int depth, Seen &seen, CachedTree &tree)
{
    const auto index = static_cast<std::uint32_t>(tree.size());
    tree.push_back({at, ClassOf(at, seen), {}});
    const Range children = ChildrenOf(at, depth);
    for (std::uintptr_t entry = children.begin; entry < children.end; entry += sizeof(std::uintptr_t))
    {
        if (const auto child = TryRead<std::uintptr_t>(entry); child && *child != 0)
        {
            const std::uint32_t kept = Cache(*child, depth + 1, seen, tree);
            tree[index].children.push_back(kept);
        }
    }
    return index;
}

Kept KeptOf(std::uintptr_t object, Seen &seen)
{
    Kept kept;
    if (const std::uintptr_t root = RootOf(object); root != 0)
    {
        Cache(root, 0, seen, kept.tree);
    }
    for (const CachedNode &node : kept.tree)
    {
        kept.kinds |= Bit(node.kind->kind);
    }
    return kept;
}

// The scene's objects in its order, read from its list; none for a list that does not read as one.
std::vector<std::uintptr_t> ListOf(std::uintptr_t scene)
{
    std::vector<std::uintptr_t> objects;
    const auto head = TryRead<std::uintptr_t>(scene + kSceneList);
    const auto count = TryRead<std::uint32_t>(scene + kSceneCount);
    if (!head || *head == 0 || !count || *count > kMostObjects)
    {
        return objects;
    }
    objects.reserve(*count);
    auto node = TryRead<std::uintptr_t>(*head + kListNext);
    for (std::uint32_t i = 0; i < *count && node && *node != 0 && *node != *head; ++i)
    {
        if (const auto value = TryRead<std::uintptr_t>(*node + kListValue); value && *value != 0)
        {
            objects.push_back(*value);
        }
        node = TryRead<std::uintptr_t>(*node + kListNext);
    }
    return objects;
}

// Brings the kept structure up to the scene's. Under g_cacheLock.
void Refresh(std::uintptr_t scene, Seen &seen)
{
    if (scene != g_scene)
    {
        g_scene = scene;
        g_kept.clear();
        g_order.clear();
        g_changed = true;
    }
    if (!g_changed)
    {
        return;
    }
    g_order = ListOf(scene);
    std::unordered_map<std::uintptr_t, Kept> all;
    all.reserve(g_order.size());
    for (const std::uintptr_t object : g_order)
    {
        auto kept = g_kept.find(object);
        all.emplace(object,
                    kept != g_kept.end() && !g_dirty.contains(object) ? std::move(kept->second) : KeptOf(object, seen));
    }
    g_kept = std::move(all);
    g_rooms = ReadRooms();
    g_dirty.clear();
    g_changed = false;
}

// AddObject and RemoveObject: __thiscall (the Object's +8 interface, a second word), `ret 8`; AddChild:
// __thiscall (child), `ret 4`. What they answer is handed back as it is.
using ObjectFn = std::uintptr_t(__fastcall *)(void *scene, int edx, std::uintptr_t object, std::uintptr_t second);
using ChildFn = std::uintptr_t(__fastcall *)(void *node, int edx, std::uintptr_t child);
Detour<ObjectFn> g_addObject;
Detour<ObjectFn> g_removeObject;
Detour<ChildFn> g_addChild;

std::uintptr_t __fastcall AddObject(void *scene, int edx, std::uintptr_t object, std::uintptr_t second)
{
    const std::uintptr_t answer = g_addObject.Original()(scene, edx, object, second);
    if (object != 0)
    {
        const std::lock_guard lock(g_cacheLock);
        g_dirty.insert(object - kObjectInterface);
        g_changed = true;
    }
    return answer;
}

std::uintptr_t __fastcall RemoveObject(void *scene, int edx, std::uintptr_t object, std::uintptr_t second)
{
    if (object != 0)
    {
        const std::lock_guard lock(g_cacheLock);
        g_kept.erase(object - kObjectInterface);
        g_dirty.erase(object - kObjectInterface);
        g_changed = true;
    }
    return g_removeObject.Original()(scene, edx, object, second);
}

std::uintptr_t __fastcall AddChild(void *node, int edx, std::uintptr_t child)
{
    const std::uintptr_t answer = g_addChild.Original()(node, edx, child);
    const auto owner = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(node) + detail::kNodeOwner);
    if (owner && *owner != 0)
    {
        const std::lock_guard lock(g_cacheLock);
        if (g_kept.contains(*owner)) // one not kept is read whole when the scene takes it
        {
            g_dirty.insert(*owner);
            g_changed = true;
        }
    }
    return answer;
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_addObject.Attach(addr::kISceneInstance_AddObject, &AddObject);
        g_removeObject.Attach(addr::kISceneInstance_RemoveObject, &RemoveObject);
        g_addChild.Attach(addr::kObjectNode_AddChild, &AddChild);
    });
}

// A kept node as a Node: what moves read now.
Node NodeOf(const CachedNode &kept)
{
    return NodeAt(kept.address, *kept.kind);
}

Node BuildKept(const CachedTree &tree, std::uint32_t at)
{
    Node node = NodeOf(tree[at]);
    node.children.reserve(tree[at].children.size());
    for (const std::uint32_t child : tree[at].children)
    {
        node.children.push_back(BuildKept(tree, child));
    }
    return node;
}

bool WalkKept(const CachedTree &tree, std::uint32_t at, int depth, const NodeVisitor &visit)
{
    const Next next = visit(NodeOf(tree[at]), depth);
    if (next != Next::Descend)
    {
        return next != Next::Stop;
    }
    return std::ranges::all_of(tree[at].children,
                               [&](const std::uint32_t child) { return WalkKept(tree, child, depth + 1, visit); });
}

// The current area's scene and the game, or nothing before an area or off a known build.
struct Here
{
    CClientExoApp *app = nullptr;
    std::uintptr_t scene = 0;
};

std::optional<Here> HereNow()
{
    CClientExoApp *app = Module::IsKnownExecutable() ? CClientExoApp::Get() : nullptr;
    const CCArea *area = app != nullptr ? world::CurrentArea() : nullptr;
    const auto scene = area != nullptr ? reinterpret_cast<std::uintptr_t>(area->Scene()) : 0;
    return scene != 0 ? std::optional<Here>(Here{app, scene}) : std::nullopt;
}

// Hands each kept object with a node of the kinds asked (all for ~0), its fields read now, with its
// tree, to `each`, until it answers false. The owners are read only once one is handed.
void ForEachKept(std::uint32_t kinds, const std::function<bool(Object &object, const CachedTree &tree)> &each)
{
    const auto here = HereNow();
    if (!here)
    {
        return;
    }
    Attach();
    const std::lock_guard lock(g_cacheLock);
    Seen seen;
    Refresh(here->scene, seen);
    std::optional<Links> links;
    static const Kept kNone;
    for (const std::uintptr_t at : g_order)
    {
        const auto found = g_kept.find(at);
        const Kept &kept = found != g_kept.end() ? found->second : kNone;
        if (kinds != ~0U && (kept.kinds & kinds) == 0)
        {
            continue;
        }
        if (!links)
        {
            links.emplace();
            links->owners = ReadOwners(*here->app);
            links->rooms = g_rooms;
        }
        Object object = ReadObject(at, *links);
        if (!each(object, kept.tree))
        {
            return;
        }
    }
}

// Hands each object of the current area's scene, its fields read and its tree not, to `each`, in the
// scene's order, reading the scene afresh and keeping nothing.
void ForEachFresh(const std::function<void(Object &object, Seen &seen)> &each)
{
    const auto here = HereNow();
    if (!here)
    {
        return;
    }
    Links links;
    ReadLinks(*here->app, links);
    Seen seen;
    for (const std::uintptr_t at : ListOf(here->scene))
    {
        Object object = ReadObject(at, links);
        each(object, seen);
    }
}

void Collect(const Node &node, Kind kind, std::vector<const Node *> &into)
{
    if (node.kind == kind)
    {
        into.push_back(&node);
    }
    for (const Node &child : node.children)
    {
        Collect(child, kind, into);
    }
}

} // namespace

std::vector<const Node *> Object::Find(Kind kind) const
{
    std::vector<const Node *> found;
    Collect(root, kind, found);
    return found;
}

std::vector<Object> Objects()
{
    std::vector<Object> objects;
    ForEachKept(~0U, [&](Object &object, const CachedTree &tree) {
        if (!tree.empty())
        {
            object.root = BuildKept(tree, 0);
        }
        objects.push_back(std::move(object));
        return true;
    });
    return objects;
}

void Walk(const std::function<Next(const Object &object)> &object,
          const std::function<Next(const Node &node, int depth)> &node)
{
    ForEachKept(~0U, [&](Object &each, const CachedTree &tree) {
        const Next next = object(each);
        if (next == Next::Stop)
        {
            return false;
        }
        return next == Next::Skip || tree.empty() || WalkKept(tree, 0, 0, node);
    });
}

void ForEach(Kind kind, const std::function<void(const Object &object, const Node &node)> &each)
{
    ForEachKept(Bit(kind), [&](Object &object, const CachedTree &tree) {
        for (const CachedNode &node : tree)
        {
            if (node.kind->kind == kind)
            {
                each(object, NodeOf(node));
            }
        }
        return true;
    });
}

} // namespace dao::scene

namespace dao
{

std::vector<scene::Object> detail::FreshSceneObjects()
{
    std::vector<scene::Object> objects;
    scene::ForEachFresh([&](scene::Object &object, scene::Seen &seen) {
        if (const std::uintptr_t root = scene::RootOf(object.address); root != 0)
        {
            object.root = scene::ReadNode(root, 0, seen);
        }
        objects.push_back(std::move(object));
    });
    return objects;
}

} // namespace dao
