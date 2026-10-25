// A follower's tactics as the tactics screen sets them, with or without the screen: its slots, what
// the screen offers each slot (the targets, conditions, commands, abilities and items), a slot set
// as the screen's pickers set it and refused where they offer nothing of the kind, the AI on or off,
// the presets and the behaviour.
//
//   for (const auto &target : dao::tactics::Targets(wynne))
//       for (const auto base : dao::tactics::BaseConditions(target.type))
//           ... dao::tactics::Conditions(base, target.type) ...
//   dao::tactics::SetCondition(wynne, 1, target, condition);
//   dao::tactics::SetCommand(wynne, 1, dao::tactics::kUseAbility, heal);
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace dao::tactics
{

// Commands with a parameter of their own, as rows of the tactics commands table.
inline constexpr std::int32_t kUseAbility = 4;     // the ability, a row of abi_base
inline constexpr std::int32_t kActivateMode = 5;   // a sustained ability
inline constexpr std::int32_t kDeactivateMode = 6; // a sustained ability
inline constexpr std::int32_t kUseItem = 7;        // the item, given apart
inline constexpr std::int32_t kJumpToTactic = 10;  // a later slot, from 0

// A slot of a follower's tactics, as the screen shows it.
struct Slot
{
    std::int32_t index = 0; // from 1, as the scripts number them
    bool on = false;        // its own switch; the AI's is apart (AiOn)
    std::int32_t target = 0;
    ObjectId targetObject = kInvalidObject; // a follower a "Follower" target names
    std::int32_t condition = 0;
    ObjectId conditionObject = kInvalidObject;
    std::int32_t command = 0;
    std::int32_t parameter = 0;     // an ability, a slot to jump to
    ObjectId item = kInvalidObject; // a Use Item's
};

// Returns a follower's slots, whether or not its AI is on.
// @return none for a creature with no tactics, one that has never been a follower
//
// Call it, and the rest, on the game thread.
std::vector<Slot> Slots(ObjectId creature);

// Returns whether a follower's AI runs its tactics, as the screen's switch shows it.
std::optional<bool> AiOn(ObjectId creature);

// A target the screen offers, with the follower it names for a "Follower" target.
struct Target
{
    std::int32_t type = 0; // a row of the tactics targets table
    ObjectId object = kInvalidObject;
};

// A condition the screen offers, with the follower it names for one per follower.
struct Condition
{
    std::int32_t condition = 0; // a row of the tactics conditions table
    ObjectId object = kInvalidObject;
};

// What the screen offers a follower's slots, as its pickers list them.
std::vector<Target> Targets(ObjectId creature);
std::vector<std::int32_t> BaseConditions(std::int32_t target);
std::vector<Condition> Conditions(std::int32_t baseCondition, std::int32_t target);
std::vector<std::int32_t> Commands(ObjectId creature, std::int32_t target, std::int32_t baseCondition);
std::vector<std::int32_t> Abilities(ObjectId creature, std::int32_t target, std::int32_t baseCondition);
std::vector<std::int32_t> ModalAbilities(ObjectId creature);
std::vector<ObjectId> Items(std::int32_t target);

// Returns a condition's base, the group the screen lists it under.
std::int32_t BaseOf(std::int32_t condition);

// Why a slot was not set, or None if it was.
enum class Refusal : std::uint8_t
{
    None,
    NotFollower, // the creature has no tactics
    NoSuchSlot,  // the slot is not one of its
    NotOffered,  // the screen offers no such target, condition, command, ability, item or jump there
};

// Sets a slot's target and condition as the screen's condition picker does: the slot's command
// then cleared if the condition does not allow it. Target and condition 0 clear the slot.
Refusal SetCondition(ObjectId creature, std::int32_t slot, Target target, Condition condition);

// Sets a slot's command as the screen's action picker does, with its ability, its sustained
// ability, the slot to jump to, or (for kUseItem) its item.
Refusal SetCommand(ObjectId creature, std::int32_t slot, std::int32_t command, std::int32_t parameter = 0,
                   ObjectId item = kInvalidObject);

// Switches a slot on or off, as its own switch does.
Refusal SetOn(ObjectId creature, std::int32_t slot, bool on);

// Switches a follower's AI on or off, as the screen's switch does.
bool SetAiOn(ObjectId creature, bool on);

// Loads a preset into a follower's slots, as the screen's preset picker does.
// @param type 0 the AI off, 1 a custom slot, 2 a preset of the game's
// @param index the custom slot or the place among the presets, from 0
bool LoadPreset(ObjectId creature, std::int32_t type, std::int32_t index);

// Saves a follower's slots into a custom slot, as the screen's Save does.
bool SavePreset(ObjectId creature, std::int32_t slot);

// Sets a follower's behaviour, a row of the aibehaviors table, as the screen's picker does.
bool SetBehaviour(ObjectId creature, std::int32_t behaviour);

} // namespace dao::tactics
