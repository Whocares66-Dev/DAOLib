#include "loader/CrashLog.h"

#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/TypeName.h"
#include "dao/World.h"
#include "dao/core/CallSite.h"
#include "dao/core/Exceptions.h"
#include "dao/core/FunctionTable.h"
#include "dao/core/Gff4.h"
#include "dao/core/LogNames.h"
#include "dao/core/Pages.h"
#include "dao/core/Relocate.h"
#include "dao/core/StackText.h"

#include <Windows.h>

#include <DbgHelp.h>
#include <Psapi.h>
#include <TlHelp32.h>
#include <detours/detours.h>
#include <share.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace loader::crashlog
{

namespace
{

// ---------------------------------------------------------------------------
// Installation
// ---------------------------------------------------------------------------

using SetFilterFn = LPTOP_LEVEL_EXCEPTION_FILTER(WINAPI *)(LPTOP_LEVEL_EXCEPTION_FILTER);
SetFilterFn g_setFilter = nullptr;      // the real one, after the detour
LPTOP_LEVEL_EXCEPTION_FILTER g_chained; // the filter the game asked for last
std::filesystem::path g_folder;         // where the loader is: daolib\ is beside it
std::atomic<bool> g_crashing{false};    // one log per process, whatever else faults

// What the crashing thread hands the writer: copies, since the writer runs
// on a thread of its own while the crashing one waits.
EXCEPTION_RECORD g_record{};
CONTEXT g_context{};
DWORD g_threadId = 0;

// Address space kept back for the log from the start, and released by the
// filter before anything else. A 32-bit game that crashes with its address
// space full leaves the report nothing to allocate in, and the log was an
// empty file (DAOLib #56: Steam's game, which is not large-address-aware, at
// 1,999 of 2,048 MiB with a largest free block of 3 MiB, 2026-10-10).
// Reserved and not committed, so it costs no memory, and at the top, away
// from where the game's own allocations grow.
constexpr std::size_t kReserveBytes = 16 * 1024 * 1024;
void *g_reserve = nullptr;

// The process's address space as the crash found it: measured in the filter
// before the reserve is released, with nothing allocated.
struct AddressSpace
{
    std::uint64_t total = 0;
    std::uint64_t free = 0;
    std::uint64_t largestFree = 0;

    // No room for an allocation the size of the reserve: one that failed is
    // then the likely cause of the crash.
    [[nodiscard]] bool AllButFull() const
    {
        return total != 0 && largestFree < kReserveBytes;
    }
};
AddressSpace g_addressSpace;
// For an access violation: what the address space held at the address and just before its
// page, asked as the crash came in. A fault a little past the end of memory that is there
// reads differently from one in memory that never was (DAOLib #105).
MEMORY_BASIC_INFORMATION g_faultPage{};
MEMORY_BASIC_INFORMATION g_pageBefore{};

// How much of the crashing thread's stack the scans read: 64 KiB.
constexpr std::size_t kScanWords = 16384;
// A GFF4 file's header is 0x20 bytes; a file past 4 MiB is not one the
// stack holds a length for, or not one to copy in a dying process.
constexpr std::size_t kGffHeader = 0x20;
constexpr std::size_t kMostGff = 4 * 1024 * 1024;
// A name, not a document.
constexpr std::size_t kLongestString = 255;
// A hang's samples, at most, however long it lasts.
constexpr int kMostSamples = 12;
// How long without a frame is a hang (DAOLIB_HANG_SECONDS).
std::uint32_t g_hangSeconds = 5;

// The thread a log is about, as it was: its registers and its stack's first
// 64 KiB from ESP up, copied while it stood still. A crash's thread waits in
// the filter; a hang's is suspended for the copy and runs on while the log
// is written from it.
struct Sample
{
    CONTEXT context{};
    DWORD thread = 0;
    std::vector<std::uint32_t> stack;
};

// Whether [address, address + size) lies in pages that can be read without
// a fault (dao/core/Pages.h), asked of Windows region by region. The scans
// follow every stack word that looks like a pointer, and some point into a
// thread's stack guard page, whose read raises an exception TryCopy does not
// catch (DAOLib issue #9: a crash log ended mid-way). Allocates nothing, so
// it serves while a thread is suspended too.
bool CanRead(std::uintptr_t address, std::size_t size)
{
    const std::uintptr_t end = address + size;
    if (size == 0 || end < address)
    {
        return false;
    }
    for (std::uintptr_t at = address; at < end;)
    {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void *>(at), &region, sizeof region) == 0 ||
            !dao::core::IsReadablePage(region.State, region.Protect))
        {
            return false;
        }
        const auto regionEnd = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
        if (regionEnd <= at)
        {
            return false;
        }
        at = regionEnd;
    }
    return true;
}

// A copy of memory a stack word points at, where CanRead says it may be read.
bool ScanCopy(void *out, std::uintptr_t address, std::size_t size)
{
    return CanRead(address, size) && dao::TryCopy(out, address, size);
}

// The stack from esp up, to its region's end or out's, into out, with
// nothing allocated: a hang's thread is suspended meanwhile and may hold the
// heap's lock. The words copied.
std::size_t CopyStack(std::uintptr_t esp, std::span<std::uint32_t> out)
{
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(reinterpret_cast<const void *>(esp), &region, sizeof region) == 0)
    {
        return 0;
    }
    const auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
    const std::size_t words = std::min<std::size_t>(out.size(), (end - esp) / 4);
    return words != 0 && dao::TryCopy(out.data(), esp, words * 4) ? words : 0;
}

LONG WINAPI Filter(EXCEPTION_POINTERS *pointers);

LPTOP_LEVEL_EXCEPTION_FILTER WINAPI SetFilterHook(LPTOP_LEVEL_EXCEPTION_FILTER filter)
{
    // The caller gets back what it would have: the filter it set before.
    const auto previous = g_chained;
    g_chained = filter;
    return previous;
}

// ---------------------------------------------------------------------------
// The log
// ---------------------------------------------------------------------------

struct ModuleInfo
{
    std::uintptr_t base = 0;
    std::uintptr_t end = 0;
    std::string name;
    std::filesystem::path path;
};

class Writer
{
  public:
    explicit Writer(FILE *file) : file_(file)
    {
    }

    template <class... Args> void Line(fmt::format_string<Args...> format, Args &&...args)
    {
        const std::string text = fmt::format(format, std::forward<Args>(args)...);
        std::fputs(text.c_str(), file_);
        std::fputc('\n', file_);
    }

    void Blank()
    {
        std::fputc('\n', file_);
    }

    void Flush()
    {
        std::fflush(file_);
    }

  private:
    FILE *file_;
};

