// The phases plugin: how long each of a list of the game's functions takes per frame (DAOLib #279).
//
// The functions are read from phases.txt beside the DLL, one "0xADDRESS name" a line, GOG's
// addresses. Each is detoured to a thunk of its own that changes no argument or register: it notes
// the time, puts a stub of ours in place of its caller's return address, and goes on into the
// function; the stub notes the time again and returns to the caller. So nothing needs the
// function's signature or calling convention. Time is counted inclusive and exclusive of the
// listed functions it calls, on the thread that runs the first function of the list (the frame's
// root, counted as the frames), and apart for every other thread.
//
// Every five seconds phases.log gets a line per function: calls and milliseconds per frame,
// inclusive and exclusive, on the root's thread and on others.
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/Plugin.h"

#include <Windows.h>

#include <detours/detours.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace
{

constexpr std::size_t kMost = 128;      // functions
constexpr std::size_t kDepth = 256;     // nested calls a thread may be in
constexpr std::uint32_t kGogStamp = 0x4eb19d53;
constexpr std::uint32_t kGogImageSize = 0x009ec0a2; // Steam's build has GOG's stamp and another size

struct Stats
{
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> inclusive{0}; // QPC ticks
    std::atomic<std::uint64_t> exclusive{0};
};

struct Phase
{
    std::uintptr_t address = 0;
    std::string name;
    void *trampoline = nullptr; // Detours' copy of the displaced start, to go on into
    Stats root;                 // on the root's thread
    Stats other;
};

std::array<Phase, kMost> g_phases;

struct BytePatch
{
    std::uintptr_t address;
    std::vector<std::uint8_t> bytes; // all within one aligned 8-byte block, written at once
    int seconds;
};
std::vector<BytePatch> g_patches;

// "!umbrascale SCALE SECONDS": Umbra's camera given SCALE times the screen's size, from SECONDS after
// the start. Umbra::Camera::setParameters(int width, int height, unsigned properties, float, float),
// a __thiscall export, called each frame from 0x00466E9F.
using SetParametersFn = void(__fastcall *)(void *camera, int, int width, int height, unsigned properties, float a,
                                           float b);
SetParametersFn g_setParameters = nullptr;
std::atomic<float> g_umbraScale{1.0F};
float g_umbraScaleWanted = 1.0F;
int g_umbraScaleSeconds = 0;

void __fastcall SetParameters(void *camera, int edx, int width, int height, unsigned properties, float a, float b)
{
    const float scale = g_umbraScale.load(std::memory_order_relaxed);
    g_setParameters(camera, edx, static_cast<int>(static_cast<float>(width) * scale),
                    static_cast<int>(static_cast<float>(height) * scale), properties, a, b);
}

// "!querynowait SECONDS": Umbra's occlusion-query poll (D3DUmbraRenderer slot 5, 0x008790D0,
// bool getResult(key, DWORD* pixels, bool wait), ret 0xC) asked never to wait; a query not yet
// answered counts as visible, one pixel, so nothing is culled on a guess.
using QueryResultFn = bool(__fastcall *)(void *renderer, int, int key, DWORD *pixels, bool wait);
QueryResultFn g_queryResult = nullptr;
std::atomic<bool> g_queryNoWait{false};
int g_queryNoWaitSeconds = -1;
std::atomic<std::uint64_t> g_queryNotReady{0};
std::atomic<std::uint64_t> g_queryAsked{0};

bool __fastcall QueryResult(void *renderer, int edx, int key, DWORD *pixels, bool wait)
{
    if (!wait || !g_queryNoWait.load(std::memory_order_relaxed))
    {
        return g_queryResult(renderer, edx, key, pixels, wait);
    }
    g_queryAsked.fetch_add(1, std::memory_order_relaxed);
    if (g_queryResult(renderer, edx, key, pixels, false))
    {
        return true;
    }
    g_queryNotReady.fetch_add(1, std::memory_order_relaxed);
    *pixels = 1;
    return true;
}

// "!commands": the engine's Umbra command handler (EclipseUmbraCommander slot 1, 0x0045C8E0,
// void command(int code), ret 4) timed by its code.
using CommandFn = void(__fastcall *)(void *commander, int, int code);
CommandFn g_command = nullptr;
bool g_timeCommands = false;
std::array<std::atomic<std::uint64_t>, 256> g_commandCalls{};
std::array<std::atomic<std::uint64_t>, 256> g_commandTicks{};

std::int64_t Ticks()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

void __fastcall Command(void *commander, int edx, int code)
{
    const std::int64_t start = Ticks();
    g_command(commander, edx, code);
    const auto slot = static_cast<std::size_t>(code) & 0xFF;
    g_commandCalls[slot].fetch_add(1, std::memory_order_relaxed);
    g_commandTicks[slot].fetch_add(static_cast<std::uint64_t>(Ticks() - start), std::memory_order_relaxed);
}

std::uint64_t Frames(); // the frames counted so far, defined with the counter below

// "!querylag SECONDS": Umbra's occlusion queries answered from the previous frame when the GPU
// has not answered yet (DAOLib #279). A query is keyed by a hash of its box's vertices, so an
// object that moved has a new key and is waited for as before. D3DUmbraRenderer (vtable
// 0x00B2EB5C) slot 3 begins a query (key) ret 4, slot 8 draws its box (Umbra OcclusionQuery*)
// ret 4, slot 5 reads its result (key, DWORD* pixels, bool wait) ret 0xC.
using QueryBeginFn = void(__fastcall *)(void *renderer, int, int key);
using QueryDrawFn = void(__fastcall *)(void *renderer, int, void *query);
using UmbraGetFn = std::uintptr_t(__fastcall *)(const void *query, int);
QueryBeginFn g_queryBegin = nullptr;
QueryDrawFn g_queryDraw = nullptr;
int g_queryLagSeconds = -1;
std::atomic<bool> g_queryLag{false};
std::unordered_map<int, std::uint32_t> g_keyOfIndex; // a query slot's box, this frame
struct LastAnswer
{
    DWORD pixels;
    std::uint64_t frame;
};
std::unordered_map<std::uint32_t, LastAnswer> g_lastAnswer; // a box's last answer
std::atomic<std::uint64_t> g_lagAnswered{0};
std::atomic<std::uint64_t> g_lagWaited{0};
std::atomic<std::uint64_t> g_lagReady{0};

std::uint32_t BoxKey(const void *query)
{
    const auto getVertices = reinterpret_cast<UmbraGetFn>(0x00A1C76E);
    const auto getVertexCount = reinterpret_cast<UmbraGetFn>(0x00A1C75C);
    const auto *vertices = reinterpret_cast<const std::uint8_t *>(getVertices(query, 0));
    const auto count = static_cast<std::size_t>(getVertexCount(query, 0));
    std::uint32_t hash = 2166136261U; // FNV-1a over the vertices' bytes
    for (std::size_t i = 0; vertices != nullptr && i < count * 12; ++i)
    {
        hash = (hash ^ vertices[i]) * 16777619U;
    }
    return hash;
}

void Remember(int key, DWORD pixels)
{
    const auto it = g_keyOfIndex.find(key);
    if (it != g_keyOfIndex.end())
    {
        g_lastAnswer[it->second] = {pixels, Frames()};
    }
}

void __fastcall QueryBegin(void *renderer, int edx, int key)
{
    if (g_queryLag.load(std::memory_order_relaxed))
    {
        DWORD pixels = 0;
        if (g_queryResult(renderer, edx, key, &pixels, false)) // the slot's previous query, before it is reused
        {
            Remember(key, pixels);
        }
        if (g_lastAnswer.size() > 200000)
        {
            g_lastAnswer.clear();
        }
    }
    g_queryBegin(renderer, edx, key);
}

void __fastcall QueryDraw(void *renderer, int edx, void *query)
{
    if (g_queryLag.load(std::memory_order_relaxed))
    {
        const auto getIndex = reinterpret_cast<UmbraGetFn>(0x00A1C70E);
        g_keyOfIndex[static_cast<int>(getIndex(query, 0))] = BoxKey(query);
    }
    g_queryDraw(renderer, edx, query);
}

bool __fastcall QueryResultLagged(void *renderer, int edx, int key, DWORD *pixels, bool wait)
{
    if (!wait || !g_queryLag.load(std::memory_order_relaxed))
    {
        return g_queryResult(renderer, edx, key, pixels, wait);
    }
    if (g_queryResult(renderer, edx, key, pixels, false))
    {
        g_lagReady.fetch_add(1, std::memory_order_relaxed);
        Remember(key, *pixels);
        return true;
    }
    const auto box = g_keyOfIndex.find(key);
    if (box != g_keyOfIndex.end())
    {
        const auto last = g_lastAnswer.find(box->second);
        if (last != g_lastAnswer.end() && last->second.frame + 2 >= Frames())
        {
            *pixels = last->second.pixels;
            g_lagAnswered.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }
    g_lagWaited.fetch_add(1, std::memory_order_relaxed);
    const bool answered = g_queryResult(renderer, edx, key, pixels, true);
    if (answered)
    {
        Remember(key, *pixels);
    }
    return answered;
}

void ApplyPatches()
{
    for (const BytePatch &patch : g_patches)
    {
        std::thread([patch] {
            Sleep(static_cast<DWORD>(patch.seconds) * 1000);
            const std::uintptr_t block = patch.address & ~std::uintptr_t{7};
            const std::size_t offset = patch.address - block;
            if (offset + patch.bytes.size() > 8)
            {
                spdlog::error("== {:#x}: {} bytes cross an 8-byte block, not patched", patch.address, patch.bytes.size());
                return;
            }
            auto *at = reinterpret_cast<volatile LONG64 *>(block);
            DWORD old = 0;
            VirtualProtect(const_cast<LONG64 *>(at), 8, PAGE_EXECUTE_READWRITE, &old);
            LONG64 was = *at;
            LONG64 now = 0;
            do
            {
                was = *at;
                now = was;
                std::memcpy(reinterpret_cast<std::uint8_t *>(&now) + offset, patch.bytes.data(), patch.bytes.size());
            } while (InterlockedCompareExchange64(at, now, was) != was);
            VirtualProtect(const_cast<LONG64 *>(at), 8, old, &old);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void *>(block), 8);
            spdlog::info("== patched {:#x}: {:016x} -> {:016x}", block, static_cast<std::uint64_t>(was),
                         static_cast<std::uint64_t>(now));
        }).detach();
    }
}
std::size_t g_count = 0;
std::atomic<DWORD> g_rootThread{0};
std::atomic<std::uint64_t> g_frames{0};

std::uint64_t Frames()
{
    return g_frames.load(std::memory_order_relaxed);
}

struct Entry
{
    std::uintptr_t returnTo;
    std::uint32_t phase;
    std::int64_t start;
    std::int64_t children; // ticks spent in listed functions it called
};

// "!sample INDEX": while the root's thread is inside the list's function INDEX (0-based), it is
// suspended about every 250 us, its stack copied, and every return address on it that follows a
// call counted, so each function's inclusive share of the phase can be read despite the frame
// pointers the exe omits. Written each 5 s to phases-samples.tsv beside the log.
std::atomic<int> g_sampleIndex{-1};
std::atomic<int> g_inTarget{0};

thread_local std::array<Entry, kDepth> t_stack;
thread_local std::uint32_t t_depth = 0;
thread_local std::uint32_t t_lost = 0; // calls past kDepth, not timed

std::int64_t Now()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

extern "C" void __cdecl ExitStub();

// slot[0] is the phase's index, pushed by its thunk; slot[1] the caller's return address. On the
// way out slot[0] holds where to go on (the trampoline) and slot[1] the stub.
extern "C" void __cdecl OnEnter(std::uintptr_t *slot)
{
    const auto index = static_cast<std::uint32_t>(slot[0]);
    Phase &phase = g_phases[index];
    slot[0] = reinterpret_cast<std::uintptr_t>(phase.trampoline);
    if (index == 0 && t_depth == 0)
    {
        g_rootThread.store(GetCurrentThreadId(), std::memory_order_relaxed);
        g_frames.fetch_add(1, std::memory_order_relaxed);
    }
    if (t_depth >= kDepth)
    {
        ++t_lost;
        return; // the caller's return address stays: this call is not timed
    }
    t_stack[t_depth++] = {slot[1], index, Now(), 0};
    slot[1] = reinterpret_cast<std::uintptr_t>(&ExitStub);
    if (static_cast<int>(index) == g_sampleIndex.load(std::memory_order_relaxed) &&
        GetCurrentThreadId() == g_rootThread.load(std::memory_order_relaxed))
    {
        g_inTarget.fetch_add(1, std::memory_order_relaxed);
    }
}

// slot[0] receives where the function was to return to.
extern "C" void __cdecl OnExit(std::uintptr_t *slot)
{
    const std::int64_t end = Now();
    const Entry entry = t_stack[--t_depth];
    slot[0] = entry.returnTo;
    if (static_cast<int>(entry.phase) == g_sampleIndex.load(std::memory_order_relaxed) &&
        GetCurrentThreadId() == g_rootThread.load(std::memory_order_relaxed))
    {
        g_inTarget.fetch_sub(1, std::memory_order_relaxed);
    }
    const auto total = static_cast<std::uint64_t>(end - entry.start);
    if (t_depth > 0)
    {
        t_stack[t_depth - 1].children += static_cast<std::int64_t>(total);
    }
    Stats &stats = GetCurrentThreadId() == g_rootThread.load(std::memory_order_relaxed) ? g_phases[entry.phase].root
                                                                                         : g_phases[entry.phase].other;
    stats.calls.fetch_add(1, std::memory_order_relaxed);
    // A recursive call's time is already in its outermost call's: inclusive counts the outermost alone.
    bool nested = false;
    for (std::uint32_t i = 0; i < t_depth && !nested; ++i)
    {
        nested = t_stack[i].phase == entry.phase;
    }
    if (!nested)
    {
        stats.inclusive.fetch_add(total, std::memory_order_relaxed);
    }
    stats.exclusive.fetch_add(total - static_cast<std::uint64_t>(entry.children), std::memory_order_relaxed);
}

// The common entry: [esp] the phase's index, [esp+4] the caller's return address. Every register
// and the flags are kept, so the function sees what its caller left.
extern "C" __declspec(naked) void __cdecl CommonEnter()
{
    __asm {
        pushfd
        pushad
        lea eax, [esp + 36]
        push eax
        call OnEnter
        add esp, 4
        popad
        popfd
        ret // into the trampoline, with the stub (or the caller) as the return address
    }
}

// Where a timed function returns: eax, edx and the x87 stack hold its result and are kept (the
// handlers use no floating point).
extern "C" __declspec(naked) void __cdecl ExitStub()
{
    __asm {
        push eax // a slot for the return address
        pushfd
        pushad
        lea eax, [esp + 36]
        push eax
        call OnExit
        add esp, 4
        popad
        popfd
        ret
    }
}

// A thunk per function: push its index, jump to the common entry. Ten bytes each.
std::uint8_t *MakeThunks(std::size_t count)
{
    auto *code = static_cast<std::uint8_t *>(
        VirtualAlloc(nullptr, count * 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (code == nullptr)
    {
        return nullptr;
    }
    for (std::size_t i = 0; i < count; ++i)
    {
        std::uint8_t *p = code + i * 16;
        p[0] = 0x68; // push imm32
        const auto index = static_cast<std::uint32_t>(i);
        std::memcpy(p + 1, &index, 4);
        p[5] = 0xE9; // jmp rel32
        const auto rel = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&CommonEnter) -
                                                   reinterpret_cast<std::intptr_t>(p + 10));
        std::memcpy(p + 6, &rel, 4);
    }
    FlushInstructionCache(GetCurrentProcess(), code, count * 16);
    return code;
}

std::filesystem::path Here()
{
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&Here), &self);
    std::wstring path(MAX_PATH, L'\0');
    path.resize(GetModuleFileNameW(self, path.data(), MAX_PATH));
    return std::filesystem::path(path).parent_path();
}

