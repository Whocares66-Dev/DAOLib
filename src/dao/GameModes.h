// The game's modes (exploration, combat, dialogue, menus, loading and the rest) and the
// controller that switches between them. Use it to tell what the game is doing and to switch
// modes. Explore mode also tells you what is under the cursor, and lets you aim abilities and
// give move orders the way the player does.
#pragma once

#include "dao/Addresses.h"
#include "dao/Camera.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/Structures.h"
#include "dao/World.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace dao
{

// Notes on GameMode: the numbers are the scripts' GM_* constants (script.ldf, GetGameMode's
// answers), and the class at each number of the controller's array, as the probe listed them in
// play 2026-09-30; the two agree on all 13. The main menu at start-up runs in Pregame (8): the
// Eluvian MCP server saw none, pregame, loading, explore from launch to a loaded save
// (2026-09-30). The probe had seen CharGen behind the main menu earlier that day, so the menu
// reached some other way (from a game, or with the character creator up) may run in CharGen;
// which way is not yet pinned.

// The game's modes, with the same numbers as the scripts' GM_* constants. The comment on each
// names the engine class that runs it.
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
    Pregame = 8,      // GameModePregame: the main menu when the game starts
    Loading = 9,      // GameModeLoading
    Movie = 10,       // GameModeMovie
    CharGen = 11,     // GameModeCharGen: character creation, and sometimes the main menu
    PartyPicker = 12, // GameModePartyPicker
};

// Explore mode, which combat mode is built on. It knows what is under the mouse cursor, and it
// handles the player's targeting and move orders. Get it with GameModeController::Explore or
// CClientExoApp::CurrentExplore, and use it on the game thread, for example from a frame callback.
//
// Fifteen times a second the mode works out what is under the cursor; this is the "pick". The
// pick is empty while the mouse is over any Flash movie, including a plugin's, because a movie
// under the cursor takes the click. A visual effect with a shape counts as an object, and is
// picked in place of the ground behind it (see SetPickable in dao/Vfx.h).
class GameModeExplore
{
  public:
    GameModeExplore() = delete;
    GameModeExplore(const GameModeExplore &) = delete;
    GameModeExplore &operator=(const GameModeExplore &) = delete;
    ~GameModeExplore() = delete;

    // Returns the object a click would act on.
    // @return the object's id, or nullopt if the cursor is not over an object
    std::optional<ObjectId> CursorObject() const
    {
        const auto id = Read<ObjectId>(kCursorObject);
        return id == kInvalidObject ? std::nullopt : std::optional<ObjectId>(id);
    }

    // Returns the point on the ground under the cursor.
    // @return the point, or nullopt if the cursor is over an object, a movie or nothing
    std::optional<Vector> CursorGround() const
    {
        const auto *hit = Read<const world::Hit *>(kCursorGround);
        return hit != nullptr ? std::optional<Vector>(hit->position) : std::nullopt;
    }

    // Notes on CursorGroundWalkable: the verdict is the engine's test (0x0070C840) that the point
    // is on the area's walkmesh for the party's controlled creature (the area's check,
    // 0x0073DE70).

    // Returns whether the controlled creature could walk to the ground point under the cursor.
    // A move click is refused on a point that is not walkable. Returns false when there is no
    // ground point.
    bool CursorGroundWalkable() const
    {
        return CursorGround().has_value() && Read<bool>(kCursorWalkable);
    }

    // Returns everything the ray of the latest pick hit, nearest first. The object or ground
    // point under the cursor is chosen from these. The span is empty when the cursor is outside
    // the view.
    std::span<const world::Hit> CursorHits() const
    {
        const auto count = Read<std::int32_t>(kHitCount);
        const auto *hits = reinterpret_cast<const world::Hit *>(reinterpret_cast<const std::byte *>(this) + kHits);
        return {hits, count > 0 && count <= kMaxHits ? static_cast<std::size_t>(count) : 0};
    }

    // Notes on RequestTarget: it is what the quickbar's handler does when it finds no target
    // (dao/Messages.h, PerformAbilityMessage), and the click uses the ability through
    // PerformAbilityOnTargetUnderCursorMessage. BeginTargeting (dao/Addresses.h) shows "Target
    // Required" instead when its input check fails. Seen in play 2026-09-30, by the turn-based
    // mod: Shield Pummel asked for its target, and the click used it.