class Report
{
  public:
    Report()
    {
        LoadModules();
        LoadFunctions();
        InitSymbols();
    }
    Report(const Report &) = delete;
    Report &operator=(const Report &) = delete;
    ~Report()
    {
        if (symbols_)
        {
            SymCleanup(GetCurrentProcess());
        }
    }

    // The game ending itself (ExitProcess, or TerminateProcess on itself)
    // before its first frame, with no exception for the crash log to see
    // (DAOLib #44): who called, from the calling thread's own stack.
    void WriteExit(Writer &w, const Sample &sample, std::string_view how, unsigned code)
    {
        sample_ = &sample;
        Header(w, "exit logger");
        w.Line("The game called {} with code {} ({:#x}) before its first frame: it ended itself, with no", how, code,
               code);
        w.Line("exception for the crash logger. The call stack below is the calling thread's, from the call.");
        w.Blank();
        w.Flush();
        System(w);
        CallStack(w);
        w.Flush();
        ReturnAddresses(w);
        Stack(w);
        w.Flush();
        GffFiles(w);
        EngineStrings(w);
        w.Flush();
        Modules(w);
        Plugins(w);
        LastLogLines(w);
        w.Flush();
        sample_ = nullptr;
    }

    void WriteCrash(Writer &w, const Sample &sample)
    {
        sample_ = &sample;
        Header(w, "crash logger");
        Exception(w);
        AddressSpaceAtCrash(w);
        w.Flush();
        System(w);
        CallStack(w);
        w.Flush();
        Registers(w);
        ReturnAddresses(w);
        Stack(w);
        w.Flush();
        GffFiles(w);
        EngineStrings(w);
        w.Flush();
        Modules(w);
        Plugins(w);
        LastLogLines(w);
        w.Flush();
        sample_ = nullptr;
    }

    // One sample of a stall: the thread's state, then, with the first, what
    // the process is and what the logs said leading in. No call stack walk:
    // dbghelp would walk the live thread, which has moved on.
    void WriteHangSample(Writer &w, const Sample &sample, int number, double seconds, std::string_view state,
                         std::span<const Sample> others = {})
    {
        sample_ = &sample;
        if (number == 1)
        {
            Header(w, "hang logger");
            w.Line("No frame for {:.1f} s on the frame thread {}. The game {}.", seconds, sample.thread, state);
            w.Line("Sampled every {} s while it lasts, up to {} times: a sample whose return addresses", g_hangSeconds,
                   kMostSamples);
            w.Line("match the one before is a loop or a wait, one that differs is the thread making slow progress.");
            w.Blank();
            System(w);
        }
        w.Line("=== SAMPLE {}, {:.1f} s since the last frame, the game {} ===", number, seconds, state);
        w.Blank();
        Registers(w);
        ReturnAddresses(w);
        Stack(w);
        w.Flush();
        GffFiles(w);
        EngineStrings(w);
        w.Flush();
        OtherThreads(w, others);
        sample_ = &sample;
        if (number == 1)
        {
            Modules(w);
            Plugins(w);
            LastLogLines(w);
            w.Flush();
        }
        sample_ = nullptr;
    }

  private:
    // -- what an address is --

    const ModuleInfo *ModuleOf(std::uintptr_t address) const
    {
        for (const auto &m : modules_)
        {
            if (address >= m.base && address < m.end)
            {
                return &m;
            }
        }
        return nullptr;
    }

    static bool IsExe(const ModuleInfo *m)
    {
        return m != nullptr && m->base == dao::Module::Base();
    }

    // "DAOrigins.exe 0x0073CD54 FUN_0073cd10+0x44", "probe.dll+2210 QueueCommand+0x40 (Probe.cpp:77)",
    // or "" for an address in no module.
    std::string Describe(std::uintptr_t address)
    {
        const ModuleInfo *m = ModuleOf(address);
        if (m == nullptr)
        {
            return {};
        }
        if (IsExe(m))
        {
            // The names are of the GOG build's addresses. In another build
            // the address is given as that build's listing shows it, then as
            // GOG's, which is what the name is looked up by; an address GOG's
            // build has no counterpart of has neither.
            const std::uint32_t own = dao::Module::ImageAddress(address);
            const std::uint32_t preferred = dao::Module::Preferred(address);
            std::string text = fmt::format("{} {:#010x}", m->name, own);
            if (preferred != own)
            {
                text += preferred != 0 ? fmt::format(" (GOG {:#010x})", preferred) : " (not in the GOG build)";
            }
            if (const auto hit = preferred != 0 ? functions_.Find(preferred) : std::nullopt)
            {
                text += fmt::format(" {}+{:#x}", hit->name, hit->offset);
            }
            return text;
        }
        std::string text = fmt::format("{}+{:X}", m->name, address - m->base);
        if (const auto symbol = Symbol(address))
        {
            text += " " + *symbol;
        }
        return text;
    }

    std::optional<std::string> Symbol(std::uintptr_t address) const
    {
        if (!symbols_)
        {
            return std::nullopt;
        }
        alignas(SYMBOL_INFO) std::array<std::byte, sizeof(SYMBOL_INFO) + 256> storage{};
        auto *info = reinterpret_cast<SYMBOL_INFO *>(storage.data());
        info->SizeOfStruct = sizeof(SYMBOL_INFO);
        info->MaxNameLen = 256;
        DWORD64 displacement = 0;
        if (SymFromAddr(GetCurrentProcess(), address, &displacement, info) == 0)
        {
            return std::nullopt;
        }
        std::string text = fmt::format("{}+{:#x}", std::string_view(info->Name, info->NameLen), displacement);
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof line;
        DWORD column = 0;
        if ((SymGetLineFromAddr64(GetCurrentProcess(), address, &column, &line) != 0) && line.FileName != nullptr)
        {
            text += fmt::format(" ({}:{})", std::filesystem::path(line.FileName).filename().string(), line.LineNumber);
        }
        return text;
    }

    // What a register or stack value is: code, an engine object, a string,
    // or a number.
    std::string Annotate(std::uint32_t value)
    {
        if (value == 0)
        {
            return "";
        }
        if (const std::string where = Describe(value); !where.empty())
        {
            return "(" + where + ")";
        }
        // TypeName keeps every read after the object's own first word in the
        // exe's image; that first word is the one to ask about.
        if (const std::string type = CanRead(value, sizeof(std::uint32_t))
                                         ? dao::TypeName(reinterpret_cast<const void *>(value))
                                         : std::string();
            !type.empty())
        {
            return "(" + type + "*)";
        }
        if (const auto text = AsciiAt(value))
        {
            return "(char*) \"" + *text + "\"";
        }
        if (const auto text = WideAt(value))
        {
            return "(wchar_t*) L\"" + *text + "\"";
        }
        if (value < 0x10000)
        {
            return fmt::format("[{}]", value);
        }
        return "";
    }

