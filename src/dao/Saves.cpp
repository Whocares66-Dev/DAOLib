// A plugin's data with the game's saves, and asking the game to save or load (dao/Saves.h).
//
// What goes in the file is the plugin's own: text, JSON, binary. Each plugin has its own file per
// save, <slot folder>\<plugin>.daolib, so there is nothing to share and no framing. Both
// callbacks run on the game's thread, one at a time, as the game's own saving and loading do, so
// plugin state they share needs no lock. They must not throw: an exception has no way back
// through the game's code.
//
// Read 2026-09-30 (dev/SAVES.md):
//
// - Saving. SaveGame::Write (dao/Addresses.h) writes a save with the
//   character, the slot and the kind resolved; in play a quicksave asked
//   for "any slot" and Write was called for slot 1, kind 2, and returned
//   1. OnSaved runs after Write returns and has succeeded.
// - Loading. A load looks for its file with SaveGame::FindSave, with the
//   same three (in play once per load), before the game reads the save;
//   the world is built after. OnGameLoaded runs at the module's load
//   event (EVENT_TYPE_MODULE_LOAD) that follows, with the save's slot, the
//   world there. The event alone is not enough: BioWare's module_core.nss
//   says it "can fire more than once for a single module", so a load
//   event with no save found before it is not a load and calls nothing.
// - A new game. module_core.nss: EVENT_TYPE_MODULE_START happens "only
//   once for a single game instance". OnGameLoaded runs there with no
//   slot, so a plugin starts a new playthrough from nothing rather than
//   from the one loaded before it.
//
// Seen in play: Write and FindSave with their slots, and the load event
// once per load. The callbacks, seen 2026-10-05 by the turn-based mod (its
// file of each companion's choice): launching straight into a save, a
// manual save written with its file (OnSaved, the slot right), and three
// loads in a running game in a row, each told once with its slot, a save
// with no file of the plugin's read as nothing; then an autosave on a first
// entry into a new area (its slot, AutoSave_3, and its file written in the
// same second as the game's) and that autosave loaded. Not yet seen: a
// quicksave, and the module's start event for a new game. An early
// quicksave in play wrote its files without Write being heard; read
// 2026-10-05, Write is the only code that names a save's files and the
// loading mode's worker its only caller, so every save of every kind goes
// through it, and that one was most likely made before the hook was in
// place. A save deleted from the game's menu takes its folder with it, and
// the plugin's file too.
//
// Asking the game to save or load: the messages its menus and keys send to
// its save proxy (SaveLoadProxy, the application's +0x150; dev/SAVES.md),
// read 2026-09-30. From the main menu or in the world, on the game's thread
// (a frame callback). Seen in play, 2026-10-01, through the Eluvian server:
// Characters, List and Meta listed the ten saves there, and Load of a
// manual save, asked at the main menu, had it loaded and in explore mode 14
// s later with no menu touched, OnGameLoaded called. A second, with no
// add-ins in the way, was asked 2 s after the menu came up and was loaded
// 11 s after the process started. OnGameLoaded is called while the game's
// mode still reads loading, explore mode coming a moment after: a caller
// that wants the world in play waits for the mode as well. Save is not yet
// seen.
#include "dao/Saves.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Data.h"
#include "dao/Events.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Messages.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/core/Msvc8.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <system_error>
#include <utility>
#include <vector>

