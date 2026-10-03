#include "dao/Portraits.h"

#include "dao/Addresses.h"
#include "dao/Gda.h"
#include "dao/Hook.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace dao::portraits
{

namespace
{

// What Render passes for a value the table is to give (dao/Addresses.h).
constexpr float kUnset = 65505.0F;
// The renderer's source creature, and the table's row it frames by.
constexpr std::uintptr_t kSource = 0xD40;
constexpr std::uintptr_t kRow = 0xD48;
// The portraits table, its Default row, and the blank row 0, which has no
// camera.
constexpr std::int32_t kTable = 7;
constexpr std::int32_t kDefaultRow = 1;
constexpr std::int32_t kBlankRow = 0;
// A creature's appearance: its +0x7B0 record's word at +0x58, as
// PlaceCamera reads it.
constexpr std::uintptr_t kAppearanceRecord = 0x7B0;
constexpr std::uintptr_t kAppearance = 0x58;

std::mutex g_lock;
std::unordered_map<ObjectId, Camera> g_cameras;

// The row PlaceCamera would read a creature on the given row from: on the
// Default row, the row for its appearance if the table has one.
std::int32_t TableRow(std::uintptr_t source, std::int32_t row)
{
    if (row != kDefaultRow)
    {
        return row;
    }
    const auto record = TryRead<std::uintptr_t>(source + kAppearanceRecord);
    const auto appearance = record && *record != 0 ? TryRead<std::uint16_t>(*record + kAppearance) : std::nullopt;
    const auto rows = gda::Rows(kTable);
    if (!appearance || !rows)
    {
        return row;
    }
    for (std::int32_t i = 1; i < *rows; ++i)
    {
        if (gda::GetInt(kTable, i, core::GdaColumnHash("Appearance")) == static_cast<std::int32_t>(*appearance))
        {
            return i;
        }
    }
    return row;
}

// PortraitRenderer::PlaceCamera: __thiscall (pitch, rotation, distance, Z
// adjust, X adjust, light X, light Y, light Z), `ret 0x20`.
using PlaceCameraFn = void(__fastcall *)(void *renderer, int edx, float pitch, float rotation, float distance,
                                         float zAdjust, float xAdjust, float lightX, float lightY, float lightZ);
Detour<PlaceCameraFn> g_placeCamera;

void __fastcall PlaceCamera(void *renderer, int edx, float pitch, float rotation, float distance, float zAdjust,
                            float xAdjust, float lightX, float lightY, float lightZ)
{
    const auto at = reinterpret_cast<std::uintptr_t>(renderer);
    const auto source = renderer != nullptr ? TryRead<std::uintptr_t>(at + kSource) : std::nullopt;
    const auto id = source && *source != 0 ? TryRead<ObjectId>(*source + CGameObject::kId) : std::nullopt;
    const auto row = renderer != nullptr ? TryRead<std::int32_t>(at + kRow) : std::nullopt;
    std::optional<Camera> camera;
    if (id && row)
    {
        const std::lock_guard lock(g_lock);
        if (const auto found = g_cameras.find(*id); found != g_cameras.end())
        {
            camera = found->second;
        }
    }
    if (!camera)
    {
        g_placeCamera.Original()(renderer, edx, pitch, rotation, distance, zAdjust, xAdjust, lightX, lightY, lightZ);
        return;
    }
    // A creature on the Default row with an appearance other than a
    // person's is framed from the table whatever it is given (PlaceCamera
    // reads every value again when its row is 1, dao/Addresses.h). So every
    // value is given here, the plugin's or the table's as PlaceCamera would
    // have read it, and the renderer's row is the blank one for the call.
    const std::int32_t tableRow = TableRow(*source, *row);
    const auto give = [tableRow](float &value, const std::optional<float> &given, std::string_view column) {
        if (value != kUnset)
        {
            return;
        }
        if (given)
        {
            value = *given;
        }
        else if (const auto table = gda::GetFloat(kTable, tableRow, core::GdaColumnHash(column)))
        {
            value = *table;
        }
    };
    give(pitch, camera->pitch, "CameraPitch");
    give(rotation, camera->rotation, "CameraRotation");
    give(distance, camera->distance, "CameraDistance");
    give(zAdjust, camera->zAdjust, "CameraZAdjust");
    give(xAdjust, camera->xAdjust, "CameraXAdjust");
    give(lightX, camera->lightX, "LightXAdjust");
    give(lightY, camera->lightY, "LightYAdjust");
    give(lightZ, camera->lightZ, "LightZAdjust");
    *reinterpret_cast<std::int32_t *>(at + kRow) = kBlankRow;
    g_placeCamera.Original()(renderer, edx, pitch, rotation, distance, zAdjust, xAdjust, lightX, lightY, lightZ);
    *reinterpret_cast<std::int32_t *>(at + kRow) = *row;
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
