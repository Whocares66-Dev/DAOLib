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
// - A spell's sub-actions while its caster conjures (Conjure.h).
// - Who advances the Flash movies, paused and not (Movies.h).
// - The HUD's movie records as they change, for the interface's fade (Hud.h).
// - A named resource's read, around a plugin's patch of it (Reads.h).
// - Every step of a loading screen, timed (Loading.h).
// - The colour curve the frame's last pass is given, as the device has it (ColourCurve.h).
#include "Census.h"
#include "ColourCurve.h"
#include "Conjure.h"
#include "Events.h"
#include "Hud.h"
#include "Loading.h"
#include "Movies.h"
#include "Reads.h"
#include "Recorder.h"
#include "ScriptChecks.h"
#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Gui.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/Plugin.h"
#include "dao/SafeRead.h"
#include "dao/Script.h"
#include "dao/String.h"
#include "dao/TypeName.h"
#include "dao/World.h"
#include "dao/core/CallSite.h"
#include "dao/core/Msvc8.h"

#include <Windows.h>

#include <intrin.h>

#include <array>
#include <atomic>
#include <cstdlib>
#include <string>

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
// it -- writes to an address nothing is mapped at. Off otherwise. With 2,
// the address space is filled first, the crash a long session ends in where
// the exe is not large-address-aware (Steam's, EA's): the crash logger must
// write its log in what it kept back (DAOLib #56).
enum class CrashTest : std::uint8_t
{
    kOff,
    kCrash,
    kFullThenCrash,
};
CrashTest g_crashTest = CrashTest::kOff;
volatile std::uintptr_t g_crashTarget = 0x10; // volatile: the analyser cannot see it is bad

// Every free block of address space a VirtualAlloc could still be given,
// reserved, the largest first: 64 KiB is the smallest it hands out. Nothing
// is committed, so no memory is used.
void FillAddressSpace()
{
    constexpr std::size_t kLargest = 0x40000000;
    constexpr std::size_t kSmallest = 0x10000;
    for (std::size_t size = kLargest; size >= kSmallest; size /= 2)
    {
        while (VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS) != nullptr)
        {
        }
    }
}

void CrashIfTesting()
{
    if (g_crashTest == CrashTest::kOff)
    {
        return;
    }
    spdlog::warn("DAOLIB_PROBE_CRASH_TEST: crashing now, on purpose");
    if (g_crashTest == CrashTest::kFullThenCrash)
    {
        FillAddressSpace();
    }
    *reinterpret_cast<volatile std::uint32_t *>(g_crashTarget) = 0xDEAD;
}

CrashTest CrashTestRequested()
{
    std::array<char, 8> value{};
    std::size_t length = 0;
    if (getenv_s(&length, value.data(), value.size(), "DAOLIB_PROBE_CRASH_TEST") != 0 || length <= 1)
    {
        return CrashTest::kOff;
    }
    switch (value[0])
    {
    case '1':
        return CrashTest::kCrash;
    case '2':
        return CrashTest::kFullThenCrash;
    default:
        return CrashTest::kOff;
    }
}

// CClientExoApp::SetPaused(bool paused, bool second): __thiscall, `ret 8`.
using SetPausedFn = void(__fastcall *)(dao::CClientExoApp *self, int edx, bool paused, bool second);
dao::Detour<SetPausedFn> g_setPaused;