namespace dao::saves
{

namespace
{

using SavedCallback = std::function<void(const Slot &)>;
using LoadedCallback = std::function<void(const std::optional<Slot> &)>;

std::mutex g_lock;
std::vector<SavedCallback> g_saved;
std::vector<LoadedCallback> g_loaded;
// The save a load found, until the module's load event says the world is
// there. Only the game's thread touches it.
std::optional<Slot> g_loading;

template <class T> std::vector<T> Copy(const std::vector<T> &callbacks)
{
    const std::lock_guard lock(g_lock);
    return callbacks;
}

// SaveGame::Write: __thiscall (slot, kind, const std::wstring &name, ?),
// `ret 0x10`, 1 when written; the character's name at the object's +0x18.
using WriteFn = int(__fastcall *)(void *self, int edx, std::int32_t slot, std::int32_t kind,
                                  const core::WStringRep *name, std::int32_t fourth);
Detour<WriteFn> g_write;
constexpr std::size_t kCharacter = 0x18;

int __fastcall Write(void *self, int edx, std::int32_t slot, std::int32_t kind, const core::WStringRep *name,
                     std::int32_t fourth)
{
    const int written = g_write.Original()(self, edx, slot, kind, name, fourth);
    if (written != 0 && self != nullptr)
    {
        const auto &character =
            *reinterpret_cast<const core::WStringRep *>(static_cast<std::byte *>(self) + kCharacter);
        const Slot saved{std::wstring(character.View()), slot, static_cast<Kind>(kind)};
        for (const auto &callback : Copy(g_saved))
        {
            detail::Guard("a save callback", [&] { callback(saved); });
        }
    }
    return written;
}

// SaveGame::FindSave: __cdecl (out, const std::wstring &character, slot,
// kind), the save's file in the slot's folder, for a load.
using FindFn = int(__cdecl *)(void *out, const core::WStringRep *character, std::int32_t slot, std::int32_t kind);
Detour<FindFn> g_find;

int __cdecl Find(void *out, const core::WStringRep *character, std::int32_t slot, std::int32_t kind)
{
    if (character != nullptr)
    {
        g_loading = Slot{std::wstring(character->View()), slot, static_cast<Kind>(kind)};
    }
    return g_find.Original()(out, character, slot, kind);
}

void Loaded(const std::optional<Slot> &loaded)
{
    for (const auto &callback : Copy(g_loaded))
    {
        detail::Guard("a game-loaded callback", [&] { callback(loaded); });
    }
}

void AttachSaving()
{
    static std::once_flag once;
    std::call_once(once, [] { g_write.Attach(addr::kSaveGame_Write, &Write); });
}

void AttachLoading()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_find.Attach(addr::kSaveGame_FindSave, &Find);
        events::OnScriptEvent(script::EventType::ModuleStart, [](ObjectId, const CScriptEvent &) {
            g_loading.reset();
            Loaded(std::nullopt);
        });
        events::OnScriptEvent(script::EventType::ModuleLoad, [](ObjectId, const CScriptEvent &) {
            if (g_loading)
            {
                const Slot loaded = *std::exchange(g_loading, std::nullopt);
                Loaded(loaded);
            }
        });
    });
}

// The save proxy (dev/SAVES.md), the application's +0x150, and in it
// SmartData attributes, each holding its value at its +0xC, read
// 2026-09-30 from its handlers:
// - +0xAC, the save screens' mode: 0 none, 2 the load screen's. Setting
//   it (SetSaveLoadModeMessage, 0x006D1A50) to 1 or 2 makes the first
//   character in the list the chosen one (in the world, the one played)
//   and refreshes the saves; the load handler does that itself unless the
//   mode is 2, which is how the load screen keeps a choice. Each save and
//   load handler sets it back to 0 once it has begun.
// - +0xE4, the chosen character's folder name, a std::wstring: whose
//   saves a load means. SelectHeroMessage sets it from the list.
// - +0x150, the characters, a vector of records whose +0x98 is the folder
//   name (a std::wstring); RefreshHeroesMessage fills it from the folders.
constexpr std::size_t kSaveLoadProxy = 0x150;
constexpr std::size_t kAttributeValue = 0xC;
constexpr std::size_t kMode = 0xAC;
constexpr std::size_t kChosen = 0xE4;
constexpr std::size_t kHeroes = 0x150;
constexpr std::size_t kHeroFolder = 0x98;
constexpr std::int32_t kLoadScreen = 2;
// CClientExoApp keeps the load it was last asked for, to make again once a
// dialog over missing content is answered (its handler of
// GUIDialogResultMessage, 0x005DAA30, read 2026-10-01): a path at +0x218
// if the load was by path, else the character's folder at +0x234 with the
// slot and kind at +0x250 and +0x254; each a std::wstring. Answer 3 sets
// the byte at +0x298, which lets the load go on without the content, and
// makes the load again from these.
constexpr std::size_t kAskedPath = 0x218;
constexpr std::size_t kAskedCharacter = 0x234;
constexpr std::int32_t kForceLoad = 3;

std::uintptr_t Proxy()
{
    const CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return 0;
    }
    return TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(app) + kSaveLoadProxy).value_or(0);
}

std::optional<std::wstring> ReadString(std::uintptr_t at)
{
    core::WStringRep rep;
    if (!TryCopy(&rep, at, sizeof rep) || rep.size > 4096)
    {
        return std::nullopt;
    }
    if (rep.capacity <= core::WStringRep::kInlineCapacity)
    {
        return std::wstring(rep.View());
    }
    std::wstring text(rep.size, L'\0');
    if (rep.size != 0 &&
        !TryCopy(text.data(), reinterpret_cast<std::uintptr_t>(rep.data.pointer), rep.size * sizeof(wchar_t)))
    {
        return std::nullopt;
    }
    return text;
}

std::vector<std::wstring> Heroes(std::uintptr_t proxy)
{
    std::vector<std::wstring> heroes;
    const auto list = TryRead<core::VectorRep<std::uint32_t>>(proxy + kHeroes + kAttributeValue);
    if (!list || list->first == nullptr || list->last < list->first || list->last - list->first > 1000)
    {
        return heroes;
    }
    for (const std::uint32_t *at = list->first; at != list->last; ++at)
    {
        const auto record = TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(at));
        if (record && *record != 0)
        {
            heroes.push_back(ReadString(*record + kHeroFolder).value_or(L""));
        }
    }
    return heroes;
}

