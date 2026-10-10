// Reads the player's journal: the quests in progress, the finished ones, the one marked active,
// and the groups they are filed under, as the journal screen shows them. Scripts cannot list
// quests; this can.
//
//   for (const dao::journal::Quest &quest : dao::journal::Current())
//       spdlog::info("{}: {}", dao::Utf8(quest.plot), dao::Utf8(quest.name));
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace dao::journal
{

// One journal entry. A text field is empty if the entry has no such text.
struct Quest
{
    std::wstring plot;        // the plot's resource name, as GetPlotGUID takes it
    std::wstring name;        // the title, in the player's language
    std::wstring description; // the entry's text with its markup removed; lines end in \r\n
    std::wstring objective;   // the highlighted part of the description: what to do next
    std::wstring parent;      // the plot this one is part of
    std::wstring group;       // the resource name of the quest's Group
    std::wstring area;        // the tag of the area the quest sends the player to
    std::wstring offer;       // the offer's ID, for a quest that offers extra content
    bool completed = false;   // a finished part of a quest, still listed by Current
    bool updated = false;     // changed since the player last looked
};

// Returns the quests in progress, in the journal's order. A finished part of a quest stays in
// this list with `completed` set.
// @return an empty list if no game is loaded
//
// Call it on the game thread (dao/Frame.h).
std::vector<Quest> Current();

// Returns the journal's list of finished quests. Call it on the game thread.
// @return an empty list if no game is loaded
std::vector<Quest> Completed();

// Returns the quest the player marked active, the one the map points to. Call it on the game
// thread.
// @return nullopt if no quest is marked or no game is loaded
std::optional<Quest> Active();

// A group the journal files quests under, such as the main plot or a place.
struct Group
{
    std::wstring resource; // the name a quest's `group` field holds
    std::wstring name;     // the group's name in the player's language
    double priority = 0;   // the group's position in the journal's order
};

// Returns the journal's quest groups. Call it on the game thread.
// @return an empty list if no game is loaded
std::vector<Group> Groups();

} // namespace dao::journal
