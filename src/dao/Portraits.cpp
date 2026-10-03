#include "dao/Portraits.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace dao::portraits
{

namespace
{

// What Render passes for a value the table is to give (dao/Addresses.h).
constexpr float kUnset = 65505.0F;
// The renderer's source creature.
constexpr std::uintptr_t kSource = 0xD40;

std::mutex g_lock;
std::unordered_map<ObjectId, Camera> g_cameras;

// PortraitRenderer::PlaceCamera: __thiscall (pitch, rotation, distance, Z
// adjust, X adjust, light X, light Y, light Z), `ret 0x20`.
using PlaceCameraFn = void(__fastcall *)(void *renderer, int edx, float pitch, float rotation, float distance,
                                         float zAdjust, float xAdjust, float lightX, float lightY, float lightZ);
Detour<PlaceCameraFn> g_placeCamera;

void __fastcall PlaceCamera(void *renderer, int edx, float pitch, float rotation, float distance, float zAdjust,
                            float xAdjust, float lightX, float lightY, float lightZ)
{
    const auto source = renderer != nullptr
                            ? TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(renderer) + kSource)
                            : std::nullopt;
    const auto id = source && *source != 0 ? TryRead<ObjectId>(*source + CGameObject::kId) : std::nullopt;
    if (id)
    {
        const std::lock_guard lock(g_lock);
        if (const auto found = g_cameras.find(*id); found != g_cameras.end())
        {
            const Camera &camera = found->second;
            const auto give = [](float &value, const std::optional<float> &given) {
                if (value == kUnset && given)
                {
                    value = *given;
                }
            };
            give(pitch, camera.pitch);
            give(rotation, camera.rotation);
            give(distance, camera.distance);
            give(zAdjust, camera.zAdjust);
            give(xAdjust, camera.xAdjust);
            give(lightX, camera.lightX);
            give(lightY, camera.lightY);
            give(lightZ, camera.lightZ);
        }
    }
    g_placeCamera.Original()(renderer, edx, pitch, rotation, distance, zAdjust, xAdjust, lightX, lightY, lightZ);
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] { g_placeCamera.Attach(addr::kPortraitRenderer_PlaceCamera, &PlaceCamera); });
}

} // namespace

void SetCamera(ObjectId creature, const Camera &camera)
{
    Attach();
    const std::lock_guard lock(g_lock);
    g_cameras[creature] = camera;
}

void ClearCamera(ObjectId creature)
{
    const std::lock_guard lock(g_lock);
    g_cameras.erase(creature);
}

} // namespace dao::portraits
