#include "dao/Script.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"

#include <Windows.h>

#include <cstddef>
#include <cstring>

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

OwnedEvent Stack::PopEvent()
{
    return OwnedEvent{static_cast<CScriptEvent *>(PopStructure(EngineStructure::Event))};
}

Array Stack::PopArray()
{
    core::ArrayRep *popped = nullptr;
    if (!CallVm<core::ArrayRep **>(addr::kCVirtualMachine_StackPopArray, &popped))
    {
        return Array{};
    }
    return Array{popped};
}

Array::~Array()
{
    if (array_ == nullptr)
    {
        return;
    }
    // As GetTeam frees its own: the destructor, which drops the payload's
    // reference, then the engine's operator delete for the 8 bytes.
    using Destructor = void(__fastcall *)(core::ArrayRep *, int);
    reinterpret_cast<Destructor>(Module::Address(addr::kScriptArray_Destructor))(array_, 0);
    using Delete = void(__cdecl *)(void *);
    reinterpret_cast<Delete>(Module::Address(addr::kOperatorDelete))(array_);
}

std::uint32_t Array::Element(core::ArrayTag kind, std::size_t index) const
{
    if (array_ == nullptr || array_->tag != static_cast<std::int32_t>(kind))
    {
        return 0;
    }
    const auto elements = array_->Elements();
    return index < elements.size() ? elements[index] : 0;
}

std::int32_t Array::Int(std::size_t index) const
{
    return static_cast<std::int32_t>(Element(core::ArrayTag::Int, index));
}