    static std::optional<std::string> AsciiAt(std::uint32_t address)
    {
        std::array<char, 64> buffer{};
        if (!ScanCopy(buffer.data(), address, buffer.size()))
        {
            return std::nullopt;
        }
        std::size_t n = 0;
        while (n < buffer.size() && buffer[n] >= 0x20 && buffer[n] < 0x7f)
        {
            ++n;
        }
        if (n < 4 || (n < buffer.size() && buffer[n] != '\0'))
        {
            return std::nullopt;
        }
        return std::string(buffer.data(), n);
    }

    static std::optional<std::string> WideAt(std::uint32_t address)
    {
        std::array<wchar_t, 64> buffer{};
        if (!ScanCopy(buffer.data(), address, sizeof buffer))
        {
            return std::nullopt;
        }
        return dao::core::PrintableWide(buffer);
    }

    // -- sections --

    static void Header(Writer &w, std::string_view logger)
    {
        const auto fingerprint = dao::Module::Fingerprint();
        w.Line("Dragon Age: Origins, DAOrigins.exe link stamp {:#x}, image size {:#x}, entry rva {:#x}{}",
               fingerprint ? fingerprint->timeDateStamp : 0, fingerprint ? fingerprint->sizeOfImage : 0,
               fingerprint ? fingerprint->entryPointRva : 0,
               dao::Module::IsKnownExecutable() ? " (a build the address library knows)"
                                                : " -- NOT a build the address library knows");
        std::array<char, 32> when{};
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        std::strftime(when.data(), when.size(), "%Y-%m-%d %H:%M:%S", &local);
        w.Line("DAOLib {} {}, {}", logger, DAO_VERSION_STRING, when.data());
        w.Blank();
    }

