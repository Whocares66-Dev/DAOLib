// The Chanter's board (dao/Board.h).
//
// Notes on the board: read 2026-10-25 (DAOLib #259) in the GOG exe and chanters.gfx, with dev/GUI.md's
// "The Chanter's board", after the Eluvian server's board tool, which read and took quests only
// while the screen was up. ShowChantersGUI (OPENCHANTERSGUI, 0x00662F40) has the journal
// (CClientExoApp::GetJournal, 10506) build the board (Journal::BuildChanterBoard, 15737,
// __thiscall (board), `ret 4`), then opens the screen: the board's rows of jobboardplots (table
// 116) whose plot is not done and whose VisibleFlag is set, each a quest in the data tree's
// Journal.ChanterBoard.ChanterEntries with its PlotName, PlotResRef, JournalText and Accepted. The
// screen's Accept sends "Journal.AcceptChantersQuest <index>" (0x006EB850), which refuses only an
// index out of the board: it sets Accepted and the plot's AcceptedFlag, running the plot's script,
// even for a quest taken already, whose Accept the movie disables. Its closing sends
// Journal.ClearChanterBoard (Journal::ClearChanterBoard, 15698). The board has no turn-in: a quest
// is turned in by its giver's conversation, which sets its plot's flag.
//
// Seen in play 2026-10-25 (examples/board), Lyna in Redcliffe with no screen open: board 13 built
// with its four quests (Caravan Down, Brothers and Sons, Skin Deep, Desperate Haven); Caravan Down
// accepted, again refused, index 99 refused; Close left none shown; built again, the board offered
// three, Caravan Down gone from it and in the journal, its plot's script having cleared its offer.
#include "dao/Board.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Data.h"
#include "dao/Gui.h"
#include "dao/Messages.h"
#include "dao/Module.h"

#include <format>

namespace dao::board
{

namespace
{

constexpr std::size_t kMostQuests = 256;

void *Journal()
{
    CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return nullptr;
    }
    using JournalFn = void *(__fastcall *)(CClientExoApp * app, int edx);
    return reinterpret_cast<JournalFn>(Module::Address(addr::kCClientExoApp_GetJournal))(app, 0);
}

std::wstring EntryPath(std::size_t index)
{
    return std::format(L"Journal.ChanterBoard.ChanterEntries.{}", index);
}

} // namespace

std::vector<Quest> Shown()
{
    std::vector<Quest> quests;
    const std::size_t count = data::MemberCount(L"Journal.ChanterBoard.ChanterEntries").value_or(0);
    for (std::size_t index = 0; index < count && index < kMostQuests; ++index)
    {
        const std::wstring at = EntryPath(index);
        quests.push_back({.name = data::Text(at + L".PlotName").value_or(L""),
                          .plot = data::Text(at + L".PlotResRef").value_or(L""),
                          .text = data::Text(at + L".JournalText").value_or(L""),
                          .accepted = data::Number(at + L".Accepted").value_or(0) != 0});
    }
    return quests;
}

std::vector<Quest> Quests(std::int32_t board)
{
    void *journal = Journal();
    if (journal == nullptr)
    {
        return {};
    }
    using BuildFn = void(__fastcall *)(void *journal, int edx, std::int32_t board);
    reinterpret_cast<BuildFn>(Module::Address(addr::kJournal_BuildChanterBoard))(journal, 0, board);
    return Shown();
}

bool Accept(std::size_t index)
{
    const auto quests = Shown();
    if (index >= quests.size() || quests[index].accepted)
    {
        return false;
    }
    messages::SendText(std::format(L"Journal.AcceptChantersQuest {}", index));
    return data::Number(EntryPath(index) + L".Accepted").value_or(0) != 0;
}

void Close()
{
    const auto full = gui::CurrentFullScreen();
    if (full && full->name == L"Chanters")
    {
        messages::SendText(L"GUI.HideFullScreenGUI");
    }
    if (void *journal = Journal(); journal != nullptr)
    {
        using ClearFn = void(__fastcall *)(void *journal, int edx);
        reinterpret_cast<ClearFn>(Module::Address(addr::kJournal_ClearChanterBoard))(journal, 0);
    }
}

} // namespace dao::board
