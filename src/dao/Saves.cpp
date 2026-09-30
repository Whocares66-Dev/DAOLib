#include "dao/Saves.h"

#include "dao/Addresses.h"
#include "dao/Events.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/String.h"
#include "dao/core/Msvc8.h"

#include <cstddef>
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
            callback(saved);
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
        callback(loaded);
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

} // namespace

std::filesystem::path Slot::Folder() const
{
    // SlotFolder constructs its answer in the string it is given (it calls
    // the engine's constructor on it first), so an empty one will do.
    WString name(character);
    WString folder;
    using Fn = void(__cdecl *)(core::WStringRep *, const core::WStringRep *, std::int32_t, std::int32_t);
    reinterpret_cast<Fn>(Module::Address(addr::kSaveGame_SlotFolder))(folder.Engine(), name.Engine(), number,
                                                                      static_cast<std::int32_t>(kind));
    return std::filesystem::path(std::wstring(folder.View()));
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
