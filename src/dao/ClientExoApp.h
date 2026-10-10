// CClientExoApp, the game's client application object. The game has exactly one, and through it
// you can find objects by id, read and set the pause, and reach the game mode controller and the
// script commands object.

// Notes on CClientExoApp: the engine allocates it on first use and keeps it at
// dao::addr::kClientExoApp. The name comes from the RTTI of the vftables its constructor stores;
// the lineage is BioWare's Aurora engine (Neverwinter Nights' CExoApp). It is an opaque type: the
// fields known so far are read at their offsets, and the engine's own functions are called at
// their addresses; nothing here reimplements the engine. The pause state and SetPaused were read
// from the disassembly on 2026-09-29 and seen working in play (probe.log); the lookups by id on
// 2026-09-30 (dev/SCRIPT_COMMANDS.md), checked in play by the probe's script checks.
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

// The game's client application object. You never construct one: call Get() for the game's.
class CClientExoApp
{
  public:
    CClientExoApp() = delete;
    CClientExoApp(const CClientExoApp &) = delete;
    CClientExoApp &operator=(const CClientExoApp &) = delete;
    ~CClientExoApp() = delete;

    // Returns the application object.
    // @return the object, or null before the engine has created it (for example, while plugins
    //  load) and after it is destroyed
    static CClientExoApp *Get()
    {
        return *At<CClientExoApp *>(addr::kClientExoApp);
    }

    // -- objects by id. Each calls the engine's own lookup: __thiscall, the id, `ret 4`.

    // Finds a game object by id.
    // @return the object, or null if the id names nothing or an object of another class
    CGameObject *GetGameObjectById(ObjectId id)
    {
        return Lookup<CGameObject>(addr::kCClientExoApp_GetGameObjectById, id);
    }
    // Finds an object by id.
    // @return the object, or null if the id names nothing or an object of another class
    CCObject *GetObjectById(ObjectId id)
    {
        return Lookup<CCObject>(addr::kCClientExoApp_GetObjectById, id);
    }
    // Finds a creature by id.
    // @return the creature, or null if the id names nothing or something that is not a creature
    CCCreature *GetCreatureById(ObjectId id)
    {
        return Lookup<CCCreature>(addr::kCClientExoApp_GetCreatureById, id);
    }
    // Finds a placeable by id.
    // @return the placeable, or null if the id names nothing or something that is not a placeable
    CCPlaceable *GetPlaceableById(ObjectId id)
    {
        return Lookup<CCPlaceable>(addr::kCClientExoApp_GetPlaceableById, id);
    }

    // Notes on GetModule: the application keeps the module's id at +0x2B8 and the engine's
    // function (0x005D4FC0) looks it up and casts it.

    // Returns the current module.
    // @return the module, or null before a game is loaded
    CCModule *GetModule()
    {
        using Fn = CCModule *(__fastcall *)(CClientExoApp *);
        return reinterpret_cast<Fn>(Module::Address(addr::kCClientExoApp_GetModule))(this);
    }
    // Finds a waypoint by id.
    // @return the waypoint, or null if the id names nothing or something that is not a waypoint
    CCWayPoint *GetWayPointById(ObjectId id)
    {
        return Lookup<CCWayPoint>(addr::kCClientExoApp_GetWayPointById, id);
    }

    // -- pause

    // Notes on IsPaused: the flag is the byte at +0xC of the flag object at +0xD8, as the
    // engine's own getter (0x00838210, `mov al, [ecx+0xC]`) reads it.

    // Returns whether the game is paused.
    bool IsPaused() const
    {
        return Read<std::uint8_t>(kPausedFlag + kFlagValue) != 0;
    }

    // Returns the total time spent paused this session, in milliseconds. A pause still in
    // progress is not counted until it ends.
    std::uint32_t PausedMilliseconds() const
    {
        return Read<std::uint32_t>(kPausedTotal);
    }

    // Pauses or resumes the game by calling the engine's own SetPaused.
    // @param paused true to pause, false to resume
    // @param second the engine's second argument. The pause key passes true; what it means is
    //  not yet known
    void SetPaused(bool paused, bool second = true)
    {
        using Fn = void(__fastcall *)(CClientExoApp *, int, bool, bool);
        reinterpret_cast<Fn>(Module::Address(addr::kCClientExoApp_SetPaused))(this, 0, paused, second);
    }

    // -- game mode

    // Returns the game mode controller (dao/GameModes.h).
    // @return the controller, or null before the application has finished starting up
    GameModeController *GameModes() const
    {
        return Read<GameModeController *>(kModeHolder);
    }
    // Returns the game's current mode.
    // @return the mode, or nullopt before the game mode controller exists
    std::optional<GameMode> Mode() const
    {
        const GameModeController *modes = GameModes();
        return modes != nullptr ? std::optional<GameMode>(modes->Mode()) : std::nullopt;
    }
    // Returns the game's current mode. A static shortcut for `Get()->Mode()`.
    // @return the mode, or nullopt before the application or its mode controller exists
    static std::optional<GameMode> CurrentMode()
    {
        const CClientExoApp *app = Get();
        return app != nullptr ? app->Mode() : std::nullopt;
    }
    // Returns explore mode, which tracks what is under the cursor (dao/GameModes.h).
    // @return explore mode, or null unless the game is in exploration or combat
    static GameModeExplore *CurrentExplore()
    {
        const CClientExoApp *app = Get();
        const GameModeController *modes = app != nullptr ? app->GameModes() : nullptr;
        return modes != nullptr ? modes->Explore() : nullptr;
    }

    // Returns the script commands object (dao/Script.h). The application creates it while
    // starting up and hands it to the script VM.
    // @return the object, or null before it exists
    void *Commands() const
    {
        return Read<void *>(kCommands);
    }

    static constexpr std::size_t kPausedFlag = 0xD8;  // offset of the pause's flag object
    static constexpr std::size_t kSecondFlag = 0xE8;  // flag object SetPaused sets from `second`
    static constexpr std::size_t kFlagValue = 0xC;    // offset of the value byte in a flag object
    static constexpr std::size_t kPausedSince = 0xF8; // GetTickCount() when the pause began
    static constexpr std::size_t kPausedTotal = 0xFC; // total milliseconds paused so far
    // Offset of the GameModeController pointer (dao/GameModes.h).
    static constexpr std::size_t kModeHolder = 0x114;

    // Notes on kCommands: the application's initialisation does
    // `app->[+0x1E4] = new CVirtualMachineCommands` (0x005DBE22), then hands the object to the VM.

    // Offset of the script commands object pointer.
    static constexpr std::size_t kCommands = 0x1E4;
    static constexpr std::size_t kSize = 0x2F0; // the object's size, as passed to operator new

  private:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }

    template <class T> T *Lookup(AddressId function, ObjectId id)
    {
        using Fn = T *(__fastcall *)(CClientExoApp *, int, ObjectId);
        return reinterpret_cast<Fn>(Module::Address(function))(this, 0, id);
    }
};

} // namespace dao
