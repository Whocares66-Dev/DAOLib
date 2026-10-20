// dao/core/FaceFx.h: the archive's primitives over a bounds-checked cursor,
// then each object in the order the exe's serializers write it.
#include "dao/core/FaceFx.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <unordered_map>

namespace dao::core::facefx
{

namespace
{

constexpr std::uint32_t kArchiveVersion = 1710;

// A reader that goes bad at the first read past the end, and stays bad.
class Cursor
{
  public:
    explicit Cursor(std::span<const std::byte> bytes) : bytes_(bytes)
    {
    }

    bool Ok() const
    {
        return ok_;
    }
    bool AtEnd() const
    {
        return pos_ == bytes_.size();
    }
    std::size_t Remaining() const
    {
        return bytes_.size() - pos_;
    }
    void Fail()
    {
        ok_ = false;
    }

    // Takes the magic, which every file starts with.
    bool Magic()
    {
        if (bytes_.size() < 4 || std::memcmp(bytes_.data(), "BIOE", 4) != 0)
        {
            ok_ = false;
            return false;
        }
        pos_ = 4;
        return true;
    }

    template <typename T> T Pod()
    {
        T v{};
        if (!ok_ || Remaining() < sizeof(T))
        {
            ok_ = false;
            return v;
        }
        std::memcpy(&v, bytes_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return v;
    }
    std::uint32_t U32()
    {
        return Pod<std::uint32_t>();
    }
    std::uint16_t U16()
    {
        return Pod<std::uint16_t>();
    }
    float F32()
    {
        return Pod<float>();
    }

    // Returns an array's count, failing if the bytes left cannot hold that many elements of at least this size.
    std::size_t Count(std::size_t minElementSize)
    {
        const auto n = U32();
        if (ok_ && n > Remaining() / minElementSize)
        {
            ok_ = false;
        }
        return ok_ ? n : 0;
    }

    std::string String()
    {
        const auto n = U32();
        if (!ok_ || n > Remaining())
        {
            ok_ = false;
            return {};
        }
        std::string s(reinterpret_cast<const char *>(bytes_.data() + pos_), n);
        pos_ += n;
        return s;
    }

    template <std::size_t N> std::array<float, N> Floats()
    {
        std::array<float, N> a{};
        for (auto &f : a)
        {
            f = F32();
        }
        return a;
    }

  private:
    std::span<const std::byte> bytes_;
    std::size_t pos_ = 0;
    bool ok_ = true;
};

bool ReadHeader(Cursor &c, Header &h)
{
    if (!c.Magic())
    {
        return false;
    }
    h.archiveVersion = c.U32();
    if (h.archiveVersion != kArchiveVersion)
    {
        return false;
    }
    h.licensee = c.String();
    h.licenseeProject = c.String();
    h.licenseeVersion = c.U32();
    h.unknownWord = c.U16();
    const auto buckets = c.Count(8);
    for (std::size_t i = 0; i < buckets && c.Ok(); ++i)
    {
        c.U32(); // the bucket's index, a hash of the class name
        const auto entries = c.Count(6);
        for (std::size_t j = 0; j < entries && c.Ok(); ++j)
        {
            auto name = c.String();
            h.classVersions.emplace_back(std::move(name), c.U16());
        }
    }
    const auto names = c.Count(4);
    h.names.reserve(names);
    for (std::size_t i = 0; i < names && c.Ok(); ++i)
    {
        h.names.push_back(c.String());
    }
    return c.Ok();
}

bool HasVersions(const Header &h, std::initializer_list<std::pair<const char *, std::uint16_t>> wanted)
{
    return std::ranges::all_of(wanted, [&](const auto &w) { return h.ClassVersion(w.first) == w.second; });
}

// An FxName: an index into the name table.
std::string Name(Cursor &c, const Header &h)
{
    const auto i = c.U32();
    if (!c.Ok() || i >= h.names.size())
    {
        c.Fail();
        return {};
    }
    return h.names[i];
}

// The keys are one pool for all curves, and the counts say how it is cut.
void CutKeys(Cursor &c, Anim &a, const std::vector<AnimKey> &pool)
{
    const auto counts = c.Count(4);
    if (counts != a.curves.size())
    {
        c.Fail();
    }
    std::size_t next = 0;
    for (std::size_t i = 0; i < counts && c.Ok(); ++i)
    {
        const auto n = c.U32();
        if (n > pool.size() - next)
        {
            c.Fail();
            break;
        }
        a.curves[i].keys.assign(pool.begin() + static_cast<std::ptrdiff_t>(next),
                                pool.begin() + static_cast<std::ptrdiff_t>(next + n));
        next += n;
    }
    if (next != pool.size())
    {
        c.Fail();
    }
}

Anim ReadAnim(Cursor &c, const Header &h)
{
    Anim a;
    a.name = Name(c, h);
    const auto curves = c.Count(8);
    a.curves.resize(curves);
    for (auto &curve : a.curves)
    {
        curve.nodeName = Name(c, h);
        curve.interpolation = c.U32();
    }
    const auto totalKeys = c.Count(16);
    std::vector<AnimKey> pool(totalKeys);
    for (auto &k : pool)
    {
        k.time = c.F32();
        k.value = c.F32();
        k.slopeIn = c.F32();
        k.slopeOut = c.F32();
    }
    CutKeys(c, a, pool);
    a.blendIn = c.F32();
    a.blendOut = c.F32();
    const auto weights = c.Count(8);
    a.boneWeights.resize(weights);
    for (auto &w : a.boneWeights)
    {
        w.bone = Name(c, h);
        w.weight = c.F32();
    }
    a.reserved1 = c.String();
    a.reserved2 = c.String();
    a.tail = c.U32();
    return a;
}

AnimGroup ReadGroup(Cursor &c, const Header &h)
{
    AnimGroup g;
    g.name = Name(c, h);
    const auto anims = c.Count(4);
    for (std::size_t i = 0; i < anims && c.Ok(); ++i)
    {
        g.anims.push_back(ReadAnim(c, h));
    }
    return g;
}

Bone ReadBone(Cursor &c, const Header &h)
{
    Bone b;
    b.name = Name(c, h);
    b.position = c.Floats<3>();
    b.rotation = c.Floats<4>();
    b.scale = c.Floats<3>();
    return b;
}

MasterBone ReadMasterBone(Cursor &c, const Header &h)
{
    MasterBone m;
    m.reference = ReadBone(c, h);
    m.conjugate = c.Floats<4>();
    m.unknownIndex = c.U32();
    m.unknownFloat = c.F32();
    const auto poses = c.Count(4 + 4 + 40);
    m.poses.resize(poses);
    for (auto &p : m.poses)
    {
        p.nodeIndex = c.U32();
        p.pose = ReadBone(c, h);
    }
    return m;
}

FaceGraphUserProperty ReadUserProperty(Cursor &c, const Header &h)
{
    FaceGraphUserProperty p;
    p.name = Name(c, h);
    p.a = c.U32();
    p.b = c.U32();
    p.c = c.F32();
    const auto strings = c.Count(4);
    p.strings.reserve(strings);
    for (std::size_t i = 0; i < strings && c.Ok(); ++i)
    {
        p.strings.push_back(c.String());
    }
    return p;
}

FaceGraphNode ReadNode(Cursor &c, const Header &h)
{
    FaceGraphNode n;
    n.type = c.U32();
    n.name = Name(c, h);
    n.min = c.F32();
    n.minReciprocal = c.F32();
    n.max = c.F32();
    n.maxReciprocal = c.F32();
    n.inputOperation = c.U32();
    const auto links = c.Count(12);
    n.links.resize(links);
    for (auto &l : n.links)
    {
        l.inputNode = c.U32();
        l.functionId = c.U32();
        const auto params = c.Count(4);
        l.parameters.resize(params);
        for (auto &p : l.parameters)
        {
            p = c.F32();
        }
    }
    const auto props = c.Count(20);
    for (std::size_t i = 0; i < props && c.Ok(); ++i)
    {
        n.userProperties.push_back(ReadUserProperty(c, h));
    }
    return n;
}

// The name-to-index map, dumped by bucket.
void ReadNodeMap(Cursor &c, Actor &a)
{
    const auto buckets = c.Count(8);
    for (std::size_t i = 0; i < buckets && c.Ok(); ++i)
    {
        c.U32(); // the bucket
        const auto n = c.Count(8);
        for (std::size_t j = 0; j < n && c.Ok(); ++j)
        {
            auto name = c.String();
            const auto index = c.U32();
            if (index >= a.nodes.size())
            {
                c.Fail();
            }
            a.nodeIndexByName.emplace_back(std::move(name), index);
        }
    }
}

void ReadPhonemes(Cursor &c, Actor &a)
{
    const auto phonemes = c.Count(12);
    a.phonemeMap.resize(phonemes);
    for (auto &p : a.phonemeMap)
    {
        p.phoneme = c.U32();
        p.node = Name(c, a.header);
        p.weight = c.F32();
    }
    const auto names = c.Count(4);
    for (std::size_t i = 0; i < names && c.Ok(); ++i)
    {
        a.phonemeNames.push_back(Name(c, a.header));
    }
}

constexpr float kEpsilon = std::numeric_limits<float>::epsilon(); // 1.19209e-7, the exe's 0xB2B574
constexpr std::uint32_t kCorrective = 9;
constexpr std::uint32_t kMaterialTarget = 8;

// A parameter, or its default when the link has fewer.
float Param(std::span<const float> p, std::size_t i, float fallback)
{
    return i < p.size() ? p[i] : fallback;
}

// The corrective link's step: the accumulator grows by the factor times the input's activity as a part of its range.
float Corrected(float corrective, const FaceGraphNode &input, float inputValue, float factor)
{
    const float activity = std::max(input.maxReciprocal * inputValue, -input.minReciprocal * inputValue);
    return std::min(1.0f, corrective + (factor * activity));
}

// The accumulator a node's links start from: the identity of its operation, and 0 when it has no links.
float InitialAccumulator(const FaceGraphNode &node)
{
    if (node.links.empty() || node.inputOperation == 0)
    {
        return 0.0f;
    }
    return 1.0f;
}

// One node: its links combined, the curve's value added, attenuated by the correctives, clamped.
float NodeValue(const FaceGraphNode &node, std::span<const FaceGraphNode> nodes, std::span<const float> values,
                float pending)
{
    float acc = InitialAccumulator(node);
    float corrective = 0;
    for (const auto &link : node.links)
    {
        if (link.inputNode >= nodes.size())
        {
            continue;
        }
        const float x = values[link.inputNode];
        if (link.functionId == kCorrective)
        {
            corrective = Corrected(corrective, nodes[link.inputNode], x, Param(link.parameters, 0, 1.0f));
            continue;
        }
        const float y = LinkFunction(link.functionId, link.parameters, x);
        acc = node.inputOperation == 0 ? acc + y : acc * y;
    }
    const float v = (acc + pending) * (1.0f - corrective);
    return std::max(std::min(v, node.max), node.min);
}

using Quat = std::array<float, 4>; // w, x, y, z

Quat Multiply(const Quat &a, const Quat &b)
{
    const float w = ((a[0] * b[0]) - (a[1] * b[1])) - ((a[2] * b[2]) + (a[3] * b[3]));
    const float x = ((a[0] * b[1]) + (a[1] * b[0])) + ((a[2] * b[3]) - (a[3] * b[2]));
    const float y = ((a[0] * b[2]) - (a[1] * b[3])) + ((a[2] * b[0]) + (a[3] * b[1]));
    const float z = ((a[0] * b[3]) + (a[1] * b[2])) - ((a[2] * b[1]) - (a[3] * b[0]));
    return {w, x, y, z};
}

float Dot(const Quat &a, const Quat &b)
{
    return ((a[0] * b[0]) + (a[1] * b[1])) + ((a[2] * b[2]) + (a[3] * b[3]));
}

// Returns a * from + b * to.
Quat Mix(float a, const Quat &from, float b, const Quat &to)
{
    return {(a * from[0]) + (b * to[0]), (a * from[1]) + (b * to[1]), (a * from[2]) + (b * to[2]),
            (a * from[3]) + (b * to[3])};
}

// The reference turned toward a pose by v along the short way: a plain lerp when they are near (dot over 0.55, as the
// exe), a slerp otherwise.
Quat Blend(const Quat &from, Quat to, float v)
{
    float dot = Dot(from, to);
    if (dot < 0)
    {
        for (auto &c : to)
        {
            c = -c;
        }
        dot = -dot;
    }
    float a = 1.0f - v;
    float b = v;
    if (dot <= 0.55f)
    {
        const float angle = std::acos(std::min(dot, 1.0f));
        const float sine = std::sin(angle);
        if (sine > kEpsilon)
        {
            a = std::sin((1.0f - v) * angle) / sine;
            b = std::sin(v * angle) / sine;
        }
    }
    return Mix(a, from, b, to);
}

// A bone's turn from its reference is the product, in the poses' order with each new factor on the right, of
// Blend(reference, pose, v) times the reference's conjugate, starting from identity and normalised once at the end
// (0x0084FAD2 to 0x0085040A; lib-agent-2's reading, 2026-10-19, run through an x87 emulator on 40 random bones against
// this formula, within 1.2e-7). The poses' positions and scales are deltas from the reference. The engine hands the
// node the turn as {x, z, y, -w} and the position's delta over 100 as (x, z, y).
BoneTransform BoneAt(const MasterBone &master, std::span<const float> values)
{
    Quat rotation = {1, 0, 0, 0};
    BoneTransform out;
    out.position = master.reference.position;
    out.scale = master.reference.scale;
    for (const auto &pose : master.poses)
    {
        const float v = pose.nodeIndex < values.size() ? values[pose.nodeIndex] : 0.0f;
        if (std::fabs(v) < kEpsilon)
        {
            continue;
        }
        for (std::size_t i = 0; i < 3; ++i)
        {
            out.position[i] += v * pose.pose.position[i];
            out.scale[i] += v * pose.pose.scale[i];
        }
        rotation =
            Multiply(rotation, Multiply(Blend(master.reference.rotation, pose.pose.rotation, v), master.conjugate));
    }
    const float length = std::sqrt(Dot(rotation, rotation));
    if (length < kEpsilon)
    {
        rotation = {1, 0, 0, 0};
    }
    else
    {
        for (auto &c : rotation)
        {
            c /= length;
        }
    }
    out.rotationDelta = rotation;
    out.rotation = Multiply(rotation, master.reference.rotation);
    out.nodeRotation = {rotation[1], rotation[3], rotation[2], -rotation[0]};
    out.positionDelta = {(out.position[0] - master.reference.position[0]) / 100,
                         (out.position[2] - master.reference.position[2]) / 100,
                         (out.position[1] - master.reference.position[1]) / 100};
    return out;
}

} // namespace

float LinkFunction(std::uint32_t functionId, std::span<const float> p, float x)
{
    switch (functionId)
    {
    case 1: // Linear: m * x + b
        if (p.size() == 2)
        {
            return (p[0] * x) + p[1];
        }
        return p.size() == 1 ? p[0] * x : x;
    case 2: // Quadratic: a * x * |x|
        return Param(p, 0, 1.0f) * (x * std::fabs(x));
    case 3: // Cubic: a * x^3
        return Param(p, 0, 1.0f) * (x * x * x);
    case 4: // Sqrt: a * sign(x) * sqrt(|x|)
        return x == 0 ? 0.0f : Param(p, 0, 1.0f) * std::copysign(std::sqrt(std::fabs(x)), x);
    case 5: // Negate
        return -x;
    case 6: // Inverse
        return std::fabs(x) < kEpsilon ? 0.0f : 1.0f / x;
    case 7: // OneClamp
        return x <= 1.0f ? 1.0f : 1.0f / x;
    case 8: // Constant
        return Param(p, 0, 1.0f);
    case 10: // ClampedLinear: m, clampx, clampy, clampdir
        if (p.size() == 4)
        {
            const bool beyond = p[3] > 0 ? x > p[1] : x < p[1];
            return beyond ? (p[0] * (x - p[1])) + p[2] : p[2];
        }
        if (p.size() == 1)
        {
            return x > 0 ? p[0] * x : 0.0f;
        }
        return x;
    default: // 0 Null; 9 Corrective is the node's; above 10 unknown
        return 0.0f;
    }
}

std::vector<float> EvaluateGraph(const Actor &actor, const Anim &anim, float time)
{
    std::unordered_map<std::string_view, std::size_t> byName;
    byName.reserve(actor.nodes.size());
    for (std::size_t i = 0; i < actor.nodes.size(); ++i)
    {
        byName.emplace(actor.nodes[i].name, i);
    }
    std::vector<float> pending(actor.nodes.size(), 0.0f);
    for (const auto &curve : anim.curves)
    {
        const auto it = byName.find(curve.nodeName);
        if (it != byName.end())
        {
            pending[it->second] = curve.Evaluate(time);
        }
    }
    std::vector<float> values(actor.nodes.size(), 0.0f);
    for (std::size_t i = 0; i < actor.nodes.size(); ++i)
    {
        values[i] = NodeValue(actor.nodes[i], actor.nodes, values, pending[i]);
    }
    return values;
}

std::vector<BoneTransform> EvaluateBones(const Actor &actor, std::span<const float> nodeValues)
{
    std::vector<BoneTransform> out;
    out.reserve(actor.bones.size());
    for (const auto &bone : actor.bones)
    {
        out.push_back(BoneAt(bone, nodeValues));
    }
    return out;
}

FaceState Evaluate(const Actor &actor, const Anim &anim, float time)
{
    FaceState state;
    state.nodeValues = EvaluateGraph(actor, anim, time);
    state.bones = EvaluateBones(actor, state.nodeValues);
    for (std::size_t i = 0; i < actor.nodes.size(); ++i)
    {
        if (actor.nodes[i].type != kMaterialTarget)
        {
            continue;
        }
        for (const auto &property : actor.nodes[i].userProperties)
        {
            if (property.name == "Material parameter index" && property.b < state.emotionWeights.size())
            {
                state.emotionWeights[property.b] = state.nodeValues[i];
            }
        }
    }
    return state;
}

std::optional<std::uint16_t> Header::ClassVersion(const std::string &className) const
{
    const auto it = std::ranges::find(classVersions, className, &std::pair<std::string, std::uint16_t>::first);
    if (it == classVersions.end())
    {
        return std::nullopt;
    }
    return it->second;
}

float AnimCurve::Evaluate(float time) const
{
    if (keys.empty())
    {
        return 0;
    }
    if (time <= keys.front().time || keys.size() == 1)
    {
        return keys.front().value;
    }
    if (time >= keys.back().time)
    {
        return keys.back().value;
    }
    // The first key past the time, so the one before it has time[i] <= t < time[i+1].
    const auto after = std::ranges::upper_bound(keys, time, {}, &AnimKey::time);
    const auto &k0 = *(after - 1);
    const auto &k1 = *after;
    const float dt = k1.time - k0.time;
    if (!(dt > 0))
    {
        return k0.value;
    }
    const float s = (time - k0.time) / dt;
    const float d = k1.value - k0.value;
    if (interpolation == static_cast<std::uint32_t>(Interpolation::Linear))
    {
        return k0.value + (s * d);
    }
    const float m0 = k0.slopeOut * dt;
    const float m1 = k1.slopeIn * dt;
    const float cubic = (m0 + m1) - (2 * d);
    const float quadratic = ((3 * d) - (2 * m0)) - m1;
    return k0.value + (s * (m0 + (s * (quadratic + (s * cubic)))));
}

float Anim::EndTime() const
{
    float end = 0;
    for (const auto &c : curves)
    {
        if (!c.keys.empty())
        {
            end = std::max(end, c.keys.back().time);
        }
    }
    return end;
}

std::optional<std::uint32_t> Actor::NodeIndex(const std::string &nodeName) const
{
    const auto it = std::ranges::find(nodeIndexByName, nodeName, &std::pair<std::string, std::uint32_t>::first);
    if (it == nodeIndexByName.end())
    {
        return std::nullopt;
    }
    return it->second;
}

std::optional<AnimSet> ReadAnimSet(std::span<const std::byte> bytes)
{
    Cursor c(bytes);
    AnimSet set;
    if (!ReadHeader(c, set.header) ||
        !HasVersions(set.header,
                     {{"FxAnimSet", 0}, {"FxAnim", 6}, {"FxAnimCurve", 1}, {"FxAnimGroup", 0}, {"FxName", 1}}))
    {
        return std::nullopt;
    }
    set.name = Name(c, set.header);
    set.secondSlot = c.U32();
    const auto groups = c.Count(4);
    for (std::size_t i = 0; i < groups && c.Ok(); ++i)
    {
        set.groups.push_back(ReadGroup(c, set.header));
    }
    if (!c.Ok() || !c.AtEnd())
    {
        return std::nullopt;
    }
    return set;
}

std::optional<Actor> ReadActor(std::span<const std::byte> bytes)
{
    Cursor c(bytes);
    Actor a;
    if (!ReadHeader(c, a.header) || !HasVersions(a.header, {{"FxActor", 3},
                                                            {"FxMasterBoneList", 2},
                                                            {"FxBone", 0},
                                                            {"FxCompiledFaceGraph", 0},
                                                            {"FxAnimGroup", 0},
                                                            {"FxName", 1},
                                                            {"FxLinkFnParameters", 1}}))
    {
        return std::nullopt;
    }
    a.name = Name(c, a.header);
    a.secondSlot = c.U32();
    const auto bones = c.Count(4);
    for (std::size_t i = 0; i < bones && c.Ok(); ++i)
    {
        a.bones.push_back(ReadMasterBone(c, a.header));
    }
    const auto nodes = c.Count(4);
    for (std::size_t i = 0; i < nodes && c.Ok(); ++i)
    {
        a.nodes.push_back(ReadNode(c, a.header));
    }
    ReadNodeMap(c, a);
    if (c.Ok())
    {
        a.defaultGroup = ReadGroup(c, a.header);
    }
    const auto groups = c.Count(4);
    for (std::size_t i = 0; i < groups && c.Ok(); ++i)
    {
        a.groups.push_back(ReadGroup(c, a.header));
    }
    ReadPhonemes(c, a);
    if (!c.Ok() || !c.AtEnd())
    {
        return std::nullopt;
    }
    return a;
}

} // namespace dao::core::facefx
