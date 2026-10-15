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
#include "dao/InputDetail.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Guard.h"
#include "dao/Gui.h"
#include "dao/Hook.h"
#include "dao/Messages.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/core/Msvc8.h"
#include "dao/core/OptionsMovie.h"

#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
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
    // The engine's record of the action, GroupedInputs (dao/Addresses.h), once it took it.
    const std::byte *record = nullptr;
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

// The record AddAction made last. Read 2026-10-15 for DAOLib #125 in GOG's exe: AddAction
// (0x00894530) pushes the record it makes onto the vector at the mappings' +0xD8, its first and
// last at +0xDC and +0xE0 (0x0049D170, then its back, 0x00893E20). A record's two keys are the
// words at +0x110 and +0x114 (0x00893810, as the controls' save reads them, 0x00895980).
constexpr std::size_t kRecordsBegin = 0xDC;
constexpr std::size_t kRecordsEnd = 0xE0;
constexpr std::array<std::size_t, 2> kKeyWords{0x110, 0x114};

const std::byte *NewestRecord(void *mappings)
{
    const auto at = reinterpret_cast<std::uintptr_t>(mappings);
    const auto begin = TryRead<std::uintptr_t>(at + kRecordsBegin).value_or(0);
    const auto end = TryRead<std::uintptr_t>(at + kRecordsEnd).value_or(0);
    if (begin == 0 || end <= begin)
    {
        return nullptr;
    }
    return reinterpret_cast<const std::byte *>(TryRead<std::uintptr_t>(end - sizeof(std::uintptr_t)).value_or(0));
}

// A plugin's key maps (DAOLib #130; dao/Addresses.h, TakeKey). Binding a key takes it from every
// action that has it (TakeKey), then gives it to the action bound (its record's SetKey), in
// SetKey and in LoadKeyBindings alike. So the take is noted and, when the key is given, an action
// it was taken from gets it back where the two are in different key maps, the game's own actions
// being in none. While a map is on, every action outside it that has one of its keys is shadowed:
// its mappings, numbered inputs and texts, refused where the modes are asked of them
// (AllowsInput, Allowed), so its keys do nothing. Each plugin's copy of the library does this for
// its own actions, one detour inside the other's; another plugin's actions are outside its maps.
struct Mapped
{
    std::uintptr_t record;
    std::wstring keymap;
};
std::mutex g_keymapMutex;              // taken after g_mutex where both are, never before
std::vector<Mapped> g_mapped;          // this plugin's actions with their records, under g_keymapMutex
std::vector<std::wstring> g_keymapsOn; // under g_keymapMutex
std::atomic<bool> g_anyKeymapOn{false};

// A numbered input's binding unbound while a map is on: the action's record and which of its
// keys, the input system's binding, and the key to give it back.
struct Silenced
{
    std::uintptr_t record;
    std::size_t which;
    std::int32_t binding;
    std::uint32_t key;
};
// What the maps that are on shadow, made again when a key or a map changes.
struct Shadow
{
    std::vector<std::wstring> texts;
    std::vector<Silenced> silenced;
};
Shadow g_shadow; // under g_keymapMutex

void NoteMapped(const Entry &entry)
{
    if (entry.record == nullptr)
    {
        return;
    }
    const std::lock_guard lock(g_keymapMutex);
    g_mapped.push_back({reinterpret_cast<std::uintptr_t>(entry.record), entry.action.keymap});
}

template <class Range, class Value> bool Has(const Range &range, const Value &value)
{
    return std::ranges::find(range, value) != std::ranges::end(range);
}

// The record's map: empty for the game's actions and another plugin's.
std::wstring KeymapOf(std::uintptr_t record)
{
    const auto found = std::ranges::find(g_mapped, record, &Mapped::record);
    return found != g_mapped.end() ? found->keymap : std::wstring();
}

bool IsOn(std::wstring_view keymap)
{
    return !keymap.empty() && std::ranges::find(g_keymapsOn, keymap) != g_keymapsOn.end();
}

void *InputMapper()
{
    using GetFn = void *(__cdecl *)();
    reinterpret_cast<GetFn>(Module::Address(addr::kInputMapper_Get))();
    return *reinterpret_cast<void **>(Module::Address(addr::kInputMapper));
}

