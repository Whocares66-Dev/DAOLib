// The par plugin: the view's draws recorded on the engine's worker threads (DAOLib #280), a prototype
// at GOG's addresses. par.txt beside the DLL holds "MODE [CHUNKS]", re-read every second:
//   0 off
//   1 census only (counts logged to par.log)
//   2 the NULL queue's items recorded in CHUNKS pieces one after another on the update thread, each into a
//     recorder of its own and spliced back in order (the proof that a chunk starting from an unknown
//     material state draws the same picture)
#include "dao/Log.h"
#include "dao/Plugin.h"

#include <Windows.h>

#include <detours/detours.h>
#include <spdlog/spdlog.h>

#include <intrin.h>
#include <malloc.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <new>
#include <thread>
#include <vector>

bool InstallDeviceVerifier(const std::filesystem::path &dir); // Verify.cpp

namespace
{

constexpr std::uint32_t kGogStamp = 0x4eb19d53;
constexpr std::uint32_t kGogImageSize = 0x009ec0a2;

std::atomic<int> g_mode{0};
std::atomic<int> g_chunkCount{4};
std::filesystem::path g_dir;

using GroupFn = void(__fastcall *)(void *group, int, void *ctx);
GroupFn g_group = nullptr;     // 0x00471CB0: one sub-group of the program queue
GroupFn g_nullQueue = nullptr; // 0x00469530: the NULL queue's submit

std::array<std::atomic<std::uint64_t>, 8> g_modeCalls{};
std::array<std::atomic<std::uint64_t>, 8> g_modeItems{};
std::array<std::atomic<std::uint64_t>, 8> g_modeTicks{};
std::atomic<std::uint64_t> g_nullTicks{0};
std::atomic<std::uint64_t> g_nullCalls{0};
std::atomic<std::uint64_t> g_nullItems{0};
std::atomic<std::uint64_t> g_chunkedFrames{0};
std::atomic<std::uint64_t> g_programBytes{0};   // bytes the program queue's submit added to the main stream
std::atomic<std::uint64_t> g_programFrames{0};
std::atomic<std::uint64_t> g_chunkedGroups{0};
std::atomic<std::uint64_t> g_chunkedWithPre{0}; // chunked groups that had pre-state objects
std::atomic<std::uint64_t> g_chunkedWithPost{0};
std::atomic<std::uint64_t> g_mostChunkBytes{0};
std::atomic<std::uint64_t> g_preStateSerial{0}; // groups drawn serially because their pre-state set tracked states

// ---- Recorders, the material state and the splice (GOG addresses, #280) ------------------------------------
//
// Renderer::ThreadedGraphicsDriver (vtable 0x00B5834C): +0 vtable, +4 the inner driver, +8 a 0x600018-byte
// buffer whose last 0x18 bytes are the control block: +0 write, +4 read, +8 end, +0xC free bytes, +0x10 the
// count of commands published. A command is finished by an InterlockedIncrement of the count.
constexpr std::uintptr_t kGraphics = 0x00C31650; // g_graphics; +0xB4 real driver, +0xB8 threaded, +0xBC/+0xC0 in use
constexpr std::uintptr_t kUpdateThreadId = 0x00C2B590;
constexpr std::uint32_t kBufferBytes = 0x600000;
constexpr std::size_t kStateBytes = 0x5F4;
constexpr std::size_t kContextBytes = 0x150;
constexpr std::uintptr_t kMainState = 0x00C30A08;

struct Control
{
    std::uint8_t *write;
    std::uint8_t *read;
    std::uint8_t *end;
    std::int32_t free;
    volatile LONG count;
};

struct Recorder
{
    void *driver = nullptr; // the ThreadedGraphicsDriver

