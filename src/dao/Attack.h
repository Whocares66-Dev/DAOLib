// Answers questions about an attack by running the game's own attack code, so a plugin can plan
// attacks without keeping a copy of the game's rules. It tells you whether an attacker is in reach
// of its target, whether it will find room on the target's melee ring (the places around a
// creature where melee attackers stand), where and how far it would walk, and how far it must
// turn first. ApproachOf and Walk answer the same for an ability used on a target.
//
//   if (const auto approach = dao::attack::ApproachOf(attacker, target))
//       if (!approach->inReach)
//           if (const auto walk = dao::attack::Walk(attacker, target))
//               spdlog::info("{:.1f} m to walk before the blow", walk->metres);
//
// Call every function on the game thread. None of them claims a place on a ring, and only
// FaceExactly changes anything.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <array>
#include <optional>
#include <vector>

namespace dao::attack
{

// Returns whether a creature's attack counts as ranged, using the attack command's own test: true
// when its main hand holds a staff, a bow or a crossbow.
// @return false also when `attacker` is not a creature
bool IsRanged(ObjectId attacker);

// How an attack or an ability would approach its target, given where the two stand now. The
// game approaches in one of two ways. "Melee" below is the approach to a place on the target's
// ring: a plain attack without a ranged weapon, or an ability whose Range in the abilities table
// is 0. "Ranged" is the approach to within a range: a plain attack with a bow, a crossbow or a
// staff, or an ability with any other Range.
struct Approach
{
    bool ranged = false;
    // Whether the action would start at once, without a walk. For melee, this is judged on the ring
    // as it is now. For a ranged attack, the target must be within the weapon's range, no nearer
    // than `spaces`, and in sight. For a ranged ability, it must be within the ability's range, and
    // in sight if `needsSight`.
    bool inReach = false;
    // A melee attack: whether the blow lands in the command's first update, before the ring fills.
    // It does when the attacker is in reach and faces the target to within 1 degree, and the target
    // is within striking reach, no more than 60 degrees above or below, and in sight. Always false
    // for an ability.
    bool strikesAtOnce = false;
    // Melee: whether the ring still has a place for the attacker after it fills. One frame after
    // an attacker claims a place, the ring fills with the creatures standing around the target.
    bool roomOnceFilled = false;
    // Whether the action will find room on the ring. The ring must have a place now, and the
    // filled ring must still have one, unless the action is done with the ring before it fills: an
    // attack whose blow lands at once, or an ability whose user is already in reach. When false,
    // the command fails with COMMAND_FAILED_NO_SPACE_IN_MELEE_RING. Always true when ranged.
    bool findsRoom = true;
    // Ranged: whether the approach needs a line of sight to the target. A ranged attack does; an
    // ability does if it has a projectile.
    bool needsSight = false;
    // Ranged: whether the game's line-of-sight test passes; true where none is needed. Melee: true.
    bool lineOfSight = true;
    // Where the approach walks to. Melee: the attacker's place on the filled ring, or else its
    // place on the ring as it is now, or else the target's position. Ranged: the target's
    // position.
    Vector goal;
    // Ranged: the straight-line distance from the target at which the walk stops: `range` minus
    // `spaces` for an attack, `range` for an ability. Melee: 0, since the walk ends at `goal`.
    float stopWithin = 0;
    float range = 0; // ranged: the weapon's or the ability's range in metres, at most 60; melee: 0
    // Both creatures' personal spaces, in metres. A ranged attack shoots from no nearer.
    float spaces = 0;
};

// Returns how the attack command would approach a target from where the attacker stands now.
// @return the approach, or nullopt if either object is not a creature
//
// A ring place found now can be taken by other creatures before the attacker gets there, and the
// attack then fails.
std::optional<Approach> ApproachOf(ObjectId attacker, ObjectId target);

// Returns how a creature using an ability on a target would approach it from where it stands now.
// @param ability a row of the abilities table (TABLE_ABILITIES_SPELLS)
// @return the approach, or nullopt if either object is not a creature or `ability` is not a row
// of the table
//
// The ability's own Range decides the kind of approach, whatever weapon the user holds. An
// ability used on its user has no approach, and the answer is in reach. An ability aimed at a
// point on the ground is not answered. As with an attack, a ring place found now can be gone
// before the user gets there.
std::optional<Approach> ApproachOf(ObjectId user, ObjectId target, std::int32_t ability);

// The turn an attacker's command makes to face its target before the blow, computed for now.
struct FacingTurn
{
    // The point the attacker turns toward: a point on the target for a melee attacker, the
    // target's position for a ranged one.
    Vector point;
    Vector toward; // the unit direction on the ground from the attacker to `point`
    // The turn still to make, in degrees from -180 to 180; positive is counter-clockwise seen from
    // above.
    float degrees = 0;
    bool done = false; // already facing to within 1 degree, so the command will not turn
    // The turn is not done and the game will never start it, so the command waits forever. This
    // happens when the creature's movement controller, which walks and turns it, is idle and
    // already thinks it faces `toward`. FaceExactly fixes it.
    bool stalls = false;
    // The turn is not done and has not started only because one of the creature's own animations
    // is playing. It starts by itself when the animation ends, so nothing needs fixing.
    bool waits = false;
};

// Computes the turn an attacker's command would make to face its target now.
// @return the turn, or nullopt if either object is not a creature or the attacker stands on the
// point it would turn to
//
// When `stalls` is set, the scripts' SetFacing commands do not help; FaceExactly does.
std::optional<FacingTurn> TurnTo(ObjectId attacker, ObjectId target);

// Turns a creature to face exactly along a direction on the ground, and sets its movement
// controller's facing to match, so the game starts no turn of its own.
// @param toward the direction to face; only x and y are used
// @return false, changing nothing, if `creature` is not a creature, `toward` has no length on the
// ground, or the creature is walking or turning
//
// Use it to fix `FacingTurn::stalls`:
//
//   if (const auto turn = dao::attack::TurnTo(attacker, target); turn && turn->stalls)
//   {
//       dao::attack::FaceExactly(attacker, turn->toward);
//   }
bool FaceExactly(ObjectId creature, Vector toward);

// One place on a melee ring.
struct RingPlace
{
    bool blocked = false;  // no attacker can stand here
    bool nearOnly = false; // free only to an attacker standing inside the ring's radius
    // The creatures holding the place, kInvalidObject where there is none. An attacker's claim
    // writes holders[2].
    std::array<ObjectId, 4> holders{kInvalidObject, kInvalidObject, kInvalidObject, kInvalidObject};
};

// One of a creature's melee rings.
struct Ring
{
    float radius = 0; // in metres
    std::vector<RingPlace> places;
};

// Returns a target's melee rings as they are now, without changing them.
// @return the rings, or an empty vector if `target` is not a creature
//
// The game tries the rings in order of how close the attacker is to each one's radius. A place is
// free to an attacker when it is not blocked, is not near-only (unless the attacker stands inside
// the radius), and no one else holds it.
std::vector<Ring> Rings(ObjectId target);

// Returns the creatures around the target, as the game last recorded them when it refreshed the
// target's rings.
// @return their ids, or an empty vector if `target` is not a creature
std::vector<ObjectId> RingNeighbours(ObjectId target);

// The walk an attacker would make before its attack.
struct Walked
{
    float metres = 0;         // the walk's length; 0 when already in reach
    std::vector<Vector> path; // from the attacker to where the walk ends; empty when in reach
};

// Computes the walk an attacker would make before attacking, along the game's path to the
// approach's goal. A melee walk goes to the end of the path; a ranged walk stops where the game
// would stop it, in range and in sight.
// @return the walk, or nullopt if either object is not a creature, no path reaches the goal, or
// the game would give up the attack on the way (no room on the melee ring, or no line of sight in
// time)
//
// A ranged walk that begins in range but out of sight depends on the frame rate in the game. This
// reckons it at sixty frames a second and the usual combat run, and can be a few metres off.
std::optional<Walked> Walk(ObjectId attacker, ObjectId target);

// Computes the walk a creature would make before using an ability on a target. It works like
// Walk above, with the approach ApproachOf gives for the ability.
// @param ability a row of the abilities table (TABLE_ABILITIES_SPELLS)
std::optional<Walked> Walk(ObjectId user, ObjectId target, std::int32_t ability);

} // namespace dao::attack
