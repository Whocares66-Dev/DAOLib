// A creature's level-up without the screen (dao/LevelUp.h).
//
// Notes on the level-up: read 2026-10-24 (DAOLib #259) in the GOG exe and the toolset's scripts.
// The screen holds no rules of its own. A creature's points are its properties 34 (attributes),
// 35 (skills), 36 (talents and spells) and 38 (specializations), granted with its level
// (Levelup_SetReadyToLevelUp, sys_autoscale_h.nss). What it is offered is AbilityChains::Find
// (0x0062E9C0, cdecl (stats, kind, mode), mode 2 for level-up), which the screen's
// FindAbilityChains handlers reach (through 0x00719A90) and which fills the party data's lists
// (stats +0x1B4; talents +0xB0, spells +0xCC, skills +0xE8, each a SmartData attribute holding a
// vector at +0xC, its begin at +4 and end at +8), each entry an AbilityGUIInfo whose ability
// record is the attribute value at +0x64 (the record's id the value at its +0x64) and whose
// display state is the value at +0x94: 1 have, 2 learnable (CCCreatureStats::IsAbilityAvailable,
// 0x0071BEF0), 3 locked. The builder ends by notifying the lists' observers, so a screen open on
// the creature redraws. A pick is the screen's CharGenAssignAbility, whose handler (0x007FCE60)
// ignores the screen and raises the module's EVENT_TYPE_CHARGEN_ASSIGN_ABILITIES (51), object 0
// the creature, int 0 the ability, int 1 0 for a skill, talent or spell or 1 for a
// specialization; sys_chargen.nss then spends the point (35 for a skill, 36 else, 38 for a
// specialization) and adds the ability, checking nothing, so the checks are here. An attribute is
// event 50 (int 0 the attribute, int 1 the points; 34 spent), Auto Level Up 56 (AL_DoAutoLevelUp,
// which never spends a specialization's point), Done 59 (int 0 the mode, 1 for level-up, int 1 1),
// which sets the tactics' slots, SetCanLevelUp and the regeneration.
//
// Seen in play, 2026-10-24, on Leliana in Leliana's Song (level 10, an attribute point and two
// talent points; examples/levelup): her talents' page offered 10 learnable, her spells' none (a
// rogue); a locked talent was refused Locked, a held one Held; Dual-Weapon Finesse taken, HasAbility
// then 1 and her talent points 1, the talent after it in its chain learnable; strength raised by 1
// and refused by 50 (NoPoints); Auto Level Up spent the last point, and Finish left her unable to
// level up. With the level-up screen open, a pick through Take showed on it: its data read the talent
// had and the points 1, and its icons (the Eluvian server's levelup offers) the talent had and the
// next learnable; the screen closed without its Done kept the pick. TakeSpecialization was not
// tried in play then: no creature to hand had a specialization point. Given one by script
// (property 38), with only the availability test, she could take Arcane Warrior, a mage's: it
// passed IsAbilityAvailable for her, which is why the screen's own tests of the row and its
// achievement are here. With them: Arcane Warrior refused NotOffered, Bard (hers) Held, Ranger
// taken (HasAbility 1, the point spent), Duelist and Assassin then NoPoints, Ranger again Held. Her
// profile had every rogue's specialization unlocked, so a locked achievement was not seen refused.
// Specializations, the same day: Leliana's were Assassin, Bard (had), Ranger and Duelist, in that
// order, as the open level-up screen listed them (the Eluvian server's levelup offers); a rogue of
// the main campaign at level 16, the same four, Ranger had; and in Awakening (Vigil's Keep, the
// hero a rogue), six, Awakening's Legionnaire Scout (401004) and Shadow (401005) after the four, as
// the screen listed them.
#include "dao/LevelUp.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Gda.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/generated/ScriptApi.h"

#include <array>

