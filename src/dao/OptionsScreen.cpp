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
// - The game's own controls (DAOLib #273) are made by OptionsUtils' helpers (MakeOptionToggleInit,
//   MakeSliderInit, MakeSliderPickerInit, MakePickerInit and the two FromListAttribute forms), which
//   keep what a control is in the set-up function they return. The recorder the movie is patched
//   with (tools/options_record/frame_1/DoAction.as) wraps them, so each control keeps it on its clip
//   too, and lists them (DAOLibOptionList). A control sends "ClientOptions.SetClientOptionMessage
//   <option> <value>" at a click or a drag, a slider of numbers its number, and Set sends the same.
//   Seen in play 2026-10-30: the Audio tab's music volume and its sound effects' checkbox set so,
//   the screen's slider and box following; the game put both back after Cancel and kept them after
//   OK, as a player's would be.
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
#include "dao/String.h"
#include "dao/Tlk.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
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

// A value as the movie's controls write it in their message: an integer without a point, a
// number with up to six places.
std::string Number(double value)
{
    if (value == std::floor(value) && std::abs(value) < 1e9)
    {
        return std::to_string(static_cast<std::int64_t>(value));
    }
    std::string text = std::to_string(value);
    text.erase(text.find_last_not_of('0') + 1);
    return text;
}

// A label or value name as the recorder gives it: the number of one of the game's strings, which
// the movie's SetText shows, or text.
std::wstring Shown(std::string_view text)
{
    std::uint32_t strref = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), strref);
    if (error == std::errc{} && end == text.data() + text.size() && !text.empty())
    {
        return tlk::String(strref);
    }
    return Wide(text);
}

std::vector<std::string_view> Split(std::string_view text, char by)
{
    std::vector<std::string_view> parts;
    std::size_t from = 0;
    while (from <= text.size())
    {
        const std::size_t to = text.find(by, from);
        parts.push_back(text.substr(from, to == std::string_view::npos ? std::string_view::npos : to - from));
        if (to == std::string_view::npos)
        {
            break;
        }
        from = to + 1;
    }
    return parts;
}

double ToNumber(std::string_view text)
{
    double number = 0;
    std::from_chars(text.data(), text.data() + text.size(), number);
    return number;
}

// A control as the recorder lists it (tools/options_record/frame_1/DoAction.as, DAOLibOptionList):
// its clip's path, its option in the movie, and what it shows. The game's own, or a plugin's
// checkbox or choice: each plugin links its own copy of the library, so another plugin's controls
// are seen only here (DAOLib #277).
struct GameControl
{
    std::string clip;
    std::string option;
    EntryState state;
};

std::optional<double> CurrentGroup();

// The controls the screen shows now, as the recorder the options movie was patched with lists
// them; none without it. Their tab is the one shown, the only one whose controls exist. A plugin's
// is named as its plugin named it, its label and values its own text, where the game's are the
// numbers of its strings.
std::vector<GameControl> GameControls()
{
    std::vector<GameControl> controls;
    auto movie = gui::ScreenMovie(kScreen);
    const auto list = movie ? movie->Call("DAOLibOptionList") : std::nullopt;
    if (!list || list->type != gui::Value::Type::String)
    {
        return controls;
    }
    const auto group = CurrentGroup();
    const auto tab = std::ranges::find_if(kTabButtons, [&](const TabButton &button) {
        return group && static_cast<std::int32_t>(*group) == button.group;
    });
    for (const std::string_view line : Split(list->text, '\n'))
    {
        const auto fields = Split(line, '\t');
        constexpr std::size_t kFields = 7;
        if (fields.size() < kFields)
        {
            continue;
        }
        GameControl control;
        control.clip = std::string(fields[0]);
        control.option = std::string(fields[2]);
        const std::string_view maker = fields[1];
        EntryState &state = control.state;
        const bool plugin = control.option.starts_with(detail::kOptionPrefix);
        const auto text = [plugin](std::string_view shown) { return plugin ? Wide(shown) : Shown(shown); };
        state.name = Wide(plugin ? fields[2].substr(detail::kOptionPrefix.size()) : fields[2]);
        state.label = text(fields[3]);
        state.value = ToNumber(fields[4]);
        state.enabled = fields[5] == "1";
        state.game = !plugin;
        state.tab = tab != kTabButtons.end() ? tab->tab : Tab::Game;
        if (maker == "MakeOptionToggleInit")
        {
            state.kind = Kind::Toggle;
        }
        else if (maker == "MakeSliderInit")
        {
            state.kind = Kind::Slider;
            state.least = ToNumber(fields[6]);
            state.most = fields.size() > kFields ? ToNumber(fields[kFields]) : state.least;
        }
        else
        {
            state.kind = Kind::Choice;
            for (const std::string_view value : Split(fields[6], '\x01'))
            {
                state.values.push_back(text(value));
            }
        }
        controls.push_back(std::move(control));
    }
    return controls;
}

// Whether a value is one a control takes: a checkbox's 0 or 1, a choice's index, a slider's number
// from its least to its most.
bool InRange(const EntryState &state, double value)
{
    switch (state.kind)
    {
    case Kind::Toggle:
        return value == 0 || value == 1;
    case Kind::Choice:
        return value >= 0 && value < static_cast<double>(state.values.size()) && value == std::floor(value);
    case Kind::Slider:
        return value >= state.least && value <= state.most;
    case Kind::Button:
        break;
    }
    return true;
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
    detail::AttachOptions(); // the movie patched as it loads, the recorder in
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

std::vector<EntryState> Entries(Tab tab)
{
    detail::AttachOptions();
    std::vector<EntryState> found;
    if (!IsOpen())
    {
        return found;
    }
    found = detail::PluginEntries(tab);
    const auto tabs = Tabs();
    const auto shown = std::ranges::find_if(tabs, &TabState::current);
    if (shown != tabs.end() && shown->tab == tab)
    {
        for (const GameControl &control : GameControls())
        {
            // This plugin's own are listed already, from what it gave.
            if (control.state.game || !detail::PlacedOption(control.state.name))
            {
                found.push_back(control.state);
            }
        }
    }
    return found;
}

Refusal Set(std::wstring_view name, double value)
{
    detail::AttachOptions();
    if (!IsOpen())
    {
        return Refusal::NotShown;
    }
    const auto tabs = Tabs();
    const auto shown = std::ranges::find_if(tabs, &TabState::current);
    std::string option;
    if (const auto control = detail::PlacedOption(name))
    {
        if (shown == tabs.end() || shown->tab != control->tab)
        {
            return Refusal::NotShown;
        }
        if (control->kind == Kind::Button)
        {
            value = 1; // what a button's click sends
        }
        else if (value < 0 || value >= control->values || value != std::floor(value))
        {
            return Refusal::OutOfRange;
        }
        option = control->option;
    }
    else
    {
        // The game's own, or another plugin's: on the screen only while its tab is shown.
        const auto controls = GameControls();
        const auto game = std::ranges::find(controls, name, [](const GameControl &c) { return c.state.name; });
        if (game == controls.end())
        {
            return Refusal::NotOffered;
        }
        if (!game->state.enabled)
        {
            return Refusal::Disabled;
        }
        if (!InRange(game->state, value))
        {
            return Refusal::OutOfRange;
        }
        option = game->option;
    }
    // The message the control itself sends at a click or a drag (OptionsUtils' toggle, a
    // picker's PickEntryCallBack, a slider's change), sent through the movie's own
    // ExternalCommands; a slider of numbers sends its number, the others an integer.
    const std::string message = "ClientOptions.SetClientOptionMessage " + option + " " + Number(value);
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
