#include "Recorder.h"

#include "Census.h"
#include "Events.h"
#include "ScriptChecks.h"
#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/TypeName.h"
#include "dao/core/CallSite.h"
#include "dao/core/Pe.h"

#include <Windows.h>

#include <TlHelp32.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <span>
#include <string>
#include <unordered_map>
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
        if (!calls_ || !samples_)
        {
            return false;
        }
        const std::string started = LocalTimeNow();
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
            if (first || now - lastRefresh >= kThreadRefreshMs)
            {
                RefreshThreads();
                lastRefresh = now;
            }
            SampleAll();
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
            Sleep(kSampleSleepMs);
        }
    }

  private:
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
            const auto mode = dao::TryRead<std::uintptr_t>(*modes + 4 * i);
            const std::string type =
                mode && *mode != 0 ? dao::TypeName(reinterpret_cast<const void *>(*mode)) : std::string("-");
            if (type != "-" && type.rfind("GameMode", 0) != 0)
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
        for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry))
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
        return ok;
    }

    void SampleAll()
    {
        for (const auto &[tid, h] : threads_)
        {
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
};

DWORD WINAPI RecorderThread(LPVOID parameter)
{
    auto *recorder = static_cast<Recorder *>(parameter);
    recorder->Run();
}

} // namespace

bool StartRecorder()
{
    const auto code = dao::Module::Code();
    if (!code)
    {
        spdlog::error("recorder: no .text section in the exe's headers");
        return false;
    }
    const Text range = *code;
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