// The mapper's text for a handle: its vector at +4 (first +8, last +0xC), 0x28 bytes each, the
// text at +8.
const core::WStringRep *TextOf(std::int32_t handle)
{
    constexpr std::size_t kTextsBegin = 0x8;
    constexpr std::size_t kTextsEnd = 0xC;
    constexpr std::size_t kTextSize = 0x28;
    constexpr std::size_t kText = 0x8;
    const auto mapper = reinterpret_cast<std::uintptr_t>(InputMapper());
    const auto begin = mapper != 0 ? TryRead<std::uintptr_t>(mapper + kTextsBegin).value_or(0) : 0;
    const auto end = mapper != 0 ? TryRead<std::uintptr_t>(mapper + kTextsEnd).value_or(0) : 0;
    const std::size_t count = begin != 0 && end > begin ? (end - begin) / kTextSize : 0;
    if (handle < 0 || std::cmp_greater_equal(handle, count))
    {
        return nullptr;
    }
    return reinterpret_cast<const core::WStringRep *>(begin + (static_cast<std::size_t>(handle) * kTextSize) + kText);
}

std::vector<std::uintptr_t> Records(void *mappings)
{
    const auto at = reinterpret_cast<std::uintptr_t>(mappings);
    const auto begin = at != 0 ? TryRead<std::uintptr_t>(at + kRecordsBegin).value_or(0) : 0;
    const auto end = at != 0 ? TryRead<std::uintptr_t>(at + kRecordsEnd).value_or(0) : 0;
    std::vector<std::uintptr_t> records;
    for (std::uintptr_t slot = begin; begin != 0 && slot < end; slot += sizeof(std::uintptr_t))
    {
        records.push_back(TryRead<std::uintptr_t>(slot).value_or(0));
    }
    std::erase(records, 0U);
    return records;
}

// A record's name, a std::wstring at +0x11C (0x008927F0, which copies it out).
constexpr std::size_t kRecordName = 0x11C;

std::wstring NameOf(std::uintptr_t record)
{
    const auto *name = reinterpret_cast<const core::WStringRep *>(record + kRecordName);
    return std::wstring(name->View());
}

std::array<std::uint32_t, 2> KeysOf(std::uintptr_t record)
{
    return {TryRead<std::uint32_t>(record + kKeyWords[0]).value_or(0),
            TryRead<std::uint32_t>(record + kKeyWords[1]).value_or(0)};
}

// An action's mappings for one of its keys, as its record keeps them (+0xF0 for the first,
// +0x100 for the second): 8 bytes each, a flag, then the input system's binding (the flag 0) or
// the mapper's text.
struct Mapping
{
    bool isText;
    std::int32_t handle;
};

std::vector<Mapping> MappingsOf(std::uintptr_t record, std::size_t which)
{
    constexpr std::size_t kFirstKeyMappings = 0xF0;
    constexpr std::size_t kSecondKeyMappings = 0x100;
    constexpr std::size_t kMappingSize = 8;
    const std::size_t list = record + (which == 0 ? kFirstKeyMappings : kSecondKeyMappings);
    const auto begin = TryRead<std::uintptr_t>(list + 4).value_or(0);
    const auto end = TryRead<std::uintptr_t>(list + 8).value_or(0);
    std::vector<Mapping> mappings;
    for (std::uintptr_t at = begin; begin != 0 && at + kMappingSize <= end; at += kMappingSize)
    {
        mappings.push_back({TryRead<std::uint8_t>(at).value_or(0) != 0, TryRead<std::int32_t>(at + 4).value_or(-1)});
    }
    return mappings;
}

void *InputSystem()
{
    using GetFn = void *(__cdecl *)();
    reinterpret_cast<GetFn>(Module::Address(addr::kInput_Get))();
    return *reinterpret_cast<void **>(Module::Address(addr::kInput));
}

// The input system's binding made again for a key, 0 for none, as the record's SetKey does.
void Bind(std::int32_t binding, std::uint32_t key)
{
    void *input = InputSystem();
    if (input != nullptr)
    {
        using BindFn = void(__fastcall *)(void *, int, const std::int32_t *, const std::uint32_t *);
        reinterpret_cast<BindFn>(Module::Address(addr::kInput_Bind))(input, 0, &binding, &key);
    }
}

