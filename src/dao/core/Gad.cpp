// dao/core/Gad.h: a clip's root motion, its keys sampled as the exe samples them.
#include "dao/core/Gad.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <numbers>
#include <span>
#include <string_view>

namespace dao::core::gad
{

namespace
{

// The fields' labels (dev/gff4_fields.csv).
constexpr std::uint32_t kNodeName = 4000;
constexpr std::uint32_t kNodeData = 4004;
constexpr std::uint32_t kNodeList = 4005;
constexpr std::uint32_t kName = 4007;
constexpr std::uint32_t kHasGobAnim = 4008;
constexpr std::uint32_t kLength = 4009;

constexpr std::wstring_view kTranslation = L"gob_gobtranslation";
constexpr std::wstring_view kRotation = L"gob_gobrotation";

// The slerp's own cut-off (0x004CBAA0): two keys this close are blended straight.
constexpr float kNearlyOne = 1e-6F;

// A list of floats cut into keys of a time and Width values, as the engine counts them: so many
// floats a key, a part key at the end left out (0x00A390F0, 0x00A38F70).
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

// The two keys about a time and how far between them it is, as the readers find them (0x00A38DE0,
// 0x00A38D20): the first key until its time, the last from its, and otherwise the last key at or
// before the time and the next.
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

float Dot(const Quaternion &a, const Quaternion &b)
{
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]) + (a[3] * b[3]);
}

// Two rotations' blend (0x00A38BF0): the second turned round where they are more than half a turn
// apart, then slerped (0x004CBC10 -> 0x004CBAA0), straight where they are all but one.
Quaternion Slerp(const Quaternion &a, Quaternion b, float fraction)
{
    float dot = Dot(a, b);
    if (dot < 0.0F)
    {
        for (float &part : b)
        {
            part = -part;
        }
        dot = -dot;
    }
    float first = 1.0F - fraction;
    float second = fraction;
    if (1.0F - dot > kNearlyOne)
    {
        const float sine = std::sqrt(1.0F - (dot * dot));
        const float angle = std::atan2(sine, dot);
        first = std::sin(first * angle) / sine;
        second = std::sin(second * angle) / sine;
    }
    Quaternion q{};
    for (std::size_t i = 0; i < 4; ++i)
    {
        q.at(i) = (first * a.at(i)) + (second * b.at(i));
    }
    return q;
}

// a's turn undone, then b's: the turn from a to b.
Quaternion TurnFrom(const Quaternion &a, const Quaternion &b)
{
    const auto [ax, ay, az, aw] = a;
    const auto [bx, by, bz, bw] = b;
    // a's conjugate times b.
    return {(aw * bx) - (ax * bw) - (ay * bz) + (az * by), (aw * by) + (ax * bz) - (ay * bw) - (az * bx),
            (aw * bz) - (ax * by) + (ay * bx) - (az * bw), (aw * bw) + (ax * bx) + (ay * by) + (az * bz)};
}

} // namespace

std::vector<PositionKey> PositionKeysOf(const gff4::Struct &in, std::uint32_t label)
{
    return KeysOf<3, PositionKey>(in, label, [](float time, const float *v) {
        return PositionKey{.time = time, .position = {v[0], v[1], v[2]}};
    });
}

std::vector<OrientationKey> OrientationKeysOf(const gff4::Struct &in, std::uint32_t label)
{
    return KeysOf<4, OrientationKey>(in, label, [](float time, const float *v) {
        return OrientationKey{.time = time, .orientation = {v[0], v[1], v[2], v[3]}};
    });
}

std::optional<Vector3> PositionAt(const std::vector<PositionKey> &keys, float time)
{
    const auto around = KeysAround(keys, time);
    if (around.from == nullptr)
    {
        return std::nullopt;
    }
    // The interpolator 0x00A38C60: each value weighted by the time to the other key.
    Vector3 position{};
    for (std::size_t i = 0; i < 3; ++i)
    {
        const float from = around.from->position.at(i);
        position.at(i) = from + ((around.to->position.at(i) - from) * around.fraction);
    }
    return position;
}

std::optional<Quaternion> OrientationAt(const std::vector<OrientationKey> &keys, float time)
{
    const auto around = KeysAround(keys, time);
    if (around.from == nullptr)
    {
        return std::nullopt;
    }
    return Slerp(around.from->orientation, around.to->orientation, around.fraction);
}

// Quaternion::Yaw (0x004EE310): atan2(-2 (xy - wz), 1 - 2 (x² + z²)) in degrees, the forward
// axis's heading, 57.29582 the exe's own degrees to a radian.
float YawDegrees(const Quaternion &rotation)
{
    const auto [x, y, z, w] = rotation;
    return std::atan2(-2.0F * ((x * y) - (w * z)), 1.0F - (2.0F * ((x * x) + (z * z)))) *
           (180.0F / std::numbers::pi_v<float>);
}

std::optional<Gad> GadOf(const gff4::File &file)
{
    const std::optional<gff4::Struct> root = file.Root();
    if (file.Type() != "GAD" || !root)
    {
        return std::nullopt;
    }
    Gad gad;
    gad.name = root->String(kName).value_or(L"");
    gad.length = root->Get<float>(kLength).value_or(0.0F);
    gad.hasGobAnim = root->Get<std::uint8_t>(kHasGobAnim).value_or(0) != 0;
    for (const gff4::Struct &node : root->Structs(kNodeList))
    {
        const std::wstring name = node.String(kNodeName).value_or(L"");
        if (name == kTranslation)
        {
            gad.translation = PositionKeysOf(node, kNodeData);
        }
        else if (name == kRotation)
        {
            gad.rotation = OrientationKeysOf(node, kNodeData);
        }
    }
    return gad;
}

Summary SummaryOf(const Gad &gad)
{
    Summary summary;
    summary.length = gad.length;
    summary.hasGobAnim = gad.hasGobAnim;
    if (!gad.translation.empty())
    {
        for (std::size_t i = 0; i < 3; ++i)
        {
            summary.translation.at(i) = gad.translation.back().position.at(i) - gad.translation.front().position.at(i);
        }
    }
    if (!gad.rotation.empty())
    {
        summary.yawDegrees = YawDegrees(TurnFrom(gad.rotation.front().orientation, gad.rotation.back().orientation));
    }
    return summary;
}

} // namespace dao::core::gad