    // One page's place in the address space, in a few words.
    static std::string PageSays(const MEMORY_BASIC_INFORMATION &page)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
        if (page.RegionSize == 0)
        {
            return "not known";
        }
        if (page.State == MEM_FREE)
        {
            return fmt::format("free, {} KiB of it from {:#010x}", page.RegionSize >> 10, base);
        }
        std::string_view kind = "private";
        if (page.Type == MEM_IMAGE)
        {
            kind = "a module's";
        }
        else if (page.Type == MEM_MAPPED)
        {
            kind = "a mapped file's";
        }
        return fmt::format("{}, {} memory of the mapping at {:#010x}, protection {:#x}",
                           page.State == MEM_COMMIT ? "committed" : "reserved and not committed", kind,
                           reinterpret_cast<std::uintptr_t>(page.AllocationBase), page.Protect);
    }

    void Exception(Writer &w)
    {
        const auto code = static_cast<std::uint32_t>(g_record.ExceptionCode);
        const auto name = dao::core::ExceptionName(code);
        const auto at = reinterpret_cast<std::uintptr_t>(g_record.ExceptionAddress);
        w.Line("Unhandled exception \"{}\" ({:#010x}) at {:#010x} {}", name.empty() ? "unknown" : name, code, at,
               Describe(at));
        if (code == EXCEPTION_ACCESS_VIOLATION && g_record.NumberParameters >= 2)
        {
            const auto target = static_cast<std::uint32_t>(g_record.ExceptionInformation[1]);
            w.Line("\t{} {:#010x} {}",
                   dao::core::AccessKind(static_cast<std::uint32_t>(g_record.ExceptionInformation[0])), target,
                   Annotate(target));
            w.Line("\tthe page there: {}", PageSays(g_faultPage));
            w.Line("\tthe page before: {}", PageSays(g_pageBefore));
        }
        std::array<std::uint8_t, 16> bytes{};
        if (ScanCopy(bytes.data(), at, bytes.size()))
        {
            std::string hex;
            for (const std::uint8_t b : bytes)
            {
                hex += fmt::format("{:02x} ", b);
            }
            w.Line("\tcode at the fault: {}", hex);
        }
        w.Line("\tthread {}", sample_->thread);
        w.Blank();
    }

    // The crash's own numbers, not SYSTEM's: by the time that section asks,
    // the reserve has been released and the report has allocated.
    static void AddressSpaceAtCrash(Writer &w)
    {
        if (g_addressSpace.total == 0)
        {
            return;
        }
        constexpr double kMiB = 1024.0 * 1024.0;
        w.Line("ADDRESS SPACE AT THE CRASH:");
        w.Line("\t{:.0f} of {:.0f} MiB in use, largest free block {:.1f} MiB",
               static_cast<double>(g_addressSpace.total - g_addressSpace.free) / kMiB,
               static_cast<double>(g_addressSpace.total) / kMiB,
               static_cast<double>(g_addressSpace.largestFree) / kMiB);
        if (g_addressSpace.AllButFull())
        {
            w.Line("\tAll but full: the game had nowhere left to put an allocation, and one that failed is the");
            w.Line("\tlikely cause of this crash, whatever code it came in. This log was written in {} MiB the",
                   kReserveBytes >> 20);
            w.Line("\tloader had kept back for it.");
        }
        w.Blank();
    }

    static void System(Writer &w)
    {
        w.Line("SYSTEM:");
        using RtlGetVersionFn = LONG(WINAPI *)(OSVERSIONINFOW *);
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (const auto get =
                ntdll != nullptr ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr)
        {
            OSVERSIONINFOW os{};
            os.dwOSVersionInfoSize = sizeof os;
            if (get(&os) == 0)
            {
                w.Line("\tWindows {}.{}.{}", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber);
            }
        }
        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof memory;
        if (GlobalMemoryStatusEx(&memory) != 0)
        {
            constexpr double kMiB = 1024.0 * 1024.0;
            w.Line("\tphysical memory: {:.0f} of {:.0f} MiB in use",
                   static_cast<double>(memory.ullTotalPhys - memory.ullAvailPhys) / kMiB,
                   static_cast<double>(memory.ullTotalPhys) / kMiB);
            // A 32-bit game runs out of address space long before memory:
            // this is the number that matters for DAO.
            w.Line("\taddress space: {:.0f} of {:.0f} MiB in use",
                   static_cast<double>(memory.ullTotalVirtual - memory.ullAvailVirtual) / kMiB,
                   static_cast<double>(memory.ullTotalVirtual) / kMiB);
        }
        PROCESS_MEMORY_COUNTERS counters{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof counters))
        {
            w.Line("\tprocess: {:.0f} MiB committed", static_cast<double>(counters.PagefileUsage) / (1024.0 * 1024.0));
        }
        w.Blank();
    }

    void CallStack(Writer &w)
    {
        w.Line("PROBABLE CALL STACK:");
        if (!symbols_)
        {
            w.Line("\t(dbghelp could not start; see RETURN ADDRESSES below)");
            w.Blank();
            return;
        }
        CONTEXT context = sample_->context;
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = context.Eip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Ebp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Esp;
        frame.AddrStack.Mode = AddrModeFlat;
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, sample_->thread);
        for (int i = 0; i < 48; ++i)
        {
            if ((StackWalk64(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(),
                             thread != nullptr ? thread : GetCurrentThread(), &frame, &context, nullptr,
                             SymFunctionTableAccess64, SymGetModuleBase64, nullptr) == 0) ||
                frame.AddrPC.Offset == 0)
            {
                break;
            }
            const auto pc = static_cast<std::uintptr_t>(frame.AddrPC.Offset);
            w.Line("\t[{:2}] {:#010x} {}", i, pc, Describe(pc));
        }
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
        w.Line("\t(frames in the game's own code may stop early: it has no symbols, so no frame data; the");
        w.Line("\t return addresses below come from scanning the stack instead)");
        w.Blank();
    }

    void Registers(Writer &w)
    {
        w.Line("REGISTERS:");
        const std::array<std::pair<const char *, std::uint32_t>, 10> registers{{
            {"EAX", sample_->context.Eax},
            {"EBX", sample_->context.Ebx},
            {"ECX", sample_->context.Ecx},
            {"EDX", sample_->context.Edx},
            {"ESI", sample_->context.Esi},
            {"EDI", sample_->context.Edi},
            {"EBP", sample_->context.Ebp},
            {"ESP", sample_->context.Esp},
            {"EIP", sample_->context.Eip},
            {"EFL", sample_->context.EFlags},
        }};
        for (const auto &[name, value] : registers)
        {
            w.Line("\t{} {:#010x}  {}", name, value, std::string_view(name) == "EFL" ? "" : Annotate(value));
        }
        w.Blank();
    }

    // The sample's stack, its first `most` words.
    std::span<const std::uint32_t> StackWords(std::size_t most) const
    {
        const std::span<const std::uint32_t> stack = sample_->stack;
        return stack.first(std::min(most, stack.size()));
    }

    // The process's other threads, sampled just after the frame thread: a
    // frame thread waiting for a lock (the process heap's, in a debug
    // free) shows the lock's holder among them, inside the same heap
    // functions (DAOLib #30).
    void OtherThreads(Writer &w, std::span<const Sample> others)
    {
        if (others.empty())
        {
            return;
        }
        w.Line("OTHER THREADS ({}), sampled just after the frame thread; each its instruction and first callers:",
               others.size());
        for (const Sample &other : others)
        {
            sample_ = &other;
            w.Line("  thread {}: EIP {:#010x} {}", other.thread, other.context.Eip, Describe(other.context.Eip));
            ReturnAddresses(w, 12, false);
        }
        w.Flush();
    }

    void ReturnAddresses(Writer &w, int most = 128, bool heading = true)
    {
        if (heading)
        {
            w.Line("RETURN ADDRESSES ON THE STACK (callers, innermost first; a few may be stale values):");
        }
        const auto words = StackWords(kScanWords);
        int shown = 0;
        for (std::size_t i = 0; i < words.size() && shown < most; ++i)
        {
            const std::uint32_t value = words[i];
            const ModuleInfo *m = ModuleOf(value);
            if (m == nullptr || value < m->base + 7)
            {
                continue;
            }
            std::array<std::byte, 7> before{};
            if (!ScanCopy(before.data(), value - 7, before.size()) || !dao::core::EndsWithCall(before))
            {
                continue;
            }
            w.Line("\t[ESP+{:<5X}] {:#010x} {}", i * 4, value, Describe(value));
            ++shown;
        }
        w.Blank();
    }

    void Stack(Writer &w)
    {
        w.Line("STACK:");
        const auto words = StackWords(128);
        for (std::size_t i = 0; i < words.size(); ++i)
        {
            w.Line("\t[ESP+{:<3X}] {:#010x}  {}", i * 4, words[i], Annotate(words[i]));
        }
        w.Blank();
    }

    // A GFF4 file a stack word points at: one the game is reading, and for
    // a model (MMH) its name, which says which model it was. Its length is
    // the word after it, as the game passes a file's bytes (2026-10-02:
    // every frame of a model's parse held the buffer and then 7566), or else
    // as far as the memory goes; parsed from a copy, every read checked.
    void GffFiles(Writer &w)
    {
        w.Line("GFF4 FILES ON THE STACK (a file being read, its root's strings by label: dev/gff4_fields.csv):");
        const auto words = StackWords(kScanWords);
        std::vector<std::uint32_t> seen;
        for (std::size_t i = 0; i < words.size() && seen.size() < 8; ++i)
        {
            const std::uint32_t at = words[i];
            std::array<std::byte, 8> head{};
            if (at < 0x10000 || std::ranges::find(seen, at) != seen.end() || !ScanCopy(head.data(), at, head.size()) ||
                !dao::core::IsGff4(head))
            {
                continue;
            }
            seen.push_back(at);
            const std::uint32_t next = i + 1 < words.size() ? words[i + 1] : 0;
            const std::size_t length = next >= kGffHeader && next <= kMostGff ? next : Readable(at, kMostGff);
            w.Line("\t[ESP+{:<5X}] {:#010x}, {} bytes: {}", i * 4, at, length, DescribeGff(at, length));
        }
        if (seen.empty())
        {
            w.Line("\tnone");
        }
        w.Blank();
    }

    static std::size_t Readable(std::uint32_t at, std::size_t most)
    {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void *>(at), &region, sizeof region) == 0)
        {
            return 0;
        }
        const auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
        return std::min<std::size_t>(most, end - at);
    }

    static std::string DescribeGff(std::uint32_t at, std::size_t length)
    {
        std::vector<std::byte> bytes(length);
        if (length < kGffHeader || !ScanCopy(bytes.data(), at, bytes.size()))
        {
            return "could not be copied";
        }
        // The header's platform, type and version, four letters each.
        const std::string header(reinterpret_cast<const char *>(bytes.data()) + 8, 12);
        const auto file = dao::core::gff4::File::Parse(bytes);
        if (!file)
        {
            return fmt::format("\"{}\", which does not parse", header);
        }
        std::string text = fmt::format("{} {}", file->Type(), file->TypeVersion());
        const auto root = file->Root();
        if (!root)
        {
            return text + ", no root";
        }
        text += fmt::format(", root {}", root->Label());
        int shown = 0;
        for (const auto &field : root->Shape().fields)
        {
            if (field.type != static_cast<std::uint16_t>(dao::core::gff4::Type::String) ||
                (field.flags & dao::core::gff4::kList) != 0)
            {
                continue;
            }
            if (const auto value = root->String(field.label))
            {
                text += fmt::format(", {} \"{}\"", field.label, dao::Utf8(*value));
                if (++shown == 4)
                {
                    break;
                }
            }
        }
        return text;
    }

    // The engine's std::wstrings lying in the stack's frames: a function's
    // own strings, the name of what it is loading among them (2026-10-02:
    // the model blueprint loader builds the name it asks for in its frame).
    void EngineStrings(Writer &w)
    {
        w.Line("ENGINE STRINGS IN THE STACK'S FRAMES (innermost first; a few may be stale):");
        const auto words = StackWords(kScanWords);
        std::vector<std::string> seen;
        for (std::size_t i = 0; i + 7 <= words.size() && seen.size() < 48; ++i)
        {
            const auto found = dao::core::WStringIn(std::span<const std::uint32_t, 7>(words.data() + i, 7));
            if (!found || found->size > kLongestString)
            {
                continue;
            }
            std::string text = found->text;
            if (found->pointer != 0)
            {
                std::array<wchar_t, kLongestString + 1> units{};
                const std::size_t count = found->size + 1;
                const auto read = ScanCopy(units.data(), found->pointer, count * sizeof(wchar_t))
                                      ? dao::core::PrintableWide(std::span(units.data(), count), 3)
                                      : std::nullopt;
                if (!read || read->size() != found->size)
                {
                    continue;
                }
                text = *read;
            }
            if (std::ranges::find(seen, text) == seen.end())
            {
                w.Line("\t[ESP+{:<5X}] L\"{}\"", i * 4, text);
                seen.push_back(std::move(text));
            }
            i += 6; // past this string's words
        }
        if (seen.empty())
        {
            w.Line("\tnone");
        }
        w.Blank();
    }

    void Modules(Writer &w) const
    {
        w.Line("MODULES:");
        for (const auto &m : modules_)
        {
            w.Line("\t{:<28} {:#010x}-{:#010x}", m.name, m.base, m.end);
        }
        w.Blank();
    }

    void Plugins(Writer &w) const
    {
        w.Line("DAOLIB PLUGINS:");
        for (const auto &m : modules_)
        {
            // daolib\plugins\<name>\<name>.dll (DAOLib #26).
            const auto folder = m.path.parent_path();
            if (folder.parent_path().filename() == L"plugins" &&
                folder.parent_path().parent_path().filename() == L"daolib")
            {
                w.Line("\t{}", m.name);
            }
        }
        w.Blank();
    }

    // Whether a file was written since this game started: one a game before left under a
    // name this game would use is not this game's (DAOLib #57: a plugin's log weeks old).
    static bool WrittenInThisRun(const std::filesystem::path &file)
    {
        FILETIME started{};
        FILETIME unused{};
        WIN32_FILE_ATTRIBUTE_DATA data{};
        return GetProcessTimes(GetCurrentProcess(), &started, &unused, &unused, &unused) != FALSE &&
               GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &data) != FALSE &&
               CompareFileTime(&data.ftLastWriteTime, &started) >= 0;
    }

    static void LastLogLines(Writer &w)
    {
        // Each DLL's log writes its lines out on a thread of its own, up to
        // kLogLatency after they were logged; those threads run on through a
        // crash or a stall, so waiting that long finds the last lines on disk.
        Sleep(static_cast<DWORD>(dao::kLogLatency.count()) + 50);
        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator(dao::LogDirectory(), ec))
        {
            const auto name = entry.path().filename().string();
            // This game's own logs, written in this run: the folder is shared with the games
            // running beside it and keeps what earlier ones left (DAOLib #57).
            if (!dao::core::IsOwnLog(name, dao::OwnsLogs(), GetCurrentProcessId()) || !WrittenInThisRun(entry.path()))
            {
                continue;
            }
            std::ifstream in(entry.path());
            std::deque<std::string> last;
            for (std::string line; std::getline(in, line);)
            {
                last.push_back(std::move(line));
                if (last.size() > 12)
                {
                    last.pop_front();
                }
            }
            w.Line("LAST LINES OF {}:", name);
            for (const auto &line : last)
            {
                w.Line("\t{}", line);
            }
            w.Blank();
        }
    }

    // -- set-up --

    void LoadModules()
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return;
        }
        MODULEENTRY32W entry{};
        entry.dwSize = sizeof entry;
        for (BOOL ok = Module32FirstW(snapshot, &entry); ok != 0; ok = Module32NextW(snapshot, &entry))
        {
            const auto base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
            modules_.push_back(ModuleInfo{.base = base,
                                          .end = base + entry.modBaseSize,
                                          .name = std::filesystem::path(entry.szModule).string(),
                                          .path = entry.szExePath});
        }
        CloseHandle(snapshot);
        std::ranges::sort(modules_, {}, &ModuleInfo::base);
    }

    void LoadFunctions()
    {
        // Ghidra's names for the game's functions, deployed beside the
        // plugins (tools\deploy.ps1). They are the GOG build's, and serve any
        // build whose addresses map back to it (Describe).
        if (!dao::Module::IsKnownExecutable())
        {
            return;
        }
        const auto path = g_folder / L"daolib" / L"functions.tsv";
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            return;
        }
        std::ostringstream text;
        text << in.rdbuf();
        functions_ = dao::core::FunctionTable::Parse(text.str());
    }

    void InitSymbols()
    {
        // Our PDBs sit beside our DLLs: the loader's beside the exe, each
        // plugin's in its own folder, daolib\plugins\<name> (DAOLib #26). The
        // search path names those folders and nothing else, so no symbol
        // server is asked and nothing waits on the network in a dying
        // process.
        std::wstring search = g_folder.wstring();
        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator(g_folder / L"daolib" / L"plugins", ec))
        {
            if (entry.is_directory(ec))
            {
                search += L";" + entry.path().wstring();
            }
        }
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS |
                      SYMOPT_NO_PROMPTS);
        symbols_ = SymInitializeW(GetCurrentProcess(), search.c_str(), TRUE) != FALSE;
    }

    std::vector<ModuleInfo> modules_;
    dao::core::FunctionTable functions_;
    bool symbols_ = false;
    const Sample *sample_ = nullptr; // the one being written
};