void ReadList()
{
    std::ifstream in(Here() / "phases.txt");
    std::string line;
    while (g_count < kMost && std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        std::istringstream fields(line);
        std::string address;
        std::string name;
        fields >> address >> name;
        if (address == "!dump") // "!dump 0xGLOBAL SECONDS": the pointer a global holds, and its vtable, logged
        {
            const std::uintptr_t global = std::stoul(name, nullptr, 16);
            int seconds = 0;
            fields >> seconds;
            std::thread([global, seconds] {
                Sleep(static_cast<DWORD>(seconds) * 1000);
                const auto object = *reinterpret_cast<const std::uintptr_t *>(global);
                const auto vtable = object != 0 ? *reinterpret_cast<const std::uintptr_t *>(object) : 0;
                spdlog::info("== dump {:#x}: object {:#x}, vtable {:#x}", global, object, vtable);
            }).detach();
            continue;
        }
        if (address == "!sample")
        {
            g_sampleIndex = std::stoi(name);
            continue;
        }
        if (address == "!commands")
        {
            g_timeCommands = true;
            continue;
        }
        if (address == "!querylag")
        {
            g_queryLagSeconds = std::stoi(name);
            continue;
        }
        if (address == "!querynowait")
        {
            g_queryNoWaitSeconds = std::stoi(name);
            continue;
        }
        if (address == "!umbrascale")
        {
            g_umbraScaleWanted = std::stof(name);
            fields >> g_umbraScaleSeconds;
            continue;
        }
        if (address == "!patch") // "!patch 0xADDRESS HEXBYTES SECONDS": bytes changed that long after the start
        {
            std::string hex;
            int seconds = 0;
            fields >> hex >> seconds;
            BytePatch patch{std::stoul(name, nullptr, 16), {}, seconds};
            for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
            {
                patch.bytes.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
            }
            g_patches.push_back(patch);
            continue;
        }
        g_phases[g_count].address = std::stoul(address, nullptr, 16);
        g_phases[g_count].name = name;
        ++g_count;
    }
}

