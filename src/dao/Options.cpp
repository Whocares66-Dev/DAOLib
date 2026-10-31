// A plugin's settings in the options screen (dao/Options.h): how the game
// does its own, and what was seen of a plugin's in play.
//
// How the game does its own, read 2026-10-01 (dev/GUI.md, dao/core/
// OptionsMovie.h): the Game tab is a clip of controls the designer placed,
// with rows to spare under its two columns; a checkbox is the movie's own
// symbol set up by the movie's own helper, which sends the engine
// "ClientOptions.SetClientOptionMessage <option> 1" (or 0) on a click and
// draws the box by what the engine tells it the option is.
//
// A plugin's section goes the same way. The library patches the options
// movie as the game opens it (dao/Gui.h, OnMovieLoad): the heading (the
// bar and heading text the tab's own "Gameplay" has) and the checkboxes
// are placed in the tab's free rows, and each checkbox is set up
// by the game's helper, so it looks and behaves as its neighbours do,
// tooltip and click sound with them. The library answers for the engine:
// it takes the checkbox's request to be told its option's value, and its
// click, before the engine sees either.
//
// There is room for eleven rows: five under the left column and six under
// the right. A heading is the game's own gold bar and takes two, so the
// left column holds a heading and three checkboxes, the right a heading
// and four. The rows are shared with every other plugin that adds a
// section, each taking what the ones before it left.
//
// Seen in play, 2026-10-01, with examples/options/ beside examples/input/
// (the play tester's session, on 3783dd4):
// - The heading and three boxes under the left column's last box, the
//   boxes and labels in line with the game's own and spaced as they are.
//   The heading was then plain text centred over the column, smaller than
//   the game's own gold bars ("Gameplay", "Feedback"): a caption. It is
//   now the game's bar and heading text, seen the same day on 98eacf4: a
//   bar of the width, height and gradient of the game's two, the title
//   left-aligned in the face and size of "Gameplay", the gaps above and
//   below it as the game's; the third box under it ends just inside the
//   panel's corner, so three is what the left column holds.
// - Each box was asked its value at each opening and drawn by the answer.
// - A click changed a box and it stayed; the box that refuses was empty
//   again 0.3 s after its click, with no flicker caught.
// - Each box had its tooltip, drawn under it over the row below.
// - The game's own boxes worked and did not move. Restore Defaults reset
//   the game's and left the plugin's; after Cancel and a second opening
//   the plugin's were as left and nothing was doubled.
// - In a fight, the screen opened from the main menu with the game paused
//   behind it: the refusing box refused and another changed, each
//   callback run at the click, not when the screen closed.
// The first try, with two plugins patching the movie, showed no section:
// dao/Gui.h, OnMovieLoad, has why. The turn-based mod's own section (one
// box, refused in a fight) was seen the same day too.
//
// A choice is the game's own slider with named stops, as the Video tab's
// texture and graphics detail are. The game asks its value as the screen
// opens, before `get` can be run on the frame's thread, so it is answered
// then with the last value `get` or `set` gave, and told `get`'s straight
// after, as a checkbox is (OnGetValue). Read 2026-10-02 from the options
// movie (dao/core/OptionsMovie.h). Seen in play the same day
// (examples/options, the GOG game, the play tester): the section under the
// right column, the slider as the Video tab's detail sliders look, its
// label at the left and the value's name at the right; the track's ends
// step it (there are no separate arrows, on the game's either) and stop at
// the first and last; a drag snaps to a stop, passing through those
// between, so `set` may be called once for each; the tooltip under the
// label; the value kept across OK, Cancel and a second opening; nothing
// overlapping, and the game's own sections and another plugin's beside it
// as before.
//
// A section can be on the Video tab instead (DAOLib #79, read 2026-10-11;
// dao/core/OptionsMovie.h, dev/GUI.md). That tab's two columns are a clip
// each, and it has less room than it looks to: two rows under the left
// column, which no section fits, and five under the right. A section a tab
// has no room for is said in the log: one left out in silence was looked
// for on every tab. Seen in play the same day (examples/options, the GOG
// game at its main menu, the screen driven through its script): a heading,
// a checkbox and a slider under the right column's "Frame-buffer effects",
// the bar as "Detail" above it, the slider in line with the tab's three
// and ending just above Restore Defaults; the checkbox asked as the tab
// was first shown, a click kept and drawn; the slider stepped twice, `set`
// called for each; the Game tab's sections, the loader's among them, as
// before. The log had a warning for the example's fourth section, which
// the Video tab had no room for, and for its second, six rows where the
// Game tab's right column had three left under the loader's own section.
//
// The Video tab is laid out again for a section (DAOLib #82; the user,
// 2026-10-11: "reduce the spacing, move detail to the left below screen,
// then have graphics take the whole right column"): the game's "Detail"
// settings go under "Screen" in the left column, everything there set as
// close as the Game tab's, and the right column is the sections' from its
// top, fourteen rows where there were five. dao/core/OptionsMovie.h has the
// reading and the places. Outside the game, patch003.erf's movie through
// the patch with a section of three checkboxes and four sliders: placed,
// and FFDec reading the result with every control where the layout has it.
// Seen in play the same day (examples/options, the GOG game at its main
// menu, windowed 1600 by 1000, the tab shown through its script): "Screen"
// over the dropdown, the checkbox, the gamma's slider and picture and
// Vertical sync; "Detail" under them over its three sliders and
// Frame-buffer effects, each showing its setting, the last well inside the
// panel; the example's heading at the right column's top, level with
// "Screen", its checkbox and slider under it, and Restore Defaults below
// with nothing over it. The log had the warning for the example's fourth
// section, 13 rows where the tab had 9 left. Not driven: a click on one of
// the game's own controls where it now stands.
//
// A choice with ChoiceStyle::Dropdown is the game's own dropdown, as the
// Game tab's Difficulty is (DAOLib #66, read 2026-10-11; dao/core/
// OptionsMovie.h, dev/GUI.md). It talks as the slider does: its value asked
// as its button is made and answered with the last known, told `get`'s
// after, and a pick sent as the same message with the entry's index, so
// nothing here knows which of the two a choice is but the patch. The
// button shows the picked entry at once and is then told what `set`
// returned, so a refused pick goes back. Seen in play the same day
// (examples/options, the GOG game at its main menu, the screen driven
// through its script): the dropdown under the right column's heading, its
// label and button in line with the game's Subtitles above and looking as it
// does, the slider under it; the list opened over the slider, drawn as the
// game's own Difficulty list is over what is under that; a pick called
// `set` once with the entry's index, closed the list and put the name on
// the button; after Cancel and a second opening `get` was asked and the
// button showed the pick. A refused pick was not tried.
//
// A button is the screen's own Restore Defaults placed again, and a section
// can be on the Online tab (DAOLib #104, read 2026-10-12; dao/core/
// OptionsMovie.h). A button's click sends what a ticked checkbox's does,
// which is heard here the same way; it asks for no value and is told none,
// so it has no observer. Seen in play the same day (examples/options, the
// GOG game at its main menu, the screen driven through its script): on the
// Online tab a heading under "Connection"'s two boxes, as that bar is, a
// checkbox and under it two buttons that look as Restore Defaults does,
// their left edges where the boxes' are. A press of one
// (SimulateButtonPush, which is a click's press and release) ran its
// `pressed` once on the frame's thread, a second press again, and the
// screen stayed open; the checkbox was asked its value as the tab was first
// shown and ticked by its click. The button with a tooltip had the
// tooltip's behaviour on it (MaxTooltipWidth 200) and the one without had
// not, and both the click sound of Restore Defaults (95). Not seen: the
// tooltip drawn, which wants the mouse resting on the button; a button on
// the Game or Video tab; a button in a game with an older copy of this
// library in another plugin, which reads a button as a checkbox's height
// and so may put a row into its lower part.
#include "dao/Options.h"
#include "dao/OptionsDetail.h"
#include "dao/core/GfxMovie.h"
#include "dao/generated/OptionsRecord.h"

