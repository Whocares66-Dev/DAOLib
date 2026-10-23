#include "dao/core/Cutscene.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <numbers>
#include <span>
#include <utility>

namespace dao::core::cutscene
{

namespace
{

constexpr std::uint32_t kTag = 1;
constexpr std::uint32_t kName = 2;
constexpr std::uint32_t kText = 22;
constexpr std::uint32_t kArea = 3010;
constexpr std::uint32_t kFogEnabled = 3168;
constexpr std::uint32_t kRunTime = 5000;
constexpr std::uint32_t kEndScript = 5001;
constexpr std::uint32_t kBlendTree = 5007;
constexpr std::uint32_t kFps = 5016;
constexpr std::uint32_t kStage = 5017;
constexpr std::uint32_t kPlayUntilVoCompletes = 5018;
constexpr std::uint32_t kAreaRequired = 5019;
constexpr std::uint32_t kHenchmanTag = 5050;
constexpr std::uint32_t kResources = 5100;
constexpr std::uint32_t kActors = 5200;
constexpr std::uint32_t kActorId = 5201;
constexpr std::uint32_t kActorModel = 5202;
constexpr std::uint32_t kActorActions = 5206;
constexpr std::uint32_t kActorCreature = 5208;
constexpr std::uint32_t kActorCameraTarget = 5209;
constexpr std::uint32_t kActorPose = 5211;
constexpr std::uint32_t kActorOriginPosition = 5214;
constexpr std::uint32_t kActorOriginOrientation = 5215;
constexpr std::uint32_t kActorMappingTag = 5216;
constexpr std::uint32_t kActorInventory = 5217;
constexpr std::uint32_t kActorMappingRequired = 5220;
constexpr std::uint32_t kActorFinalPosition = 5221;
constexpr std::uint32_t kActorFinalOrientation = 5222;
constexpr std::uint32_t kActorMaster = 5223;
constexpr std::uint32_t kActorScale = 5226;
constexpr std::uint32_t kActionType = 5300;
constexpr std::uint32_t kActionStart = 5301;
constexpr std::uint32_t kActionStop = 5302;
constexpr std::uint32_t kActionCurves = 5303;
constexpr std::uint32_t kActionCategory = 5304;
constexpr std::uint32_t kCurveBase = 5350;
constexpr std::uint32_t kCurveVertices = 5351;
constexpr std::uint32_t kCurveTransitions = 5352;
constexpr std::uint32_t kVertexTime = 5370;
constexpr std::uint32_t kVertexValue = 5371;
constexpr std::uint32_t kTransitionType = 5380;
constexpr std::uint32_t kTransitionControl1 = 5381;
constexpr std::uint32_t kTransitionControl2 = 5382;
constexpr std::uint32_t kAnimation = 5400;
constexpr std::uint32_t kAnimationBlendTree = 5401;
constexpr std::uint32_t kAnimationSpeed = 5402;
constexpr std::uint32_t kAnimationStartOffset = 5403;
constexpr std::uint32_t kAnimationPlayGad = 5405;
constexpr std::uint32_t kAnimationGadKeysPos = 5408;
constexpr std::uint32_t kAnimationGadKeysOri = 5409;
constexpr std::uint32_t kAnimationBlendGad = 5410;
constexpr std::uint32_t kAnimationExtendGad = 5411;
constexpr std::uint32_t kAnimationApplyFutureGads = 5413;
constexpr std::uint32_t kEffectName = 5521;
constexpr std::uint32_t kEffectParameters = 5522;
constexpr std::uint32_t kParameterName = 5523;
constexpr std::uint32_t kParameterValue = 5524;
constexpr std::uint32_t kParameterCurve = 5525;
constexpr std::uint32_t kParameterVector = 5526;
constexpr std::uint32_t kLineLipsync = 5562;
constexpr std::uint32_t kLineVoBank = 5563;
constexpr std::uint32_t kStageCameraDefault = 5570;
constexpr std::uint32_t kStageCameraHenchman = 5571;
constexpr std::uint32_t kStageLookAt = 5580;
// The four actions that name another actor, each in a field of its own:
// AACC's camera and ALNK's and ACRU's targets an S32, ACHT's a U32.
constexpr std::array<std::uint32_t, 4> kOtherActor = {5610, 5620, 5630, 5640};
constexpr std::uint32_t kHeadtrackSpeed = 5621;
constexpr std::uint32_t kHeadtrackRealignStart = 5628;
constexpr std::uint32_t kHeadtrackRealignCont = 5629;
constexpr std::uint32_t kLinkNode = 5631;
constexpr std::uint32_t kSound = 5670;
constexpr std::uint32_t kVisible = 5680;
constexpr std::uint32_t kDrawMain = 5730;
constexpr std::uint32_t kDrawOff = 5731;
constexpr std::uint32_t kMovie = 5740;
constexpr std::uint32_t kFogColor = 22529;
constexpr std::uint32_t kFogIntensity = 22530;
constexpr std::uint32_t kFogCap = 22531;
constexpr std::uint32_t kFogZenith = 22532;

template <std::size_t N> std::array<float, N> Several(const gff4::Struct &in, std::uint32_t label)
{
    const std::vector<float> floats = in.Floats(label);
    std::array<float, N> out{};
    for (std::size_t i = 0; i < N && i < floats.size(); ++i)
    {
        out[i] = floats[i];
    }
    return out;
}

// An actor's id as a field holds it, signed or not: ACHT's no-one is 0xFFFFFFFF, the others' -1.
std::optional<std::int32_t> Id(const gff4::Struct &in, std::uint32_t label)
{
    if (const auto whole = in.Get<std::int32_t>(label))
    {
        return whole;
    }
    if (const auto unsigned_ = in.Get<std::uint32_t>(label))
    {
        return static_cast<std::int32_t>(*unsigned_);
    }
    return std::nullopt;
}

// A list of floats, GadKeysPos's or GadKeysOri's, cut into keys of a time and Width values.
template <std::size_t Width, class Key, class Make>
std::vector<Key> KeysOf(const gff4::Struct &in, std::uint32_t label, Make make)
{
    const std::span<const std::byte> bytes = in.ListBytes(label, gff4::Type::F32);
    std::vector<float> floats(bytes.size() / sizeof(float));
    if (!floats.empty())
    {
        std::memcpy(floats.data(), bytes.data(), floats.size() * sizeof(float));
    }
    std::vector<Key> keys;
    keys.reserve(floats.size() / (Width + 1));
    for (std::size_t at = 0; at + Width + 1 <= floats.size(); at += Width + 1)
    {
        keys.push_back(make(floats[at], &floats[at + 1]));
    }
    return keys;
}

Vertex VertexOf(const gff4::Struct &in)
{
    return {.time = in.Get<float>(kVertexTime).value_or(0.0F), .value = in.Get<float>(kVertexValue).value_or(0.0F)};
}

Vertex VertexOf(const std::optional<gff4::Struct> &in)
{
    return in ? VertexOf(*in) : Vertex{};
}

Curve CurveOf(const gff4::Struct &in)
{
    Curve curve{.base = in.Get<float>(kCurveBase).value_or(0.0F), .vertices = {}, .transitions = {}};
    const std::vector<gff4::Struct> vertices = in.Structs(kCurveVertices);
    curve.vertices.reserve(vertices.size());
    for (const gff4::Struct &vertex : vertices)
    {
        curve.vertices.push_back(VertexOf(vertex));
    }
    const std::vector<gff4::Struct> transitions = in.Structs(kCurveTransitions);
    curve.transitions.reserve(transitions.size());
    for (const gff4::Struct &transition : transitions)
    {
        curve.transitions.push_back({.type = transition.Get<std::uint8_t>(kTransitionType).value_or(0),
                                     .control1 = VertexOf(transition.Child(kTransitionControl1)),
                                     .control2 = VertexOf(transition.Child(kTransitionControl2))});
    }
    return curve;
}

std::vector<EffectParameter> ParametersOf(const gff4::Struct &in)
{
    const std::vector<gff4::Struct> list = in.Structs(kEffectParameters);
    std::vector<EffectParameter> out;
    out.reserve(list.size());
    for (const gff4::Struct &parameter : list)
    {
        out.push_back({.name = parameter.String(kParameterName).value_or(L""),
                       .value = parameter.Get<float>(kParameterValue).value_or(0.0F),
                       .curve = parameter.Get<std::int8_t>(kParameterCurve).value_or(-1),
                       .vector = parameter.Get<std::int8_t>(kParameterVector).value_or(-1)});
    }
    return out;
}

std::optional<Fog> FogOf(const gff4::Struct &in)
{
    const auto enabled = in.Get<std::uint8_t>(kFogEnabled);
    if (!enabled)
    {
        return std::nullopt;
    }
    return Fog{.enabled = *enabled != 0,
               .color = Several<3>(in, kFogColor),
               .intensity = in.Get<float>(kFogIntensity).value_or(0.0F),
               .cap = in.Get<float>(kFogCap).value_or(0.0F),
               .zenith = in.Get<float>(kFogZenith).value_or(0.0F)};
}

// Every template's own fields are read from every action: a field its
// template does not have reads as nothing, so the member keeps its default.
// The templates were the same in all 3,158 files (2026-10-18), so no field
// label means two things in two templates.
Action ActionOf(const gff4::Struct &in)
{
    Action action;
    action.kind = in.Label();
    action.type = in.Get<std::uint8_t>(kActionType).value_or(0);
    action.start = in.Get<float>(kActionStart).value_or(0.0F);
    action.stop = in.Get<float>(kActionStop).value_or(0.0F);
    action.category = in.Get<std::uint8_t>(kActionCategory).value_or(0);
    const std::vector<gff4::Struct> curves = in.Structs(kActionCurves);
    action.curves.reserve(curves.size());
    for (const gff4::Struct &curve : curves)
    {
        action.curves.push_back(CurveOf(curve));
    }
    action.animation = in.String(kAnimation).value_or(L"");
    action.blendTree = in.String(kAnimationBlendTree).value_or(L"");
    action.speed = in.Get<float>(kAnimationSpeed).value_or(0.0F);
    action.startOffset = in.Get<float>(kAnimationStartOffset).value_or(0.0F);
    action.playGad = in.Get<std::uint8_t>(kAnimationPlayGad).value_or(0) != 0;
    action.blendGad = in.Get<std::uint8_t>(kAnimationBlendGad).value_or(0) != 0;
    action.extendGad = in.Get<std::uint8_t>(kAnimationExtendGad).value_or(0) != 0;
    action.applyFutureGads = in.Get<std::uint8_t>(kAnimationApplyFutureGads).value_or(0) != 0;
    action.gadPositions = KeysOf<3, PositionKey>(in, kAnimationGadKeysPos, [](float time, const float *v) {
        return PositionKey{.time = time, .position = {v[0], v[1], v[2]}};
    });
    action.gadOrientations = KeysOf<4, OrientationKey>(in, kAnimationGadKeysOri, [](float time, const float *v) {
        return OrientationKey{.time = time, .orientation = {v[0], v[1], v[2], v[3]}};
    });
    action.text = in.Tlk(kText).value_or(gff4::TlkString{});
    action.voBank = in.String(kLineVoBank).value_or(L"");
    action.lipsync = in.String(kLineLipsync).value_or(L"");
    action.sound = in.String(kSound).value_or(L"");
    for (const std::uint32_t label : kOtherActor)
    {
        if (const auto id = Id(in, label))
        {
            action.actor = *id;
        }
    }
    action.node = in.Get<std::int32_t>(kLinkNode).value_or(-1);
    action.lookSpeed = in.Get<float>(kHeadtrackSpeed).value_or(0.0F);
    action.realignStart = in.Get<std::uint8_t>(kHeadtrackRealignStart).value_or(0) != 0;
    action.realignContinue = in.Get<std::uint8_t>(kHeadtrackRealignCont).value_or(0) != 0;
    action.tag = in.String(kTag).value_or(L"");
    action.lookAt = in.String(kStageLookAt).value_or(L"");
    // Bools to the engine, which takes any value but 0 as set (0x007DC450; dev/CUTSCENES.md,
    // JumpToCamera, DAOLib #203).
    action.stageCameraDefault = in.Get<std::uint8_t>(kStageCameraDefault).value_or(0) != 0;
    action.stageCameraHenchman = in.Get<std::uint8_t>(kStageCameraHenchman).value_or(0) != 0;
    action.visible = in.Get<std::int8_t>(kVisible).value_or(0) != 0;
    action.drawMain = in.Get<std::uint8_t>(kDrawMain).value_or(0) != 0;
    action.drawOff = in.Get<std::uint8_t>(kDrawOff).value_or(0) != 0;
    action.effect = in.String(kEffectName).value_or(L"");
    action.parameters = ParametersOf(in);
    action.movie = in.String(kMovie).value_or(L"");
    action.fog = FogOf(in);
    return action;
}

Actor ActorOf(const gff4::Struct &in)
{
    Actor actor{
        .name = in.String(kName).value_or(L""),
        .id = in.Get<std::uint32_t>(kActorId).value_or(0),
        .model = in.String(kActorModel).value_or(L""),
        .creature = in.String(kActorCreature).value_or(L""),
        .mappingTag = in.String(kActorMappingTag).value_or(L""),
        .mappingRequired = in.Get<std::uint8_t>(kActorMappingRequired).value_or(0),
        .cameraTarget = in.Get<std::int32_t>(kActorCameraTarget).value_or(-1),
        .pose = in.Get<std::int32_t>(kActorPose).value_or(-1),
        .originPosition = Several<3>(in, kActorOriginPosition),
        .originOrientation = Several<4>(in, kActorOriginOrientation),
        .finalPosition = Several<3>(in, kActorFinalPosition),
        .finalOrientation = Several<4>(in, kActorFinalOrientation),
        .master = in.Get<std::uint8_t>(kActorMaster).value_or(0),
        .inventory = in.Get<std::uint8_t>(kActorInventory).value_or(0),
        .scale = in.Get<float>(kActorScale).value_or(1.0F),
        .actions = {},
    };
    const std::vector<gff4::Struct> actions = in.Structs(kActorActions);
    actor.actions.reserve(actions.size());
    for (const gff4::Struct &action : actions)
    {
        actor.actions.push_back(ActionOf(action));
    }
    return actor;
}

// Notes on sampling, read 2026-10-18 by lib-agent-2 in the GOG exe (dev/CUTSCENES.md) and
// checked again here in its disassembly (DAOLib #184). A curve is sampled at the cutscene's
// own time, an action's start plus its local time (0x007D30B0), and its vertices' times are
// the cutscene's too. The sampler is 0x007D2440: a curve with no vertices is its base; at or
// before the first vertex it is the first value, at or after the last the last. Otherwise
// the vertices from the second on are walked: one within 1e-4 s of the time (0x00B10B98, a
// double) is its own value, and the first past the time ends the segment. The segment's
// transition type picks the formula (0x007D25C3-0x007D25D7). A Bezier curve's parameter is
// found by bisection (0x007CCDF0): from [0, 1], twenty halvings at most, until the curve's
// time is within 1e-5 (0x00B23124) of the time; its value is then the same curve's
// (0x007CCEF0). The tracks' frames are 0x007D0660 (a position: the stage's frame, then the
// actor's origin) and 0x007CF6B0 (an orientation: stage, origin, then yaw about z, pitch
// about x, roll about y, 0x004EE200), the quaternions' product Hamilton's (0x004CB640).
// Their result was checked against FinalPos (5221) and FinalOri (5222) at CutsceneRunTime
// for the 57 track-driven actors of jowan_attacks, bhm500cs_harrowing and bhmcs_intro.
// The clock is the player's update, 0x007E3D30: its seconds made a whole frame,
// (int)(seconds * FPS + 0.5), and shown as frame / FPS; the load (0x007E1910) takes 30 for
// a file with no CutsceneFPS.

constexpr double kAtVertex = 1e-4;
constexpr float kBezierTolerance = 1e-5F;
constexpr int kBezierHalvings = 20;
constexpr float kEndedBy = 1e-5F; // a clip is over when its local time passes its length less this
constexpr std::uint32_t kDefaultFps = 30;
constexpr std::uint8_t kTypeAnimation = 4;
constexpr std::uint8_t kTypeMoveSpline = 17;
constexpr std::uint8_t kTypeRotate = 18;

float Bezier(float p0, float p1, float p2, float p3, float s)
{
    const float r = 1.0F - s;
    return (r * r * r * p0) + (3.0F * r * r * s * p1) + (3.0F * r * s * s * p2) + (s * s * s * p3);
}

float BezierValue(const Vertex &from, const Transition &transition, const Vertex &to, float time)
{
    float low = 0.0F;
    float high = 1.0F;
    float s = 0.0F;
    for (int halving = 0; halving < kBezierHalvings; ++halving)
    {
        s = low + ((high - low) * 0.5F);
        const float error = Bezier(from.time, transition.control1.time, transition.control2.time, to.time, s) - time;
        if (std::abs(error) <= kBezierTolerance)
        {
            break;
        }
        if (error > 0.0F)
        {
            high = s;
        }
        else
        {
            low = s;
        }
    }
    return Bezier(from.value, transition.control1.value, transition.control2.value, to.value, s);
}

float Sample(const Action &track, std::size_t curve, float time)
{
    return curve < track.curves.size() ? ValueAt(track.curves[curve], time) : 0.0F;
}

Quaternion Multiply(const Quaternion &a, const Quaternion &b)
{
    return {(a[3] * b[0]) + (a[0] * b[3]) + (a[1] * b[2]) - (a[2] * b[1]),
            (a[3] * b[1]) - (a[0] * b[2]) + (a[1] * b[3]) + (a[2] * b[0]),
            (a[3] * b[2]) + (a[0] * b[1]) - (a[1] * b[0]) + (a[2] * b[3]),
            (a[3] * b[3]) - (a[0] * b[0]) - (a[1] * b[1]) - (a[2] * b[2])};
}

// q v q*, for a unit q.
Vector3 Rotate(const Quaternion &q, const Vector3 &v)
{
    const Quaternion turned = Multiply(Multiply(q, {v[0], v[1], v[2], 0.0F}), {-q[0], -q[1], -q[2], q[3]});
    return {turned[0], turned[1], turned[2]};
}

Vector3 Add(const Vector3 &a, const Vector3 &b)
{
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

// A turn of some degrees about one of the axes, 0 x, 1 y, 2 z.
Quaternion About(std::size_t axis, float degrees)
{
    const float half = degrees * (std::numbers::pi_v<float> / 360.0F);
    Quaternion q{0.0F, 0.0F, 0.0F, std::cos(half)};
    q.at(axis) = std::sin(half);
    return q;
}

// A GAD key list is read as the engine's readers do, 0x00A38DE0 for positions and 0x00A38D20
// for orientations: the first key at or before the first key's time, the last at or after the
// last's, and between two the interpolator 0x00A38C60, the keys' values weighted by the times.
// An orientation is read the same way and normalised after; the engine's blend of two
// orientations was not read, and the keys come thirty to a second.
template <class Key> struct Between
{
    const Key *from = nullptr;
    const Key *to = nullptr;
    float fraction = 0.0F;
};

template <class Key> Between<Key> KeysAround(const std::vector<Key> &keys, float time)
{
    if (keys.empty())
    {
        return {};
    }
    if (time <= keys.front().time)
    {
        return {.from = &keys.front(), .to = &keys.front(), .fraction = 0.0F};
    }
    for (std::size_t next = 1; next < keys.size(); ++next)
    {
        if (keys[next].time > time)
        {
            const Key &from = keys[next - 1];
            return {.from = &from, .to = &keys[next], .fraction = (time - from.time) / (keys[next].time - from.time)};
        }
    }
    return {.from = &keys.back(), .to = &keys.back(), .fraction = 0.0F};
}

Quaternion Blend(const Quaternion &a, const Quaternion &b, float fraction)
{
    const float dot = (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]) + (a[3] * b[3]);
    const float sign = dot < 0.0F ? -1.0F : 1.0F; // the shorter way round
    Quaternion q{};
    float length = 0.0F;
    for (std::size_t i = 0; i < 4; ++i)
    {
        q.at(i) = a.at(i) + (((sign * b.at(i)) - a.at(i)) * fraction);
        length += q.at(i) * q.at(i);
    }
    length = std::sqrt(length);
    for (float &component : q)
    {
        component /= length;
    }
    return q;
}

// The track's own place before any stage, 0x007D0660: the actor's origin, then the three curves.
Vector3 LocalPosition(const Actor &actor, const Action &track, float time)
{
    const Vector3 curves{Sample(track, 0, time), Sample(track, 1, time), Sample(track, 2, time)};
    return Add(actor.originPosition, Rotate(actor.originOrientation, curves));
}

// The track's own turn before any stage, 0x007CF6B0: the origin's, then yaw, pitch and roll.
Quaternion LocalTurn(const Actor &actor, const Action &track, float time)
{
    const Quaternion euler = Multiply(Multiply(About(2, Sample(track, 0, time)), About(0, Sample(track, 1, time))),
                                      About(1, Sample(track, 2, time)));
    return Multiply(actor.originOrientation, euler);
}

// The last track of a type that has begun by a time: the one that sets the place then.
const Action &LastBegun(const Actor &actor, std::uint8_t type, float time)
{
    static const Action kNone;
    const Action *found = &kNone;
    for (const Action &action : actor.actions)
    {
        if (action.type == type && time >= action.start - kAtVertex)
        {
            found = &action;
        }
    }
    return *found;
}

} // namespace

float FrameTime(float seconds, std::uint32_t fps)
{
    const auto rate = static_cast<float>(fps == 0 ? kDefaultFps : fps);
    // The engine's (int)(x + 0.5) is the floor for the clock's time, which is never negative.
    return std::floor((seconds * rate) + 0.5F) / rate;
}

float ValueAt(const Curve &curve, float time)
{
    const std::vector<Vertex> &vertices = curve.vertices;
    if (vertices.empty())
    {
        return curve.base;
    }
    if (time <= vertices.front().time)
    {
        return vertices.front().value;
    }
    if (vertices.back().time <= time)
    {
        return vertices.back().value;
    }
    std::size_t k = 0;
    for (std::size_t next = 1; next < vertices.size(); ++next)
    {
        const double at = vertices[next].time;
        if (at > time - kAtVertex && at < time + kAtVertex)
        {
            return vertices[next].value;
        }
        if (time < vertices[next].time)
        {
            k = next - 1;
            break;
        }
    }
    // Ours: a curve with fewer transitions than segments is not in the game's files.
    if (k >= curve.transitions.size())
    {
        return curve.base;
    }
    const Vertex &from = vertices[k];
    const Vertex &to = vertices[k + 1];
    const Transition &transition = curve.transitions[k];
    switch (static_cast<TransitionType>(transition.type))
    {
    case TransitionType::Bezier:
        return BezierValue(from, transition, to, time);
    case TransitionType::Linear:
        return from.value + ((to.value - from.value) * ((time - from.time) / (to.time - from.time)));
    case TransitionType::Step:
        return from.value;
    }
    return curve.base;
}

Travel TravelAt(const Action &animation, float time)
{
    const float local = time - animation.start;
    Travel travel;
    if (const auto keys = KeysAround(animation.gadPositions, local); keys.from != nullptr)
    {
        for (std::size_t i = 0; i < 3; ++i)
        {
            const float from = keys.from->position.at(i);
            travel.position.at(i) = from + ((keys.to->position.at(i) - from) * keys.fraction);
        }
    }
    if (const auto keys = KeysAround(animation.gadOrientations, local); keys.from != nullptr)
    {
        travel.orientation = Blend(keys.from->orientation, keys.to->orientation, keys.fraction);
    }
    return travel;
}

// Notes on PlacementAt, read 2026-10-18 by lib-agent-2 (dev/CUTSCENES.md, "Root motion"): a
// clip's travel is the cut's own GadKeysPos and GadKeysOri, not the clip's channels, which are
// in place. Category 0, the tracks, sets the place absolutely (0x007D0660, 0x007CF6B0); then
// each live AANI with PlayGad adds its travel at its local time (0x007DB400 -> 0x007D92E0),
// the position turned by the turns of the clips that ended before it with ExtendGad, and the
// orientation multiplied on; then each of those ended clips adds its last key in turn
// (0x007D8610 -> 0x007D84A0, 0x007D9450). A clip with BlendGad is put on a list and resolved by
// weight (0x007D8240), which was not read, and is left out here. The turn of the actor's own
// orientation that the engine adds for the henchman's actor alone (player +0x210) is left out
// too. The order of the ended clips' own positions among themselves is the file's (inferred).
Placement PlacementAt(const Actor &actor, float time, const Stage &stage)
{
    Vector3 local = LocalPosition(actor, LastBegun(actor, kTypeMoveSpline, time), time);
    Quaternion turn = LocalTurn(actor, LastBegun(actor, kTypeRotate, time), time);
    Quaternion extended{0.0F, 0.0F, 0.0F, 1.0F};
    std::vector<const Action *> live;
    for (const Action &action : actor.actions)
    {
        if (action.type != kTypeAnimation || !action.playGad || action.blendGad || time < action.start - kAtVertex)
        {
            continue;
        }
        const bool ended = (time - action.start) > ((action.stop - action.start) - kEndedBy);
        if (ended && action.extendGad)
        {
            const Travel last = TravelAt(action, action.stop);
            local = Add(local, Rotate(extended, last.position));
            extended = Multiply(extended, last.orientation);
        }
        else if (time <= action.stop + kAtVertex)
        {
            live.push_back(&action);
        }
    }
    for (const Action *action : live)
    {
        const Travel travel = TravelAt(*action, time);
        local = Add(local, Rotate(extended, travel.position));
        turn = Multiply(turn, travel.orientation);
    }
    turn = Multiply(turn, extended);
    return {.position = Add(stage.position, Rotate(stage.orientation, local)),
            .orientation = Multiply(stage.orientation, turn)};
}

Vector3 PositionAt(const Actor &actor, const Action &track, float time, const Stage &stage)
{
    return Add(stage.position, Rotate(stage.orientation, LocalPosition(actor, track, time)));
}

Quaternion OrientationAt(const Actor &actor, const Action &track, float time, const Stage &stage)
{
    return Multiply(stage.orientation, LocalTurn(actor, track, time));
}

Cutscene CutsceneOf(const gff4::Struct &root)
{
    Cutscene cutscene{
        .area = root.String(kArea).value_or(L""),
        .runTime = root.Get<float>(kRunTime).value_or(0.0F),
        .endScript = root.String(kEndScript).value_or(L""),
        .blendTree = root.String(kBlendTree).value_or(L""),
        .fps = root.Get<std::uint32_t>(kFps).value_or(0),
        .stage = root.String(kStage).value_or(L""),
        .playUntilVoCompletes = root.Get<std::uint8_t>(kPlayUntilVoCompletes).value_or(0),
        .areaRequired = root.Get<std::uint8_t>(kAreaRequired).value_or(0),
        .henchmanTag = root.String(kHenchmanTag).value_or(L""),
        .resources = root.Strings(kResources),
        .actors = {},
    };
    const std::vector<gff4::Struct> actors = root.Structs(kActors);
    cutscene.actors.reserve(actors.size());
    for (const gff4::Struct &actor : actors)
    {
        cutscene.actors.push_back(ActorOf(actor));
    }
    return cutscene;
}

std::optional<Cutscene> CutsceneOf(const gff4::File &file)
{
    const std::optional<gff4::Struct> root = file.Root();
    if (file.Type() != "CUT" || !root)
    {
        return std::nullopt;
    }
    return CutsceneOf(*root);
}

} // namespace dao::core::cutscene
