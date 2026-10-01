// The first word of a command queued on a game object (dao/Objects.h,
// Command): read in play 2026-09-30 from about 200 commands through
// CCObject::QueueCommand (dev/TURN_BASED.md, "The queue in play").
//
//   bits  0-15  a sequence number, one higher each command in the game
//   bits 16-23  the command's type: script.ldf's COMMAND_TYPE_* numbers
//   bits 24-31  varies; not read yet
#pragma once

#include <cstdint>
#include <string_view>

namespace dao::core
{

// Every command type the game has: the commands table (TABLE_COMMANDS,
// commands.gda, 43 rows, tools/gda.py --dump commands), which QueueCommand
// reads each type's add behaviour from. Named as script.ldf's
// COMMAND_TYPE_* where it declares one (20 of them), else after the
// table's label. Two differ: script.ldf calls 22 RECOVER, but the table
// calls it "Move to Multiple Points" and CommandMoveToMultiLocations makes
// it; and CommandMoveAwayFromObject makes 21, not 16.
enum class CommandType : std::uint8_t
{
    Invalid = 0,
    Attack = 1,
    Appear = 2,
    Disappear = 3,
    ForceFollowObject = 4,
    RandomWalk = 5,
    DoFunction = 6,
    GiveItem = 7,
    TakeItem = 8,
    JumpToObject = 9,
    JumpToLocation = 10, // "Jump To Point"
    SpeakString = 11,
    SpeakStrref = 12,
    Wait = 13,
    PlayAnimation = 14,
    PlaySound = 15,
    MoveAwayFromObject = 16,
    MoveAwayFromPoint = 17,
    StartConversation = 18, // "Initiate Dialog"
    Examine = 19,
    MoveToLocation = 20, // "Move To Point"
    MoveToObject = 21,
    MoveToMultipleLocations = 22,
    Lock = 23,
    Unlock = 24,
    UseAbility = 25,
    UseItem = 26,
    DropItem = 27,
    PickupItem = 28,
    EquipItem = 29,
    UnequipItem = 30,
    UseObject = 31,
    ExecuteEffect = 32, // the engine's, as effects apply
    Drive = 33,         // the player steering the character
    SellItem = 34,
    BuyItem = 35,
    DoEvent = 36,
    Interact = 37,
    Deathblow = 38,
    SheatheWeapons = 39, // "Sheathing": CommandSheatheWeapons and CommandUnsheatheWeapons
    SwitchWeaponSets = 40,
    Turn = 41,
    Fly = 42,
};

// A command type's name, as the enum spells it ("UseAbility"); empty for a
// type not named above. The scripts have no constants for these.
constexpr std::string_view Name(CommandType type)
{
    switch (type)
    {
    case CommandType::Invalid:
        return "Invalid";
    case CommandType::Attack:
        return "Attack";
    case CommandType::Appear:
        return "Appear";
    case CommandType::Disappear:
        return "Disappear";
    case CommandType::ForceFollowObject:
        return "ForceFollowObject";
    case CommandType::RandomWalk:
        return "RandomWalk";
    case CommandType::DoFunction:
        return "DoFunction";
    case CommandType::GiveItem:
        return "GiveItem";
    case CommandType::TakeItem:
        return "TakeItem";
    case CommandType::JumpToObject:
        return "JumpToObject";
    case CommandType::JumpToLocation:
        return "JumpToLocation";
    case CommandType::SpeakString:
        return "SpeakString";
    case CommandType::SpeakStrref:
        return "SpeakStrref";
    case CommandType::Wait:
        return "Wait";
    case CommandType::PlayAnimation:
        return "PlayAnimation";
    case CommandType::PlaySound:
        return "PlaySound";
    case CommandType::MoveAwayFromObject:
        return "MoveAwayFromObject";
    case CommandType::MoveAwayFromPoint:
        return "MoveAwayFromPoint";
    case CommandType::StartConversation:
        return "StartConversation";
    case CommandType::Examine:
        return "Examine";
    case CommandType::MoveToLocation:
        return "MoveToLocation";
    case CommandType::MoveToObject:
        return "MoveToObject";
    case CommandType::MoveToMultipleLocations:
        return "MoveToMultipleLocations";
    case CommandType::Lock:
        return "Lock";
    case CommandType::Unlock:
        return "Unlock";
    case CommandType::UseAbility:
        return "UseAbility";
    case CommandType::UseItem:
        return "UseItem";
    case CommandType::DropItem:
        return "DropItem";
    case CommandType::PickupItem:
        return "PickupItem";
    case CommandType::EquipItem:
        return "EquipItem";
    case CommandType::UnequipItem:
        return "UnequipItem";
    case CommandType::UseObject:
        return "UseObject";
    case CommandType::ExecuteEffect:
        return "ExecuteEffect";
    case CommandType::Drive:
        return "Drive";
    case CommandType::SellItem:
        return "SellItem";
    case CommandType::BuyItem:
        return "BuyItem";
    case CommandType::DoEvent:
        return "DoEvent";
    case CommandType::Interact:
        return "Interact";
    case CommandType::Deathblow:
        return "Deathblow";
    case CommandType::SheatheWeapons:
        return "SheatheWeapons";
    case CommandType::SwitchWeaponSets:
        return "SwitchWeaponSets";
    case CommandType::Turn:
        return "Turn";
    case CommandType::Fly:
        return "Fly";
    default:
        return {};
    }
}

constexpr CommandType TypeOf(std::uint32_t word)
{
    return static_cast<CommandType>((word >> 16) & 0xFF);
}

constexpr std::uint16_t SequenceOf(std::uint32_t word)
{
    return static_cast<std::uint16_t>(word & 0xFFFF);
}

} // namespace dao::core
