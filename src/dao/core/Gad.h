// A clip's root motion (.gad) out of the game's files, over bytes (DAOLib #287): how far and
// which way a clip carries the creature that plays it, as keyed curves, and the engine's way of
// sampling them. dao/core/Clip.h reads the clip's .ani beside it; a cutscene's own travel keys
// (dao/core/Cutscene.h, GadKeysPos and GadKeysOri) are sampled the same way.
//
// Read 2026-10-31 in mh_m.mov_wf_p_lfoot.gad and the GOG exe. A .gad is GFF4 "GAD V0.1": the
// clip's name (AnimationGeneralAnimName, 4007), its length in seconds (4009), whether it has root
// motion (AnimationHasGobAnim, 4008) and a list of nodes (4005), each a name (4000), a target
// (4001), a source type (4002), the values to a key (4003) and the keys as one list of floats
// (4004), each key its time in seconds and then its values. The engine reads two nodes by name:
// "gob_gobtranslation" (0x00A390F0), three values to a key, where the clip has carried the
// creature by then; and "gob_gobrotation" (0x00A38F70), four, x, y, z, w, how far it has turned
// it. The walk-to-pause clip has both, thirty keys a second; the walk loop, mh_m.mov_wf, has
// neither and HasGobAnim 0, which leaves the creature to its own movement.
#pragma once

#include "dao/core/Gff4.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dao::core::gad
{

using Vector3 = std::array<float, 3>;
using Quaternion = std::array<float, 4>; // x, y, z, w

// A key of a clip's travel: where its root has gone, cumulative from the clip's start.
struct PositionKey
{
    float time = 0.0F; // seconds into the clip
    Vector3 position{};
};

// A key of a clip's turn, cumulative from the clip's start.
struct OrientationKey
{
    float time = 0.0F; // seconds into the clip
    Quaternion orientation{0.0F, 0.0F, 0.0F, 1.0F};
};

// Returns the keys of a list of floats, each a time and three values, as a .gad's translation
// and a cutscene's GadKeysPos hold them. A part key at the end is left out.
std::vector<PositionKey> PositionKeysOf(const gff4::Struct &in, std::uint32_t label);
// Returns the keys of a list of floats, each a time and a rotation's x, y, z and w.
std::vector<OrientationKey> OrientationKeysOf(const gff4::Struct &in, std::uint32_t label);

// Returns the travel at a time: the keys either side blended by the time between them, the first
// key's before it and the last's after.
// @return nothing for no keys
std::optional<Vector3> PositionAt(const std::vector<PositionKey> &keys, float time);

// Returns the turn at a time: the keys either side slerped as the engine slerps them, the first
// key's before it and the last's after.
// @return nothing for no keys
std::optional<Quaternion> OrientationAt(const std::vector<OrientationKey> &keys, float time);

// Returns the rotation a fraction of the way from one to another, the shorter way round.
Quaternion Slerp(const Quaternion &a, Quaternion b, float fraction);

// Returns a rotation's yaw as the engine reckons it: the heading of the forward axis (+y) turned
// by it, in degrees, turning towards -x positive.
float YawDegrees(const Quaternion &rotation);

struct Gad
{
    std::wstring name; // the clip's: "mov_wf_p_lfoot"
    float length = 0;  // seconds
    // Whether the engine moves the creature by the clip; without it, the keys are not played.
    bool hasGobAnim = false;
    std::vector<PositionKey> translation; // gob_gobtranslation
    std::vector<OrientationKey> rotation; // gob_gobrotation
};

// Returns a clip's root motion, or nothing for a file that is not one.
std::optional<Gad> GadOf(const gff4::File &file);

// What a clip does to the creature over its whole length, before it plays.
struct Summary
{
    float length = 0; // seconds
    bool hasGobAnim = false;
    Vector3 translation{}; // the last key's travel less the first's
    float yawDegrees = 0;  // the turn from the first key to the last, as YawDegrees
};

Summary SummaryOf(const Gad &gad);

} // namespace dao::core::gad
