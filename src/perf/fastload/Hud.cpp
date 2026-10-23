#include "perf/fastload/Hud.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/GameModes.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "perf/fastload/Shared.h"

#include <spdlog/spdlog.h>

#include <Windows.h>

#include <array>
#include <cstdint>

namespace perf::fastload::hud
{

namespace
{

// The loading mode's first state of an area transition, which every door and DoAreaTransition
// begins in (dao/Addresses.h, "The loading mode's states"); it is the state the GUI's mode
// change finds, as it runs in the switch to the loading mode.
constexpr std::uint16_t kTransition = 7;

// DragonAgeGUISystem's slots 28 and 29, __thiscall with nothing, `ret`.
using HudFn = void(__fastcall *)(void *gui, int edx);
dao::Detour<HudFn> g_close;
HudFn g_hide = nullptr;

bool InTransition()
{
    const auto *app = dao::CClientExoApp::Get();
    const auto *modes = app != nullptr ? app->GameModes() : nullptr;
    if (modes == nullptr || modes->Mode() != dao::GameMode::Loading)
    {
        return false;
    }
    const auto mode = reinterpret_cast<std::uintptr_t>(modes->ModeObject(dao::GameMode::Loading));
    return mode != 0 && At<std::uint16_t>(mode, kModeState) == kTransition;
}

void __fastcall Close(void *gui, int edx)
{
    if (On() && InTransition())
    {
        g_hide(gui, edx);
        return;
    }
    g_close.Original()(gui, edx);
}

} // namespace

void Attach()
{
    std::array<char, 4> value{};
    if (GetEnvironmentVariableA("DAOLIB_KEEP_HUD", value.data(), static_cast<DWORD>(value.size())) == 1 &&
        value[0] == '0')
    {
        spdlog::info("fast load: DAOLIB_KEEP_HUD=0, so a door makes the HUD anew as ever");
        return;
    }
    g_hide = reinterpret_cast<HudFn>(dao::Module::Address(dao::addr::kDragonAgeGUISystem_HideHud));
    if (g_hide == nullptr || !g_close.Attach(dao::addr::kDragonAgeGUISystem_CloseHud, &Close))
    {
        spdlog::warn("fast load: the HUD's close could not be detoured, so a door makes it anew as ever");
    }
}

} // namespace perf::fastload::hud