// An action with a key of a map that is on: its texts refused, and its numbered inputs' bindings
// for that key unbound, so the input does not answer to it but does to the action's other key.
void ShadowRecord(std::uintptr_t record, const std::vector<std::uint32_t> &keys, Shadow &shadow)
{
    for (const Mapping &mapping : MappingsOf(record, 0))
    {
        const core::WStringRep *text = mapping.isText ? TextOf(mapping.handle) : nullptr;
        if (text != nullptr)
        {
            shadow.texts.emplace_back(text->View());
        }
    }
    const std::array<std::uint32_t, 2> held = KeysOf(record);
    for (std::size_t which = 0; which < held.size(); ++which)
    {
        if (held.at(which) == 0 || !Has(keys, held.at(which)))
        {
            continue;
        }
        for (const Mapping &mapping : MappingsOf(record, which))
        {
            if (!mapping.isText)
            {
                Bind(mapping.handle, 0);
                shadow.silenced.push_back({record, which, mapping.handle, held.at(which)});
            }
        }
    }
}

// The shadow made again, on the game thread under g_keymapMutex: the bindings unbound before
// given back where their action still has that key, then the maps that are on shadowing anew.
void Reshadow()
{
    for (const Silenced &silenced : g_shadow.silenced)
    {
        if (KeysOf(silenced.record).at(silenced.which) == silenced.key)
        {
            Bind(silenced.binding, silenced.key);
        }
    }
    g_shadow = Shadow{};
    if (g_keymapsOn.empty())
    {
        return;
    }
    const std::vector<std::uintptr_t> records = Records(Mappings());
    std::vector<std::uint32_t> keys;
    for (const std::uintptr_t record : records)
    {
        if (IsOn(KeymapOf(record)))
        {
            for (const std::uint32_t key : KeysOf(record))
            {
                if (key != 0)
                {
                    keys.push_back(key);
                }
            }
        }
    }
    std::string shadowed;
    for (const std::uintptr_t record : records)
    {
        if (!IsOn(KeymapOf(record)) &&
            std::ranges::any_of(KeysOf(record), [&](std::uint32_t key) { return Has(keys, key); }))
        {
            ShadowRecord(record, keys, g_shadow);
            shadowed += (shadowed.empty() ? "" : ", ") + Utf8(NameOf(record));
        }
    }
    spdlog::info("input: key maps on shadow {}: {} bindings unbound, {} texts refused",
                 shadowed.empty() ? "nothing" : shadowed, g_shadow.silenced.size(), g_shadow.texts.size());
}

bool ShadowedText(std::wstring_view text)
{
    if (!g_anyKeymapOn.load(std::memory_order_relaxed))
    {
        return false;
    }
    const std::lock_guard lock(g_keymapMutex);
    return Has(g_shadow.texts, text);
}

struct Taken
{
    std::uintptr_t record;
    std::int32_t which;
};
struct Take
{
    std::uint32_t key;
    std::vector<Taken> taken;
    core::WStringRep *name; // the "already bound" action's, for the controls screen
};
thread_local std::optional<Take> t_take;

using TakeKeyFn = void(__fastcall *)(void *mappings, int edx, core::WStringRep *name, const std::uint32_t *key);
Detour<TakeKeyFn> g_takeKey;
using SetRecordKeyFn = void(__fastcall *)(void *record, int edx, const std::uint32_t *key, std::int32_t which);
Detour<SetRecordKeyFn> g_setRecordKey;

void __fastcall TakeKey(void *mappings, int edx, core::WStringRep *name, const std::uint32_t *key)
{
    Take take{key != nullptr ? *key : 0U, {}, name};
    for (const std::uintptr_t record : take.key != 0 ? Records(mappings) : std::vector<std::uintptr_t>())
    {
        const std::array<std::uint32_t, 2> keys = KeysOf(record);
        for (std::size_t which = 0; which < keys.size(); ++which)
        {
            if (keys.at(which) == take.key)
            {
                take.taken.push_back({record, static_cast<std::int32_t>(which)});
            }
        }
    }
    g_takeKey.Original()(mappings, edx, name, key);
    t_take = take.taken.empty() ? std::nullopt : std::optional(std::move(take));
}