    [[nodiscard]] std::uint8_t *Base() const
    {
        return *reinterpret_cast<std::uint8_t *const *>(static_cast<std::uint8_t *>(driver) + 8);
    }
    [[nodiscard]] Control *Ctrl() const
    {
        return reinterpret_cast<Control *>(Base() + kBufferBytes);
    }
    void Reset() const
    {
        Control *c = Ctrl();
        c->write = Base();
        c->read = Base();
        c->free = static_cast<std::int32_t>(kBufferBytes);
        c->count = 0;
    }
};

using NewFn = void *(__cdecl *)(std::size_t);
using RecorderCtorFn = void *(__fastcall *)(void *self, int, void *inner); // ret 4
using DriverNowFn = void *(__fastcall *)(void *self, int);
using ApplyFn = void(__fastcall *)(void *state, int, void *block); // MaterialStateManager apply, ret 4
using StateCtorFn = void *(__fastcall *)(void *self);
using AssignFn = void(__fastcall *)(void *handle, int, void *value); // 0x00458420, ret 4

thread_local void *t_recorder = nullptr; // the recorder this thread's draws go to, when not the engine's
thread_local void *t_state = nullptr;    // the material state this thread applies, when not the engine's

DriverNowFn g_driverNow = nullptr;   // 0x005278C0
DriverNowFn g_driverOther = nullptr; // 0x005278F0
ApplyFn g_apply = nullptr;           // 0x004FC2C0

void *__fastcall DriverNow(void *self, int edx)
{
    return t_recorder != nullptr ? t_recorder : g_driverNow(self, edx);
}

void *__fastcall DriverOther(void *self, int edx)
{
    return t_recorder != nullptr ? t_recorder : g_driverOther(self, edx);
}

void __fastcall Apply(void *state, int edx, void *block)
{
    g_apply(t_state != nullptr ? t_state : state, edx, block);
}

std::uint8_t *Graphics()
{
    return *reinterpret_cast<std::uint8_t **>(kGraphics);
}

// Whether draws are recorded for the render thread (the engine's threaded rendering is on).
bool Threaded()
{
    std::uint8_t *g = Graphics();
    if (g == nullptr)
    {
        return false;
    }
    void *threaded = *reinterpret_cast<void **>(g + 0xB8);
    return threaded != nullptr && *reinterpret_cast<void **>(g + 0xBC) == threaded;
}

Recorder MainRecorder()
{
    return Recorder{*reinterpret_cast<void **>(Graphics() + 0xB8)};
}

Recorder MakeRecorder()
{
    void *mem = reinterpret_cast<NewFn>(0x00413660)(0xC);
    void *inner = *reinterpret_cast<void **>(Graphics() + 0xB4);
    return Recorder{reinterpret_cast<RecorderCtorFn>(0x00A221E0)(mem, 0, inner)};
}

void *MakeState()
{
    void *state = _aligned_malloc(kStateBytes + 16, 16);
    std::memset(state, 0, kStateBytes + 16);
    reinterpret_cast<StateCtorFn>(0x004FCA90)(state);
    return state;
}

// The state made "unknown": its next apply emits the block's full technique and restores every default,
// whatever the state before it (read in 0x004FBB30 / 0x004FBFF0).
void MakeUnknown(void *state)
{
    auto *s = static_cast<std::uint8_t *>(state);
    *reinterpret_cast<std::int32_t *>(s + 0x24) = -1;
    reinterpret_cast<AssignFn>(0x00458420)(s + 0x1C, 0, nullptr);
    std::memset(*reinterpret_cast<void **>(s + 0x94), 1, 0x33);
    std::memset(*reinterpret_cast<void **>(s + 0x5E8), 1, 0xC0);
}

// Appends what `from` recorded to the engine's main recorder, as if it had been recorded there.
void Splice(const Recorder &from)
{
    const Recorder main = MainRecorder();
    Control *src = from.Ctrl();
    Control *dst = main.Ctrl();
    const auto bytes = static_cast<std::int32_t>(src->write - from.Base());
    if (bytes <= 0)
    {
        return;
    }
    dst->free -= bytes;
    if (dst->free <= 0)
    {
        reinterpret_cast<void(__fastcall *)(void *)>(0x00527840)(Graphics()); // drains; the buffer is then empty
        dst->free -= bytes;
    }
    std::memcpy(dst->write, from.Base(), static_cast<std::size_t>(bytes));
    dst->write += bytes;
    InterlockedExchangeAdd(&dst->count, src->count);
}

#pragma warning(push)
#pragma warning(disable : 4324) // the padding is the point: the context is 16-byte aligned
struct Chunk
{
    Recorder recorder;
    void *state = nullptr;
    alignas(16) std::uint8_t context[kContextBytes];
};
#pragma warning(pop)
std::vector<Chunk *> g_chunks;

Chunk *ChunkAt(std::size_t i)
{
    while (g_chunks.size() <= i)
    {
        auto *c = new (_aligned_malloc(sizeof(Chunk), 16)) Chunk();
        c->recorder = MakeRecorder();
        t_recorder = c->recorder.driver; // whatever the state's constructor emits goes nowhere
        c->state = MakeState();
        t_recorder = nullptr;
        spdlog::info("chunk {}: recorder {}, {} bytes recorded by the state's constructor", g_chunks.size(),
                     c->recorder.driver, c->recorder.Ctrl()->write - c->recorder.Base());
        c->recorder.Reset();
        int different = 0;
        const auto *a = static_cast<const std::uint32_t *>(c->state);
        const auto *b = reinterpret_cast<const std::uint32_t *>(kMainState);
        for (std::size_t w = 0x9C / 4; w < 0x5E8 / 4; ++w)
        {
            different += a[w] != b[w] ? 1 : 0;
        }
        spdlog::info("chunk {}: {} dwords of the tables differ from the engine's state", g_chunks.size(), different);
        g_chunks.push_back(c);
    }
    return g_chunks[i];
}


// ---- Shared state a draw writes (read in 0x0046BC40, 0x00506550) -----------------------------------------------

// The light provider (0x0046BC40, __thiscall, ret 0x10): the object's light state filled once per pass use, then
// three 64-byte blocks copied into one global scratch (0x00C1F3D0, 0xC0 bytes) that the caller is handed. On a
// chunk's thread the scratch is the thread's own.
using LightFn = void(__fastcall *)(void *lights, int, std::uint32_t p2, void **out, std::uint32_t *size, void *p5);
LightFn g_lightConstants = nullptr;
// The constants are handed on by address and read when the command is played, so each draw gets a block of its
// own from a ring (20,000 blocks to a thread) and not one scratch the next draw would overwrite.
thread_local std::uint8_t *t_lightRing = nullptr;
thread_local std::size_t t_lightNext = 0;
constexpr std::size_t kLightBlocks = 20000;

void __fastcall LightConstants(void *lights, int edx, std::uint32_t p2, void **out, std::uint32_t *size, void *p5)
{
    if (t_recorder == nullptr)
    {
        g_lightConstants(lights, edx, p2, out, size, p5);
        return;
    }
    auto *l = static_cast<std::uint8_t *>(lights);
    if (*reinterpret_cast<volatile std::int32_t *>(0x00C1F374) != *reinterpret_cast<std::int32_t *>(l + 0x1FC))
    {
        reinterpret_cast<void(__fastcall *)(void *, int, void *)>(0x0046B260)(lights, 0, p5);
    }
    auto **blocks = *reinterpret_cast<void ***>(l + 0x120);
    const std::size_t n =
        blocks == nullptr ? 0 : static_cast<std::size_t>(*reinterpret_cast<void ***>(l + 0x124) - blocks);
    if (t_lightRing == nullptr)
    {
        t_lightRing = static_cast<std::uint8_t *>(_aligned_malloc(kLightBlocks * 0xC0, 16));
    }
    std::uint8_t *scratch = t_lightRing + (t_lightNext++ % kLightBlocks) * 0xC0;
    for (std::size_t k = 0; k < 3; ++k)
    {
        const void *from = n > k ? blocks[k] : *reinterpret_cast<void *const *>(0x00C1F378);
        std::memcpy(scratch + k * 64, from, 64);
    }
    *out = scratch;
    *size = 0xC0;
}

// MaterialPass slot 3 (0x00506550, __thiscall, ret 0x14): draws with the pass, writing the pass's own state
// (its parity bit, its one-shot blocks, its parameter references). One lock per pass (striped by address),
// taken only while chunks run on several threads.
using PassFn = std::uint32_t(__fastcall *)(void *pass, int, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                           std::uint32_t);
PassFn g_pass = nullptr;
std::atomic<bool> g_parallel{false};
std::array<CRITICAL_SECTION, 64> g_passLocks;
CRITICAL_SECTION g_globalPassLock; // lock mode 1: every pass draw under one lock
std::atomic<int> g_lockMode{1};   // par.txt's third number: 0 a lock per pass, 1 one lock for all
std::atomic<std::uint64_t> g_lockedTicks{0}; // time spent inside the lock while chunks ran in parallel
std::atomic<std::uint64_t> g_lockWaitTicks{0};
std::atomic<std::uint64_t> g_driverMismatch{0};
thread_local LARGE_INTEGER t_heldSince;

// Lock mode 0: a lock per pass, taken when the instance attaches the pass to its slot (0x004F4250, which clears
// and refills the pass's binding list) and let go when the pass has drawn (0x00506550) or the instance's render
// ends, so the attach, the instance's setup of the pass (0x004F3340) and the draw are one stretch alone.
thread_local CRITICAL_SECTION *t_heldLock = nullptr;

CRITICAL_SECTION *PassLock(const void *pass)
{
    return &g_passLocks[(reinterpret_cast<std::uintptr_t>(pass) >> 4) % g_passLocks.size()];
}

void ReleaseHeld()
{
    if (t_heldLock != nullptr)
    {
        LARGE_INTEGER t2;
        QueryPerformanceCounter(&t2);
        g_lockedTicks.fetch_add(static_cast<std::uint64_t>(t2.QuadPart - t_heldSince.QuadPart), std::memory_order_relaxed);
        LeaveCriticalSection(t_heldLock);
        t_heldLock = nullptr;
    }
}

void Hold(CRITICAL_SECTION *lock)
{
    ReleaseHeld();
    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    EnterCriticalSection(lock);
    QueryPerformanceCounter(&t_heldSince);
    g_lockWaitTicks.fetch_add(static_cast<std::uint64_t>(t_heldSince.QuadPart - t0.QuadPart), std::memory_order_relaxed);
    t_heldLock = lock;
}

std::uint32_t __fastcall PassDraw(void *pass, int edx, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d,
                                  std::uint32_t e)
{
    if (!g_parallel.load(std::memory_order_relaxed) || g_lockMode.load(std::memory_order_relaxed) == 1)
    {
        return g_pass(pass, edx, a, b, c, d, e);
    }
    if (t_heldLock != PassLock(pass))
    {
        Hold(PassLock(pass));
    }
    const std::uint32_t result = g_pass(pass, edx, a, b, c, d, e);
    ReleaseHeld();
    return result;
}

using AttachFn = void(__fastcall *)(void *instance, int, void *pass, std::uint32_t index);
AttachFn g_attach = nullptr; // 0x004F4250, ret 8

void __fastcall AttachPass(void *instance, int edx, void *pass, std::uint32_t index)
{
    if (g_parallel.load(std::memory_order_relaxed) && g_lockMode.load(std::memory_order_relaxed) == 0)
    {
        Hold(PassLock(pass));
    }
    g_attach(instance, edx, pass, index);
}

// MaterialObjectInstance::Render (0x004F5840, __thiscall, ret 0xC) and RenderPasses (0x004F4AC0, ret 0x14): an
// instance draws its passes, attaching each pass to its own slot first (0x004F4250 and 0x005063B0 clear and refill
// the pass's binding list: a crash on two threads in 0x0086E7B0, play 2026-10-31). Everything a pass holds is
// then written by whoever draws with it, so while chunks run on several threads these run under one lock.
using InstanceRenderFn = std::uintptr_t(__fastcall *)(void *self, int, std::uintptr_t, std::uintptr_t, std::uintptr_t);
using InstancePassesFn = std::uintptr_t(__fastcall *)(void *self, int, std::uintptr_t, std::uintptr_t, std::uintptr_t,
                                                      std::uintptr_t, std::uintptr_t);
InstanceRenderFn g_instanceRender = nullptr;
InstancePassesFn g_instancePasses = nullptr;

struct InstanceLock
{
    bool held = false;
    LARGE_INTEGER t1{};
    InstanceLock()
    {
        if (g_parallel.load(std::memory_order_relaxed) && g_lockMode.load(std::memory_order_relaxed) == 1)
        {
            LARGE_INTEGER t0;
            QueryPerformanceCounter(&t0);
            EnterCriticalSection(&g_globalPassLock);
            QueryPerformanceCounter(&t1);
            g_lockWaitTicks.fetch_add(static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart), std::memory_order_relaxed);
            held = true;
        }
    }
    ~InstanceLock()
    {
        if (held)
        {
            LARGE_INTEGER t2;
            QueryPerformanceCounter(&t2);
            g_lockedTicks.fetch_add(static_cast<std::uint64_t>(t2.QuadPart - t1.QuadPart), std::memory_order_relaxed);
            LeaveCriticalSection(&g_globalPassLock);
        }
    }
    InstanceLock(const InstanceLock &) = delete;
    InstanceLock &operator=(const InstanceLock &) = delete;
};