bool Place()
{
    std::uint8_t *thunks = MakeThunks(g_count);
    if (thunks == nullptr || DetourTransactionBegin() != NO_ERROR)
    {
        return false;
    }
    DetourUpdateThread(GetCurrentThread());
    for (std::size_t i = 0; i < g_count; ++i)
    {
        Phase &phase = g_phases[i];
        phase.trampoline = reinterpret_cast<void *>(phase.address);
        if (DetourAttach(&phase.trampoline, thunks + i * 16) != NO_ERROR)
        {
            spdlog::error("{} at {:#x}: Detours refused it", phase.name, phase.address);
            DetourTransactionAbort();
            return false;
        }
    }
    return DetourTransactionCommit() == NO_ERROR;
}

void Report()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    const double msPerTick = 1000.0 / static_cast<double>(frequency.QuadPart);
    std::vector<std::array<std::uint64_t, 6>> last(g_count);
    std::uint64_t lastFrames = 0;
    std::int64_t lastTime = Now();
    for (;;)
    {
        Sleep(5000);
        const std::uint64_t frames = g_frames.load();
        const std::int64_t time = Now();
        const auto n = static_cast<double>(std::max<std::uint64_t>(frames - lastFrames, 1));
        const double seconds = static_cast<double>(time - lastTime) * msPerTick / 1000.0;
        spdlog::info("-- {:.0f} frames in {:.1f} s, {:.2f} ms a frame", n, seconds, seconds * 1000.0 / n);
        lastFrames = frames;
        lastTime = time;
        std::vector<std::pair<double, std::string>> lines;
        for (std::size_t i = 0; i < g_count; ++i)
        {
            const Phase &p = g_phases[i];
            const std::array<std::uint64_t, 6> now{p.root.calls,  p.root.inclusive,  p.root.exclusive,
                                                   p.other.calls, p.other.inclusive, p.other.exclusive};
            std::array<double, 6> d{};
            for (std::size_t k = 0; k < 6; ++k)
            {
                d[k] = static_cast<double>(now[k] - last[i][k]);
            }
            last[i] = now;
            if (d[0] == 0 && d[3] == 0)
            {
                continue;
            }
            lines.emplace_back(d[1] * msPerTick / n,
                               fmt::format("{:8.3f} ms incl {:8.3f} excl {:7.2f} calls | others {:8.3f} ms {:7.2f} calls  {} {:#x}",
                                           d[1] * msPerTick / n, d[2] * msPerTick / n, d[0] / n, d[4] * msPerTick / n,
                                           d[3] / n, p.name, p.address));
        }
        std::sort(lines.begin(), lines.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
        for (const auto &line : lines)
        {
            spdlog::info("{}", line.second);
        }
        spdlog::default_logger()->flush();
    }
}

