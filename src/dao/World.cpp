#include "dao/World.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace dao::world
{

namespace
{

constexpr std::uint32_t kMaxHits = 32;    // what the engine's own callers ask for
constexpr std::size_t kRaycaster = 0x10C; // in the application: Raycast's `this`

// The engine's vectors are 16 bytes and aligned for SSE.
struct alignas(16) Vector4
{
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 0;

    Vector4() = default;
    explicit Vector4(const Vector &v) : x(v.x), y(v.y), z(v.z)
    {
    }
    Vector ToVector() const
    {
        return {x, y, z};
    }
};

// Off the game's thread, once it is known, a query would race the cursor
// pick for the raycast's static results.
bool OnGameThread()
{
    const std::uint32_t frame = FrameThreadId();
    return frame == 0 || frame == GetCurrentThreadId();
}

float DistanceSquared(const Vector &a, const Vector &b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

Sight Ask(CCArea *area, const Vector &from, const Vector &to, ObjectId ignore)
{
    if (area == nullptr || !OnGameThread())
    {
        return {};
    }
    using Fn = bool(__fastcall *)(CCArea *, int, const Vector4 *, const Vector4 *, Vector4 *, ObjectId, std::uint32_t);
    const Vector4 a(from);
    const Vector4 b(to);
    Vector4 blocked;
    const bool clear = reinterpret_cast<Fn>(Module::Address(addr::kCCArea_LineOfSight))(area, 0, &a, &b, &blocked,
                                                                                        ignore, groups::kSight);
    Sight sight;
    sight.clear = clear;
    if (!clear)
    {
        sight.blockedAt = blocked.ToVector();
    }
    return sight;
}

} // namespace

CCArea *CurrentArea()
{
    CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return nullptr;
    }
    CCModule *module = app->GetModule();
    return module != nullptr ? module->CurrentArea() : nullptr;
}

std::vector<Hit> Raycast(const Vector &origin, const Vector &direction, std::uint32_t mask, bool selectionVolumes)
{
    CClientExoApp *app = CClientExoApp::Get();
    const float length = std::sqrt(DistanceSquared(direction, {}));
    if (app == nullptr || length < 1e-6F || !OnGameThread() || CurrentArea() == nullptr)
    {
        return {};
    }
    const auto raycaster = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(app) + kRaycaster);
    const Vector4 from(origin);
    const Vector4 unit(Vector{direction.x / length, direction.y / length, direction.z / length});
    alignas(16) std::array<Hit, kMaxHits> hits{};

    using Fn = std::uint32_t(__fastcall *)(void *, int, Hit *, std::uint32_t, const Vector4 *, const Vector4 *,
                                           std::uint32_t, bool, void *);
    const std::uint32_t count = reinterpret_cast<Fn>(Module::Address(addr::kWorld_Raycast))(
        reinterpret_cast<void *>(raycaster.value_or(0)), 0, hits.data(), kMaxHits, &from, &unit, mask, selectionVolumes,
        nullptr);

    std::vector<Hit> out(hits.begin(), hits.begin() + static_cast<std::ptrdiff_t>(std::min(count, kMaxHits)));
    std::ranges::sort(out, [&](const Hit &a, const Hit &b) {
        return DistanceSquared(a.position, origin) < DistanceSquared(b.position, origin);
    });
    return out;
}

std::optional<Hit> FirstHit(const Vector &from, const Vector &to, std::uint32_t mask)
{
    const Vector along{to.x - from.x, to.y - from.y, to.z - from.z};
    const std::vector<Hit> hits = Raycast(from, along, mask); // nearest first
    if (hits.empty() || DistanceSquared(hits.front().position, from) > DistanceSquared(to, from))
    {
        return std::nullopt;
    }
    return hits.front();
}

Sight LineOfSight(const Vector &from, const Vector &to, ObjectId ignore)
{
    return Ask(CurrentArea(), from, to, ignore);
}

Sight LineOfSight(ObjectId viewer, ObjectId target)
{
    CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr || !OnGameThread())
    {
        return {};
    }
    CCObject *from = app->GetObjectById(viewer);
    CCObject *to = app->GetObjectById(target);
    if (from == nullptr || to == nullptr)
    {
        return {};
    }
    CCArea *area = from->Area();
    if (area == nullptr || area->Id() != to->AreaId())
    {
        return {};
    }
    Vector eye = from->Position();
    Vector seen = to->Position();
    eye.z += kEyeHeight;
    seen.z += kEyeHeight;
    return Ask(area, eye, seen, kInvalidObject);
}

} // namespace dao::world
