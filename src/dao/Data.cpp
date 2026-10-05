#include "dao/Data.h"

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/String.h"
#include "dao/core/Msvc8.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace dao::data
{

namespace
{

template <class Fn> Fn Slot(const void *object, std::size_t slot)
{
    return (*static_cast<Fn *const *>(object))[slot];
}

} // namespace

Attribute *Find(std::wstring_view path)
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
    using FindFn = Attribute *(__fastcall *)(void *, int, const core::WStringRep *, std::int32_t);
    Attribute *found = reinterpret_cast<FindFn>(Module::Address(addr::kTokenManager_Find))(root, 0, text.Engine(), 0);
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

Attribute::Type Attribute::Kind() const
{
    const auto vtable = *reinterpret_cast<const std::uintptr_t *>(this);
    const auto is = [vtable](AddressId id) { return vtable == Module::Address(id); };
    if (is(addr::kAttributeBool_Vftable))
    {
        return Type::Bool;
    }
    if (is(addr::kAttributeInt_Vftable))
    {
        return Type::Int;
    }
    if (is(addr::kAttributeUnsignedInt_Vftable))
    {
        return Type::UnsignedInt;
    }
    if (is(addr::kAttributeShort_Vftable))
    {
        return Type::Short;
    }
    if (is(addr::kAttributeByte_Vftable))
    {
        return Type::Byte;
    }
    if (is(addr::kAttributeFloat_Vftable) || is(addr::kAttributeFloatCustom_Vftable))
    {
        return Type::Float;
    }
    return Type::Other;
}

namespace
{

// Slot 21, Set(T): the value in the low bytes of its one stack word.
bool SetWord(Attribute *attribute, std::uint32_t word)
{
    using Fn = void(__fastcall *)(Attribute *, int, std::uint32_t);
    Slot<Fn>(attribute, 21)(attribute, 0, word);
    return true;
}

} // namespace

bool Attribute::Set(bool value)
{
    return Kind() == Type::Bool && SetWord(this, value ? 1U : 0U);
}

bool Attribute::Set(std::int32_t value)
{
    switch (Kind())
    {
    case Type::Int:
        return SetWord(this, static_cast<std::uint32_t>(value));
    case Type::Short:
        return SetWord(this, static_cast<std::uint16_t>(value));
    case Type::Byte:
        return SetWord(this, static_cast<std::uint8_t>(value));
    default:
        return false;
    }
}

bool Attribute::Set(std::uint32_t value)
{
    return Kind() == Type::UnsignedInt && SetWord(this, value);
}

bool Attribute::Set(float value)
{
    if (Kind() != Type::Float)
    {
        return false;
    }
    using Fn = void(__fastcall *)(Attribute *, int, float);
    Slot<Fn>(this, 21)(this, 0, value);
    return true;
}

std::size_t Attribute::MemberCount() const
{
    using Fn = std::int32_t(__fastcall *)(const Attribute *, int);
    const std::int32_t count = Slot<Fn>(this, 16)(this, 0);
    return count > 0 ? static_cast<std::size_t>(count) : 0;
}

std::vector<std::wstring> Attribute::MemberNames() const
{
    std::vector<std::wstring> names;
    // As SmartData::MemberIterator does: the attribute's own iterator, made
    // on the engine's heap and freed by its own slot 0.
    using MakeFn = void *(__fastcall *)(const Attribute *, int);
    void *iterator = Slot<MakeFn>(this, 17)(this, 0);
    if (iterator == nullptr)
    {
        return names;
    }
    using AtMemberFn = bool(__fastcall *)(void *, int);
    using StepFn = void(__fastcall *)(void *, int);
    using KeyFn = std::uint32_t(__fastcall *)(void *, int);
    using NameFn = const core::WStringRep *(__fastcall *)(const Attribute *, int, std::uint32_t);
    using FreeFn = void(__fastcall *)(void *, int, std::int32_t);
    // Past this many, something is wrong: a screen's list has dozens.
    constexpr std::size_t kMost = 100000;
    for (std::size_t i = 0; i < kMost && Slot<AtMemberFn>(iterator, 2)(iterator, 0); ++i)
    {
        const std::uint32_t key = Slot<KeyFn>(iterator, 3)(iterator, 0);
        if (const core::WStringRep *name = Slot<NameFn>(this, 7)(this, 0, key))
        {
            names.emplace_back(name->View());
        }
        Slot<StepFn>(iterator, 1)(iterator, 0);
    }
    Slot<FreeFn>(iterator, 0)(iterator, 0, 1);
    return names;
}

void Attribute::SetText(std::wstring_view value)
{
    // Slot 10 takes the engine's own string, as SetValueUnicode passes it.
    WString text(value);
    using Fn = void(__fastcall *)(Attribute *, int, const core::WStringRep *);
    Slot<Fn>(this, 10)(this, 0, text.Engine());
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

std::optional<std::size_t> MemberCount(std::wstring_view path)
{
    const Attribute *attribute = Find(path);
    return attribute != nullptr ? std::optional{attribute->MemberCount()} : std::nullopt;
}

std::vector<std::wstring> MemberNames(std::wstring_view path)
{
    const Attribute *attribute = Find(path);
    return attribute != nullptr ? attribute->MemberNames() : std::vector<std::wstring>{};
}

std::vector<std::wstring> Members(std::wstring_view path)
{
    std::vector<std::wstring> members;
    const Attribute *attribute = Find(path);
    if (attribute == nullptr)
    {
        return members;
    }
    for (const std::wstring &name : attribute->MemberNames())
    {
        std::wstring member = std::wstring(path) + L"." + name;
        if (Find(member) != nullptr)
        {
            members.push_back(std::move(member));
        }
    }
    return members;
}

} // namespace dao::data
