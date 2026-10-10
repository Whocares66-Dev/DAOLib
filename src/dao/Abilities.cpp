// Abilities' names, cooldowns, lists and checks (dao/Abilities.h).
//
// Names, read 2026-09-30 from the scripts: an ability's row in
// TABLE_ABILITIES_SPELLS (1, ABI_base, every ability, by its id) has Label
// and NameStrRef, and the ability bar names an ability by NameStrRef
// through the talk tables (ui_h.nss). Both go through dao/Gda.h and
// dao/Tlk.h; nothing comes back for an id with no row, or before the
// tables are loaded.
//
// Cooldowns, read 2026-09-30 (dao/Addresses.h, "Cooldowns"). A creature
// has a record for each ability it has, which holds the seconds the
// cooldown was last set to, the seconds left, and the share of the one the
// other is. Each frame of game time the engine takes the frame's seconds
// from what is left and sets the share to what is left over what it was
// set to; the record is what the creature's ability list hands the
// interface, so the share is what a quickbar slot's sweep can show. The
// scripts' SetCooldown sets both numbers to the one time, so a long hold
// set that way shows a full sweep that never moves. GetCooldown and
// SetCooldown here read and set the two. Seen in play, 2026-10-01
// (examples/cooldown): Powerful Swings set to 800 s left of 1200 s read
// back 800 of 1200, ticked a second a second, and its quickbar slot drew a
// sweep about two thirds dark. The turn-based mod, the same day, wrote a
// three-turn wait as thirds of 3600 s: 3599.9 left drew the slot wholly
// dark, 2399 a third lit, 1199 two thirds lit, then ready.
#include "dao/Abilities.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Data.h"
#include "dao/Gda.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/Tlk.h"
#include "dao/core/Msvc8.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <mutex>

namespace dao::abilities
{

namespace
{
constexpr auto kTable = static_cast<script::Table>(script::constants::TABLE_ABILITIES_SPELLS);

// A cooldown's record (dao/Addresses.h, "Cooldowns").
constexpr std::size_t kTotal = 0x20;
constexpr std::size_t kRemaining = 0x24; // an attribute
constexpr std::size_t kShare = 0x34;     // an attribute
constexpr std::size_t kAttributeValue = 0xC;

void *Stats(ObjectId creature)
{
    CClientExoApp *app = CClientExoApp::Get();
    const CCCreature *object = app != nullptr ? app->GetCreatureById(creature) : nullptr;
    return object != nullptr ? object->Stats() : nullptr;
}

void *Manager(void *stats)
{
    using CooldownsFn = void *(__fastcall *)(void *);
    return reinterpret_cast<CooldownsFn>(Module::Address(addr::kCCCreatureStats_Cooldowns))(stats);
}

std::uintptr_t Record(void *stats, std::int32_t ability, std::wstring_view itemTag)
{
    void *manager = Manager(stats);
    if (manager == nullptr)
    {
        return 0;
    }
    WString tag(itemTag);
    using FindFn = std::uintptr_t(__fastcall *)(void *, int, std::int32_t, const core::WStringRep *);
    return reinterpret_cast<FindFn>(Module::Address(addr::kCooldownManager_Find))(manager, 0, ability, tag.Engine());
}
constexpr std::size_t kTicking = 0x10; // in the manager: the records that tick, a std::list
constexpr std::size_t kItemTag = 4;    // in a record: a std::wstring
constexpr std::size_t kMostTag = 256;  // characters read of a tag

// An engine string in the process, copied out through reads that may fail.
std::optional<std::wstring> ReadText(std::uintptr_t at)
{
    const auto rep = TryRead<core::WStringRep>(at);
    if (!rep || rep->size > kMostTag)
    {
        return std::nullopt;
    }
    if (rep->capacity <= core::WStringRep::kInlineCapacity)
    {
        return std::wstring(rep->data.buffer, rep->size);
    }
    std::wstring text(rep->size, L'\0');
    if (!TryCopy(text.data(), reinterpret_cast<std::uintptr_t>(rep->data.pointer), rep->size * sizeof(wchar_t)))
    {
        return std::nullopt;
    }
    return text;
}

// The scripts' SetCooldown, watched (OnCooldownSet). __thiscall on the
// stats (ability, float seconds, const std::wstring *tag), `ret 0xC`.
using SetCooldownFn = void(__fastcall *)(void *, int, std::int32_t, float, const core::WStringRep *);
constexpr std::size_t kStatsCreature = 0x5C; // in the stats: the creature they belong to
std::mutex g_setLock;
std::vector<std::function<void(const CooldownSet &)>> g_setObservers;
Detour<SetCooldownFn> g_setCooldown;
thread_local bool t_ourSet = false; // in this library's own SetCooldown

// The creature the stats belong to, if it reads and its own Stats() are
// these.
ObjectId CreatureOf(void *stats)
{
    const auto owner = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(stats) + kStatsCreature);
    if (!owner || *owner == 0)
    {
        return kInvalidObject;
    }
    const auto back = TryRead<std::uintptr_t>(*owner + CCCreature::kStats);
    const auto id = TryRead<ObjectId>(*owner + 4 + kAttributeValue);
    return back && *back == reinterpret_cast<std::uintptr_t>(stats) && id ? *id : kInvalidObject;
}

void __fastcall SetCooldownHook(void *stats, int edx, std::int32_t ability, float seconds, const core::WStringRep *tag)
{
    g_setCooldown.Original()(stats, edx, ability, seconds, tag);
    if (t_ourSet)
    {
        return;
    }
    std::vector<std::function<void(const CooldownSet &)>> observers;
    {
        const std::lock_guard lock(g_setLock);
        observers = g_setObservers;
    }
    const ObjectId creature = observers.empty() ? kInvalidObject : CreatureOf(stats);
    if (creature == kInvalidObject)
    {
        return;
    }
    CooldownSet set;
    set.creature = creature;
    set.ability = ability;
    set.seconds = seconds;
    set.itemTag = tag != nullptr ? std::wstring(tag->View()) : std::wstring();
    const std::uintptr_t record = Record(stats, ability, set.itemTag);
    const auto total = record != 0 ? TryRead<float>(record + kTotal) : std::nullopt;
    const auto remaining = record != 0 ? TryRead<float>(record + kRemaining + kAttributeValue) : std::nullopt;
    if (total && remaining)
    {
        set.cooldown = Cooldown{*remaining, *total};
        set.took = std::fabs(*total - std::max(seconds, 0.0F)) < 1e-3F;
    }
    for (const auto &observer : observers)
    {
        detail::Guard("a cooldown callback", [&] { observer(set); });
    }
}

} // namespace

