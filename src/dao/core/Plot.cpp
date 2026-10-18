#include "dao/core/Plot.h"

#include <algorithm>

namespace dao::core::plot
{

namespace
{

constexpr std::uint32_t kFlags = 13000;
constexpr std::uint32_t kFlagId = 13001;
constexpr std::uint32_t kFlagName = 13002;
constexpr std::uint32_t kFlagReward = 13003;
constexpr std::uint32_t kFlagJournal = 13004;
constexpr std::uint32_t kFlagEndsPlot = 13005;
constexpr std::uint32_t kFlagMultiReward = 13006;
constexpr std::uint32_t kGuid = 13007;
constexpr std::uint32_t kName = 13008;
constexpr std::uint32_t kScript = 13009;
constexpr std::uint32_t kPriority = 13010;
constexpr std::uint32_t kStored = 13011; // PlotFlags1; 2 to 4 follow
constexpr std::uint32_t kJournalImage = 13015;
constexpr std::uint32_t kPlots = 13016;
constexpr std::uint32_t kParent = 13017;
constexpr std::uint32_t kFlagAreaLocationTag = 13018;
constexpr std::uint32_t kAssists = 13019;
constexpr std::uint32_t kAssistTag = 13020;
constexpr std::uint32_t kAssistAdvances = 13021;
constexpr std::uint32_t kEntryType = 13022;
constexpr std::uint32_t kAllowPausing = 13023;
constexpr std::uint32_t kFlagOfferId = 13024;
constexpr std::uint32_t kParentGuid = 13025;
constexpr std::uint32_t kTemplateResRef = 3;
constexpr std::int32_t kBitsAWord = 32;

Flag FlagOf(const gff4::Struct &in)
{
    Flag flag{.id = in.Get<std::int32_t>(kFlagId).value_or(0),
              .name = in.String(kFlagName).value_or(L""),
              .reward = in.Get<std::int32_t>(kFlagReward).value_or(0),
              .journal = in.Tlk(kFlagJournal).value_or(gff4::TlkString{}),
              .endsPlot = in.Get<std::uint8_t>(kFlagEndsPlot).value_or(0),
              .multiReward = in.Get<std::uint8_t>(kFlagMultiReward).value_or(0),
              .areaLocationTag = in.String(kFlagAreaLocationTag).value_or(L""),
              .offerId = in.String(kFlagOfferId).value_or(L""),
              .assists = {}};
    for (const gff4::Struct &assist : in.Structs(kAssists))
    {
        flag.assists.push_back({.tag = assist.String(kAssistTag).value_or(L""),
                                .advancesPlot = assist.Get<std::uint8_t>(kAssistAdvances).value_or(0)});
    }
    return flag;
}

} // namespace

const Flag *Plot::Find(std::int32_t id) const
{
    const auto found = std::ranges::find_if(flags, [&](const Flag &f) { return f.id == id; });
    return found != flags.end() ? &*found : nullptr;
}

bool Plot::Stored(std::int32_t id) const
{
    if (id < 0 || id >= kBitsAWord * static_cast<std::int32_t>(stored.size()))
    {
        return false;
    }
    return ((stored[static_cast<std::size_t>(id / kBitsAWord)] >> static_cast<unsigned>(id % kBitsAWord)) & 1U) != 0;
}

std::optional<Plot> PlotOf(const gff4::File &file)
{
    const std::optional<gff4::Struct> root = file.Root();
    if (file.Type() != "PLTS" || !root)
    {
        return std::nullopt;
    }
    const std::vector<gff4::Struct> plots = root->Structs(kPlots);
    if (plots.empty())
    {
        return std::nullopt;
    }
    const gff4::Struct &in = plots.front();
    Plot plot{.resref = in.String(kTemplateResRef).value_or(L""),
              .guid = in.String(kGuid).value_or(L""),
              .name = in.Tlk(kName).value_or(gff4::TlkString{}),
              .script = in.String(kScript).value_or(L""),
              .priority = in.Get<std::int32_t>(kPriority).value_or(0),
              .stored = {},
              .journalImage = in.String(kJournalImage).value_or(L""),
              .parent = in.String(kParent).value_or(L""),
              .parentGuid = in.String(kParentGuid).value_or(L""),
              .entryType = in.Get<std::int32_t>(kEntryType).value_or(0),
              .allowPausing = in.Get<std::uint8_t>(kAllowPausing).value_or(0),
              .flags = {}};
    for (std::uint32_t i = 0; i < plot.stored.size(); ++i)
    {
        plot.stored[i] = in.Get<std::uint32_t>(kStored + i).value_or(0);
    }
    for (const gff4::Struct &flag : in.Structs(kFlags))
    {
        plot.flags.push_back(FlagOf(flag));
    }
    return plot;
}

} // namespace dao::core::plot
