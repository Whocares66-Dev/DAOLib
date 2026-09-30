// The probe: the first plugin, there to prove the chain end to end -- the
// loader is in the process, a plugin's DAOLib_Load runs before the game,
// a detour on the engine holds, and what the engine hands a hook can be
// read (its type, through RTTI). Everything it learns goes to probe.log.
// It changes nothing about the game: every hook calls the original with
// the arguments it was given.
//
// What it watches:
//
// - Pause, the first thing a turn-based mode needs (dev/TURN_BASED.md): the
//   pause key's handler, and the setter every pause goes through, with who
//   called it. A session of play lists every source of pause -- the key,
//   auto-pause on combat, menus, dialogue -- by the address it came from.
// - Every script command and message handler, counted (Census.h), and
//   where the game's threads are, sampled (Recorder.h): what runs while
//   exploring, what while fighting, written beside probe.log for
//   tools/probe_report.py. dev/PROBE.md says how to read a session.
// - The library's fields, each checked against the script command it was
//   read from, every time a script calls it (ScriptChecks.h).
// - Every event the engine's queue delivers, counted by kind (Events.h).
#include "Census.h"
#include "Events.h"
#include "Recorder.h"
#include "ScriptChecks.h"
#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/Plugin.h"
#include "dao/SafeRead.h"
#include "dao/Script.h"
#include "dao/TypeName.h"
#include "dao/core/CallSite.h"

#include <Windows.h>

#include <intrin.h>

#include <array>
#include <atomic>