#include "dao/Addresses.h"
#include "dao/Frame.h"
#include "dao/Guard.h"
#include "dao/Gui.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/String.h"
#include "dao/core/InputCode.h"
#include "dao/core/OptionsMovie.h"

#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <mutex>
#include <optional>
#include <span>
#include <utility>

namespace dao::options
{

namespace
{

// What the movie's helper puts before an option's name: the token it asks
// to be told of, and the message a click sends.
constexpr std::string_view kToken = "ClientOptions.";
constexpr std::string_view kSetMessage = "ClientOptions.SetClientOptionMessage ";
// The screen's Restore Defaults: "ClientOptions.RestoreDefaults <tab>", the tab as the movie numbers
// it (OptionsMenu.as, read 2026-10-15 for DAOLib #140: OPTION_GROUP_GAME 0, ONLINE 1, CONTROLS 2,
// AUDIO 3, VIDEO 4, MOVIES 5). CClientOptions' handler (0x005E61A0) resets the game's own options
// of that group (0x005E49C0) and leaves this library's alone, so the message goes on to it and
// each control of the tab with a default is given it here, through the click's path.
constexpr std::string_view kRestoreMessage = "ClientOptions.RestoreDefaults ";
// OK's: OptionsMenu.CommitOptions sends it before it hides the screen; Cancel hides the screen
// alone (OptionsMenu.as, read 2026-10-30 for DAOLib #272).
constexpr std::string_view kCommitMessage = "ClientOptions.CommitOptions";

// The ids the library gives its observers. The engine's own count down
// from 0xFFFFFFFF, a movie's own count up from 0 (dao/Addresses.h), so
// ids from 0x40000000 meet neither. Each plugin links its own copy of this
// library, and two copies' checkboxes can be in one movie, which keeps
// its callbacks by id; so a copy's ids carry the plugin they are in: 15
// bits of its DLL's base address (which is a multiple of 0x10000, and
// below 0x80000000), then 15 bits of a count.
constexpr std::uint32_t kFirstObserver = 0x40000000;
constexpr std::uint32_t kObserverCount = 0x7FFF;

std::uint32_t ObserverBase()
{
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ObserverBase), &module);
    const std::uint32_t plugin =
        (static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module)) >> 16U) & 0x7FFFU;
    return kFirstObserver | plugin << 15U;
}

