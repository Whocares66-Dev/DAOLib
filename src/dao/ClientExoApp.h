// CClientExoApp: the client application object, a singleton the engine
// allocates on first use and keeps at dao::addr::kClientExoApp. The name
// comes from the RTTI of the vftables its constructor stores; the lineage
// is BioWare's Aurora engine (Neverwinter Nights' CExoApp).
//
// An opaque type, the fields known so far read at their offsets, and the
// engine's own functions called at their addresses. Nothing here
// reimplements the engine. The pause state and SetPaused were read from the
// disassembly on 2026-09-29 and seen working in play (probe.log); the
// lookups by id on 2026-09-30 (dev/SCRIPT_COMMANDS.md), checked in play by
// the probe's script checks.
#pragma once

#include "dao/Addresses.h"
#include "dao/GameModes.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/Objects.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace dao
{

class CClientExoApp
{
  public:
    CClientExoApp() = delete;
    CClientExoApp(const CClientExoApp &) = delete;
    CClientExoApp &operator=(const CClientExoApp &) = delete;
    ~CClientExoApp() = delete;

    // The singleton, or null before the engine has made it (at plugin load,
    // for one) and after it is torn down.
    static CClientExoApp *Get()
    {
        return *At<CClientExoApp *>(addr::kClientExoApp);
    }

    // -- objects by id: null for an id that names nothing, or names an
    //    object of another class. __thiscall, the id, `ret 4`, each.

    CGameObject *GetGameObjectById(ObjectId id)
    {
        return Lookup<CGameObject>(addr::kCClientExoApp_GetGameObjectById, id);
    }
    CCObject *GetObjectById(ObjectId id)
    {
        return Lookup<CCObject>(addr::kCClientExoApp_GetObjectById, id);
    }
    CCCreature *GetCreatureById(ObjectId id)
    {
        return Lookup<CCCreature>(addr::kCClientExoApp_GetCreatureById, id);
    }
    CCPlaceable *GetPlaceableById(ObjectId id)
    {
        return Lookup<CCPlaceable>(addr::kCClientExoApp_GetPlaceableById, id);
    }
    CCWayPoint *GetWayPointById(ObjectId id)
    {
        return Lookup<CCWayPoint>(addr::kCClientExoApp_GetWayPointById, id);
    }

    // -- pause

    // The paused flag: the byte at +0xC of the flag object at +0xD8, as the
    // engine's own getter (0x00838210, `mov al, [ecx+0xC]`) reads it.
    bool IsPaused() const
    {
        return Read<std::uint8_t>(kPausedFlag + kFlagValue) != 0;
    }

    // Milliseconds spent paused so far this session: the running total
    // SetPaused keeps at +0xFC, not counting a pause still in progress.
    std::uint32_t PausedMilliseconds() const
    {
        return Read<std::uint32_t>(kPausedTotal);
    }

    // The engine's own pause. `second` is what the pause key passes 1 for;
    // its meaning is not yet read.
    void SetPaused(bool paused, bool second = true)
    {
        using Fn = void(__fastcall *)(CClientExoApp *, int, bool, bool);
        reinterpret_cast<Fn>(Module::Address(addr::kCClientExoApp_SetPaused))(this, 0, paused, second);
    }

    // -- game mode

    // The game mode controller (dao/GameModes.h), made in the application's
    // initialisation; null before.
    GameModeController *GameModes() const
    {
        return Read<GameModeController *>(kModeHolder);
    }
    // The game's mode, or nothing before the controller is made.
    std::optional<GameMode> Mode() const
    {
        const GameModeController *modes = GameModes();
        return modes != nullptr ? std::optional<GameMode>(modes->Mode()) : std::nullopt;
    }
    // The same from anywhere: nothing before the application is up either.
    static std::optional<GameMode> CurrentMode()
    {
        const CClientExoApp *app = Get();
        return app != nullptr ? app->Mode() : std::nullopt;
    }

    // The script commands object (dao/Script.h), made in the application's
    // initialisation and given to the script VM.
    void *Commands() const
    {
        return Read<void *>(kCommands);
    }

    static constexpr std::size_t kPausedFlag = 0xD8;
    static constexpr std::size_t kSecondFlag = 0xE8; // set from SetPaused's `second`, by the disassembly
    static constexpr std::size_t kFlagValue = 0xC;
    static constexpr std::size_t kPausedSince = 0xF8;
    static constexpr std::size_t kPausedTotal = 0xFC;
    // The GameModeController (dao/GameModes.h).
    static constexpr std::size_t kModeHolder = 0x114;
    // `app->[+0x1E4] = new CVirtualMachineCommands` in the application's
    // initialisation (0x005DBE22), the object then handed to the VM.
    static constexpr std::size_t kCommands = 0x1E4;
    static constexpr std::size_t kSize = 0x2F0; // what operator new is asked for

  private:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }

    template <class T> T *Lookup(std::uintptr_t function, ObjectId id)
    {
        using Fn = T *(__fastcall *)(CClientExoApp *, int, ObjectId);
        return reinterpret_cast<Fn>(Module::Address(function))(this, 0, id);
    }
};

} // namespace dao
