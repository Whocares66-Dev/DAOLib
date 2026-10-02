// The journal: the quests the player has, the ones finished, and the one
// marked active, as the game's own journal screen reads them.
//
//   for (const dao::journal::Quest &quest : dao::journal::Current())
//       spdlog::info("{}: {}", dao::Utf8(quest.plot), dao::Utf8(quest.name));
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
// From the frame's thread (dao/Frame.h), where the engine reads its own.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace dao::journal
{

struct Quest
{
    std::wstring plot;        // the plot's resource name, what GetPlotGUID takes
    std::wstring name;        // the title, in the player's language
    std::wstring description; // the entry showing, the same, without the marks
    std::wstring objective;   // the part of it the journal marks: what to do now
    std::wstring parent;      // the plot it is a part of, if any
    std::wstring group;       // its plot group's resource name
    std::wstring area;        // the tag of the area it points the player to
    std::wstring offer;       // for a quest of content on offer: the offer's id
    bool completed = false;   // a part of a quest that is done, still listed
    bool updated = false;     // changed since the player last looked
};

// The quests under way, in the journal's own order, and the ones finished.
// Empty with no game loaded.
std::vector<Quest> Current();
std::vector<Quest> Completed();

// The quest the player marked active, the one the map points to. Nothing
// if none is.
std::optional<Quest> Active();

// A group the journal files quests under (the main plot, a place's).
struct Group
{
    std::wstring resource; // what a quest's `group` names
    std::wstring name;     // in the player's language
    double priority = 0;   // its place in the journal's order
};
std::vector<Group> Groups();

} // namespace dao::journal
