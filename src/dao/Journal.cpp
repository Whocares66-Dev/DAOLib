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

bool FlagAt(const std::wstring &prefix, std::wstring_view member)
{
    const data::Attribute *attribute = data::Find(prefix + std::wstring(member));
    return attribute != nullptr && attribute->Number() != 0.0;
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
    quest.description = TextAt(prefix, L"Description");
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