std::filesystem::path LogPath(const char *kind)
{
    std::array<char, 40> name{};
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    std::strftime(name.data(), name.size(), "-%Y-%m-%d-%H-%M-%S.log", &local);
    return dao::LogDirectory() / (std::string(kind) + name.data());
}

void WriteLog()
{
    const auto path = LogPath("crash");
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    // Shared: a peer reads a log while the game still writes it (DAOLib #31).
    FILE *file = _wfsopen(path.c_str(), L"w", _SH_DENYNO);
    if (file == nullptr)
    {
        return;
    }
    if (g_addressSpace.AllButFull())
    {
        constexpr double kMiB = 1024.0 * 1024.0;
        spdlog::critical("the game crashed with its address space all but full: {:.0f} of {:.0f} MiB in use, "
                         "largest free block {:.1f} MiB",
                         static_cast<double>(g_addressSpace.total - g_addressSpace.free) / kMiB,
                         static_cast<double>(g_addressSpace.total) / kMiB,
                         static_cast<double>(g_addressSpace.largestFree) / kMiB);
    }
    // An allocation can fail yet, the reserve notwithstanding. Uncaught on
    // this thread it would end the process with whatever of the log had not
    // been flushed lost; caught, the log says where it stopped.
    try
    {
        Writer w(file);
        Sample sample;
        sample.context = g_context;
        sample.thread = g_threadId;
        sample.stack.resize(kScanWords);
        sample.stack.resize(CopyStack(g_context.Esp, sample.stack));
        Report report;
        report.WriteCrash(w, sample);
    }
    catch (const std::exception &error)
    {
        std::fputs("\nThe log ends here: writing it failed (", file);
        std::fputs(error.what(), file);
        std::fputs(").\n", file);
    }
    std::fclose(file);
    spdlog::critical("the game crashed; the crash log is {}", path.string());
}

