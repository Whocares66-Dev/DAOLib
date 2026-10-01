#include "dao/Options.h"

#include "dao/Addresses.h"
#include "dao/Frame.h"
#include "dao/Guard.h"
#include "dao/Gui.h"
#include "dao/Hook.h"
#include "dao/String.h"
#include "dao/core/InputCode.h"
#include "dao/core/OptionsMovie.h"

#include <algorithm>
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
// these meet neither.
constexpr std::uint32_t kFirstObserver = 0x40000000;

struct Entry
{
    Toggle toggle;
    std::string option; // as the movie has it: "DAOLibTurnBasedCombat"
    std::size_t section = 0;
};

struct Section
{
    std::wstring title;
    SectionState state = SectionState::Waiting;
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
    bool wanted = false;
};

struct Delivery
{
    void *owner = nullptr;
    std::uint32_t id = 0;
    bool value = false;
};

std::mutex g_mutex;
std::vector<Section> g_sections; // all under g_mutex
std::vector<Entry> g_entries;
std::vector<Observer> g_observers;
std::vector<std::uint32_t> g_asked; // observers to tell their option's value, by id
std::vector<Click> g_clicks;
std::vector<Delivery> g_deliveries;
std::uint32_t g_nextObserver = kFirstObserver;

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
            for (const Entry &entry : g_entries)
            {
                if (entry.section == i)
                {
                    section.toggles.push_back({entry.option, Utf8(entry.toggle.label), Utf8(entry.toggle.tooltip)});
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
        g_sections[i].state = i < shown             ? SectionState::Shown
                              : readable && patched ? SectionState::NoRoom
                                                    : SectionState::Failed;
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
    const std::uint32_t id = g_nextObserver++;
    g_asked.push_back(id);
    g_observers.push_back({call.owner, id, *entry});
    call.Return(gui::Value::Number(id));
    return true;
}

// A click: the FSCommand SendMessage with "ClientOptions.
// SetClientOptionMessage <option> 1" (or 0).
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
    g_clicks.push_back({*entry, value != "0"});
    return true;
}

// Each frame: the callbacks, on the frame's thread, and what they answer
// queued for the movies.
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
            return std::find(owners.begin(), owners.end(), observer.owner) == owners.end();
        });
        for (const Observer &observer : g_observers)
        {
            if (std::find(g_asked.begin(), g_asked.end(), observer.id) != g_asked.end())
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
    for (const Observer &observer : asked)
    {
        bool value = false;
        detail::Guard("an option's get", [&] { value = entries[observer.entry].toggle.get(); });
        deliveries.push_back({observer.owner, observer.id, value});
    }
    for (const Click &click : clicks)
    {
        // What the option now is goes to every box that shows it, the one
        // clicked among them: a refusal draws it back.
        bool value = !click.wanted;
        detail::Guard("an option's set", [&] { value = entries[click.entry].toggle.set(click.wanted); });
        for (const Observer &observer : observers)
        {
            if (observer.entry == click.entry)
            {
                deliveries.push_back({observer.owner, observer.id, value});
            }
        }
    }
    const std::lock_guard lock(g_mutex);
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
    std::vector<const wchar_t *> values;
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
            values.push_back(delivery.value ? L"1" : L"0");
            return true;
        });
    }
    gui::Movie movie(gui::MovieOf(owner));
    if (ids.empty() || !movie)
    {
        return;
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
            gui::InterceptFSCommand("SendMessage", &OnSendMessage);
            OnFrame(&Frame);
        }
    });
    return attached;
}

} // namespace

bool AddSection(std::wstring title, std::vector<Toggle> toggles)
{
    if (toggles.empty())
    {
        return false;
    }
    for (std::size_t i = 0; i < toggles.size(); ++i)
    {
        const Toggle &toggle = toggles[i];
        if (!core::ValidActionName(toggle.name) || !toggle.get || !toggle.set)
        {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j)
        {
            if (toggles[j].name == toggle.name)
            {
                return false;
            }
        }
    }
    if (!Attach())
    {
        return false;
    }
    const std::lock_guard lock(g_mutex);
    std::vector<std::string> options;
    for (const Toggle &toggle : toggles)
    {
        std::string option(kOptionPrefix);
        for (const wchar_t c : toggle.name)
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
    g_sections.push_back({std::move(title), SectionState::Waiting});
    for (std::size_t i = 0; i < toggles.size(); ++i)
    {
        g_entries.push_back({std::move(toggles[i]), std::move(options[i]), section});
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