// A checkbox, a choice or a button; a checkbox's values are 0 and 1, and a
// button has none.
struct Entry
{
    std::optional<Toggle> toggle;
    std::optional<Choice> choice;
    std::optional<Button> button;
    std::string option; // as the movie has it: "DAOLibTurnBasedCombat"
    std::size_t section = 0;
    std::int32_t last = 0; // the value last got or set: what a GetValue is answered
};

struct Section
{
    std::wstring title;
    SectionState state = SectionState::Waiting;
    Column column = Column::Either;
    Tab tab = Tab::Game;
};

// A checkbox in an open movie waiting to be told its option's value.
struct Observer
{
    void *owner = nullptr; // the movie's record
    std::uint32_t id = 0;
    std::size_t entry = 0;
};

struct Click
{
    std::size_t entry = 0;
    std::int32_t wanted = 0;
};

struct Delivery
{
    void *owner = nullptr;
    std::uint32_t id = 0;
    std::int32_t value = 0;
};

std::mutex g_mutex;
std::vector<Section> g_sections; // all under g_mutex
std::vector<Entry> g_entries;
std::vector<Observer> g_observers;
std::vector<std::uint32_t> g_asked; // observers to tell their option's value, by id
std::vector<Click> g_clicks;
std::vector<Delivery> g_deliveries;
std::uint32_t g_observerCount = 0;
// Cancel's revert (DAOLib #272): each option's value as the screen opened, taken on the first frame
// it is shown, and whether OK's ClientOptions.CommitOptions went by before it closed.
bool g_screenShown = false;
bool g_committed = false;
std::vector<std::optional<std::int32_t>> g_opened; // by entry; none for a button

// Under g_mutex: the entry an option's name in the movie is, if it is ours.
std::optional<std::size_t> EntryOf(std::string_view option)
{
    for (std::size_t i = 0; i < g_entries.size(); ++i)
    {
        if (g_entries[i].option == option)
        {
            return i;
        }
    }
    return std::nullopt;
}

// A column as the movie's sections have it: 0 the left, 1 the right.
std::int32_t MovieColumn(Column column)
{
    switch (column)
    {
    case Column::Left:
        return 0;
    case Column::Right:
        return 1;
    case Column::Either:
        break;
    }
    return core::options_movie::kEitherColumn;
}