std::uintptr_t __fastcall InstanceRender(void *self, int edx, std::uintptr_t a, std::uintptr_t b, std::uintptr_t c)
{
    const InstanceLock lock;
    const std::uintptr_t result = g_instanceRender(self, edx, a, b, c);
    ReleaseHeld();
    return result;
}

std::uintptr_t __fastcall InstancePasses(void *self, int edx, std::uintptr_t a, std::uintptr_t b, std::uintptr_t c,
                                         std::uintptr_t d, std::uintptr_t e)
{
    const InstanceLock lock;
    const std::uintptr_t result = g_instancePasses(self, edx, a, b, c, d, e);
    ReleaseHeld();
    return result;
}

// The engine's own GetCurrentThreadId (0x004BBD40, a jump through the import): thirteen places compare it with
// the update thread's id (0x00C2B590) and take another path off that thread (a deferred one, answering "not
// ready"): seen as constants that came out zero on a worker. On a chunk's thread those places are told it is
// the update thread, which in effect it is: it does the update thread's draw for it, the update thread waiting.
// Other callers (the threaded driver's render-thread tests, locks) see the real id.
using ThreadIdFn = DWORD(__stdcall *)();
ThreadIdFn g_threadId = nullptr;

DWORD __stdcall ThreadIdHook()
{
    if (t_recorder != nullptr)
    {
        static constexpr std::array<std::uintptr_t, 13> kUpdateThreadTests = {
            0x0040E5D6, 0x004D5BD1, 0x00529B46, 0x0052B4D3, 0x005733E9, 0x005771D9, 0x0057836A,
            0x0057A9D7, 0x00679F03, 0x0079DCC0, 0x0079E9DD, 0x00A6786C, 0x00A67B38};
        const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        if (std::find(kUpdateThreadTests.begin(), kUpdateThreadTests.end(), caller) != kUpdateThreadTests.end())
        {
            return *reinterpret_cast<DWORD *>(kUpdateThreadId);
        }
    }
    return g_threadId();
}