float Array::Float(std::size_t index) const
{
    const std::uint32_t bits = Element(core::ArrayTag::Float, index);
    float value = 0;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

ObjectId Array::Object(std::size_t index) const
{
    if (array_ == nullptr || array_->tag != static_cast<std::int32_t>(core::ArrayTag::Object) ||
        index >= array_->Elements().size())
    {
        return kInvalidObject;
    }
    return array_->Elements()[index];
}

std::wstring_view Array::String(std::size_t index) const
{
    const auto *string = reinterpret_cast<const core::WStringRep *>(Element(core::ArrayTag::String, index));
    return string != nullptr ? string->View() : std::wstring_view{};
}

const void *Array::Structure(std::size_t index) const
{
    if (array_ == nullptr || core::ArrayStructure(array_->tag) < 0)
    {
        return nullptr;
    }
    const auto elements = array_->Elements();
    return index < elements.size() ? reinterpret_cast<const void *>(elements[index]) : nullptr;
}

const CGameEffect *Array::Effect(std::size_t index) const
{
    return core::ArrayStructure(Tag()) == static_cast<std::int32_t>(EngineStructure::Effect)
               ? static_cast<const CGameEffect *>(Structure(index))
               : nullptr;
}

const CScriptEvent *Array::Event(std::size_t index) const
{
    return core::ArrayStructure(Tag()) == static_cast<std::int32_t>(EngineStructure::Event)
               ? static_cast<const CScriptEvent *>(Structure(index))
               : nullptr;
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

bool Stack::PushEvent(const CScriptEvent &value)
{
    return PushStructure(EngineStructure::Event, &value);
}

bool Stack::PushArray(const Array &value)
{
    return value.get() != nullptr && CallVm<const core::ArrayRep *>(addr::kCVirtualMachine_StackPushArray, value.get());
}

namespace
{

// The commands object's OBJECT_SELF (+8) and whether it is set (+4).
constexpr std::size_t kSelfValid = 4;
constexpr std::size_t kSelf = 8;
// The VM's stack object, whose first word is its depth.
constexpr std::size_t kStack = 0x1738;

std::byte *CommandsObject()
{
    auto *app = CClientExoApp::Get();
    return app != nullptr ? static_cast<std::byte *>(app->Commands()) : nullptr;
}

} // namespace

ObjectId Self()
{
    const std::byte *commands = CommandsObject();
    return commands != nullptr ? *reinterpret_cast<const ObjectId *>(commands + kSelf) : kInvalidObject;
}

ScopedSelf::ScopedSelf(ObjectId self)
{
    std::byte *commands = CommandsObject();
    if (commands == nullptr)
    {
        return;
    }
    hadSelf_ = *reinterpret_cast<std::int32_t *>(commands + kSelfValid);
    previous_ = *reinterpret_cast<ObjectId *>(commands + kSelf);
    *reinterpret_cast<std::int32_t *>(commands + kSelfValid) = 1;
    *reinterpret_cast<ObjectId *>(commands + kSelf) = self;
}

ScopedSelf::~ScopedSelf()
{
    std::byte *commands = CommandsObject();
    if (commands == nullptr)
    {
        return;
    }
    *reinterpret_cast<std::int32_t *>(commands + kSelfValid) = hadSelf_;
    *reinterpret_cast<ObjectId *>(commands + kSelf) = previous_;
}

std::int32_t StackDepth()
{
    const auto *vm = static_cast<const std::byte *>(VirtualMachine());
    return vm != nullptr ? *reinterpret_cast<const std::int32_t *>(vm + kStack) : 0;
}

void TruncateStack(std::int32_t depth)
{
    auto *vm = static_cast<std::byte *>(VirtualMachine());
    if (vm == nullptr || depth < 0 || depth >= StackDepth())
    {
        return;
    }
    using Fn = void(__fastcall *)(void *, int, std::int32_t);
    reinterpret_cast<Fn>(Module::Address(addr::kCVirtualMachineStack_Truncate))(vm + kStack, 0, depth);
}

namespace detail
{

bool Push(std::int32_t value)
{
    return Stack::PushInt(value);
}
bool Push(float value)
{
    return Stack::PushFloat(value);
}
bool Push(ObjectId value)
{
    return Stack::PushObject(value);
}
bool Push(std::wstring_view value)
{
    return Stack::PushString(value);
}
bool Push(const Vector &value)
{
    return Stack::PushVector(value);
}
bool Push(const CScriptLocation &value)
{
    return Stack::PushLocation(value);
}
bool Push(const CGameEffect &value)
{
    return Stack::PushEffect(value);
}
bool Push(const CScriptEvent &value)
{
    return Stack::PushEvent(value);
}
bool Push(const Array &value)
{
    return Stack::PushArray(value);
}
bool Push(const ::dao::Command &value)
{
    return Stack::PushStructure(EngineStructure::Command, &value);
}

template <> std::optional<std::int32_t> Pop<std::int32_t>()
{
    std::int32_t value = 0;
    return Stack::PopInt(value) ? std::optional{value} : std::nullopt;
}
template <> std::optional<float> Pop<float>()
{
    float value = 0;
    return Stack::PopFloat(value) ? std::optional{value} : std::nullopt;
}
template <> std::optional<ObjectId> Pop<ObjectId>()
{
    ObjectId value = kInvalidObject;
    return Stack::PopObject(value) ? std::optional{value} : std::nullopt;
}
template <> std::optional<std::wstring> Pop<std::wstring>()
{
    std::wstring value;
    return Stack::PopString(value) ? std::optional{std::move(value)} : std::nullopt;
}
template <> std::optional<Vector> Pop<Vector>()
{
    Vector value;
    return Stack::PopVector(value) ? std::optional{value} : std::nullopt;
}
template <> std::optional<CScriptLocation> Pop<CScriptLocation>()
{
    CScriptLocation value;
    return Stack::PopLocation(value) ? std::optional{value} : std::nullopt;
}
template <> std::optional<OwnedEffect> Pop<OwnedEffect>()
{
    OwnedEffect value = Stack::PopEffect();
    return value ? std::optional{std::move(value)} : std::nullopt;
}
template <> std::optional<OwnedEvent> Pop<OwnedEvent>()
{
    OwnedEvent value = Stack::PopEvent();
    return value ? std::optional{std::move(value)} : std::nullopt;
}
template <> std::optional<OwnedCommand> Pop<OwnedCommand>()
{
    OwnedCommand value{static_cast<::dao::Command *>(Stack::PopStructure(EngineStructure::Command))};
    return value ? std::optional{std::move(value)} : std::nullopt;
}
template <> std::optional<Array> Pop<Array>()
{
    Array value = Stack::PopArray();
    return value ? std::optional{std::move(value)} : std::nullopt;
}

int Run(int command, int paramCount)
{
    Handler handler = Get(command);
    return handler != nullptr ? Call(handler, command, paramCount) : kInvalidCommand;
}

} // namespace detail

} // namespace dao::script