    // Asks the player to pick a target for an ability. It shows the targeting cursor, and uses
    // the ability on whatever the player clicks. Call it on the game thread.
    // @param ability the ability's id
    // @param caster the creature that will use it
    void RequestTarget(std::int32_t ability, ObjectId caster)
    {
        using Fn = void(__fastcall *)(GameModeExplore *, int, std::int32_t, ObjectId, ObjectId);
        reinterpret_cast<Fn>(Module::Address(addr::kGameModeExplore_BeginTargeting))(this, 0, ability, caster, 0);
    }

    // Notes on Targeting: read 2026-10-02 from BeginTargeting and explore mode's aiming
    // (dao/Addresses.h, kAbilityTargeting), which test the same two fields. Not yet seen in play.

    // An ability the player is aiming, and the creature that will use it.
    struct Aim
    {
        std::int32_t ability = 0;         // the ability's id
        ObjectId caster = kInvalidObject; // the creature that will use it
    };
    // Returns the ability the player is aiming while the targeting cursor is up. That happens
    // after a quickbar ability is pressed with no valid target, or after RequestTarget. Not yet
    // tested in the game.
    // @return the ability and its caster, or nullopt if no ability is being aimed
    static std::optional<Aim> Targeting()
    {
        constexpr std::size_t kActive = 0x14;
        constexpr std::size_t kAbility = 0x40;
        constexpr std::size_t kCaster = 0x50;
        const auto *state = reinterpret_cast<const std::byte *>(Module::Address(addr::kAbilityTargeting));
        if (state == nullptr)
        {
            return std::nullopt;
        }
        const auto active = *reinterpret_cast<const std::int32_t *>(state + kActive);
        const auto ability = *reinterpret_cast<const std::int32_t *>(state + kAbility);
        if (active != 1 || ability == 0)
        {
            return std::nullopt;
        }
        return Aim{ability, *reinterpret_cast<const ObjectId *>(state + kCaster)};
    }

    // Returns the selected target. The cycle-target keys choose it, and a quickbar ability is
    // used on it if the ability can target it (see PerformAbilityMessage in dao/Messages.h).
    // @return the target's id, or nullopt if no target is selected
    static std::optional<ObjectId> SelectedTarget()
    {
        const auto id = *reinterpret_cast<const ObjectId *>(Module::Address(addr::kSelectedTarget));
        return id == kInvalidObject ? std::nullopt : std::optional<ObjectId>(id);
    }

    // Notes on SelectTarget: it calls what the cycle-target keys call (0x0070D030), which keeps
    // the id, tells the target's GUI and moves the highlight. Not yet seen in play.

    // Selects a target, the same as the cycle-target keys. A PerformAbilityMessage sent next,
    // for an ability that can target it, then uses the ability on it, as a quickbar click would.
    // Not yet tested in the game. Call it on the game thread.
    void SelectTarget(ObjectId target)
    {
        using Fn = void(__fastcall *)(GameModeExplore *, int, ObjectId);
        reinterpret_cast<Fn>(Module::Address(addr::kGameModeExplore_SelectTarget))(this, 0, target);
    }

    // Notes on SetCursorObject: PerformPrimaryActionOnTargetUnderCursorMessage's handler
    // (0x00712830) reads +0xCC, and acts for every party member selected in the party bar, not the
    // controlled creature alone: an attack on a hostile creature is each selected member's
    // (dao::orders::Attack is one creature's), and with none selected nothing is ordered. Its
    // answer is a void call's leftover and says nothing. In play, 2026-09-30, in a turn-based
    // fight, the message reached the handler and no command was queued.

    // Sets the object a click acts on, as if the cursor were over it, ready for a
    // PerformPrimaryActionOnTargetUnderCursorMessage. The mode re-picks from the cursor 15 times
    // a second, so set the object and send the message in the same frame callback. Every party
    // member selected in the party bar carries out the action; if none is selected, nobody does.
    void SetCursorObject(ObjectId object)
    {
        *reinterpret_cast<ObjectId *>(reinterpret_cast<std::byte *>(this) + kCursorObject) = object;
    }

    // Returns where the latest move click sent the controlled creature.
    Vector LastMoveDestination() const
    {
        return Read<Vector>(kLastMoveDestination);
    }

    // Notes on MoveTo: it calls the move click's own function (0x0070BE80).

    // Sends a creature to a point, the same as a move click. It records the point as the latest
    // move's destination and hands the creature the order. For the player's other orders, see
    // dao/Orders.h.
    void MoveTo(ObjectId creature, const Vector &point)
    {
        alignas(16) const float at[4] = {point.x, point.y, point.z, 0.0F};
        using Fn = void(__fastcall *)(GameModeExplore *, int, ObjectId, const float *, std::int32_t);
        reinterpret_cast<Fn>(Module::Address(addr::kGameModeExplore_MoveTo))(this, 0, creature, at, -1);
    }

