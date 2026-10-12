// The area's sky and sunlight (dao/Atmosphere.h).
//
// Read 2026-10-10 (DAOLib #62). SetAtmosphere's handler (0x00664A10) asks
// the party (0x005D57C0 on the application) for ForScript_SetAtmosphere
// (0x0062FD10): with a party that has members, the controlled creature's
// object (0x005D5CA0, 0x005D58C0), its area by its vtable's +0xA0, and
// that area's slot 2. What comes back has the area's layout at +0x658,
// and the layout has two blocks of the same shape (dao/core/Sky.h): the live one
// at +0x120, which the handler writes a float into and flags as changed
// (the block's +0x84), and the area's own at +0x2A0, which ResetAtmosphere
// (0x00664E80, 0x005CFBD0) copies over the live one (0x00538490) before
// working the scattering out again (0x0055BFA0). EA's build has the same
// offsets (0x00550770, 0x00550BE0, 0x0070D3F0).
//
// Setting goes through the script commands, one a value, so the engine
// does the writing and the flagging; only the reading is ours.
//
// The layout is EC::ChunkManager (vftable 0x00AFFE24), whose load from
// the area's file (its slot 6, 0x005D1290) also keeps the level's
// sunlight: EnvAreaSunlightColor (field 3152) at +0xB4, four floats;
// EnvAreaSunlightColorChar (3149) at +0xC4, or the colour again where the
// file has none; EnvAreaSunlightDirection (3150) at +0xE0. No command sets
// these, so SetSunlight writes them where they lie. EA's load (0x0070E980)
// has the same offsets.
//
// Seen in play 2026-10-10, in Redcliffe's day and night villages
// (the harness of #62, 7dc6606's examples/atmosphere): every sky and sunlight set read back as set and
// was in the picture taken four seconds on. The sky changes what is drawn
// above and the far haze; the sunlight the ground, the creatures, their
// shadows and where the sun's glow stands in the sky, so the renderer
// takes those fields every frame. Trees, grass and buildings followed
// neither. The day village's sunlight read 0.72 0.585 0.369 (0.64 0.52
// 0.328 on creatures) from -0.714 -0.336 -0.615, the night village's 0.27
// 0.41 0.79 from -0.0321 0.878 -0.477: unit vectors, the way the light
// travels.
//
// The presets are the game's atmosphere, clouds and fog tables, four rows
// each (default, battle, night, lightning), as core_h.nss's
// SetAtmosphericConditions, SetCloudConditions and SetFogConditions apply
// them: a value of -10000 (the atmosphere's FEarthInScatterPower and FHG
// in every row but the first) is one the row leaves out, and the scatter
// power has no parameter at all (SetAtmosphere refuses 8).
#include "dao/Atmosphere.h"

#include "dao/Addresses.h"
#include "dao/Gda.h"
#include "dao/Module.h"
#include "dao/Party.h"
#include "dao/SafeRead.h"
#include "dao/generated/ScriptApi.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>

