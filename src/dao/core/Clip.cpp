// dao/core/Clip.h: a clip's channels and their keys, each rotation decoded as
// the exe's three decoders do it.
#include "dao/core/Clip.h"

#include <cmath>
#include <numbers>

namespace dao::core::clip
{

namespace
{

// The fields' labels (dev/gff4_fields.csv).
constexpr std::uint32_t kNodeName = 4000;
constexpr std::uint32_t kTarget = 4001;
constexpr std::uint32_t kNodeData = 4004; // a channel's keys, behind a generic reference to a list
constexpr std::uint32_t kNodeList = 4005;
constexpr std::uint32_t kName = 4006;
constexpr std::uint32_t kLength = 4009;
constexpr std::uint32_t kAdditive = 4011;
constexpr std::uint32_t kOverride = 4012;
constexpr std::uint32_t kOverridePriority = 4013;
constexpr std::uint32_t kKeyTime = 4035;
constexpr std::uint32_t kKeyData0 = 4036;
constexpr std::uint32_t kKeyData1 = 4037;
constexpr std::uint32_t kKeyData2 = 4038;
constexpr std::uint32_t kKeyData3 = 4039;
constexpr std::uint32_t kIgnoreScale = 4040;

constexpr float kTimeSteps = 65535.0F;

// Where the largest part goes, by the place a key gives (0x00B265F0): the largest first, then the
// three in order.
constexpr std::array<std::array<int, 4>, 4> kOrder{{{0, 1, 2, 3}, {1, 0, 2, 3}, {1, 2, 0, 3}, {1, 2, 3, 0}}};

// What a step of `steps` is worth: 2 / (sqrt 2 * steps), as the exe works it out.
float Step(float steps)
{
    return 2.0F / (std::numbers::sqrt2_v<float> * steps);
}

// A rotation from its three smaller parts and the place of the largest.
Quaternion Finish(float a, float b, float c, unsigned place)
{
    const float left = 1.0F - ((c * c) + (a * a) + (b * b));
    const std::array<float, 4> parts{left > 0.0F ? std::sqrt(left) : 0.0F, a, b, c};
    const std::array<int, 4> &order = kOrder.at(place & 3U);
    return {parts.at(order[0]), parts.at(order[1]), parts.at(order[2]), parts.at(order[3])};
}

// A key's value by the channel's target; nothing for a target this does not know.
std::optional<std::array<float, 4>> ValueOf(const gff4::Struct &key, std::uint8_t target)
{
    switch (static_cast<Target>(target))
    {
    case Target::Rotation32:
        return Rotation32(key.Get<std::uint32_t>(kKeyData0).value_or(0));
    case Target::Rotation48:
        return Rotation48(key.Get<std::uint16_t>(kKeyData0).value_or(0), key.Get<std::uint16_t>(kKeyData1).value_or(0),
                          key.Get<std::uint16_t>(kKeyData2).value_or(0));
    case Target::Rotation64:
        return Rotation64(key.Get<std::uint64_t>(kKeyData0).value_or(0));
    case Target::Vector:
        return std::array<float, 4>{key.Get<float>(kKeyData0).value_or(0.0F), key.Get<float>(kKeyData1).value_or(0.0F),
                                    key.Get<float>(kKeyData2).value_or(0.0F), 0.0F};
    case Target::Float:
        return std::array<float, 4>{key.Get<float>(kKeyData0).value_or(0.0F), 0.0F, 0.0F, 0.0F};
    case Target::Colour:
        return std::array<float, 4>{key.Get<float>(kKeyData0).value_or(0.0F), key.Get<float>(kKeyData1).value_or(0.0F),
                                    key.Get<float>(kKeyData2).value_or(0.0F), key.Get<float>(kKeyData3).value_or(0.0F)};
    }
    return std::nullopt;
}

Channel ChannelOf(const gff4::Struct &node, float length)
{
    Channel channel;
    channel.name = node.String(kNodeName).value_or(L"");
    const std::size_t split = channel.name.rfind(L'_');
    channel.node = split == std::wstring::npos ? channel.name : channel.name.substr(0, split);
    channel.what = split == std::wstring::npos ? std::wstring() : channel.name.substr(split + 1);
    channel.target = node.Get<std::uint8_t>(kTarget).value_or(0);
    const std::optional<gff4::Struct> list = node.Child(kNodeData);
    if (!list)
    {
        return channel;
    }
    for (const gff4::Struct &key : list->Structs(kNodeData))
    {
        const std::optional<std::array<float, 4>> value = ValueOf(key, channel.target);
        if (!value)
        {
            break;
        }
        const float time = static_cast<float>(key.Get<std::uint16_t>(kKeyTime).value_or(0)) / kTimeSteps * length;
        channel.keys.push_back({.time = time, .value = *value});
    }
    return channel;
}

} // namespace

Quaternion Rotation32(std::uint32_t packed)
{
    const float step = Step(1023.0F);
    const auto part = [step](std::uint32_t v) { return (static_cast<float>(v) - 511.5F) * step; };
    return Finish(part(packed >> 22), part((packed >> 12) & 0x3FFU), part((packed >> 2) & 0x3FFU), packed & 3U);
}

Quaternion Rotation48(std::uint16_t first, std::uint16_t second, std::uint16_t third)
{
    const float step = Step(65535.0F);
    const auto part = [step](unsigned v) { return (static_cast<float>(v) - 32767.5F) * step; };
    return Finish(part(first & 0xFFFEU), part(second & 0xFFFEU), part(third), ((first & 1U) << 1U) | (second & 1U));
}

Quaternion Rotation64(std::uint64_t packed)
{
    const auto low = static_cast<std::uint32_t>(packed);
    const auto high = static_cast<std::uint32_t>(packed >> 32);
    const float wide = Step(2097151.0F);
    const float narrow = Step(1048575.0F);
    const float a = (static_cast<float>(high >> 11) - 1048575.5F) * wide;
    const float b = (static_cast<float>((low >> 22) | ((high & 0x7FFU) << 10)) - 1048575.5F) * wide;
    const float c = (static_cast<float>((low >> 2) & 0xFFFFFU) - 524287.5F) * narrow;
    return Finish(a, b, c, low & 3U);
}

bool Channel::IsRotation() const
{
    const auto kind = static_cast<Target>(target);
    return kind == Target::Rotation32 || kind == Target::Rotation48 || kind == Target::Rotation64;
}

std::optional<Clip> ClipOf(const gff4::File &animation)
{
    const std::optional<gff4::Struct> root = animation.Root();
    if (animation.Type() != "ANI" || !root)
    {
        return std::nullopt;
    }
    Clip clip;
    clip.name = root->String(kName).value_or(L"");
    clip.length = root->Get<float>(kLength).value_or(0.0F);
    clip.additive = root->Get<std::uint8_t>(kAdditive).value_or(0) != 0;
    clip.isOverride = root->Get<std::uint8_t>(kOverride).value_or(0) != 0;
    clip.overridePriority = root->Get<std::uint32_t>(kOverridePriority).value_or(0);
    clip.ignoreScale = root->Get<std::uint8_t>(kIgnoreScale).value_or(0) != 0;
    for (const gff4::Struct &node : root->Structs(kNodeList))
    {
        clip.channels.push_back(ChannelOf(node, clip.length));
    }
    return clip;
}

} // namespace dao::core::clip
