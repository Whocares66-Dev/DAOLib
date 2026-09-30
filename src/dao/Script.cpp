#include "dao/Script.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"

#include <Windows.h>

#include <cstddef>

namespace dao::script
{

namespace
{

// The table pointer inside the commands object (Register's `[ecx + 0xC]`).
constexpr std::size_t kTableOffset = 0xC;

void *VirtualMachine()
{
    return *At<void *>(addr::kVirtualMachine);
}

// The VM's pops and pushes: BOOL, __thiscall on the VM, one argument, `ret 4`.
template <class Arg> bool CallVm(std::uintptr_t function, Arg arg)
{
    void *vm = VirtualMachine();
    if (vm == nullptr)
    {
        return false;
    }
    using Fn = BOOL(__fastcall *)(void *, int, Arg);
    return reinterpret_cast<Fn>(Module::Address(function))(vm, 0, arg) != FALSE;
}

template <class A, class B> bool CallVm(std::uintptr_t function, A a, B b)
{
    void *vm = VirtualMachine();
    if (vm == nullptr)
    {
        return false;
    }
    using Fn = BOOL(__fastcall *)(void *, int, A, B);
    return reinterpret_cast<Fn>(Module::Address(function))(vm, 0, a, b) != FALSE;
}

} // namespace

void DestroyStructure(EngineStructure type, void *structure)
{
    const auto *app = CClientExoApp::Get();
    if (app == nullptr || structure == nullptr)
    {
        return;
    }
    using Fn = void(__fastcall *)(void *, int, std::int32_t, void *);
    reinterpret_cast<Fn>(Module::Address(addr::kCVirtualMachineCommands_DestroyGameDefinedStructure))(
        app->Commands(), 0, static_cast<std::int32_t>(type), structure);
}

Handler *Table()
{
    const auto *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return nullptr;
    }
    const auto *commands = static_cast<const std::byte *>(app->Commands());
    if (commands == nullptr)
    {
        return nullptr;
    }
    const auto table = TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(commands + kTableOffset));
    return table ? reinterpret_cast<Handler *>(*table) : nullptr;
}

Handler Get(int id)
{
    Handler *table = Table();
    if (table == nullptr || id < 0 || id >= kSlots)
    {
        return nullptr;
    }
    return table[id];
}

Handler Set(int id, Handler handler)
{
    Handler *table = Table();
    if (table == nullptr || id < 0 || id >= kSlots)
    {
        return nullptr;
    }
    static_assert(sizeof(Handler) == sizeof(LONG), "a handler is one 32-bit word");
    const LONG previous = InterlockedExchange(reinterpret_cast<volatile LONG *>(&table[id]),
                                              static_cast<LONG>(reinterpret_cast<std::uintptr_t>(handler)));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(previous));
}

int Call(Handler handler, int id, int paramCount)
{
    const auto *app = CClientExoApp::Get();
    return handler(app != nullptr ? app->Commands() : nullptr, 0, id, paramCount);
}

bool Stack::PopInt(std::int32_t &out)
{
    return CallVm<std::int32_t *>(addr::kCVirtualMachine_StackPopInteger, &out);
}

bool Stack::PopFloat(float &out)
{
    return CallVm<float *>(addr::kCVirtualMachine_StackPopFloat, &out);
}

bool Stack::PopObject(ObjectId &out)
{
    return CallVm<ObjectId *>(addr::kCVirtualMachine_StackPopObject, &out);
}

bool Stack::PopString(std::wstring &out)
{
    WString popped;
    if (!CallVm<core::WStringRep *>(addr::kCVirtualMachine_StackPopString, popped.Engine()))
    {
        return false;
    }
    out.assign(popped.View());
    return true;
}

bool Stack::PopVector(Vector &out)
{
    return CallVm<Vector *>(addr::kCVirtualMachine_StackPopVector, &out);
}

void *Stack::PopStructure(EngineStructure type)
{
    void *structure = nullptr;
    if (!CallVm<std::int32_t, void **>(addr::kCVirtualMachine_StackPopEngineStructure, static_cast<std::int32_t>(type),
                                       &structure))
    {
        return nullptr;
    }
    return structure;
}

bool Stack::PopLocation(CScriptLocation &out)
{
    void *popped = PopStructure(EngineStructure::Location);
    if (popped == nullptr)
    {
        return false;
    }
    out = *static_cast<const CScriptLocation *>(popped);
    DestroyStructure(EngineStructure::Location, popped);
    return true;
}

OwnedEffect Stack::PopEffect()
{
    return OwnedEffect{static_cast<CGameEffect *>(PopStructure(EngineStructure::Effect))};
}

bool Stack::PushInt(std::int32_t value)
{
    return CallVm<std::int32_t>(addr::kCVirtualMachine_StackPushInteger, value);
}

bool Stack::PushFloat(float value)
{
    return CallVm<float>(addr::kCVirtualMachine_StackPushFloat, value);
}

bool Stack::PushObject(ObjectId value)
{
    return CallVm<ObjectId>(addr::kCVirtualMachine_StackPushObject, value);
}

bool Stack::PushString(std::wstring_view value)
{
    // The VM copy-constructs its own from this, which only reads it.
    const core::WStringRep borrowed = core::WStringRep::Borrow(value);
    return CallVm<const core::WStringRep *>(addr::kCVirtualMachine_StackPushString, &borrowed);
}

bool Stack::PushVector(const Vector &value)
{
    return CallVm<const Vector *>(addr::kCVirtualMachine_StackPushVector, &value);
}

bool Stack::PushStructure(EngineStructure type, const void *value)
{
    return CallVm<std::int32_t, const void *>(addr::kCVirtualMachine_StackPushEngineStructure,
                                              static_cast<std::int32_t>(type), value);
}

bool Stack::PushLocation(const CScriptLocation &value)
{
    return PushStructure(EngineStructure::Location, &value);
}

bool Stack::PushEffect(const CGameEffect &value)
{
    return PushStructure(EngineStructure::Effect, &value);
}

} // namespace dao::script