namespace dao::atmosphere
{

namespace
{
constexpr std::size_t kLayout = 0x658;                // in what ForScript_SetAtmosphere returns
constexpr std::size_t kLive = 0x120;                  // in the layout: the block that is drawn
constexpr std::size_t kAreaOwn = 0x2A0;               // in the layout: the block Reset copies from
constexpr std::size_t kSunlightColor = 0xB4;          // in the layout: four floats
constexpr std::size_t kSunlightCharacterColor = 0xC4; // four more
constexpr std::size_t kSunlightDirection = 0xE0;      // four, aligned
constexpr float kLeftOut = -9999.0F;                  // ATM_ATMOSPHERE_INVALID_VALUE: at or below it, not a value

// The layout of the controlled creature's area, or 0 with no game or nobody controlled.
std::uintptr_t Layout()
{
    const std::uintptr_t party = party::EngineObject();
    if (party == 0 || !Module::IsKnownExecutable())
    {
        return 0;
    }
    using Fn = std::uintptr_t(__fastcall *)(std::uintptr_t);
    const std::uintptr_t area =
        reinterpret_cast<Fn>(Module::Address(addr::kScriptBackend_ForScript_SetAtmosphere))(party);
    return area != 0 ? TryRead<std::uintptr_t>(area + kLayout).value_or(0) : 0;
}

std::optional<Sky> Read(std::size_t block)
{
    const std::uintptr_t layout = Layout();
    std::array<std::byte, core::kSkyBlockSize> bytes{};
    if (layout == 0 || !TryCopy(bytes.data(), layout + block, bytes.size()))
    {
        return std::nullopt;
    }
    return core::SkyFromBlock(bytes);
}

// What each layout's sunlight was before SetSunlight wrote over it (dao/core/Sky.h). For each
// layout and not for the last one written: the game keeps the layout of an area the party has
// left, with what was written in it, and a plugin that sets the sunlight every frame writes
// the loading screen's layout and the next area's in between. With one record the first
// area's layout, come back to, read as the game's own, and a sunlight set as so many times
// the area's own grew by that much with every return (DAOLib #98: Redcliffe's 0.77 0.63 0.40
// read as 2.47, then 15.10, then 42.79).
core::OwnSunlight g_own; // the game thread's alone

struct Column
{
    float Sky::*member;
    std::string_view name;
};

// A row's columns put over a sky, those the row leaves out kept.
Sky WithRow(Sky sky, script::Table table, Preset preset, std::span<const Column> columns)
{
    for (const Column &column : columns)
    {
        const auto value = gda::GetFloat(table, static_cast<std::int32_t>(preset), column.name);
        if (value && *value > kLeftOut)
        {
            sky.*column.member = *value;
        }
    }
    return sky;
}
} // namespace

std::optional<Sky> CurrentSky()
{
    return Read(kLive);
}

std::optional<Sky> AreaOwnSky()
{
    return Read(kAreaOwn);
}

bool SetSky(const Sky &sky)
{
    if (Layout() == 0)
    {
        return false;
    }
    for (const core::SkyField &field : core::kSkyFields)
    {
        if (field.param != core::kFogColor)
        {
            script::api::SetAtmosphere(field.param, sky.*field.member);
        }
    }
    script::api::SetAtmosphereRGB(core::kFogColor, sky.fogRed, sky.fogGreen, sky.fogBlue);
    return true;
}

void ResetSky()
{
    script::api::ResetAtmosphere();
}

std::optional<Sunlight> CurrentSunlight()
{
    const std::uintptr_t layout = Layout();
    Sunlight light;
    if (layout == 0 || !TryCopy(light.color.data(), layout + kSunlightColor, sizeof light.color) ||
        !TryCopy(light.characterColor.data(), layout + kSunlightCharacterColor, sizeof light.characterColor) ||
        !TryCopy(light.direction.data(), layout + kSunlightDirection, sizeof light.direction))
    {
        return std::nullopt;
    }
    return light;
}

std::optional<Sunlight> AreaOwnSunlight()
{
    const auto now = CurrentSunlight();
    if (!now)
    {
        return std::nullopt;
    }
    return g_own.Of(Layout(), *now);
}

bool SetSunlight(const Sunlight &light)
{
    // Read first: what can be read of a live layout can be written.
    const auto now = CurrentSunlight();
    if (!now)
    {
        return false;
    }
    const std::uintptr_t layout = Layout();
    g_own.Write(layout, *now, light);
    std::memcpy(reinterpret_cast<void *>(layout + kSunlightColor), light.color.data(), sizeof light.color);
    std::memcpy(reinterpret_cast<void *>(layout + kSunlightCharacterColor), light.characterColor.data(),
                sizeof light.characterColor);
    std::memcpy(reinterpret_cast<void *>(layout + kSunlightDirection), light.direction.data(), sizeof light.direction);
    return true;
}

void ResetSunlight()
{
    if (const auto own = AreaOwnSunlight())
    {
        SetSunlight(*own);
    }
}

Sky WithAir(Sky sky, Preset preset)
{
    static constexpr std::array<Column, 13> kColumns{{
        {&Sky::sunRed, "fSunColor0"},
        {&Sky::sunGreen, "fSunColor1"},
        {&Sky::sunBlue, "fSunColor2"},
        {&Sky::sunIntensity, "fSunIntensity"},
        {&Sky::turbidity, "fTurbidity"},
        {&Sky::earthReflectance, "fEarthReflectance"},
        {&Sky::mieMultiplier, "fMieMultiplier"},
        {&Sky::rayleighMultiplier, "fRayLeighMultiplier"},
        {&Sky::distanceMultiplier, "fDistanceMultiplier"},
        {&Sky::hg, "fHg"},
        {&Sky::atmosphereAlpha, "fAtmAlpha"},
        {&Sky::moonScale, "fMoonScale"},
        {&Sky::moonAlpha, "fMoonAlpha"},
    }};
    return WithRow(sky, script::Table::Atmosphere, preset, kColumns);
}

Sky WithClouds(Sky sky, Preset preset)
{
    static constexpr std::array<Column, 8> kColumns{{
        {&Sky::cloudRed, "fCloudColor0"},
        {&Sky::cloudGreen, "fCloudColor1"},
        {&Sky::cloudBlue, "fCloudColor2"},
        {&Sky::cloudDensity, "fCloudDensity"},
        {&Sky::cloudSharpness, "fCloudSharpness"},
        {&Sky::cloudDepth, "fCloudDepth"},
        {&Sky::cloudRangeMultiplier1, "fCloudRangeMultiplier1"},
        {&Sky::cloudRangeMultiplier2, "fCloudRangeMultiplier2"},
    }};
    return WithRow(sky, script::Table::Clouds, preset, kColumns);
}

Sky WithFog(Sky sky, Preset preset)
{
    static constexpr std::array<Column, 6> kColumns{{
        {&Sky::fogRed, "fFogColor0"},
        {&Sky::fogGreen, "fFogColor1"},
        {&Sky::fogBlue, "fFogColor2"},
        {&Sky::fogIntensity, "fFogIntensity"},
        {&Sky::fogCap, "fFogCap"},
        {&Sky::fogZenith, "fFogZenith"},
    }};
    return WithRow(sky, script::Table::Fog, preset, kColumns);
}

} // namespace dao::atmosphere