void __fastcall SetPaused(dao::CClientExoApp *self, int edx, bool paused, bool second)
{
    // The address the call returns to, as a preferred-base address: the
    // caller, to look up in Ghidra. And as the function it is in, which is
    // what a plugin compares to tell callers apart in any build.
    const auto returnsTo = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto caller = dao::Module::Preferred(returnsTo);
    const auto function = dao::Module::Containing(returnsTo);
    const bool before = self->IsPaused();
    CrashIfTesting();
    g_setPaused.Original()(self, edx, paused, second);
    spdlog::info(
        "SetPaused({}, {}) from {:#010x} (ID {} + {:#x}): paused {} -> {}, {} ms paused in total, app {} at {}", paused,
        second, caller, function ? static_cast<std::uint32_t>(function->id) : 0, function ? function->offset : 0,
        before, self->IsPaused(), self->PausedMilliseconds(), dao::TypeName(self), static_cast<void *>(self));
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
std::array<std::uint32_t, 8> CallerChain(const void *returnSlot)
{
    std::array<std::uint32_t, 8> chain{};
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
    spdlog::debug("QueueCommand from {:#010x} via {:#010x} {:#010x} {:#010x}: object {} at {}, command {} at {} "
                  "[{:08x} {:08x} {:08x} {:08x} {:08x}], args {} {}, result {}",
                  chain[0], chain[1], chain[2], chain[3], dao::TypeName(self), self, dao::TypeName(command), command,
                  words[0], words[1], words[2], words[3], words[4], second, third, result);
    return result;
}

// A creature's animation events (dao/Addresses.h, kCCCreature_OnAnimationEvent;
// DAOLib #88): an attack's impact is event 1 of its animation, and with the
// camera some 60 m from a fight no impact reaches the scripts. Who raises
// the event, and whether any is raised out there, is what this logs: each
// impact event counted, the first ten and every tenth after with the
// creature and the callers above. It changes nothing.
using AnimationEventFn = char(__fastcall *)(void *creature, int edx, const std::uint32_t *event, char second);
dao::Detour<AnimationEventFn> g_animationEvent;
std::atomic<std::uint32_t> g_impactEvents{0};

char __fastcall AnimationEvent(void *creature, int edx, const std::uint32_t *event, char second)
{
    if (dao::TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(event)) == 1U)
    {
        const std::uint32_t n = ++g_impactEvents;
        if (n <= 3)
        {
            const auto chain = CallerChain(_AddressOfReturnAddress());
            spdlog::info("animation event 1, an attack's impact, call {}, thread {}: creature at {}, second {}, from "
                         "{:#010x} via {:#010x} {:#010x} {:#010x} {:#010x} {:#010x} {:#010x} {:#010x}",
                         n, GetCurrentThreadId(), creature, static_cast<int>(second), chain[0], chain[1], chain[2],
                         chain[3], chain[4], chain[5], chain[6], chain[7]);
        }
        else
        {
            spdlog::info("impact event {}", n);
        }
    }
    return g_animationEvent.Original()(creature, edx, event, second);
}

// The same event as the animation manager is given it (dao/Addresses.h,
// kAnimationManager_AddEvent): whether an impact is raised at all far from
// the camera, and by what, is the other half of the question. Counted as the
// handled ones are, the first three with their callers.
using AddAnimationEventFn = void(__fastcall *)(void *manager, int edx, const std::uint32_t *event, int flags);
dao::Detour<AddAnimationEventFn> g_addAnimationEvent;
std::atomic<std::uint32_t> g_impactsRaised{0};

void __fastcall AddAnimationEvent(void *manager, int edx, const std::uint32_t *event, int flags)
{
    if (dao::TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(event)) == 1U)
    {
        const std::uint32_t n = ++g_impactsRaised;
        if (n <= 3)
        {
            const auto chain = CallerChain(_AddressOfReturnAddress());
            spdlog::info("impact raised {}, thread {}, flags {}: from {:#010x} via {:#010x} {:#010x} {:#010x} "
                         "{:#010x} {:#010x} {:#010x} {:#010x}",
                         n, GetCurrentThreadId(), flags & 0xFF, chain[0], chain[1], chain[2], chain[3], chain[4],
                         chain[5], chain[6], chain[7]);
        }
        else
        {
            spdlog::info("impact raised {}", n);
        }
    }
    g_addAnimationEvent.Original()(manager, edx, event, flags);
}

// Who confines the game to a few processors: every copy of the game was seen
// with a process affinity of 3, the first two logical processors, so that
// several copies share one core while the rest idle (2026-10-12). The
// system's own SetProcessAffinityMask is detoured, and each call logged with
// its mask, what the process had before, and the callers above.
using SetProcessAffinityFn = BOOL(WINAPI *)(HANDLE, DWORD_PTR);
dao::Detour<SetProcessAffinityFn> g_setProcessAffinity;

BOOL WINAPI SetProcessAffinity(HANDLE process, DWORD_PTR mask)
{
    DWORD_PTR had = 0;
    DWORD_PTR system = 0;
    GetProcessAffinityMask(GetCurrentProcess(), &had, &system);
    const auto from = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    HMODULE module = nullptr;
    std::array<char, MAX_PATH> name{};
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(from), &module) != 0)
    {
        GetModuleFileNameA(module, name.data(), MAX_PATH);
    }
    const auto chain = CallerChain(_AddressOfReturnAddress());
    spdlog::info("SetProcessAffinityMask({:#x}) on thread {}: the process had {:#x} of the system's {:#x}; from {:#x} "
                 "in {}, the game's callers above: {:#010x} {:#010x} {:#010x} {:#010x}",
                 mask, GetCurrentThreadId(), had, system, from, name.data(), chain[0], chain[1], chain[2], chain[3]);
    return g_setProcessAffinity.Original()(process, mask);
}

