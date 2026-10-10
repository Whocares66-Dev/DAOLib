// An attack's reach and approach (dao/Attack.h), each answered by calling the attack command's
// own code, so a plugin keeps no copy of the rule (DAOLib issue #16; the turn-based mod's copy
// drifted, and stopped 67 archers' shots in one ambush).
//
// Read 2026-10-04 (dao/Addresses.h, "An attack's reach"). The attack
// command's step makes no reach test: it queues an approach, sub-action 15
// for a ranged weapon or 4 for melee, then the attack, 50; the approach,
// on its first run, ends at once when the attacker is in reach, and else
// walks.
// - Melee: the target gives the attacker a place on its melee ring (every
//   creature has one), and the attacker is in reach when the centres are
//   at most both personal spaces and 1.2 m apart and it stands within 1.25
//   m of that place, on the ground (x and y; the centres' distance counts
//   height too). The walk is to the place. There is no facing test
//   here: the attack turns to face its target (sub-action 39, to within
//   1 degree), and the blow (1) waits while the target is more than 60
//   degrees above or below it or out of sight, then sends EVENT_TYPE_
//   COMMAND_PENDING for the scripts to resolve it; a shot turns to face,
//   aims (within 67.5 degrees, in sight) and then shoots (2), which sends
//   it (dao/Conjure.h, RunningSubAction). Corrected 2026-10-04: the 60
//   degrees were first read here as facing; they are elevation
//   (0x004EE5A0).
// - Ranged (a staff, bow or crossbow in the main hand): in range when the
//   distance on the ground (x and y) is at most the weapon's Range
//   (bitm_base, capped at 60 m) and at least both personal spaces, and the
//   attacker has line of sight. The walk is toward the target until within
//   a stop distance of it, on the ground, with a line of sight from there.
//   The stop distance is Range less both spaces at first, and is cut by
//   1 m on every update that finds the walker within it without a line of
//   sight, until a cut would bring it to both spaces: there the move
//   update ends the walk as though it had arrived, and no shot follows.
//   Corrected 2026-10-10 (DAOLib #50): the cut was first read as one per
//   metre walked. It is one per update, so for a shooter that starts in
//   range without sight the stop distance falls to its own distance
//   within a second, and is then tested only each metre nearer. Seen in
//   play 2026-10-10 (examples/attack, the turn-based mod's session,
//   Awakening's Wending Wood in real time, the targets walking): of 30
//   ranged approaches none that ended in a shot had Walk answer nothing.
//   An archer ordered from 25 m, in range and out of sight, walked 20.1 m
//   where Walk had said 21.9 m, ending 1.2 m from Walk's place (the first
//   reading gives that walk up after 10.75 m); from out of range, 125.9 m
//   for 129.2 m, 1.9 m apart; his eleven shorter walks ended 0.2 to 2.3 m
//   from it. Enemy archers at party members on the move: 1.4 to 4.1 m, the
//   engine stopping the sooner each time. The approaches that ended far
//   from Walk's place were the AI's own timeouts (COMMAND_FAILED_TIMEOUT
//   after 10 s, and one after 1 s) and a command a conversation cleared. A
//   target standing still has not been tried.
// The ring place is asked for without claiming it: the engine's own
// approach claims it (and lets go of the attacker's last), which a
// question here must not. And as an active ring holds it: a ring nobody
// has claimed is emptied by its creature's update every frame
// (RefreshMeleeRing's early exit, MeleeRing::Clear), and filled with
// whoever stands round it only once an attacker has claimed on it, after
// which the places they stand on are no longer free and the claim on them
// is gone; so in a crowd the engine's first claim finds room and the blow
// then finds none (DAOLib #19). Here an unclaimed ring is filled as the
// engine fills an active one (its keep byte, +0x28, set for the engine's
// own refresh), asked, and emptied again as the engine would have. Both
// rings are asked: the first claim meets the one standing now, and an
// attacker that strikes in its first update strikes on that claim; one
// that must turn or walk meets the filled one. Seen in play 2026-10-04 (the
// turn-based mod's staged crowd): answering by the filled ring alone, no
// attack it gave an approach failed for room (0 of 106), but three it gave
// none landed, each striking in its first update. The search is the same
// either way, so the answer is the approach's first run's at the moment it
// is asked: nothing exactly where that run fails with
// COMMAND_FAILED_NO_SPACE_IN_MELEE_RING (-5). A place found can be gone
// later, the ring filled by others: the approach asks it again, claiming,
// whenever the target moves or each 1.5 s, and the blow asks again if the
// target does not hold the attacker as its own (+0xC60); each fails the
// command with -5 on a full ring. Seen in play 2026-10-04 (the turn-based
// mod, dragonlings swarming one creature above Haven, 18 failures): 6 at
// the first run, each where ApproachOf had answered nothing; 8 during the
// approach (sub-action 4); 4 at the turn to face (39), the blow not yet
// begun.
#include "dao/Attack.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Gda.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/World.h"
#include "dao/core/ObjectType.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace dao::attack
{

namespace
{

// The engine's points, four floats aligned for SSE.
struct alignas(16) Vector4
{
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 0;
};

Vector4 ToVector4(const Vector &v)
{
    return {v.x, v.y, v.z, 0};
}

// The step's Range before an item is read (0x00B07ECC) and its cap
// (0x0069F330); bitm_base's table and its Range column's hash, as the
// step asks GdaTables for it.
constexpr float kNoRange = 65505.0F;
constexpr float kMostRange = 60.0F;
constexpr std::int32_t kBaseItems = 6;
constexpr std::uint32_t kRangeColumn = 0x81F2A384;
// How far a ranged walker goes between two of the move update's tests,
// each of which may cut the stop distance: one update's run at the usual
// combat speed, 4.66 m/s (apr_base's Game_CbtSpeed, 55 of its 90 rows), at
// sixty updates a second. The engine's own figure is the walker's speed
// and the frame's time, so a walk that begins in range without sight can
// end a few metres from where this puts it (DAOLib #50; it was 0.25 m,
// four cuts to the metre, which gave up walks the engine makes).
constexpr float kWalkStep = 4.66F / 60.0F;
// A creature's equipped items: its +0xBE0 a SmartData::Attribute whose
// value is its Equipment, the Equipment's +0x24 a SlotItemLocation; the
// main hand slot 0; an item's base row the word at +0x464.
constexpr std::uintptr_t kEquipped = 0xBE0;
constexpr std::uintptr_t kSlots = 0x24;
constexpr std::int32_t kMainHand = 0;
constexpr std::uintptr_t kBaseRow = 0x464;

template <class Fn> Fn At(AddressId id)
{
    return reinterpret_cast<Fn>(Module::Address(id));
}

CCObject *Creature(ObjectId id)
{
    CClientExoApp *app = CClientExoApp::Get();
    CCObject *object = app != nullptr ? app->GetObjectById(id) : nullptr;
    return object != nullptr && object->Type() == core::ObjectType::Creature ? object : nullptr;
}

bool HasRangedWeapon(CCObject *creature)
{
    // CCCreature::HasRangedWeapon: __thiscall, no arguments.
    using Fn = std::int32_t(__fastcall *)(CCObject *, int);
    return At<Fn>(addr::kCCCreature_HasRangedWeapon)(creature, 0) == 1;
}

// SubActions::WithinBand: __cdecl (a, b, max, min), on the ground (x, y);
// Combat::LineOfSight: __cdecl (from, to, to's id, from point or null: the
// ray from that point, or from the first object's place a little to its
// side).
using BandFn = bool(__cdecl *)(const Vector4 *, const Vector4 *, float, float);
using SightFn = bool(__cdecl *)(CCObject *, CCObject *, ObjectId, const Vector4 *);

float PersonalSpaces(CCObject *a, CCObject *b)
{
    // CCObject::PersonalSpaces: __cdecl (a, b), a float.
    using Fn = float(__cdecl *)(CCObject *, CCObject *);
    return At<Fn>(addr::kCCObject_PersonalSpaces)(a, b);
}

// The weapon's Range as the attack command's step reads it: the main hand's
// base item's, through the engine's own lookups, at most 60 m.
float WeaponRange(CCObject *creature)
{
    float range = kNoRange;
    // SmartData::Attribute::Get: __thiscall, no arguments, the value;
    // SlotItemLocation::ItemIn: __thiscall (slot), `ret 4`.
    using GetFn = std::uintptr_t(__fastcall *)(std::uintptr_t, int);
    using ItemInFn = std::uintptr_t(__fastcall *)(std::uintptr_t, int, std::int32_t);
    const std::uintptr_t holder =
        At<GetFn>(addr::kSmartDataAttribute_Get)(reinterpret_cast<std::uintptr_t>(creature) + kEquipped, 0);
    const std::uintptr_t item =
        holder != 0 ? At<ItemInFn>(addr::kSlotItemLocation_ItemIn)(holder + kSlots, 0, kMainHand) : 0;
    if (const auto row = item != 0 ? TryRead<std::uint16_t>(item + kBaseRow) : std::nullopt)
    {
        range = gda::GetFloat(kBaseItems, *row, kRangeColumn).value_or(range);
    }
    return std::min(range, kMostRange);
}

// Where a ranged walk along the engine's path ends, as the move update
// (0x007A9130) ends it each frame where the walker stands: within the stop
// distance of the target (WithinBand, on the ground) and with a line of
// sight from there (LineOfSight from that point); in range without one,
// the stop distance is cut by 1 m and the walk goes on, and it is given up
// once that cut would bring it to both spaces. The engine's tests are
// called once where the walker stands, as its first update is before any
// step, and then at each point a kWalkStep apart, an update's run; the 1 m
// and the giving up are the move update's own, copied. No walk where the
// first update ends it; nothing if it is given up or the path ends first.
std::optional<Walked> RangedWalk(CCObject *from, CCObject *to, ObjectId target, const std::vector<Vector> &path,
                                 float stop, float spaces)
{
    const Vector4 aim = ToVector4(to->Position());
    // True where the walk ends at `point`, false where it goes on; nothing
    // where it is given up.
    const auto endsAt = [&](const Vector &point) -> std::optional<bool> {
        const Vector4 at = ToVector4(point);
        if (!At<BandFn>(addr::kSubActions_WithinBand)(&at, &aim, stop, 0))
        {
            return false;
        }
        if (At<SightFn>(addr::kCombat_LineOfSight)(from, to, target, &at))
        {
            return true;
        }
        if (stop - 1 <= spaces)
        {
            return std::nullopt;
        }
        stop -= 1;
        return false;
    };
    if (path.empty())
    {
        return std::nullopt;
    }
    const auto atOnce = endsAt(path.front());
    if (!atOnce)
    {
        return std::nullopt;
    }
    if (*atOnce)
    {
        return Walked{};
    }
    Walked walked;
    walked.path.push_back(path.front());
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        const Vector &a = path[i - 1];
        const Vector &b = path[i];
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float dz = b.z - a.z;
        const float length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        const int steps = std::max(1, static_cast<int>(std::ceil(length / kWalkStep)));
        for (int step = 1; step <= steps; ++step)
        {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            const Vector point{a.x + (t * dx), a.y + (t * dy), a.z + (t * dz)};
            const auto ends = endsAt(point);
            if (!ends)
            {
                return std::nullopt;
            }
            if (*ends)
            {
                walked.metres += t * length;
                walked.path.push_back(point);
                return walked;
            }
        }
        walked.metres += length;
        walked.path.push_back(b);
    }
    return std::nullopt;
}

// A creature's rings (Rings, below): the std::vector of pointers at its
// +0xB44, its first and last at +0xB48 and +0xB4C.
std::vector<std::uintptr_t> RingsOf(const CCObject *creature)
{
    std::vector<std::uintptr_t> rings;
    const auto first = TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(creature) + 0xB48);
    const auto last = TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(creature) + 0xB4C);
    constexpr std::uint32_t kMostRings = 16;
    for (std::uint32_t i = 0;
         first && last && *first != 0 && *last > *first && i < (*last - *first) / 4 && i < kMostRings; ++i)
    {
        rings.push_back(TryRead<std::uint32_t>(*first + (i * 4)).value_or(0));
    }
    return rings;
}

