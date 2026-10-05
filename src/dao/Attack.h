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
//   Range less both spaces of it, on the ground, with a line of sight
//   from there; without one there, 1 m nearer, and so on until that would
//   reach both spaces, where the engine gives the attack up.
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
// none landed, each striking in its first update. The search is the same either way, so the
// answer is the approach's first run's at the moment it is asked: nothing
// exactly where that run fails with COMMAND_FAILED_NO_SPACE_IN_MELEE_RING
// (-5). A place found can be gone later, the ring filled by others: the
// approach asks it again, claiming, whenever the target moves or each 1.5
// s, and the blow asks again if the target does not hold the attacker as
// its own (+0xC60); each fails the command with -5 on a full ring. Seen in
// play 2026-10-04 (the turn-based mod, dragonlings swarming one creature
// above Haven, 18 failures): 6 at the first run, each where ApproachOf had
// answered nothing; 8 during the approach (sub-action 4); 4 at the turn to
// face (39), the blow not yet begun.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <array>
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
    // The approach would end at once and the attack begin, with no walk:
    // as its first run judges it, on the ring as it stands now.
    bool inReach = false;
    // Melee: the blow comes in the command's first update, before the
    // ring is filled (below): in reach, already facing the target within
    // the turn's 1 degree (sub-action 39, 0x007A69E0, through TurnToward),
    // then the blow's own tests (0x007A87C0): its reach (InStrikingReach,
    // the flat distance to the target's point within both spaces and 2.2),
    // the target within 60 degrees of level and in sight.
    bool strikesAtOnce = false;
    // Melee: whether the ring still has a place for the attacker once it
    // is filled with who stands round the target, as it is a frame after
    // the attacker's claim; the place the approach then walks to, and the
    // place the blow asks for if it comes later.
    bool roomOnceFilled = false;
    // Whether the engine's attack will find room on the ring: it strikes
    // at once, or the filled ring has a place. Where it will not, the
    // attack fails COMMAND_FAILED_NO_SPACE_IN_MELEE_RING after its first
    // update (DAOLib #19); and false where the ring as it stands now has no
    // place, where the approach fails at its first run. Ranged, always.
    // Seen in play 2026-10-04 (the turn-based mod's staged crowd of
    // dragonlings): it matched the engine for 184 attacks of 184, false for
    // all 24 that failed for want of room, true for the 160 that did not.
    bool findsRoom = true;
    // Ranged: whether the engine's line-of-sight test passes; melee, true.
    bool lineOfSight = true;
    // Where the approach walks: the attacker's place on the target's melee
    // ring once filled (or as it stands now, where the filled one has
    // none), or the target itself for a ranged attack.
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

// Nothing only if either is not a creature here: not asked. Every answer
// for two creatures is an Approach, `findsRoom` false where the engine's
// attack will fail COMMAND_FAILED_NO_SPACE_IN_MELEE_RING (its ring has no
// place now, or none once filled and the blow does not come at once). On
// the game's thread.
std::optional<Approach> ApproachOf(ObjectId attacker, ObjectId target);

// A target's melee rings as the engine holds them now, read only (DAOLib
// #19). A creature keeps a std::vector of rings at its +0xB44, each a
// radius (+0x10) and a pointer array of places (+0x48, the count +0x4C);
// MeleeRingSpot tries them nearest first by how far the attacker is from
// each one's radius. A place is free to an attacker (0x007CAED0) when it
// is not blocked, not near-only unless the attacker stands inside the
// radius, and every holder is none or the attacker; a run of places as
// wide as the attacker must be free, and the place's point must pass the
// fit test (0x007CB0E0: on ground under a 60 degree slope, and no cell
// about it unwalkable or under a creature's stamp, 0x00A46FC0 with stamps
// counted). A claim writes holders[2] (+0x18) over the run. Empty for no
// creature. On the game's thread.
struct RingPlace
{
    bool blocked = false;  // +8
    bool nearOnly = false; // +9
    std::array<ObjectId, 4> holders{kInvalidObject, kInvalidObject, kInvalidObject,
                                    kInvalidObject}; // +0xC, +0x14, +0x18, +0x1C
};
struct Ring
{
    float radius = 0;
    std::vector<RingPlace> places;
};
std::vector<Ring> Rings(ObjectId target);

// The creatures the target's ring refresh records round it: the
// std::set<ObjectId> at its +0xC18, as the refresh walks it. On the
// game's thread.
std::vector<ObjectId> RingNeighbours(ObjectId target);

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
// way: no room on the melee ring once filled, or no line of sight in time.
// On the game's thread.
std::optional<Walked> Walk(ObjectId attacker, ObjectId target);

} // namespace dao::attack