// An experiment for DAOLib #88, off unless DAOLIB_PROBE_CREATURE_SHOW is set
// to a distance in metres: the library holds the distance from the camera
// within which the game shows a creature at it (dao/World.h,
// SetCreatureShowDistance; 60 or less as the game has it), to see whether a
// blow then lands farther off. The one thing the probe changes.
void WidenCreatureShow()
{
    std::array<char, 32> text{};
    const DWORD length = GetEnvironmentVariableA("DAOLIB_PROBE_CREATURE_SHOW", text.data(), 32);
    const float wanted = length != 0 && length < text.size() ? std::strtof(text.data(), nullptr) : 0.0F;
    if (wanted <= 0.0F)
    {
        return;
    }
    const auto was = dao::world::CreatureShowDistance();
    spdlog::warn("DAOLIB_PROBE_CREATURE_SHOW is set: creatures shown within {} m of the camera, where it was {}: {}",
                 wanted, was.value_or(0.0F), dao::world::SetCreatureShowDistance(wanted));
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

// The Scaleform manager's OpenMovie (dao/Addresses.h): every movie the
// game opens, with what it passes, so the arguments' meanings are seen
// before a plugin opens a movie of its own. Observe only.
using OpenMovieFn = void(__fastcall *)(void *manager, int edx, void *screen, const dao::core::WStringRep *name,
                                       std::int32_t layer, std::int32_t flags);
dao::Detour<OpenMovieFn> g_openMovie;

void __fastcall OpenMovie(void *manager, int edx, void *screen, const dao::core::WStringRep *name, std::int32_t layer,
                          std::int32_t flags)
{
    const auto caller = dao::Module::Preferred(reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    const std::wstring movie = name != nullptr ? std::wstring(name->View()) : std::wstring();
    g_openMovie.Original()(manager, edx, screen, name, layer, flags);
    void *record = dao::gui::FindOwner(movie);
    spdlog::debug("OpenMovie \"{}\" from {:#010x} on thread {}: screen {} at {}, layer {}, flags {:#x}; record {}, "
                  "movie {}",
                  dao::Utf8(movie), caller, GetCurrentThreadId(),
                  screen != nullptr ? dao::TypeName(screen) : std::string("none"), screen, layer, flags, record,
                  dao::gui::MovieOf(record));
}

// The game writing a save and finding one to load (dao/Addresses.h,
// SaveGame): their arguments, so a plugin's own data can be kept beside a
// save once what they mean is seen. Observe only.
using WriteSaveFn = int(__fastcall *)(void *self, int edx, std::int32_t slot, std::int32_t kind,
                                      const dao::core::WStringRep *name, std::int32_t fourth);
dao::Detour<WriteSaveFn> g_writeSave;

int __fastcall WriteSave(void *self, int edx, std::int32_t slot, std::int32_t kind, const dao::core::WStringRep *name,
                         std::int32_t fourth)
{
    const int result = g_writeSave.Original()(self, edx, slot, kind, name, fourth);
    const auto *character = reinterpret_cast<const dao::core::WStringRep *>(static_cast<std::byte *>(self) + 0x18);
    spdlog::info(R"(SaveGame::Write on {} ({}): character "{}", slot {}, kind {}, name "{}", fourth {}, result {:#x})",
                 self, dao::TypeName(self), dao::Utf8(character->View()), slot, kind,
                 name != nullptr ? dao::Utf8(name->View()) : std::string("none"), fourth, result);
    return result;
}

using FindSaveFn = int(__cdecl *)(void *out, const dao::core::WStringRep *character, std::int32_t slot,
                                  std::int32_t kind);
dao::Detour<FindSaveFn> g_findSave;

int __cdecl FindSave(void *out, const dao::core::WStringRep *character, std::int32_t slot, std::int32_t kind)
{
    spdlog::info("SaveGame::FindSave: character \"{}\", slot {}, kind {}, from {:#010x}",
                 character != nullptr ? dao::Utf8(character->View()) : std::string("none"), slot, kind,
                 dao::Module::Preferred(reinterpret_cast<std::uintptr_t>(_ReturnAddress())));
    return g_findSave.Original()(out, character, slot, kind);
}

// The address logged is the GOG build's listing's, as everywhere in the logs.
template <class Fn> void Place(dao::Detour<Fn> &detour, dao::AddressId id, Fn hook, const char *what)
{
    if (detour.Attach(id, hook))
    {
        spdlog::info("detoured {} at {:#010x}", what, dao::Module::Preferred(id));
    }
    else
    {
        spdlog::error("could not detour {} at {:#010x}", what, dao::Module::Preferred(id));
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
    const auto fingerprint = dao::Module::Fingerprint();
    spdlog::info("exe at {:#x}, link stamp {:#x}, image size {:#x}, {}", dao::Module::Base(),
                 fingerprint ? fingerprint->timeDateStamp : 0, fingerprint ? fingerprint->sizeOfImage : 0,
                 info->knownExecutable ? "a build the address library knows" : "not a build the address library knows");
    if (!info->knownExecutable)
    {
        return false;
    }
    g_crashTest = CrashTestRequested();
    if (g_crashTest != CrashTest::kOff)
    {
        spdlog::warn("DAOLIB_PROBE_CRASH_TEST is set: the first SetPaused will crash the game on purpose{}",
                     g_crashTest == CrashTest::kFullThenCrash ? ", with the address space filled first" : "");
    }
    spdlog::info("this game has the plain names in its log folder: {}", dao::OwnsLogs());
    // No game object exists yet: the app is made on first use, after this.
    spdlog::info("CClientExoApp at load: {}", static_cast<void *>(dao::CClientExoApp::Get()));

    Place(g_handleTogglePause, dao::addr::kGameModeController_HandleUserTogglePause, &HandleTogglePause,
          "GameModeController::HandleMessage(UserTogglePauseMessage)");
    Place(g_setPaused, dao::addr::kCClientExoApp_SetPaused, &SetPaused, "CClientExoApp::SetPaused");
    Place(g_queueCommand, dao::addr::kCCObject_QueueCommand, &QueueCommand, "CCObject::QueueCommand");
    Place(g_clientAddCommand, dao::addr::kCClientVirtualMachineCommands_AddCommand, &ClientAddCommand,
          "CClientVirtualMachineCommands::AddCommand");
    Place(g_openMovie, dao::addr::kScaleformManager_OpenMovie, &OpenMovie, "GUI::ScaleformManager::OpenMovie");
    Place(g_writeSave, dao::addr::kSaveGame_Write, &WriteSave, "SaveGame::Write");
    Place(g_findSave, dao::addr::kSaveGame_FindSave, &FindSave, "SaveGame::FindSave");
    Place(g_animationEvent, dao::addr::kCCCreature_OnAnimationEvent, &AnimationEvent, "CCCreature::OnAnimationEvent");
    Place(g_addAnimationEvent, dao::addr::kAnimationManager_AddEvent, &AddAnimationEvent,
          "Animation::AnimationManager::AddEvent");
    if (!g_setProcessAffinity.AttachTo(&SetProcessAffinityMask, &SetProcessAffinity))
    {
        spdlog::warn("could not detour SetProcessAffinityMask");
    }
    DWORD_PTR had = 0;
    DWORD_PTR system = 0;
    GetProcessAffinityMask(GetCurrentProcess(), &had, &system);
    spdlog::info("process affinity at load: {:#x} of the system's {:#x}", had, system);
    dao::OnFrame([] {
        static DWORD_PTR last = 0;
        DWORD_PTR now = 0;
        DWORD_PTR all = 0;
        GetProcessAffinityMask(GetCurrentProcess(), &now, &all);
        if (now != last)
        {
            spdlog::info("process affinity on a frame: {:#x}, where it was {:#x}", now, last);
            last = now;
        }
    });
    WidenCreatureShow();
    probe::AttachScriptChecks();
    probe::AttachColourCurveWatch();
    probe::AttachEventWatch();
    probe::AttachConjureWatch();
    probe::AttachMovieWatch();
    probe::AttachHudWatch();
    probe::AttachReadWatch();
    probe::AttachLoadingWatch();

    // The census, on everything but what is hooked above.
    const dao::AddressId hookedAbove[] = {
        dao::addr::kGameModeController_HandleUserTogglePause,
        dao::addr::kCClientExoApp_SetPaused,
        dao::addr::kCCObject_QueueCommand,
        dao::addr::kCClientVirtualMachineCommands_AddCommand,
        dao::addr::kScaleformManager_OpenMovie,
        dao::addr::kSaveGame_Write,
        dao::addr::kSaveGame_FindSave,
    };
    const auto census = probe::AttachCensus(hookedAbove);
    spdlog::info("census: {} of {} functions counted, {} skipped (hooked above), {} refused by Detours",
                 census.attached, probe::Targets().size(), census.skipped, census.failed);
    probe::StartRecorder();
    return true;
}
