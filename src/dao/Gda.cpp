#include "dao/Gda.h"

#include "dao/Addresses.h"
#include "dao/Module.h"

#include <cstddef>

namespace dao::gda
{

namespace
{

// The tables' keeper's getters: __thiscall, (table, row, column hash, out),
// `ret 0x10`, true if the cell was there.
template <class T> std::optional<T> Get(std::uintptr_t getter, std::int32_t table, std::int32_t row, std::uint32_t hash)
{
    void *tables = *At<void *>(addr::kGdaTables);
    // The keeper makes its inner store (+4) on first use; the getters read
    // through it without a check.
    if (tables == nullptr || *reinterpret_cast<void *const *>(static_cast<const std::byte *>(tables) + 4) == nullptr)
    {
        return std::nullopt;
    }
    T value{};
    using Fn = bool(__fastcall *)(void *, int, std::int32_t, std::int32_t, std::uint32_t, T *);
    const bool found = reinterpret_cast<Fn>(Module::Address(getter))(tables, 0, table, row, hash, &value);
    return found ? std::optional{value} : std::nullopt;
}

} // namespace

std::optional<std::int32_t> GetInt(std::int32_t table, std::int32_t row, std::uint32_t columnHash)
{
    return Get<std::int32_t>(addr::kGdaTables_GetInt, table, row, columnHash);
}

std::optional<float> GetFloat(std::int32_t table, std::int32_t row, std::uint32_t columnHash)
{
    return Get<float>(addr::kGdaTables_GetFloat, table, row, columnHash);
}

} // namespace dao::gda
