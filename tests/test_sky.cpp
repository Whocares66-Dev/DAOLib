#include "dao/core/Sky.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <set>

using Catch::Approx;
using dao::core::Blend;
using dao::core::kSkyBlockSize;
using dao::core::kSkyFields;
using dao::core::Sky;
using dao::core::SkyFromBlock;
using dao::core::Sunlight;

namespace
{
// A block whose float at each offset is the offset itself, so a field read
// from the wrong place reads as another's number.
std::array<std::byte, kSkyBlockSize> NumberedBlock()
{
    std::array<std::byte, kSkyBlockSize> block{};
    for (std::size_t offset = 0; offset + sizeof(float) <= block.size(); offset += sizeof(float))
    {
        const auto value = static_cast<float>(offset);
        std::memcpy(block.data() + offset, &value, sizeof value);
    }
    return block;
}
} // namespace

TEST_CASE("each value of a sky has its own float of the block and its own parameter")
{
    std::set<std::size_t> offsets;
    std::set<std::int32_t> params;
    for (const auto &field : kSkyFields)
    {
        CHECK(field.offset % sizeof(float) == 0);
        CHECK(field.offset + sizeof(float) <= kSkyBlockSize);
        CHECK(offsets.insert(field.offset).second);
        if (field.param != dao::core::kFogColor)
        {
            CHECK(params.insert(field.param).second);
        }
    }
    // The numbers SetAtmosphere refuses (0x00664A10's jump table): none is a field's.
    for (const std::int32_t refused : {8, 19, 20, 21})
    {
        CHECK(!params.contains(refused));
    }
}

TEST_CASE("a sky is read from the block where the engine's setters write it")
{
    const Sky sky = SkyFromBlock(NumberedBlock());
    // As SetAtmosphere's cases write them: 3 at +0x34, 22 at +0x4C, 10 at +0x50, 28 at +0x78;
    // and SetAtmosphereRGB's: 23 at +0, 20 at +0x28, 21 at +0x68.
    CHECK(sky.sunIntensity == Approx(0x34));
    CHECK(sky.atmosphereAlpha == Approx(0x4C));
    CHECK(sky.hg == Approx(0x50));
    CHECK(sky.moonAlpha == Approx(0x78));
    CHECK(sky.fogRed == Approx(0x00));
    CHECK(sky.fogBlue == Approx(0x08));
    CHECK(sky.sunRed == Approx(0x28));
    CHECK(sky.cloudGreen == Approx(0x6C));
    CHECK(sky.cloudDensity == Approx(0x54));
}

TEST_CASE("a block too short to hold a sky reads as the default one")
{
    const auto block = NumberedBlock();
    CHECK(SkyFromBlock(std::span(block).first(kSkyBlockSize - 1)) == Sky{});
}

TEST_CASE("a blend goes in a straight line from one sky to the other and no further")
{
    Sky day;
    day.sunIntensity = 20;
    day.moonAlpha = 0;
    Sky night;
    night.sunIntensity = 4;
    night.moonAlpha = 1;
    CHECK(Blend(day, night, 0.0F) == day);
    CHECK(Blend(day, night, 1.0F) == night);
    const Sky dusk = Blend(day, night, 0.25F);
    CHECK(dusk.sunIntensity == Approx(16));
    CHECK(dusk.moonAlpha == Approx(0.25));
    CHECK(Blend(day, night, 7.0F) == night);
    CHECK(Blend(day, night, -1.0F) == day);
}

TEST_CASE("sunlight blends its colours in a straight line and turns its direction, a unit vector all the way")
{
    // Redcliffe village's by day and by night, as read in play 2026-10-10.
    const Sunlight day{{0.72F, 0.585F, 0.369F, 1}, {0.64F, 0.52F, 0.328F, 1}, {-0.714F, -0.336F, -0.615F, 1}};
    const Sunlight night{{0.27F, 0.41F, 0.79F, 1}, {0.27F, 0.41F, 0.79F, 1}, {-0.0321F, 0.878F, -0.477F, 1}};
    CHECK(Blend(day, night, 0.0F).color == day.color);
    CHECK(Blend(day, night, 1.0F).color == night.color);
    CHECK(Blend(day, night, 2.0F).characterColor == night.characterColor);
    const Sunlight dusk = Blend(day, night, 0.5F);
    CHECK(dusk.color[0] == Approx(0.495F));
    CHECK(dusk.characterColor[2] == Approx(0.559F));
    CHECK(dusk.direction[3] == Approx(1));
    for (const float share : {0.0F, 0.25F, 0.5F, 0.75F, 1.0F})
    {
        const Sunlight light = Blend(day, night, share);
        CHECK(std::hypot(light.direction[0], light.direction[1], light.direction[2]) == Approx(1).margin(1e-3));
    }
}

TEST_CASE("sunlight from straight against where it was has no way round: it keeps its side of half way")
{
    const Sunlight east{{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 0, 0, 1}};
    const Sunlight west{{0, 0, 0, 1}, {0, 0, 0, 1}, {-1, 0, 0, 1}};
    CHECK(Blend(east, west, 0.5F).direction == west.direction);
    CHECK(Blend(east, west, 0.4999F).direction[0] == Approx(1));
    CHECK(Blend(east, west, 0.5F).color[0] == Approx(0.5F));
}