// A section is never on Controls or Audio (AddSection refuses them).
core::options_movie::Tab MovieTab(Tab tab)
{
    switch (tab)
    {
    case Tab::Video:
        return core::options_movie::Tab::Video;
    case Tab::Online:
        return core::options_movie::Tab::Online;
    case Tab::Game:
    case Tab::Controls:
    case Tab::Audio:
        break;
    }
    return core::options_movie::Tab::Game;
}

std::string_view NameOf(Tab tab)
{
    switch (tab)
    {
    case Tab::Video:
        return "Video";
    case Tab::Online:
        return "Online";
    case Tab::Controls:
        return "Controls";
    case Tab::Audio:
        return "Audio";
    case Tab::Game:
        break;
    }
    return "Game";
}

// Under g_mutex: section `index` as the movie's patch takes it.
core::options_movie::Section MovieSection(std::size_t index)
{
    core::options_movie::Section section;
    section.title = Utf8(g_sections[index].title);
    section.column = MovieColumn(g_sections[index].column);
    section.tab = MovieTab(g_sections[index].tab);
    for (const Entry &entry : g_entries)
    {
        if (entry.section != index)
        {
            continue;
        }
        if (entry.toggle)
        {
            section.toggles.push_back({entry.option, Utf8(entry.toggle->label), Utf8(entry.toggle->tooltip)});
        }
        else if (entry.choice)
        {
            core::options_movie::Choice choice{entry.option,
                                               Utf8(entry.choice->label),
                                               Utf8(entry.choice->tooltip),
                                               {},
                                               entry.choice->style == ChoiceStyle::Dropdown};
            for (const std::wstring &value : entry.choice->values)
            {
                choice.values.push_back(Utf8(value));
            }
            section.choices.push_back(std::move(choice));
        }
        else if (entry.button)
        {
            section.buttons.push_back({entry.option, Utf8(entry.button->label), Utf8(entry.button->tooltip)});
        }
    }
    return section;
}

// Under g_mutex: gives a section the state the patch left it in, and says so
// in the log when it is newly left out. A section left out in silence is
// looked for on the screen (DAOLib #79: one was, on every tab).
void Became(Section &section, SectionState state, std::size_t rows, std::size_t freeRows)
{
    if (section.state == state)
    {
        return;
    }
    section.state = state;
    const std::string_view tab = NameOf(section.tab);
    if (state == SectionState::NoRoom)
    {
        spdlog::warn("options: the section \"{}\" is left out: it takes {} rows one under the other in one column "
                     "of the {} tab, which has {} left in its two, or a section before it on that tab was left out",
                     Utf8(section.title), rows, tab, freeRows);
    }
    else if (state == SectionState::Failed)
    {
        spdlog::warn("options: the section \"{}\" is left out: the options screen's {} tab is not as it was read, "
                     "so nothing was added to it",
                     Utf8(section.title), tab);
    }
}

// The movie with the recorder of the screen's own controls on its first frame (DAOLib #273,
// tools/options_record/frame_1/DoAction.as): each control the movie's helpers make keeps on its
// clip what it was made with, and DAOLibOptionList lists them. Nothing if the movie is not as read.
std::optional<std::vector<std::byte>> WithRecorder(std::span<const std::byte> movie)
{
    const auto plain = core::gfx::Unpack(movie);
    if (!plain)
    {
        return std::nullopt;
    }
    const auto recorder = std::as_bytes(std::span(generated::kOptionsRecord));
    return core::gfx::AppendDoAction(*plain, recorder);
}

// The options movie as the game opens it: the sections placed, and the recorder.
std::optional<std::vector<std::byte>> Patch(std::span<const std::byte> movie)
{
    std::vector<core::options_movie::Section> sections;
    {
        const std::lock_guard lock(g_mutex);
        for (std::size_t i = 0; i < g_sections.size(); ++i)
        {
            sections.push_back(MovieSection(i));
        }
    }
    std::vector<bool> shown;
    auto patched = core::options_movie::AddSections(movie, sections, shown);
    const std::lock_guard lock(g_mutex);
    // Sections are only ever added, so the first of g_sections are the ones patched.
    // The rows a tab has left are counted with this patch's own sections in.
    const std::span<const std::byte> after = patched ? std::span<const std::byte>(*patched) : movie;
    for (std::size_t i = 0; i < sections.size(); ++i)
    {
        // AddSections gives nothing both when the movie is not as read and when no section
        // fits: on a tab that reads, a section not shown had no room (DAOLib #46).
        const auto slots = core::options_movie::FreeSlots(after, sections[i].tab);
        SectionState state = slots ? SectionState::NoRoom : SectionState::Failed;
        if (shown[i])
        {
            state = SectionState::Shown;
        }
        Became(g_sections[i], state, core::options_movie::RowsOf(sections[i]), slots ? slots->size() : 0);
    }
    auto recorded = WithRecorder(patched ? std::span<const std::byte>(*patched) : movie);
    return recorded ? recorded : patched;
}

