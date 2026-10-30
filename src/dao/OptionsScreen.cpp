// The options screen driven as a player drives it (dao/Options.h, DAOLib #272): opened from the
// start menu or Escape's menu by that menu's own Options button, a tab shown by its own button,
// a plugin's control clicked by the message its own control sends, OK and Cancel pressed.
//
// Read 2026-10-30 in optionsmenu.gfx's and mainmenu.gfx's ActionScript (OptionsMenu.as,
// MainMenu.as, OptionsUtils.as, Behaviour/StickyButton.as) and seen by hand in play the same day,
// GOG, Redcliffe's day village:
// - Escape's menu (the scene MainMenu, ToggleFullScreenGUIMessage MainMenu, as the Escape key's
//   binding sends) has its buttons in an unnamed clip of Menu_mc
//   (_level0.Menu_mc.instance18.Options_mc). Its Options button hides the menu's buttons and pushes
//   the Options screen over the menu, whose Background_mc stays shown behind: the panel. The Options
//   screen pushed alone (GameModeController.PushGUIScreen Options) has none, and the scene shows
//   through (DAOElgaraShaders #68). Options' FinishScene shows the menu's buttons again as it closes.
// - The start menu's (startmenu2.gfx) is OptionsButton, in _level0.StartButtons_mc.StartMenuButtons_mc;
//   pressed, the screen opens over the start menu, dimmed behind it, as Escape's menu's does.
// - A tab's button (_level0.OptionTabsGroup_mc.<Name>Tab_mc, a StickyButton) runs
//   OptionsMenu.OptionTabAction: OptionsMenu.ms_nCurrentTab set (Game 0, Online 1, Controls 2,
//   Audio 3, Video 4), the movie's token observers dropped, and the page played. Its
//   SimulateButtonPush takes a click only while its `enabled` and m_bStickyButtonEnabled are true.
//   SceneManager.PlayScene alone shows the page and leaves the strip and ms_nCurrentTab as they
//   were, so Restore Defaults acts on another tab.
// - OK and Cancel are buttons in an unnamed clip of the screen (_level0.instance16 that day). OK
//   shows the game's message box for a change it must restart for, else commits
//   (ClientOptions.CommitOptions) and hides the screen; Cancel hides it alone.
// A clip whose name is unknown is found by its children's names: a container's children are walked
// by getInstanceAtDepth, as Eluvian's movie tool walks them, the designer's at depths from -16384
// and the script's from 0. A button's own clip reads as "undefined" (its class's toString), so the
// walk goes by containers and asks each for the button by name.
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Gui.h"
#include "dao/Messages.h"
#include "dao/Options.h"
#include "dao/OptionsDetail.h"
#include "dao/Tlk.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>

