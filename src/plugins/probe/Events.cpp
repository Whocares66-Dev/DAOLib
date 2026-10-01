#include "Events.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/SafeRead.h"
#include "dao/ScriptHooks.h"
#include "dao/Structures.h"
#include "dao/TypeName.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace probe
{

namespace
{

// A CAIEvent as its constructor (0x0079BDA0) and the queue's insert
// (0x0064A630) lay it out: after the vtable, the day and time it is due,
// the caller, the target, the queue's id for the kind of event, and the
// kind's data (a CScriptEvent for id 10, SignalEvent's).
struct Node
{
    std::uint32_t vtable;
    std::uint32_t day;
    std::uint32_t time;
    std::uint32_t caller;
    std::uint32_t target;
    std::int32_t id;
    std::uint32_t data;
};
static_assert(sizeof(Node) == 0x1C);

constexpr std::int32_t kSignalEvent = 10;
constexpr std::uint32_t kLogged = 3;

std::mutex g_mutex;
// (queue id, EVENT_TYPE_* or -1) -> count
std::map<std::pair<std::int32_t, std::int32_t>, std::uint32_t> g_counts;
std::atomic<std::uint32_t> g_total{0};

using ProcessEventFn = void(__fastcall *)(void *master, int edx, void *node);
dao::Detour<ProcessEventFn> g_processEvent;

void Count(const Node &node)
{
    std::int32_t eventType = -1;
    std::string dataType;
    if (node.data != 0)
    {
        dataType = dao::TypeName(reinterpret_cast<const void *>(node.data));
        if (node.id == kSignalEvent && dataType == "CScriptEvent")
        {
            const auto type = dao::TryRead<std::int32_t>(node.data + 0x20);
            eventType = type ? *type : -1;
        }
    }
    std::uint32_t seen = 0;
    {
        const std::lock_guard lock(g_mutex);
        seen = ++g_counts[{node.id, eventType}];
    }
    ++g_total;
    if (seen <= kLogged)
    {
        spdlog::info("event queue: id {} ({}), script event type {}, target {:#010x}, caller {:#010x}, due {}:{}",
                     node.id, dataType.empty() ? "no data" : dataType, eventType, node.target, node.caller, node.day,
                     node.time);
    }
}

// AIMaster::ProcessEvent(CAIEvent **): __thiscall, one argument.
void __fastcall ProcessEvent(void *master, int edx, void *nodeSlot)
{
    const auto slot = dao::TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(nodeSlot));
    if (slot && *slot != 0)
    {
        Node node{};
        if (dao::TryCopy(&node, *slot, sizeof node))
        {
            Count(node);
        }
    }
    g_processEvent.Original()(master, edx, nodeSlot);
}

// -- frames (dao/Frame.h) --------------------------------------------------
//
// dao::OnFrame's callback counts frames, paused and not, and once both are
// known says whether the frame's thread is the scripts' thread: whether a
// plugin can call script commands from its frame callback.
std::atomic<std::uint64_t> g_framesPaused{0};
std::atomic<std::uint64_t> g_framesRunning{0};
std::atomic<bool> g_threadsLogged{false};

void Frame()
{
    auto *app = dao::CClientExoApp::Get();
    if (app != nullptr && app->IsPaused())
    {
        ++g_framesPaused;
    }
    else
    {
        ++g_framesRunning;
    }
    const std::uint32_t script = dao::script::ScriptThreadId();
    if (script != 0 && !g_threadsLogged.exchange(true))
    {
        const std::uint32_t frame = dao::FrameThreadId();
        spdlog::info("frames: the update runs on thread {}, scripts on thread {}: {}", frame, script,
                     frame == script ? "the same, so a frame callback may call script commands"
                                     : "different, so a frame callback must not call script commands");
    }
}

} // namespace

void AttachEventWatch()
{
    dao::script::OnCommandsReady([] {}); // to learn the scripts' thread
    dao::OnFrame(&Frame);
    spdlog::info("frame watch: detoured the per-frame update at {:#010x}",
                 dao::Module::Preferred(dao::addr::kEclipseGame_Update));

    if (g_processEvent.Attach(dao::addr::kAIMaster_ProcessEvent, &ProcessEvent))
    {
        spdlog::info("event watch: detoured AIMaster::ProcessEvent at {:#010x}",
                     dao::Module::Preferred(dao::addr::kAIMaster_ProcessEvent));
    }
    else
    {
        spdlog::error("event watch: could not detour AIMaster::ProcessEvent at {:#010x}",
                      dao::Module::Preferred(dao::addr::kAIMaster_ProcessEvent));
    }
}

void ReportEventWatch()
{
    std::map<std::pair<std::int32_t, std::int32_t>, std::uint32_t> counts;
    {
        const std::lock_guard lock(g_mutex);
        counts = g_counts;
    }
    if (counts.empty())
    {
        spdlog::info("frames: {} running, {} paused", g_framesRunning.load(), g_framesPaused.load());
        return;
    }
    std::string line;
    for (const auto &[key, count] : counts)
    {
        line += key.second >= 0 ? fmt::format(" {}/{}:{}", key.first, key.second, count)
                                : fmt::format(" {}:{}", key.first, count);
    }
    spdlog::info("event queue: {} events, by id/script event type:{}", g_total.load(), line);
    spdlog::info("frames: {} running, {} paused", g_framesRunning.load(), g_framesPaused.load());
}

} // namespace probe