// A model's trees (0x00533140, __stdcall(p1, list, kind), ret 0xC; list[0] is the SpeedTreeModel): it sets the
// kind in the model's own fields (0x0052DD60: +0xBA0, +0xBA4, +0xD38) and draws the list's instances through
// 0x00530C20, which read them back. Two groups of one model on two threads wrote over each other (a null +0xBA0
// on a worker, play 2026-10-31). One lock per model while chunks run on several threads.
using TreeFn = void(__stdcall *)(std::uint32_t p1, void *list, std::uint32_t kind);
TreeFn g_tree = nullptr;
std::array<CRITICAL_SECTION, 64> g_modelLocks;

void __stdcall TreeGroup(std::uint32_t p1, void *list, std::uint32_t kind)
{
    if (!g_parallel.load(std::memory_order_relaxed) || list == nullptr)
    {
        g_tree(p1, list, kind);
        return;
    }
    const auto model = reinterpret_cast<std::uintptr_t>(*static_cast<void **>(list));
    CRITICAL_SECTION *lock = &g_modelLocks[(model >> 4) % g_modelLocks.size()];
    EnterCriticalSection(lock);
    g_tree(p1, list, kind);
    LeaveCriticalSection(lock);
}

// The workers: one thread to a chunk after the first (the update thread runs the first chunk itself). A worker
// sleeps on its own event, runs its chunk's items, and signals done.
struct Worker
{
    HANDLE wake = nullptr;
    HANDLE done = nullptr;
    std::function<void()> job;
};
std::vector<Worker *> g_workers;

void WorkerMain(Worker *w)
{
    for (;;)
    {
        WaitForSingleObject(w->wake, INFINITE);
        w->job();
        SetEvent(w->done);
    }
}

