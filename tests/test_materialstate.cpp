#include "dao/core/Erf.h"
#include "dao/core/MaterialState.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace sta = dao::core::sta;

namespace
{

using sta::DeviceState;
using sta::DeviceValue;
using sta::SamplerType;
using sta::SamplerValue;

constexpr std::uint32_t V(DeviceValue value)
{
    return static_cast<std::uint32_t>(value);
}
constexpr std::uint32_t V(SamplerValue value)
{
    return static_cast<std::uint32_t>(value);
}
constexpr std::size_t I(DeviceState state)
{
    return static_cast<std::size_t>(state);
}
constexpr std::size_t I(SamplerType type)
{
    return static_cast<std::size_t>(type);
}

// The head of a shipped file: a BOM, comments, spaces about some `=`, a stage's and a texture's samplers.
constexpr std::string_view kDefaults =
    "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!-- <DeviceState State=\"DS_ZENABLE\" Value=\"TRUE\" /> -->\n"
    "<MaterialState Name=\"DefaultState\">\n"
    "  <DeviceState State=\"DS_ZENABLE\" Value=\"ZBUFFERENABLE_TRUE\" />\n"
    "  <DeviceState State=\"DS_CULLMODE\" Value=\"CULL_NONE\" />\n"
    "  <DeviceState State=\"DS_ALPHAREF\" Value=\"8\" />\n"
    "  <SamplerState Stage=\"0\" State=\"SAMP_MINFILTER\" Value=\"TEXFILT_LINEAR\" />\n"
    "  <SamplerState Stage = \"0\" State = \"SAMP_ADDRESSU\" Value = \"TEXADD_WRAP\"/>\n"
    "  <SamplerState Stage=\"7\" State=\"SAMP_MAXANISOTROPY\" Value=\"1\" />\n"
    "  <SamplerState SamplerName=\"smpDecal\" State=\"SAMP_ADDRESSU\" Value=\"TEXADD_CLAMP\" />\n"
    "</MaterialState>\n";

constexpr std::string_view kPass =
    "<MaterialState Name=\"Pass\">\n"
    "  <DeviceState State=\"DS_CULLMODE\" Value=\"CULL_CW\" />\n"
    "  <DeviceState State=\"DS_ZWRITEENABLE\" Value=\"FALSE\" />\n"
    "  <DeviceState State=\"DS_HALFPIXELOFFSET\" Value=\"TRUE\" />\n"
    "  <SamplerState SamplerName=\"smpDecal\" State=\"SAMP_ADDRESSU\" Value=\"TEXADD_BORDER\" />\n"
    "  <SamplerState SamplerName=\"smpGone\" State=\"SAMP_ADDRESSV\" Value=\"TEXADD_MIRROR\" />\n"
    "</MaterialState>\n";

} // namespace

TEST_CASE("a state file reads its entries, the unknown and the refused dropped", "[sta]")
{
    const auto state = sta::Read(kDefaults);
    REQUIRE(state);
    CHECK(state->name == "DefaultState");
    REQUIRE(state->device.size() == 3);
    CHECK(state->device[0].state == DeviceState::ZEnable);
    CHECK(state->device[0].value == V(DeviceValue::ZBufferEnableTrue));
    CHECK(state->device[2].value == 8); // not a name: its integer
    // Two stage entries and a texture's; SAMP_MAXANISOTROPY is refused (0x004F9EB8).
    REQUIRE(state->samplers.size() == 3);
    CHECK(state->dropped == 1);
    CHECK(state->samplers[1].stage == 0);
    CHECK(state->samplers[1].type == SamplerType::AddressU);
    CHECK(state->samplers[2].texture == "smpDecal");

    CHECK_FALSE(sta::Read("<Material Name=\"x\"/>"));
    CHECK_FALSE(sta::Read(""));

    const auto pass = sta::Read(kPass);
    REQUIRE(pass);
    CHECK(pass->dropped == 1); // DS_HALFPIXELOFFSET is no state of the engine
    CHECK(pass->device.size() == 2);
}

TEST_CASE("names match whole and with case, and the codes are the engine's", "[sta]")
{
    CHECK(sta::ParseDeviceState("DS_ZENABLE") == DeviceState::ZEnable);
    CHECK(sta::ParseDeviceState("DS_BLENDOPALPHA") == DeviceState::BlendOpAlpha);
    CHECK(static_cast<int>(DeviceState::BlendOpAlpha) == 50);
    CHECK_FALSE(sta::ParseDeviceState("ds_zenable"));
    CHECK_FALSE(sta::ParseDeviceState("DS_ZENABLE "));
    CHECK_FALSE(sta::ParseSamplerType("SAMP_MAXANISOTROPY"));
    CHECK(sta::ParseSamplerType("SAMP_SRGBTEXTURE") == SamplerType::SrgbTexture);
    CHECK(static_cast<int>(SamplerType::BorderColor) == 11);

    CHECK(sta::ParseDeviceValue("ZBUFFER_USEW") == 2);
    CHECK(sta::ParseDeviceValue("BLEND_BLENDFACTOR") == 21);
    CHECK(sta::ParseDeviceValue("CULL_CCW") == 25);
    CHECK(sta::ParseDeviceValue("CMP_ALWAYS") == 33);
    CHECK(sta::ParseDeviceValue("STENCILOP_DECR") == 45);
    CHECK(sta::ParseDeviceValue("BLENDOP_MAX") == 50);
    CHECK(sta::ParseDeviceValue("TRUE") == 1);
    CHECK(sta::ParseDeviceValue("FALSE") == 0);
    CHECK(sta::ParseSamplerValue("TEXADD_MIRRORONCE") == 4);
    CHECK(sta::ParseSamplerValue("TEXFILT_LINEAR") == 7);
    CHECK(sta::ParseSamplerValue("TEXFILT_GAUSSIANQUAD") == 10);
}

