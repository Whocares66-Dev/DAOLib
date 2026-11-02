// A material state file (.sta, kept as <name>.sta.xml in states.erf) out of the game's files, and the
// rule the engine draws it by (DAOLib #295): a state names some of the 51 render states (DS_*) and some
// sampler states (SAMP_*) of a stage or of a texture parameter, and DefaultState.sta holds the values of
// all the others. A pass's effective state is the defaults with the pass's named entries over them; the
// engine restores the defaults for what the next state does not name, so it never inherits from the
// previous pass. For drawing a material as the game does (DAOParagonEngine) and for tools that change
// states (DAOElgaraShaders).
//
// Marks: [exe] read in the GOG build (MaterialState and MaterialStateManager, 0x004F93B0-0x004FC2C0, the
// builder 0x0050E4F0), [read] from a reading of it or of the 178 shipped files (2026-11-01), not seen run.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::sta
{

// The render states a DeviceState names, by the index the engine keeps them at. [exe 0x004F93B0]
enum class DeviceState : int
{
    ZEnable = 0,
    FillMode,
    ShadeMode,
    ZWriteEnable,
    AlphaTestEnable,
    SrcBlend,
    DestBlend,
    CullMode,
    ZFunc,
    AlphaRef,
    AlphaFunc,
    AlphaBlendEnable,
    FogEnable,
    FogColor,
    FogTableMode,
    FogStart,
    FogEnd,
    FogDensity,
    RangeFogEnable,
    StencilEnable,
    StencilFail,
    StencilZFail,
    StencilPass,
    StencilFunc,
    StencilRef,
    StencilMask,
    StencilWriteMask,
    Clipping,
    FogVertexMode,
    ClipPlaneEnable,
    MultisampleAntialias,
    MultisampleMask,
    ColorWriteMask,
    BlendOp,
    ScissorTestEnable,
    SlopeScaleDepthBias,
    TwoSidedStencilMode,
    CcwStencilFail,
    CcwStencilZFail,
    CcwStencilPass,
    CcwStencilFunc,
    ColorWriteMask1,
    ColorWriteMask2,
    ColorWriteMask3,
    BlendFactor,
    SrgbWriteEnable,
    DepthBias,
    SeparateAlphaBlendEnable,
    SrcBlendAlpha,
    DestBlendAlpha,
    BlendOpAlpha,
};
inline constexpr std::size_t kDeviceStates = 51;

// The sampler states a SamplerState names, by the index the engine keeps them at. 8 is the
// anisotropy, which the engine refuses to read from a file. [exe 0x004F9DE0]
enum class SamplerType : int
{
    AddressU = 0,
    AddressV = 1,
    AddressW = 2,
    MagFilter = 3,
    MinFilter = 4,
    MipFilter = 5,
    MipMapLodBias = 6,
    MaxMipLevel = 7,
    SrgbTexture = 9,
    ElementIndex = 10,
    BorderColor = 11,
};
inline constexpr std::size_t kSamplerTypes = 12;
inline constexpr std::size_t kSamplerStages = 16;

// The engine's own codes for the names a DeviceState's Value may be (not Direct3D's; the driver
// maps them). [exe 0x004F98B0] 19 and 20 have no name.
enum class DeviceValue : std::uint32_t
{
    ZBufferEnableFalse = 0,
    ZBufferEnableTrue = 1,
    ZBufferUseW = 2,
    FillPoint = 3,
    FillWireframe = 4,
    FillSolid = 5,
    ShadeFlat = 6,
    ShadeGouraud = 7,
    BlendZero = 8,
    BlendOne = 9,
    BlendSrcColor = 10,
    BlendInvSrcColor = 11,
    BlendSrcAlpha = 12,
    BlendInvSrcAlpha = 13,
    BlendDestAlpha = 14,
    BlendInvDestAlpha = 15,
    BlendDestColor = 16,
    BlendInvDestColor = 17,
    BlendSrcAlphaSat = 18,
    BlendBlendFactor = 21,
    BlendInvBlendFactor = 22,
    CullNone = 23,
    CullCw = 24,
    CullCcw = 25,
    CmpNever = 26,
    CmpLess = 27,
    CmpEqual = 28,
    CmpLessEqual = 29,
    CmpGreater = 30,
    CmpNotEqual = 31,
    CmpGreaterEqual = 32,
    CmpAlways = 33,
    FogNone = 34,
    FogExp = 35,
    FogExp2 = 36,
    FogLinear = 37,
    StencilOpKeep = 38,
    StencilOpZero = 39,
    StencilOpReplace = 40,
    StencilOpIncrSat = 41,
    StencilOpDecrSat = 42,
    StencilOpInvert = 43,
    StencilOpIncr = 44,
    StencilOpDecr = 45,
    BlendOpAdd = 46,
    BlendOpSubtract = 47,
    BlendOpRevSubtract = 48,
    BlendOpMin = 49,
    BlendOpMax = 50,
};

// The codes a SamplerState's Value may name. [exe 0x004F9F10]
enum class SamplerValue : std::uint32_t
{
    TexAddWrap = 0,
    TexAddMirror,
    TexAddClamp,
    TexAddBorder,
    TexAddMirrorOnce,
    TexFiltNone,
    TexFiltPoint,
    TexFiltLinear,
    TexFiltAnisotropic,
    TexFiltPyramidalQuad,
    TexFiltGaussianQuad,
};

// Returns the state a name ("DS_CULLMODE") gives, matched whole and with case.
// @return nullopt where the engine gives -1, and drops the entry
std::optional<DeviceState> ParseDeviceState(std::string_view name);
// Returns the type a name ("SAMP_ADDRESSU") gives. SAMP_MAXANISOTROPY gives nothing.
std::optional<SamplerType> ParseSamplerType(std::string_view name);
// Returns a DeviceState's value as the engine takes it: a name ("CULL_CW", "TRUE", "FALSE") matched
// whole and with case; else the text as an integer (C's strtol, base 0, so "1.0" is 1 and "0x10" is 16);
// if that is 0, as a float, whose bits are returned. Text that parses to nothing is 0.
std::uint32_t ParseDeviceValue(std::string_view text);
// Returns a SamplerState's value the same way, over the sampler names.
std::uint32_t ParseSamplerValue(std::string_view text);

struct DeviceEntry
{
    DeviceState state = DeviceState::ZEnable;
    std::uint32_t value = 0;
};

// A sampler state of a stage, or of the texture parameter a shader samples (`texture` non-empty),
// whose unit is found when the shader is bound.
struct SamplerEntry
{
    int stage = 0;       // the unit; unused where `texture` is named
    std::string texture; // the SamplerName; empty for a stage's
    SamplerType type = SamplerType::AddressU;
    std::uint32_t value = 0;
};

// A state file as it says it, entries in file order. Entries the engine drops (a state or type name it
// does not know, every SAMP_MAXANISOTROPY) are left out, and `dropped` counts them.
struct State
{
    std::string name;
    std::vector<DeviceEntry> device;
    std::vector<SamplerEntry> samplers;
    std::size_t dropped = 0;
};

// Reads a state file's text (a BOM and comments are fine).
// @return nullopt where the text has no MaterialState element
std::optional<State> Read(std::string_view text);

// Returns whether a resource name has only 0-9 a-z A-Z _ - ( ) . in it; an empty one has.
bool ValidResourceName(std::string_view name);
// Returns the resource a state asked for by name loads: "<name>.xml", or "DefaultState.sta.xml" where
// that has more than 32 characters or is not a valid resource name, and the state is then filed under
// the name asked for. A valid name with no file in the archive loads nothing.
std::string FileNameFor(std::string_view name);

// The values of all the render states and sampler states at once; unset where nothing names them
// (the engine's 0x7FFFFFFF).
struct Values
{
    std::array<std::optional<std::uint32_t>, kDeviceStates> device;
    std::array<std::array<std::optional<std::uint32_t>, kSamplerTypes>, kSamplerStages> sampler;
};

// Returns the unit a shader has bound the texture parameter of a name to, or nothing where it has not.
using TextureUnits = std::function<std::optional<int>(std::string_view texture)>;

// Returns a state's own entries as values, a later entry of the same state replacing an earlier. A
// texture-named sampler is placed by `units`, and left out where that gives no unit, or a unit past
// the 16 stages.
Values ValuesOf(const State &state, const TextureUnits &units = {});

// Returns what is set when a pass draws under its state: the defaults, with the entries the pass
// names over them. Nothing of the state drawn before is kept, so a value an earlier state named and
// this one does not is back at the default. The defaults' own texture-named samplers are not part of
// them.
Values Effective(const State &defaults, const State &pass, const TextureUnits &units = {});

} // namespace dao::core::sta
