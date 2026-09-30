#include "dao/Tlk.h"

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/String.h"

#include <cstddef>

namespace dao::tlk
{

std::wstring String(std::uint32_t strref, bool female)
{
    void *table = *At<void *>(addr::kTlkTable);
    if (table == nullptr)
    {
        return {};
    }
    // The script command sets the gender byte for its call and leaves it;
    // this puts it back.
    auto &gender = *(static_cast<std::byte *>(table) + 0x1C);
    const std::byte before = gender;
    gender = female ? std::byte{1} : std::byte{0};
    WString text;
    using Fn = void(__fastcall *)(void *, int, std::uint32_t, core::WStringRep *, std::int32_t);
    reinterpret_cast<Fn>(Module::Address(addr::kCTlkTable_GetString))(table, 0, strref, text.Engine(), 0);
    gender = before;
    return std::wstring(text.View());
}

} // namespace dao::tlk
