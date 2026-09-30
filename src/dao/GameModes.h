// The game's modes -- exploring, combat, dialogue, a menu, loading -- and
// the controller that switches between them, CClientExoApp+0x114.
//
// Read 2026-09-30. The application builds the controller at 0x005DC064
// (its constructor, 0x00641370, stores GameModeController's four vtables)
// and keeps it at +0x114. Its current mode is the int at +0x78, which the
// probe has read in play since 2026-09-29 (dev/TURN_BASED.md); the modes
// themselves are objects in a table at +0x58, one per number, each a
// GameMode class (GameModeExplore, GameModeDialog, ...). SetGameMode
// (0x006414A0) switches: it asks the new mode's object (its vtable slot
// 10) whether to refuse, special-cases loading (9), and goes on; the
// controller's handler for SetGameModeMessage calls it with the message's
// mode and two zeros, and so does SetMode here.
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"

#include <cstddef>
#include <cstdint>

namespace dao
{

// The modes seen in play, 2026-09-29/30. Others exist; the probe lists the
// table's classes by number (dev/PROBE.md).
enum class GameMode : std::int32_t
{
    Explore = 0,
    Combat = 1,
    MenuScreen = 3,
    Dialogue = 4,
    Loading = 9,
    MainMenu = 11,
};

class GameModeController
{
  public:
    GameModeController() = delete;
    GameModeController(const GameModeController &) = delete;
    GameModeController &operator=(const GameModeController &) = delete;
    ~GameModeController() = delete;

    GameMode Mode() const
    {
        return static_cast<GameMode>(Read<std::int32_t>(kMode));
    }

    // Switches to a mode, as a SetGameModeMessage does.
    void SetMode(GameMode mode)
    {
        using Fn = void(__fastcall *)(GameModeController *, int, std::int32_t, std::int32_t, std::int32_t);
        reinterpret_cast<Fn>(Module::Address(addr::kGameModeController_SetGameMode))(
            this, 0, static_cast<std::int32_t>(mode), 0, 0);
    }

    // The mode object for a number (its class says which mode it is:
    // dao::TypeName), or null past the table's end. Read as a std::vector
    // whose first and last pointers are at +0x58 and +0x5C (the constructor
    // zeroes +0x58 to +0x60 together; SetGameMode indexes +0x58 by mode).
    // NOT CONFIRMED: in the session of 2026-09-30 the probe never saw a
    // non-empty table there; do not rely on these two yet.
    const void *ModeObject(GameMode mode) const
    {
        const auto *first = Read<const void *const *>(kModesFirst);
        const auto *last = Read<const void *const *>(kModesLast);
        const auto i = static_cast<std::ptrdiff_t>(mode);
        return first != nullptr && i >= 0 && i < last - first ? first[i] : nullptr;
    }
    std::size_t ModeCount() const
    {
        const auto *first = Read<const void *const *>(kModesFirst);
        const auto *last = Read<const void *const *>(kModesLast);
        return first != nullptr && last >= first ? static_cast<std::size_t>(last - first) : 0;
    }

    static constexpr std::size_t kModesFirst = 0x58;
    static constexpr std::size_t kModesLast = 0x5C;
    static constexpr std::size_t kMode = 0x78;

  private:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
};

} // namespace dao
