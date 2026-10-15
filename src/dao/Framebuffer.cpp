// The framebuffer's effects (dao/Framebuffer.h).
//
// Read 2026-10-10 from the two commands' handlers (DAOLib #62).
// FB_SetEffectResource (0x00651880) and FB_SetEffectEnabled (0x006519B0)
// both ask the object in the global at 0x00C1E8B0 for the effect of a name
// (0x00456CA0, __thiscall (const std::wstring &), `ret 4`): a std::vector
// of Renderer::FrameBufferEffect pointers at its +0 (first at +4, last at
// +8), each compared by the name at the effect's +8. The effect's +4 is
// whether it is on (0x0045FA10 sets it and tells the effect's stages). Its
// values are a map by name at +0x34, which 0x004602D0 looks in (__thiscall
// (const std::wstring &), `ret 4`): a Renderer::FrameBufferResource, whose
// +4 is its type, 3 a float, the only type the command sets. A float's
// value is at +0x2C (0x0045ED90 sets it and clears +0x54); its getter
// (0x0045EDB0) answers +0x44 instead while +0x54 is set, a value on its
// way to another. EA's build has the same offsets (0x00891570, 0x008982F0,
// 0x008976D0). The global's class has no RTTI; its name here is ours.
//
// Seen in play 2026-10-10 (7dc6606's examples/atmosphere): two effects, da_bloom and
// da_hsvmatrix, both on; a name is found whatever its case. An area comes
// with its own values: Redcliffe village's da_hsvmatrix read hue 0,
// saturation 0.85, contrast 1.02, brightness 1, and the night village's
// saturation 0.82. Turning da_hsvmatrix off is therefore not the game's
// own look: it left the picture about two thirds as bright as a load does.
//
// What clears an effect's target, read 2026-10-15 after DAOElgaraShaders saw SMAA's edge target
// keep the last frame's edges. A filter's `bind` line (0x00460CE0, tokens split on space and
// tab) knows a target or `backbuffer` or `null`, `at` and an index, and `viewport` with
// `original`, `recalculate` or `stored`, and nothing else: `clear` and a colour after it, as
// da_bssrdf's chain has them, are skipped, and FBIBindTarget's run (its slot 2, 0x0045E410) only
// sets the render target and the viewport. So a bind never clears. What does is the target's own
// attribute: `<target ... autoclear="r g b a">` (four floats, as da_distortion's DistortionMask
// has "0.5 0.5 0.5 0.705"; FBRenderTargetResource's 0x004617A0 sets its +0x4C flag 0x80 and the
// colour from +0x60) puts the target, once made (0x00461DE2), in a list (0x00C1EAD8, taken out
// as it goes, 0x0045FC84). The list is cleared each frame, every target bound in turn and cleared
// to its colour (0x00456D20), at the end of the world's drawing (Renderer::EclipseUmbraCommander's
// callback for 0xA1, 0x0045CCA2), once the graphics are up (their slot 62, +0x114).
#include "dao/Framebuffer.h"

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/core/Msvc8.h"
#include "dao/generated/ScriptApi.h"

#include <cstddef>
#include <cstdint>

namespace dao::framebuffer
{

namespace
{
constexpr std::size_t kEnabled = 4;          // in an effect: a bool
constexpr std::size_t kName = 8;             // in an effect: a std::wstring
constexpr std::size_t kType = 4;             // in a value
constexpr std::int32_t kFloat = 3;           // the type of a float value
constexpr std::size_t kFloatValue = 0x2C;    // in a float value
constexpr std::size_t kFloatMoving = 0x44;   // the value while it is on its way to another
constexpr std::size_t kFloatIsMoving = 0x54; // a bool

using FindFn = std::uintptr_t(__fastcall *)(std::uintptr_t, int, const core::WStringRep *);

std::uintptr_t Keeper()
{
    return Module::IsKnownExecutable() ? Global<std::uintptr_t>(addr::kFrameBufferEffects) : 0;
}

std::uintptr_t Find(AddressId finder, std::uintptr_t in, std::wstring_view name)
{
    if (in == 0)
    {
        return 0;
    }
    const core::WStringRep text = core::WStringRep::Borrow(name);
    return reinterpret_cast<FindFn>(Module::Address(finder))(in, 0, &text);
}

std::uintptr_t Effect(std::wstring_view name)
{
    return Find(addr::kFrameBufferEffects_Find, Keeper(), name);
}
} // namespace

std::vector<std::wstring> Effects()
{
    std::vector<std::wstring> names;
    const std::uintptr_t keeper = Keeper();
    const auto effects = keeper != 0 ? TryRead<core::VectorRep<std::uintptr_t>>(keeper) : std::nullopt;
    if (!effects)
    {
        return names;
    }
    for (const std::uintptr_t effect : effects->View())
    {
        if (const auto name = TryRead<core::WStringRep>(effect + kName))
        {
            names.emplace_back(name->View());
        }
    }
    return names;
}

std::optional<bool> Enabled(std::wstring_view effect)
{
    const std::uintptr_t found = Effect(effect);
    return found != 0 ? TryRead<bool>(found + kEnabled) : std::nullopt;
}

std::optional<float> Value(std::wstring_view effect, std::wstring_view value)
{
    const std::uintptr_t found = Find(addr::kFrameBufferEffect_FindResource, Effect(effect), value);
    if (found == 0 || TryRead<std::int32_t>(found + kType) != kFloat)
    {
        return std::nullopt;
    }
    const bool moving = TryRead<bool>(found + kFloatIsMoving).value_or(false);
    return TryRead<float>(found + (moving ? kFloatMoving : kFloatValue));
}

void SetEnabled(std::wstring_view effect, bool on)
{
    script::api::FB_SetEffectEnabled(effect, on ? 1 : 0);
}

void SetValue(std::wstring_view effect, std::wstring_view value, float to)
{
    script::api::FB_SetEffectResource(effect, value, to);
}

} // namespace dao::framebuffer