Worker *WorkerAt(std::size_t i)
{
    while (g_workers.size() <= i)
    {
        auto *w = new Worker();
        w->wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        w->done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        std::thread(WorkerMain, w).detach();
        g_workers.push_back(w);
    }
    return g_workers[i];
}

// Runs items [0, n) of a draw loop in chunks, each recorded into a recorder of its own from a copy of the
// render context and an unknown material state, then spliced into the engine's stream in order. With
// threads (mode 3) the chunks after the first run on workers at once; `body(context, i)` draws item i.
// The engine's own state is unknown afterwards, and the context is what the last chunk left, as the serial
// loop would leave it.
template <class Body> void RunChunked(std::size_t n, std::uint8_t *ctx, Body body)
{
    g_chunkedGroups.fetch_add(1, std::memory_order_relaxed);
    // At most kPerChunk items to a chunk, so a chunk's recorder (6 MB, about 40 KB used by 100 items) can never
    // fill: a full recorder would drain and reset the engine's main buffer (0x00527840).
    constexpr std::size_t kPerChunk = 400;
    constexpr std::size_t kMostChunks = 16;
    const auto wanted = static_cast<std::size_t>(g_chunkCount.load(std::memory_order_relaxed));
    const std::size_t chunks = std::min(kMostChunks, std::max(wanted, (n + kPerChunk - 1) / kPerChunk));
    ChunkAt(kMostChunks - 1); // every chunk made here, on the update thread, before any is used
    const std::size_t per = (n + chunks - 1) / chunks;
    const bool threaded = g_mode.load(std::memory_order_relaxed) == 3;
    const std::size_t used = (n + per - 1) / per;
    auto run = [&](std::size_t c) {
        const std::size_t first = c * per;
        const std::size_t count = std::min(per, n - first);
        Chunk *chunk = g_chunks[c];
        std::memcpy(chunk->context, ctx, kContextBytes);
        chunk->recorder.Reset();
        MakeUnknown(chunk->state);
        t_recorder = chunk->recorder.driver;
        t_state = chunk->state;
        for (std::size_t i = first; i < first + count; ++i)
        {
            body(chunk->context, i);
        }
        t_recorder = nullptr;
        t_state = nullptr;
    };
    if (g_mode.load(std::memory_order_relaxed) == 4)
    {
        // mode 4: every chunk on a worker, one at a time: tells a thread's own effects from races
        g_parallel.store(true);
        for (std::size_t c = 0; c < used; ++c)
        {
            Worker *w = WorkerAt(c);
            w->job = [&run, c] { run(c); };
            SetEvent(w->wake);
            WaitForSingleObject(w->done, INFINITE);
        }
        g_parallel.store(false);
    }
    else if (threaded && used > 1)
    {
        g_parallel.store(true);
        for (std::size_t c = 1; c < used; ++c)
        {
            Worker *w = WorkerAt(c - 1);
            w->job = [&run, c] { run(c); };
            SetEvent(w->wake);
        }
        run(0);
        for (std::size_t c = 1; c < used; ++c)
        {
            WaitForSingleObject(g_workers[c - 1]->done, INFINITE);
        }
        g_parallel.store(false);
    }
    else
    {
        for (std::size_t c = 0; c < used; ++c)
        {
            run(c);
        }
    }
    for (std::size_t c = 0; c < used; ++c)
    {
        Chunk *chunk = g_chunks[c];
        const auto bytes = static_cast<std::uint64_t>(chunk->recorder.Ctrl()->write - chunk->recorder.Base());
        std::uint64_t most = g_mostChunkBytes.load(std::memory_order_relaxed);
        while (bytes > most && !g_mostChunkBytes.compare_exchange_weak(most, bytes))
        {
        }
        Splice(chunk->recorder);
    }
    MakeUnknown(reinterpret_cast<void *>(kMainState)); // the engine's own state does not know what the chunks left
    std::memcpy(ctx, g_chunks[used - 1]->context, kContextBytes);
}

// The loop as the engine runs it, on the update thread and the engine's own material state.
template <class Body> void RunOrSerial(bool serial, std::size_t n, std::uint8_t *ctx, Body body)
{
    if (!serial)
    {
        RunChunked(n, ctx, body);
        return;
    }
    g_preStateSerial.fetch_add(1, std::memory_order_relaxed);
    for (std::size_t i = 0; i < n; ++i)
    {
        body(ctx, i);
    }
}

bool CanChunk(std::size_t n)
{
    return g_mode.load(std::memory_order_relaxed) >= 2 && n >= 8 && Threaded() &&
           GetCurrentThreadId() == *reinterpret_cast<std::uint32_t *>(kUpdateThreadId);
}

void *VSlot(void *object, std::size_t offset)
{
    return (*static_cast<void ***>(object))[offset / 4];
}

template <class R = void *> R Call0(void *object, std::size_t offset)
{
    return reinterpret_cast<R(__fastcall *)(void *, int)>(VSlot(object, offset))(object, 0);
}

struct Vector4
{
    void **begin;
    void **end;
    [[nodiscard]] std::size_t Size() const { return begin == nullptr ? 0 : static_cast<std::size_t>(end - begin); }
};

