#include "dao/Input.h"

#include "dao/Addresses.h"
#include "dao/Frame.h"
#include "dao/Guard.h"
#include "dao/Gui.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/String.h"
#include "dao/core/Msvc8.h"
#include "dao/core/OptionsMovie.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace dao::input
{

namespace
{

struct Entry
{
    Action action;
    std::wstring text; // what its keys send: "DAOLibAction <name>"
    std::function<void()> pressed;
    ActionState state = ActionState::Waiting;
    std::uint64_t presses = 0;
};

std::mutex g_mutex;
std::vector<Entry> g_actions;      // under g_mutex
std::vector<std::size_t> g_queued; // under g_mutex: pressed since the last frame, by index
std::atomic<bool> g_gameRegistered{false};

// The text of an action's label as the engine takes it (0x004D1C70 makes one
// from a string reference): a flag that the text is there, the reference,
// the text. With the flag set the engine reads the text and asks no talk
// table (0x004D1EB0).
struct Label
{
    std::uint8_t resolved = 1;
    std::uint8_t unused[3] = {};
    std::int32_t reference = 0;
    core::WStringRep text;
};
static_assert(sizeof(Label) == 0x24, "the engine's talk string is 0x24 bytes");

// The names of the modes' groups of texts, as Input::RegisterInputMessages
// (0x006DC900) names them: the modes' class names.
std::wstring_view GroupOf(GameMode mode)
{
    switch (mode)
    {
    case GameMode::Explore:
        return L"GameModeExplore";
    case GameMode::Combat:
        return L"GameModeCombat";
    case GameMode::Dead:
        return L"GameModeDead";
    case GameMode::Gui:
        return L"GameModeGUI";
    case GameMode::Dialog:
        return L"GameModeDialog";
    case GameMode::Cutscene:
        return L"GameModeCutscene";
    case GameMode::FlyCam:
        return L"GameModeFlyCam";
    case GameMode::Fixed:
        return L"GameModeFixed";
    case GameMode::Pregame:
        return L"GameModePregame";
    case GameMode::Movie:
        return L"GameModeMovie";
    case GameMode::CharGen:
        return L"GameModeCharGen";
    case GameMode::PartyPicker:
        return L"GameModePartyPicker";
    case GameMode::Loading:
        break; // has no group
    }
    return {};
}

// The engine's own, each made on first use.
void *Mappings()
{
    using GetFn = void *(__cdecl *)();
    reinterpret_cast<GetFn>(Module::Address(addr::kGroupedInputMappings_Get))();
    return *reinterpret_cast<void **>(Module::Address(addr::kGroupedInputMappings));
}

void *Modes()
{
    using GetFn = void *(__cdecl *)();
    reinterpret_cast<GetFn>(Module::Address(addr::kInputModes_Get))();
    return *reinterpret_cast<void **>(Module::Address(addr::kInputModes));
}

// Whether the engine has an action of that name, as AddAction asks before
// it makes one: the actions' container (at +0x58) hashes the name (its slot
// 6) and is asked for the hash (its slot 2).
bool HasAction(void *mappings, const core::WStringRep &name)
{
    constexpr std::size_t kActions = 0x58;
    void *actions = static_cast<std::byte *>(mappings) + kActions;
    using HashFn = std::uint32_t(__fastcall *)(void *, int, const core::WStringRep *);
    using HasFn = bool(__fastcall *)(void *, int, std::uint32_t);
    void *const *slots = *static_cast<void *const **>(actions);
    const std::uint32_t hash = reinterpret_cast<HashFn>(slots[6])(actions, 0, &name);
    return reinterpret_cast<HasFn>(slots[2])(actions, 0, hash);
}

// The action, what its keys send, and the modes that allow it.
void AddToEngine(Entry &entry)
{
    void *mappings = Mappings();
    void *modes = Modes();
    if (mappings == nullptr || modes == nullptr)
    {
        return;
    }
    const core::WStringRep name = core::WStringRep::Borrow(entry.action.name);
    if (HasAction(mappings, name))
    {
        entry.state = ActionState::NameTaken;
        return;
    }

    Label label;
    label.text = core::WStringRep::Borrow(entry.action.label);
    using AddActionFn =
        void(__fastcall *)(void *, int, const core::WStringRep *, const Label *, std::uint32_t, std::uint32_t);
    reinterpret_cast<AddActionFn>(Module::Address(addr::kGroupedInputMappings_AddAction))(mappings, 0, &name, &label, 0,
                                                                                          0);

    // What the game's press-once actions are registered with: the
    // keyboard, on a press, and no key yet.
    const std::uint32_t trigger = core::kOnPress | core::kKeyboard;
    const core::WStringRep text = core::WStringRep::Borrow(entry.text);
    const bool flag = false;
    using AddMappingFn = void(__fastcall *)(void *, int, const core::WStringRep *, const std::uint32_t *,
                                            const core::WStringRep *, const bool *);
    reinterpret_cast<AddMappingFn>(Module::Address(addr::kGroupedInputMappings_AddMapping))(mappings, 0, &name,
                                                                                            &trigger, &text, &flag);

    using GroupFn = void *(__fastcall *)(void *, int, const core::WStringRep *);
    using AllowFn = void(__fastcall *)(void *, int, const core::WStringRep *);
    for (const GameMode mode : entry.action.modes)
    {
        const std::wstring_view groupName = GroupOf(mode);
        if (groupName.empty())
        {
            continue;
        }
        const core::WStringRep group = core::WStringRep::Borrow(groupName);
        if (void *found = reinterpret_cast<GroupFn>(Module::Address(addr::kInputModes_Group))(modes, 0, &group))
        {
            reinterpret_cast<AllowFn>(Module::Address(addr::kInputModeGroup_Allow))(found, 0, &text);
        }
    }
    entry.state = ActionState::Registered;
}

void GiveDefaultKey(const Entry &entry)
{
    void *mappings = Mappings();
    if (mappings == nullptr || entry.state != ActionState::Registered || entry.action.key == Key::None)
    {
        return;
    }
    const core::WStringRep name = core::WStringRep::Borrow(entry.action.name);
    const std::uint32_t input = core::KeyboardInput(entry.action.key, entry.action.modifiers);
    // The first of the action's two keys, and not taking the key from any
    // action that has it (the last argument, which the defaults pass 0).
    using SetKeyFn =
        void(__fastcall *)(void *, int, const core::WStringRep *, const std::uint32_t *, std::int32_t, std::uint32_t);
    reinterpret_cast<SetKeyFn>(Module::Address(addr::kGroupedInputMappings_SetKey))(mappings, 0, &name, &input, 0, 0);
}

// The game registering its actions: __cdecl, no arguments.
using VoidFn = void(__cdecl *)();
Detour<VoidFn> g_registerActions;
Detour<VoidFn> g_resetKeys;

void __cdecl RegisterActions()
{
    g_registerActions.Original()();
    const std::lock_guard lock(g_mutex);
    g_gameRegistered = true;
    for (Entry &entry : g_actions)
    {
        AddToEngine(entry);
    }
}

// The defaults: the keys cleared, the game's own set, keymap.gda's read.
// At start-up before KeyBindings.ini, and from the controls screen.
void __cdecl ResetKeys()
{
    g_resetKeys.Original()();
    const std::lock_guard lock(g_mutex);
    for (const Entry &entry : g_actions)
    {
        GiveDefaultKey(entry);
    }
}

// InputModes::Allowed: __thiscall (const std::wstring &), `ret 4`. Its one
// caller is the input mapper, for a pressed key's text.
using AllowedFn = bool(__fastcall *)(void *, int, const core::WStringRep *);
Detour<AllowedFn> g_allowed;

bool __fastcall Allowed(void *modes, int edx, const core::WStringRep *text)
{
    const bool allowed = g_allowed.Original()(modes, edx, text);
    if (text == nullptr)
    {
        return allowed;
    }
    const auto name = core::ActionOfText(text->View());
    if (!name)
    {
        return allowed;
    }
    // A plugin's action, but whose? Each plugin links its own copy of this
    // library and each copy detours this function, one inside the other.
    // An action of another copy's is passed on as the engine answered it,
    // or that copy, further out, would never hear its key was allowed.
    // (Seen in play, 2026-10-01: the turn-based mod and examples/input/
    // together, both plugins' keys working, three headings in the list.)
    const std::lock_guard lock(g_mutex);
    for (std::size_t i = 0; i < g_actions.size(); ++i)
    {
        if (g_actions[i].action.name == *name)
        {
            // Ours: no message has this name, so the engine is told no.
            if (allowed)
            {
                ++g_actions[i].presses;
                g_queued.push_back(i);
            }
            return false;
        }
    }
    return allowed;
}

void RunQueued()
{
    std::vector<std::function<void()>> run;
    {
        const std::lock_guard lock(g_mutex);
        if (g_queued.empty())
        {
            return;
        }
        for (const std::size_t i : g_queued)
        {
            run.push_back(g_actions[i].pressed);
        }
        g_queued.clear();
    }
    for (auto &pressed : run)
    {
        detail::Guard("an action's callback", pressed);
    }
}

// The controls screen's list is written in its movie's script: a header
// for each section and AddKeyBinding("TogglePause") for each action, the
// last of them HideMainInterface (optionsmenu.gfx, read 2026-10-01). So
// the actions the engine took are listed by adding the same calls after
// that one, under a header of their own. A row shows the action's label
// and its two keys from the engine's own data ("InputMappings.Mappings.
// <name>"), and rebinds as the game's rows do.
std::optional<std::vector<std::byte>> ListActions(std::span<const std::byte> movie)
{
    // The actions the engine took, by their sections in the order each
    // was first named.
    std::vector<core::options_movie::KeySection> sections;
    {
        const std::lock_guard lock(g_mutex);
        for (const Entry &entry : g_actions)
        {
            if (entry.state != ActionState::Registered)
            {
                continue;
            }
            const std::string title = Utf8(entry.action.section);
            auto section = std::find_if(sections.begin(), sections.end(),
                                        [&](const core::options_movie::KeySection &s) { return s.title == title; });
            if (section == sections.end())
            {
                section = sections.insert(sections.end(), {title, {}});
            }
            // The names are letters and digits (ValidActionName).
            std::string name;
            for (const wchar_t c : entry.action.name)
            {
                name.push_back(static_cast<char>(c));
            }
            section->actions.push_back(std::move(name));
        }
    }
    return core::options_movie::ListKeys(movie, sections);
}

bool Attach()
{
    static std::once_flag once;
    static bool attached = false;
    std::call_once(once, [] {
        attached = g_registerActions.Attach(addr::kInput_RegisterActions, &RegisterActions) &&
                   g_resetKeys.Attach(addr::kInput_ResetKeys, &ResetKeys) &&
                   g_allowed.Attach(addr::kInputModes_Allowed, &Allowed);
        if (attached)
        {
            OnFrame(&RunQueued);
            gui::OnMovieLoad(L"optionsmenu", &ListActions);
        }
    });
    return attached;
}

} // namespace

bool RegisterAction(Action action, std::function<void()> pressed)
{
    if (!core::ValidActionName(action.name) || !pressed || g_gameRegistered || !Attach())
    {
        return false;
    }
    const std::lock_guard lock(g_mutex);
    if (g_gameRegistered)
    {
        return false;
    }
    for (const Entry &entry : g_actions)
    {
        if (entry.action.name == action.name)
        {
            return false;
        }
    }
    std::wstring text = core::ActionText(action.name);
    g_actions.push_back({std::move(action), std::move(text), std::move(pressed)});
    return true;
}

ActionState StateOf(std::wstring_view name)
{
    const std::lock_guard lock(g_mutex);
    for (const Entry &entry : g_actions)
    {
        if (entry.action.name == name)
        {
            return entry.state;
        }
    }
    return ActionState::Unknown;
}

std::uint64_t Presses(std::wstring_view name)
{
    const std::lock_guard lock(g_mutex);
    for (const Entry &entry : g_actions)
    {
        if (entry.action.name == name)
        {
            return entry.presses;
        }
    }
    return 0;
}

} // namespace dao::input
