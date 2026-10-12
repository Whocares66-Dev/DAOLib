// A plugin's own actions and the pointed cursor (dao/Input.h): how the
// game registers its own actions, and how a plugin's goes the same way.
//
// How the game does its own, read 2026-10-01 (dao/Addresses.h):
//
// - At start-up the game mode controller (0x00641870) registers each action
//   by name with the text the controls screen shows for it
//   (GroupedInputMappings::AddAction), and what its keys send: a text naming
//   a message, "UserTogglePauseMessage" (AddMapping).
// - It then gives the actions their keys: the defaults (ResetKeys, which
//   the controls screen's defaults button calls too), then the player's
//   own from KeyBindings.ini, whose keys are the actions' names
//   (TogglePause_0, TogglePause_1 for an action's two keys).
// - A pressed key's texts are made into messages and sent, but only those
//   the current mode allows (InputModes::Allowed): each mode has a group of
//   texts, and a group that shuts all others out while it is on.
//
// A plugin's action goes the same way, added just after the game's own:
// its name and label, a text no message has ("DAOLibAction <name>",
// dao/core/InputCode.h), that text added to the groups of the modes the
// plugin names, and its default key after the game's defaults. When the
// key is pressed and the mode allows the text, this takes it before the
// engine looks for a message, and the callback runs at the next frame, on
// the frame's thread (dao/Frame.h), where a plugin may call script
// commands and send messages.
//
// So the action is the game's in every way but what it sends: it is listed
// in the engine's actions, its key is saved in KeyBindings.ini with the
// rest, and it is silent where the game's own keys are (a mode not named,
// or while the engine shuts the keys out).
//
// Seen in play, 2026-10-01, with examples/input/ (the play tester's
// session, the real keyboard):
// - The engine took both actions as the game came up, 7.4 s after load.
// - The default key, F11, ran the callback once a press while exploring,
//   and once for a key held two seconds: the trigger is the press.
// - It was silent with the area map, the main menu and the inventory up,
//   and worked again when they closed. It did run with the game paused
//   (Space) while exploring: a pause is not a mode.
// - The game wrote the actions into KeyBindings.ini at that first start,
//   after its own: "DAOLibExamplePing_0=Keyboard::Button_F11",
//   "DAOLibExamplePing_1=(UNASSIGNED)".
// - The controls screen's Restore Defaults left F11 working, and the
//   game's own keys worked throughout.
// - The controls screen did NOT list the actions. That a line changed in
//   KeyBindings.ini is read back for a plugin's action is not yet seen.
//
// The controls screen, read after: its list is written in its movie's
// script, one AddKeyBinding("TogglePause") for each action it shows, so
// no action the script does not name is listed. This now adds the same
// call for each action the engine took, under a header after the game's
// last section (the action's `section`, "Mods" unless the plugin names its
// own), by patching the movie as the game opens it
// (dao/Gui.h, OnMovieLoad; dao/core/GfxMovie.h). A row made that way is
// the game's own row: the label and the two keys from the engine's data,
// rebound as the game's are. Seen in play the same day, with
// examples/input/: the header and both rows at the list's end; a key bound
// to the row with none (F10) ran its callback, and the other rebound (F11
// to F12) answered the new key and not the old; both were in
// KeyBindings.ini and still bound after a restart; opening the screen
// again doubled nothing; Restore Defaults put the first back to F11 and
// emptied the second; and a row of the game's own still rebound.
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
#include <array>
#include <atomic>
#include <cstddef>
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
            auto section = std::ranges::find_if(
                sections, [&](const core::options_movie::KeySection &s) { return s.title == title; });
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

namespace
{

// The cursor's place a plugin has set, and the input the pick asks for.
constexpr std::int32_t kCursorInput = 43;
std::atomic<bool> g_cursorHeld{false};
std::atomic<std::int64_t> g_cursorAt{0}; // x in the low half, y in the high

using AxisPairFn = std::int32_t *(__fastcall *)(void *input, int edx, std::int32_t *out, const std::int32_t *id);
Detour<AxisPairFn> g_axisPair;

std::int32_t *__fastcall AxisPair(void *input, int edx, std::int32_t *out, const std::int32_t *id)
{
    std::int32_t *answer = g_axisPair.Original()(input, edx, out, id);
    if (g_cursorHeld.load(std::memory_order_relaxed) && id != nullptr && *id == kCursorInput && answer != nullptr)
    {
        const std::int64_t at = g_cursorAt.load(std::memory_order_relaxed);
        answer[0] = static_cast<std::int32_t>(at & 0xFFFFFFFF);
        answer[1] = static_cast<std::int32_t>(at >> 32);
    }
    return answer;
}

// The interface's hover test, which explore mode asks before it keeps a
// pick: while a plugin points the cursor, the world is under it, wherever
// the real mouse is (dao/Gui.h, MouseOverInterface).
using OverInterfaceFn = bool(__fastcall *)(void *manager, int edx);
Detour<OverInterfaceFn> g_overInterface;

bool __fastcall OverInterface(void *manager, int edx)
{
    if (g_cursorHeld.load(std::memory_order_relaxed))
    {
        return false;
    }
    return g_overInterface.Original()(manager, edx);
}

} // namespace

