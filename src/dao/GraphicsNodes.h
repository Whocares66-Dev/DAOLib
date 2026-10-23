// A node of the renderer's models and the model it is part of, as dao/Lights.cpp (a point light's)
// and dao/Vfx.cpp (an emitter's) read them. Not an API.
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/core/Msvc8.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// Notes on the nodes (DAOLib #223 and #224, read 2026-10-21; dao/Addresses.h at
// kObjectNodePointLight_vftable): ObjectNode's constructor (0x0055ED20) keeps the
// GraphicsStructures::Object it is part of at +0x154 and its blueprint at +0x174, whose +0xC0
// is its name, a std::wstring. The Object keeps its blueprint at +0x40 (its slot 43,
// 0x00571500), whose +0x8 is the model's name, and its lights as a vector of their ILights (a
// light's node's +0x180) at +0x70, begin +0x74 and end +0x78: what the slot +0xB4 of its +8 base
// (0x00439320) hands the light manager's AddLights (0x00464E30). Its emitters are another
// vector, begin +0x64 and end +0x68, which SetTint (0x00546700) walks. Seen in play in
// Redcliffe's village by night: the emitters' vector fxe_fire_trch_ost_p's four emitters, as its
// .mmh has them, and fxa_lit_swrd_c's two without its light; the Object's own +0x14 the room
// layouts' names, but empty for a placed effect.

namespace dao::detail
{

inline constexpr std::size_t kNodeOwner = 0x154;     // in a node
inline constexpr std::size_t kNodeBlueprint = 0x174; // in a node
inline constexpr std::size_t kNodeName = 0xC0;       // in a node's blueprint
inline constexpr std::size_t kObjectBlueprint = 0x40;
inline constexpr std::size_t kModelName = 0x8;     // in an Object's blueprint
inline constexpr std::size_t kObjectLights = 0x74; // in an Object: begin and end of its lights' ILights

inline std::uintptr_t Field(std::uintptr_t at)
{
    return *reinterpret_cast<const std::uintptr_t *>(at);
}

inline std::wstring_view StringAt(std::uintptr_t at)
{
    return reinterpret_cast<const core::WStringRep *>(at)->View();
}

// The node's name in its model, or empty.
inline std::wstring_view NodeName(std::uintptr_t node)
{
    const std::uintptr_t blueprint = Field(node + kNodeBlueprint);
    return blueprint != 0 ? StringAt(blueprint + kNodeName) : std::wstring_view();
}

// The name of an Object's model, or empty.
inline std::wstring_view ObjectModelName(std::uintptr_t object)
{
    const std::uintptr_t blueprint = object != 0 ? Field(object + kObjectBlueprint) : 0;
    return blueprint != 0 ? StringAt(blueprint + kModelName) : std::wstring_view();
}

// The name of the model the node is part of, or empty.
inline std::wstring_view ModelName(std::uintptr_t node)
{
    return ObjectModelName(Field(node + kNodeOwner));
}

// Whether an Object's model has a point light of its own.
inline bool ObjectHasPointLight(std::uintptr_t object)
{
    if (object == 0)
    {
        return false;
    }
    const auto *begin = reinterpret_cast<const std::uintptr_t *const *>(Field(object + kObjectLights));
    const auto *end = reinterpret_cast<const std::uintptr_t *const *>(Field(object + kObjectLights + 4));
    const std::uintptr_t pointLight = Module::Address(addr::kObjectNodePointLight_ILight_vftable);
    return begin != nullptr &&
           std::any_of(begin, end, [pointLight](const std::uintptr_t *l) { return l != nullptr && *l == pointLight; });
}

// The GraphicsStructures::Object a game object (a CCObject: a creature, placeable, item, visual
// effect) is drawn as, or 0 while it has none: its +0x154, that one's +0x18, and that one's +4, the
// Object's +8 interface (dev/SCENE.md).
inline std::uintptr_t GraphicsObjectOf(std::uintptr_t gameObject)
{
    constexpr std::size_t kAnimation = 0x154;  // in a game object: what holds its model
    constexpr std::size_t kModelHolder = 0x18; // in that: what holds the Object's +8 interface at +4
    constexpr std::size_t kObjectBase = 8;     // the interface's place in the Object
    const std::uintptr_t animation = gameObject != 0 ? Field(gameObject + kAnimation) : 0;
    const std::uintptr_t holder = animation != 0 ? Field(animation + kModelHolder) : 0;
    const std::uintptr_t base = holder != 0 ? Field(holder + 4) : 0;
    return base != 0 ? base - kObjectBase : 0;
}

// Returns the ids of the visual effects the area has running (dao/Vfx.cpp, the effect manager's).
std::vector<ObjectId> RunningEffectIds();

// Whether the model the node is part of has a point light of its own.
inline bool ModelHasPointLight(std::uintptr_t node)
{
    return ObjectHasPointLight(Field(node + kNodeOwner));
}

} // namespace dao::detail
