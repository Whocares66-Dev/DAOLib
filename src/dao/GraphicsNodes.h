// A node of the renderer's models and the model it is part of, as dao/Lights.cpp (a point light's)
// and dao/Vfx.cpp (an emitter's) read them. Not an API.
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/core/Msvc8.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

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

// The name of the model the node is part of, or empty.
inline std::wstring_view ModelName(std::uintptr_t node)
{
    const std::uintptr_t owner = Field(node + kNodeOwner);
    const std::uintptr_t blueprint = owner != 0 ? Field(owner + kObjectBlueprint) : 0;
    return blueprint != 0 ? StringAt(blueprint + kModelName) : std::wstring_view();
}

// Whether the model the node is part of has a point light of its own.
inline bool ModelHasPointLight(std::uintptr_t node)
{
    const std::uintptr_t owner = Field(node + kNodeOwner);
    if (owner == 0)
    {
        return false;
    }
    const auto *begin = reinterpret_cast<const std::uintptr_t *const *>(Field(owner + kObjectLights));
    const auto *end = reinterpret_cast<const std::uintptr_t *const *>(Field(owner + kObjectLights + 4));
    const std::uintptr_t pointLight = Module::Address(addr::kObjectNodePointLight_ILight_vftable);
    return begin != nullptr &&
           std::any_of(begin, end, [pointLight](const std::uintptr_t *l) { return l != nullptr && *l == pointLight; });
}

} // namespace dao::detail