// Read 2026-10-08 (DAOLib #41): the cooldown manager's Set (0x0071D630) has
// one caller, CCCreatureStats::SetCooldown, and that one, the scripts'
// SetCooldown command; so every ability's cooldown the game starts passes
// here, the scripts' Ability_SetCooldown as a cast begins among them,
// whatever command or none is running. The stats keep their creature at
// +0x5C (their constructor's argument, which the creature passes as itself,
// 0x006B4070), checked each time against the creature's own Stats(). An
// item's cooldown comes the same way: its record is made, like every
// record, in the manager's first list (0x0071D6D0, the record's only
// maker), so the drink's set takes while the party still holds the item;
// once the last one is used, its record leaves that list (see SetCooldown).
//
// Seen in play 2026-10-09 (the turn-based mod, the Cultist Overseer's
// fight): 87 cooldowns, every one through here, the Overseer's sustained
// spells at 1.5 s as each cast began, and a mana potion's 5 s with its
// item's tag, `took` true.
void OnCooldownSet(std::function<void(const CooldownSet &)> observer)
{
    static std::once_flag once;
    std::call_once(once, [] { g_setCooldown.Attach(addr::kCCCreatureStats_SetCooldown, &SetCooldownHook); });
    const std::lock_guard lock(g_setLock);
    g_setObservers.push_back(std::move(observer));
}

// Read 2026-10-06 (DAOLib #38): the cooldown manager keeps, at its +0x10,
// a std::list of the records that tick (core::ListValues), which its Tick
// (0x0071C840) walks each frame, taking the time from each and dropping
// one that has run out (0x0071C190). So the list is exactly what is
// cooling, an item's ability's record among the rest: the ability's own
// use, a mode that would not stand, a script's SetCooldown, an item's. A
// cooldown set with SetCooldown joins it as the engine's do.
std::vector<RunningCooldown> Cooldowns(ObjectId creature)
{
    std::vector<RunningCooldown> running;
    void *stats = Stats(creature);
    void *manager = stats != nullptr ? Manager(stats) : nullptr;
    if (manager == nullptr)
    {
        return running;
    }
    const auto read = [](std::uint32_t at) { return TryRead<std::uint32_t>(at); };
    for (const std::uint32_t record : core::ListValues(reinterpret_cast<std::uintptr_t>(manager) + kTicking, read))
    {
        const auto ability = TryRead<std::int32_t>(record);
        const auto total = TryRead<float>(record + kTotal);
        const auto remaining = TryRead<float>(record + kRemaining + kAttributeValue);
        auto tag = ReadText(record + kItemTag);
        if (ability && total && remaining && tag)
        {
            running.push_back({*ability, std::move(*tag), Cooldown{*remaining, *total}});
        }
    }
    return running;
}