// While it lives, the target's unclaimed rings are filled as the engine
// fills an active one: each ring's keep byte (+0x28) set where it held
// no claimant (the count at +0x38, of its claimants' list at +0x30) and was
// not kept, then the engine's
// own RefreshMeleeRing run for every ring, as CCCreature::Update runs it;
// on going, the keep bytes put back and those rings cleared by the
// engine's own MeleeRing::Clear, as the next update would leave them.
class FilledRings
{
  public:
    explicit FilledRings(CCObject *creature) : creature_(creature)
    {
        const std::vector<std::uintptr_t> rings = RingsOf(creature);
        for (const std::uintptr_t ring : rings)
        {
            const auto claimants = ring != 0 ? TryRead<std::uint32_t>(ring + kClaimants) : std::nullopt;
            const auto kept = ring != 0 ? TryRead<std::uint8_t>(ring + kKeep) : std::nullopt;
            if (claimants && *claimants == 0 && kept && *kept == 0)
            {
                *reinterpret_cast<std::uint8_t *>(ring + kKeep) = 1;
                opened_.push_back(ring);
            }
        }
        if (opened_.empty())
        {
            return;
        }
        // CCCreature::RefreshMeleeRing: __thiscall (ring index), `ret 4`.
        using RefreshFn = void(__fastcall *)(CCObject *, int, std::uint32_t);
        const auto refresh = At<RefreshFn>(addr::kCCCreature_RefreshMeleeRing);
        for (std::uint32_t i = 0; i < rings.size(); ++i)
        {
            refresh(creature_, 0, i);
        }
    }
    FilledRings(const FilledRings &) = delete;
    FilledRings &operator=(const FilledRings &) = delete;
    ~FilledRings()
    {
        // MeleeRing::Clear: __thiscall, no arguments.
        using ClearFn = void(__fastcall *)(std::uintptr_t, int);
        const auto clear = At<ClearFn>(addr::kMeleeRing_Clear);
        for (const std::uintptr_t ring : opened_)
        {
            *reinterpret_cast<std::uint8_t *>(ring + kKeep) = 0;
            clear(ring, 0);
        }
    }

