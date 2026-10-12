#include "Recorder.h"

#include "Census.h"
#include "Events.h"
#include "ScriptChecks.h"
#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/TypeName.h"
#include "dao/core/CallSite.h"
#include "dao/core/Pe.h"

#include <Windows.h>

#include <TlHelp32.h>
#include <spdlog/spdlog.h>
#include <timeapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace probe
{

namespace
{

constexpr ULONGLONG kCensusPeriodMs = 1000;
constexpr ULONGLONG kWindowMs = 10000;
constexpr ULONGLONG kThreadRefreshMs = 2000;
constexpr ULONGLONG kHealthMs = 60000;
constexpr DWORD kSampleSleepMs = 10; // the system timer makes this about 15
constexpr std::size_t kStackWords =
    4096; // 16 KiB: 2 KiB missed the game's own frames under the graphics driver's (2026-09-30)
constexpr std::size_t kMaxReturnsPerSample = 128;
constexpr std::uint32_t kLowestAddress = 0x10000; // nothing is mapped below it
constexpr DWORD kLoadingSleepMs = 1;              // with the timer at a millisecond, while a loading screen is up

// What the loading watch last said, and the thread that said it.
std::atomic<std::uint32_t> g_loadingPhase{0};
std::atomic<DWORD> g_loadingThread{0};

// The engine's threads by the names they give themselves at their start
// (dao/Addresses.h, "The engine's threads").
std::mutex g_namesLock;
std::map<DWORD, std::string> g_names;

// The ones sampled with the loading thread while a loading screen is up:
// one of each, and what a loading waits on is theirs to do.
constexpr std::array<const char *, 3> kWatched{"DragonAge::Render", "DragonAge::Streaming", "DragonAge::FileIO"};

// ThreadManager::SetThreadName: __thiscall (thread id or -1, name), `ret 8`. The name is the
// caller's temporary, so it is copied here.
using SetThreadNameFn = void(__fastcall *)(void *manager, int, DWORD thread, const char *name);
dao::Detour<SetThreadNameFn> g_setThreadName;

void __fastcall OnSetThreadName(void *manager, int /*edx*/, DWORD thread, const char *name)
{
    constexpr DWORD kThisThread = 0xFFFFFFFF;
    if (name != nullptr)
    {
        const DWORD tid = thread == kThisThread ? GetCurrentThreadId() : thread;
        spdlog::info("recorder: thread {} is {}", tid, name);
        const std::scoped_lock lock(g_namesLock);
        g_names[tid] = name;
    }
    g_setThreadName.Original()(manager, 0, thread, name);
}

// The thread of a name, 0 if none has taken it; and a thread's name, or "?".
DWORD ThreadNamed(const char *name)
{
    const std::scoped_lock lock(g_namesLock);
    const auto found = std::ranges::find_if(g_names, [&](const auto &entry) { return entry.second == name; });
    return found != g_names.end() ? found->first : 0;
}

std::string NameOfThread(DWORD tid)
{
    const std::scoped_lock lock(g_namesLock);
    const auto found = g_names.find(tid);
    return found != g_names.end() ? found->second : "?";
}

using Text = dao::Module::CodeRange;

// A sample is of the game's logic when the update task's frame is on its
// stack (dao/Addresses.h, kEclipseUpdateTask_Execute): the game's worker
// threads run it, among everything else they run. Its samples are also
// counted apart, so a comparison of them is not drowned by the fifty-odd
// other threads the process has (2026-09-30: the logic thread was under 2%
// of all samples).
bool IsUpdate(std::uint32_t preferredReturn)
{
    // Both ends as the listing has them, which is what the return is given as.
    static const std::uint32_t begin = dao::Module::Preferred(dao::addr::kEclipseUpdateTask_Execute);
    static const std::uint32_t end = begin + dao::addr::kEclipseUpdateTask_ExecuteSize;
    return preferredReturn >= begin && preferredReturn <= end;
}

// What one sample copies while its thread is suspended: registers and stack
// into storage made before, so nothing is allocated with the thread frozen.
struct Capture
{
    DWORD tid = 0;
    std::uint32_t eip = 0;
    std::size_t words = 0;
    std::array<std::uint32_t, kStackWords> stack{};
};

struct Window
{
    std::uint64_t samples = 0;
    std::unordered_map<std::uint32_t, std::uint32_t> eip;
    std::unordered_map<std::uint32_t, std::uint32_t> stack;
    std::map<DWORD, std::uint32_t> inExe; // by thread id
    // The same, for the samples with the game's update on the stack.
    std::uint64_t updateSamples = 0;
    std::unordered_map<std::uint32_t, std::uint32_t> updateEip;
    std::unordered_map<std::uint32_t, std::uint32_t> updateStack;
};

// The first callers on a sampled stack, in their order from the innermost:
// one word that looks like a return can be a stale one, four in a row seen
// again and again are a path.
constexpr std::size_t kChain = 4;
using Chain = std::array<std::uint32_t, kChain>;
// The first return addresses from the top of a stack into any module's code.
constexpr std::size_t kVia = 6;
using Via = std::array<std::uint32_t, kVia>;

// A thread's samples in one state and step, inside the update or between
// two.
struct StepSamples
{
    std::uint32_t samples = 0;
    std::uint32_t outside = 0; // of them, with the thread outside the exe's code
    std::unordered_map<std::uint32_t, std::uint32_t> leaf;
    std::unordered_map<std::uint32_t, std::uint32_t> inner;
    std::unordered_map<std::uint32_t, std::uint32_t> stack;
    std::map<std::string, std::uint32_t> module; // of those outside: the module the thread was in
    std::map<Chain, std::uint32_t> chain;
    std::map<Via, std::uint32_t> via; // of those outside: who had called, in any module
};

// One loading screen.
struct LoadingScreen
{
    std::uint32_t number = 0;
    ULONGLONG began = 0;
    std::uint32_t samples = 0;
    std::map<std::uint32_t, StepSamples> steps;                          // the loading thread's, by phase
    std::map<std::string, std::map<std::uint32_t, StepSamples>> watched; // each watched thread's, by phase
    std::map<DWORD, std::uint64_t> cpuAtStart;                           // by thread id, in 100 ns
    // The recorder's own health: a step it took nothing in is its fault or the thread's.
    std::uint32_t turns = 0;        // of its loop
    ULONGLONG lastTurn = 0;         //
    ULONGLONG longestGap = 0;       // between two turns, in ms
    std::uint32_t notSuspended = 0; // SuspendThread failed
    std::uint32_t noContext = 0;    // GetThreadContext failed
    std::uint32_t notFound = 0;     // the loading thread was not among the threads known
};

// A loaded module: its file's name, lower case, and its exports that are code, by RVA.
using Export = std::pair<std::uint32_t, std::string>;
struct Module
{
    std::string name;
    std::vector<Export> exports;
};

Module LoadedModule(std::uintptr_t base)
{
    Module module;
    std::array<char, MAX_PATH> path{};
    const DWORD length =
        GetModuleFileNameA(reinterpret_cast<HMODULE>(base), path.data(), static_cast<DWORD>(path.size()));
    module.name.assign(path.data(), length);
    module.name = module.name.substr(module.name.find_last_of("\\/") + 1);
    std::ranges::transform(module.name, module.name.begin(),
                           [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    if (module.name.empty())
    {
        module.name = "?";
    }
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return module;
    }
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY &directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (nt->Signature != IMAGE_NT_SIGNATURE || directory.VirtualAddress == 0 || directory.Size == 0)
    {
        return module;
    }
    const auto *table = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY *>(base + directory.VirtualAddress);
    const auto *names = reinterpret_cast<const DWORD *>(base + table->AddressOfNames);
    const auto *ordinals = reinterpret_cast<const WORD *>(base + table->AddressOfNameOrdinals);
    const auto *functions = reinterpret_cast<const DWORD *>(base + table->AddressOfFunctions);
    for (DWORD i = 0; i < table->NumberOfNames; ++i)
    {
        // An RVA inside the export directory is a forwarder's text, not code.
        const DWORD rva = ordinals[i] < table->NumberOfFunctions ? functions[ordinals[i]] : 0;
        if (rva != 0 && (rva < directory.VirtualAddress || rva >= directory.VirtualAddress + directory.Size))
        {
            module.exports.emplace_back(rva, reinterpret_cast<const char *>(base + names[i]));
        }
    }
    std::ranges::sort(module.exports);
    return module;
}

// The processor time a thread has used, in 100 ns; 0 if it cannot be asked.
std::uint64_t CpuTime(HANDLE thread)
{
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (GetThreadTimes(thread, &created, &exited, &kernel, &user) == FALSE)
    {
        return 0;
    }
    const auto whole = [](const FILETIME &time) {
        return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    };
    return whole(kernel) + whole(user);
}

std::string LocalTimeNow()
{
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    std::array<char, 32> text{};
    std::strftime(text.data(), text.size(), "%Y-%m-%d %H:%M:%S", &local);
    return text.data();
}

using dao::Module::Preferred;

class Recorder
{
  public:
    explicit Recorder(Text text) : text_(text), previous_(Targets().size(), 0)
    {
    }

    bool Open()
    {
        const auto dir = dao::LogDirectory();
        calls_.open(dir / "probe-calls.tsv", std::ios::trunc);
        samples_.open(dir / "probe-samples.tsv", std::ios::trunc);
        loadingFile_.open(dir / "probe-loading.tsv", std::ios::trunc);
        if (!calls_ || !samples_ || !loadingFile_)
        {
            return false;
        }
        const std::string started = LocalTimeNow();
        loadingFile_ << "# probe-loading, ms since " << started << "; see src/plugins/probe/Recorder.h\n";
        loadingFile_.flush();
        calls_ << "# probe-calls, ms since " << started << "; see src/plugins/probe/Recorder.h\n";
        samples_ << "# probe-samples, ms since " << started << "; see src/plugins/probe/Recorder.h\n";
        calls_.flush();
        samples_.flush();
        return true;
    }

    // The sampler runs for the life of the process.
    [[noreturn]] void Run()
    {
        const ULONGLONG start = GetTickCount64();
        ULONGLONG lastCensus = 0;
        ULONGLONG lastWindow = 0;
        ULONGLONG lastRefresh = 0;
        ULONGLONG lastHealth = 0;
        bool first = true;
        for (;;)
        {
            const ULONGLONG now = GetTickCount64() - start;
            if (first || (!loadingNow_ && now - lastRefresh >= kThreadRefreshMs))
            {
                RefreshThreads();
                lastRefresh = now;
            }
            SampleTurn(now);
            // Nothing else while a loading screen is up, or has begun since the turn did: a
            // window's write is turns not taken, and a door is over in a few of them. In play,
            // 2026-10-11, the two doors of five loading screens that a ten-second window's end
            // fell in, 0.75 s each, were not seen to begin at all.
            if (loadingNow_ || g_loadingPhase.load(std::memory_order_relaxed) != 0)
            {
                Sleep(kLoadingSleepMs);
                continue;
            }
            LogGameModesOnce();
            if (now - lastCensus >= kCensusPeriodMs)
            {
                WriteCensus(now);
                lastCensus = now;
            }
            if (now - lastWindow >= kWindowMs)
            {
                WriteWindow(lastWindow, now);
                lastWindow = now;
            }
            if (first || now - lastHealth >= kHealthMs)
            {
                spdlog::info("recorder: {} threads sampled, {} calls counted so far", threads_.size(), totalCalls_);
                ReportScriptChecks();
                ReportEventWatch();
                lastHealth = now;
            }
            first = false;
            Sleep(loadingNow_ ? kLoadingSleepMs : kSampleSleepMs);
        }
    }

  private:
    // One turn of sampling: while a loading screen is up, the loading thread and the engine's
    // threads it waits on, by the loading mode's step; every thread otherwise. A loading screen
    // begins and ends here, as the loading mode's step comes and goes.
    void SampleTurn(ULONGLONG now)
    {
        const std::uint32_t phase = g_loadingPhase.load(std::memory_order_relaxed);
        if (phase != 0 && !loadingNow_)
        {
            BeginLoading(now);
        }
        else if (phase == 0 && loadingNow_)
        {
            EndLoading(now);
        }
        if (!loadingNow_)
        {
            // Not while a loading screen is up: a pass over every thread
            // took half of a short screen's time, and the recorder's turns
            // came 100 to 375 ms apart (in play, 2026-10-11).
            SampleAll();
            return;
        }
        ++loading_.turns;
        loading_.longestGap = std::max(loading_.longestGap, now - loading_.lastTurn);
        loading_.lastTurn = now;
        const DWORD loadingThread = g_loadingThread.load(std::memory_order_relaxed);
        SampleThread(loadingThread, loading_.steps[phase], true);
        for (const char *name : kWatched)
        {
            const DWORD tid = ThreadNamed(name);
            if (tid != 0 && tid != loadingThread)
            {
                SampleThread(tid, loading_.watched[name][phase], false);
            }
        }
    }

    // Which GameMode class is at each number of the controller's table
    // (dao/GameModes.h), once, as soon as the table is there: what the
    // mode numbers mean.
    void LogGameModesOnce()
    {
        if (modesLogged_)
        {
            return;
        }
        const auto app = dao::TryRead<std::uintptr_t>(dao::Module::Address(dao::addr::kClientExoApp));
        const auto controller =
            app && *app != 0 ? dao::TryRead<std::uintptr_t>(*app + dao::CClientExoApp::kModeHolder) : std::nullopt;
        if (!controller || *controller == 0)
        {
            return;
        }
        const auto modes = dao::TryRead<std::uintptr_t>(*controller + dao::GameModeController::kModes);
        const auto current = dao::TryRead<std::int32_t>(*controller + dao::GameModeController::kMode);
        // Once a game is loaded, the array +0x58 points to (dao/GameModes.h),
        // entry by entry, until one is not a GameMode: which class is which
        // mode number.
        if (!modes || *modes == 0 || !current || *current != static_cast<std::int32_t>(dao::GameMode::Explore))
        {
            return;
        }
        modesLogged_ = true;
        std::string line;
        std::uintptr_t i = 0;
        for (; i < dao::GameModeController::kMaxModes; ++i)
        {
            const auto mode = dao::TryRead<std::uintptr_t>(*modes + (4 * i));
            const std::string type =
                mode && *mode != 0 ? dao::TypeName(reinterpret_cast<const void *>(*mode)) : std::string("-");
            if (type != "-" && !type.starts_with("GameMode"))
            {
                break;
            }
            line += fmt::format(" {}={}", i, type);
        }
        spdlog::info("game modes: {} entries in the controller's array:{}", i, line);
    }

    bool modesLogged_ = false;

    void RefreshThreads()
    {
        const DWORD self = GetCurrentThreadId();
        const DWORD pid = GetCurrentProcessId();
        std::map<DWORD, HANDLE> seen;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return;
        }
        THREADENTRY32 entry{};
        entry.dwSize = sizeof entry;
        for (BOOL ok = Thread32First(snapshot, &entry); ok != 0; ok = Thread32Next(snapshot, &entry))
        {
            if (entry.th32OwnerProcessID != pid || entry.th32ThreadID == self)
            {
                continue;
            }
            const auto have = threads_.find(entry.th32ThreadID);
            if (have != threads_.end())
            {
                seen.emplace(have->first, have->second);
                threads_.erase(have);
                continue;
            }
            HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE,
                                  entry.th32ThreadID);
            if (h != nullptr)
            {
                seen.emplace(entry.th32ThreadID, h);
            }
        }
        CloseHandle(snapshot);
        for (auto &[tid, h] : threads_) // the ones that are gone
        {
            CloseHandle(h);
        }
        threads_ = std::move(seen);
    }

    // Suspend, copy, resume: nothing in between allocates, locks or logs.
    bool Take(DWORD tid, HANDLE h)
    {
        if (SuspendThread(h) == static_cast<DWORD>(-1))
        {
            ++loading_.notSuspended;
            return false;
        }
        CONTEXT context{};
        context.ContextFlags = CONTEXT_CONTROL;
        bool ok = GetThreadContext(h, &context) != FALSE;
        if (ok)
        {
            capture_.tid = tid;
            capture_.eip = context.Eip;
            capture_.words = 0;
            MEMORY_BASIC_INFORMATION region{};
            if (VirtualQuery(reinterpret_cast<const void *>(context.Esp), &region, sizeof region) != 0)
            {
                const auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
                const std::size_t words = std::min<std::size_t>(kStackWords, (end - context.Esp) / 4);
                if (dao::TryCopy(capture_.stack.data(), context.Esp, words * 4))
                {
                    capture_.words = words;
                }
            }
        }
        ResumeThread(h);
        loading_.noContext += ok ? 0 : 1;
        return ok;
    }

    // The return addresses on the captured stack, into the exe's code just
    // after a call, each once; how many were found.
    std::size_t CallersOnStack(std::array<std::uint32_t, kMaxReturnsPerSample> &found) const
    {
        std::size_t count = 0;
        for (std::size_t i = 0; i < capture_.words && count < found.size(); ++i)
        {
            const std::uint32_t value = capture_.stack[i];
            if (!text_.Contains(value) || value < text_.begin + 7)
            {
                continue;
            }
            std::array<std::byte, 7> before{};
            std::memcpy(before.data(), reinterpret_cast<const void *>(value - 7), before.size());
            if (!dao::core::EndsWithCall(before))
            {
                continue;
            }
            const auto seen = std::span(found.data(), count);
            if (std::ranges::find(seen, value) == seen.end())
            {
                found[count++] = value;
            }
        }
        return count;
    }

    void SampleAll()
    {
        for (const auto &[tid, h] : threads_)
        {
            if (g_loadingPhase.load(std::memory_order_relaxed) != 0)
            {
                break; // a loading screen has begun: the pass is left for the screen's own samples
            }
            if (!Take(tid, h))
            {
                continue;
            }
            ++window_.samples;
            if (text_.Contains(capture_.eip))
            {
                ++window_.eip[Preferred(capture_.eip)];
                ++window_.inExe[tid];
            }
            // Return addresses on the stack, each counted once per sample.
            std::array<std::uint32_t, kMaxReturnsPerSample> found{};
            const std::size_t count = CallersOnStack(found);
            bool update = false;
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::uint32_t at = Preferred(found[i]);
                ++window_.stack[at];
                update = update || IsUpdate(at);
            }
            if (update)
            {
                ++window_.updateSamples;
                if (text_.Contains(capture_.eip))
                {
                    ++window_.updateEip[Preferred(capture_.eip)];
                }
                for (std::size_t i = 0; i < count; ++i)
                {
                    ++window_.updateStack[Preferred(found[i])];
                }
            }
        }
    }

    // A loading screen is up: the timer goes to a millisecond, so the
    // loading thread can be sampled that often, and every thread's
    // processor time is noted.
    void BeginLoading(ULONGLONG now)
    {
        timeBeginPeriod(1);
        loadingNow_ = true;
        loading_ = LoadingScreen{};
        loading_.number = ++screens_;
        loading_.began = now;
        loading_.lastTurn = now;
        for (const auto &[tid, h] : threads_)
        {
            loading_.cpuAtStart[tid] = CpuTime(h);
        }
    }

    // Where an address outside the exe is: its module, lower case, and the module's export at or
    // before it, as "ntdll.dll!NtWaitForSingleObject", or "?". A wait in the system is in the
    // export itself; code between two exports takes the earlier one's name, which may not be
    // its own. Asked with no thread suspended, and kept by the address.
    const std::string &PlaceOf(std::uint32_t address)
    {
        const auto known = places_.find(address);
        if (known != places_.end())
        {
            return known->second;
        }
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void *>(static_cast<std::uintptr_t>(address)), &region,
                         sizeof region) == 0 ||
            region.AllocationBase == nullptr || region.Type != MEM_IMAGE)
        {
            return places_.emplace(address, "?").first->second;
        }
        const auto base = reinterpret_cast<std::uintptr_t>(region.AllocationBase);
        auto module = modules_.find(base);
        if (module == modules_.end())
        {
            module = modules_.emplace(base, LoadedModule(base)).first;
        }
        std::string place = module->second.name;
        const auto &exports = module->second.exports;
        const auto after = std::ranges::upper_bound(exports, address - base, {}, &Export::first);
        if (after != exports.begin())
        {
            place += '!' + std::prev(after)->second;
        }
        return places_.emplace(address, std::move(place)).first->second;
    }

    // Whether an address is in a loaded module's code. Asked of the system once a region, and
    // kept: a page that was not code is not asked about again.
    bool InCode(std::uint32_t address)
    {
        if (text_.Contains(address))
        {
            return true;
        }
        if (std::ranges::any_of(code_,
                                [&](const auto &range) { return address >= range.first && address < range.second; }))
        {
            return true;
        }
        constexpr std::uint32_t kPageShift = 12;
        if (address < kLowestAddress || notCode_.contains(address >> kPageShift))
        {
            return false;
        }
        constexpr DWORD kExecutable =
            PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void *>(static_cast<std::uintptr_t>(address)), &region,
                         sizeof region) == 0 ||
            region.State != MEM_COMMIT || region.Type != MEM_IMAGE || (region.Protect & kExecutable) == 0)
        {
            notCode_.insert(address >> kPageShift);
            return false;
        }
        const auto begin = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(region.BaseAddress));
        code_.emplace_back(begin, begin + static_cast<std::uint32_t>(region.RegionSize));
        return true;
    }

    // Who had called the system function the sampled thread is in: the first return addresses
    // from the top of its stack into any module's code, the game's included. Unlike a caller
    // in the game found farther up, which a system function's own locals can hold stale, the
    // words at the top of a waiting thread's stack are its live frames'.
    Via SystemCallers()
    {
        constexpr std::size_t kWordsLooked = 96;
        constexpr std::uint32_t kCallBytes = 7;
        Via callers{};
        std::size_t count = 0;
        for (std::size_t i = 0; i < std::min(capture_.words, kWordsLooked) && count < callers.size(); ++i)
        {
            const std::uint32_t value = capture_.stack[i];
            if (!InCode(value) || !InCode(value - kCallBytes))
            {
                continue;
            }
            std::array<std::byte, kCallBytes> before{};
            std::memcpy(before.data(), reinterpret_cast<const void *>(static_cast<std::uintptr_t>(value - kCallBytes)),
                        before.size());
            if (dao::core::EndsWithCall(before))
            {
                callers[count++] = value;
            }
        }
        return callers;
    }

    // One sample of one thread into a step's counts. `loadingThread` for the
    // thread the screen is about: only its samples are the screen's count,
    // and only its being unknown is counted against the recorder.
    void SampleThread(DWORD tid, StepSamples &step, bool loadingThread)
    {
        const auto thread = threads_.find(tid);
        if (thread == threads_.end())
        {
            loading_.notFound += loadingThread ? 1 : 0;
            return;
        }
        if (!Take(tid, thread->second))
        {
            return;
        }
        loading_.samples += loadingThread ? 1 : 0;
        ++step.samples;
        std::array<std::uint32_t, kMaxReturnsPerSample> found{};
        const std::size_t count = CallersOnStack(found);
        if (text_.Contains(capture_.eip))
        {
            ++step.leaf[Preferred(capture_.eip)];
        }
        else
        {
            ++step.outside;
            ++step.module[PlaceOf(capture_.eip)];
            ++step.via[SystemCallers()];
            if (count > 0)
            {
                ++step.inner[Preferred(found[0])];
            }
        }
        Chain chain{};
        for (std::size_t i = 0; i < count; ++i)
        {
            ++step.stack[Preferred(found[i])];
            if (i < chain.size())
            {
                chain[i] = Preferred(found[i]);
            }
        }
        ++step.chain[chain];
    }

    // A chain or a path seen once says nothing.
    static constexpr std::uint32_t kFewestOfAChain = 2;

    // The callers of a system function as one path, innermost first: by name, so two returns
    // into one export are one place, and the game's own code as its preferred address.
    std::string PathOf(const auto &callers)
    {
        std::string path;
        std::string last;
        for (const std::uint32_t caller : callers)
        {
            if (caller == 0)
            {
                break;
            }
            const std::string place =
                text_.Contains(caller) ? fmt::format("{:#x}", Preferred(caller)) : PlaceOf(caller);
            if (place != last)
            {
                path += (path.empty() ? "" : " < ") + place;
                last = place;
            }
        }
        return path.empty() ? "?" : path;
    }

    // A step's "via" rows: its samples outside the game's code, by the path that led there.
    void WriteVia(const std::string &lead, const std::string &key, const auto &counts)
    {
        std::map<std::string, std::uint32_t> via;
        for (const auto &[callers, count] : counts)
        {
            via[PathOf(callers)] += count;
        }
        for (const auto &[path, count] : via)
        {
            if (count >= kFewestOfAChain)
            {
                loadingFile_ << lead << "via\t" << key << path << '\t' << count << '\n';
            }
        }
    }

    // A step's "chain" rows: the returns on the stack, in their order.
    void WriteChains(const std::string &lead, const std::string &key, const auto &chains)
    {
        for (const auto &[chain, count] : chains)
        {
            if (count < kFewestOfAChain)
            {
                continue;
            }
            loadingFile_ << lead << "chain\t" << key << std::hex;
            for (std::size_t i = 0; i < chain.size(); ++i)
            {
                loadingFile_ << (i == 0 ? "0x" : ",0x") << chain[i];
            }
            loadingFile_ << std::dec << '\t' << count << '\n';
        }
    }

    // A step's rows of one kind that count by address.
    void WriteCounts(const std::string &lead, const char *kind, const std::string &key,
                     const std::unordered_map<std::uint32_t, std::uint32_t> &counts)
    {
        for (const auto &[address, count] : counts)
        {
            loadingFile_ << lead << kind << '\t' << key << "0x" << std::hex << address << std::dec << '\t' << count
                         << '\n';
        }
    }

    void WriteSteps(const std::string &n, const std::string &prefix, const std::map<std::uint32_t, StepSamples> &steps)
    {
        const std::string lead = n + prefix;
        for (const auto &[phase, step] : steps)
        {
            constexpr std::uint32_t kInside = 0x80000000;
            const std::string key = std::to_string((phase >> 16) & 0x7FFF) + '\t' +
                                    std::to_string(static_cast<std::int16_t>(phase & 0xFFFF)) + '\t' +
                                    ((phase & kInside) != 0 ? "1\t" : "0\t");
            loadingFile_ << lead << "step\t" << key << step.samples << '\t' << step.outside << '\n';
            WriteCounts(lead, "leaf", key, step.leaf);
            WriteCounts(lead, "inner", key, step.inner);
            WriteCounts(lead, "stack", key, step.stack);
            for (const auto &[name, count] : step.module)
            {
                loadingFile_ << lead << "module\t" << key << name << '\t' << count << '\n';
            }
            WriteVia(lead, key, step.via);
            WriteChains(lead, key, step.chain);
        }
    }

    void EndLoading(ULONGLONG now)
    {
        timeEndPeriod(1);
        loadingNow_ = false;
        const std::string n = std::to_string(loading_.number) + '\t';
        const DWORD loadingThread = g_loadingThread.load(std::memory_order_relaxed);
        loadingFile_ << n << "screen\t" << loading_.began << '\t' << now << '\t' << loading_.samples << '\n';
        loadingFile_ << n << "health\t" << loading_.turns << '\t' << loading_.longestGap << '\t'
                     << loading_.notSuspended << '\t' << loading_.noContext << '\t' << loading_.notFound << '\n';
        for (const auto &[tid, h] : threads_)
        {
            const auto before = loading_.cpuAtStart.find(tid);
            const std::uint64_t used = CpuTime(h) - (before != loading_.cpuAtStart.end() ? before->second : 0);
            constexpr std::uint64_t kPerMillisecond = 10000;
            if (used >= kPerMillisecond)
            {
                loadingFile_ << n << "cpu\t" << tid << '\t' << used / kPerMillisecond << '\t'
                             << (tid == loadingThread ? 1 : 0) << '\t' << NameOfThread(tid) << '\n';
            }
        }
        WriteSteps(n, "", loading_.steps);
        for (const auto &[name, steps] : loading_.watched)
        {
            WriteSteps(n, name + '.', steps);
        }
        loadingFile_.flush();
        spdlog::info("recorder: loading screen {}: {} samples of its thread over {} ms, in probe-loading.tsv; {} turns "
                     "of the recorder, the longest gap {} ms; suspend failed {}, no context {}, thread not known {}; "
                     "{} other thread(s) watched",
                     loading_.number, loading_.samples, now - loading_.began, loading_.turns, loading_.longestGap,
                     loading_.notSuspended, loading_.noContext, loading_.notFound, loading_.watched.size());
        loading_ = LoadingScreen{};
    }

    void WriteCensus(ULONGLONG now)
    {
        // The application's state: -1 where it cannot be read (not made
        // yet, or torn down).
        int paused = -1;
        int mode = -1;
        int second = -1;
        if (const auto app = dao::TryRead<std::uint32_t>(dao::Module::Address(dao::addr::kClientExoApp));
            app && *app != 0)
        {
            using App = dao::CClientExoApp;
            if (const auto v = dao::TryRead<std::uint8_t>(*app + App::kPausedFlag + App::kFlagValue))
            {
                paused = *v != 0 ? 1 : 0;
            }
            if (const auto v = dao::TryRead<std::uint8_t>(*app + App::kSecondFlag + App::kFlagValue))
            {
                second = *v != 0 ? 1 : 0;
            }
            if (const auto holder = dao::TryRead<std::uint32_t>(*app + App::kModeHolder); holder && *holder != 0)
            {
                if (const auto v = dao::TryRead<std::int32_t>(*holder + dao::GameModeController::kMode))
                {
                    mode = *v;
                }
            }
        }
        calls_ << now << "\tstatus\t" << paused << '\t' << mode << '\t' << second << '\n';

        const auto targets = Targets();
        for (std::size_t i = 0; i < targets.size(); ++i)
        {
            if (!IsAttached(i))
            {
                continue;
            }
            const std::uint32_t count = CallCount(i);
            const std::uint32_t delta = count - previous_[i];
            previous_[i] = count;
            if (delta == 0)
            {
                continue;
            }
            totalCalls_ += delta;
            calls_ << now << "\tcall\t" << std::hex << "0x" << targets[i].address << std::dec << '\t' << delta << '\t'
                   << targets[i].label << '\n';
        }
        calls_.flush();
    }

    void WriteWindow(ULONGLONG from, ULONGLONG to)
    {
        const std::string span = std::to_string(from) + '\t' + std::to_string(to) + '\t';
        samples_ << span << "samples\t0\t" << window_.samples << '\n';
        for (const auto &[tid, n] : window_.inExe)
        {
            samples_ << span << "thread\t" << tid << '\t' << n << '\n';
        }
        for (const auto &[address, n] : window_.eip)
        {
            samples_ << span << "eip\t0x" << std::hex << address << std::dec << '\t' << n << '\n';
        }
        for (const auto &[address, n] : window_.stack)
        {
            samples_ << span << "stack\t0x" << std::hex << address << std::dec << '\t' << n << '\n';
        }
        samples_ << span << "usamples\t0\t" << window_.updateSamples << '\n';
        for (const auto &[address, n] : window_.updateEip)
        {
            samples_ << span << "ueip\t0x" << std::hex << address << std::dec << '\t' << n << '\n';
        }
        for (const auto &[address, n] : window_.updateStack)
        {
            samples_ << span << "ustack\t0x" << std::hex << address << std::dec << '\t' << n << '\n';
        }
        samples_.flush();
        window_ = Window{};
    }

    Text text_;
    std::vector<std::uint32_t> previous_;
    std::uint64_t totalCalls_ = 0;
    std::map<DWORD, HANDLE> threads_;
    Capture capture_;
    Window window_;
    std::ofstream calls_;
    std::ofstream samples_;
    std::ofstream loadingFile_;
    bool loadingNow_ = false;
    std::uint32_t screens_ = 0;
    LoadingScreen loading_;
    std::map<std::uintptr_t, Module> modules_;                  // by base
    std::unordered_map<std::uint32_t, std::string> places_;     // by address: what PlaceOf made of it
    std::vector<std::pair<std::uint32_t, std::uint32_t>> code_; // the regions InCode found to be code
    std::unordered_set<std::uint32_t> notCode_;                 // and the pages it found not to be
};