// Every free region between the lowest and highest addresses a process may
// use, asked of Windows one region at a time: nothing is allocated, so it
// runs in the filter with the address space as the crash left it.
AddressSpace MeasureAddressSpace()
{
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const auto low = reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress);
    const auto high = reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);
    AddressSpace space;
    space.total = static_cast<std::uint64_t>(high - low) + 1;
    MEMORY_BASIC_INFORMATION region{};
    std::uintptr_t at = low;
    while (at < high && VirtualQuery(reinterpret_cast<LPCVOID>(at), &region, sizeof region) != 0)
    {
        if (region.State == MEM_FREE)
        {
            space.free += region.RegionSize;
            space.largestFree = std::max<std::uint64_t>(space.largestFree, region.RegionSize);
        }
        const std::uintptr_t next = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
        if (next <= at)
        {
            break;
        }
        at = next;
    }
    return space;
}

DWORD WINAPI WriterThread(LPVOID /*unused*/)
{
    WriteLog();
    return 0;
}

LONG WINAPI Filter(EXCEPTION_POINTERS *pointers)
{
    if (!g_crashing.exchange(true) && pointers != nullptr && pointers->ExceptionRecord != nullptr &&
        pointers->ContextRecord != nullptr)
    {
        g_record = *pointers->ExceptionRecord;
        g_context = *pointers->ContextRecord;
        g_threadId = GetCurrentThreadId();
        // Before the writer's thread, whose stack is itself an allocation.
        g_addressSpace = MeasureAddressSpace();
        if (g_record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && g_record.NumberParameters >= 2)
        {
            constexpr ULONG_PTR kPageMask = 0xFFF;
            const ULONG_PTR page = g_record.ExceptionInformation[1] & ~kPageMask;
            VirtualQuery(reinterpret_cast<const void *>(page), &g_faultPage, sizeof g_faultPage);
            if (page != 0)
            {
                VirtualQuery(reinterpret_cast<const void *>(page - 1), &g_pageBefore, sizeof g_pageBefore);
            }
        }
        if (g_reserve != nullptr)
        {
            VirtualFree(g_reserve, 0, MEM_RELEASE);
            g_reserve = nullptr;
        }
        // The log is written on a thread of its own: this one may be out of
        // stack, and it is the one whose stack the log reads.
        HANDLE writer = CreateThread(nullptr, 0, &WriterThread, nullptr, 0, nullptr);
        if (writer != nullptr)
        {
            WaitForSingleObject(writer, 60000);
            CloseHandle(writer);
        }
        else
        {
            WriteLog();
        }
    }
    // The filter the game set, if any, sees the same exception; with none
    // to see there is nothing to hand on.
    return g_chained != nullptr && pointers != nullptr ? g_chained(pointers) : EXCEPTION_CONTINUE_SEARCH;
}

// ---------------------------------------------------------------------------
// The hang watchdog
// ---------------------------------------------------------------------------

std::atomic<std::uint64_t> g_lastFrame{0}; // GetTickCount64 at the last frame; 0 before the first
// GetTickCount64 at the first frame after a gap of the hang limit or more:
// a stall's end as the frames came back, not as the watchdog, still writing
// its sample, noticed them (DAOLib #20).
std::atomic<std::uint64_t> g_resumed{0};
// The current area's rooms at the last frame, counted on the frame thread:
// those wanted (kWanted or kWantedNow) and not loaded, and those whose load
// in the background is under way. A stall with rooms owed is most likely
// their streaming: the second stall seen, 2026-10-04 16:11, began 0.6 s
// after a camera jump, the frame thread setting up materials and
// textures (dev/CRASHES.md); this says so in the log instead of leaving it
// to be reasoned out. Outdoor areas' terrain cells (state 6) are left out
// of both: their load never reads loaded.
std::atomic<std::int32_t> g_roomsOwed{0};
std::atomic<std::int32_t> g_roomsLoading{0};

void CountRooms()
{
    std::int32_t owed = 0;
    std::int32_t loading = 0;
    for (const dao::world::Room &room : dao::world::Rooms())
    {
        // An indoor room alone: outdoors the rooms are the terrain's cells,
        // state 6, whose load never reads loaded and whose loading byte
        // stood for 179 of them in two stalls 90 s apart (DAOLib #29).
        if (room.state < dao::world::Room::kDropped || room.state > dao::world::Room::kWantedNow)
        {
            continue;
        }
        const bool wanted = room.state == dao::world::Room::kWanted || room.state == dao::world::Room::kWantedNow;
        owed += wanted && !room.Loaded() ? 1 : 0;
        loading += room.loading ? 1 : 0;
    }
    g_roomsOwed = owed;
    g_roomsLoading = loading;
}

std::uint32_t HangSeconds()
{
    std::array<char, 16> text{};
    const DWORD n = GetEnvironmentVariableA("DAOLIB_HANG_SECONDS", text.data(), static_cast<DWORD>(text.size()));
    if (n == 0 || n >= text.size())
    {
        return 5;
    }
    char *end = nullptr;
    const unsigned long value = std::strtoul(text.data(), &end, 10);
    return end != text.data() ? static_cast<std::uint32_t>(std::min(value, 3600UL)) : 5;
}

// The game's window: visible, of this process, owned by no other.
HWND GameWindow()
{
    struct Find
    {
        DWORD process = 0;
        HWND found = nullptr;
    } find{GetCurrentProcessId(), nullptr};
    EnumWindows(
        [](HWND window, LPARAM param) -> BOOL {
            auto *f = reinterpret_cast<Find *>(param);
            DWORD process = 0;
            GetWindowThreadProcessId(window, &process);
            if (process != f->process || !IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr)
            {
                return TRUE;
            }
            f->found = window;
            return FALSE;
        },
        reinterpret_cast<LPARAM>(&find));
    return find.found;
}

