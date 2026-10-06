#include "dao/ScriptHooks.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/core/Msvc8.h"
#include "dao/core/VmStack.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

namespace dao::script
{

namespace
{

// The VM's stack, as dao/core/VmStack.h reads it (offsets from the VM).
constexpr std::size_t kDepth = 0x1738;
constexpr std::size_t kTags = 0x1744;
constexpr std::size_t kValues = 0x1748;

core::VmStackView VmStack()
{
    const auto *vm = *At<const std::byte *>(addr::kVirtualMachine);
    if (vm == nullptr)
    {
        return {};
    }
    return {*reinterpret_cast<const std::int32_t *>(vm + kDepth),
            *reinterpret_cast<const std::int32_t *const *>(vm + kTags),
            *reinterpret_cast<const std::uint32_t *const *>(vm + kValues)};
}

// -- the table's readiness -------------------------------------------------

std::mutex g_readyMutex;
std::vector<std::function<void()>> g_waiting;
bool g_ready = false;
std::atomic<std::uint32_t> g_scriptThread{0};

using InitializeCommandsFn = void(__fastcall *)(void *self, int edx);
Detour<InitializeCommandsFn> g_initializeCommands;

void __fastcall InitializeCommands(void *self, int edx)
{
    g_initializeCommands.Original()(self, edx);
    g_scriptThread = GetCurrentThreadId();
    std::vector<std::function<void()>> run;
    {
        const std::lock_guard lock(g_readyMutex);
        g_ready = true;
        run.swap(g_waiting);
    }
    for (auto &ready : run)
    {
        ::dao::detail::Guard("a commands-ready callback", ready);
    }
}

// -- the intercepts ----------------------------------------------------------

struct Chain
{
    std::vector<Intercept> intercepts;
    Handler previous = nullptr; // what was in the table: the end of this module's chain
    bool installed = false;
};
std::array<Chain, kSlots> g_chains;

int __fastcall Trampoline(void *commands, int edx, int id, int paramCount);

void Install(int command)
{
    Chain &chain = g_chains.at(static_cast<std::size_t>(command));
    if (chain.installed)
    {
        return;
    }
    chain.previous = Set(command, &Trampoline);
    chain.installed = true;
}

int RunLink(int command, int paramCount, int link, void *commands, int edx)
{
    Chain &chain = g_chains.at(static_cast<std::size_t>(command));
    if (std::cmp_less(link, chain.intercepts.size()))
    {
        CommandCall call(command, paramCount, link);
        int result = kInvalidCommand;
        if (::dao::detail::Guard("a command intercept",
                                 [&] { result = chain.intercepts.at(static_cast<std::size_t>(link))(call); }))
        {
            return result;
        }
        // It threw: the game's own command answers, run now if the
        // intercept had not run it yet, else as it answered then. The
        // arguments are still on the stack until then: an intercept reads
        // them without popping.
        return call.Proceeded() ? call.ProceededResult() : call.Proceed();
    }
    return chain.previous != nullptr ? chain.previous(commands, edx, command, paramCount) : kInvalidCommand;
}

int __fastcall Trampoline(void *commands, int edx, int id, int paramCount)
{
    return RunLink(id, paramCount, 0, commands, edx);
}

// Pops the answer on top and pushes `value`, if the top is of `tag`.
template <class T, class PopFn, class PushFn> bool Replace(core::VmTag tag, T value, PopFn pop, PushFn push)
{
    if (VmStack().Tag(0) != static_cast<std::int32_t>(tag))
    {
        return false;
    }
    T old{};
    return pop(old) && push(value);
}

} // namespace

void OnCommandsReady(std::function<void()> ready)
{
    {
        const std::lock_guard lock(g_readyMutex);
        if (!g_ready && Table() == nullptr)
        {
            if (g_initializeCommands.Original() == nullptr)
            {
                g_initializeCommands.Attach(addr::kCVirtualMachineCommands_InitializeCommands, &InitializeCommands);
            }
            g_waiting.push_back(std::move(ready));
            return;
        }
        g_ready = true;
    }
    ready();
}

bool CommandsReady()
{
    const std::lock_guard lock(g_readyMutex);
    return g_ready;
}

std::uint32_t ScriptThreadId()
{
    return g_scriptThread;
}

bool InterceptCommand(int command, Intercept intercept)
{
    if (command < 0 || command >= kSlots || !intercept)
    {
        return false;
    }
    Chain &chain = g_chains.at(static_cast<std::size_t>(command));
    chain.intercepts.push_back(std::move(intercept));
    OnCommandsReady([command] { Install(command); });
    return true;
}

// -- CommandCall ---------------------------------------------------------------

// A call's arguments and answer are on the script VM's stack, not in the
// CommandCall, so most of these do not touch `this`. They stay members all
// the same: a CommandCall exists only while a command is intercepted, and a
// static reader could be called with no command running (DAOLib #33).
// NOLINTBEGIN(readability-convert-member-functions-to-static)

std::optional<std::int32_t> CommandCall::Int(int n) const
{
    const auto word = VmStack().Word(n, static_cast<std::int32_t>(core::VmTag::Int));
    return word ? std::optional{static_cast<std::int32_t>(*word)} : std::nullopt;
}

std::optional<float> CommandCall::Float(int n) const
{
    const auto word = VmStack().Word(n, static_cast<std::int32_t>(core::VmTag::Float));
    if (!word)
    {
        return std::nullopt;
    }
    float value = 0;
    std::memcpy(&value, &*word, sizeof value);
    return value;
}

std::optional<ObjectId> CommandCall::Object(int n) const
{
    const auto word = VmStack().Word(n, static_cast<std::int32_t>(core::VmTag::Object));
    return word ? std::optional{static_cast<ObjectId>(*word)} : std::nullopt;
}

namespace
{

// The n-th argument's word on the VM's stack, to write, if its tag is
// `tag`: VmStack's view, the values array being the VM's own.
std::uint32_t *ArgumentWord(int n, core::VmTag tag)
{
    const core::VmStackView stack = VmStack();
    const auto i = stack.Index(n);
    if (!i || stack.tags[*i] != static_cast<std::int32_t>(tag))
    {
        return nullptr;
    }
    return const_cast<std::uint32_t *>(stack.values) + *i;
}

} // namespace

bool CommandCall::SetInt(int n, std::int32_t value) const
{
    std::uint32_t *word = proceeded_ ? nullptr : ArgumentWord(n, core::VmTag::Int);
    if (word == nullptr)
    {
        return false;
    }
    *word = static_cast<std::uint32_t>(value);
    return true;
}

bool CommandCall::SetFloat(int n, float value) const
{
    std::uint32_t *word = proceeded_ ? nullptr : ArgumentWord(n, core::VmTag::Float);
    if (word == nullptr)
    {
        return false;
    }
    std::memcpy(word, &value, sizeof value);
    return true;
}

bool CommandCall::SetObject(int n, ObjectId value) const
{
    std::uint32_t *word = proceeded_ ? nullptr : ArgumentWord(n, core::VmTag::Object);
    if (word == nullptr)
    {
        return false;
    }
    *word = value;
    return true;
}

std::optional<std::wstring_view> CommandCall::String(int n) const
{
    const auto word = VmStack().Word(n, static_cast<std::int32_t>(core::VmTag::String));
    if (!word || *word == 0)
    {
        return std::nullopt;
    }
    return reinterpret_cast<const core::WStringRep *>(*word)->View();
}

namespace
{
const void *StructureArgument(int n, EngineStructure type)
{
    const auto word =
        VmStack().Word(n, static_cast<std::int32_t>(core::VmTag::Structure) + static_cast<std::int32_t>(type));
    return word ? reinterpret_cast<const void *>(*word) : nullptr;
}
} // namespace

const CScriptLocation *CommandCall::Location(int n) const
{
    return static_cast<const CScriptLocation *>(StructureArgument(n, EngineStructure::Location));
}

const CGameEffect *CommandCall::Effect(int n) const
{
    return static_cast<const CGameEffect *>(StructureArgument(n, EngineStructure::Effect));
}

const CScriptEvent *CommandCall::Event(int n) const
{
    return static_cast<const CScriptEvent *>(StructureArgument(n, EngineStructure::Event));
}

const ::dao::Command *CommandCall::Command(int n) const
{
    return static_cast<const ::dao::Command *>(StructureArgument(n, EngineStructure::Command));
}

int CommandCall::Proceed()
{
    proceeded_ = true;
    auto *app = CClientExoApp::Get();
    result_ = RunLink(command_, paramCount_, link_ + 1, app != nullptr ? app->Commands() : nullptr, 0);
    return result_;
}

std::optional<std::int32_t> CommandCall::IntAnswer() const
{
    return Int(0);
}

std::optional<float> CommandCall::FloatAnswer() const
{
    return Float(0);
}

std::optional<ObjectId> CommandCall::ObjectAnswer() const
{
    return Object(0);
}

bool CommandCall::SetIntAnswer(std::int32_t value)
{
    return Replace(core::VmTag::Int, value, &Stack::PopInt, &Stack::PushInt);
}

bool CommandCall::SetFloatAnswer(float value)
{
    return Replace(core::VmTag::Float, value, &Stack::PopFloat, &Stack::PushFloat);
}

bool CommandCall::SetObjectAnswer(ObjectId value)
{
    return Replace(core::VmTag::Object, value, &Stack::PopObject, &Stack::PushObject);
}

// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace dao::script
