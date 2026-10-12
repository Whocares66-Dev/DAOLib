// An area's sky as the engine keeps it: the sun, the air's scattering, the
// clouds, the fog and the moon, 27 floats in a block of the area's scene,
// each of which a script sets by number (SetAtmosphere's ATM_PARAM_*). Here
// because which float is which number and where in the block is a table a
// test can hold with no game (tests/test_sky.cpp), and so is a blend of two
// skies, which is how one becomes another over time.
//
// Read 2026-10-10 from SetAtmosphere's handler (0x00664A10: a jump table of
// 30, each case one float into the block and 1 into its changed flag at
// +0x84) and SetAtmosphereRGB's (0x00664D60: 20 the sun's colour, 21 the
// clouds', 23 the fog's). EA's build writes the same offsets (0x00550770).
// Numbers 8, 19 to 21 and 23 are refused by SetAtmosphere; 29 writes +0x7C,
// which no script constant names, and is left alone here. The block's
// +0x18 to +0x24 are set by no command.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <unordered_map>

namespace dao::core
{

// An area's sky: every value a script can set with SetAtmosphere or SetAtmosphereRGB.
struct Sky
{
    float sunRed = 1, sunGreen = 1, sunBlue = 1; // the sun's colour
    float sunIntensity = 1;
    float turbidity = 1;
    float earthReflectance = 1;
    float mieMultiplier = 1;      // scattering by haze
    float rayleighMultiplier = 1; // scattering by air
    float distanceMultiplier = 1;
    float hg = 0;              // the haze's Henyey-Greenstein term
    float atmosphereAlpha = 1; // how much of the sky is drawn
    float cloudRed = 1, cloudGreen = 1, cloudBlue = 1;
    float cloudDensity = 0;
    float cloudSharpness = 0;
    float cloudDepth = 0;
    float cloudRangeMultiplier1 = 1;
    float cloudRangeMultiplier2 = 1;
    float fogRed = 1, fogGreen = 1, fogBlue = 1;
    float fogIntensity = 0;
    float fogCap = 1;
    float fogZenith = 0;
    float moonScale = 1;
    float moonAlpha = 0;

    bool operator==(const Sky &) const = default;
};

// One value of a sky: its member, its offset in the engine's block, the number SetAtmosphere
// takes for it, and a name for a log.
struct SkyField
{
    float Sky::*member;
    std::size_t offset;
    std::int32_t param; // ATM_PARAM_*; the fog's colour has one for all three, kFogColor
    const char *name;
};

inline constexpr std::int32_t kFogColor = 23;        // ATM_PARAM_FOG_COLOR, which only SetAtmosphereRGB takes
inline constexpr std::size_t kSkyBlockSize = 0x7C;   // the bytes of the block the fields lie in
inline constexpr std::size_t kSkyChangedFlag = 0x84; // in the block: an int the engine's setters set to 1

inline constexpr std::array<SkyField, 27> kSkyFields{{
    {&Sky::fogRed, 0x00, kFogColor, "fog red"},
    {&Sky::fogGreen, 0x04, kFogColor, "fog green"},
    {&Sky::fogBlue, 0x08, kFogColor, "fog blue"},
    {&Sky::fogIntensity, 0x0C, 24, "fog intensity"},
    {&Sky::fogCap, 0x10, 25, "fog cap"},
    {&Sky::fogZenith, 0x14, 26, "fog zenith"},
    {&Sky::sunRed, 0x28, 0, "sun red"},
    {&Sky::sunGreen, 0x2C, 1, "sun green"},
    {&Sky::sunBlue, 0x30, 2, "sun blue"},
    {&Sky::sunIntensity, 0x34, 3, "sun intensity"},
    {&Sky::turbidity, 0x38, 4, "turbidity"},
    {&Sky::earthReflectance, 0x3C, 5, "earth reflectance"},
    {&Sky::mieMultiplier, 0x40, 6, "mie multiplier"},
    {&Sky::rayleighMultiplier, 0x44, 7, "rayleigh multiplier"},
    {&Sky::distanceMultiplier, 0x48, 9, "distance multiplier"},
    {&Sky::atmosphereAlpha, 0x4C, 22, "atmosphere alpha"},
    {&Sky::hg, 0x50, 10, "hg"},
    {&Sky::cloudDensity, 0x54, 14, "cloud density"},
    {&Sky::cloudSharpness, 0x58, 15, "cloud sharpness"},
    {&Sky::cloudDepth, 0x5C, 16, "cloud depth"},
    {&Sky::cloudRangeMultiplier1, 0x60, 17, "cloud range multiplier 1"},
    {&Sky::cloudRangeMultiplier2, 0x64, 18, "cloud range multiplier 2"},
    {&Sky::cloudRed, 0x68, 11, "cloud red"},
    {&Sky::cloudGreen, 0x6C, 12, "cloud green"},
    {&Sky::cloudBlue, 0x70, 13, "cloud blue"},
    {&Sky::moonScale, 0x74, 27, "moon scale"},
    {&Sky::moonAlpha, 0x78, 28, "moon alpha"},
}};

// The sky in a copy of the engine's block, which must hold kSkyBlockSize bytes; a default Sky
// from a shorter one.
inline Sky SkyFromBlock(std::span<const std::byte> block)
{
    Sky sky;
    if (block.size() < kSkyBlockSize)
    {
        return sky;
    }
    for (const SkyField &field : kSkyFields)
    {
        std::memcpy(&(sky.*field.member), block.data() + field.offset, sizeof(float));
    }
    return sky;
}

// The level's sunlight: the light an area's sun gives its ground and its creatures, and the
// way their shadows fall.
struct Sunlight
{
    std::array<float, 4> color{};          // red, green and blue, and a fourth the area's file gives (1)
    std::array<float, 4> characterColor{}; // the same, for creatures
    std::array<float, 4> direction{};      // the way the light travels, a unit vector, and a fourth (1)