DWORD WINAPI RecorderThread(LPVOID parameter)
{
    auto *recorder = static_cast<Recorder *>(parameter);
    recorder->Run();
}

} // namespace

void SetLoadingPhase(std::uint32_t phase)
{
    if (phase != 0)
    {
        g_loadingThread.store(GetCurrentThreadId(), std::memory_order_relaxed);
    }
    g_loadingPhase.store(phase, std::memory_order_relaxed);
}

bool StartRecorder()
{
    const auto code = dao::Module::Code();
    if (!code)
    {
        spdlog::error("recorder: no .text section in the exe's headers");
        return false;
    }
    const Text range = *code;
    // Before any of the engine's threads has started, so each is known by its name.
    if (!g_setThreadName.Attach(dao::addr::kThreadManager_SetThreadName, &OnSetThreadName))
    {
        spdlog::warn("recorder: the engine's threads will have no names: the detour could not be placed");
    }
    // Lives as long as the process: the thread runs until the game exits,
    // and the files are flushed as they are written, so nothing is lost
    // when it is killed with the rest.
    auto *recorder = new Recorder(range);
    if (!recorder->Open())
    {
        spdlog::error("recorder: could not open probe-calls.tsv and probe-samples.tsv in {}",
                      dao::LogDirectory().string());
        delete recorder;
        return false;
    }
    HANDLE thread = CreateThread(nullptr, 0, &RecorderThread, recorder, 0, nullptr);
    if (thread == nullptr)
    {
        spdlog::error("recorder: could not start its thread, error {}", GetLastError());
        delete recorder;
        return false;
    }
    CloseHandle(thread);
    // The running build's own section, not its counterpart in GOG's: the
    // code's end is the end of nothing in another build.
    spdlog::info("recorder: started; .text {:#010x}-{:#010x}; files in {}", dao::Module::ImageAddress(range.begin),
                 dao::Module::ImageAddress(range.end), dao::LogDirectory().string());
    return true;
}

} // namespace probe