  private:
    static constexpr std::uintptr_t kKeep = 0x28;
    static constexpr std::uintptr_t kClaimants = 0x38; // the claimants' count
    CCObject *creature_;
    std::vector<std::uintptr_t> opened_;
};

// The creature's movement controller and what TurnTo and FaceExactly read
// of it (dao/Addresses.h has the reading, 2026-10-05): its own facing (its
// wanted one, +0x90, SetDesiredFacing's), its state (0 idle, 3 and 4
// turning), its motion record, and the angle beyond
// which SetDesiredFacing starts a turn, in degrees, and the squared length
// below which it takes its own facing for none.
constexpr std::uintptr_t kController = 0xA30;
constexpr std::uintptr_t kControllerFacing = 0xA0;
constexpr std::uintptr_t kControllerState = 0xF0;
constexpr std::uintptr_t kControllerMotion = 0x70;
constexpr std::uintptr_t kControllerHasMotion = 0x74;
constexpr float kTurnStarts = 0.99999899F;
constexpr float kNoLength = 1e-6F;

// The turn of TurnTo (Attack.h): the target's slot 29 for a melee attacker,
// its position for a ranged one; each direction an angle by the engine's
// VectorToAngle and their difference by its Angle::Wrap to -180..180.
std::optional<FacingTurn> TurnFor(CCObject *from, CCObject *to)
{
    constexpr float kTurnDone = 1.0F;
    constexpr std::uintptr_t kFacing = 0x130;
    using PointFn = Vector4 *(__fastcall *)(CCObject *, int, Vector4 *, const Vector4 *);
    using AngleFn = float(__cdecl *)(const Vector4 *);
    using WrapFn = float(__cdecl *)(float, float, float);
    const Vector4 at = ToVector4(from->Position());
    Vector4 point = ToVector4(to->Position());
    if (!HasRangedWeapon(from))
    {
        // The target's slot 29: the point a melee attacker at `at` turns to.
        using Slot = PointFn;
        const Slot *vtable = *reinterpret_cast<const Slot *const *>(to);
        constexpr std::size_t kPointSlot = 29;
        Vector4 out;
        point = *vtable[kPointSlot](to, 0, &out, &at);
    }
    Vector4 toward{point.x - at.x, point.y - at.y, 0, 0};
    const float length = std::sqrt((toward.x * toward.x) + (toward.y * toward.y));
    if (length < 1e-3F)
    {
        return std::nullopt;
    }
    toward.x /= length;
    toward.y /= length;
    const auto facing = TryRead<Vector4>(reinterpret_cast<std::uintptr_t>(from) + kFacing);
    if (!facing)
    {
        return std::nullopt;
    }
    const auto angle = At<AngleFn>(addr::kScriptBackend_ForScript_VectorToAngle);
    FacingTurn turn;
    turn.point = {point.x, point.y, point.z};
    turn.toward = {toward.x, toward.y, 0};
    turn.degrees = At<WrapFn>(addr::kAngle_Wrap)(angle(&toward) - angle(&*facing), -180.0F, 180.0F);
    turn.done = std::fabs(turn.degrees) <= kTurnDone;
    // As SetDesiredFacing judges it: the controller idle, and its own
    // facing within its 1 degree of the aim by the engine's AngleBetween;
    // a controller facing of no length starts a turn, as it does there.
    if (!turn.done)
    {
        using AngleBetweenFn = float(__cdecl *)(const Vector4 *, const Vector4 *);
        const auto controller = reinterpret_cast<std::uintptr_t>(from) + kController;
        const auto state = TryRead<std::int32_t>(controller + kControllerState);
        const auto own = TryRead<Vector4>(controller + kControllerFacing);
        if (state && *state == 0)
        {
            // SetDesiredFacing's two refusals from an idle controller: its own
            // facing already within the angle of the aim (one of no length is
            // never within it), or its motion record busy, the byte at +0x74
            // set and the record's +0x10 holding a nonzero +0xC0 (the
            // address library's session, 2026-10-05).
            const bool within = own && (own->x * own->x) + (own->y * own->y) + (own->z * own->z) > kNoLength &&
                                At<AngleBetweenFn>(addr::kVector_AngleBetween)(&toward, &*own) <= kTurnStarts;
            const auto hasMotion = TryRead<std::uint8_t>(controller + kControllerHasMotion);
            const auto motion = TryRead<std::uintptr_t>(controller + kControllerMotion);
            const auto inner = motion && *motion != 0 ? TryRead<std::uintptr_t>(*motion + 0x10) : std::nullopt;
            const auto busyWord = inner && *inner != 0 ? TryRead<std::uint32_t>(*inner + 0xC0) : std::nullopt;
            const bool busy = hasMotion && *hasMotion != 0 && busyWord && *busyWord != 0;
            turn.stalls = within;
            turn.waits = !within && busy;
        }
    }
    return turn;
}

