#include "dao/GameModes.h"

#include "dao/SafeRead.h"
#include "dao/TypeName.h"

#include <string>

namespace dao
{

const void *GameModeController::ModeObject(GameMode mode) const
{
    const auto index = static_cast<std::size_t>(mode);
    const auto modes = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(this) + kModes);
    if (!modes || *modes == 0 || index >= kMaxModes)
    {
        return nullptr;
    }
    const auto entry = TryRead<std::uintptr_t>(*modes + 4 * index);
    if (!entry || *entry == 0)
    {
        return nullptr;
    }
    const auto *object = reinterpret_cast<const void *>(*entry);
    return TypeName(object).rfind("GameMode", 0) == 0 ? object : nullptr;
}

} // namespace dao
