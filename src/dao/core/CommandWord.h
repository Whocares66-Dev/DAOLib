// The first word of a command queued on a game object (dao/Objects.h,
// Command): read in play 2026-09-30 from about 200 commands through
// CCObject::QueueCommand (dev/TURN_BASED.md, "The queue in play").
//
//   bits  0-15  a sequence number, one higher each command in the game
//   bits 16-23  the command's type: script.ldf's COMMAND_TYPE_* numbers
//   bits 24-31  varies; not read yet
#pragma once

#include <cstdint>

namespace dao::core
{

// script.ldf's COMMAND_TYPE_*, and the types the exe's own command
// constructors set that script.ldf does not declare (39 and 41), read
// 2026-09-30 from the constant each passes to Command::SetType
// (dao/Addresses.h). 32 is queued by the engine in play and made by no
// script command.
enum class CommandType : std::uint8_t
{
    Invalid = 0,
    Attack = 1,
    DoFunction = 6,
    JumpToObject = 9,
    JumpToLocation = 10,
    Wait = 13,
    PlayAnimation = 14,
    StartConversation = 18,
    MoveToLocation = 20,
    MoveToObject = 21,
    Recover = 22, // what CommandMoveToMultiLocations makes
    UseAbility = 25,
    EquipItem = 29,
    UnequipItem = 30,
    UseObject = 31,
    Drive = 33,
    Interact = 37,
    Deathblow = 38,
    SheatheWeapons = 39, // CommandSheatheWeapons and CommandUnsheatheWeapons; int 0 is 1 to draw
    SwitchWeaponSets = 40,
    Turn = 41, // CommandTurn
    Fly = 42,
};

constexpr CommandType TypeOf(std::uint32_t word)
{
    return static_cast<CommandType>((word >> 16) & 0xFF);
}

constexpr std::uint16_t SequenceOf(std::uint32_t word)
{
    return static_cast<std::uint16_t>(word & 0xFFFF);
}

} // namespace dao::core
