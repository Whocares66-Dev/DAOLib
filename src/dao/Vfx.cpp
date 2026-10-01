#include "dao/Vfx.h"

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/World.h"

namespace dao::vfx
{

namespace
{

struct alignas(16) Vector4
{
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 0;
};

Vector4 Padded(const Vector &v)
{
    return {v.x, v.y, v.z, 0.0F};
}

// What Place is given (0x00711320 builds one on the stack).
struct Descriptor
{
    std::int32_t vfx;
    ObjectId area;
    float scale;
    std::int32_t zero;
};

void *Manager()
{
    using Fn = void *(*)();
    return reinterpret_cast<Fn>(Module::Address(addr::kVfxManager_Get))();
}

// The placed effect's object, or null when it is gone.
void *Find(ObjectId effect)
{
    using Fn = void *(__fastcall *)(void *, int, ObjectId);
    return reinterpret_cast<Fn>(Module::Address(addr::kVfxManager_Find))(Manager(), 0, effect);
}

constexpr std::size_t kSetOrientation = 112; // the effect object's vtable slot

void Turn(void *object, const Vector4 &facing)
{
    using Fn = void(__fastcall *)(void *, int, const Vector4 *);
    const Fn *vtable = *static_cast<const Fn *const *>(object);
    vtable[kSetOrientation](object, 0, &facing);
}

} // namespace

ObjectId Place(std::int32_t vfx, const Vector &at, float scale, const std::optional<Vector> &facing)
{
    const CCArea *area = world::CurrentArea();
    if (area == nullptr)
    {
        return kInvalidObject;
    }
    const Descriptor descriptor{vfx, area->Id(), scale, 0};
    const Vector4 where = Padded(at);
    const Vector4 direction = Padded(facing.value_or(Vector{}));
    // Mode 2, as explore mode passes: the effect is loaded now, not by a task.
    constexpr std::int32_t kMode = 2;
    using Fn = ObjectId(__fastcall *)(void *, int, const Descriptor *, ObjectId, std::int32_t, const Vector4 *,
                                      const Vector4 *, std::uint32_t);
    return reinterpret_cast<Fn>(Module::Address(addr::kVfxManager_Place))(
        Manager(), 0, &descriptor, kInvalidObject, kMode, &where, facing ? &direction : nullptr, 0);
}

bool Move(ObjectId effect, const Vector &at, const std::optional<Vector> &facing)
{
    void *object = Find(effect);
    if (object == nullptr)
    {
        return false;
    }
    const Vector4 where = Padded(at);
    using SetPositionFn = void(__fastcall *)(void *, int, const Vector4 *, std::int32_t);
    reinterpret_cast<SetPositionFn>(Module::Address(addr::kCCObject_SetPosition))(object, 0, &where, 0);
    if (facing)
    {
        Turn(object, Padded(*facing));
    }
    return true;
}

void Remove(ObjectId effect)
{
    if (effect == kInvalidObject)
    {
        return;
    }
    using Fn = void(__fastcall *)(void *, int, ObjectId, std::int32_t);
    reinterpret_cast<Fn>(Module::Address(addr::kVfxManager_Remove))(Manager(), 0, effect, 0);
}

} // namespace dao::vfx
