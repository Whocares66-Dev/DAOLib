// The journal, read from the data tree by path.
//
// The scripts can ask about a plot whose name they know (GetPlotGUID,
// GetPartyPlotFlag) and cannot list what the journal holds. The journal
// screen can: the engine's Journal is an object of the data tree
// (dao/Data.h), and the screen asks for its lists by path.
//
// Read 2026-10-01 from Journal's and JournalEntry's constructors
// (0x006F6350, 0x006EBD60), which register their attributes by name, and
// from the paths in the journal's movie (journal.gfx). The tree's root has
// "Journal", with the lists PlotList and CompletedPlots (JournalEntry
// each), PlotGroups, CodexGroups, Conversations, PlotGivers and
// PlotDestinations, and the pointer ActivePlot. A list's member is its
// index: the walk of a path (0x005C6C80) hands the segment to the list's
// attribute, which reads it as a number (its slot 6 is the scripts'
// StringToInt), so the first quest's title is "Journal.PlotList.0.Name".
// A list says nothing of its length (as a number it is 0): the walk ends
// where an index has no entry. An entry has ResRef, ParentPlot, AreaTag,
// PlotGroup and OfferId as strings, Name and Description as strings of the
// talk table, already in the player's language, and Completed and Updated
// as bools.
//
// Seen in play 2026-10-02 by the Eluvian server, path by path, in Lyna's
// save at the ruins: four quests under way (the main plot, the ruins', and
// two parts of it, one with Completed set), the walk ending at the fifth
// index, the active one the main plot's, one group. A part of a quest that
// is done stays in PlotList with Completed set; CompletedPlots was empty.
// Completed and Updated read as text, "1" or "0", and as the number 0
// either way. A description marks its objective <emp>...</emp> and breaks
// lines with \r\n. The wrapper over them seen in play the same day, in
// Steam's game: the quests, titles and active one the journal screen shows.
//
// Read from the frame's thread (dao/Frame.h), where the engine reads its own.
#include "dao/Journal.h"

#include "dao/Data.h"

#include <cstddef>
#include <string_view>

namespace dao::journal
{

namespace
{

// More than any journal holds: a walk that has not ended by then is not
// one over a list.
constexpr std::size_t kMost = 2048;

std::wstring TextAt(const std::wstring &prefix, std::wstring_view member)
{
    const data::Attribute *attribute = data::Find(prefix + std::wstring(member));
    return attribute != nullptr ? attribute->Text() : std::wstring();
}

// Completed and Updated: bools that answer "1" or "0" as text and 0 as a
// number either way (seen in play).
bool FlagAt(const std::wstring &prefix, std::wstring_view member)
{
    const data::Attribute *attribute = data::Find(prefix + std::wstring(member));
    return attribute != nullptr && attribute->Text() == L"1";
}

// A description as the journal shows it: the objective between <emp> and
// </emp>, which is taken out as well, and the marks dropped.
void SplitDescription(const std::wstring &text, Quest &quest)
{
    constexpr std::wstring_view kOpen = L"<emp>";
    constexpr std::wstring_view kClose = L"</emp>";
    std::wstring plain;
    std::size_t at = 0;
    while (at < text.size())
    {
        const std::size_t open = text.find(kOpen, at);
        if (open == std::wstring::npos)
        {
            plain += text.substr(at);
            break;
        }
        plain += text.substr(at, open - at);
        const std::size_t start = open + kOpen.size();
        const std::size_t close = text.find(kClose, start);
        const std::wstring marked =
            text.substr(start, close == std::wstring::npos ? std::wstring::npos : close - start);
        plain += marked;
        if (quest.objective.empty())
        {
            quest.objective = marked;
        }
        at = close == std::wstring::npos ? text.size() : close + kClose.size();
    }
    quest.description = std::move(plain);
}

// The entry at a path that ends in '.', or nothing if there is none: every
// entry has a ResRef, so its absence says the path leads nowhere.
std::optional<Quest> QuestAt(const std::wstring &prefix)
{
    const data::Attribute *resource = data::Find(prefix + L"ResRef");
    if (resource == nullptr)
    {
        return std::nullopt;
    }
    Quest quest;
    quest.plot = resource->Text();
    quest.name = TextAt(prefix, L"Name");
    SplitDescription(TextAt(prefix, L"Description"), quest);
    quest.parent = TextAt(prefix, L"ParentPlot");
    quest.group = TextAt(prefix, L"PlotGroup");
    quest.area = TextAt(prefix, L"AreaTag");
    quest.offer = TextAt(prefix, L"OfferId");
    quest.completed = FlagAt(prefix, L"Completed");
    quest.updated = FlagAt(prefix, L"Updated");
    return quest;
}

std::vector<Quest> Quests(std::wstring_view list)
{
    std::vector<Quest> quests;
    for (std::size_t index = 0; index < kMost; ++index)
    {
        auto quest = QuestAt(std::wstring(list) + std::to_wstring(index) + L".");
        if (!quest)
        {
            break;
        }
        quests.push_back(std::move(*quest));
    }
    return quests;
}

} // namespace

std::vector<Quest> Current()
{
    return Quests(L"Journal.PlotList.");
}

std::vector<Quest> Completed()
{
    return Quests(L"Journal.CompletedPlots.");
}

std::optional<Quest> Active()
{
    return QuestAt(L"Journal.ActivePlot.");
}

std::vector<Group> Groups()
{
    std::vector<Group> groups;
    for (std::size_t index = 0; index < kMost; ++index)
    {
        const std::wstring prefix = L"Journal.PlotGroups." + std::to_wstring(index) + L".";
        const data::Attribute *resource = data::Find(prefix + L"ResRef");
        if (resource == nullptr)
        {
            break;
        }
        Group group;
        group.resource = resource->Text();
        group.name = TextAt(prefix, L"Name");
        if (const data::Attribute *priority = data::Find(prefix + L"Priority"))
        {
            group.priority = priority->Number();
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

} // namespace dao::journal