// The movie's helper asks to be told an option's value: ExternalInterface's
// AddTokenObserver(label, "", token), answering the observer's id. For one
// of ours the library is the observer; the engine never hears of it.
bool OnAddTokenObserver(const gui::Call &call)
{
    if (call.args.size() < 3)
    {
        return false;
    }
    const auto token = call.args[2].AsString();
    if (!token || !token->starts_with(kToken))
    {
        return false;
    }
    const std::lock_guard lock(g_mutex);
    const auto entry = EntryOf(token->substr(kToken.size()));
    if (!entry)
    {
        return false;
    }
    static const std::uint32_t base = ObserverBase();
    const std::uint32_t id = base | (g_observerCount++ & kObserverCount);
    g_asked.push_back(id);
    g_observers.push_back({call.owner, id, *entry});
    call.Return(gui::Value::Number(id));
    return true;
}

// A choice asks its value as it is set up: ExternalInterface's
// GetValue("ClientOptions.<option>"), answered at once, on the interface's
// thread, so with the value last got or set rather than by `get`.
bool OnGetValue(const gui::Call &call)
{
    if (call.args.empty())
    {
        return false;
    }
    const auto token = call.args[0].AsString();
    if (!token || !token->starts_with(kToken))
    {
        return false;
    }
    const std::lock_guard lock(g_mutex);
    const auto entry = EntryOf(token->substr(kToken.size()));
    if (!entry)
    {
        return false;
    }
    call.Return(gui::Value::Number(g_entries[*entry].last));
    return true;
}

// A click: the FSCommand SendMessage with "ClientOptions.
// SetClientOptionMessage <option> 1" (or 0), or a choice's index; a button
// sends the 1.
std::optional<Tab> TabOfGroup(std::string_view group)
{
    if (group == "0")
    {
        return Tab::Game;
    }
    if (group == "1")
    {
        return Tab::Online;
    }
    if (group == "4")
    {
        return Tab::Video;
    }
    return std::nullopt;
}

// Under g_mutex: each control on the tab with a default, queued as a click to that value.
void QueueDefaults(Tab tab)
{
    for (std::size_t i = 0; i < g_entries.size(); ++i)
    {
        const Entry &entry = g_entries[i];
        if (g_sections[entry.section].tab != tab)
        {
            continue;
        }
        if (entry.toggle && entry.toggle->defaultValue)
        {
            g_clicks.push_back({i, *entry.toggle->defaultValue ? 1 : 0});
        }
        else if (entry.choice && entry.choice->defaultValue)
        {
            g_clicks.push_back({i, *entry.choice->defaultValue});
        }
    }
}

bool OnSendMessage(void * /*owner*/, std::string_view args)
{
    if (args == kCommitMessage)
    {
        const std::lock_guard lock(g_mutex);
        g_committed = true;
        return false; // the game's own options are the game's to keep
    }
    if (args.starts_with(kRestoreMessage))
    {
        if (const auto tab = TabOfGroup(args.substr(kRestoreMessage.size())))
        {
            const std::lock_guard lock(g_mutex);
            QueueDefaults(*tab);
        }
        return false; // the game's own options are the game's to restore
    }
    if (!args.starts_with(kSetMessage))
    {
        return false;
    }
    args.remove_prefix(kSetMessage.size());
    const std::size_t space = args.find(' ');
    if (space == std::string_view::npos)
    {
        return false;
    }
    const std::string_view value = args.substr(space + 1);
    const std::lock_guard lock(g_mutex);
    const auto entry = EntryOf(args.substr(0, space));
    if (!entry)
    {
        return false;
    }
    std::int32_t wanted = 0;
    if (g_entries[*entry].toggle || g_entries[*entry].button)
    {
        wanted = value != "0" ? 1 : 0;
    }
    else if (std::from_chars(value.data(), value.data() + value.size(), wanted).ec != std::errc{} ||
             wanted == g_entries[*entry].last)
    {
        // Ours, and nothing to do: not a number, or the slider repeating
        // the value it was just told (it sends whenever its value changes,
        // a delivered one included; seen in play as a "change" to the same
        // value as the screen opened).
        return true;
    }
    g_clicks.push_back({*entry, wanted});
    return true;
}