std::optional<Cooldown> GetCooldown(ObjectId creature, std::int32_t ability, std::wstring_view itemTag)
{
    void *stats = Stats(creature);
    const std::uintptr_t record = stats != nullptr ? Record(stats, ability, itemTag) : 0;
    const auto total = record != 0 ? TryRead<float>(record + kTotal) : std::nullopt;
    const auto remaining = record != 0 ? TryRead<float>(record + kRemaining + kAttributeValue) : std::nullopt;
    if (!total || !remaining)
    {
        return std::nullopt;
    }
    return Cooldown{*remaining, *total};
}

bool SetCooldown(ObjectId creature, std::int32_t ability, float remaining, float total, std::wstring_view itemTag)
{
    remaining = std::max(remaining, 0.0F);
    void *stats = Stats(creature);
    if (stats == nullptr || (remaining > 0 && total <= 0) || Record(stats, ability, itemTag) == 0)
    {
        return false;
    }
    // The engine's own, which sets both numbers to the one time and has the
    // record tick; then what is left and its share, as its tick sets them.
    WString tag(itemTag);
    using SetFn = void(__fastcall *)(void *, int, std::int32_t, float, const core::WStringRep *);
    t_ourSet = true; // not told to OnCooldownSet's observers
    reinterpret_cast<SetFn>(Module::Address(addr::kCCCreatureStats_SetCooldown))(
        stats, 0, ability, remaining > 0 ? total : 0.0F, tag.Engine());
    t_ourSet = false;
    const std::uintptr_t record = Record(stats, ability, itemTag);
    if (record == 0)
    {
        return false;
    }
    // The engine's Set acts on the records in the manager's first list. An
    // item's record is there while the party holds the item, and Set takes
    // on it then; once the last one is used, the record leaves that list
    // and only the ticking list holds it (a poultice's, shared by the
    // group), and Set leaves it as it was. So the total is written here as
    // Set writes it, a plain float, for that case (DAOLib #38; seen in play
    // 2026-10-06, a poultice's hold read 2895 s left of 5).
    //
    // Seen in play 2026-10-10 (DAOLib #46, Wynne at Fort Drakon): the
    // scripts' set on a Health Poultice and a Lesser Injury Kit, the
    // party's last of each, took (the record read 5.00 of 5.00); a moment
    // later Record found none, so a set after the last use returns false.
    // The example passed no item tag, so a set on an item still held, with
    // its tag, is not yet seen.
    *reinterpret_cast<float *>(record + kTotal) = remaining > 0 ? total : 0.0F;
    auto *left = reinterpret_cast<data::Attribute *>(record + kRemaining);
    auto *share = reinterpret_cast<data::Attribute *>(record + kShare);
    return left->Set(remaining) && share->Set(remaining > 0 ? std::min(remaining / total, 1.0F) : 0.0F);
}

std::optional<std::wstring> Label(std::int32_t ability)
{
    return gda::GetString(kTable, ability, "Label");
}

std::optional<std::wstring> Name(std::int32_t ability)
{
    const auto strref = gda::GetInt(kTable, ability, "NameStrRef");
    if (strref && *strref > 0)
    {
        if (std::wstring text = tlk::String(static_cast<std::uint32_t>(*strref)); !text.empty())
        {
            return text;
        }
    }
    return Label(ability);
}