namespace dao::options
{

namespace
{

constexpr std::wstring_view kScreen = L"Options";
constexpr std::wstring_view kStartMenu = L"StartMenu";
constexpr std::wstring_view kEscapeMenu = L"MainMenu";
constexpr std::string_view kTabs = "_level0.OptionTabsGroup_mc.";

// A tab's button, its number in the movie (OPTION_GROUP_*) and its label's string (Localization.as).
struct TabButton
{
    Tab tab;
    std::string_view clip;
    std::int32_t group;
    std::uint32_t label;
};
constexpr std::array<TabButton, 5> kTabButtons{{
    {Tab::Game, "GameTab_mc", 0, 325566},
    {Tab::Controls, "ControlsTab_mc", 2, 325567},
    {Tab::Audio, "AudioTab_mc", 3, 325568},
    {Tab::Video, "VideoTab_mc", 4, 325569},
    {Tab::Online, "OnlineTab_mc", 1, 376832},
}};

bool Shown(std::wstring_view name)
{
    const auto screens = gui::ShownScreens();
    return std::ranges::any_of(screens, [&](const gui::Screen &screen) { return screen.name == name; });
}

// The path of a clip that has a child of this name, found from _level0 down by containers; nothing if
// none within a few levels.
std::optional<std::string> FindParentOf(gui::Movie &movie, std::string_view child)
{
    constexpr int kPlacedFrom = -16384;
    constexpr int kPlacedTo = -16000; // the designer's depths in these movies are well within
    constexpr std::size_t kMostContainers = 400;
    std::deque<std::string> containers{"_level0"};
    for (std::size_t looked = 0; !containers.empty() && looked < kMostContainers; ++looked)
    {
        std::string container = std::move(containers.front());
        containers.pop_front();
        if (movie.IsAvailable((container + "." + std::string(child)).c_str()))
        {
            return container;
        }
        const std::string at = container + ".getInstanceAtDepth";
        const auto add = [&](int depth) {
            const gui::Value args[] = {gui::Value::Number(depth)};
            const auto answer = movie.Call(at.c_str(), args);
            if (answer && answer->type == gui::Value::Type::String && answer->text.starts_with("_level0."))
            {
                containers.push_back(answer->text);
            }
        };
        for (int depth = kPlacedFrom; depth < kPlacedTo; ++depth)
        {
            add(depth);
        }
        const auto next = movie.Call((container + ".getNextHighestDepth").c_str());
        const int highest = next && next->type == gui::Value::Type::Number ? static_cast<int>(next->number) : 0;
        for (int depth = 0; depth < highest; ++depth)
        {
            add(depth);
        }
    }
    return std::nullopt;
}

// Presses a screen's button by name, as a click does.
bool Press(std::wstring_view screen, std::string_view button)
{
    auto movie = gui::ScreenMovie(screen);
    if (!movie)
    {
        return false;
    }
    const auto parent = FindParentOf(*movie, button);
    return parent && movie->Invoke((*parent + "." + std::string(button) + ".SimulateButtonPush").c_str());
}

// Escape's menu asked for and its Options button not yet pressed: the frame it was asked on.
std::optional<std::uint64_t> g_escapeAsked;

// Each frame while Escape's menu is being opened: its Options button pressed once the menu shows it.
void FinishOpening()
{
    constexpr std::uint64_t kMostFrames = 300;
    if (!g_escapeAsked)
    {
        return;
    }
    if (IsOpen() || FrameCount() - *g_escapeAsked > kMostFrames)
    {
        g_escapeAsked.reset();
        return;
    }
    if (Shown(kEscapeMenu) && Press(kEscapeMenu, "Options_mc"))
    {
        g_escapeAsked.reset();
    }
}

std::optional<double> CurrentGroup()
{
    auto movie = gui::ScreenMovie(kScreen);
    return movie ? movie->GetNumber("OptionsMenu.ms_nCurrentTab") : std::nullopt;
}

} // namespace

bool IsOpen()
{
    return Shown(kScreen);
}

Refusal Open()
{
    static const bool watched = [] {
        OnFrame(&FinishOpening);
        return true;
    }();
    (void)watched;
    if (IsOpen())
    {
        return Refusal::AlreadySo;
    }
    if (Shown(kStartMenu))
    {
        return Press(kStartMenu, "OptionsButton") ? Refusal::None : Refusal::NotShown;
    }
    if (Shown(kEscapeMenu))
    {
        return Press(kEscapeMenu, "Options_mc") ? Refusal::None : Refusal::NotShown;
    }
    const auto mode = CClientExoApp::CurrentMode();
    if (mode != GameMode::Explore && mode != GameMode::Combat)
    {
        return Refusal::NotShown;
    }
    if (!messages::SendBinding(L"ToggleFullScreenGUIMessage MainMenu"))
    {
        return Refusal::NotShown;
    }
    g_escapeAsked = FrameCount();
    return Refusal::None;
}

std::vector<TabState> Tabs()
{
    std::vector<TabState> tabs;
    auto movie = gui::ScreenMovie(kScreen);
    if (!IsOpen() || !movie)
    {
        return tabs;
    }
    const auto group = CurrentGroup();
    for (const TabButton &button : kTabButtons)
    {
        const std::string clip = std::string(kTabs) + std::string(button.clip);
        TabState tab{.tab = button.tab, .name = tlk::String(button.label)};
        tab.enabled = movie->GetBool((clip + ".enabled").c_str()).value_or(false) &&
                      movie->GetBool((clip + ".m_bStickyButtonEnabled").c_str()).value_or(false);
        tab.current = group && static_cast<std::int32_t>(*group) == button.group;
        tabs.push_back(std::move(tab));
    }
    return tabs;
}

Refusal ShowTab(Tab tab)
{
    const auto tabs = Tabs();
    const auto found = std::ranges::find(tabs, tab, &TabState::tab);
    if (found == tabs.end())
    {
        return Refusal::NotShown;
    }
    if (found->current)
    {
        return Refusal::AlreadySo;
    }
    if (!found->enabled)
    {
        return Refusal::Disabled;
    }
    const auto button = std::ranges::find(kTabButtons, tab, &TabButton::tab);
    auto movie = gui::ScreenMovie(kScreen);
    const std::string push = std::string(kTabs) + std::string(button->clip) + ".SimulateButtonPush";
    return movie && movie->Invoke(push.c_str()) ? Refusal::None : Refusal::NotShown;
}

Refusal Set(std::wstring_view name, std::int32_t value)
{
    if (!IsOpen())
    {
        return Refusal::NotShown;
    }
    const auto control = detail::PlacedOption(name);
    if (!control)
    {
        return Refusal::NotOffered;
    }
    const auto tabs = Tabs();
    const auto shown = std::ranges::find_if(tabs, &TabState::current);
    if (shown == tabs.end() || shown->tab != control->tab)
    {
        return Refusal::NotShown;
    }
    if (control->kind == Kind::Button)
    {
        value = 1; // what a button's click sends
    }
    else if (value < 0 || value >= control->values)
    {
        return Refusal::OutOfRange;
    }
    // The message the control itself sends at a click (OptionsUtils' toggle, a picker's
    // PickEntryCallBack, a slider's change), sent through the movie's own ExternalCommands.
    const std::string message = "ClientOptions.SetClientOptionMessage " + control->option + " " + std::to_string(value);
    auto movie = gui::ScreenMovie(kScreen);
    const gui::Value args[] = {gui::Value::String(message.c_str())};
    return movie && movie->Invoke("ExternalCommands.SendMessage", args) ? Refusal::None : Refusal::NotShown;
}

Refusal Ok()
{
    if (!IsOpen())
    {
        return Refusal::NotShown;
    }
    return Press(kScreen, "OK_mc") ? Refusal::None : Refusal::NotShown;
}

Refusal Cancel()
{
    if (!IsOpen())
    {
        return Refusal::NotShown;
    }
    return Press(kScreen, "Cancel_mc") ? Refusal::None : Refusal::NotShown;
}

} // namespace dao::options