// Whether a melee attacker in reach strikes in its command's first update,
// as the engine runs it: the turn (sub-action 39, 0x007A69E0, through
// CSSubActionHandler::TurnToward) done at
// once, the facing within 1 degree of the direction to the target's point
// for it (the target's slot 29 given the attacker's place, flattened), each
// an angle by the engine's VectorToAngle and their difference by its
// Angle::Wrap to -180..180; then the blow's own tests (0x007A87C0), as it
// asks them: InStrikingReach, the target within 60 degrees of level
// (Vector::Elevation) and in sight (DAOLib #23).
bool StrikesAtOnce(CCObject *from, CCObject *to, ObjectId target)
{
    // CCCreature::InStrikingReach: __thiscall (target), `ret 4`.
    using StrikingReachFn = bool(__fastcall *)(CCObject *, int, CCObject *);
    if (!At<StrikingReachFn>(addr::kCCCreature_InStrikingReach)(from, 0, to))
    {
        return false;
    }
    constexpr float kMostElevation = 60.0F;
    using ElevationFn = float(__cdecl *)(const Vector4 *);
    const auto turn = TurnFor(from, to);
    if (!turn || !turn->done)
    {
        return false;
    }
    const Vector4 at = ToVector4(from->Position());
    const Vector4 aimAt = ToVector4(to->Position());
    Vector4 rise{aimAt.x - at.x, aimAt.y - at.y, aimAt.z - at.z, 0};
    const float span = std::sqrt((rise.x * rise.x) + (rise.y * rise.y) + (rise.z * rise.z));
    if (span > 1e-3F)
    {
        rise.x /= span;
        rise.y /= span;
        rise.z /= span;
    }
    if (std::fabs(At<ElevationFn>(addr::kVector_Elevation)(&rise)) > kMostElevation)
    {
        return false;
    }
    return At<SightFn>(addr::kCombat_LineOfSight)(from, to, target, nullptr);
}

} // namespace

