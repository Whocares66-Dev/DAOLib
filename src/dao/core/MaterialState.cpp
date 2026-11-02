// dao/core/MaterialState.h: a .sta.xml's elements read one tag at a time (dao/core/XmlTags.h), and the
// engine's name tables, copied out of the GOG build (a script of 2026-11-01 over the compare chains of
// 0x004F93B0, 0x004F98B0, 0x004F9DE0 and 0x004F9F10: each name is a wide literal compared whole and
// with case by 0x004C0E10, which ends in a plain wchar compare at 0x00413800, then a code).
#include "dao/core/MaterialState.h"

#include "dao/core/XmlTags.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <utility>

namespace dao::core::sta
{

namespace
{

template <class Code> struct Name
{
    std::string_view text;
    Code code;
};

constexpr Name<DeviceState> kDeviceStateNames[] = {
    {"DS_ZENABLE", DeviceState::ZEnable},
    {"DS_FILLMODE", DeviceState::FillMode},
    {"DS_SHADEMODE", DeviceState::ShadeMode},
    {"DS_ZWRITEENABLE", DeviceState::ZWriteEnable},
    {"DS_ALPHATESTENABLE", DeviceState::AlphaTestEnable},
    {"DS_SRCBLEND", DeviceState::SrcBlend},
    {"DS_DESTBLEND", DeviceState::DestBlend},
    {"DS_CULLMODE", DeviceState::CullMode},
    {"DS_ZFUNC", DeviceState::ZFunc},
    {"DS_ALPHAREF", DeviceState::AlphaRef},
    {"DS_ALPHAFUNC", DeviceState::AlphaFunc},
    {"DS_ALPHABLENDENABLE", DeviceState::AlphaBlendEnable},
    {"DS_FOGENABLE", DeviceState::FogEnable},
    {"DS_FOGCOLOR", DeviceState::FogColor},
    {"DS_FOGTABLEMODE", DeviceState::FogTableMode},
    {"DS_FOGSTART", DeviceState::FogStart},
    {"DS_FOGEND", DeviceState::FogEnd},
    {"DS_FOGDENSITY", DeviceState::FogDensity},
    {"DS_RANGEFOGENABLE", DeviceState::RangeFogEnable},
    {"DS_STENCILENABLE", DeviceState::StencilEnable},
    {"DS_STENCILFAIL", DeviceState::StencilFail},
    {"DS_STENCILZFAIL", DeviceState::StencilZFail},
    {"DS_STENCILPASS", DeviceState::StencilPass},
    {"DS_STENCILFUNC", DeviceState::StencilFunc},
    {"DS_STENCILREF", DeviceState::StencilRef},
    {"DS_STENCILMASK", DeviceState::StencilMask},
    {"DS_STENCILWRITEMASK", DeviceState::StencilWriteMask},
    {"DS_CLIPPING", DeviceState::Clipping},
    {"DS_FOGVERTEXMODE", DeviceState::FogVertexMode},
    {"DS_CLIPPLANEENABLE", DeviceState::ClipPlaneEnable},
    {"DS_MULTISAMPLEANTIALIAS", DeviceState::MultisampleAntialias},
    {"DS_MULTISAMPLEMASK", DeviceState::MultisampleMask},
    {"DS_COLORWRITEMASK", DeviceState::ColorWriteMask},
    {"DS_BLENDOP", DeviceState::BlendOp},
    {"DS_SCISSORTESTENABLE", DeviceState::ScissorTestEnable},
    {"DS_SLOPESCALEDEPTHBIAS", DeviceState::SlopeScaleDepthBias},
    {"DS_TWOSIDEDSTENCILMODE", DeviceState::TwoSidedStencilMode},
    {"DS_CCW_STENCILFAIL", DeviceState::CcwStencilFail},
    {"DS_CCW_STENCILZFAIL", DeviceState::CcwStencilZFail},
    {"DS_CCW_STENCILPASS", DeviceState::CcwStencilPass},
    {"DS_CCW_STENCILFUNC", DeviceState::CcwStencilFunc},
    {"DS_COLORWRITEMASK1", DeviceState::ColorWriteMask1},
    {"DS_COLORWRITEMASK2", DeviceState::ColorWriteMask2},
    {"DS_COLORWRITEMASK3", DeviceState::ColorWriteMask3},
    {"DS_BLENDFACTOR", DeviceState::BlendFactor},
    {"DS_SRGBWRITEENABLE", DeviceState::SrgbWriteEnable},
    {"DS_DEPTHBIAS", DeviceState::DepthBias},
    {"DS_SEPARATEALPHABLENDENABLE", DeviceState::SeparateAlphaBlendEnable},
    {"DS_SRCBLENDALPHA", DeviceState::SrcBlendAlpha},
    {"DS_DESTBLENDALPHA", DeviceState::DestBlendAlpha},
    {"DS_BLENDOPALPHA", DeviceState::BlendOpAlpha},
};

constexpr Name<DeviceValue> kDeviceValueNames[] = {
    {"ZBUFFERENABLE_FALSE", DeviceValue::ZBufferEnableFalse},
    {"ZBUFFERENABLE_TRUE", DeviceValue::ZBufferEnableTrue},
    {"ZBUFFER_USEW", DeviceValue::ZBufferUseW},
    {"FILL_POINT", DeviceValue::FillPoint},
    {"FILL_WIREFRAME", DeviceValue::FillWireframe},
    {"FILL_SOLID", DeviceValue::FillSolid},
    {"SHADE_FLAT", DeviceValue::ShadeFlat},
    {"SHADE_GOURAUD", DeviceValue::ShadeGouraud},
    {"BLEND_ZERO", DeviceValue::BlendZero},
    {"BLEND_ONE", DeviceValue::BlendOne},
    {"BLEND_SRCCOLOR", DeviceValue::BlendSrcColor},
    {"BLEND_INVSRCCOLOR", DeviceValue::BlendInvSrcColor},
    {"BLEND_SRCALPHA", DeviceValue::BlendSrcAlpha},
    {"BLEND_INVSRCALPHA", DeviceValue::BlendInvSrcAlpha},
    {"BLEND_DESTALPHA", DeviceValue::BlendDestAlpha},
    {"BLEND_INVDESTALPHA", DeviceValue::BlendInvDestAlpha},
    {"BLEND_DESTCOLOR", DeviceValue::BlendDestColor},
    {"BLEND_INVDESTCOLOR", DeviceValue::BlendInvDestColor},
    {"BLEND_SRCALPHASAT", DeviceValue::BlendSrcAlphaSat},
    {"BLEND_BLENDFACTOR", DeviceValue::BlendBlendFactor},
    {"BLEND_INVBLENDFACTOR", DeviceValue::BlendInvBlendFactor},
    {"CULL_NONE", DeviceValue::CullNone},
    {"CULL_CW", DeviceValue::CullCw},
    {"CULL_CCW", DeviceValue::CullCcw},
    {"CMP_NEVER", DeviceValue::CmpNever},
    {"CMP_LESS", DeviceValue::CmpLess},
    {"CMP_EQUAL", DeviceValue::CmpEqual},
    {"CMP_LESSEQUAL", DeviceValue::CmpLessEqual},
    {"CMP_GREATER", DeviceValue::CmpGreater},
    {"CMP_NOTEQUAL", DeviceValue::CmpNotEqual},
    {"CMP_GREATEREQUAL", DeviceValue::CmpGreaterEqual},
    {"CMP_ALWAYS", DeviceValue::CmpAlways},
    {"FOG_NONE", DeviceValue::FogNone},
    {"FOG_EXP", DeviceValue::FogExp},
    {"FOG_EXP2", DeviceValue::FogExp2},
    {"FOG_LINEAR", DeviceValue::FogLinear},
    {"STENCILOP_KEEP", DeviceValue::StencilOpKeep},
    {"STENCILOP_ZERO", DeviceValue::StencilOpZero},
    {"STENCILOP_REPLACE", DeviceValue::StencilOpReplace},
    {"STENCILOP_INCRSAT", DeviceValue::StencilOpIncrSat},
    {"STENCILOP_DECRSAT", DeviceValue::StencilOpDecrSat},
    {"STENCILOP_INVERT", DeviceValue::StencilOpInvert},
    {"STENCILOP_INCR", DeviceValue::StencilOpIncr},
    {"STENCILOP_DECR", DeviceValue::StencilOpDecr},
    {"BLENDOP_ADD", DeviceValue::BlendOpAdd},
    {"BLENDOP_SUBTRACT", DeviceValue::BlendOpSubtract},
    {"BLENDOP_REVSUBTRACT", DeviceValue::BlendOpRevSubtract},
    {"BLENDOP_MIN", DeviceValue::BlendOpMin},
    {"BLENDOP_MAX", DeviceValue::BlendOpMax},
};

constexpr Name<SamplerType> kSamplerTypeNames[] = {
    {"SAMP_ADDRESSU", SamplerType::AddressU},           {"SAMP_ADDRESSV", SamplerType::AddressV},
    {"SAMP_ADDRESSW", SamplerType::AddressW},           {"SAMP_MAGFILTER", SamplerType::MagFilter},
    {"SAMP_MINFILTER", SamplerType::MinFilter},         {"SAMP_MIPFILTER", SamplerType::MipFilter},
    {"SAMP_MIPMAPLODBIAS", SamplerType::MipMapLodBias}, {"SAMP_MAXMIPLEVEL", SamplerType::MaxMipLevel},
    {"SAMP_SRGBTEXTURE", SamplerType::SrgbTexture},     {"SAMP_ELEMENTINDEX", SamplerType::ElementIndex},
    {"SAMP_BORDERCOLOR", SamplerType::BorderColor},
};

constexpr Name<SamplerValue> kSamplerValueNames[] = {
    {"TEXADD_WRAP", SamplerValue::TexAddWrap},
    {"TEXADD_MIRROR", SamplerValue::TexAddMirror},
    {"TEXADD_CLAMP", SamplerValue::TexAddClamp},
    {"TEXADD_BORDER", SamplerValue::TexAddBorder},
    {"TEXADD_MIRRORONCE", SamplerValue::TexAddMirrorOnce},
    {"TEXFILT_NONE", SamplerValue::TexFiltNone},
    {"TEXFILT_POINT", SamplerValue::TexFiltPoint},
    {"TEXFILT_LINEAR", SamplerValue::TexFiltLinear},
    {"TEXFILT_ANISOTROPIC", SamplerValue::TexFiltAnisotropic},
    {"TEXFILT_PYRAMIDALQUAD", SamplerValue::TexFiltPyramidalQuad},
    {"TEXFILT_GAUSSIANQUAD", SamplerValue::TexFiltGaussianQuad},
};

template <class Code, std::size_t N> std::optional<Code> Find(const Name<Code> (&names)[N], std::string_view text)
{
    for (const Name<Code> &name : names)
    {
        if (name.text == text)
        {
            return name.code;
        }
    }
    return std::nullopt;
}

// The fallback of 0x004F98B0 and 0x004F9F10 for a text that is no name: wcstol, base 0, and if that
// is 0 then a float's bits (0x004F9DA6-0x004F9DCD).
std::uint32_t NumberBits(std::string_view text)
{
    const std::string z(text);
    const long whole = std::strtol(z.c_str(), nullptr, 0);
    if (whole != 0)
    {
        return static_cast<std::uint32_t>(whole);
    }
    const float real = std::strtof(z.c_str(), nullptr);
    std::uint32_t bits = 0;
    if (real != 0.0F)
    {
        std::memcpy(&bits, &real, sizeof bits);
    }
    return bits;
}

// Returns an attribute's value with its name matched in any case: the engine compares attribute
// names with __wcsicmp (ReadAttributes 0x0050DDE0), and 8 shipped files write `value=`.
std::optional<std::string> Attr(std::string_view tag, std::string_view name)
{
    std::string lower(tag);
    std::ranges::transform(lower, lower.begin(),
                           [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    std::string wanted(name);
    std::ranges::transform(wanted, wanted.begin(),
                           [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    for (std::size_t at = lower.find(wanted); at != std::string::npos; at = lower.find(wanted, at + 1))
    {
        if (at > 0 && xml::Space(tag[at - 1]))
        {
            if (auto value = xml::Attribute(tag.substr(at - 1), tag.substr(at, name.size())))
            {
                return value;
            }
        }
    }
    return std::nullopt;
}

// wcstol(s, 0, 10) (0x008E642D): the stage of a SamplerState, -1 where it gives none.
int StageOf(const std::optional<std::string> &text)
{
    return text ? static_cast<int>(std::strtol(text->c_str(), nullptr, 10)) : -1;
}

std::string Lower(std::string_view text)
{
    std::string out(text);
    std::ranges::transform(out, out.begin(),
                           [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return out;
}

class Reader
{
  public:
    void Take(std::string_view tag)
    {
        if (tag.starts_with('/'))
        {
            return;
        }
        const std::string_view element = xml::ElementOf(tag);
        if (Lower(element) == "materialstate")
        {
            // The first one makes the state, a later one is ignored (0x0050EF0F-0x0050EF21);
            // its name is Name, or ID (0x0050E175).
            if (!seen_)
            {
                seen_ = true;
                const auto name = Attr(tag, "Name");
                out_.name = name ? *name : Attr(tag, "ID").value_or("");
            }
        }
        else if (element == "DeviceState" && seen_)
        {
            Device(tag);
        }
        else if (element == "SamplerState" && seen_)
        {
            Sampler(tag);
        }
    }

    std::optional<State> Done()
    {
        return seen_ ? std::optional(std::move(out_)) : std::nullopt;
    }

  private:
    // 0x0050E4F0 -> MaterialState::AddDeviceState 0x004FA430: kept where the name is known.
    void Device(std::string_view tag)
    {
        const auto state = ParseDeviceState(Attr(tag, "State").value_or(""));
        if (!state)
        {
            ++out_.dropped;
            return;
        }
        out_.device.push_back({*state, ParseDeviceValue(Attr(tag, "Value").value_or(""))});
    }

    // 0x0050E62D -> MaterialState::AddSamplerState 0x004FB8B0: a SamplerName makes it the texture's.
    void Sampler(std::string_view tag)
    {
        const auto type = ParseSamplerType(Attr(tag, "State").value_or(""));
        if (!type)
        {
            ++out_.dropped;
            return;
        }
        SamplerEntry entry;
        entry.texture = Attr(tag, "SamplerName").value_or("");
        entry.stage = entry.texture.empty() ? StageOf(Attr(tag, "Stage")) : 0;
        if (entry.stage == -1) // kept when type and stage are not -1 (0x004FB9CF-0x004FB9E7)
        {
            ++out_.dropped;
            return;
        }
        entry.type = *type;
        entry.value = ParseSamplerValue(Attr(tag, "Value").value_or(""));
        out_.samplers.push_back(std::move(entry));
    }

    State out_;
    bool seen_ = false;
};

void Place(Values &values, const SamplerEntry &entry, int unit)
{
    if (unit >= 0 && std::cmp_less(unit, kSamplerStages))
    {
        values.sampler[static_cast<std::size_t>(unit)][static_cast<std::size_t>(entry.type)] = entry.value;
    }
}

} // namespace

std::optional<DeviceState> ParseDeviceState(std::string_view name)
{
    return Find(kDeviceStateNames, name);
}

std::optional<SamplerType> ParseSamplerType(std::string_view name)
{
    // SAMP_MAXANISOTROPY is matched at 0x004F9EB8 and refused: its JNZ 0x004F9F05 is the -1 return.
    return Find(kSamplerTypeNames, name);
}

std::uint32_t ParseDeviceValue(std::string_view text)
{
    if (const auto code = Find(kDeviceValueNames, text))
    {
        return static_cast<std::uint32_t>(*code);
    }
    if (text == "TRUE") // 0x004F98DB shares ZBUFFERENABLE_TRUE's return
    {
        return 1;
    }
    if (text == "FALSE")
    {
        return 0;
    }
    return NumberBits(text);
}

std::uint32_t ParseSamplerValue(std::string_view text)
{
    if (const auto code = Find(kSamplerValueNames, text))
    {
        return static_cast<std::uint32_t>(*code);
    }
    return NumberBits(text);
}

std::optional<State> Read(std::string_view text)
{
    Reader reader;
    xml::ForEachTag(text, [&reader](std::string_view tag) { reader.Take(tag); });
    return reader.Done();
}

bool ValidResourceName(std::string_view name)
{
    // 0x004F7ACF-0x004F7B04
    return std::ranges::all_of(name, [](char letter) {
        return (letter >= '0' && letter <= '9') || (letter >= 'a' && letter <= 'z') ||
               (letter >= 'A' && letter <= 'Z') || letter == '_' || letter == '-' || letter == '(' || letter == ')' ||
               letter == '.';
    });
}

std::string FileNameFor(std::string_view name)
{
    // MaterialStateManager::Load 0x004FC720: the tests are over the file name, ".xml" in it.
    std::string file(name);
    file += ".xml";
    if (file.size() > 0x20 || !ValidResourceName(file))
    {
        return "DefaultState.sta.xml";
    }
    return file;
}

Values ValuesOf(const State &state, const TextureUnits &units)
{
    Values values;
    for (const DeviceEntry &entry : state.device)
    {
        values.device[static_cast<std::size_t>(entry.state)] = entry.value;
    }
    for (const SamplerEntry &entry : state.samplers)
    {
        if (entry.texture.empty())
        {
            Place(values, entry, entry.stage);
        }
        else if (units)
        {
            // Skipped where the bound shader has no such texture (0xFFFF, 0x004FC113-0x004FC19C).
            if (const auto unit = units(entry.texture))
            {
                Place(values, entry, *unit);
            }
        }
    }
    return values;
}

std::vector<NamedSampler> NamedSamplers(const State &state)
{
    std::vector<NamedSampler> named;
    for (const SamplerEntry &entry : state.samplers)
    {
        if (entry.texture.empty())
        {
            continue;
        }
        auto found = std::ranges::find(named, entry.texture, &NamedSampler::texture);
        if (found == named.end())
        {
            named.push_back({.texture = entry.texture, .values = {}});
            found = std::prev(named.end());
        }
        found->values[static_cast<std::size_t>(entry.type)] = entry.value;
    }
    return named;
}

Values Effective(const State &defaults, const State &pass, const TextureUnits &units)
{
    // The defaults take stage entries only (0x004FBD6B-0x004FBE0C).
    Values values = ValuesOf(defaults);
    const Values named = ValuesOf(pass, units);
    for (std::size_t i = 0; i < kDeviceStates; ++i)
    {
        if (named.device[i])
        {
            values.device[i] = named.device[i];
        }
    }
    for (std::size_t stage = 0; stage < kSamplerStages; ++stage)
    {
        for (std::size_t type = 0; type < kSamplerTypes; ++type)
        {
            if (named.sampler[stage][type])
            {
                values.sampler[stage][type] = named.sampler[stage][type];
            }
        }
    }
    return values;
}

} // namespace dao::core::sta