// The point is in the game's pixels, as dao::world::ToScreen gives them,
// and explore mode casts its cursor ray through it, so what it picks there
// (GameModeExplore::CursorObject, CursorGround) is what the player would
// have under a real cursor there. The real mouse is not moved, nor read for
// the pick while this holds.
//
// Read 2026-10-02 (dao/Addresses.h, kInput_AxisPair): the pick asks the
// input system for input 43, an axis pair, and this answers it. While it
// holds, the interface's hover test (dao/Gui.h, MouseOverInterface, which
// explore mode asks before it keeps a pick) answers "not over the
// interface" as well, so the pick holds wherever the real mouse is: seen
// in play the same day, through the Eluvian server, the ray followed the
// pointed pixel (four hits at a creature's, none before) but the pick kept
// nothing, the real mouse being off the game's window and the hover test
// saying "over the interface". The Flash interface itself still follows
// the real mouse (its own hovers, the cursor drawn on the screen), and
// MouseOverInterface answers false while this holds. Seen in play the same
// day with that: the real mouse on another monitor and never moved, a
// creature's pixel gave CursorObject that creature within 0.3 s, a ground
// point gave CursorGround, and ReleaseCursor gave the pick back (the
// Eluvian server); and with the turn-based mod's preview, the pointed
// spiders each held the pick and its path, plate, caption and the cost on
// the experience bar all drew.
void PointCursor(std::int32_t x, std::int32_t y)
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_axisPair.Attach(addr::kInput_AxisPair, &AxisPair);
        g_overInterface.Attach(addr::kScaleformManager_MouseOverInterface, &OverInterface);
    });
    g_cursorAt.store(static_cast<std::int64_t>(static_cast<std::uint32_t>(x)) | static_cast<std::int64_t>(y) << 32,
                     std::memory_order_relaxed);
    g_cursorHeld.store(true, std::memory_order_relaxed);
}

namespace
{

// The inputs the steering asks of, by Move's order (dao/Addresses.h, kInput_IsDown).
constexpr std::array<std::int32_t, 4> kMoveInputs{2, 3, 4, 5};
std::array<std::atomic<bool>, 4> g_moveHeld{};

using IsDownFn = bool(__fastcall *)(void *input, int edx, const std::int32_t *id);
Detour<IsDownFn> g_isDown;

// The input system asked whether an input is down. A key really down is down; one a plugin
// holds is down where the game would have looked at the key at all, which the modes say.
bool __fastcall IsDown(void *input, int edx, const std::int32_t *id)
{
    if (g_isDown.Original()(input, edx, id))
    {
        return true;
    }
    if (id == nullptr)
    {
        return false;
    }
    for (std::size_t i = 0; i < kMoveInputs.size(); ++i)
    {
        if (*id == kMoveInputs.at(i) && g_moveHeld.at(i).load(std::memory_order_relaxed))
        {
            using AllowsFn = bool(__fastcall *)(void *modes, int edx, const std::int32_t *id);
            return reinterpret_cast<AllowsFn>(Module::Address(addr::kInputModes_AllowsInput))(Modes(), 0, id);
        }
    }
    return false;
}

} // namespace

// The steering asks the input system each frame whether the four inputs are down (dao/Addresses.h,
// kInput_IsDown), and this answers for one a plugin holds, as PointCursor answers for the mouse's
// place: everything after is the engine's own, the direction by the camera, the drive command,
// the stop when nothing is held.
//
// Seen in play 2026-10-12 (examples/input with example_input.walk, GOG's game, Lyna's autosave
// in Redcliffe, no key pressed): each direction held a second and a half in turn took the
// controlled creature from 352.49 338.36 forward to 351.85 345.66, back to 352.44 338.99, left
// to 346.40 337.96 and right to 353.07 338.54, and a second and a half after the last was let
// go it stood 0.08 from there. So inputs 4 and 5 are left and right. The game had gone into a
// fight at the first step, and steering went on in it. Again with the game's window not the
// foreground one all through (Eluvian's `window` said so three times in the walk): the same.
void Hold(Move direction, bool held)
{
    static std::once_flag once;
    std::call_once(once, [] { g_isDown.Attach(addr::kInput_IsDown, &IsDown); });
    g_moveHeld.at(static_cast<std::size_t>(direction)).store(held, std::memory_order_relaxed);
}

void ReleaseCursor()
{
    g_cursorHeld.store(false, std::memory_order_relaxed);
}

} // namespace dao::input