Vector4 VectorAt(const void *group, std::size_t offset)
{
    const auto *p = reinterpret_cast<void *const *>(static_cast<const std::uint8_t *>(group) + offset);
    return {reinterpret_cast<void **>(p[0]), reinterpret_cast<void **>(p[1])};
}

void CopyMatrices(std::uint8_t *ctx, void *item)
{
    *reinterpret_cast<void **>(ctx + 0xE8) = item;
    std::memcpy(ctx + 0x40, Call0(item, 0xC), 0x40);
    std::memcpy(ctx + 0x80, Call0(item, 0x8), 0x40);
}

// Whether a recorded stream holds a command that sets a render or sampler state (ops 0xB to 0xF, read in
// PlayCommand 0x00A1EDA0), or one this does not know the length of. A group whose pre-state sets such a state
// keeps it for the group's draws; a chunk's material state, made unknown, would restore its default first.
bool SetsStates(const Recorder &r)
{
    // words in each command, from PlayCommand; the inline ones (0x16, 0x27, 0x28) end the walk as unknown
    static constexpr std::array<std::uint8_t, 0x29> kWords = {1, 1, 1, 2, 5, 2, 2, 2, 7, 5, 2, 2, 2, 2, 3, 3, 3,
                                                               6, 8, 5, 2, 2, 0, 6, 7, 2, 2, 2, 2, 2, 2, 2, 2,
                                                               6, 4, 11, 2, 9, 5, 0, 0};
    const auto *p = reinterpret_cast<const std::uint32_t *>(r.Base());
    const auto *end = reinterpret_cast<const std::uint32_t *>(r.Ctrl()->write);
    while (p < end)
    {
        const std::uint32_t op = *p;
        if (op >= kWords.size() || kWords[op] == 0 || (op >= 0xB && op <= 0xF))
        {
            return true;
        }
        p += kWords[op];
    }
    return false;
}

Recorder g_scratch; // the pre-state's own recorder, to see what it sets

// Runs a group's pre-state into the scratch recorder, then splices it into the stream (it comes first
// either way). Returns whether it set tracked states.
bool PreStateSetsStates(void *group);

// A group's pre-state or post-state objects, each told the group's list (slot 1, vf +4).
void StateObjects(void *group, std::size_t vector)
{
    const Vector4 v = VectorAt(group, vector);
    for (std::size_t i = 0; i < v.Size(); ++i)
    {
        reinterpret_cast<void(__fastcall *)(void *, int, void *)>(VSlot(v.begin[i], 4))(
            v.begin[i], 0, static_cast<std::uint8_t *>(group) + 0x10);
    }
}

bool PreStateSetsStates(void *group)
{
    if (g_scratch.driver == nullptr)
    {
        g_scratch = MakeRecorder();
    }
    g_scratch.Reset();
    t_recorder = g_scratch.driver;
    StateObjects(group, 0x24);
    t_recorder = nullptr;
    const bool sets = SetsStates(g_scratch);
    Splice(g_scratch);
    return sets;
}

GroupFn g_mode3 = nullptr; // 0x00471440
GroupFn g_mode4 = nullptr; // 0x004709B0

// Mode 3 (0x00471440): pre-state, every item's slot 0x40, then each item drawn through slot 0x44, then post-state.
void __fastcall Mode3(void *group, int edx, void *ctxPointer)
{
    const Vector4 items = VectorAt(group, 0x14);
    if (!CanChunk(items.Size()))
    {
        g_mode3(group, edx, ctxPointer);
        return;
    }
    auto *ctx = static_cast<std::uint8_t *>(ctxPointer);
    const int value = *reinterpret_cast<int *>(static_cast<std::uint8_t *>(group) + 0x78);
    if (VectorAt(group, 0x24).Size() != 0)
    {
        g_chunkedWithPre.fetch_add(1, std::memory_order_relaxed);
    }
    if (VectorAt(group, 0x34).Size() != 0)
    {
        g_chunkedWithPost.fetch_add(1, std::memory_order_relaxed);
    }
    const bool serial = PreStateSetsStates(group);
    for (std::size_t i = 0; i < items.Size(); ++i)
    {
        Call0(items.begin[i], 0x40);
    }
    auto draw = [&](std::uint8_t *context, std::size_t i) {
        void *item = items.begin[i];
        void *material = Call0(item, 0x18);
        if (material == nullptr)
        {
            return;
        }
        void *state = *reinterpret_cast<void **>(static_cast<std::uint8_t *>(material) + 0xC);
        void *shader = *reinterpret_cast<void **>(static_cast<std::uint8_t *>(state) + 0x24);
        const int pass = reinterpret_cast<int(__fastcall *)(void *, int, int)>(VSlot(shader, 0x10))(shader, 0, value);
        CopyMatrices(context, item);
        reinterpret_cast<void(__fastcall *)(void *, int, void *, int, int)>(VSlot(item, 0x44))(item, 0, context, pass,
                                                                                                value);
    };
    RunOrSerial(serial, items.Size(), ctx, draw);
    StateObjects(group, 0x34);
}

