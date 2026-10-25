// A follower's tactics (dao/Tactics.h).
//
// Notes on the tactics: read 2026-10-25 (DAOLib #259) in the GOG exe and tactics.gfx, with
// dev/PARTY.md's "Tactics: the engine's share" and dev/GUI.md's "The tactics screen", and the
// Eluvian server's tactics tool, which asked a shown movie's ExternalCommands for the offers and
// wrote the screen's tree. A follower's tactics are a PartyTactics (PartyTactics::Of, 19942, by
// the creature's id; none for a creature never a follower), its MessageReceiver at +0: Enabled,
// the AI's switch, at +0x5C; its rows (PartyTactics::EntryOf, 19956, by the creature and a row
// from 0), each a PartyTacticEntry of attributes: Enabled +0x58, Target +0x68, TargetID +0x78,
// Condition +0x88, ConditionID +0x98, Command +0xA8, CommandParam +0xB8, CommandItem +0xC8 (-1
// for none). The screen's pickers offer what the engine's builders list (the GuiFSCommands
// ListTactic*, each one call): ListTargets (20010), ListBaseConditions (19983), ListConditions
// (19973), ListCommands (19993), ListAbilities (19974), ListModalAbilities (19975), ListItems
// (19984), each cdecl, filling the engine's std::vector<unsigned> and answering whether it found
// any. A pick writes the row's attributes and sends DirtyPreset; a condition's then
// ValidateAction (0x007B0170), which clears the command PartyTactics::IsCommandValid (19994) does
// not allow; an item's RefreshTacticsData. Jump offers only a later row and never the last's (the
// movie's rule; the engine checks none). The script command SetTacticEntry is not the screen's
// path: it passes IsCommandValid a base of 1 and no item, writes no TargetID, ConditionID or
// CommandItem, and marks nothing changed, so it is not used here.
//
// Seen in play 2026-10-25 (examples/tactics), Lyna's party at Redcliffe with the tactics screen
// open: Lyna's 7 slots, Alistair's 6, Wynne's 12, Leliana's 6, each with its target, condition and
// command, and 13 targets offered; Alistair's slot 1 set to target 1 and condition 70, then command
// 3, which the scripts' GetTacticTargetType, GetTacticCondition and GetTacticCommand then read; the
// module refused NotFollower, slot 99 NoSuchSlot, condition 99999, command 999 and a jump from the
// last slot NotOffered. Wynne's slot 2 set to Use Ability 200254, which GetTacticCommand and its
// parameter read, and ability 999999 refused NotOffered; her behaviour set 0 to 3 and back. The
// game's presets 0 to 3 loaded each its own slots, GetTacticPresetID then 4 to 7; slot 3 set to Use
// Item with the offered Deep Mushroom, its command 7 and its item read back.
#include "dao/Tactics.h"

#include "dao/Addresses.h"
#include "dao/Data.h"
#include "dao/Gda.h"
#include "dao/Messages.h"
#include "dao/Module.h"
#include "dao/core/GdaHash.h"
#include "dao/core/Msvc8.h"

#include <algorithm>