// Read 2026-10-01 from the scripts' GetAbilityCount (0x00668A20, on to
// 0x0071A2F0), which counts them: the creature's stats keep four lists of
// ids, one a kind in the kinds' order, at +0x150 and each 0x10 on (the
// list of a kind is found by 0x007199D0). Seen in play 2026-10-01 by the
// Eluvian server: a level 1 rogue's lists (4 talents, no spells, 6 skills,
// 2 items) and a warrior's (7, 0, 3, 2) each came to the 12 that
// GetAbilityCount answers. The lists hold the hidden abilities too (the
// class's, the humanoid trait), the item list is the usable items'
// abilities (a poultice, elfroot), and a skill's ranks are an id each.
std::vector<std::int32_t> Known(ObjectId creature, Kind kind)
{
    constexpr std::size_t kLists = 0x150; // in the stats: four vectors of ids, a kind each
    constexpr std::size_t kMost = 4096;   // more than any creature has; a list longer is not one
    std::vector<std::int32_t> known;
    const void *stats = Stats(creature);
    if (stats == nullptr)
    {
        return known;
    }
    const auto first = kind == Kind::All ? std::size_t{1} : static_cast<std::size_t>(kind);
    const auto last = kind == Kind::All ? std::size_t{4} : first;
    if (first < 1 || last > 4)
    {
        return known;
    }
    for (std::size_t which = first; which <= last; ++which)
    {
        const std::uintptr_t list =
            reinterpret_cast<std::uintptr_t>(stats) + kLists + ((which - 1) * sizeof(core::VectorRep<std::int32_t>));
        const auto begin = TryRead<std::uintptr_t>(list + offsetof(core::VectorRep<std::int32_t>, first));
        const auto end = TryRead<std::uintptr_t>(list + offsetof(core::VectorRep<std::int32_t>, last));
        if (!begin || !end || *begin == 0 || *end < *begin || (*end - *begin) / sizeof(std::int32_t) > kMost)
        {
            continue;
        }
        for (std::uintptr_t at = *begin; at < *end; at += sizeof(std::int32_t))
        {
            if (const auto id = TryRead<std::int32_t>(at))
            {
                known.push_back(*id);
            }
        }
    }
    return known;
}

std::wstring Use::Reason() const
{
    return reason > 0 ? tlk::String(static_cast<std::uint32_t>(reason)) : std::wstring{};
}

// Read 2026-10-03 (dao/Addresses.h, CanUseAbility): the engine's one check,
// which the quickbar asks for each button and the player's order asks
// before it issues a command, and which answers with the reason a string
// reference. It looks, in order, at an item's ability's item, the active
// modal abilities (usable, to turn off), being disabled by effects, the
// cost, the ability's conditions (a weapon, a shield), its cooldown, and the
// ability being cast already. The scripts' IsAbilityAvailable asks whether
// the ability can be learned, not used: true for a 40-stamina Mighty Blow at
// 12 stamina, whose order then did nothing (Eluvian, 2026-10-03). One path
// of the check gives a reason of 0, which is answered as -1.
//
// The scripts check the cost again when the command runs (ability_core's
// Ability_CostCheck) a little differently: rounded, by the difficulty's
// modifier for some ranks, and a modal ability's upkeep against the total
// rather than its cost against what is left. So a modal ability this calls
// usable can still fail. Seen in play 2026-10-03 through the Eluvian
// server, on the Tower Guard (a bow, no shield) at full stamina, 115: Shield
// Bash and Shield Block not usable, "No shield equipped"; Crippling Shot,
// Pinning Shot, Shattering Shot and Taunt usable, costing 28.775, 23.02,
// 28.775 and 46.04, the table's 25, 20, 25 and 40 times 1.151 (his
// fatigue). At 12 stamina every one was not usable, "Insufficient stamina".
// IsAbilityAvailable had said yes to all.
std::optional<Use> CanUse(ObjectId creature, std::int32_t ability, std::wstring_view itemTag)
{
    void *stats = Stats(creature);
    if (stats == nullptr)
    {
        return std::nullopt;
    }
    // As the quickbar asks: the reason at -1 first.
    Use use;
    WString tag(itemTag);
    using Fn = bool(__fastcall *)(void *, int, std::int32_t, std::int32_t *, const core::WStringRep *);
    use.usable = reinterpret_cast<Fn>(Module::Address(addr::kCCCreatureStats_CanUseAbility))(stats, 0, ability,
                                                                                             &use.reason, tag.Engine());
    if (use.usable || use.reason == 0)
    {
        use.reason = -1;
    }
    return use;
}

// The ability table's cost times one plus the creature's fatigue in
// hundredths (property 41's modifier: armour's, a sustained ability's), as
// CanUse's check reckons it.
std::optional<float> Cost(ObjectId creature, std::int32_t ability)
{
    constexpr std::int32_t kFatigue = 41; // properties.gda's "Fatique", an attribute
    CClientExoApp *app = CClientExoApp::Get();
    const CCCreature *object = app != nullptr ? app->GetCreatureById(creature) : nullptr;
    const auto cost = gda::GetFloat(kTable, ability, "cost");
    if (object == nullptr || !cost)
    {
        return std::nullopt;
    }
    const float fatigue = object->Property(kFatigue, script::PropertyValue::Modifier).value_or(0.0F);
    return *cost * (1.0F + (fatigue * 0.01F));
}

} // namespace dao::abilities
