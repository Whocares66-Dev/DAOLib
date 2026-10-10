// Answers questions about an attack by running the game's own attack code, so a plugin can plan
// attacks without keeping a copy of the game's rules. It tells you whether an attacker is in reach
// of its target, whether it will find room on the target's melee ring (the places around a
// creature where melee attackers stand), where and how far it would walk, and how far it must
// turn first.
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

// How the attack command would approach a target, given where the attacker and target stand now.
struct Approach
{
    bool ranged = false;
    // Whether the attack would start at once, without a walk. For melee, this is judged on the ring
    // as it is now. For a ranged attack, the target must be within the weapon's range, no nearer
    // than `spaces`, and in sight.
    bool inReach = false;
    // Melee: whether the blow lands in the command's first update, before the ring fills. It does
    // when the attacker is in reach and faces the target to within 1 degree, and the target is
    // within striking reach, no more than 60 degrees above or below, and in sight.
    bool strikesAtOnce = false;
    // Melee: whether the ring still has a place for the attacker after it fills. One frame after
    // an attacker claims a place, the ring fills with the creatures standing around the target.
    bool roomOnceFilled = false;
    // Whether the attack will find room on the ring: the ring has a place now, and either the blow
    // lands at once or the filled ring still has a place. When false, the attack fails with
    // COMMAND_FAILED_NO_SPACE_IN_MELEE_RING. Always true for a ranged attack.
    bool findsRoom = true;
    bool lineOfSight = true; // ranged: whether the game's line-of-sight test passes; melee: true
    // Where the approach walks to. Melee: the attacker's place on the filled ring, or else its
    // place on the ring as it is now, or else the target's position. Ranged: the target's
    // position.
    Vector goal;
    // Ranged: the straight-line distance from the target at which the walk stops, `range` minus
    // `spaces`. Melee: 0, since the walk ends at `goal`.
    float stopWithin = 0;
    float range = 0;  // ranged: the weapon's range in metres, at most 60; melee: 0
    float spaces = 0; // both creatures' personal spaces, in metres; the minimum shot distance
};

// Returns how the attack command would approach a target from where the attacker stands now.
// @return the approach, or nullopt if either object is not a creature
//
// A ring place found now can be taken by other creatures before the attacker gets there, and the
// attack then fails.
std::optional<Approach> ApproachOf(ObjectId attacker, ObjectId target);

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

} // namespace dao::attack
