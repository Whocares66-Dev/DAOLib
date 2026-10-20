// A FaceFX file out of the game's files, over bytes (DAOLib #206): a spoken
// line's face (.fxe, an animation set of curves) and an actor (.fxa: the bones
// the face moves, the face graph, the phoneme map). For a reimplementation
// that plays a line's face (DAOParagonEngine); dev/ANIMATION.md says how the
// engine plays it, and the names follow OC3 Entertainment's FaceFX SDK 1.7
// (OC3Ent::Face, statically linked in the exe, archive version 1710).
//
// Read 2026-10-19 from the bytes of all 797 .fxe and humanmale.fxa that the
// game's archives hold and from the exe's own serializers (FxActor::Serialize
// 0x00A0AD00, FxAnimSet 0x00A0B640, FxAnim 0x00A0D330, FxAnimCurve 0x00A17160
// and its Evaluate 0x00A16890). Checked: every file parses to its last byte;
// the container, the animation set, the curves and their keys, the master
// bones and their poses, the compiled face graph's nodes and links, the
// phoneme map. Inferred, from values and not from code: the names of the node's
// four floats (min, its reciprocal, max, its reciprocal), the input operation
// (0 sum, 1 multiply), a link's parameters' meaning, the animation's two blend
// floats as blend in and out, the bone's two unexplained fields, the node
// types' meanings (0 a combiner, 4 a bone target, 8 a material target). Read
// and kept but not understood: the licensee's u16, the sets' second slot, an
// animation's trailing word. Only the class versions the game's files hold are
// laid out (FxAnim 6, FxActor 3, FxMasterBoneList 2, FxAnimCurve 1, FxName 1);
// a file of another version is refused, as is one that does not end where its
// last object does.
//
// The face graph's evaluation (Evaluate, below) is lib-agent-2's reading of
// 2026-10-19 (FxFaceGraph::Update 0x00A19B60, the link functions' switch at
// 0x00A19E9B with its jump table 0x00A1A688, the bone update 0x0084F550). Read
// in the exe: the one forward pass in the file's node order, the sum or
// product of the links, the corrective accumulator, the clamp. Inferred: that
// the corrective accumulator is capped at 1, that every node's input index is
// below its own in the actors the game does not ship (checked in the 13
// readable ones), that node types 0, 4 and 8 are alike but for the emotion
// weights, and the bone update's blend (below). Not read, and so not used:
// blendIn and blendOut, the per-bone weights (always 1.0), a node's tracked
// user value (never set by the player).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace dao::core::facefx
{

// The archive's header: the licensee, the class versions the objects are laid out by, and the name table.
struct Header
{
    std::uint32_t archiveVersion = 0; // 1710 in the game's files
    std::string licensee;             // "Bioware Edmonton"
    std::string licenseeProject;
    std::uint32_t licenseeVersion = 0;
    std::uint16_t unknownWord = 0; // always 1
    std::vector<std::pair<std::string, std::uint16_t>> classVersions;
    std::vector<std::string> names;

    // Returns a class's version in the file, or nullopt.
    std::optional<std::uint16_t> ClassVersion(const std::string &className) const;
};

// One key of a curve.
struct AnimKey
{
    float time = 0;     // seconds, from before the line's start
    float value = 0;    // about 0 to 1
    float slopeIn = 0;  // value per second
    float slopeOut = 0; // value per second
};

// A curve over one face graph node, named by it.
struct AnimCurve
{
    enum class Interpolation : std::uint32_t
    {
        Hermite = 0, // cubic, by the keys' slopes
        Linear = 1,
    };

    std::string nodeName;
    std::uint32_t interpolation = 0; // an Interpolation; kept as read
    std::vector<AnimKey> keys;

    // Returns the curve's value at a time as FxAnimCurve::Evaluate does: 0 with no keys, the first value before the
    // first key, the last after the last.
    float Evaluate(float time) const;
};

// A bone's weight in an animation; none are in the game's files.
struct AnimBoneWeight
{
    std::string bone;
    float weight = 0;
};

// An animation: the curves of one line.
struct Anim
{
    std::string name;
    std::vector<AnimCurve> curves;
    float blendIn = 0;  // seconds
    float blendOut = 0; // seconds
    std::vector<AnimBoneWeight> boneWeights;
    std::string reserved1; // empty in every file
    std::string reserved2;
    std::uint32_t tail = 0; // 0xFFFFFFFF in every file

    // Returns the latest key's time over the curves, 0 with none.
    float EndTime() const;
};

struct AnimGroup
{
    std::string name;
    std::vector<Anim> anims;
};

// What an .fxe holds.
struct AnimSet
{
    Header header;
    std::string name; // the line, "100007_m"
    std::uint32_t secondSlot = 0;
    std::vector<AnimGroup> groups;
};

// A bone's transform: the reference pose, and a pose a node gives it.
struct Bone
{
    std::string name;
    std::array<float, 3> position{};
    std::array<float, 4> rotation{1, 0, 0, 0}; // w, x, y, z
    std::array<float, 3> scale{1, 1, 1};       // a pose's is a delta
};

// A pose a node gives a bone: scaled by the node's value, added to the reference.
struct BonePose
{
    std::uint32_t nodeIndex = 0;
    Bone pose;
};

// A bone the face moves, with the poses the nodes give it.
struct MasterBone
{
    Bone reference;
    std::array<float, 4> conjugate{1, 0, 0, 0}; // the reference rotation's conjugate, w, x, y, z
    std::uint32_t unknownIndex = 0;
    float unknownFloat = 1;
    std::vector<BonePose> poses;
};

// A link into a node from another, by a link function (1 linear, 2 quadratic, 3 cubic, 4 sqrt, 5 negate, 6 inverse, 7
// one-clamp, 8 constant, 9 corrective, 10 clamped linear, 0 null).
struct FaceGraphLink
{
    std::uint32_t inputNode = 0;
    std::uint32_t functionId = 0;
    std::vector<float> parameters;
};

struct FaceGraphUserProperty
{
    std::string name;
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    float c = 0;
    std::vector<std::string> strings;
};

struct FaceGraphNode
{
    std::uint32_t type = 0; // 0 combiner, 4 bone target, 8 material target [inferred]
    std::string name;
    float min = 0;
    float minReciprocal = 0;
    float max = 0;
    float maxReciprocal = 0;
    std::uint32_t inputOperation = 0; // 0 sum, 1 multiply [inferred]
    std::vector<FaceGraphLink> links;
    std::vector<FaceGraphUserProperty> userProperties;
};

struct PhonemeMapEntry
{
    std::uint32_t phoneme = 0;
    std::string node;
    float weight = 0;
};

// What an .fxa holds.
struct Actor
{
    Header header;
    std::string name;
    std::uint32_t secondSlot = 0;
    std::vector<MasterBone> bones;
    std::vector<FaceGraphNode> nodes;
    std::vector<std::pair<std::string, std::uint32_t>> nodeIndexByName; // as the file's map holds it
    AnimGroup defaultGroup;
    std::vector<AnimGroup> groups;
    std::vector<PhonemeMapEntry> phonemeMap;
    std::vector<std::string> phonemeNames;

    // Returns a node's index by its name from the file's map, or nullopt.
    std::optional<std::uint32_t> NodeIndex(const std::string &nodeName) const;
};

// A bone's transform after the face graph, as the bone update computes it.
struct BoneTransform
{
    std::array<float, 3> position{};
    std::array<float, 4> rotation{1, 0, 0,
                                  0}; // w, x, y, z: the turn on the reference, rotationDelta times it [inferred]
    std::array<float, 3> scale{1, 1, 1};
    std::array<float, 4> rotationDelta{1, 0, 0,
                                       0}; // w, x, y, z: the turn from the reference, as the engine works it out
    std::array<float, 4> nodeRotation{
        0, 0, 0, -1}; // x, y, z, w: rotationDelta as the engine hands it to the node, {x, z, y, -w}
    std::array<float, 3>
        positionDelta{}; // (position - reference) / 100 with y and z swapped, as the engine hands it to the node
};

// What a time of an animation makes of an actor's face.
struct FaceState
{
    std::vector<float> nodeValues; // by node index
    std::array<float, 8>
        emotionWeights{}; // mml_fEmotionsMapWeight's slots, from the type 8 nodes by their "Material parameter index"
    std::vector<BoneTransform> bones; // by master bone
};

// Returns a link function's value for an input x and the link's parameters, by its id; ids above 10 give 0, and so does
// the corrective (9), which the node handles.
float LinkFunction(std::uint32_t functionId, std::span<const float> parameters, float x);

// Returns every node's value at a time: the animation's curves set their nodes (found by name), then one pass in the
// file's node order.
std::vector<float> EvaluateGraph(const Actor &actor, const Anim &anim, float time);

// Returns the master bones' transforms for node values (from EvaluateGraph).
std::vector<BoneTransform> EvaluateBones(const Actor &actor, std::span<const float> nodeValues);

// Returns the face at a time of an animation, in seconds as the curves' keys are.
FaceState Evaluate(const Actor &actor, const Anim &anim, float time);

// Reads an .fxe, or returns nullopt on bytes that are not one or are cut or damaged.
std::optional<AnimSet> ReadAnimSet(std::span<const std::byte> bytes);

// Reads an .fxa, or returns nullopt on bytes that are not one or are cut or damaged.
std::optional<Actor> ReadActor(std::span<const std::byte> bytes);

} // namespace dao::core::facefx