void GiveBack(void *record, const std::uint32_t *key);

// A key given to an action: those it was just taken from given theirs back, and the shadow made
// again.
void __fastcall SetRecordKey(void *record, int edx, const std::uint32_t *key, std::int32_t which)
{
    g_setRecordKey.Original()(record, edx, key, which);
    GiveBack(record, key);
    if (g_anyKeymapOn.load(std::memory_order_relaxed))
    {
        const std::lock_guard lock(g_keymapMutex);
        Reshadow();
    }
}

// The key given to the action bound: each it was just taken from in another map gets it back.
void GiveBack(void *record, const std::uint32_t *key)
{
    if (!t_take || key == nullptr || *key != t_take->key)
    {
        return;
    }
    const Take take = std::move(*t_take);
    t_take.reset();
    const auto bound = reinterpret_cast<std::uintptr_t>(record);
    std::uintptr_t kept = 0; // the last it was taken from and kept from, for the message
    for (const Taken &taken : take.taken)
    {
        if (taken.record == bound)
        {
            continue;
        }
        bool share = false;
        {
            const std::lock_guard lock(g_keymapMutex);
            share = KeymapOf(bound) != KeymapOf(taken.record);
        }
        if (share)
        {
            g_setRecordKey.Original()(reinterpret_cast<void *>(taken.record), 0, key, taken.which);
        }
        else
        {
            kept = taken.record;
        }
    }
    // The controls screen's "already bound" line names an action the key was taken from and kept
    // from, or none: the take named the last it took from, which may have it back (DAOTurnBased's
    // test, 2026-10-15: Move backward bound to W named the map's action, not Move forward).
    if (take.name == nullptr)
    {
        return;
    }
    if (kept != 0)
    {
        using AssignFn = void(__fastcall *)(core::WStringRep *, int, const core::WStringRep *);
        reinterpret_cast<AssignFn>(Module::Address(addr::kStdWString_Assign))(
            take.name, 0, reinterpret_cast<const core::WStringRep *>(kept + kRecordName));
    }
    else
    {
        take.name->size = 0;
        const_cast<wchar_t *>(take.name->Characters())[0] = L'\0';
    }
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
    entry.record = NewestRecord(mappings);
    NoteMapped(entry);

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

// Under g_mutex: an action outside a map always answers, one in a map while it is on.
bool KeymapAnswers(std::wstring_view keymap)
{
    if (keymap.empty())
    {
        return true;
    }
    const std::lock_guard lock(g_keymapMutex);
    return IsOn(keymap);
}

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
        return allowed && !ShadowedText(text->View());
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
            if (allowed && KeymapAnswers(g_actions[i].action.keymap))
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
        dao::detail::Guard("an action's callback", pressed);
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
                   g_allowed.Attach(addr::kInputModes_Allowed, &Allowed) &&
                   g_takeKey.Attach(addr::kGroupedInputMappings_TakeKey, &TakeKey) &&
                   g_setRecordKey.Attach(addr::kGroupedInputs_SetKey, &SetRecordKey);
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

namespace
{

// The game's devices, read 2026-10-15 for DAOLib #127 in GOG's exe. ECInput::RawInputWin32 reads
// DirectInput each frame (its slot 1, 0x008804F0): the keyboard's state, 256 bytes by scan code
// from +0x3D, a key down where bit 7 is set (0x0087DE50); the mouse's (ReadMouse, kRawInputWin32_
// ReadMouse) from its buffered records, x and y added into +0x14 and +0x18, its buttons' state
// from +0x188 a byte each, and each notch of the wheel (120 of its units) made an event of code
// 0x16 away from the player or 0x17 toward, kind 0x50000. An event is 16 bytes, the input word,
// the kind, a time and a sequence, pushed onto the vector at +4 of ReadMouse's argument, the
// frame's queue (0x0087EBD0, `mov ecx, edi` with edi the argument). Something else sets +0x14
// and +0x18 back between frames, so the first reading, of their difference frame to frame, saw
// a drag's sign alternate (DAOTurnBased's test, 2026-10-15): each read's own addition is taken
// here, across the call. The
// game's own query (slot 2, 0x0087E350) answers a key only with exactly the modifiers held that
// its word names, so these read the state itself.
constexpr std::size_t kKeys = 0x3D;
constexpr std::size_t kMouseX = 0x14;
constexpr std::size_t kMouseY = 0x18;
constexpr std::size_t kMouseButtons = 0x188;
constexpr std::size_t kEventsBegin = 0x4;
constexpr std::size_t kEventsEnd = 0x8;
constexpr std::size_t kEventSize = 16;
// The kind word carries the device's bit as the input word does (0x00880760 ors in the mouse's,
// 0x20000000): the first reading compared it whole and counted no notch (DAOTurnBased's retest,
// 2026-10-15).
constexpr std::uint32_t kDeviceBits = 0xF0000000;
constexpr std::uint32_t kWheelKind = 0x50000;
constexpr std::uint32_t kWheelAway = 0x16;
constexpr std::uint32_t kWheelToward = 0x17;
constexpr std::size_t kButtons = 5;

std::atomic<std::uintptr_t> g_rawInput{0};
std::atomic<std::int32_t> g_wheelTotal{0};
std::atomic<std::int32_t> g_totalX{0};
std::atomic<std::int32_t> g_totalY{0};
std::atomic<std::int32_t> g_frameX{0};
std::atomic<std::int32_t> g_frameY{0};
std::atomic<std::int32_t> g_frameWheel{0};

using ReadMouseFn = void(__fastcall *)(void *raw, int edx, void *argument);
Detour<ReadMouseFn> g_readMouse;

std::size_t EventCount(std::uintptr_t queue)
{
    const auto begin = TryRead<std::uintptr_t>(queue + kEventsBegin).value_or(0);
    const auto end = TryRead<std::uintptr_t>(queue + kEventsEnd).value_or(0);
    return begin != 0 && end >= begin ? (end - begin) / kEventSize : 0;
}

// The mouse read: the device noted, its movement added, and the wheel's notches it made events
// of counted.
void __fastcall ReadMouse(void *raw, int edx, void *argument)
{
    const auto at = reinterpret_cast<std::uintptr_t>(raw);
    const auto queue = reinterpret_cast<std::uintptr_t>(argument);
    g_rawInput.store(at, std::memory_order_relaxed);
    const std::int32_t x = TryRead<std::int32_t>(at + kMouseX).value_or(0);
    const std::int32_t y = TryRead<std::int32_t>(at + kMouseY).value_or(0);
    const std::size_t before = EventCount(queue);
    g_readMouse.Original()(raw, edx, argument);
    g_totalX.fetch_add(TryRead<std::int32_t>(at + kMouseX).value_or(x) - x, std::memory_order_relaxed);
    g_totalY.fetch_add(TryRead<std::int32_t>(at + kMouseY).value_or(y) - y, std::memory_order_relaxed);
    const std::size_t after = EventCount(queue);
    const auto begin = TryRead<std::uintptr_t>(queue + kEventsBegin).value_or(0);
    std::int32_t notches = 0;
    for (std::size_t i = before; i < after && begin != 0; ++i)
    {
        const auto word = TryRead<std::uint32_t>(begin + (i * kEventSize)).value_or(0);
        const auto kind = TryRead<std::uint32_t>(begin + (i * kEventSize) + 4).value_or(0);
        if ((kind & ~kDeviceBits) == kWheelKind && (word & 0xFFFFU) == kWheelAway)
        {
            ++notches;
        }
        else if ((kind & ~kDeviceBits) == kWheelKind && (word & 0xFFFFU) == kWheelToward)
        {
            --notches;
        }
    }
    g_wheelTotal.fetch_add(notches, std::memory_order_relaxed);
}

// Each frame: what the mouse did since the last.
void TakeFrame()
{
    static std::int32_t lastX = 0;
    static std::int32_t lastY = 0;
    static std::int32_t lastWheel = 0;
    const std::int32_t x = g_totalX.load(std::memory_order_relaxed);
    const std::int32_t y = g_totalY.load(std::memory_order_relaxed);
    const std::int32_t wheel = g_wheelTotal.load(std::memory_order_relaxed);
    g_frameX.store(x - lastX, std::memory_order_relaxed);
    g_frameY.store(y - lastY, std::memory_order_relaxed);
    g_frameWheel.store(wheel - lastWheel, std::memory_order_relaxed);
    lastX = x;
    lastY = y;
    lastWheel = wheel;
}

void AttachReadings()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_readMouse.Attach(addr::kRawInputWin32_ReadMouse, &ReadMouse);
        OnFrame(&TakeFrame);
    });
}