    bool operator==(const Sunlight &) const = default;
};

// The sunlight each layout had before it was written over. A layout keeps its own sunlight
// nowhere but in the fields a write goes over, and the game keeps a layout, the written
// values in it, for as long as its area stays loaded: so what a layout held before the first
// write is remembered for each layout, by its address. A layout still holds a write while it
// reads back as last written there. One that reads back as anything else is the game's again
// (a new area's at the same address comes with its file's values) and is forgotten.
class OwnSunlight
{
  public:
    // Returns the layout's own sunlight, given what it holds now.
    [[nodiscard]] Sunlight Of(std::uintptr_t layout, const Sunlight &now)
    {
        const auto kept = kept_.find(layout);
        if (kept == kept_.end())
        {
            return now;
        }
        if (kept->second.written == now)
        {
            return kept->second.own;
        }
        kept_.erase(kept);
        return now;
    }

    // Records that `written` is going over a layout that holds `now`.
    void Write(std::uintptr_t layout, const Sunlight &now, const Sunlight &written)
    {
        const Sunlight own = Of(layout, now);
        kept_[layout] = {own, written};
    }

  private:
    struct Kept
    {
        Sunlight own;
        Sunlight written;
    };
    std::unordered_map<std::uintptr_t, Kept> kept_;
};

// The sky `share` of the way from one to another, every value on a straight line: 0 is `from`,
// 1 is `to`, and a share outside them is held to the nearer.
inline Sky Blend(const Sky &from, const Sky &to, float share)
{
    const float t = std::clamp(share, 0.0F, 1.0F);
    Sky sky;
    for (const SkyField &field : kSkyFields)
    {
        sky.*field.member = std::lerp(from.*field.member, to.*field.member, t);
    }
    return sky;
}

// The sunlight `share` of the way from one to another, as Blend does a sky: the colours on a
// straight line, and the direction turned from one to the other, a unit vector all the way.
// Two directions straight against each other have no way round between them: the light then
// keeps `from`'s until half way and has `to`'s after.
inline Sunlight Blend(const Sunlight &from, const Sunlight &to, float share)
{
    const float t = std::clamp(share, 0.0F, 1.0F);
    Sunlight light;
    for (std::size_t i = 0; i < 4; ++i)
    {
        light.color[i] = std::lerp(from.color[i], to.color[i], t);
        light.characterColor[i] = std::lerp(from.characterColor[i], to.characterColor[i], t);
        light.direction[i] = std::lerp(from.direction[i], to.direction[i], t);
    }
    const float length = std::hypot(light.direction[0], light.direction[1], light.direction[2]);
    if (length < 1e-3F)
    {
        light.direction = t < 0.5F ? from.direction : to.direction;
        return light;
    }
    for (std::size_t i = 0; i < 3; ++i)
    {
        light.direction[i] /= length;
    }
    return light;
}

} // namespace dao::core
