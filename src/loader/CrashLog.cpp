#include "loader/CrashLog.h"

#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/TypeName.h"
#include "dao/core/CallSite.h"
#include "dao/core/Exceptions.h"
#include "dao/core/FunctionTable.h"
#include "dao/core/Gff4.h"
#include "dao/core/Relocate.h"
#include "dao/core/StackText.h"

#include <Windows.h>

#include <DbgHelp.h>
#include <Psapi.h>
#include <TlHelp32.h>
#include <detours/detours.h>
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

    void WriteCrash(Writer &w, const Sample &sample)
    {
        sample_ = &sample;
        Header(w, "crash logger");
        Exception(w);
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
    void WriteHangSample(Writer &w, const Sample &sample, int number, double seconds, std::string_view state)
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

    bool IsExe(const ModuleInfo *m) const
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

    std::optional<std::string> Symbol(std::uintptr_t address)
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
        if (!SymFromAddr(GetCurrentProcess(), address, &displacement, info))
        {
            return std::nullopt;
        }
        std::string text = fmt::format("{}+{:#x}", std::string_view(info->Name, info->NameLen), displacement);
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof line;
        DWORD column = 0;
        if (SymGetLineFromAddr64(GetCurrentProcess(), address, &column, &line) && line.FileName != nullptr)
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
        if (const std::string type = dao::TypeName(reinterpret_cast<const void *>(value)); !type.empty())
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
        if (!dao::TryCopy(buffer.data(), address, buffer.size()))
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
        if (!dao::TryCopy(buffer.data(), address, sizeof buffer))
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
        }
        std::array<std::uint8_t, 16> bytes{};
        if (dao::TryCopy(bytes.data(), at, bytes.size()))
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
        if (GlobalMemoryStatusEx(&memory))
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
            if (!StackWalk64(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(),
                             thread != nullptr ? thread : GetCurrentThread(), &frame, &context, nullptr,
                             SymFunctionTableAccess64, SymGetModuleBase64, nullptr) ||
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

    void ReturnAddresses(Writer &w)
    {
        w.Line("RETURN ADDRESSES ON THE STACK (callers, innermost first; a few may be stale values):");
        const auto words = StackWords(kScanWords);
        int shown = 0;
        for (std::size_t i = 0; i < words.size() && shown < 128; ++i)
        {
            const std::uint32_t value = words[i];
            const ModuleInfo *m = ModuleOf(value);
            if (m == nullptr || value < m->base + 7)
            {
                continue;
            }
            std::array<std::byte, 7> before{};
            if (!dao::TryCopy(before.data(), value - 7, before.size()) || !dao::core::EndsWithCall(before))
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
            if (at < 0x10000 || std::ranges::find(seen, at) != seen.end() ||
                !dao::TryCopy(head.data(), at, head.size()) || !dao::core::IsGff4(head))
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
        if (length < kGffHeader || !dao::TryCopy(bytes.data(), at, bytes.size()))
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
                const auto read = dao::TryCopy(units.data(), found->pointer, count * sizeof(wchar_t))
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
            if (m.path.parent_path().filename() == L"plugins" &&
                m.path.parent_path().parent_path().filename() == L"daolib")
            {
                w.Line("\t{}", m.name);
            }
        }
        w.Blank();
    }

    static void LastLogLines(Writer &w)
    {
        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator(dao::LogDirectory(), ec))
        {
            const auto name = entry.path().filename().string();
            if (entry.path().extension() != ".log" || name.starts_with("crash-"))
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
        for (BOOL ok = Module32FirstW(snapshot, &entry); ok; ok = Module32NextW(snapshot, &entry))
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
        // Our PDBs sit beside our DLLs; the search path names those folders
        // and nothing else, so no symbol server is asked and nothing waits
        // on the network in a dying process.
        const std::wstring search = g_folder.wstring() + L";" + (g_folder / L"daolib" / L"plugins").wstring();
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
    FILE *file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"w") != 0 || file == nullptr)
    {
        return;
    }
    Writer w(file);
    Sample sample;
    sample.context = g_context;
    sample.thread = g_threadId;
    sample.stack.resize(kScanWords);
    sample.stack.resize(CopyStack(g_context.Esp, sample.stack));
    Report report;
    report.WriteCrash(w, sample);
    std::fclose(file);
    spdlog::critical("the game crashed; the crash log is {}", path.string());
}

DWORD WINAPI WriterThread(LPVOID)
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

GameState State()
{
    static constexpr std::array<const char *, 13> kModes{"Explore",  "Combat",  "Dead",       "Gui",     "Dialog",
                                                         "Cutscene", "FlyCam",  "Fixed",      "Pregame", "Loading",
                                                         "Movie",    "CharGen", "PartyPicker"};
    GameState state;
    const auto *app = dao::CClientExoApp::Get();
    const auto *modes = app != nullptr ? app->GameModes() : nullptr;
    const auto mode = modes != nullptr ? static_cast<std::int32_t>(modes->Mode()) : -1;
    const std::string name = mode >= 0 && mode < static_cast<std::int32_t>(kModes.size())
                                 ? std::string(kModes[static_cast<std::size_t>(mode)]) + " mode"
                                 : fmt::format("mode {}", mode);
    HWND window = GameWindow();
    const bool minimised = window != nullptr && IsIconic(window) != FALSE;
    const bool front = window != nullptr && GetForegroundWindow() == window;
    const bool hung = window != nullptr && IsHungAppWindow(window) != FALSE;
    state.text = fmt::format("in {}, its window {}{}", name,
                             window == nullptr ? "not found"
                             : minimised       ? "minimised"
                             : front           ? "in front"
                                               : "not in front",
                             hung ? ", which Windows calls not responding" : "");
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

// One stall, from its first sample to the frame that ends it: one file, a
// sample every g_hangSeconds up to kMostSamples, and how long it lasted.
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
            if (Capture(static_cast<DWORD>(dao::FrameThreadId()), sample, buffer))
            {
                if (file == nullptr)
                {
                    const auto path = LogPath("hang");
                    std::error_code ec;
                    std::filesystem::create_directories(path.parent_path(), ec);
                    if (_wfopen_s(&file, path.c_str(), L"w") != 0 || file == nullptr)
                    {
                        return;
                    }
                    report.emplace();
                    spdlog::warn("no frame for {:.1f} s; the hang log is {}", static_cast<double>(now - last) / 1000.0,
                                 path.string());
                }
                Writer w(file);
                report->WriteHangSample(w, sample, ++samples, static_cast<double>(now - last) / 1000.0, state.text);
            }
        }
        Sleep(250);
    }
    if (file != nullptr)
    {
        const double seconds = static_cast<double>(GetTickCount64() - last) / 1000.0;
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
}

DWORD WINAPI Watchdog(LPVOID)
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
    dao::OnFrame([] { g_lastFrame = GetTickCount64(); });
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

void Install(const std::filesystem::path &loaderFolder)
{
    g_folder = loaderFolder;
    g_chained = SetUnhandledExceptionFilter(&Filter);
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
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