// The game's numbered inputs (dao/Addresses.h, kInput_IsDown), by the order 0x00895F00 registers
// their names in: 2 MoveCharacterForward and 43 CursorPosition among them, as read before.
struct Withheld
{
    std::uint32_t reaction;
    std::array<std::int32_t, 11> inputs; // -1 past the last
};
constexpr std::array<Withheld, 4> kWithheld{{
    // AccelerateCharacterLateral and Forward, MoveCharacterForward, Backward, Left, Right,
    // LookAndRunForward.
    {kMovement, {0, 1, 2, 3, 4, 5, 6, -1, -1, -1, -1}},
    // ControlCameraYaw, ControlCameraIncrease and DecreasePitch, CameraControlMode_Yaw and _Pitch,
    // CameraControlMode (the right mouse), ControlCameraLeft and Right, FreeLookMode,
    // MoveCameraLeft and Right.
    {kCameraTurn, {7, 9, 10, 11, 12, 14, 21, 22, 25, 26, 27}},
    // ControlCameraZoom, CameraControlMode_Zoom, ControlCameraZoomIn and Out.
    {kCameraZoom, {8, 13, 23, 24, -1, -1, -1, -1, -1, -1, -1}},
    // CameraPanMode, its _Horizontal and _Vertical, ControlCameraUp and Down, AnalogScrollValue.
    {kCameraPan, {16, 17, 18, 19, 20, 51, -1, -1, -1, -1, -1}},
}};
std::atomic<std::uint32_t> g_withheld{0};
// The fly camera's own inputs, FlyCam_Yaw (28) to FlyCam_ElevateRatio (42), withheld while the
// free view holds its mode (dao/Camera.cpp).
constexpr std::int32_t kFirstFlyCamera = 28;
constexpr std::int32_t kLastFlyCamera = 42;
std::atomic<bool> g_flyWithheld{false};