// Mode 4 (0x004709B0): pre-state, then each item told its per-item states (list +0x44, slot 2) and drawn
// through slot 0x14, then post-state.
void __fastcall Mode4(void *group, int edx, void *ctxPointer)
{
    const Vector4 items = VectorAt(group, 0x14);
    if (!CanChunk(items.Size()))
    {
        g_mode4(group, edx, ctxPointer);
        return;
    }
    auto *ctx = static_cast<std::uint8_t *>(ctxPointer);
    const Vector4 perItem = VectorAt(group, 0x44);
    const bool serial = PreStateSetsStates(group);
    auto draw = [&](std::uint8_t *context, std::size_t i) {
        void *item = items.begin[i];
        for (std::size_t s = 0; s < perItem.Size(); ++s)
        {
            reinterpret_cast<void(__fastcall *)(void *, int, void *)>(VSlot(perItem.begin[s], 8))(perItem.begin[s], 0,
                                                                                                    item);
        }
        CopyMatrices(context, item);
        reinterpret_cast<void(__fastcall *)(void *, int, void *)>(VSlot(item, 0x14))(item, 0, context);
    };
    RunOrSerial(serial, items.Size(), ctx, draw);
    StateObjects(group, 0x34);
}

// ---- Census ---------------------------------------------------------------------------------------------------

std::uint32_t Count(const void *group, std::size_t first)
{
    const auto *p = reinterpret_cast<const std::uint32_t *>(reinterpret_cast<const std::uint8_t *>(group) + first);
    return p[0] == 0 ? 0U : (p[1] - p[0]) >> 2;
}

void __fastcall Group(void *group, int edx, void *ctx)
{
    if (g_mode.load(std::memory_order_relaxed) >= 1)
    {
        const auto mode = *reinterpret_cast<const std::uint32_t *>(reinterpret_cast<const std::uint8_t *>(group) + 0x70);
        const auto slot = mode < 7 ? mode : 7;
        g_modeCalls[slot].fetch_add(1, std::memory_order_relaxed);
        g_modeItems[slot].fetch_add(Count(group, 0x14), std::memory_order_relaxed);
        LARGE_INTEGER t0;
        LARGE_INTEGER t1;
        QueryPerformanceCounter(&t0);
        g_group(group, edx, ctx);
        QueryPerformanceCounter(&t1);
        g_modeTicks[slot].fetch_add(static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart), std::memory_order_relaxed);
        return;
    }
    g_group(group, edx, ctx);
}

void __fastcall NullQueue(void *queue, int edx, void *ctx)
{
    if (g_mode.load(std::memory_order_relaxed) >= 1)
    {
        g_nullCalls.fetch_add(1, std::memory_order_relaxed);
        g_nullItems.fetch_add(Count(queue, 0x24), std::memory_order_relaxed);
    }
    g_nullQueue(queue, edx, ctx);
}

GroupFn g_programQueue = nullptr; // 0x0046D0C0: the program queue's submit

