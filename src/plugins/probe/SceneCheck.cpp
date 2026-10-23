#include "SceneCheck.h"

#include "dao/Frame.h"
#include "dao/GraphicsNodes.h"
#include "dao/Log.h"
#include "dao/SafeRead.h"
#include "dao/Scene.h"
#include "dao/SceneDetail.h"
#include "dao/String.h"
#include "dao/Vfx.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace probe
{

namespace
{

using Clock = std::chrono::steady_clock;

constexpr auto kEvery = std::chrono::seconds(5);
constexpr std::size_t kMostListed = 12;
constexpr float kNear = 0.01F; // metres: two places the same

Clock::time_point g_last;
std::wstring g_lastFirstModel; // the first room model with emitters logged whole, once per area

float Distance(const dao::Vector &a, const dao::Vector &b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return std::sqrt((x * x) + (y * y) + (z * z));
}

void CountKinds(const dao::scene::Node &node, std::array<std::size_t, 14> &counts)
{
    counts.at(static_cast<std::size_t>(node.kind))++;
    for (const dao::scene::Node &child : node.children)
    {
        CountKinds(child, counts);
    }
}

void LogTree(const dao::scene::Node &node, int depth)
{
    spdlog::info("scene check:   {:{}}{} [{}] at ({:.2f}, {:.2f}, {:.2f})", "", depth * 2, dao::Utf8(node.name),
                 node.type, node.position.x, node.position.y, node.position.z);
    for (const dao::scene::Node &child : node.children)
    {
        LogTree(child, depth + 1);
    }
}

// The rooms' own models with emitters: #242's fires.
void LogRoomEmitters(const std::vector<dao::scene::Object> &objects)
{
    std::map<std::wstring, std::size_t> byModel;
    std::size_t listed = 0;
    for (const dao::scene::Object &object : objects)
    {
        const auto emitters = object.Find(dao::scene::Kind::Emitter);
        if (!object.room || emitters.empty())
        {
            continue;
        }
        byModel[std::wstring(object.model)]++;
        if (listed++ < kMostListed)
        {
            spdlog::info("scene check: room {} model {} at ({:.2f}, {:.2f}, {:.2f}), {} emitter(s), the first {} at "
                         "({:.2f}, {:.2f}, {:.2f})",
                         *object.room, dao::Utf8(object.model), object.place.x, object.place.y, object.place.z,
                         emitters.size(), dao::Utf8(emitters.front()->name), emitters.front()->position.x,
                         emitters.front()->position.y, emitters.front()->position.z);
        }
        if (g_lastFirstModel.empty())
        {
            g_lastFirstModel = std::wstring(object.model);
            spdlog::info("scene check: the tree of {}:", dao::Utf8(object.model));
            LogTree(object.root, 0);
        }
    }
    std::string summary;
    for (const auto &[model, count] : byModel)
    {
        summary += " " + dao::Utf8(model) + " x" + std::to_string(count);
    }
    spdlog::info("scene check: rooms' models with emitters:{}", summary.empty() ? std::string(" none") : summary);
}

// Two nodes the same, all their tree: the same engine node, kind, name and place, the same children in order.
bool SameTree(const dao::scene::Node &a, const dao::scene::Node &b)
{
    if (a.address != b.address || a.kind != b.kind || a.name != b.name || a.world != b.world ||
        a.children.size() != b.children.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.children.size(); ++i)
    {
        if (!SameTree(a.children[i], b.children[i]))
        {
            return false;
        }
    }
    return true;
}

// The cache against a fresh read of the whole scene, object by object; and whether each tree's root
// knows its object at +0x154, which tells the cache whose tree a new child changed.
void CompareCache(const std::vector<dao::scene::Object> &kept, const std::vector<dao::scene::Object> &fresh)
{
    std::size_t same = 0;
    std::size_t differ = 0;
    std::size_t knowsOwner = 0;
    std::size_t roots = 0;
    for (std::size_t i = 0; i < std::min(kept.size(), fresh.size()); ++i)
    {
        const dao::scene::Object &a = kept[i];
        const dao::scene::Object &b = fresh[i];
        const bool sameObject = a.address == b.address && a.model == b.model && a.owner == b.owner &&
                                a.room == b.room && SameTree(a.root, b.root);
        if (sameObject)
        {
            ++same;
        }
        else if (differ++ < kMostListed)
        {
            spdlog::warn("scene check: cache and fresh read differ at object {} ({} {:#x}, fresh {} {:#x})", i,
                         dao::Utf8(a.model), a.address, dao::Utf8(b.model), b.address);
        }
        if (b.root.address != 0)
        {
            ++roots;
            const auto owner = dao::TryRead<std::uintptr_t>(b.root.address + dao::detail::kNodeOwner);
            knowsOwner += owner && *owner == b.address ? 1 : 0;
        }
    }
    spdlog::info("scene check: cache {} objects, fresh {}: {} the same, {} differ; {} of {} roots hold their object "
                 "at +0x154",
                 kept.size(), fresh.size(), same, differ, knowsOwner, roots);
}

// The rooms' own models RunningEffects adds (#242), against the full tree's own filter; returns how many of the
// effects without an id it listed.
std::size_t CompareRoomModels(const std::vector<dao::scene::Object> &objects,
                              const std::vector<dao::vfx::RunningEffect> &effects)
{
    std::vector<const dao::scene::Object *> roomFires;
    for (const dao::scene::Object &object : objects)
    {
        if (object.room && !object.Find(dao::scene::Kind::Emitter).empty())
        {
            roomFires.push_back(&object);
        }
    }
    std::size_t roomListed = 0;
    std::size_t roomMatched = 0;
    for (const dao::vfx::RunningEffect &effect : effects)
    {
        if (effect.id != dao::kInvalidObject)
        {
            continue;
        }
        ++roomListed;
        for (const dao::scene::Object *object : roomFires)
        {
            if (object->model == effect.model && effect.position && Distance(*effect.position, object->place) <= kNear)
            {
                ++roomMatched;
                break;
            }
        }
    }
    spdlog::info("scene check: RunningEffects lists {} rooms' models with emitters, the full tree has {}; {} of the "
                 "listed match one of them by model and place",
                 roomListed, roomFires.size(), roomMatched);
    return roomListed;
}

// The scene's objects by the game object each draws.
std::unordered_map<dao::ObjectId, const dao::scene::Object *> ByOwner(const std::vector<dao::scene::Object> &objects)
{
    std::unordered_map<dao::ObjectId, const dao::scene::Object *> byOwner;
    for (const dao::scene::Object &object : objects)
    {
        if (object.owner != dao::kInvalidObject)
        {
            byOwner.emplace(object.owner, &object);
        }
    }
    return byOwner;
}

// RunningEffects' list against the scene: each effect's own Object, found by its owner.
void Compare(const std::vector<dao::scene::Object> &objects, const std::vector<dao::vfx::RunningEffect> &effects)
{
    const auto byOwner = ByOwner(objects);
    std::size_t same = 0;
    std::size_t noModel = 0;
    std::size_t differs = 0;
    std::size_t missing = 0;
    const std::size_t roomListed = CompareRoomModels(objects, effects);
    for (const dao::vfx::RunningEffect &effect : effects)
    {
        if (effect.id == dao::kInvalidObject)
        {
            continue;
        }
        const auto found = byOwner.find(effect.id);
        if (found == byOwner.end())
        {
            if (effect.model.empty())
            {
                ++noModel; // no model loaded, so no Object: expected
                continue;
            }
            if (missing++ < kMostListed)
            {
                spdlog::warn("scene check: effect {:#x} ({}) lists model {}, but no scene object is its", effect.id,
                             effect.vfx, dao::Utf8(effect.model));
            }
            continue;
        }
        const dao::scene::Object &object = *found->second;
        const bool modelSame = object.model == effect.model;
        // An effect on an object takes its owner's place; one at a point, its own.
        const bool placeSame = !effect.position || effect.owner != dao::kInvalidObject ||
                               Distance(*effect.position, object.place) <= kNear;
        if (modelSame && placeSame)
        {
            ++same;
        }
        else if (differs++ < kMostListed)
        {
            spdlog::warn("scene check: effect {:#x} ({}): RunningEffects says model {} at ({:.2f}, {:.2f}, {:.2f}), "
                         "the scene {} at ({:.2f}, {:.2f}, {:.2f})",
                         effect.id, effect.vfx, dao::Utf8(effect.model), effect.position ? effect.position->x : 0.0F,
                         effect.position ? effect.position->y : 0.0F, effect.position ? effect.position->z : 0.0F,
                         dao::Utf8(object.model), object.place.x, object.place.y, object.place.z);
        }
    }
    spdlog::info("scene check: RunningEffects' {} effects with an id against the scene: {} the same, {} with no "
                 "model (no object, as expected), {} differ, {} with a model and no object",
                 effects.size() - roomListed, same, noModel, differs, missing);
}

void Check()
{
    const auto now = Clock::now();
    if (now - g_last < kEvery)
    {
        return;
    }
    g_last = now;
    const auto start = Clock::now();
    const std::vector<dao::scene::Object> fresh = dao::detail::FreshSceneObjects();
    const auto before = Clock::now();
    const std::vector<dao::scene::Object> objects = dao::scene::Objects();
    const auto read = Clock::now();
    const std::vector<dao::vfx::RunningEffect> effects = dao::vfx::RunningEffects();
    const auto listed = Clock::now();
    // The walk's parts, timed apart: the objects and their links alone, then every node visited.
    dao::scene::Walk([](const dao::scene::Object &) { return dao::scene::Next::Skip; },
                     [](const dao::scene::Node &, int) { return dao::scene::Next::Descend; });
    const auto objectsOnly = Clock::now();
    std::size_t visited = 0;
    dao::scene::Walk([](const dao::scene::Object &) { return dao::scene::Next::Descend; },
                     [&](const dao::scene::Node &, int) {
                         ++visited;
                         return dao::scene::Next::Descend;
                     });
    const auto everyNode = Clock::now();
    if (objects.empty())
    {
        g_lastFirstModel.clear();
        return;
    }
    std::array<std::size_t, 14> kinds{};
    std::size_t owned = 0;
    std::size_t roomModels = 0;
    for (const dao::scene::Object &object : objects)
    {
        CountKinds(object.root, kinds);
        owned += object.owner != dao::kInvalidObject ? 1 : 0;
        roomModels += object.room ? 1 : 0;
    }
    using Micro = std::chrono::microseconds;
    spdlog::info("scene check: {} objects ({} a game object's, {} a room's own) read in {} us; RunningEffects {} in "
                 "{} us; nodes: {} plain, {} bones, {} crust hooks, {} meshes, {} emitters, {} emitter groups, {} "
                 "lights, {} other",
                 objects.size(), owned, roomModels, std::chrono::duration_cast<Micro>(read - before).count(),
                 effects.size(), std::chrono::duration_cast<Micro>(listed - read).count(), kinds[0], kinds[1], kinds[2],
                 kinds[6], kinds[7], kinds[8], kinds[12], kinds[13]);
    spdlog::info("scene check: a fresh read {} us; a walk of the objects alone {} us; of every node ({} nodes) {} us",
                 std::chrono::duration_cast<Micro>(before - start).count(),
                 std::chrono::duration_cast<Micro>(objectsOnly - listed).count(), visited,
                 std::chrono::duration_cast<Micro>(everyNode - objectsOnly).count());
    CompareCache(objects, fresh);
    LogRoomEmitters(objects);
    Compare(objects, effects);
}

} // namespace

void AttachSceneCheck()
{
    dao::OnFrame(&Check);
    spdlog::info("scene check: on, every {} s", kEvery.count());
}

} // namespace probe
