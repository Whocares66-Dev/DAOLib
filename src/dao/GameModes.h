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
#include "dao/Camera.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/Structures.h"
#include "dao/World.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

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

// Explore mode, and combat mode, which derives from it (dao::TypeName and
// tools\daoexe.py --bases GameModeCombat): what is under the mouse cursor,
// as a click would act on it. Read 2026-09-30.
//
// The mode's update (vtable slot 8, 0x00712F90) re-picks 15 times a second
// (a timer at +0x6E4 reset to 1/15 s), or at once when the engine asks: it
// casts a ray from the camera through the cursor (0x0070B590, up to 32
// hits at +0xE0, their count at +0x6E0, sorted nearest first), then chooses
// (0x0070F270): the object at +0xCC, or the ground hit at +0xD0 with its
// verdict at +0xD4. An object the GUI names takes precedence (a global at
// 0x00C35FF0, +0xC), and an object hit wins over the ground. What the click
// handlers read: the primary action's (0x007132B0) target is +0xCC, and a
// move (0x0070C9A0) goes to +0xD0's point if +0xD4 allows, recording it at
// +0xA0 (0x0070BE80) before the creature walks. Each mode keeps its own;
// the one current is the one updated. On the game's thread, as a frame
// callback runs. Seen in play 2026-09-30 (dev/LIBRARY.md). The hits are
// dao::world::Hit, which dao/World.h's Raycast returns too.
class GameModeExplore
{
  public:
    GameModeExplore() = delete;
    GameModeExplore(const GameModeExplore &) = delete;
    GameModeExplore &operator=(const GameModeExplore &) = delete;
    ~GameModeExplore() = delete;

    // The object a click would act on, if the cursor is over one.
    std::optional<ObjectId> CursorObject() const
    {
        const auto id = Read<ObjectId>(kCursorObject);
        return id == kInvalidObject ? std::nullopt : std::optional<ObjectId>(id);
    }

    // The point on the ground under the cursor, when the cursor is on the
    // ground rather than an object.
    std::optional<Vector> CursorGround() const
    {
        const auto *hit = Read<const world::Hit *>(kCursorGround);
        return hit != nullptr ? std::optional<Vector>(hit->position) : std::nullopt;
    }

    // Whether a move there would go: the engine's test (0x0070C840) that
    // the point is on the area's walkmesh for the party's controlled
    // creature (the area's check, 0x0073DE70). A move click refuses an
    // unwalkable point.
    bool CursorGroundWalkable() const
    {
        return CursorGround().has_value() && Read<bool>(kCursorWalkable);
    }

    // Everything along the ray, nearest first, from the latest pick; the
    // cursor's choice is made among these.
    std::span<const world::Hit> CursorHits() const
    {
        const auto count = Read<std::int32_t>(kHitCount);
        const auto *hits = reinterpret_cast<const world::Hit *>(reinterpret_cast<const std::byte *>(this) + kHits);
        return {hits, count > 0 && count <= kMaxHits ? static_cast<std::size_t>(count) : 0};
    }

    // Ask the player for an ability's target, as the quickbar's handler does
    // when it finds none (dao/Messages.h, PerformAbilityMessage): the
    // targeting cursor, and the ability used on what the player clicks,
    // through PerformAbilityOnTargetUnderCursorMessage. BeginTargeting
    // (dao/Addresses.h) shows "Target Required" instead when its input
    // check fails. On the game's thread. Seen in play 2026-09-30, by the
    // turn-based mod: Shield Pummel asked for its target, and the click used
    // it.
    void RequestTarget(std::int32_t ability, ObjectId caster)
    {
        using Fn = void(__fastcall *)(GameModeExplore *, int, std::int32_t, ObjectId, ObjectId);
        reinterpret_cast<Fn>(Module::Address(addr::kGameModeExplore_BeginTargeting))(this, 0, ability, caster, 0);
    }

    // Where the latest move click sent the controlled creature.
    Vector LastMoveDestination() const
    {
        return Read<Vector>(kLastMoveDestination);
    }

    // Send a creature to a point, as a move click does (0x0070BE80): the
    // point recorded as the latest move's destination, the creature told
    // its order arrived (dao/Orders.h has the rest of the player's orders).
    void MoveTo(ObjectId creature, const Vector &point)
    {
        alignas(16) const float at[4] = {point.x, point.y, point.z, 0.0F};
        using Fn = void(__fastcall *)(GameModeExplore *, int, ObjectId, const float *, std::int32_t);
        reinterpret_cast<Fn>(Module::Address(addr::kGameModeExplore_MoveTo))(this, 0, creature, at, -1);
    }

    static constexpr std::size_t kLastMoveDestination = 0xA0;
    static constexpr std::size_t kCursorObject = 0xCC;
    static constexpr std::size_t kCursorGround = 0xD0;
    static constexpr std::size_t kCursorWalkable = 0xD4;
    static constexpr std::size_t kHits = 0xE0;
    static constexpr std::size_t kHitCount = 0x6E0;
    static constexpr std::int32_t kMaxHits = 32;

  private:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
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
    // (dao::TypeName), else null. What a plugin sends a mode's messages to
    // (dao/Messages.h).
    void *ModeObject(GameMode mode) const;
    void *CurrentModeObject() const
    {
        return ModeObject(Mode());
    }

    // The current mode as explore mode, if it is explore or combat (whose
    // class derives from explore's); null in any other.
    GameModeExplore *Explore() const
    {
        const GameMode mode = Mode();
        return mode == GameMode::Explore || mode == GameMode::Combat ? static_cast<GameModeExplore *>(ModeObject(mode))
                                                                     : nullptr;
    }

    // The camera's controller in explore and combat mode (dao/Camera.h):
    // the controller's +0x60, if its class is CameraControllerExplore
    // (dao::TypeName), else null.
    CameraController *Camera() const;

    // The mode before the current one, by the value seen after a load.
    GameMode PreviousMode() const
    {
        return static_cast<GameMode>(Read<std::int32_t>(kPreviousMode));
    }

    static constexpr std::size_t kModes = 0x58;
    static constexpr std::size_t kCamera = 0x60;
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