void __fastcall ProgramQueue(void *queue, int edx, void *ctx)
{
    static std::atomic<std::uint64_t> told{0};
    if (g_mode.load(std::memory_order_relaxed) >= 1 && told.fetch_add(1) % 400 == 0)
    {
        spdlog::info("program queue: threaded {}, thread {} vs update thread {}; +B4 {} +B8 {} +BC {} +C0 {}", Threaded(),
                     GetCurrentThreadId(), *reinterpret_cast<std::uint32_t *>(kUpdateThreadId),
                     *reinterpret_cast<void **>(Graphics() + 0xB4), *reinterpret_cast<void **>(Graphics() + 0xB8),
                     *reinterpret_cast<void **>(Graphics() + 0xBC), *reinterpret_cast<void **>(Graphics() + 0xC0));
    }
    std::uint8_t *before = Threaded() ? MainRecorder().Ctrl()->write : nullptr;
    g_programQueue(queue, edx, ctx);
    if (before != nullptr)
    {
        std::uint8_t *after = MainRecorder().Ctrl()->write;
        if (after >= before)
        {
            g_programBytes.fetch_add(static_cast<std::uint64_t>(after - before), std::memory_order_relaxed);
            g_programFrames.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void Poll()
{
    for (;;)
    {
        std::ifstream in(g_dir / "par.txt");
        int mode = 0;
        int chunks = 4;
        int lockMode = 1;
        in >> mode >> chunks >> lockMode;
        g_lockMode = lockMode;
        g_chunkCount = std::max(1, chunks);
        if (mode != g_mode.exchange(mode))
        {
            spdlog::info("== mode {}, {} chunks", mode, chunks);
        }
        Sleep(1000);
    }
}

void Report()
{
    for (;;)
    {
        Sleep(5000);
        if (g_mode.load() < 1)
        {
            continue;
        }
        for (std::size_t i = 0; i < g_modeCalls.size(); ++i)
        {
            const auto calls = g_modeCalls[i].exchange(0);
            const auto items = g_modeItems[i].exchange(0);
            const auto ticks = g_modeTicks[i].exchange(0);
            if (calls != 0)
            {
                LARGE_INTEGER f;
                QueryPerformanceFrequency(&f);
                spdlog::info("group mode {}: {} calls, {} items, {:.1f} ms (5 s)", i, calls, items,
                             static_cast<double>(ticks) * 1000.0 / static_cast<double>(f.QuadPart));
            }
        }
        const auto pf = std::max<std::uint64_t>(g_programFrames.exchange(0), 1);
        spdlog::info("program queue adds {} bytes a frame; chunked groups {} (pre-state {}, post-state {})",
                     g_programBytes.exchange(0) / pf, g_chunkedGroups.exchange(0), g_chunkedWithPre.exchange(0),
                     g_chunkedWithPost.exchange(0));
        {
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            const auto ms = [&](std::uint64_t t) { return static_cast<double>(t) * 1000.0 / static_cast<double>(f.QuadPart); };
            spdlog::info("pass draws under the lock: {:.1f} ms held, {:.1f} ms waited (5 s, all threads); driver mismatches {}",
                         ms(g_lockedTicks.exchange(0)), ms(g_lockWaitTicks.exchange(0)), g_driverMismatch.load());
        }
        spdlog::info("most bytes a chunk recorded: {}; groups drawn serially for their pre-state: {}",
                     g_mostChunkBytes.load(), g_preStateSerial.exchange(0));
        spdlog::info("null queue: {} calls, {} items; chunked frames {}", g_nullCalls.exchange(0),
                     g_nullItems.exchange(0), g_chunkedFrames.exchange(0));
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("par");
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
        spdlog::error("par holds GOG's addresses, and this is not GOG's exe");
        return false;
    }
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&DAOLib_Load), &self);
    std::array<wchar_t, MAX_PATH> path{};
    GetModuleFileNameW(self, path.data(), static_cast<DWORD>(path.size()));
    g_dir = std::filesystem::path(path.data()).parent_path();

    g_group = reinterpret_cast<GroupFn>(0x00471CB0);
    g_nullQueue = reinterpret_cast<GroupFn>(0x00469530);
    g_programQueue = reinterpret_cast<GroupFn>(0x0046D0C0);
    InitializeCriticalSection(&g_globalPassLock);
    for (CRITICAL_SECTION &lock : g_passLocks)
    {
        InitializeCriticalSection(&lock);
    }
    for (CRITICAL_SECTION &lock : g_modelLocks)
    {
        InitializeCriticalSection(&lock);
    }
    g_threadId = reinterpret_cast<ThreadIdFn>(0x004BBD40);
    g_tree = reinterpret_cast<TreeFn>(0x00533140);
    g_attach = reinterpret_cast<AttachFn>(0x004F4250);
    g_instanceRender = reinterpret_cast<InstanceRenderFn>(0x004F5840);
    g_instancePasses = reinterpret_cast<InstancePassesFn>(0x004F4AC0);
    g_lightConstants = reinterpret_cast<LightFn>(0x0046BC40);
    g_pass = reinterpret_cast<PassFn>(0x00506550);
    g_mode3 = reinterpret_cast<GroupFn>(0x00471440);
    g_mode4 = reinterpret_cast<GroupFn>(0x004709B0);
    g_driverNow = reinterpret_cast<DriverNowFn>(0x005278C0);
    g_driverOther = reinterpret_cast<DriverNowFn>(0x005278F0);
    g_apply = reinterpret_cast<ApplyFn>(0x004FC2C0);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<PVOID *>(&g_group), reinterpret_cast<PVOID>(&Group));
    DetourAttach(reinterpret_cast<PVOID *>(&g_nullQueue), reinterpret_cast<PVOID>(&NullQueue));
    DetourAttach(reinterpret_cast<PVOID *>(&g_programQueue), reinterpret_cast<PVOID>(&ProgramQueue));
    DetourAttach(reinterpret_cast<PVOID *>(&g_attach), reinterpret_cast<PVOID>(&AttachPass));
    DetourAttach(reinterpret_cast<PVOID *>(&g_instanceRender), reinterpret_cast<PVOID>(&InstanceRender));
    DetourAttach(reinterpret_cast<PVOID *>(&g_instancePasses), reinterpret_cast<PVOID>(&InstancePasses));
    DetourAttach(reinterpret_cast<PVOID *>(&g_threadId), reinterpret_cast<PVOID>(&ThreadIdHook));
    DetourAttach(reinterpret_cast<PVOID *>(&g_tree), reinterpret_cast<PVOID>(&TreeGroup));
    DetourAttach(reinterpret_cast<PVOID *>(&g_lightConstants), reinterpret_cast<PVOID>(&LightConstants));
    DetourAttach(reinterpret_cast<PVOID *>(&g_pass), reinterpret_cast<PVOID>(&PassDraw));
    DetourAttach(reinterpret_cast<PVOID *>(&g_mode3), reinterpret_cast<PVOID>(&Mode3));
    DetourAttach(reinterpret_cast<PVOID *>(&g_mode4), reinterpret_cast<PVOID>(&Mode4));
    DetourAttach(reinterpret_cast<PVOID *>(&g_driverNow), reinterpret_cast<PVOID>(&DriverNow));
    DetourAttach(reinterpret_cast<PVOID *>(&g_driverOther), reinterpret_cast<PVOID>(&DriverOther));
    DetourAttach(reinterpret_cast<PVOID *>(&g_apply), reinterpret_cast<PVOID>(&Apply));
    DetourTransactionCommit();
    if (std::filesystem::exists(g_dir / "verify.txt"))
    {
        InstallDeviceVerifier(g_dir);
    }
    std::thread(Poll).detach();
    std::thread(Report).detach();
    spdlog::info("par loaded");
    return true;
}