// Whether `v` is an address in the exe's code just after a call instruction.
bool AfterCall(std::uint32_t v)
{
    constexpr std::uint32_t kTextLo = 0x00401006;
    constexpr std::uint32_t kTextHi = 0x00AD96AA;
    if (v < kTextLo || v > kTextHi)
    {
        return false;
    }
    const auto *p = reinterpret_cast<const std::uint8_t *>(v);
    return p[-5] == 0xE8 ||                                                          // call rel32
           (p[-2] == 0xFF && (p[-1] & 0xF8) == 0xD0) ||                              // call reg
           (p[-2] == 0xFF && (p[-1] & 0x38) == 0x10 && (p[-1] & 0xC0) == 0x00) ||  // call [reg]
           (p[-3] == 0xFF && (p[-2] & 0x38) == 0x10 && (p[-2] & 0xC0) == 0x40) ||  // call [reg+d8]
           (p[-6] == 0xFF && (p[-5] & 0x38) == 0x10);                                // call [d32], [reg+d32]
}

// Ghidra's function starts (game\ghidra\functions.tsv), for the sampler's walk.
std::vector<std::uint32_t> g_functionStarts;

void LoadFunctions()
{
    std::ifstream in(R"(C:\project\DAOLib\game\ghidraunctions.tsv)");
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line[0] != '#')
        {
            g_functionStarts.push_back(static_cast<std::uint32_t>(std::stoul(line.substr(0, 8), nullptr, 16)));
        }
    }
    std::sort(g_functionStarts.begin(), g_functionStarts.end());
}