using AllowsInputFn = bool(__fastcall *)(void *modes, int edx, const std::int32_t *id);
Detour<AllowsInputFn> g_allowsInput;

bool __fastcall AllowsInput(void *modes, int edx, const std::int32_t *id)
{
    if (id != nullptr && g_flyWithheld.load(std::memory_order_relaxed) && *id >= kFirstFlyCamera &&
        *id <= kLastFlyCamera)
    {
        return false;
    }
    const std::uint32_t withheld = g_withheld.load(std::memory_order_relaxed);
    if (withheld != 0 && id != nullptr)
    {
        for (const Withheld &entry : kWithheld)
        {
            if ((withheld & entry.reaction) != 0 && std::ranges::find(entry.inputs, *id) != entry.inputs.end())
            {
                return false;
            }
        }
    }
    return g_allowsInput.Original()(modes, edx, id);
}

} // namespace

// Read 2026-10-15 (above, and dao/Addresses.h, kInput_IsDown): every IsDown the game asks of an
// input, and its axes (kInput_AxisPair), ask InputModes::AllowsInput first and answer nothing
// where it says no, so a withheld input is one it says no to. The game's actions, the keys bound
// to messages and a plugin's own, go by their texts (kInputModes_Allowed) and are not withheld.
bool Down(Key key)
{
    AttachReadings();
    const std::uintptr_t raw = g_rawInput.load(std::memory_order_relaxed);
    const auto scan = static_cast<std::uint32_t>(key);
    return raw != 0 && scan < 256 && (TryRead<std::uint8_t>(raw + kKeys + scan).value_or(0) & 0x80U) != 0;
}