// Each frame: the callbacks, on the frame's thread, and what they answer
// queued for the movies.
// An option's value as the plugin's get says it, 0 or 1 for a toggle; the
// last known if the get throws.
std::int32_t ValueOf(const Entry &entry)
{
    std::int32_t value = entry.last;
    detail::Guard("an option's get",
                  [&] { value = entry.toggle ? static_cast<std::int32_t>(entry.toggle->get()) : entry.choice->get(); });
    return value;
}

// A click's wanted value given to the plugin's set, a choice's kept to its
// stops; the value the option then has, which a refusal leaves as it was. A
// button's click is its `pressed`, and it has no value.
std::int32_t Apply(const Entry &entry, std::int32_t wanted)
{
    std::int32_t value = entry.last;
    if (entry.button)
    {
        detail::Guard("a button's pressed", [&] { entry.button->pressed(); });
        return value;
    }
    detail::Guard("an option's set", [&] {
        if (entry.toggle)
        {
            value = entry.toggle->set(wanted != 0) ? 1 : 0;
        }
        else
        {
            const auto top = static_cast<std::int32_t>(entry.choice->values.size()) - 1;
            value = entry.choice->set(std::clamp(wanted, 0, top));
        }
    });
    return value;
}

// Cancel's revert (DAOLib #272, the user's word through lib-agent, 2026-10-30): as the screen
// opens, each option's value is taken; when it closes with no commit since, an option that changed
// is given that value back through its set, as a click back to it would. On the game thread.
void WatchScreen()
{
    const auto shown = gui::ShownScreens();
    const bool now = std::ranges::any_of(shown, [](const gui::Screen &screen) { return screen.name == L"Options"; });
    std::vector<Entry> entries;
    std::vector<std::optional<std::int32_t>> opened;
    {
        const std::lock_guard lock(g_mutex);
        if (now == g_screenShown)
        {
            return;
        }
        g_screenShown = now;
        entries = g_entries;
        if (now)
        {
            g_committed = false;
        }
        else if (!g_committed)
        {
            opened = g_opened;
        }
    }
    if (now)
    {
        std::vector<std::optional<std::int32_t>> values;
        values.reserve(entries.size());
        for (const Entry &entry : entries)
        {
            values.push_back(entry.button ? std::nullopt : std::optional(ValueOf(entry)));
        }
        const std::lock_guard lock(g_mutex);
        g_opened = std::move(values);
        return;
    }
    std::vector<std::pair<std::size_t, std::int32_t>> values;
    for (std::size_t i = 0; i < entries.size() && i < opened.size(); ++i)
    {
        if (opened[i] && *opened[i] != entries[i].last)
        {
            values.emplace_back(i, Apply(entries[i], *opened[i]));
        }
    }
    const std::lock_guard lock(g_mutex);
    for (const auto &[entry, value] : values)
    {
        g_entries[entry].last = value;
    }
}

void Frame()
{
    WatchScreen();
    std::vector<Observer> asked;
    std::vector<Click> clicks;
    std::vector<Entry> entries;
    std::vector<Observer> observers;
    {
        const std::lock_guard lock(g_mutex);
        if (g_asked.empty() && g_clicks.empty())
        {
            return;
        }
        // A closed movie's observers are let go: its record is gone.
        const std::vector<void *> owners = gui::Owners();
        std::erase_if(g_observers, [&](const Observer &observer) {
            return std::ranges::find(owners, observer.owner) == owners.end();
        });
        for (const Observer &observer : g_observers)
        {
            if (std::ranges::find(g_asked, observer.id) != g_asked.end())
            {
                asked.push_back(observer);
            }
        }
        g_asked.clear();
        clicks = std::move(g_clicks);
        g_clicks.clear();
        entries = g_entries;
        observers = g_observers;
    }

    std::vector<Delivery> deliveries;
    std::vector<std::pair<std::size_t, std::int32_t>> values; // to remember, by entry
    for (const Observer &observer : asked)
    {
        const std::int32_t value = ValueOf(entries[observer.entry]);
        deliveries.push_back({observer.owner, observer.id, value});
        values.emplace_back(observer.entry, value);
    }
    for (const Click &click : clicks)
    {
        // What the option now is goes to every control that shows it, the
        // one clicked among them: a refusal draws it back.
        const std::int32_t value = Apply(entries[click.entry], click.wanted);
        for (const Observer &observer : observers)
        {
            if (observer.entry == click.entry)
            {
                deliveries.push_back({observer.owner, observer.id, value});
            }
        }
        values.emplace_back(click.entry, value);
    }
    const std::lock_guard lock(g_mutex);
    for (const auto &[entry, value] : values)
    {
        g_entries[entry].last = value;
    }
    g_deliveries.insert(g_deliveries.end(), deliveries.begin(), deliveries.end());
}