    static constexpr std::size_t kLastMoveDestination = 0xA0; // the latest move's destination
    static constexpr std::size_t kCursorObject = 0xCC;        // offset of the picked object's id
    static constexpr std::size_t kCursorGround = 0xD0;        // offset of the picked ground hit
    static constexpr std::size_t kCursorWalkable = 0xD4;      // whether the ground hit is walkable
    static constexpr std::size_t kHits = 0xE0;                // offset of the ray's hits
    static constexpr std::size_t kHitCount = 0x6E0;           // offset of the number of hits
    static constexpr std::int32_t kMaxHits = 32;              // the most hits a pick keeps

  private:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
};

// The controller that holds the game's modes and switches between them. Get it with
// CClientExoApp::GameModes.
class GameModeController
{
  public:
    GameModeController() = delete;
    GameModeController(const GameModeController &) = delete;
    GameModeController &operator=(const GameModeController &) = delete;
    ~GameModeController() = delete;

    // Returns the current mode.
    GameMode Mode() const
    {
        return static_cast<GameMode>(Read<std::int32_t>(kMode));
    }

    // Switches to a mode, the same as a SetGameModeMessage. The new mode can refuse.
    void SetMode(GameMode mode)
    {
        using Fn = void(__fastcall *)(GameModeController *, int, std::int32_t, std::int32_t, std::int32_t);
        reinterpret_cast<Fn>(Module::Address(addr::kGameModeController_SetGameMode))(
            this, 0, static_cast<std::int32_t>(mode), 0, 0);
    }

    // Returns the engine's object for a mode. Send that mode's messages to it (dao/Messages.h).
    // @return the object, or null if there is none or it is not a GameMode class
    void *ModeObject(GameMode mode) const;
    // Returns the engine's object for the current mode. Returns null in the same cases as
    // ModeObject.
    void *CurrentModeObject() const
    {
        return ModeObject(Mode());
    }

    // Returns the current mode as explore mode.
    // @return explore mode, or null unless the current mode is explore or combat
    GameModeExplore *Explore() const
    {
        const GameMode mode = Mode();
        return mode == GameMode::Explore || mode == GameMode::Combat ? static_cast<GameModeExplore *>(ModeObject(mode))
                                                                     : nullptr;
    }

    // Returns the camera controller used in explore and combat mode (dao/Camera.h).
    // @return the controller, or null if it is not a CameraControllerExplore
    CameraController *Camera() const;

    // Returns the mode before the current one. This meaning is inferred from the value after
    // loading a save.
    GameMode PreviousMode() const
    {
        return static_cast<GameMode>(Read<std::int32_t>(kPreviousMode));
    }

    static constexpr std::size_t kModes = 0x58;        // offset of the pointer to the modes' array
    static constexpr std::size_t kCamera = 0x60;       // offset of the camera controller
    static constexpr std::size_t kMode = 0x78;         // offset of the current mode
    static constexpr std::size_t kPreviousMode = 0x7C; // offset of the previous mode
    // The number of entries in the array of modes, one per GameMode.
    static constexpr std::size_t kMaxModes = 13;

  private:
    template <class T> const T &Read(std::size_t offset) const
    {
        return *reinterpret_cast<const T *>(reinterpret_cast<const std::byte *>(this) + offset);
    }
};

} // namespace dao

namespace dao::modes
{

// Calls `observer` each time the game's mode changes, once the new mode has entered.
// @param observer receives the mode left and the mode entered
//
// Callbacks run on the game thread, from the controller's update, after the module has had its
// EVENT_TYPE_GAMEMODE_CHANGE.
void OnModeChanged(std::function<void(GameMode before, GameMode now)> observer);

// Calls `observer` once per load, as the loading screen goes: a save loaded, an area
// transition, a new game. The area is loaded, the party stands in it where the engine placed it,
// and the area's and module's load scripts have run. The loading screen shown while the game is
// saved is not a load.
// If a load script starts a cutscene or a conversation, it comes before that plays.
void OnAreaLoaded(std::function<void()> observer);

// Calls `observer` when the game takes the player's input again: explore or combat mode entered
// from any other, as a load, a cutscene, a conversation or a full-screen screen ends.
void OnInputBack(std::function<void()> observer);

} // namespace dao::modes