// A creature whose main-hand item's base type (bitm_base's Type) is 5, a staff, or 6, a bow or
// crossbow; a staff's Range is 45 m, as a bow's.
bool IsRanged(ObjectId attacker)
{
    CCObject *creature = Creature(attacker);
    return creature != nullptr && HasRangedWeapon(creature);
}

// The turn of sub-action 39 (0x007A69E0, through CSSubActionHandler::TurnToward), as the engine
// would compute it now (DAOLib #32). For a melee attacker the point is the target's slot 29 given
// the attacker's place: where it stands for a person, a point of one of its melee rings for a
// creature whose rings give one (0x007CC1A0). For a ranged one it is where the target stands, as
// the turn-based mod's builder read it (2026-10-05). The degrees are the engine's VectorToAngle of
// `toward` less that of the attacker's facing (+0x130), by its Angle::Wrap; `done` is
// TurnToward's own `<=` of 1 degree, the command going past the turn at its first run.
//
// `stalls`: the creature's movement controller is idle (state 0) and its own facing is within the
// 1 degree SetDesiredFacing allows of `toward`, so nothing turns the creature and the command
// waits at sub-action 39 for ever (DAOLib #25). The controller keeps a facing apart from the
// creature's and follows it only at times, so the two drift apart: a snap of the creature's
// alone, or the engine's own. When `done` is false and the turn is small, the scripts'
// SetOrientation of `toward` faces it exactly (their SetFacing commands only ask the controller,
// which turns nothing there).
//
// `waits`: the idle controller's motion record is busy (an animation of the creature's own
// playing out), which SetDesiredFacing also refuses. TurnToward asks again at each update, so the
// turn begins when the record is free: this passes by itself, and is no reason to face the
// creature. Seen in play 2026-10-05 (the turn-based mod): read as `stalls`, it had a creature 159
// degrees off, just out of an ability's animation.
std::optional<FacingTurn> TurnTo(ObjectId attacker, ObjectId target)
{
    CCObject *from = Creature(attacker);
    CCObject *to = Creature(target);
    if (from == nullptr || to == nullptr)
    {
        return std::nullopt;
    }
    return TurnFor(from, to);
}