// The engine telling a movie its tokens' new values (dao/Addresses.h):
// __thiscall on the movie's record, one argument, `ret 4`. Ours go to the
// movie after the engine's, the same way: the observers' ids, the values
// as text, and the movie's own function that hands them out.
using DeliverFn = void(__fastcall *)(void *owner, int edx, std::uint32_t onlyIfReady);
Detour<DeliverFn> g_deliver;

void __fastcall Deliver(void *owner, int edx, std::uint32_t onlyIfReady)
{
    g_deliver.Original()(owner, edx, onlyIfReady);
    std::vector<std::int32_t> ids;
    std::vector<std::wstring> texts;
    {
        const std::lock_guard lock(g_mutex);
        if (g_deliveries.empty())
        {
            return;
        }
        std::erase_if(g_deliveries, [&](const Delivery &delivery) {
            if (delivery.owner != owner)
            {
                return false;
            }
            ids.push_back(static_cast<std::int32_t>(delivery.id));
            texts.push_back(std::to_wstring(delivery.value));
            return true;
        });
    }
    gui::Movie movie(gui::MovieOf(owner));
    if (ids.empty() || !movie)
    {
        return;
    }
    std::vector<const wchar_t *> values;
    values.reserve(texts.size());
    for (const std::wstring &text : texts)
    {
        values.push_back(text.c_str());
    }
    movie.SetArray("TokenManager.BatchTokenReplacementCallbackIDs", std::span<const std::int32_t>(ids));
    movie.SetArray("TokenManager.BatchTokenReplacementValues", std::span<const wchar_t *const>(values));
    movie.Invoke("TokenManager.PerformBatchTokenReplacements");
}

bool Attach()
{
    static std::once_flag once;
    static bool attached = false;
    std::call_once(once, [] {
        attached = g_deliver.Attach(addr::kGuiMovie_DeliverTokens, &Deliver);
        if (attached)
        {
            gui::OnMovieLoad(L"optionsmenu", &Patch);
            gui::InterceptExternalInterface("AddTokenObserver", &OnAddTokenObserver);
            gui::InterceptExternalInterface("GetValue", &OnGetValue);
            gui::InterceptFSCommand("SendMessage", &OnSendMessage);
            OnFrame(&Frame);
        }
    });
    return attached;
}

// The names of a section's controls, in the order they are kept: its checkboxes', its
// choices', its buttons'. Nothing if the section has no control, a control lacks a callback, a
// choice has fewer than two values, or a name is not letters and digits or is given twice.
std::optional<std::vector<std::wstring>> NamesOf(const std::vector<Toggle> &toggles, const std::vector<Choice> &choices,
                                                 const std::vector<Button> &buttons)
{
    const bool complete =
        std::ranges::all_of(toggles, [](const Toggle &toggle) { return toggle.get && toggle.set; }) &&
        std::ranges::all_of(
            choices, [](const Choice &choice) { return choice.get && choice.set && choice.values.size() >= 2; }) &&
        std::ranges::all_of(buttons, [](const Button &button) { return static_cast<bool>(button.pressed); });
    std::vector<std::wstring> names;
    std::ranges::transform(toggles, std::back_inserter(names), &Toggle::name);
    std::ranges::transform(choices, std::back_inserter(names), &Choice::name);
    std::ranges::transform(buttons, std::back_inserter(names), &Button::name);
    if (names.empty() || !complete)
    {
        return std::nullopt;
    }
    for (auto name = names.begin(); name != names.end(); ++name)
    {
        if (!core::ValidActionName(*name) || std::find(names.begin(), name, *name) != name)
        {
            return std::nullopt;
        }
    }
    return names;
}

} // namespace