// The start of the function holding `address`, or 0 outside the exe's code.
std::uint32_t FunctionOf(std::uint32_t address)
{
    if (address < 0x00401000 || address > 0x00AD96AA)
    {
        return 0;
    }
    auto it = std::upper_bound(g_functionStarts.begin(), g_functionStarts.end(), address);
    return it == g_functionStarts.begin() ? 0 : *(it - 1);
}

// Whether the call just before `v` can have called the function starting at `current`: a direct
// call must target it (or a thunk in it); an indirect one cannot be checked and is taken.
bool CallsInto(std::uint32_t v, std::uint32_t current)
{
    const auto *p = reinterpret_cast<const std::uint8_t *>(v);
    if (p[-5] != 0xE8 || current == 0)
    {
        return true;
    }
    std::int32_t rel = 0;
    std::memcpy(&rel, p - 4, 4);
    const std::uint32_t target = v + static_cast<std::uint32_t>(rel);
    return target == current || FunctionOf(target) == current;
}

void Sampler()
{
    LoadFunctions();
    // The root's thread is known once the first frame has run.
    while (g_rootThread.load() == 0)
    {
        Sleep(100);
    }
    HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE,
                               g_rootThread.load());
    if (thread == nullptr)
    {
        spdlog::error("sampler: the root's thread could not be opened ({})", GetLastError());
        return;
    }
    constexpr std::size_t kCopy = 32 * 1024;
    static std::array<std::uint32_t, kCopy / 4> copy;
    std::unordered_map<std::uint32_t, std::uint32_t> returns;
    std::unordered_map<std::uint32_t, std::uint32_t> eips;
    std::vector<std::uint32_t> seen;
    std::uint64_t samples = 0;
    std::int64_t lastWrite = Now();
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    const std::int64_t interval = frequency.QuadPart / 4000;
    const auto file = dao::LogDirectory() / "phases-samples.tsv";
    std::ofstream(file, std::ios::trunc) << "# phases-samples: window, kind, address, count (lib-agent #279)\n";
    std::uint32_t window = 0;
    for (;;)
    {
        const std::int64_t next = Now() + interval;
        while (Now() < next)
        {
            YieldProcessor();
        }
        if (g_inTarget.load(std::memory_order_relaxed) > 0 && SuspendThread(thread) != static_cast<DWORD>(-1))
        {
            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;
            std::size_t words = 0;
            bool inside = false;
            if (GetThreadContext(thread, &context) != 0)
            {
                inside = g_inTarget.load(std::memory_order_relaxed) > 0;
                MEMORY_BASIC_INFORMATION region{};
                if (inside && VirtualQuery(reinterpret_cast<void *>(context.Esp), &region, sizeof region) != 0)
                {
                    const auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
                    const std::size_t bytes = std::min<std::size_t>(kCopy, end - context.Esp);
                    std::memcpy(copy.data(), reinterpret_cast<const void *>(context.Esp), bytes);
                    words = bytes / 4;
                }
            }
            ResumeThread(thread);
            if (inside)
            {
                ++samples;
                ++eips[context.Eip];
                seen.clear();
                std::uint32_t current = FunctionOf(context.Eip);
                for (std::size_t i = 0; i < words; ++i)
                {
                    const std::uint32_t v = copy[i];
                    if (!AfterCall(v) || !CallsInto(v, current))
                    {
                        continue; // not a return address, or a stale one off the chain
                    }
                    if (std::find(seen.begin(), seen.end(), v) == seen.end())
                    {
                        seen.push_back(v);
                        ++returns[v];
                    }
                    current = FunctionOf(v);
                }
            }
        }
        if (Now() - lastWrite > frequency.QuadPart * 5)
        {
            lastWrite = Now();
            std::ofstream out(file, std::ios::app);
            out << window << "\tsamples\t0\t" << samples << "\n";
            for (const auto &[address, count] : eips)
            {
                out << window << "\teip\t" << std::hex << address << std::dec << "\t" << count << "\n";
            }
            for (const auto &[address, count] : returns)
            {
                out << window << "\tret\t" << std::hex << address << std::dec << "\t" << count << "\n";
            }
            ++window;
            samples = 0;
            eips.clear();
            returns.clear();
        }
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("phases");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion)
    {
        spdlog::error("not the loader this was built for");
        return false;
    }
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(GetModuleHandleW(nullptr));
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(reinterpret_cast<const std::uint8_t *>(dos) +
                                                                  dos->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kGogStamp || nt->OptionalHeader.SizeOfImage != kGogImageSize)
    {
        spdlog::error("phases.txt holds GOG's addresses, and this is not GOG's exe");
        return false;
    }
    ReadList();
    if (g_count == 0 || !Place())
    {
        spdlog::error("{} functions read, none placed", g_count);
        return false;
    }
    spdlog::info("{} functions timed; the first, {}, is the frame", g_count, g_phases[0].name);
    ApplyPatches();
    if (g_umbraScaleWanted != 1.0F)
    {
        g_setParameters = reinterpret_cast<SetParametersFn>(0x00A1C66C);
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(reinterpret_cast<PVOID *>(&g_setParameters), reinterpret_cast<PVOID>(&SetParameters));
        DetourTransactionCommit();
        std::thread([] {
            Sleep(static_cast<DWORD>(g_umbraScaleSeconds) * 1000);
            g_umbraScale = g_umbraScaleWanted;
            spdlog::info("== patched: Umbra's camera at {:.2f} of the screen", g_umbraScaleWanted);
        }).detach();
    }
    if (g_sampleIndex.load() >= 0)
    {
        std::thread(Sampler).detach();
    }
    if (g_timeCommands)
    {
        g_command = reinterpret_cast<CommandFn>(0x0045C8E0);
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(reinterpret_cast<PVOID *>(&g_command), reinterpret_cast<PVOID>(&Command));
        DetourTransactionCommit();
        std::thread([] {
            LARGE_INTEGER frequency;
            QueryPerformanceFrequency(&frequency);
            std::uint64_t lastFrames = g_frames.load();
            for (;;)
            {
                Sleep(5000);
                const std::uint64_t frames = g_frames.load();
                const auto n = static_cast<double>(std::max<std::uint64_t>(frames - lastFrames, 1));
                lastFrames = frames;
                for (std::size_t code = 0; code < 256; ++code)
                {
                    const auto calls = g_commandCalls[code].exchange(0);
                    const auto ticks = g_commandTicks[code].exchange(0);
                    if (calls != 0)
                    {
                        spdlog::info("   command {:#04x}: {:8.2f} calls {:8.3f} ms a frame", code,
                                     static_cast<double>(calls) / n,
                                     static_cast<double>(ticks) * 1000.0 / static_cast<double>(frequency.QuadPart) / n);
                    }
                }
            }
        }).detach();
    }
    if (g_queryLagSeconds >= 0)
    {
        g_queryResult = reinterpret_cast<QueryResultFn>(0x008790D0);
        g_queryBegin = reinterpret_cast<QueryBeginFn>(0x00879090);
        g_queryDraw = reinterpret_cast<QueryDrawFn>(0x00877640);
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(reinterpret_cast<PVOID *>(&g_queryResult), reinterpret_cast<PVOID>(&QueryResultLagged));
        DetourAttach(reinterpret_cast<PVOID *>(&g_queryBegin), reinterpret_cast<PVOID>(&QueryBegin));
        DetourAttach(reinterpret_cast<PVOID *>(&g_queryDraw), reinterpret_cast<PVOID>(&QueryDraw));
        DetourTransactionCommit();
        std::thread([] {
            Sleep(static_cast<DWORD>(g_queryLagSeconds) * 1000);
            g_queryLag = true;
            spdlog::info("== patched: occlusion queries answered from the previous frame when not ready");
            for (;;)
            {
                Sleep(5000);
                spdlog::info("   queries: ready {}, answered from last frame {}, waited {}", g_lagReady.exchange(0),
                             g_lagAnswered.exchange(0), g_lagWaited.exchange(0));
            }
        }).detach();
    }
    if (g_queryNoWaitSeconds >= 0)
    {
        g_queryResult = reinterpret_cast<QueryResultFn>(0x008790D0);
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(reinterpret_cast<PVOID *>(&g_queryResult), reinterpret_cast<PVOID>(&QueryResult));
        DetourTransactionCommit();
        std::thread([] {
            Sleep(static_cast<DWORD>(g_queryNoWaitSeconds) * 1000);
            g_queryNoWait = true;
            spdlog::info("== patched: occlusion queries not waited for, an unanswered one visible");
            for (;;)
            {
                Sleep(5000);
                spdlog::info("   queries waited on before: {}, not ready and taken as visible: {}", g_queryAsked.exchange(0),
                             g_queryNotReady.exchange(0));
            }
        }).detach();
    }
    std::thread(Report).detach();
    return true;
}