namespace dao::levelup
{

namespace
{

namespace api = script::api;

constexpr std::size_t kPartyData = 0x1B4;  // in the creature's stats: the party data
constexpr std::size_t kListBegin = 0x10;   // in a list's attribute: the vector's begin (+0xC, then +4)
constexpr std::size_t kInfoAbility = 0x64; // in an AbilityGUIInfo: its ability record
constexpr std::size_t kRecordId = 0x64;    // in an ability record: its id
constexpr std::size_t kInfoState = 0x94;   // in an AbilityGUIInfo: its display state
constexpr std::int32_t kLevelUp = 2;       // the builder's mode for the level-up screen
constexpr std::size_t kMostOffers = 1024;

constexpr std::int32_t kAttributePoints = 34;
constexpr std::int32_t kSkillPoints = 35;
constexpr std::int32_t kTalentPoints = 36;
constexpr std::int32_t kSpecializationPoints = 38;
constexpr std::int32_t kClass = 27; // the creature's class, a row of Classes, as the movie reads it

constexpr std::int32_t kAssignAttributes = 50;
constexpr std::int32_t kAssignAbilities = 51;
constexpr std::int32_t kAutoLevel = 56;
constexpr std::int32_t kEnd = 59;

std::size_t ListOf(Kind kind)
{
    switch (kind)
    {
    case Kind::Talents:
        return 0xB0;
    case Kind::Spells:
        return 0xCC;
    default:
        return 0xE8;
    }
}

CCCreature *CreatureOf(ObjectId creature)
{
    CClientExoApp *app = CClientExoApp::Get();
    return app != nullptr ? app->GetCreatureById(creature) : nullptr;
}

std::int32_t PointsIn(const CCCreature &creature, std::int32_t property)
{
    return static_cast<std::int32_t>(creature.Property(property).value_or(0));
}

// Raises one of the screen's events on the module, at once, as its handlers do.
void Raise(std::int32_t type, ObjectId creature, std::int32_t first, std::int32_t second)
{
    auto event = api::Event(type);
    if (event)
    {
        event = api::SetEventObject(*event.get(), 0, creature);
    }
    if (event)
    {
        event = api::SetEventInteger(*event.get(), 0, first);
    }
    if (event)
    {
        event = api::SetEventInteger(*event.get(), 1, second);
    }
    const ObjectId module = api::GetModule();
    if (event && module != kInvalidObject)
    {
        api::SignalEvent(module, *event.get(), 1);
    }
}

bool Available(const CCCreature &creature, std::int32_t ability)
{
    using Fn = bool(__fastcall *)(void *stats, int edx, std::int32_t ability);
    void *stats = creature.Stats();
    return stats != nullptr &&
           reinterpret_cast<Fn>(Module::Address(addr::kCCCreatureStats_IsAbilityAvailable))(stats, 0, ability);
}

// Which page shows the ability, and how.
std::optional<std::pair<Kind, Offer>> Find(ObjectId creature, std::int32_t ability)
{
    for (const Kind kind : {Kind::Skills, Kind::Talents, Kind::Spells})
    {
        for (const Offer &offer : Offers(creature, kind))
        {
            if (offer.ability == ability)
            {
                return std::pair{kind, offer};
            }
        }
    }
    return std::nullopt;
}

// Whether the screen shows the kind for the creature: skills always; spells for a class that uses
// mana, talents for one that does not (AbilityScene.GetAbilityType, abilities.gfx: Classes'
// BUsesMana of the creature's class, property 27).
bool Shown(const CCCreature &creature, Kind kind)
{
    if (kind == Kind::Skills)
    {
        return true;
    }
    const auto row = static_cast<std::int32_t>(creature.Property(kClass).value_or(0));
    const bool mana = gda::GetInt(script::Table::RulesClasses, row, "BUsesMana").value_or(0) != 0;
    return mana == (kind == Kind::Spells);
}

// A specialization the screen offers: its class's row of Classes and the ability it gives.
struct Specialization
{
    std::int32_t row = 0;
    std::int32_t ability = 0;
};

// The specializations the screen offers the creature's class, in its order: the rows of Classes,
// by their index, for players (PlayerClass 1) whose BaseClass is the creature's base class (its
// class's own BaseClass, or the class itself where that is 0), each with its StartingAbility1
// (ClassSpecializationScene.GrabSpecializations, levelup.gfx). An add-in's rows are among them.
std::vector<Specialization> SpecializationsOf(const CCCreature &creature)
{
    using script::Table;
    const auto table = static_cast<std::int32_t>(Table::RulesClasses);
    const auto own = static_cast<std::int32_t>(creature.Property(kClass).value_or(0));
    const std::int32_t parent = gda::GetInt(Table::RulesClasses, own, "BaseClass").value_or(0);
    const std::int32_t base = parent != 0 ? parent : own;
    std::vector<Specialization> found;
    const std::int32_t rows = api::GetM2DARows(table);
    for (std::int32_t index = 0; index < rows; ++index)
    {
        const std::int32_t row = api::GetM2DARowIdFromRowIndex(table, index);
        if (gda::GetInt(Table::RulesClasses, row, "PlayerClass").value_or(0) == 1 &&
            gda::GetInt(Table::RulesClasses, row, "BaseClass").value_or(0) == base)
        {
            found.push_back({row, gda::GetInt(Table::RulesClasses, row, "StartingAbility1").value_or(0)});
        }
    }
    return found;
}

// The class a specialization's ability makes the creature, if the screen offers it.
std::optional<std::int32_t> SpecializationRow(const CCCreature &creature, std::int32_t ability)
{
    for (const Specialization &offered : SpecializationsOf(creature))
    {
        if (offered.ability == ability)
        {
            return offered.row;
        }
    }
    return std::nullopt;
}

// Whether the specialization's class is unlocked: the game's own record of achievements, which a
// specialization's manual or trainer unlocks (RW_UnlockSpecializationTrainer, sys_rewards_h.nss)
// and which the screen tests by the row's Achievement (HasAchievement, 0x007163D0, the same test
// as the scripts' GetHasAchievement).
bool Unlocked(std::int32_t row)
{
    const auto achievement = gda::GetString(script::Table::RulesClasses, row, "Achievement");
    return achievement && api::GetHasAchievement(*achievement) != 0;
}

} // namespace

std::vector<Offer> Offers(ObjectId creature, Kind kind)
{
    std::vector<Offer> offers;
    const CCCreature *found = CreatureOf(creature);
    if (found != nullptr && !Shown(*found, kind))
    {
        return offers;
    }
    void *stats = found != nullptr ? found->Stats() : nullptr;
    const auto party = stats != nullptr
                           ? TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(stats) + kPartyData).value_or(0)
                           : 0;
    if (party == 0)
    {
        return offers;
    }
    using FindFn = void(__cdecl *)(void *stats, std::int32_t kind, std::int32_t mode);
    reinterpret_cast<FindFn>(Module::Address(addr::kAbilityChains_Find))(stats, static_cast<std::int32_t>(kind),
                                                                         kLevelUp);
    const std::uintptr_t list = party + ListOf(kind);
    const auto begin = TryRead<std::uintptr_t>(list + kListBegin).value_or(0);
    const auto end = TryRead<std::uintptr_t>(list + kListBegin + 4).value_or(0);
    for (std::uintptr_t at = begin; begin != 0 && at < end && offers.size() < kMostOffers; at += 4)
    {
        const auto info = TryRead<std::uintptr_t>(at).value_or(0);
        const auto record = info != 0 ? TryRead<std::uintptr_t>(info + kInfoAbility).value_or(0) : 0;
        const auto id = record != 0 ? TryRead<std::int32_t>(record + kRecordId) : std::nullopt;
        const auto state = info != 0 ? TryRead<std::int32_t>(info + kInfoState) : std::nullopt;
        if (id && state)
        {
            offers.push_back({*id, static_cast<State>(*state)});
        }
    }
    return offers;
}