TEST_CASE("a value that is no name is an integer, else a float's bits, else 0", "[sta]")
{
    CHECK(sta::ParseDeviceValue("255") == 255);
    CHECK(sta::ParseDeviceValue("0x10") == 16);
    CHECK(sta::ParseDeviceValue("1.0") == 1); // the integer comes first: 1, not the bits of 1.0f
    CHECK(sta::ParseDeviceValue("-0.5") == std::bit_cast<std::uint32_t>(-0.5F));
    CHECK(sta::ParseDeviceValue("0.00001") == std::bit_cast<std::uint32_t>(0.00001F));
    CHECK(sta::ParseDeviceValue("BLEND_ALPHA") == 0); // a name the engine does not have
    CHECK(sta::ParseDeviceValue("zbufferenable_true") == 0);
    CHECK(sta::ParseDeviceValue("") == 0);
    // The CRT's 32-bit signed parse saturates; no shipped sampler value is read this way but
    // SAMP_BORDERCOLOR's 4294967295 [read: 0x004C0990 calls the CRT's strtol-like helper, signed].
    CHECK(sta::ParseSamplerValue("4294967295") == 0x7FFFFFFFU);
}

TEST_CASE("a pass's named entries replace the defaults and the rest revert", "[sta]")
{
    const auto defaults = *sta::Read(kDefaults);
    const auto pass = *sta::Read(kPass);
    const sta::Values value = sta::Effective(defaults, pass);
    CHECK(value.device[I(DeviceState::CullMode)] == V(DeviceValue::CullCw));           // named: replaced
    CHECK(value.device[I(DeviceState::ZWriteEnable)] == 0);                            // named, but no default
    CHECK(value.device[I(DeviceState::ZEnable)] == V(DeviceValue::ZBufferEnableTrue)); // not named: default
    CHECK(value.device[I(DeviceState::AlphaRef)] == 8);
    CHECK_FALSE(value.device[I(DeviceState::FogEnable)]); // named by neither: unset

    // A later pass that names nothing gets the defaults, whatever this one set.
    const sta::State empty;
    const sta::Values next = sta::Effective(defaults, empty);
    CHECK(next.device[I(DeviceState::CullMode)] == V(DeviceValue::CullNone));
    CHECK_FALSE(next.device[I(DeviceState::ZWriteEnable)]);
    CHECK(next.sampler[0][I(SamplerType::MinFilter)] == V(SamplerValue::TexFiltLinear));
    CHECK_FALSE(next.sampler[7][8]); // the anisotropy the defaults file tries to set is refused
}

TEST_CASE("a sampler named by a texture is placed by the shader's unit, or skipped", "[sta]")
{
    const auto defaults = *sta::Read(kDefaults);
    const auto pass = *sta::Read(kPass);
    const sta::TextureUnits units = [](std::string_view texture) -> std::optional<int> {
        if (texture == "smpDecal")
        {
            return 3;
        }
        return std::nullopt; // smpGone: not in the bound shader
    };
    const sta::Values value = sta::Effective(defaults, pass, units);
    CHECK(value.sampler[3][I(SamplerType::AddressU)] == V(SamplerValue::TexAddBorder));
    CHECK_FALSE(value.sampler[3][I(SamplerType::AddressV)]);
    // The stage 0 default is not touched by it.
    CHECK(value.sampler[0][I(SamplerType::AddressU)] == V(SamplerValue::TexAddWrap));

    // With no units at all every texture-named entry is skipped, and the defaults never take one.
    const sta::Values none = sta::Effective(defaults, pass);
    CHECK_FALSE(none.sampler[3][I(SamplerType::AddressU)]);
    CHECK(sta::ValuesOf(defaults, units).sampler[3][I(SamplerType::AddressU)] == V(SamplerValue::TexAddClamp));

    // A unit past the 16 stages is left out, not written past the array.
    const sta::TextureUnits far = [](std::string_view) -> std::optional<int> { return 99; };
    CHECK_FALSE(sta::Effective(defaults, pass, far).sampler[0][I(SamplerType::AddressV)]);
}