bool SendToProxy(std::uintptr_t proxy, const messages::Message &message)
{
    return messages::Send(reinterpret_cast<void *>(proxy), message);
}

messages::SetSaveLoadModeMessage ModeMessage(std::int32_t mode)
{
    auto message = messages::Make<messages::SetSaveLoadModeMessage>();
    message.mode = mode;
    return message;
}

std::int32_t Mode(std::uintptr_t proxy)
{
    return TryRead<std::int32_t>(proxy + kMode + kAttributeValue).value_or(0);
}

} // namespace

// Read from the save proxy's list, which the game fills from the folders when its load screen
// opens; asking refreshes it first if it is empty.
std::vector<std::wstring> Characters()
{
    const std::uintptr_t proxy = Proxy();
    if (proxy == 0)
    {
        return {};
    }
    auto heroes = Heroes(proxy);
    if (heroes.empty())
    {
        SendToProxy(proxy, messages::Make<messages::RefreshHeroesMessage>());
        heroes = Heroes(proxy);
    }
    return heroes;
}

std::wstring Character()
{
    const std::uintptr_t proxy = Proxy();
    return proxy != 0 ? ReadString(proxy + kChosen + kAttributeValue).value_or(L"") : L"";
}

std::vector<Slot> List(std::wstring_view character)
{
    std::vector<Slot> slots;
    // The Saves folder holding the slots' folders, as the game names it.
    const auto saves = Slot{std::wstring(character), 1, Kind::Manual}.Folder().parent_path();
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(saves, error))
    {
        const auto name = core::SlotFolderName::Parse(entry.path().filename().wstring());
        if (!name || !entry.is_directory(error))
        {
            continue;
        }
        Slot slot{std::wstring(character), name->number, static_cast<Kind>(name->kind)};
        if (slot.Exists())
        {
            slots.push_back(std::move(slot));
        }
    }
    std::ranges::sort(slots, {}, [](const Slot &slot) { return std::pair(slot.kind, slot.number); });
    return slots;
}

// The load screen's mode set, the character chosen, then LoadGameMessage. A save that needs
// content the game does not have registered stops at the game's own "requires the following
// content" dialog (Force Load or Cancel): Load has answered true and nothing follows until the
// player answers it (in play, 2026-10-01, after another install of the game had rewritten
// Settings\AddIns.xml). Seeing that dialog from a plugin is not read; a caller that must know
// waits for OnGameLoaded with a time limit.
bool Load(const Slot &save)
{
    const std::uintptr_t proxy = Proxy();
    if (proxy == 0)
    {
        return false;
    }
    const std::wstring character = save.character.empty() ? Character() : save.character;
    if (character.empty() || !Slot{character, save.number, save.kind}.Exists())
    {
        return false;
    }
    // As the load screen: its mode, which keeps the choice, then the
    // character by its place in the game's list.
    const std::int32_t before = Mode(proxy);
    SendToProxy(proxy, ModeMessage(kLoadScreen));
    const auto heroes = Characters();
    const auto found = std::ranges::find(heroes, character);
    if (found == heroes.end())
    {
        SendToProxy(proxy, ModeMessage(before));
        return false;
    }
    auto select = messages::Make<messages::SelectHeroMessage>();
    select.index = static_cast<std::int32_t>(found - heroes.begin());
    SendToProxy(proxy, select);
    auto load = messages::Make<messages::LoadGameMessage>();
    load.slot = save.number;
    load.kind = static_cast<std::int32_t>(save.kind);
    SendToProxy(proxy, load);
    // A load that began set the mode back itself; one declined left it.
    if (Mode(proxy) == kLoadScreen && before != kLoadScreen)
    {
        SendToProxy(proxy, ModeMessage(before));
    }
    return true;
}

// A save made with content this install lacks is not loaded when asked for: the game puts up
// "This saved game cannot be loaded. It requires the following content", with Force Load and
// Cancel, and waits. Seen 2026-10-01: a save made in the GOG install, asked for in the EA app's,
// which has no Collector's Edition content (dev/BUILDS.md). This is the Force Load button, the
// dialog's answer to the application (GUIDialogResultMessage, 3): the dialog closes and the game
// makes the load it was last asked for again, this time without that content, so a save made
// from the game then loaded no longer has it.
bool ForceLoad()
{
    CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return false;
    }
    const auto at = reinterpret_cast<std::uintptr_t>(app);
    if (ReadString(at + kAskedPath).value_or(L"").empty() && ReadString(at + kAskedCharacter).value_or(L"").empty())
    {
        return false; // no load was ever asked for
    }
    // The answer's text as the engine's empty string; the handler does not
    // read it, and it holds nothing to free.
    WString none(L"");
    auto answer = messages::Make<messages::GUIDialogResultMessage>();
    answer.result = kForceLoad;
    std::memcpy(answer.text, none.Engine(), sizeof answer.text);
    return messages::Send(app, answer);
}

