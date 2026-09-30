// A game object's type: the byte at CGameObject+0x14, and the scripts'
// OBJECT_TYPE_* number for it.
//
// Read 2026-09-30. CGameObject's constructor (0x00A58240) stores its first
// argument at +0x14. The typed lookups compare that byte with constants in
// .rdata (CClientExoApp::GetCreatureById: 6, from 0x00B593FC), and the
// script command GetObjectType (0x00659470) returns `1 << (type - 1)`.
// The game's script.ldf declares those script numbers (OBJECT_TYPE_GUI = 1
// to OBJECT_TYPE_MAP = 262144, as xoreos's Dragon Age ObjectType has them
// too), so each value below is the constant's bit plus one, and each is one
// of the constants in the run at 0x00B593F8 that the engine compares with.
#pragma once

#include <cstdint>

namespace dao::core
{

enum class ObjectType : std::uint8_t
{
    Gui = 1,
    Tile = 2,
    Module = 3,
    Area = 4,
    Store = 5,
    Creature = 6,
    Item = 7,
    Trigger = 8,
    Projectile = 9,
    Placeable = 10,
    AreaOfEffect = 12,
    Waypoint = 13,
    Sound = 15,
    Party = 16,
    MapPatch = 17,
    VisualEffect = 18,
    Map = 19,
};

// The scripts' OBJECT_TYPE_* for a type, as GetObjectType computes it; 0
// for a byte of 0, which the engine never shifts (no object).
constexpr std::uint32_t ScriptObjectType(ObjectType type)
{
    const auto value = static_cast<std::uint32_t>(type);
    return value == 0 || value > 32 ? 0 : std::uint32_t{1} << (value - 1);
}

} // namespace dao::core
