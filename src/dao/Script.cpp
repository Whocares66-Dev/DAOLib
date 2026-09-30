#include "dao/Script.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"

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

} // namespace

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

} // namespace dao::script