// SaveGameMessage, as the save screen and the quicksave key send it. A slot of -1 for a manual
// save is chosen here; the quicksave and the autosave have their own.
//
// Seen in play 2026-10-02 through the Eluvian server, and changed for it:
// a manual save to a free slot was written, OnSaved called 23 s after; but
// -1 was refused, the game's own next free slot (SaveLoad.OpenSlot, the
// proxy's +0x6C) being worked out by the save screen alone, and the name
// was lost, the file "savegame". The handler (0x006D1C60) hands the game
// the save screen's SaveLoad.SaveName as well as the message's name, and
// the screen's is the one written; so the name is set there first, as the
// screen sets it, and -1 is a slot chosen here. Both changes are not yet
// seen in play.
bool Save(Kind kind, std::int32_t slot, std::wstring_view name)
{
    const std::uintptr_t proxy = Proxy();
    if (proxy == 0)
    {
        return false;
    }
    if (kind == Kind::Manual && slot < 0)
    {
        // The game's next free slot is the save screen's to work out; the
        // lowest the character has no save in is the same choice.
        constexpr std::int32_t kManualSlots = 500;
        const std::wstring character = Character();
        std::vector<std::int32_t> taken;
        for (const Slot &save : List(character))
        {
            if (save.kind == Kind::Manual)
            {
                taken.push_back(save.number);
            }
        }
        slot = 1;
        while (slot <= kManualSlots && std::ranges::find(taken, slot) != taken.end())
        {
            ++slot;
        }
        if (slot > kManualSlots)
        {
            return false;
        }
    }
    // The save screen's name for it, which is the one the game writes.
    if (data::Attribute *screenName = data::Find(L"SaveLoad.SaveName"))
    {
        screenName->SetText(name);
    }
    // The name as the engine's string, which the handler copies; this one
    // is freed here, by the engine's own functions (dao/String.h).
    WString text(name);
    auto message = messages::Make<messages::SaveGameMessage>();
    message.slot = slot;
    message.kind = static_cast<std::int32_t>(kind);
    std::memcpy(message.name, text.Engine(), sizeof message.name);
    return SendToProxy(proxy, message);
}

bool Slot::Exists() const
{
    std::error_code error;
    return std::ranges::any_of(std::filesystem::directory_iterator(Folder(), error),
                               [](const auto &entry) { return entry.path().extension() == L".das"; });
}

std::optional<core::SaveMeta> Slot::Meta() const
{
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(Folder(), error))
    {
        const std::wstring file = entry.path().filename().wstring();
        if (!file.ends_with(L".das.met"))
        {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return core::SaveMeta::Parse(std::as_bytes(std::span(bytes.data(), bytes.size())));
    }
    return std::nullopt;
}

std::filesystem::path Slot::Folder() const
{
    // SlotFolder constructs its answer in the string it is given (it calls
    // the engine's constructor on it first), so an empty one will do.
    WString name(character);
    WString folder;
    using Fn = void(__cdecl *)(core::WStringRep *, const core::WStringRep *, std::int32_t, std::int32_t);
    reinterpret_cast<Fn>(Module::Address(addr::kSaveGame_SlotFolder))(folder.Engine(), name.Engine(), number,
                                                                      static_cast<std::int32_t>(kind));
    return {std::wstring(folder.View())};
}

std::filesystem::path Slot::File(std::string_view plugin) const
{
    return Folder() / std::filesystem::path(std::string(plugin) + ".daolib");
}

std::optional<std::string> Slot::Read(std::string_view plugin) const
{
    std::ifstream in(File(plugin), std::ios::binary);
    if (!in)
    {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool Slot::Write(std::string_view plugin, std::string_view data) const
{
    const auto file = File(plugin);
    auto partial = file;
    partial += ".partial";
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        if (!out.write(data.data(), static_cast<std::streamsize>(data.size())))
        {
            return false;
        }
    }
    std::error_code error;
    std::filesystem::rename(partial, file, error); // replaces the old one
    return !error;
}

void OnSaved(std::function<void(const Slot &saved)> callback)
{
    {
        const std::lock_guard lock(g_lock);
        g_saved.push_back(std::move(callback));
    }
    AttachSaving();
}

void OnGameLoaded(std::function<void(const std::optional<Slot> &loaded)> callback)
{
    {
        const std::lock_guard lock(g_lock);
        g_loaded.push_back(std::move(callback));
    }
    AttachLoading();
}

} // namespace dao::saves