TEST_CASE("a state's name loads <name>.xml, or DefaultState for a name the engine refuses", "[sta]")
{
    CHECK(sta::FileNameFor("Face.sta") == "Face.sta.xml");
    CHECK(sta::FileNameFor("DepthPass.sta") == "DepthPass.sta.xml");
    CHECK(sta::FileNameFor("Copy of (1)-a_b.sta") == "DefaultState.sta.xml"); // a space
    CHECK(sta::FileNameFor("a+b.sta") == "DefaultState.sta.xml");
    CHECK(sta::FileNameFor("") == ".xml");
    // 32 characters with the ".xml" is the longest; 33 falls back.
    const std::string longest = std::string(28 - 4, 'a') + ".sta"; // 24 + 4 + 4 = 32
    CHECK(sta::FileNameFor(longest).size() == 32);
    CHECK(sta::FileNameFor(longest) == longest + ".xml");
    CHECK(sta::FileNameFor("b" + longest) == "DefaultState.sta.xml");

    CHECK(sta::ValidResourceName(""));
    CHECK(sta::ValidResourceName("aZ09_-().x"));
    CHECK_FALSE(sta::ValidResourceName("a b"));
    CHECK_FALSE(sta::ValidResourceName("a/b"));
}

TEST_CASE("on the GOG install, every state file reads and DefaultState is the base", "[sta-real]")
{
    const std::filesystem::path archive =
        L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins/packages/core/data/states.erf";
    if (!std::filesystem::exists(archive))
    {
        SKIP("no GOG install here");
    }
    const auto erf = dao::core::erf::Archive::Open(archive);
    REQUIRE(erf);
    std::size_t read = 0;
    for (const auto &entry : erf->Entries())
    {
        const auto bytes = erf->Read(entry);
        REQUIRE(bytes);
        const std::string_view text(reinterpret_cast<const char *>(bytes->data()), bytes->size());
        const auto state = sta::Read(text);
        REQUIRE(state);
        ++read;
    }
    CHECK(read == 178);

    const auto defaultBytes = erf->Read(L"defaultstate.sta.xml");
    REQUIRE(defaultBytes);
    const auto defaults =
        sta::Read(std::string_view(reinterpret_cast<const char *>(defaultBytes->data()), defaultBytes->size()));
    REQUIRE(defaults);
    CHECK(defaults->device.size() == 16);
    CHECK(defaults->samplers.size() == 96); // 16 stages of 6
    CHECK(defaults->dropped == 1);          // the anisotropy of stage 7: refused, so it never takes effect

    const auto blendBytes = erf->Read(L"bloomblend.sta.xml");
    REQUIRE(blendBytes);
    const auto blend =
        sta::Read(std::string_view(reinterpret_cast<const char *>(blendBytes->data()), blendBytes->size()));
    REQUIRE(blend);
    const sta::Values value = sta::Effective(*defaults, *blend);
    CHECK(value.device[I(DeviceState::ZEnable)] == V(DeviceValue::ZBufferEnableFalse));
    CHECK(value.device[I(DeviceState::DestBlend)] == V(DeviceValue::BlendOne));
    CHECK(value.device[I(DeviceState::AlphaTestEnable)] == 0); // the default's
    CHECK(value.device[I(DeviceState::ColorWriteMask)] == 15); // the default's
    CHECK(value.sampler[0][I(SamplerType::AddressU)] == V(SamplerValue::TexAddClamp));
    CHECK(value.sampler[2][I(SamplerType::AddressU)] == V(SamplerValue::TexAddWrap)); // the default's
}

TEST_CASE("attribute names match in any case, as the engine's do", "[sta]")
{
    // cookiestate.sta.xml and seven more write `value=` on DS_FILLMODE (0x0050DDE0, __wcsicmp).
    const auto state = sta::Read(R"(<MaterialState ID="Lower">
  <DeviceState State="DS_FILLMODE" value="FILL_WIREFRAME" />
  <DeviceState STATE="DS_CULLMODE" VALUE="CULL_CW" />
  <SamplerState samplername="smpA" state="SAMP_MINFILTER" value="TEXFILT_POINT" />
  <SamplerState stage="2" State="SAMP_MAGFILTER" Value="TEXFILT_POINT" />
  <SamplerState State="SAMP_MAGFILTER" Value="TEXFILT_POINT" />
</MaterialState>)");
    REQUIRE(state);
    CHECK(state->name == "Lower"); // ID, where there is no Name
    REQUIRE(state->device.size() == 2);
    CHECK(state->device[0].value == V(DeviceValue::FillWireframe));
    CHECK(state->device[1].value == V(DeviceValue::CullCw));
    REQUIRE(state->samplers.size() == 2);
    CHECK(state->samplers[0].texture == "smpA");
    CHECK(state->samplers[1].stage == 2);
    CHECK(state->dropped == 1); // no Stage and no SamplerName: stage -1, dropped (0x004FB9CF)
}

TEST_CASE("the first MaterialState makes the state and a second is ignored", "[sta]")
{
    const auto state = sta::Read(R"(<DeviceState State="DS_ZENABLE" Value="TRUE"/>
<MaterialState Name="A"><Meta/><DeviceState State="DS_ZENABLE" Value="FALSE"/>
<MaterialState Name="B"/></MaterialState>)");
    REQUIRE(state);
    CHECK(state->name == "A");
    CHECK(state->device.size() == 1);
}