// Leaves the engine's two facings equal (DAOLib #32): the creature's own by the engine's writer,
// the scripts' SetOrientation's, which tells the model; the movement controller's own facing,
// which no engine function sets but its update copies from the creature's while following it;
// and its wanted facing by the engine's SetDesiredFacing, which then starts no turn. Only on an
// idle controller.
bool FaceExactly(ObjectId creature, Vector toward)
{
    CCObject *object = Creature(creature);
    const float length = std::sqrt((toward.x * toward.x) + (toward.y * toward.y));
    if (object == nullptr || length < 1e-3F)
    {
        return false;
    }
    const auto controller = reinterpret_cast<std::uintptr_t>(object) + kController;
    const auto state = TryRead<std::int32_t>(controller + kControllerState);
    if (!state || *state != 0)
    {
        return false;
    }
    const Vector4 facing{toward.x / length, toward.y / length, 0, 0};
    // The controller's own facing first, so that SetDesiredFacing finds the
    // wanted one on it and starts nothing; then the creature's, by the
    // writer that tells the model. SetDesiredFacing and SetOrientation:
    // __thiscall (const Vector4 *), `ret 4`.
    using SetFn = void(__fastcall *)(void *, int, const Vector4 *);
    *reinterpret_cast<Vector4 *>(controller + kControllerFacing) = facing;
    At<SetFn>(addr::kMovementController_SetDesiredFacing)(reinterpret_cast<void *>(controller), 0, &facing);
    At<SetFn>(addr::kScriptBackend_ForScript_SetOrientation)(object, 0, &facing);
    return true;
}