std::vector<Offer> Specializations(ObjectId creature)
{
    std::vector<Offer> offers;
    const CCCreature *found = CreatureOf(creature);
    if (found == nullptr || found->Stats() == nullptr)
    {
        return offers;
    }
    for (const Specialization &offered : SpecializationsOf(*found))
    {
        State state = State::Locked;
        if (api::HasAbility(creature, offered.ability) != 0)
        {
            state = State::Have;
        }
        else if (Unlocked(offered.row) && Available(*found, offered.ability))
        {
            state = State::Learnable;
        }
        offers.push_back({offered.ability, state});
    }
    return offers;
}

std::optional<Points> PointsOf(ObjectId creature)
{
    const CCCreature *found = CreatureOf(creature);
    if (found == nullptr || found->Stats() == nullptr)
    {
        return std::nullopt;
    }
    return Points{PointsIn(*found, kAttributePoints), PointsIn(*found, kSkillPoints), PointsIn(*found, kTalentPoints),
                  PointsIn(*found, kSpecializationPoints)};
}

Refusal Take(ObjectId creature, std::int32_t ability)
{
    const CCCreature *found = CreatureOf(creature);
    if (found == nullptr || found->Stats() == nullptr)
    {
        return Refusal::NoCreature;
    }
    const auto offer = Find(creature, ability);
    if (!offer)
    {
        return Refusal::NotOffered;
    }
    if (offer->second.state == State::Have)
    {
        return Refusal::Held;
    }
    if (offer->second.state != State::Learnable)
    {
        return Refusal::Locked;
    }
    if (PointsIn(*found, offer->first == Kind::Skills ? kSkillPoints : kTalentPoints) < 1)
    {
        return Refusal::NoPoints;
    }
    Raise(kAssignAbilities, creature, ability, 0);
    Offers(creature, offer->first); // an open screen redraws the page
    return Refusal::None;
}

Refusal TakeSpecialization(ObjectId creature, std::int32_t ability)
{
    const CCCreature *found = CreatureOf(creature);
    if (found == nullptr || found->Stats() == nullptr)
    {
        return Refusal::NoCreature;
    }
    const auto row = SpecializationRow(*found, ability);
    if (!row)
    {
        return Refusal::NotOffered;
    }
    if (api::HasAbility(creature, ability) != 0)
    {
        return Refusal::Held;
    }
    if (!Unlocked(*row) || !Available(*found, ability))
    {
        return Refusal::Locked;
    }
    if (PointsIn(*found, kSpecializationPoints) < 1)
    {
        return Refusal::NoPoints;
    }
    Raise(kAssignAbilities, creature, ability, 1);
    return Refusal::None;
}

Refusal RaiseAttribute(ObjectId creature, std::int32_t attribute, std::int32_t points)
{
    const CCCreature *found = CreatureOf(creature);
    if (found == nullptr || found->Stats() == nullptr)
    {
        return Refusal::NoCreature;
    }
    if (attribute < 1 || attribute > 6 || points < 1)
    {
        return Refusal::NotOffered;
    }
    if (PointsIn(*found, kAttributePoints) < points)
    {
        return Refusal::NoPoints;
    }
    Raise(kAssignAttributes, creature, attribute, points);
    return Refusal::None;
}

void AutoLevelUp(ObjectId creature)
{
    if (CreatureOf(creature) != nullptr)
    {
        Raise(kAutoLevel, creature, 0, 0);
    }
}

void Finish(ObjectId creature)
{
    if (CreatureOf(creature) != nullptr)
    {
        Raise(kEnd, creature, 1, 1);
    }
}

} // namespace dao::levelup
