// Example: straight back into the game (dao/Saves.h).
//
// The game starts at its main menu. This plugin loads the save written
// last, once the menu is up, with no menu touched: what "Resume" on the
// menu does, done for the player. It is also how a check of the library in
// play gets into the world with nobody at the keyboard (dev/BUILDS.md used
// it to see the EA build in play).
//
// Three parts of the library do it. A frame callback (dao/Frame.h) runs on
// the game's thread, which is where a load must be asked from. The game's
// mode (dao/ClientExoApp.h) says when the menu is up: the main menu at
// start-up runs in Pregame, and the intro movies before it do not, so a
// few seconds of Pregame is the menu. And dao/Saves.h lists the saves and
// asks for the load as the load screen does.
//
// "Written last" is by the save file's own time, across every character.
// The plugin asks once: if the game declines, it logs that and leaves the
// menu to the player, and a game the player then loads is theirs.
//
// One way the game declines is by asking: a save made with content this
// install lacks gets "This saved game cannot be loaded. It requires the
// following content", with Force Load and Cancel (a save from the GOG
// install, asked for in the EA app's, 2026-10-01). That is the player's to
// answer, since a forced load drops the content from the game in play, so
// the plugin leaves the dialog up. A check with nobody there to answer says
// so beforehand, with an empty file named example_resume.force beside the
// logs: then the plugin answers Force Load itself (dao::saves::ForceLoad).
//
// What it logs goes to example_resume.log.
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/Saves.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <system_error>

namespace
{

using Clock = std::chrono::steady_clock;

// How long the menu's mode must have held before it is the menu, drawn and
// ready: the mode is set a moment before the menu's movie opens.
constexpr auto kSettle = std::chrono::seconds(3);
// How long after asking the game is given to leave the menu for its loading
// screen, before it is taken to have asked about content instead.
constexpr auto kStarted = std::chrono::seconds(5);

enum class State
{
    AtStart, // waiting for the main menu
    Asked,   // the load asked for, the game not yet seen to start it
    Done,
};
State g_state = State::AtStart;
std::optional<Clock::time_point> g_menuSince;
Clock::time_point g_askedAt;

// When a slot's save was written: its .das file's time. Nothing if the
// folder cannot be read; error codes, not exceptions, since a callback has
// no way to throw back through the game.
std::optional<std::filesystem::file_time_type> WrittenAt(const dao::saves::Slot &slot)
{
    std::error_code ec;
    std::optional<std::filesystem::file_time_type> latest;
    for (const auto &entry : std::filesystem::directory_iterator(slot.Folder(), ec))
    {
        if (entry.path().extension() != L".das")
        {
            continue;
        }
        const auto written = entry.last_write_time(ec);
        if (!ec && (!latest || written > *latest))
        {
            latest = written;
        }
    }
    return latest;
}

std::optional<dao::saves::Slot> LastSave()
{
    std::optional<dao::saves::Slot> last;
    std::optional<std::filesystem::file_time_type> lastWritten;
    for (const auto &character : dao::saves::Characters())
    {
        for (const auto &slot : dao::saves::List(character))
        {
            const auto written = WrittenAt(slot);
            if (written && (!lastWritten || *written > *lastWritten))
            {
                last = slot;
                lastWritten = written;
            }
        }
    }
    return last;
}

bool MayForce()
{
    std::error_code ec;
    return std::filesystem::exists(dao::LogDirectory() / L"example_resume.force", ec);
}

void Ask()
{
    const auto save = LastSave();
    if (!save)
    {
        spdlog::info("the main menu is up; no save to resume");
        g_state = State::Done;
        return;
    }
    const auto meta = save->Meta();
    const bool asked = dao::saves::Load(*save);
    spdlog::info("the main menu is up; {} {}'s {} save, slot {}{}", asked ? "loading" : "the game would not load",
                 dao::Utf8(save->character), dao::saves::Name(save->kind), save->number,
                 meta ? fmt::format(": {}, level {}", dao::Utf8(meta->area), meta->level) : "");
    g_askedAt = Clock::now();
    g_state = asked ? State::Asked : State::Done;
}

void Frame()
{
    if (g_state == State::Done)
    {
        return;
    }
    const bool atMenu = dao::CClientExoApp::CurrentMode() == dao::GameMode::Pregame;
    const auto now = Clock::now();
    if (g_state == State::Asked)
    {
        if (!atMenu)
        {
            g_state = State::Done; // the loading screen: the game took it
        }
        else if (now - g_askedAt >= kStarted)
        {
            g_state = State::Done;
            if (!MayForce())
            {
                spdlog::info("the game has not started the load: its question, if it asked one, is the player's");
            }
            else if (dao::saves::ForceLoad())
            {
                spdlog::info(
                    "the save needs content this install lacks; example_resume.force is there, so: Force Load");
            }
            else
            {
                spdlog::info("the game has not started the load, and no save is waiting on Force Load");
            }
        }
        return;
    }
    if (!atMenu)
    {
        g_menuSince.reset();
        return;
    }
    if (!g_menuSince)
    {
        g_menuSince = now;
    }
    if (now - *g_menuSince >= kSettle)
    {
        Ask();
    }
}

void Loaded(const std::optional<dao::saves::Slot> &save)
{
    if (save)
    {
        spdlog::info("in the world: {}'s {} save, slot {}", dao::Utf8(save->character), dao::saves::Name(save->kind),
                     save->number);
    }
    // A game is in play, however it came to be: nothing to resume any more.
    g_state = State::Done;
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_resume");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::OnFrame(&Frame);
    dao::saves::OnGameLoaded(&Loaded);
    return true;
}