// Approach's melee fields, as read 2026-10-04:
// - strikesAtOnce: the blow comes in the command's first update, before the ring is filled: in
//   reach, already facing the target within the turn's 1 degree (sub-action 39, 0x007A69E0,
//   through TurnToward), then the blow's own tests (0x007A87C0): its reach (InStrikingReach, the
//   flat distance to the target's point within both spaces and 2.2), the target within 60
//   degrees of level and in sight.
// - roomOnceFilled: the ring as it is a frame after the attacker's claim; the place the approach
//   then walks to, and the place the blow asks for if it comes later.
// - findsRoom: where it is false, the attack fails COMMAND_FAILED_NO_SPACE_IN_MELEE_RING after
//   its first update (DAOLib #19), or at the approach's first run where the ring as it stands now
//   has no place. Seen in play 2026-10-04 (the turn-based mod's staged crowd of dragonlings): it
//   matched the engine for 184 attacks of 184, false for all 24 that failed for want of room,
//   true for the 160 that did not.
// - range: melee is 0, its reach being the engine's own test, InMeleeReach.
std::optional<Approach> ApproachOf(ObjectId attacker, ObjectId target)
{
    CCObject *from = Creature(attacker);
    CCObject *to = Creature(target);
    if (from == nullptr || to == nullptr)
    {
        return std::nullopt;
    }
    Approach approach;
    approach.spaces = PersonalSpaces(from, to);
    approach.ranged = HasRangedWeapon(from);
    if (approach.ranged)
    {
        // As sub-action 15 tests it (SubActions::ApproachRanged).
        approach.range = WeaponRange(from);
        approach.goal = to->Position();
        approach.stopWithin = std::max(0.0F, approach.range - approach.spaces);
        const Vector4 at = ToVector4(from->Position());
        const Vector4 aim = ToVector4(approach.goal);
        const bool inBand = At<BandFn>(addr::kSubActions_WithinBand)(&at, &aim, approach.range, approach.spaces);
        approach.lineOfSight = At<SightFn>(addr::kCombat_LineOfSight)(from, to, target, nullptr);
        approach.inReach = inBand && approach.lineOfSight;
        return approach;
    }
    // As sub-action 4 tests it (SubActions::ApproachMelee): the target's
    // ring place for the attacker, asked without claiming it, then the
    // target's reach test with that place. CCCreature::PersonalSpace:
    // __thiscall, a float; MeleeRingSpot: __thiscall (attacker id, out,
    // unread, claim, unread), `ret 0x14`, passed the space as the
    // engine's own approach passes it; InMeleeReach: __thiscall (attacker,
    // place), `ret 8`.
    using SpaceFn = float(__fastcall *)(CCObject *, int);
    using SpotFn = bool(__fastcall *)(CCObject *, int, ObjectId, Vector4 *, float, std::int32_t, std::int32_t);
    using ReachFn = std::int32_t(__fastcall *)(CCObject *, int, CCObject *, const Vector4 *);
    const float space = At<SpaceFn>(addr::kCCCreature_PersonalSpace)(from, 0);
    const auto spot = At<SpotFn>(addr::kCCCreature_MeleeRingSpot);
    const auto reach = At<ReachFn>(addr::kCCCreature_InMeleeReach);
    // The ring as the first claim meets it, as it stands now: with no
    // place there the engine's approach fails at its first run.
    Vector4 now;
    const bool roomNow = spot(to, 0, attacker, &now, space, 0, 0);
    approach.inReach = roomNow && reach(to, 0, from, &now) != 0;
    approach.strikesAtOnce = approach.inReach && StrikesAtOnce(from, to, target);
    // The ring as it stands once the claim has made it active.
    Vector4 filled;
    {
        const FilledRings fill(to);
        approach.roomOnceFilled = spot(to, 0, attacker, &filled, space, 0, 0);
    }
    approach.findsRoom = roomNow && (approach.strikesAtOnce || approach.roomOnceFilled);
    if (approach.roomOnceFilled)
    {
        approach.goal = Vector{filled.x, filled.y, filled.z};
    }
    else
    {
        approach.goal = roomNow ? Vector{now.x, now.y, now.z} : to->Position();
    }
    return approach;
}

// The engine's path to the goal (dao/World.h, FindPath), to its end for melee; for a ranged
// attack, along it to where the engine's move update would end it, its own tests (WithinBand,
// LineOfSight from each point) asked an update's run apart, its 1 m cut of the stop distance
// without a line of sight copied (RangedWalk).
std::optional<Walked> Walk(ObjectId attacker, ObjectId target)
{
    const auto approach = ApproachOf(attacker, target);
    if (!approach)
    {
        return std::nullopt;
    }
    // A blow that will find no room is given up, at its first run or on
    // the way, so it has no walk.
    if (!approach->ranged && !approach->findsRoom)
    {
        return std::nullopt;
    }
    if (approach->inReach)
    {
        return Walked{};
    }
    // A ranged walk is toward the target, which stands at the goal; a melee
    // one to an empty place on its ring.
    const auto path = world::FindPath(attacker, approach->goal, approach->ranged ? target : kInvalidObject);
    if (!path)
    {
        return std::nullopt;
    }
    if (!approach->ranged)
    {
        return Walked{path->Length(), path->waypoints};
    }
    return RangedWalk(Creature(attacker), Creature(target), target, path->waypoints, approach->stopWithin,
                      approach->spaces);
}

