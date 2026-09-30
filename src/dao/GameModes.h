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

// The game's modes by number: the scripts' GM_* constants (script.ldf,
// GetGameMode's answers), and the class at each number of the controller's
// array, as the probe listed them in play 2026-09-30 -- the two agree on all
// 13. The main menu runs in CharGen, the character creator's mode.
enum class GameMode : std::int32_t
{
    Explore = 0,      // GameModeExplore
    Combat = 1,       // GameModeCombat
    Dead = 2,         // GameModeDead
    Gui = 3,          // GameModeGUI: a menu screen over the game
    Dialog = 4,       // GameModeDialog
    Cutscene = 5,     // GameModeCutscene
    FlyCam = 6,       // GameModeFlyCam
    Fixed = 7,        // GameModeFixed
    Pregame = 8,      // GameModePregame
    Loading = 9,      // GameModeLoading
    Movie = 10,       // GameModeMovie
    CharGen = 11,     // GameModeCharGen: character creation, and the main menu
    PartyPicker = 12, // GameModePartyPicker
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

    // The mode object for a number, as SetGameMode finds it: +0x58 points to
    // an array of them indexed by the mode's number, 13 entries (seen in
    // play 2026-09-30, one GameMode class per number: dao::GameMode). Not a
    // std::vector: +0x5C and +0x60 are other pointers. The current mode is
    // at +0x78 and, by its value after a load (9, loading), the previous one
    // at +0x7C. An entry is returned only if its class is a GameMode
    // (dao::TypeName), else null.
    const void *ModeObject(GameMode mode) const;

    // The mode before the current one, by the value seen after a load.
    GameMode PreviousMode() const
    {
        return static_cast<GameMode>(Read<std::int32_t>(kPreviousMode));
    }

    static constexpr std::size_t kModes = 0x58;
    static constexpr std::size_t kMode = 0x78;
    static constexpr std::size_t kPreviousMode = 0x7C;
    // The array's entries, one per GameMode.
    static constexpr std::size_t kMaxModes = 13;

  private:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
};

} // namespace dao
