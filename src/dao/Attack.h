// An attack as the engine makes one: whether an attacker is in reach of its
// target, where its approach would walk and how far, each by calling the
// attack command's own code, so a plugin keeps no copy of the rule (DAOLib
// issue #16; the turn-based mod's copy drifted, and stopped 67 archers'
// shots in one ambush).
//
//   if (const auto approach = dao::attack::ApproachOf(attacker, target))
//       if (!approach->inReach)
//           if (const auto walk = dao::attack::Walk(attacker, target))
//               spdlog::info("{:.1f} m to walk before the blow", walk->metres);
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
//   here: the blow itself (sub-action 1) waits until the attacker faces
//   within 60 degrees and sees the target, and only then does the engine
//   send EVENT_TYPE_COMMAND_PENDING, for the scripts to resolve it; a
//   shot turns to face, aims (within 67.5 degrees, in sight) and then
//   shoots (2), which sends it (dao/Conjure.h, RunningSubAction).
// - Ranged (a staff, bow or crossbow in the main hand): in range when the
//   distance on the ground (x and y) is at most the weapon's Range
//   (bitm_base, capped at 60 m) and at least both personal spaces, and the
//   attacker has line of sight. The walk is toward the target until within
//   Range less both spaces of it, on the ground, with a line of sight
//   from there; without one there, 1 m nearer, and so on until that would
//   reach both spaces, where the engine gives the attack up.
// The ring place is asked for without claiming it: the engine's own
// approach claims it (and lets go of the attacker's last), which a
// question here must not.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <optional>
#include <vector>

namespace dao::attack
{

// Whether the attack command takes a creature's attack as ranged: a
// creature whose main-hand item's base type (bitm_base's Type) is 5, a
// staff, or 6, a bow or crossbow; a staff's Range is 45 m, as a bow's.
bool IsRanged(ObjectId attacker);

// The attack command's approach for a pair as they stand now.
struct Approach
{
    bool ranged = false;
    // The approach would end at once and the attack begin, with no walk.
    bool inReach = false;
    // Ranged: whether the engine's line-of-sight test passes; melee, true.
    bool lineOfSight = true;
    // Where the approach walks: the attacker's place on the target's melee
    // ring, or the target itself for a ranged attack.
    Vector goal;
    // Ranged: the straight distance from the target at which the walk
    // ends, Range less both spaces; melee, 0, the walk ends at the goal.
    float stopWithin = 0;
    // Ranged: the weapon's Range as the command takes it, at most 60 m;
    // melee, 0 (its reach is the engine's own test, InMeleeReach).
    float range = 0;
    // Both creatures' personal spaces: ranged, the nearest a shot is made
    // from.
    float spaces = 0;
};

// Nothing if either is not a creature here, or the target has no place on
// its ring for the attacker (where the engine's own approach fails). On
// the game's thread.
std::optional<Approach> ApproachOf(ObjectId attacker, ObjectId target);

// The walk that approach is: the engine's path to the goal (dao/World.h,
// FindPath), to its end for melee; for a ranged attack, along it to where
// the engine's move update would end it, its own tests (WithinBand,
// LineOfSight from each point) asked every 0.25 m, its 1 m step nearer
// without a line of sight copied. In reach, 0 m and no path.
struct Walked
{
    float metres = 0;
    std::vector<Vector> path; // from the attacker to where the walk ends
};
// Nothing if no path reaches, or the engine would give the attack up on the
// way. On the game's thread.
std::optional<Walked> Walk(ObjectId attacker, ObjectId target);

} // namespace dao::attack