// What the game is doing, in words, and whether a stall now is one to log:
// not before the game has a mode, while loading or at the main menu's
// start-up, nor with the window minimised or gone. Read while the frame
// thread may run: a glance, not a lock.
struct GameState
{
    std::string text;
    bool worth = true;
};

const char *WindowWord(bool found, bool minimised, bool front)
{
    if (!found)
    {
        return "not found";
    }
    if (minimised)
    {
        return "minimised";
    }
    return front ? "in front" : "not in front";
}

GameState State()
{
    static constexpr std::array<const char *, 13> kModes{"Explore",  "Combat",  "Dead",       "Gui",     "Dialog",
                                                         "Cutscene", "FlyCam",  "Fixed",      "Pregame", "Loading",
                                                         "Movie",    "CharGen", "PartyPicker"};
    GameState state;
    const auto *app = dao::CClientExoApp::Get();
    const auto *modes = app != nullptr ? app->GameModes() : nullptr;
    const auto mode = modes != nullptr ? static_cast<std::int32_t>(modes->Mode()) : -1;
    const std::string name = mode >= 0 && std::cmp_less(mode, kModes.size())
                                 ? std::string(kModes[static_cast<std::size_t>(mode)]) + " mode"
                                 : fmt::format("mode {}", mode);
    HWND window = GameWindow();
    const bool minimised = window != nullptr && IsIconic(window) != FALSE;
    const bool front = window != nullptr && GetForegroundWindow() == window;
    const bool hung = window != nullptr && IsHungAppWindow(window) != FALSE;
    state.text =
        fmt::format("in {}, its window {}{}; at the last frame {} room(s) wanted and not loaded, {} loading", name,
                    WindowWord(window != nullptr, minimised, front), hung ? ", which Windows calls not responding" : "",
                    g_roomsOwed.load(), g_roomsLoading.load());
    // Loading and the main menu's start-up may hold frames back by design,
    // and with no window the game is closing.
    state.worth = mode >= 0 && mode != static_cast<std::int32_t>(dao::GameMode::Loading) &&
                  mode != static_cast<std::int32_t>(dao::GameMode::Pregame) && window != nullptr && !minimised;
    return state;
}

// The frame thread stopped for a moment: its registers and its stack copied
// into `buffer`, then let go. Nothing is allocated while it is stopped, since
// it may hold the heap's lock; the sample takes its copy after.
bool Capture(DWORD threadId, Sample &sample, std::span<std::uint32_t> buffer)
{
    HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, threadId);
    if (thread == nullptr)
    {
        return false;
    }
    bool taken = false;
    std::size_t words = 0;
    if (SuspendThread(thread) != static_cast<DWORD>(-1))
    {
        sample.context.ContextFlags = CONTEXT_FULL;
        taken = GetThreadContext(thread, &sample.context) != FALSE;
        words = taken ? CopyStack(sample.context.Esp, buffer) : 0;
        ResumeThread(thread);
    }
    CloseHandle(thread);
    if (taken)
    {
        sample.thread = threadId;
        sample.stack.assign(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(words));
    }
    return taken;
}

// Every thread of the process but the frame thread and this one, each
// stopped in turn and its stack copied as Capture does (nothing allocated
// while it is stopped). The snapshot is taken before any is stopped.
std::vector<Sample> OtherThreads(DWORD frameThread, std::span<std::uint32_t> buffer)
{
    std::vector<DWORD> ids;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        const DWORD process = GetCurrentProcessId();
        const DWORD self = GetCurrentThreadId();
        for (BOOL more = Thread32First(snapshot, &entry); more != FALSE; more = Thread32Next(snapshot, &entry))
        {
            if (entry.th32OwnerProcessID == process && entry.th32ThreadID != self && entry.th32ThreadID != frameThread)
            {
                ids.push_back(entry.th32ThreadID);
            }
        }
        CloseHandle(snapshot);
    }
    std::vector<Sample> others;
    others.reserve(ids.size());
    for (const DWORD id : ids)
    {
        Sample sample;
        if (Capture(id, sample, buffer))
        {
            others.push_back(std::move(sample));
        }
    }
    return others;
}

// One stall, from its first sample to the frame that ends it: one file, a
// sample every g_hangSeconds up to kMostSamples, and how long it lasted.
// A stall's hang log, begun at its first sample, and said in loader.log.
FILE *OpenHangLog(double seconds)
{
    const auto path = LogPath("hang");
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    // Shared: a peer reads a log while the game still writes it (DAOLib #31).
    FILE *file = _wfsopen(path.c_str(), L"w", _SH_DENYNO);
    if (file == nullptr)
    {
        return nullptr;
    }
    spdlog::warn("no frame for {:.1f} s; the hang log is {}", seconds, path.string());
    return file;
}

// The hang log's last line: how the stall ended, frames back or a crash.
void CloseHangLog(FILE *file, std::uint64_t last, int samples)
{
    const std::uint64_t resumed = g_resumed.load();
    const std::uint64_t end = resumed > last ? resumed : GetTickCount64();
    const double seconds = static_cast<double>(end - last) / 1000.0;
    Writer w(file);
    if (g_crashing)
    {
        w.Line("=== The game crashed {:.1f} s after the last frame; its crash log is beside this ===", seconds);
    }
    else
    {
        w.Line("=== Frames came back {:.1f} s after the last one; {} sample(s) ===", seconds, samples);
        spdlog::info("frames came back after {:.1f} s", seconds);
    }
    std::fclose(file);
}

void Stall(std::uint64_t last, std::span<std::uint32_t> buffer)
{
    const std::uint64_t interval = std::uint64_t{g_hangSeconds} * 1000;
    FILE *file = nullptr;
    std::optional<Report> report;
    int samples = 0;
    std::uint64_t next = 0;
    while (g_lastFrame.load() == last && !g_crashing)
    {
        const std::uint64_t now = GetTickCount64();
        if (samples < kMostSamples && now >= next)
        {
            next = now + interval;
            const GameState state = State();
            Sample sample;
            const auto frameThread = static_cast<DWORD>(dao::FrameThreadId());
            if (Capture(frameThread, sample, buffer))
            {
                // The other threads, at once, while whatever holds the
                // frame thread still holds it; the first sample only.
                std::vector<Sample> others;
                if (samples == 0)
                {
                    others = OtherThreads(frameThread, buffer);
                }
                if (file == nullptr)
                {
                    file = OpenHangLog(static_cast<double>(now - last) / 1000.0);
                    if (file == nullptr)
                    {
                        return;
                    }
                    report.emplace();
                }
                Writer w(file);
                report->WriteHangSample(w, sample, ++samples, static_cast<double>(now - last) / 1000.0, state.text,
                                        others);
            }
        }
        Sleep(250);
    }
    if (file != nullptr)
    {
        CloseHangLog(file, last, samples);
    }
}

