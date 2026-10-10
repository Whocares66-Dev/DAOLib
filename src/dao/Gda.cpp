// The game's data tables, read through the engine's own getters.
//
// A table is one of the scripts' TABLE_* numbers (dao::script::Table, from
// 2da_constants_h.nss): an M2DA, every file of the game's 2da.erf whose
// name starts with the table's prefix (m2da_base.gda: 1 is "ABI_", 6
// "BITM_"). dev/gda_tables.csv lists each file's table and columns with
// their types (tools/gda.py). A row is the row's id, as scripts pass it to
// GetM2DAInt; a column is its name, hashed as the game does
// (dao/core/GdaHash.h). The same values are the scripts' GetM2DAInt and
// GetM2DAFloat, which dao::script::api also reaches; these go straight to
// the table, with no script machinery.
//
// Read 2026-09-30: the engine keeps its tables behind the object in the
// global at 0x00C1D090; its getters find a table by number and call the
// table's typed getter (C2DASource's vtable, which M2DA and G2DA fill):
// slot 10 for an int, which GetM2DAInt uses and GameModeExplore's ability
// handler reads TargetType with, slot 9 for a float, which GetM2DAFloat
// uses, slot 8 for a string, which GetM2DAString uses, and slot 11 for a
// bool. The row and column counts are the table's slots 5 and 4, as
// GetM2DARows and GetM2DAColumns have them. A string or bool asked of a
// column of another type gets what the engine gives: whatever the table's
// conversion makes of it, as GetM2DAString would.
#include "dao/Gda.h"

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/String.h"

#include <cstddef>

namespace dao::gda
{

namespace
{

// The keeper, or null until it has made its inner store (+4), which it
// does on first use; the getters read through the store without a check.
void *Keeper()
{
    void *tables = *At<void *>(addr::kGdaTables);
    if (tables == nullptr || *reinterpret_cast<void *const *>(static_cast<const std::byte *>(tables) + 4) == nullptr)
    {
        return nullptr;
    }
    return tables;
}

// The keeper's getters: __thiscall, (table, row, column hash, out),
// `ret 0x10`, true if the cell was there.
template <class T> bool Call(AddressId getter, std::int32_t table, std::int32_t row, std::uint32_t hash, T *out)
{
    void *tables = Keeper();
    if (tables == nullptr)
    {
        return false;
    }
    using Fn = bool(__fastcall *)(void *, int, std::int32_t, std::int32_t, std::uint32_t, T *);
    return reinterpret_cast<Fn>(Module::Address(getter))(tables, 0, table, row, hash, out);
}

template <class T> std::optional<T> Get(AddressId getter, std::int32_t table, std::int32_t row, std::uint32_t hash)
{
    T value{};
    return Call(getter, table, row, hash, &value) ? std::optional{value} : std::nullopt;
}

// A table's count through a slot with no arguments.
std::optional<std::int32_t> Count(std::int32_t table, std::size_t slot)
{
    void *tables = Keeper();
    if (tables == nullptr)
    {
        return std::nullopt;
    }
    void *store = *reinterpret_cast<void **>(static_cast<std::byte *>(tables) + 4);
    using FindFn = void *(__fastcall *)(void *, int, std::int32_t);
    void *found = reinterpret_cast<FindFn>(Module::Address(addr::kGdaStore_FindTable))(store, 0, table);
    if (found == nullptr)
    {
        return std::nullopt;
    }
    using CountFn = std::int32_t(__fastcall *)(void *, int);
    const auto *vtable = *static_cast<CountFn *const *>(found);
    return vtable[slot](found, 0);
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

std::optional<std::wstring> GetString(std::int32_t table, std::int32_t row, std::uint32_t columnHash)
{
    // The engine assigns into the string, so it is the engine's own.
    WString value;
    if (!Call(addr::kGdaTables_GetString, table, row, columnHash, value.Engine()))
    {
        return std::nullopt;
    }
    return std::wstring(value.View());
}

std::optional<bool> GetBool(std::int32_t table, std::int32_t row, std::uint32_t columnHash)
{
    return Get<bool>(addr::kGdaTables_GetBool, table, row, columnHash);
}

std::optional<std::int32_t> Rows(std::int32_t table)
{
    return Count(table, 5);
}

std::optional<std::int32_t> Columns(std::int32_t table)
{
    return Count(table, 4);
}

} // namespace dao::gda
