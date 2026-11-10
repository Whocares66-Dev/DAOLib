// DAOElgaraShaders #80, 2026-11-10: Steam's GetSwapChain discovers a vtable replacement,
// hooks that callback inline too, then recurses through its shared Present trampoline.
// Watch the two engine callers instead (Addresses.h, IDs 23543 and 24028). RTSS restoring
// d3d9's code cannot erase these hooks either. Each consumer DLL has its own callback list,
// as with OnFrame; its engine detours chain with the loader's fast-loading detours.
#include "dao/RenderFrame.h"

#include "dao/Addresses.h"
#include "dao/Guard.h"
#include "dao/Hook.h"

#include <d3d9.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace dao::render
{
namespace
{

using PresentFn = bool(__fastcall *)(std::uintptr_t self, int edx);
Detour<PresentFn> g_driverPresent;
Detour<PresentFn> g_chainPresent;
std::mutex g_mutex;
using Callbacks = std::vector<std::shared_ptr<FrameEndEvent>>;
std::atomic<std::shared_ptr<const Callbacks>> g_callbacks;

void EndFrame(IDirect3DDevice9 *device, bool bySwapChain)
{
    // Keep each callable itself: copying std::function each frame would discard a mutable
    // callback's state. The immutable list also avoids allocating on the rendering thread.
    const auto callbacks = g_callbacks.load();
    if (!callbacks)
    {
        return;
    }
    for (const auto &callback : *callbacks)
    {
        detail::Guard("a render frame-end callback", [&] { (*callback)(device, bySwapChain); });
    }
}

bool __fastcall DriverPresent(std::uintptr_t driver, int edx)
{
    // These are the fields read by the original engine functions in all three builds.
    auto *device = *reinterpret_cast<IDirect3DDevice9 **>(driver + 0x100);
    EndFrame(device, false);
    return g_driverPresent.Original()(driver, edx);
}

bool __fastcall ChainPresent(std::uintptr_t engineChain, int edx)
{
    auto *chain = *reinterpret_cast<IDirect3DSwapChain9 **>(engineChain + 0x10);
    IDirect3DDevice9 *device = nullptr;
    if (chain != nullptr)
    {
        chain->GetDevice(&device);
    }
    EndFrame(device, true);
    if (device != nullptr)
    {
        device->Release();
    }
    return g_chainPresent.Original()(engineChain, edx);
}

} // namespace

bool OnFrameEnd(FrameEndEvent callback)
{
    if (!callback)
    {
        return false;
    }
    const std::lock_guard lock(g_mutex);
    const auto callbacks = g_callbacks.load();
    auto updated = callbacks ? std::make_shared<Callbacks>(*callbacks) : std::make_shared<Callbacks>();
    updated->push_back(std::make_shared<FrameEndEvent>(std::move(callback)));
    if (!g_driverPresent.IsAttached())
    {
        if (!g_driverPresent.Attach(addr::kD3DGraphicsDriver_Present, &DriverPresent) ||
            !g_chainPresent.Attach(addr::kD3DSwapChain_Present, &ChainPresent))
        {
            g_chainPresent.Detach();
            g_driverPresent.Detach();
            spdlog::error("render frames: the engine's two presentation hooks could not be placed");
            return false;
        }
        spdlog::info(
            "render frames: the engine's device and swap-chain presentation watched; Direct3D Present unchanged");
    }
    g_callbacks.store(std::move(updated));
    return true;
}

} // namespace dao::render
