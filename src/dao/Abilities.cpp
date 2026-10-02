#include "dao/Abilities.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Data.h"
#include "dao/Gda.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/Tlk.h"
#include "dao/core/Msvc8.h"

#include <algorithm>
#include <cstddef>

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

std::uintptr_t Record(void *stats, std::int32_t ability, std::wstring_view itemTag)
{
    using CooldownsFn = void *(__fastcall *)(void *);
    void *manager = reinterpret_cast<CooldownsFn>(Module::Address(addr::kCCCreatureStats_Cooldowns))(stats);
    if (manager == nullptr)
    {
        return 0;
    }
    WString tag(itemTag);
    using FindFn = std::uintptr_t(__fastcall *)(void *, int, std::int32_t, const core::WStringRep *);
    return reinterpret_cast<FindFn>(Module::Address(addr::kCooldownManager_Find))(manager, 0, ability, tag.Engine());
}
} // namespace

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
    reinterpret_cast<SetFn>(Module::Address(addr::kCCCreatureStats_SetCooldown))(
        stats, 0, ability, remaining > 0 ? total : 0.0F, tag.Engine());
    const std::uintptr_t record = Record(stats, ability, itemTag);
    if (record == 0)
    {
        return false;
    }
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
            reinterpret_cast<std::uintptr_t>(stats) + kLists + (which - 1) * sizeof(core::VectorRep<std::int32_t>);
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

} // namespace dao::abilities
