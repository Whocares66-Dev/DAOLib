// Example: the journal (dao/Journal.h).
//
// About once a second the quests under way, the ones finished and the
// active one are read, and when any of it has changed since the last look
// the journal is written to example_journal.log: each quest's plot, title
// and group, the active one marked. Load a save and the journal it holds is
// logged; take or finish a quest and it is logged again.
#include "dao/Journal.h"
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <string>
#include <vector>

namespace
{

constexpr std::uint64_t kFramesBetween = 60;

// What was logged last, as one string to compare with.
std::wstring g_last;

std::wstring Summary(const std::vector<dao::journal::Quest> &current, const std::vector<dao::journal::Quest> &completed,
                     const std::wstring &active)
{
    std::wstring summary = active + L"|";
    for (const auto &quest : current)
    {
        summary += quest.plot + L"=" + quest.description + L";";
    }
    summary += L"|";
    for (const auto &quest : completed)
    {
        summary += quest.plot + L";";
    }
    return summary;
}

void Frame()
{
    if (dao::FrameCount() % kFramesBetween != 0)
    {
        return;
    }
    const auto current = dao::journal::Current();
    const auto completed = dao::journal::Completed();
    const auto active = dao::journal::Active();
    const std::wstring activePlot = active ? active->plot : std::wstring();
    std::wstring summary = Summary(current, completed, activePlot);
    if (summary == g_last)
    {
        return;
    }
    g_last = std::move(summary);
    spdlog::info("the journal: {} under way, {} finished, {} group(s); active: {}", current.size(), completed.size(),
                 dao::journal::Groups().size(), activePlot.empty() ? "none" : dao::Utf8(activePlot));
    for (const auto &quest : current)
    {
        spdlog::info("  {}{} \"{}\" in group {}{}{}: {}", quest.plot == activePlot ? "* " : "", dao::Utf8(quest.plot),
                     dao::Utf8(quest.name), dao::Utf8(quest.group),
                     quest.parent.empty() ? "" : ", part of " + dao::Utf8(quest.parent),
                     quest.updated ? ", updated" : "", dao::Utf8(quest.description));
    }
    for (const auto &quest : completed)
    {
        spdlog::info("  finished: {} \"{}\"", dao::Utf8(quest.plot), dao::Utf8(quest.name));
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_journal");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::OnFrame(&Frame);
    return true;
}