bool Down(MouseButton button)
{
    AttachReadings();
    const std::uintptr_t raw = g_rawInput.load(std::memory_order_relaxed);
    const auto index = static_cast<std::size_t>(button);
    return raw != 0 && index < kButtons &&
           (TryRead<std::uint8_t>(raw + kMouseButtons + index).value_or(0) & 0x80U) != 0;
}

// An action's keys read from its record (NewestRecord, above) against the keyboard Down reads.
bool Held(std::wstring_view name)
{
    constexpr std::uint32_t kDevice = 0xF0000000;
    constexpr std::uint32_t kModifiers = kShift | kControl | kAlt;
    constexpr std::uint32_t kScanCode = 0xFF;
    std::array<std::uint32_t, 2> words{};
    std::vector<GameMode> modes;
    {
        const std::lock_guard lock(g_mutex);
        const auto found = std::ranges::find_if(g_actions, [&](const Entry &e) { return e.action.name == name; });
        if (found == g_actions.end() || found->state != ActionState::Registered || found->record == nullptr)
        {
            return false;
        }
        const auto record = reinterpret_cast<std::uintptr_t>(found->record);
        for (std::size_t i = 0; i < words.size(); ++i)
        {
            words.at(i) = TryRead<std::uint32_t>(record + kKeyWords.at(i)).value_or(0);
        }
        modes = found->action.modes;
        if (!KeymapAnswers(found->action.keymap))
        {
            return false;
        }
    }
    auto *app = CClientExoApp::Get();
    auto *controller = app != nullptr ? app->GameModes() : nullptr;
    if (controller == nullptr || std::ranges::find(modes, controller->Mode()) == modes.end())
    {
        return false;
    }
    const auto modifierDown = [](std::uint32_t modifier, Key left, Key right) {
        return Down(left) || Down(right) ? modifier : 0U;
    };
    const std::uint32_t modifiers = modifierDown(kShift, Key::LeftShift, Key::RightShift) |
                                    modifierDown(kControl, Key::LeftControl, Key::RightControl) |
                                    modifierDown(kAlt, Key::LeftAlt, Key::RightAlt);
    return std::ranges::any_of(words, [&](std::uint32_t word) {
        return (word & kDevice) == core::kKeyboard && (word & kScanCode) != 0 &&
               (word & kModifiers & ~modifiers) == 0 && Down(static_cast<Key>(word & kScanCode));
    });
}

MouseMove MouseDelta()
{
    AttachReadings();
    return {g_frameX.load(std::memory_order_relaxed), g_frameY.load(std::memory_order_relaxed)};
}

std::int32_t WheelNotches()
{
    AttachReadings();
    return g_frameWheel.load(std::memory_order_relaxed);
}

namespace
{

// The wheel zooms by the key bindings ZoomIn and ZoomOut, messages to explore mode and not
// inputs AllowsInput is asked of: the wheel still zoomed while kCameraZoom held the zoom inputs
// (DAOTurnBased's test, 2026-10-15). So their messages are taken as well.
void AttachWithholding()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_allowsInput.Attach(addr::kInputModes_AllowsInput, &AllowsInput);
        const auto zoomWithheld = [](void *, const messages::Message &) {
            return (g_withheld.load(std::memory_order_relaxed) & kCameraZoom) != 0;
        };
        messages::InterceptMessage(messages::CameraZoomInMessage::kVftable, zoomWithheld);
        messages::InterceptMessage(messages::CameraZoomOutMessage::kVftable, zoomWithheld);
    });
}

} // namespace

void Withhold(std::uint32_t reactions)
{
    AttachWithholding();
    g_withheld.store(reactions, std::memory_order_relaxed);
}

void detail::WithholdFlyCamera(bool withheld)
{
    AttachWithholding();
    g_flyWithheld.store(withheld, std::memory_order_relaxed);
}

void SetKeymap(std::wstring_view keymap, bool on)
{
    if (keymap.empty())
    {
        return;
    }
    AttachWithholding();
    const std::lock_guard lock(g_keymapMutex);
    std::erase(g_keymapsOn, keymap);
    if (on)
    {
        g_keymapsOn.emplace_back(keymap);
    }
    g_anyKeymapOn.store(!g_keymapsOn.empty(), std::memory_order_relaxed);
    Reshadow();
}

} // namespace dao::input