namespace dao::tactics
{

namespace
{

using List = core::VectorRep<std::uint32_t>;

constexpr std::size_t kAiOn = 0x5C;
constexpr std::size_t kOn = 0x58;
constexpr std::size_t kTarget = 0x68;
constexpr std::size_t kTargetId = 0x78;
constexpr std::size_t kCondition = 0x88;
constexpr std::size_t kConditionId = 0x98;
constexpr std::size_t kCommand = 0xA8;
constexpr std::size_t kParameter = 0xB8;
constexpr std::size_t kItem = 0xC8;
constexpr std::int32_t kMostSlots = 64;
constexpr std::int32_t kConditionsTable = 137;

void *TacticsOf(ObjectId creature)
{
    using OfFn = void *(__cdecl *)(ObjectId creature);
    return reinterpret_cast<OfFn>(Module::Address(addr::kPartyTactics_Of))(creature);
}

std::uintptr_t EntryOf(ObjectId creature, std::int32_t row)
{
    using EntryFn = std::uintptr_t(__cdecl *)(ObjectId creature, std::int32_t row);
    return reinterpret_cast<EntryFn>(Module::Address(addr::kPartyTactics_EntryOf))(creature, row);
}

data::Attribute *At(std::uintptr_t object, std::size_t offset)
{
    return reinterpret_cast<data::Attribute *>(object + offset);
}

std::int64_t Read(std::uintptr_t object, std::size_t offset)
{
    return static_cast<std::int64_t>(At(object, offset)->Number());
}

// Sets an attribute as the movie's SetValue does, of whichever whole type it holds.
void Write(std::uintptr_t object, std::size_t offset, std::int64_t value)
{
    data::Attribute *attribute = At(object, offset);
    if (!attribute->Set(static_cast<std::int32_t>(value)))
    {
        attribute->Set(static_cast<std::uint32_t>(value));
    }
}

ObjectId ObjectOf(std::int64_t value)
{
    return value < 0 ? kInvalidObject : static_cast<ObjectId>(value);
}

std::vector<std::uint32_t> Take(List &list)
{
    std::vector<std::uint32_t> values(list.View().begin(), list.View().end());
    if (list.first != nullptr)
    {
        using DeleteFn = void(__cdecl *)(void *);
        reinterpret_cast<DeleteFn>(Module::Address(addr::kOperatorDelete))(list.first);
    }
    list = {};
    return values;
}

std::vector<std::int32_t> Ints(List &list)
{
    std::vector<std::int32_t> values;
    for (const std::uint32_t value : Take(list))
    {
        values.push_back(static_cast<std::int32_t>(value));
    }
    return values;
}

template <class T> bool Holds(const std::vector<T> &values, const T &value)
{
    return std::ranges::find(values, value) != values.end();
}

std::int32_t Count(ObjectId creature)
{
    std::int32_t count = 0;
    while (count < kMostSlots && EntryOf(creature, count) != 0)
    {
        ++count;
    }
    return count;
}

// After a pick, as the movie sends it: the preset no longer the one chosen.
void Changed(void *tactics)
{
    messages::Send(tactics, messages::Make<messages::DirtyPresetMessage>());
}

} // namespace

std::vector<Slot> Slots(ObjectId creature)
{
    std::vector<Slot> slots;
    for (std::int32_t row = 0; row < kMostSlots; ++row)
    {
        const std::uintptr_t entry = EntryOf(creature, row);
        if (entry == 0)
        {
            break;
        }
        slots.push_back({.index = row + 1,
                         .on = Read(entry, kOn) != 0,
                         .target = static_cast<std::int32_t>(Read(entry, kTarget)),
                         .targetObject = ObjectOf(Read(entry, kTargetId)),
                         .condition = static_cast<std::int32_t>(Read(entry, kCondition)),
                         .conditionObject = ObjectOf(Read(entry, kConditionId)),
                         .command = static_cast<std::int32_t>(Read(entry, kCommand)),
                         .parameter = static_cast<std::int32_t>(Read(entry, kParameter)),
                         .item = ObjectOf(Read(entry, kItem))});
    }
    return slots;
}

std::optional<bool> AiOn(ObjectId creature)
{
    void *tactics = TacticsOf(creature);
    return tactics != nullptr ? std::optional<bool>(Read(reinterpret_cast<std::uintptr_t>(tactics), kAiOn) != 0)
                              : std::nullopt;
}

std::vector<Target> Targets(ObjectId creature)
{
    using ListFn = bool(__cdecl *)(ObjectId creature, List * types, List * objects);
    List types{};
    List objects{};
    reinterpret_cast<ListFn>(Module::Address(addr::kPartyTactics_ListTargets))(creature, &types, &objects);
    const auto kinds = Take(types);
    const auto whom = Take(objects);
    std::vector<Target> targets;
    targets.reserve(kinds.size());
    for (std::size_t i = 0; i < kinds.size(); ++i)
    {
        targets.push_back({static_cast<std::int32_t>(kinds[i]),
                           i < whom.size() ? ObjectOf(static_cast<std::int32_t>(whom[i])) : kInvalidObject});
    }
    return targets;
}

std::vector<std::int32_t> BaseConditions(std::int32_t target)
{
    using ListFn = bool(__cdecl *)(std::int32_t target, List *bases);
    List bases{};
    reinterpret_cast<ListFn>(Module::Address(addr::kPartyTactics_ListBaseConditions))(target, &bases);
    return Ints(bases);
}

std::vector<Condition> Conditions(std::int32_t baseCondition, std::int32_t target)
{
    using ListFn = bool(__cdecl *)(std::int32_t base, std::int32_t target, List *conditions, List *objects);
    List conditions{};
    List objects{};
    reinterpret_cast<ListFn>(Module::Address(addr::kPartyTactics_ListConditions))(baseCondition, target, &conditions,
                                                                                  &objects);
    const auto rows = Take(conditions);
    const auto whom = Take(objects);
    std::vector<Condition> offered;
    offered.reserve(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        offered.push_back({static_cast<std::int32_t>(rows[i]),
                           i < whom.size() ? ObjectOf(static_cast<std::int32_t>(whom[i])) : kInvalidObject});
    }
    return offered;
}

std::vector<std::int32_t> Commands(ObjectId creature, std::int32_t target, std::int32_t baseCondition)
{
    using ListFn = bool(__cdecl *)(ObjectId creature, std::int32_t target, std::int32_t base, List *commands);
    List commands{};
    reinterpret_cast<ListFn>(Module::Address(addr::kPartyTactics_ListCommands))(creature, target, baseCondition,
                                                                                &commands);
    return Ints(commands);
}

std::vector<std::int32_t> Abilities(ObjectId creature, std::int32_t target, std::int32_t baseCondition)
{
    using ListFn = bool(__cdecl *)(ObjectId creature, std::int32_t target, std::int32_t base, List *abilities);
    List abilities{};
    reinterpret_cast<ListFn>(Module::Address(addr::kPartyTactics_ListAbilities))(creature, target, baseCondition,
                                                                                 &abilities);
    return Ints(abilities);
}

std::vector<std::int32_t> ModalAbilities(ObjectId creature)
{
    using ListFn = bool(__cdecl *)(ObjectId creature, List * abilities);
    List abilities{};
    reinterpret_cast<ListFn>(Module::Address(addr::kPartyTactics_ListModalAbilities))(creature, &abilities);
    return Ints(abilities);
}

std::vector<ObjectId> Items(std::int32_t target)
{
    using ListFn = bool(__cdecl *)(std::int32_t target, List *items);
    List items{};
    reinterpret_cast<ListFn>(Module::Address(addr::kPartyTactics_ListItems))(target, &items);
    std::vector<ObjectId> found;
    for (const std::uint32_t item : Take(items))
    {
        found.push_back(static_cast<ObjectId>(item));
    }
    return found;
}

std::int32_t BaseOf(std::int32_t condition)
{
    return gda::GetInt(kConditionsTable, condition, core::GdaColumnHash("ConditionBase")).value_or(0);
}

Refusal SetCondition(ObjectId creature, std::int32_t slot, Target target, Condition condition)
{
    void *tactics = TacticsOf(creature);
    if (tactics == nullptr)
    {
        return Refusal::NotFollower;
    }
    const std::uintptr_t entry = EntryOf(creature, slot - 1);
    if (slot < 1 || entry == 0)
    {
        return Refusal::NoSuchSlot;
    }
    const bool clearing = target.type == 0 && condition.condition == 0;
    if (!clearing)
    {
        const auto targets = Targets(creature);
        const bool targetOffered = std::ranges::any_of(
            targets, [&](const Target &offer) { return offer.type == target.type && offer.object == target.object; });
        const auto conditions = Conditions(BaseOf(condition.condition), target.type);
        const bool conditionOffered = std::ranges::any_of(conditions, [&](const Condition &offer) {
            return offer.condition == condition.condition && offer.object == condition.object;
        });
        if (!targetOffered || !conditionOffered)
        {
            return Refusal::NotOffered;
        }
    }
    Write(entry, kTarget, target.type);
    Write(entry, kTargetId, target.object == kInvalidObject ? -1 : static_cast<std::int64_t>(target.object));
    Write(entry, kCondition, condition.condition);
    Write(entry, kConditionId, condition.object == kInvalidObject ? -1 : static_cast<std::int64_t>(condition.object));
    Changed(tactics);
    auto validate = messages::Make<messages::ValidateActionMessage>();
    validate.row = slot - 1;
    messages::Send(tactics, validate);
    return Refusal::None;
}

Refusal SetCommand(ObjectId creature, std::int32_t slot, std::int32_t command, std::int32_t parameter, ObjectId item)
{
    void *tactics = TacticsOf(creature);
    if (tactics == nullptr)
    {
        return Refusal::NotFollower;
    }
    const std::uintptr_t entry = EntryOf(creature, slot - 1);
    if (slot < 1 || entry == 0)
    {
        return Refusal::NoSuchSlot;
    }
    const auto target = static_cast<std::int32_t>(Read(entry, kTarget));
    const std::int32_t base = BaseOf(static_cast<std::int32_t>(Read(entry, kCondition)));
    bool offered = target != 0 && Holds(Commands(creature, target, base), command);
    if (offered && command == kUseAbility)
    {
        offered = Holds(Abilities(creature, target, base), parameter);
    }
    else if (offered && (command == kActivateMode || command == kDeactivateMode))
    {
        offered = Holds(ModalAbilities(creature), parameter);
    }
    else if (offered && command == kUseItem)
    {
        offered = Holds(Items(target), item);
        parameter = 0;
    }
    else if (offered && command == kJumpToTactic)
    {
        const std::int32_t count = Count(creature);
        offered = slot < count && parameter >= slot && parameter < count;
    }
    if (!offered)
    {
        return Refusal::NotOffered;
    }
    Write(entry, kCommand, command);
    Write(entry, kParameter, parameter);
    Write(entry, kItem, command == kUseItem ? static_cast<std::int64_t>(item) : -1);
    if (command == kUseItem)
    {
        auto refresh = messages::Make<messages::RefreshTacticsDataMessage>();
        refresh.fromTemplate = 1;
        messages::Send(tactics, refresh);
    }
    Changed(tactics);
    return Refusal::None;
}

Refusal SetOn(ObjectId creature, std::int32_t slot, bool on)
{
    void *tactics = TacticsOf(creature);
    if (tactics == nullptr)
    {
        return Refusal::NotFollower;
    }
    const std::uintptr_t entry = EntryOf(creature, slot - 1);
    if (slot < 1 || entry == 0)
    {
        return Refusal::NoSuchSlot;
    }
    Write(entry, kOn, on ? 1 : 0);
    Changed(tactics);
    return Refusal::None;
}

bool SetAiOn(ObjectId creature, bool on)
{
    void *tactics = TacticsOf(creature);
    if (tactics == nullptr)
    {
        return false;
    }
    Write(reinterpret_cast<std::uintptr_t>(tactics), kAiOn, on ? 1 : 0);
    return true;
}

bool LoadPreset(ObjectId creature, std::int32_t type, std::int32_t index)
{
    void *tactics = TacticsOf(creature);
    if (tactics == nullptr)
    {
        return false;
    }
    auto load = messages::Make<messages::LoadPresetMessage>();
    load.index = index;
    load.type = type;
    messages::Send(tactics, load);
    return true;
}

bool SavePreset(ObjectId creature, std::int32_t slot)
{
    void *tactics = TacticsOf(creature);
    if (tactics == nullptr)
    {
        return false;
    }
    auto save = messages::Make<messages::SavePresetMessage>();
    save.slot = slot;
    messages::Send(tactics, save);
    return true;
}

bool SetBehaviour(ObjectId creature, std::int32_t behaviour)
{
    void *tactics = TacticsOf(creature);
    if (tactics == nullptr)
    {
        return false;
    }
    auto set = messages::Make<messages::SetBehaviourMessage>();
    set.behaviour = behaviour;
    messages::Send(tactics, set);
    return true;
}

} // namespace dao::tactics