namespace
{

// GameModeController::HandleMessage(const UserTogglePauseMessage&): a
// __thiscall member returning bool. Written __fastcall so `this` arrives in
// ecx as the engine sends it; edx is the unused second register.
using HandleTogglePauseFn = bool(__fastcall *)(void *self, int edx, const void *message);
dao::Detour<HandleTogglePauseFn> g_handleTogglePause;

bool __fastcall HandleTogglePause(void *self, int edx, const void *message)
{
    spdlog::info("pause key: controller {} at {}, message {} at {}", dao::TypeName(self), self, dao::TypeName(message),
                 message);
    return g_handleTogglePause.Original()(self, edx, message);
}

// A crash on purpose, to test the crash logger (dev/CRASHES.md): with
// DAOLIB_PROBE_CRASH_TEST=1 in the environment, the first SetPaused -- the
// engine's own, at start-up, on the game's thread with engine frames below
// it -- writes to an address nothing is mapped at. Off otherwise.
bool g_crashTest = false;
volatile std::uintptr_t g_crashTarget = 0x10; // volatile: the analyser cannot see it is bad

void CrashIfTesting()
{
    if (g_crashTest)
    {
        spdlog::warn("DAOLIB_PROBE_CRASH_TEST: crashing now, on purpose");
        *reinterpret_cast<volatile std::uint32_t *>(g_crashTarget) = 0xDEAD;
    }
}

bool CrashTestRequested()
{
    std::array<char, 8> value{};
    std::size_t length = 0;
    return getenv_s(&length, value.data(), value.size(), "DAOLIB_PROBE_CRASH_TEST") == 0 && length > 1 &&
           value[0] == '1';
}

// CClientExoApp::SetPaused(bool paused, bool second): __thiscall, `ret 8`.
using SetPausedFn = void(__fastcall *)(dao::CClientExoApp *self, int edx, bool paused, bool second);
dao::Detour<SetPausedFn> g_setPaused;

void __fastcall SetPaused(dao::CClientExoApp *self, int edx, bool paused, bool second)
{
    // The address the call returns to, as a preferred-base address: the
    // caller, to look up in Ghidra.
    const auto caller =
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - dao::Module::Base() + dao::core::kPreferredBase;
    const bool before = self->IsPaused();
    CrashIfTesting();
    g_setPaused.Original()(self, edx, paused, second);
    spdlog::info("SetPaused({}, {}) from {:#010x}: paused {} -> {}, {} ms paused in total, app {} at {}", paused,
                 second, caller, before, self->IsPaused(), self->PausedMilliseconds(), dao::TypeName(self),
                 static_cast<void *>(self));
}

// The command queue's entry (dao/Addresses.h, kCCObject_QueueCommand):
// __thiscall, three arguments, `ret 0xC`, the command first, returning a
// value AddCommand hands back to its script. What the object and the
// command are, and what the two other arguments mean, is what this logs:
// their RTTI, the command's first words, the arguments, and the caller.
using QueueCommandFn = int(__fastcall *)(void *self, int edx, void *command, int second, int third);
dao::Detour<QueueCommandFn> g_queueCommand;

// The return addresses into the game's code above a hook's frame, innermost
// first, as preferred-base addresses: its caller, that one's caller, and on.
// Read by scanning up the stack from the hook's own return-address slot and
// keeping values just after a call instruction (dao/core/CallSite.h); the
// game has no frame data to walk instead. A stale value can slip in.
std::array<std::uint32_t, 4> CallerChain(const void *returnSlot)
{
    std::array<std::uint32_t, 4> chain{};
    static const auto code = dao::Module::Code();
    if (!code)
    {
        return chain;
    }
    std::size_t found = 0;
    auto at = reinterpret_cast<std::uintptr_t>(returnSlot);
    for (std::size_t i = 0; i < 1024 && found < chain.size(); ++i, at += 4)
    {
        const auto value = dao::TryRead<std::uint32_t>(at);
        if (!value)
        {
            break;
        }
        if (!code->Contains(*value) || *value < code->begin + 7)
        {
            continue;
        }
        std::array<std::byte, 7> before{};
        if (!dao::TryCopy(before.data(), *value - 7, before.size()) || !dao::core::EndsWithCall(before))
        {
            continue;
        }
        chain[found++] = dao::Module::Preferred(*value);
    }
    return chain;
}

int __fastcall QueueCommand(void *self, int edx, void *command, int second, int third)
{
    const auto chain = CallerChain(_AddressOfReturnAddress());
    const int result = g_queueCommand.Original()(self, edx, command, second, third);
    // All five words: the last is player issued (dao::Command).
    std::array<std::uint32_t, 5> words{};
    dao::TryCopy(words.data(), reinterpret_cast<std::uintptr_t>(command), sizeof words);
    spdlog::info("QueueCommand from {:#010x} via {:#010x} {:#010x} {:#010x}: object {} at {}, command {} at {} "
                 "[{:08x} {:08x} {:08x} {:08x} {:08x}], args {} {}, result {}",
                 chain[0], chain[1], chain[2], chain[3], dao::TypeName(self), self, dao::TypeName(command), command,
                 words[0], words[1], words[2], words[3], words[4], second, third, result);
    return result;
}

// The client command table's AddCommand (dao/Addresses.h,
// CClientVirtualMachineCommands): whether single-player runs a script on
// that table at all. Counted, the first calls and every 100th logged.
dao::Detour<dao::script::Handler> g_clientAddCommand;
std::atomic<std::uint32_t> g_clientAddCommands{0};

int __fastcall ClientAddCommand(void *commands, int edx, int id, int paramCount)
{
    const std::uint32_t n = ++g_clientAddCommands;
    if (n <= 5 || n % 100 == 0)
    {
        spdlog::info("client AddCommand, call {}, thread {}: commands {} at {}, command {}, {} parameters", n,
                     GetCurrentThreadId(), dao::TypeName(commands), commands, id, paramCount);
    }
    return g_clientAddCommand.Original()(commands, edx, id, paramCount);
}

template <class Fn> void Place(dao::Detour<Fn> &detour, std::uintptr_t address, Fn hook, const char *what)
{
    if (detour.Attach(address, hook))
    {
        spdlog::info("detoured {} at {:#010x}", what, address);
    }
    else
    {
        spdlog::error("could not detour {} at {:#010x}", what, address);
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("probe");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion)
    {
        spdlog::error("loader API {} is not {}", info != nullptr ? info->apiVersion : 0, dao::kPluginApiVersion);
        return false;
    }
    spdlog::info("exe at {:#x}, expected build: {}", dao::Module::Base(), info->expectedExecutable);
    if (!info->expectedExecutable)
    {
        return false;
    }
    g_crashTest = CrashTestRequested();
    if (g_crashTest)
    {
        spdlog::warn("DAOLIB_PROBE_CRASH_TEST is set: the first SetPaused will crash the game on purpose");
    }
    // No game object exists yet: the app is made on first use, after this.
    spdlog::info("CClientExoApp at load: {}", static_cast<void *>(dao::CClientExoApp::Get()));

    Place(g_handleTogglePause, dao::addr::kGameModeController_HandleUserTogglePause, &HandleTogglePause,
          "GameModeController::HandleMessage(UserTogglePauseMessage)");
    Place(g_setPaused, dao::addr::kCClientExoApp_SetPaused, &SetPaused, "CClientExoApp::SetPaused");
    Place(g_queueCommand, dao::addr::kCCObject_QueueCommand, &QueueCommand, "CCObject::QueueCommand");
    Place(g_clientAddCommand, dao::addr::kCClientVirtualMachineCommands_AddCommand, &ClientAddCommand,
          "CClientVirtualMachineCommands::AddCommand");
    probe::AttachScriptChecks();
    probe::AttachEventWatch();

    // The census, on everything but what is hooked above.
    const std::uint32_t hookedAbove[] = {
        static_cast<std::uint32_t>(dao::addr::kGameModeController_HandleUserTogglePause),
        static_cast<std::uint32_t>(dao::addr::kCClientExoApp_SetPaused),
        static_cast<std::uint32_t>(dao::addr::kCCObject_QueueCommand),
        static_cast<std::uint32_t>(dao::addr::kCClientVirtualMachineCommands_AddCommand),
    };
    const auto census = probe::AttachCensus(hookedAbove);
    spdlog::info("census: {} of {} functions counted, {} skipped (hooked above), {} refused by Detours",
                 census.attached, probe::Targets().size(), census.skipped, census.failed);
    probe::StartRecorder();
    return true;
}
