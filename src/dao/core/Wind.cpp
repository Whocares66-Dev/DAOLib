#include "dao/core/Wind.h"

#include <cstddef>
#include <vector>

namespace dao::core::wind
{

namespace
{

constexpr std::uint32_t kGustFrequency = 3730;
constexpr std::uint32_t kStrength = 22003;
constexpr std::uint32_t kDirection = 22004;
constexpr std::uint32_t kRadiusFalloff = 22005;
constexpr std::uint32_t kGustMinStrength = 22010;
constexpr std::uint32_t kGustMaxStrength = 22011;
constexpr std::uint32_t kGustMinDuration = 22012;
constexpr std::uint32_t kGustMaxDuration = 22013;
constexpr std::uint32_t kTreeNumWindMatrices = 22020;
constexpr std::uint32_t kTreeNumLeafAngles = 22021;
constexpr std::uint32_t kTreeResponse = 22022;
constexpr std::uint32_t kTreeResponseLimit = 22023;
constexpr std::uint32_t kTreeBranchExponent = 22025;
constexpr std::uint32_t kTreeLeafExponent = 22026;
constexpr std::uint32_t kTreeBranchOscillationX = 22027;
constexpr std::uint32_t kTreeBranchOscillationY = 22028;
constexpr std::uint32_t kTreeLeafRocking = 22029;
constexpr std::uint32_t kTreeLeafRustling = 22030;
constexpr std::uint32_t kClothResponse = 22031;
constexpr std::uint32_t kClothResponseLimit = 22032;
constexpr std::uint32_t kClothStrength = 22033;
constexpr std::uint32_t kClothGustStrengthMin = 22034;
constexpr std::uint32_t kClothGustStrengthMax = 22035;
constexpr std::uint32_t kClothGustDurationMin = 22036;
constexpr std::uint32_t kClothGustDurationMax = 22037;
constexpr std::uint32_t kClothGustIntervalMin = 22038;
constexpr std::uint32_t kClothGustIntervalMax = 22039;
constexpr std::uint32_t kClothGustDirChange = 22040;
constexpr std::uint32_t kClothGustAxisRatio = 22041;

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

float One(const gff4::Struct &in, std::uint32_t label)
{
    return in.Get<float>(label).value_or(0.0F);
}

} // namespace

Wind WindOf(const gff4::Struct &root)
{
    return {
        .strength = One(root, kStrength),
        .direction = Several<3>(root, kDirection),
        .gustMinStrength = One(root, kGustMinStrength),
        .gustMaxStrength = One(root, kGustMaxStrength),
        .gustMinDuration = One(root, kGustMinDuration),
        .gustMaxDuration = One(root, kGustMaxDuration),
        .gustFrequency = One(root, kGustFrequency),
        .radiusFalloff = One(root, kRadiusFalloff),
        .treeNumWindMatrices = root.Get<std::int8_t>(kTreeNumWindMatrices).value_or(0),
        .treeNumLeafAngles = root.Get<std::int8_t>(kTreeNumLeafAngles).value_or(0),
        .treeResponse = One(root, kTreeResponse),
        .treeResponseLimit = One(root, kTreeResponseLimit),
        .treeBranchExponent = One(root, kTreeBranchExponent),
        .treeLeafExponent = One(root, kTreeLeafExponent),
        .treeBranchOscillationX = Several<4>(root, kTreeBranchOscillationX),
        .treeBranchOscillationY = Several<4>(root, kTreeBranchOscillationY),
        .treeLeafRocking = Several<4>(root, kTreeLeafRocking),
        .treeLeafRustling = Several<4>(root, kTreeLeafRustling),
        .clothResponse = One(root, kClothResponse),
        .clothResponseLimit = One(root, kClothResponseLimit),
        .clothStrength = One(root, kClothStrength),
        .clothGustStrengthMin = One(root, kClothGustStrengthMin),
        .clothGustStrengthMax = One(root, kClothGustStrengthMax),
        .clothGustDurationMin = One(root, kClothGustDurationMin),
        .clothGustDurationMax = One(root, kClothGustDurationMax),
        .clothGustIntervalMin = One(root, kClothGustIntervalMin),
        .clothGustIntervalMax = One(root, kClothGustIntervalMax),
        .clothGustDirChange = One(root, kClothGustDirChange),
        .clothGustAxisRatio = Several<3>(root, kClothGustAxisRatio),
    };
}

std::optional<Wind> WindOf(const gff4::File &file)
{
    const std::optional<gff4::Struct> root = file.Root();
    if (file.Type() != "WND" || !root)
    {
        return std::nullopt;
    }
    return WindOf(*root);
}

} // namespace dao::core::wind