bool AddSection(std::wstring title, std::vector<Toggle> toggles, std::vector<Choice> choices, Column column, Tab tab,
                std::vector<Button> buttons)
{
    const auto names = NamesOf(toggles, choices, buttons);
    if (!names || tab == Tab::Controls || tab == Tab::Audio || !Attach())
    {
        return false;
    }
    const std::lock_guard lock(g_mutex);
    std::vector<std::string> options;
    for (const std::wstring &name : *names)
    {
        std::string option(detail::kOptionPrefix);
        for (const wchar_t c : name)
        {
            option.push_back(static_cast<char>(c)); // letters and digits
        }
        if (EntryOf(option))
        {
            return false;
        }
        options.push_back(std::move(option));
    }
    const std::size_t section = g_sections.size();
    g_sections.push_back({std::move(title), SectionState::Waiting, column, tab});
    std::size_t next = 0;
    for (Toggle &toggle : toggles)
    {
        g_entries.push_back({std::move(toggle), std::nullopt, std::nullopt, std::move(options[next++]), section, 0});
    }
    for (Choice &choice : choices)
    {
        g_entries.push_back({std::nullopt, std::move(choice), std::nullopt, std::move(options[next++]), section, 0});
    }
    for (Button &button : buttons)
    {
        g_entries.push_back({std::nullopt, std::nullopt, std::move(button), std::move(options[next++]), section, 0});
    }
    return true;
}

} // namespace dao::options

namespace dao::detail
{

bool AttachOptions()
{
    return options::Attach();
}

std::vector<options::EntryState> PluginEntries(options::Tab tab)
{
    using namespace options;
    std::vector<EntryState> found;
    const std::lock_guard lock(g_mutex);
    for (const Entry &entry : g_entries)
    {
        const Section &section = g_sections[entry.section];
        if (section.tab != tab || section.state != SectionState::Shown)
        {
            continue;
        }
        EntryState state;
        state.name = std::wstring(entry.option.begin() + static_cast<std::ptrdiff_t>(detail::kOptionPrefix.size()),
                                  entry.option.end());
        state.tab = tab;
        if (entry.toggle)
        {
            state.label = entry.toggle->label;
            state.value = entry.last;
        }
        else if (entry.choice)
        {
            state.label = entry.choice->label;
            state.kind = Kind::Choice;
            state.value = entry.last;
            state.values = entry.choice->values;
        }
        else if (entry.button)
        {
            state.label = entry.button->label;
            state.kind = Kind::Button;
        }
        found.push_back(std::move(state));
    }
    return found;
}

} // namespace dao::detail

namespace dao::options
{

SectionState StateOf(std::wstring_view title)
{
    const std::lock_guard lock(g_mutex);
    for (const Section &section : g_sections)
    {
        if (section.title == title)
        {
            return section.state;
        }
    }
    return SectionState::Unknown;
}

} // namespace dao::options

namespace dao::detail
{

std::optional<OptionControl> PlacedOption(std::wstring_view name)
{
    std::string option(kOptionPrefix);
    for (const wchar_t c : name)
    {
        if (c > 0x7F)
        {
            return std::nullopt; // a name is letters and digits
        }
        option.push_back(static_cast<char>(c));
    }
    const std::lock_guard lock(options::g_mutex);
    const auto index = options::EntryOf(option);
    if (!index)
    {
        return std::nullopt;
    }
    const options::Entry &entry = options::g_entries[*index];
    const options::Section &section = options::g_sections[entry.section];
    if (section.state != options::SectionState::Shown)
    {
        return std::nullopt;
    }
    OptionControl control{.option = std::move(option), .tab = section.tab};
    if (entry.toggle)
    {
        control.values = 2;
    }
    else if (entry.choice)
    {
        control.kind = options::Kind::Choice;
        control.values = static_cast<std::int32_t>(entry.choice->values.size());
    }
    else
    {
        control.kind = options::Kind::Button;
    }
    return control;
}

} // namespace dao::detail
