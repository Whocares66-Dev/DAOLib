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
#include "dao/Options.h"

#include "dao/Addresses.h"
#include "dao/Frame.h"
#include "dao/Guard.h"
#include "dao/Gui.h"
#include "dao/Hook.h"
#include "dao/String.h"
#include "dao/core/InputCode.h"
#include "dao/core/OptionsMovie.h"

#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
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
// An option's name in the movie: the plugin's, marked as the library's.
constexpr std::string_view kOptionPrefix = "DAOLib";

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

// A checkbox or a choice; a checkbox's values are 0 and 1.
struct Entry
{
    std::optional<Toggle> toggle;
    std::optional<Choice> choice;
    std::string option; // as the movie has it: "DAOLibTurnBasedCombat"
    std::size_t section = 0;
    std::int32_t last = 0; // the value last got or set: what a GetValue is answered
};

struct Section
{
    std::wstring title;
    SectionState state = SectionState::Waiting;
    Column column = Column::Either;
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

// The options movie as the game opens it: the sections placed.
std::optional<std::vector<std::byte>> Patch(std::span<const std::byte> movie)
{
    std::vector<core::options_movie::Section> sections;
    {
        const std::lock_guard lock(g_mutex);
        for (std::size_t i = 0; i < g_sections.size(); ++i)
        {
            core::options_movie::Section section;
            section.title = Utf8(g_sections[i].title);
            section.column = MovieColumn(g_sections[i].column);
            for (const Entry &entry : g_entries)
            {
                if (entry.section != i)
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
            }
            sections.push_back(std::move(section));
        }
    }
    std::size_t shown = 0;
    auto patched = core::options_movie::AddSections(movie, sections, shown);
    const std::lock_guard lock(g_mutex);
    const bool readable = core::options_movie::FreeSlots(movie).has_value();
    for (std::size_t i = 0; i < g_sections.size(); ++i)
    {
        if (i < shown)
        {
            g_sections[i].state = SectionState::Shown;
        }
        else
        {
            // AddSections gives nothing both when the movie is not as read and when no
            // section fits: in a movie that reads, a section not shown had no room
            // (DAOLib #46).
            g_sections[i].state = readable ? SectionState::NoRoom : SectionState::Failed;
        }
    }
    return patched;
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
// SetClientOptionMessage <option> 1" (or 0), or a choice's index.
bool OnSendMessage(void * /*owner*/, std::string_view args)
{
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
    if (g_entries[*entry].toggle)
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
// stops; the value the option then has, which a refusal leaves as it was.
std::int32_t Apply(const Entry &entry, std::int32_t wanted)
{
    std::int32_t value = entry.last;
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

void Frame()
{
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

} // namespace

bool AddSection(std::wstring title, std::vector<Toggle> toggles, std::vector<Choice> choices, Column column)
{
    if (toggles.empty() && choices.empty())
    {
        return false;
    }
    std::vector<std::wstring> names;
    for (const Toggle &toggle : toggles)
    {
        if (!toggle.get || !toggle.set)
        {
            return false;
        }
        names.push_back(toggle.name);
    }
    for (const Choice &choice : choices)
    {
        if (!choice.get || !choice.set || choice.values.size() < 2)
        {
            return false;
        }
        names.push_back(choice.name);
    }
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (!core::ValidActionName(names[i]) || std::find(names.begin(), names.begin() + static_cast<std::ptrdiff_t>(i),
                                                          names[i]) != names.begin() + static_cast<std::ptrdiff_t>(i))
        {
            return false;
        }
    }
    if (!Attach())
    {
        return false;
    }
    const std::lock_guard lock(g_mutex);
    std::vector<std::string> options;
    for (const std::wstring &name : names)
    {
        std::string option(kOptionPrefix);
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
    g_sections.push_back({std::move(title), SectionState::Waiting, column});
    std::size_t next = 0;
    for (Toggle &toggle : toggles)
    {
        g_entries.push_back({std::move(toggle), std::nullopt, std::move(options[next++]), section, 0});
    }
    for (Choice &choice : choices)
    {
        g_entries.push_back({std::nullopt, std::move(choice), std::move(options[next++]), section, 0});
    }
    return true;
}

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
