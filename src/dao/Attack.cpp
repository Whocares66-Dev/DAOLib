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
// How finely a ranged walk is followed along its path: the engine tests
// each frame where the walker stands, a few centimetres apart at a walk.
constexpr float kWalkStep = 0.25F;
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
// called at each point a kWalkStep apart; the 1 m and the giving up are
// the move update's own, copied. Nothing if it is given up or the path
// ends first.
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
    Walked walked;
    walked.path.push_back(path.front());
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        const Vector &a = path[i - 1];
        const Vector &b = path[i];
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float dz = b.z - a.z;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        const int steps = std::max(1, static_cast<int>(std::ceil(length / kWalkStep)));
        for (int step = 1; step <= steps; ++step)
        {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            const Vector point{a.x + t * dx, a.y + t * dy, a.z + t * dz};
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

} // namespace

bool IsRanged(ObjectId attacker)
{
    CCObject *creature = Creature(attacker);
    return creature != nullptr && HasRangedWeapon(creature);
}

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
    Vector4 spot;
    if (!At<SpotFn>(addr::kCCCreature_MeleeRingSpot)(to, 0, attacker, &spot, space, 0, 0))
    {
        return std::nullopt;
    }
    approach.goal = Vector{spot.x, spot.y, spot.z};
    approach.inReach = At<ReachFn>(addr::kCCCreature_InMeleeReach)(to, 0, from, &spot) != 0;
    return approach;
}

std::optional<Walked> Walk(ObjectId attacker, ObjectId target)
{
    const auto approach = ApproachOf(attacker, target);
    if (!approach)
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

} // namespace dao::attack
