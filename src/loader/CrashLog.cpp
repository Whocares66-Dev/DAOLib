#include "loader/CrashLog.h"

#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/TypeName.h"
#include "dao/core/CallSite.h"
#include "dao/core/Exceptions.h"
#include "dao/core/FunctionTable.h"
#include "dao/core/Relocate.h"

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
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <optional>
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

    void Write(Writer &w)
    {
        Header(w);
        Exception(w);
        w.Flush();
        System(w);
        CallStack(w);
        w.Flush();
        Registers(w);
        ReturnAddresses(w);
        Stack(w);
        w.Flush();
        Modules(w);
        Plugins(w);
        LastLogLines(w);
        w.Flush();
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

    // -- sections --

    static void Header(Writer &w)
    {
        const auto fingerprint = dao::Module::Fingerprint();
        w.Line("Dragon Age: Origins, DAOrigins.exe link stamp {:#x}{}", fingerprint ? fingerprint->timeDateStamp : 0,
               dao::Module::IsKnownExecutable() ? " (a build the address library knows)"
                                                : " -- NOT a build the address library knows");
        std::array<char, 32> when{};
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        std::strftime(when.data(), when.size(), "%Y-%m-%d %H:%M:%S", &local);
        w.Line("DAOLib crash logger {}, {}", DAO_VERSION_STRING, when.data());
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
        w.Line("\tthread {}", g_threadId);
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
        CONTEXT context = g_context;
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = context.Eip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Ebp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Esp;
        frame.AddrStack.Mode = AddrModeFlat;
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, g_threadId);
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
            {"EAX", g_context.Eax},
            {"EBX", g_context.Ebx},
            {"ECX", g_context.Ecx},
            {"EDX", g_context.Edx},
            {"ESI", g_context.Esi},
            {"EDI", g_context.Edi},
            {"EBP", g_context.Ebp},
            {"ESP", g_context.Esp},
            {"EIP", g_context.Eip},
            {"EFL", g_context.EFlags},
        }};
        for (const auto &[name, value] : registers)
        {
            w.Line("\t{} {:#010x}  {}", name, value, std::string_view(name) == "EFL" ? "" : Annotate(value));
        }
        w.Blank();
    }

    std::vector<std::uint32_t> StackWords(std::size_t most) const
    {
        const std::uintptr_t esp = g_context.Esp;
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void *>(esp), &region, sizeof region) == 0)
        {
            return {};
        }
        const auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
        const std::size_t words = std::min<std::size_t>(most, (end - esp) / 4);
        std::vector<std::uint32_t> out(words);
        if (words == 0 || !dao::TryCopy(out.data(), esp, words * 4))
        {
            return {};
        }
        return out;
    }

    void ReturnAddresses(Writer &w)
    {
        w.Line("RETURN ADDRESSES ON THE STACK (callers, innermost first; a few may be stale values):");
        const auto words = StackWords(16384); // 64 KiB
        int shown = 0;
        for (std::size_t i = 0; i < words.size() && shown < 64; ++i)
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
};

std::filesystem::path LogPath()
{
    std::array<char, 40> name{};
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    std::strftime(name.data(), name.size(), "crash-%Y-%m-%d-%H-%M-%S.log", &local);
    return dao::LogDirectory() / name.data();
}

void WriteLog()
{
    const auto path = LogPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    FILE *file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"w") != 0 || file == nullptr)
    {
        return;
    }
    Writer w(file);
    Report report;
    report.Write(w);
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

} // namespace

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
