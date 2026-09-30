#include "dao/Saves.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/String.h"
#include "dao/core/Msvc8.h"

#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

namespace dao::saves
{

namespace
{

std::mutex g_lock;
std::vector<Observer> g_saved;
std::vector<Observer> g_loading;

std::vector<Observer> Copy(const std::vector<Observer> &observers)
{
    const std::lock_guard lock(g_lock);
    return observers;
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
        for (const auto &observer : Copy(g_saved))
        {
            observer(saved);
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
        const Slot loading{std::wstring(character->View()), slot, static_cast<Kind>(kind)};
        for (const auto &observer : Copy(g_loading))
        {
            observer(loading);
        }
    }
    return g_find.Original()(out, character, slot, kind);
}

void Add(std::vector<Observer> &observers, Observer observer)
{
    const std::lock_guard lock(g_lock);
    observers.push_back(std::move(observer));
    if (g_write.Original() == nullptr)
    {
        g_write.Attach(addr::kSaveGame_Write, &Write);
        g_find.Attach(addr::kSaveGame_FindSave, &Find);
    }
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

void OnSaved(Observer observer)
{
    Add(g_saved, std::move(observer));
}

void OnLoading(Observer observer)
{
    Add(g_loading, std::move(observer));
}

} // namespace dao::saves
