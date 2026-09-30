#include "dao/Data.h"

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/String.h"

#include <cstddef>
#include <cstdint>

namespace dao::data
{

namespace
{

template <class Fn> Fn Slot(const void *object, std::size_t slot)
{
    return (*static_cast<Fn *const *>(object))[slot];
}

} // namespace

const Attribute *Find(std::wstring_view path)
{
    // The root, made on first use: __cdecl, no arguments.
    using RootFn = void *(__cdecl *)();
    void *root = reinterpret_cast<RootFn>(Module::Address(addr::kTokenManager_Get))();
    if (root == nullptr)
    {
        return nullptr;
    }
    // Find takes the engine's own string; the flag is what GetValue passes.
    WString text(path);
    using FindFn = const Attribute *(__fastcall *)(void *, int, const core::WStringRep *, std::int32_t);
    const Attribute *found =
        reinterpret_cast<FindFn>(Module::Address(addr::kTokenManager_Find))(root, 0, text.Engine(), 0);
    const auto *null = reinterpret_cast<const Attribute *>(Module::Address(addr::kNullAttribute));
    return found != null ? found : nullptr;
}

std::wstring Attribute::Text() const
{
    // Slot 12 returns a std::wstring by value, made in the caller's space.
    WString value;
    using Fn = core::WStringRep *(__fastcall *)(const Attribute *, int, core::WStringRep *);
    Slot<Fn>(this, 12)(this, 0, value.Engine());
    return std::wstring(value.View());
}

double Attribute::Number() const
{
    using Fn = double(__fastcall *)(const Attribute *, int);
    return Slot<Fn>(this, 13)(this, 0);
}

std::optional<std::wstring> Text(std::wstring_view path)
{
    const Attribute *attribute = Find(path);
    return attribute != nullptr ? std::optional{attribute->Text()} : std::nullopt;
}

std::optional<double> Number(std::wstring_view path)
{
    const Attribute *attribute = Find(path);
    return attribute != nullptr ? std::optional{attribute->Number()} : std::nullopt;
}

} // namespace dao::data