DWORD WINAPI Watchdog(LPVOID /*unused*/)
{
    // Made once, before any stall: nothing may be allocated while the frame
    // thread is suspended.
    static std::array<std::uint32_t, kScanWords> buffer{};
    const std::uint64_t limit = std::uint64_t{g_hangSeconds} * 1000;
    while (!g_crashing)
    {
        Sleep(1000);
        const std::uint64_t last = g_lastFrame.load();
        if (last == 0 || dao::FrameThreadId() == 0 || GetTickCount64() - last < limit || !State().worth)
        {
            continue;
        }
        Stall(last, buffer);
    }
    return 0;
}

} // namespace

void StartWatchdog()
{
    g_hangSeconds = HangSeconds();
    if (g_hangSeconds == 0)
    {
        spdlog::info("hang watchdog: off (DAOLIB_HANG_SECONDS is 0)");
        return;
    }
    if (!dao::Module::IsKnownExecutable())
    {
        spdlog::info("hang watchdog: off, the game is not a build the address library knows");
        return;
    }
    dao::OnFrame([] {
        const std::uint64_t now = GetTickCount64();
        const std::uint64_t before = g_lastFrame.exchange(now);
        if (before != 0 && now - before >= std::uint64_t{g_hangSeconds} * 1000)
        {
            g_resumed = now;
        }
        CountRooms();
    });
    HANDLE thread = CreateThread(nullptr, 0, &Watchdog, nullptr, 0, nullptr);
    if (thread == nullptr)
    {
        spdlog::warn("hang watchdog: its thread could not start, error {}", GetLastError());
        return;
    }
    CloseHandle(thread);
    spdlog::info("hang watchdog: no frame for {} s writes hang-<time>.log in {}", g_hangSeconds,
                 dao::LogDirectory().string());
}

// ---------------------------------------------------------------------------
// The exit logger
// ---------------------------------------------------------------------------

// The game ending itself. Before its first frame that is a start that
// failed, which leaves no crash log and, until this, no word at all (the
// game exited at start with a plugin's folder as a resource source, DAOLib
// #43): an exit log with the caller. After it, a quit, which gets a line in
// loader.log.
namespace
{

using ExitProcessFn = void(WINAPI *)(UINT);
using TerminateProcessFn = BOOL(WINAPI *)(HANDLE, UINT);
ExitProcessFn g_exitProcess = nullptr;
TerminateProcessFn g_terminateProcess = nullptr;
std::atomic<bool> g_exiting{false};

void WriteExitLog(const char *how, unsigned code)
{
    if (g_exiting.exchange(true) || g_crashing)
    {
        return;
    }
    if (g_lastFrame.load() != 0)
    {
        spdlog::info("the game is ending: {}({}), after its frames had begun", how, code);
        return;
    }
    const auto path = LogPath("exit");
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    FILE *file = _wfsopen(path.c_str(), L"w", _SH_DENYNO);
    if (file == nullptr)
    {
        return;
    }
    Sample sample;
    RtlCaptureContext(&sample.context);
    sample.thread = GetCurrentThreadId();
    sample.stack.resize(kScanWords);
    sample.stack.resize(CopyStack(sample.context.Esp, sample.stack));
    Writer w(file);
    Report report;
    report.WriteExit(w, sample, how, code);
    std::fclose(file);
    spdlog::critical("the game ended itself before its first frame: {}({}); the exit log is {}", how, code,
                     path.string());
}

void WINAPI ExitProcessHook(UINT code)
{
    WriteExitLog("ExitProcess", code);
    g_exitProcess(code);
}

BOOL WINAPI TerminateProcessHook(HANDLE process, UINT code)
{
    if (process == GetCurrentProcess() || GetProcessId(process) == GetCurrentProcessId())
    {
        WriteExitLog("TerminateProcess", code);
    }
    return g_terminateProcess(process, code);
}

void InstallExitLogger(HMODULE kernel32)
{
    g_exitProcess = reinterpret_cast<ExitProcessFn>(GetProcAddress(kernel32, "ExitProcess"));
    g_terminateProcess = reinterpret_cast<TerminateProcessFn>(GetProcAddress(kernel32, "TerminateProcess"));
    if (g_exitProcess == nullptr || g_terminateProcess == nullptr || DetourTransactionBegin() != NO_ERROR)
    {
        spdlog::warn("exit logger: not installed; an exit at start leaves no word");
        return;
    }
    DetourUpdateThread(GetCurrentThread());
    if (DetourAttach(reinterpret_cast<PVOID *>(&g_exitProcess), reinterpret_cast<PVOID>(&ExitProcessHook)) !=
            NO_ERROR ||
        DetourAttach(reinterpret_cast<PVOID *>(&g_terminateProcess), reinterpret_cast<PVOID>(&TerminateProcessHook)) !=
            NO_ERROR ||
        DetourTransactionCommit() != NO_ERROR)
    {
        DetourTransactionAbort();
        spdlog::warn("exit logger: not installed; an exit at start leaves no word");
    }
}

} // namespace

void Install(const std::filesystem::path &loaderFolder)
{
    g_folder = loaderFolder;
    g_reserve = VirtualAlloc(nullptr, kReserveBytes, MEM_RESERVE | MEM_TOP_DOWN, PAGE_NOACCESS);
    g_chained = SetUnhandledExceptionFilter(&Filter);
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32 != nullptr)
    {
        InstallExitLogger(kernel32);
    }
    g_setFilter = kernel32 != nullptr
                      ? reinterpret_cast<SetFilterFn>(GetProcAddress(kernel32, "SetUnhandledExceptionFilter"))
                      : nullptr;
    if (g_setFilter == nullptr || DetourTransactionBegin() != NO_ERROR)
    {
        spdlog::warn("crash logger: installed, but not kept: the game can replace it");
        return;
    }
    DetourUpdateThread(GetCurrentThread());
    if (DetourAttach(reinterpret_cast<PVOID *>(&g_setFilter), reinterpret_cast<PVOID>(&SetFilterHook)) != NO_ERROR ||
        DetourTransactionCommit() != NO_ERROR)
    {
        DetourTransactionAbort();
        spdlog::warn("crash logger: installed, but not kept: the game can replace it");
        return;
    }
    spdlog::info("crash logger: installed; a crash writes crash-<time>.log in {}", dao::LogDirectory().string());
}

} // namespace loader::crashlog