// Read only (DAOLib #19). A creature keeps a std::vector of rings at its +0xB44, each a radius
// (+0x10) and a pointer array of places (+0x48, the count +0x4C); MeleeRingSpot tries them
// nearest first by how far the attacker is from each one's radius. A place is free to an
// attacker (0x007CAED0) when it is not blocked (+8), not near-only (+9) unless the attacker stands
// inside the radius, and every holder (+0xC, +0x14, +0x18, +0x1C) is none or the attacker; a run
// of places as wide as the attacker must be free, and the place's point must pass the fit test
// (0x007CB0E0: on ground under a 60 degree slope, and no cell about it unwalkable or under a
// creature's stamp, 0x00A46FC0 with stamps counted). A claim writes holders[2] (+0x18) over the
// run.
std::vector<Ring> Rings(ObjectId target)
{
    // The rings: a std::vector of pointers at the creature's +0xB44 (its
    // first and last at +4 and +8); each ring's radius at +0x10 and its
    // places a pointer array at +0x48, the count at +0x4C.
    constexpr std::uintptr_t kRings = 0xB44;
    constexpr std::uintptr_t kFirst = 4;
    constexpr std::uintptr_t kLast = 8;
    constexpr std::uintptr_t kRadius = 0x10;
    constexpr std::uintptr_t kPlaces = 0x48;
    constexpr std::uintptr_t kCount = 0x4C;
    constexpr std::uint32_t kMostRings = 16;
    constexpr std::uint32_t kMostPlaces = 256;
    constexpr std::array<std::uintptr_t, 4> kHolders{0xC, 0x14, 0x18, 0x1C};
    std::vector<Ring> rings;
    CCObject *creature = Creature(target);
    if (creature == nullptr)
    {
        return rings;
    }
    const std::uintptr_t vector = reinterpret_cast<std::uintptr_t>(creature) + kRings;
    const auto first = TryRead<std::uint32_t>(vector + kFirst);
    const auto last = TryRead<std::uint32_t>(vector + kLast);
    if (!first || !last || *first == 0 || *last < *first)
    {
        return rings;
    }
    const std::uint32_t count = std::min((*last - *first) / 4, kMostRings);
    for (std::uint32_t r = 0; r < count; ++r)
    {
        const auto ring = TryRead<std::uint32_t>(*first + (r * 4));
        if (!ring || *ring == 0)
        {
            continue;
        }
        Ring out;
        out.radius = TryRead<float>(*ring + kRadius).value_or(0);
        const auto array = TryRead<std::uint32_t>(*ring + kPlaces);
        const auto places = TryRead<std::uint32_t>(*ring + kCount);
        for (std::uint32_t i = 0; array && *array != 0 && places && i < std::min(*places, kMostPlaces); ++i)
        {
            const auto at = TryRead<std::uint32_t>(*array + (i * 4));
            if (!at || *at == 0)
            {
                continue;
            }
            RingPlace place;
            place.blocked = TryRead<std::uint8_t>(*at + 8).value_or(0) != 0;
            place.nearOnly = TryRead<std::uint8_t>(*at + 9).value_or(0) != 0;
            for (std::size_t h = 0; h < kHolders.size(); ++h)
            {
                place.holders[h] = TryRead<ObjectId>(*at + kHolders[h]).value_or(kInvalidObject);
            }
            out.places.push_back(place);
        }
        rings.push_back(std::move(out));
    }
    return rings;
}

// The std::set<ObjectId> at the creature's +0xC18, as the ring refresh walks it.
std::vector<ObjectId> RingNeighbours(ObjectId target)
{
    // A std::set<ObjectId> at +0xC18: its head at +4, the count at +8; a
    // node's left, parent and right at +0, +4 and +8, the id at +0xC and
    // the nil byte at +0x11 (the step at 0x00510D40). In order, as the
    // engine's own step walks it.
    constexpr std::uintptr_t kSet = 0xC18;
    constexpr std::uint32_t kMost = 256;
    std::vector<ObjectId> found;
    CCObject *creature = Creature(target);
    if (creature == nullptr)
    {
        return found;
    }
    const std::uintptr_t set = reinterpret_cast<std::uintptr_t>(creature) + kSet;
    const auto head = TryRead<std::uint32_t>(set + 4);
    const auto count = TryRead<std::uint32_t>(set + 8);
    if (!head || *head == 0 || !count)
    {
        return found;
    }
    const auto nil = [](std::uint32_t node) { return TryRead<std::uint8_t>(node + 0x11).value_or(1) != 0; };
    auto node = TryRead<std::uint32_t>(*head).value_or(0); // the head's left: the first
    for (std::uint32_t i = 0; i < std::min(*count, kMost) && node != 0 && !nil(node); ++i)
    {
        found.push_back(TryRead<ObjectId>(node + 0xC).value_or(kInvalidObject));
        // The next in order.
        const std::uint32_t right = TryRead<std::uint32_t>(node + 8).value_or(0);
        if (right != 0 && !nil(right))
        {
            node = right;
            for (std::uint32_t left = TryRead<std::uint32_t>(node).value_or(0); left != 0 && !nil(left);
                 left = TryRead<std::uint32_t>(node).value_or(0))
            {
                node = left;
            }
            continue;
        }
        std::uint32_t parent = TryRead<std::uint32_t>(node + 4).value_or(0);
        while (parent != 0 && !nil(parent) && node == TryRead<std::uint32_t>(parent + 8).value_or(0))
        {
            node = parent;
            parent = TryRead<std::uint32_t>(node + 4).value_or(0);
        }
        node = parent;
    }
    return found;
}

} // namespace dao::attack
