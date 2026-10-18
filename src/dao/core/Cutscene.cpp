#include "dao/core/Cutscene.h"

#include <cstddef>
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
constexpr std::uint32_t kEffectName = 5521;
constexpr std::uint32_t kEffectParameters = 5522;
constexpr std::uint32_t kParameterName = 5523;
constexpr std::uint32_t kParameterValue = 5524;
constexpr std::uint32_t kParameterCurve = 5525;
constexpr std::uint32_t kParameterVector = 5526;
constexpr std::uint32_t kLineLipsync = 5562;
constexpr std::uint32_t kLineVoBank = 5563;
constexpr std::uint32_t kStageLookAt = 5580;
// The four actions that name another actor, each in a field of its own:
// AACC's camera and ALNK's and ACRU's targets an S32, ACHT's a U32.
constexpr std::array<std::uint32_t, 4> kOtherActor = {5610, 5620, 5630, 5640};
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
    action.tag = in.String(kTag).value_or(L"");
    action.lookAt = in.String(kStageLookAt).value_or(L"");
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

} // namespace

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
