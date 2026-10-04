#include "dao/Timers.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/ScriptHooks.h"
#include "dao/String.h"
#include "dao/core/HeldEvents.h"
#include "dao/generated/ScriptApi.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dao::timers
{

namespace
{

// The queue's id for a script event, its data a CScriptEvent (dao/Events.cpp).
constexpr std::int32_t kScriptEvent = 10;
// The world timer at the application's +0xD4, the day's length in seconds
// at the timer's +0x50, and the event queue's list at its +0x64
// (dao/Addresses.h).
constexpr std::uintptr_t kAppWorldTimer = 0xD4;
constexpr std::uintptr_t kTimerDayLength = 0x50;
constexpr std::uintptr_t kQueueList = 0x64;
// A CScriptEvent's EVENT_TYPE_* (dao/Structures.h, CScriptEvent::Type).
constexpr std::uintptr_t kScriptEventType = 0x20;
// As many as are delivered in one of the queue's updates: an event re-armed
// at a tiny delay and a long advance would otherwise take the frame.
constexpr int kMostPerUpdate = 2000;
// As many nodes as are read from the queue's list before it is taken to be
// broken.
constexpr int kMostNodes = 100000;

// A CAIEvent (dao/Events.cpp).
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

struct Held
{
    std::uint32_t caller = 0;
    std::uint32_t target = 0;
    std::int32_t id = 0;
    std::uint32_t data = 0;
};

std::mutex g_lock;
bool g_held = false;
// The same, read without the lock by the hooks every event passes, which
// have nothing to do while nothing is held.
std::atomic<bool> g_holding{false};
core::HeldByTarget<Held> g_events;
// Events in the queue when the hold began, by their node, each with when
// it comes due on its target's clock; read on the queue's next update
// after it.
std::unordered_map<std::uint32_t, double> g_before;
bool g_readBefore = false;
// What was let go, to queue again on the queue's next update.
std::vector<core::HeldByTarget<Held>::Left> g_released;

// Which events are held (dao/core/HeldEvents.h): a damage over time's tick
// and an area's pulse, and none by delay, unless a plugin says otherwise;
// the module's game mode change left out, which the scripts end a fight
// with (combat_h, player_core: 1 to 3 s after the last enemy goes), held
// on a fight's clock it would wait for rounds no longer played.
std::mutex g_kindsLock;
core::HoldRule g_rule{
    {static_cast<std::int32_t>(script::EventType::DotTick), static_cast<std::int32_t>(script::EventType::AoeHeartbeat)},
    {static_cast<std::int32_t>(script::EventType::DelayedGmChange)},
    0};

// A script event's EVENT_TYPE_*, or nothing for any other event.
std::optional<std::int32_t> ScriptEventType(std::int32_t id, std::uint32_t data)
{
    if (id != kScriptEvent || data == 0 ||
        TryRead<std::uint32_t>(data) != static_cast<std::uint32_t>(Module::Address(addr::kCScriptEvent_Vftable)))
    {
        return std::nullopt;
    }
    return TryRead<std::int32_t>(data + kScriptEventType);
}

// Whether a script event sent with this delay is held, by its kind and its
// delay.
bool IsHeld(std::int32_t id, std::uint32_t data, double delay)
{
    const auto type = ScriptEventType(id, data);
    const std::lock_guard lock(g_kindsLock);
    return type && g_rule.Holds(*type, delay);
}

std::uintptr_t WorldTimer()
{
    CClientExoApp *app = CClientExoApp::Get();
    const auto timer =
        app != nullptr ? TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(app) + kAppWorldTimer) : std::nullopt;
    return timer ? *timer : 0;
}

std::uint32_t DayLength(std::uintptr_t timer)
{
    const auto length = timer != 0 ? TryRead<std::int32_t>(timer + kTimerDayLength) : std::nullopt;
    return length && *length > 0 ? static_cast<std::uint32_t>(*length) : 0;
}

// --- The trace -------------------------------------------------------------

std::atomic<bool> g_tracing{false};

// A traced event sent and not yet delivered, by its data: its type and
// target, and the world clock it was sent at, in seconds.
struct Sent
{
    std::int32_t type = 0;
    std::uint32_t target = 0;
    double at = 0;
};
std::mutex g_sentLock;
std::unordered_map<std::uint32_t, Sent> g_sent;
// As many as are remembered: an event dropped without being delivered (an
// area's going) is forgotten past this.
constexpr std::size_t kMostSent = 10000;

// The world clock now, as its day and milliseconds and in seconds.
struct WorldNow
{
    std::uint32_t day = 0;
    std::uint32_t ms = 0;
    double seconds = 0;
};

WorldNow ReadWorldNow()
{
    WorldNow now;
    const std::uintptr_t timer = WorldTimer();
    if (timer == 0)
    {
        return now;
    }
    // EC::WorldTimer::WorldTime: __thiscall (day *, ms *), `ret 8`.
    using WorldTimeFn = void(__fastcall *)(std::uintptr_t timer, int edx, std::uint32_t *day, std::uint32_t *ms);
    reinterpret_cast<WorldTimeFn>(Module::Address(addr::kWorldTimer_WorldTime))(timer, 0, &now.day, &now.ms);
    now.seconds = static_cast<double>(now.day) * DayLength(timer) + now.ms / 1000.0;
    return now;
}

// An object as a trace names it: its id, and its tag where a script command
// may be called (on the scripts' thread, dao/ScriptHooks.h).
std::string Named(std::uint32_t object)
{
    const std::uint32_t scripts = script::ScriptThreadId();
    if (scripts == 0 || scripts != GetCurrentThreadId())
    {
        return std::format("{:#010x}", object);
    }
    return std::format("{:#010x} \"{}\"", object, Utf8(script::api::GetTag(object)));
}

std::string TypeName(std::int32_t type)
{
    const std::string_view name = script::Name(static_cast<script::EventType>(type));
    return name.empty() ? std::to_string(type) : std::format("{} {}", type, name);
}

void TraceSent(std::uint32_t days, std::uint32_t ms, std::uint32_t caller, std::uint32_t target, std::int32_t id,
               std::uint32_t data, bool heldHere)
{
    const auto type = ScriptEventType(id, data);
    if (!type)
    {
        return;
    }
    const WorldNow now = ReadWorldNow();
    const double delay = static_cast<double>(days) * DayLength(WorldTimer()) + ms / 1000.0;
    spdlog::info("timers: event {} sent to {} by {}, in {:.3f} s, world {}:{:08}{}", TypeName(*type), Named(target),
                 Named(caller), delay, now.day, now.ms, heldHere ? ", held on the plugin's clock" : "");
    const std::lock_guard lock(g_sentLock);
    if (g_sent.size() >= kMostSent)
    {
        g_sent.clear();
    }
    g_sent[data] = Sent{*type, target, now.seconds};
}

void TraceDelivered(std::uint32_t data, const char *how)
{
    Sent sent;
    {
        const std::lock_guard lock(g_sentLock);
        const auto found = g_sent.find(data);
        if (found == g_sent.end())
        {
            return;
        }
        sent = found->second;
        g_sent.erase(found);
    }
    const WorldNow now = ReadWorldNow();
    spdlog::info("timers: event {} delivered to {}{}, {:.3f} s of the world's clock after it was sent, world {}:{:08}",
                 TypeName(sent.type), Named(sent.target), how, now.seconds - sent.at, now.day, now.ms);
}

// AIMaster::AddEventDeltaTime: __thiscall (days, ms, caller, target, id,
// data), `ret 0x18`, 1 when queued.
using AddEventDeltaTimeFn = std::int32_t(__fastcall *)(void *queue, int edx, std::uint32_t days, std::uint32_t ms,
                                                       std::uint32_t caller, std::uint32_t target, std::int32_t id,
                                                       std::uint32_t data);
Detour<AddEventDeltaTimeFn> g_addEventDeltaTime;

std::int32_t __fastcall AddEventDeltaTime(void *queue, int edx, std::uint32_t days, std::uint32_t ms,
                                          std::uint32_t caller, std::uint32_t target, std::int32_t id,
                                          std::uint32_t data)
{
    const bool traced = g_tracing.load(std::memory_order_relaxed) && (days != 0 || ms != 0);
    const double delay = days != 0 || ms != 0 ? static_cast<double>(days) * DayLength(WorldTimer()) + ms / 1000.0 : 0;
    if (g_holding.load(std::memory_order_relaxed) && (days != 0 || ms != 0) && IsHeld(id, data, delay))
    {
        bool heldHere = false;
        {
            const std::lock_guard lock(g_lock);
            if (g_held)
            {
                g_events.Add(target, delay, Held{caller, target, id, data});
                heldHere = true;
            }
        }
        if (heldHere)
        {
            if (traced)
            {
                TraceSent(days, ms, caller, target, id, data, true);
            }
            return 1;
        }
    }
    if (traced)
    {
        TraceSent(days, ms, caller, target, id, data, false);
    }
    return g_addEventDeltaTime.Original()(queue, edx, days, ms, caller, target, id, data);
}

// AIMaster::ProcessEvent: __thiscall (CAIEvent **), `ret 4`; it frees the
// node through its vtable's first slot (a deleting destructor, 1) and
// clears the slot before it delivers.
using ProcessEventFn = void(__fastcall *)(void *queue, int edx, std::uint32_t *slot);
Detour<ProcessEventFn> g_processEvent;

// An event queued before the hold, of a held kind, as it comes due: taken
// out to wait here, its node freed as ProcessEvent would free it. True if
// it was taken.
bool TakeQueued(std::uint32_t *slot)
{
    if (slot == nullptr || !g_holding.load(std::memory_order_relaxed))
    {
        return false;
    }
    const auto at = TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(slot));
    if (!at || *at == 0)
    {
        return false;
    }
    std::optional<double> due;
    {
        const std::lock_guard lock(g_lock);
        if (const auto found = g_before.find(*at); found != g_before.end())
        {
            if (g_held)
            {
                due = found->second;
            }
            g_before.erase(found);
        }
    }
    Node node{};
    if (!due || !TryCopy(&node, *at, sizeof node) || !ScriptEventType(node.id, node.data))
    {
        return false;
    }
    using DeleteFn = void *(__fastcall *)(std::uint32_t node, int edx, std::uint32_t flags);
    const auto destroy = TryRead<std::uint32_t>(node.vtable);
    if (!destroy || *destroy == 0)
    {
        return false;
    }
    reinterpret_cast<DeleteFn>(*destroy)(*at, 0, 1);
    *slot = 0;
    const std::lock_guard lock(g_lock);
    g_events.AddAt(node.target, *due, Held{node.caller, node.target, node.id, node.data});
    return true;
}

void __fastcall ProcessEvent(void *queue, int edx, std::uint32_t *slot)
{
    if (TakeQueued(slot))
    {
        return;
    }
    if (g_tracing.load(std::memory_order_relaxed) && slot != nullptr)
    {
        // Read before the original, which frees the node.
        const auto at = TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(slot));
        Node node{};
        if (at && *at != 0 && TryCopy(&node, *at, sizeof node))
        {
            TraceDelivered(node.data, "");
        }
    }
    g_processEvent.Original()(queue, edx, slot);
}

// An event in the queue that a hold keeps: its node, its target, and how
// long after now it comes due.
struct Queued
{
    std::uint32_t node = 0;
    std::uint32_t target = 0;
    double inSeconds = 0;
};

// The events in the queue now that a hold keeps, by their kind and the
// time they have left (dao/Addresses.h: the list, walked from its tail as
// AddEventLater does).
std::vector<Queued> ReadQueue(std::uintptr_t queue)
{
    std::vector<Queued> found;
    const std::uintptr_t timer = WorldTimer();
    const auto list = TryRead<std::uint32_t>(queue + kQueueList);
    if (timer == 0 || !list || *list == 0)
    {
        return found;
    }
    // EC::WorldTimer::WorldTime: __thiscall (day *, ms *), `ret 8`.
    using WorldTimeFn = void(__fastcall *)(std::uintptr_t timer, int edx, std::uint32_t *day, std::uint32_t *ms);
    std::uint32_t day = 0;
    std::uint32_t ms = 0;
    reinterpret_cast<WorldTimeFn>(Module::Address(addr::kWorldTimer_WorldTime))(timer, 0, &day, &ms);
    const std::uint32_t dayLength = DayLength(timer);
    auto position = TryRead<std::uint32_t>(*list + 4);
    for (int i = 0; i < kMostNodes && position && *position != 0; ++i)
    {
        const auto event = TryRead<std::uint32_t>(*position + 8);
        Node node{};
        if (event && *event != 0 && TryCopy(&node, *event, sizeof node))
        {
            const double left = core::WorldSeconds(day, ms, node.day, node.time, dayLength);
            if (IsHeld(node.id, node.data, left))
            {
                found.push_back(Queued{*event, node.target, left});
            }
        }
        position = TryRead<std::uint32_t>(*position);
    }
    return found;
}

// AIMaster::DeliverNow: __thiscall (caller, target, id, data), `ret 0x10`.
using DeliverNowFn = std::int32_t(__fastcall *)(void *queue, int edx, std::uint32_t caller, std::uint32_t target,
                                                std::int32_t id, std::uint32_t data);

// Before each of the queue's updates, on the game's thread: read what was
// queued before a hold, give back what a release let go, and deliver what
// has come due.
void BeforeUpdate(void *queue)
{
    bool read = false;
    std::vector<core::HeldByTarget<Held>::Left> released;
    {
        const std::lock_guard lock(g_lock);
        read = std::exchange(g_readBefore, false) && g_held;
        released = std::exchange(g_released, {});
        g_events.Prune();
    }
    if (read)
    {
        const auto before = ReadQueue(reinterpret_cast<std::uintptr_t>(queue));
        const std::lock_guard lock(g_lock);
        for (const auto &queued : before)
        {
            g_before[queued.node] = g_events.Now(queued.target) + queued.inSeconds;
        }
    }
    if (!released.empty())
    {
        const std::uint32_t dayLength = DayLength(WorldTimer());
        for (const auto &[target, left, event] : released)
        {
            const core::WorldDelay delay = core::ToWorldDelay(left, dayLength);
            // A delay of none would be delivered now, where a held event
            // comes due at least a millisecond on.
            const std::uint32_t ms = delay.days == 0 && delay.ms == 0 ? 1 : delay.ms;
            g_addEventDeltaTime.Original()(queue, 0, delay.days, ms, event.caller, event.target, event.id, event.data);
        }
    }
    const auto deliverNow = reinterpret_cast<DeliverNowFn>(Module::Address(addr::kAIMaster_DeliverNow));
    for (int i = 0; i < kMostPerUpdate; ++i)
    {
        std::optional<std::pair<std::uint32_t, Held>> next;
        {
            const std::lock_guard lock(g_lock);
            next = g_events.Next();
        }
        if (!next)
        {
            break;
        }
        const Held &event = next->second;
        if (g_tracing.load(std::memory_order_relaxed))
        {
            TraceDelivered(event.data, " from the plugin's clock");
        }
        deliverNow(queue, 0, event.caller, event.target, event.id, event.data);
    }
}

// AIMaster::Update: __thiscall (bool), `ret 4`.
using UpdateFn = void(__fastcall *)(void *queue, int edx, std::int32_t flag);
Detour<UpdateFn> g_update;

void __fastcall Update(void *queue, int edx, std::int32_t flag)
{
    if (queue != nullptr)
    {
        BeforeUpdate(queue);
    }
    g_update.Original()(queue, edx, flag);
}

// AIMaster::ClearEvents: __thiscall, no arguments; it frees each event's
// data through DeleteEventData, __thiscall (id, data), `ret 8`.
using ClearEventsFn = void(__fastcall *)(void *queue, int edx);
Detour<ClearEventsFn> g_clearEvents;

void __fastcall ClearEvents(void *queue, int edx)
{
    std::vector<core::HeldByTarget<Held>::Left> dropped;
    {
        const std::lock_guard lock(g_lock);
        dropped = g_events.Release();
        for (const auto &left : std::exchange(g_released, {}))
        {
            dropped.push_back(left);
        }
        g_before.clear();
    }
    {
        const std::lock_guard lock(g_sentLock);
        g_sent.clear();
    }
    using DeleteEventDataFn = void(__fastcall *)(void *queue, int edx, std::int32_t id, std::uint32_t data);
    const auto deleteData = reinterpret_cast<DeleteEventDataFn>(Module::Address(addr::kAIMaster_DeleteEventData));
    for (const auto &left : dropped)
    {
        deleteData(queue, 0, left.event.id, left.event.data);
    }
    g_clearEvents.Original()(queue, edx);
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_addEventDeltaTime.Attach(addr::kAIMaster_AddEventDeltaTime, &AddEventDeltaTime);
        g_processEvent.Attach(addr::kAIMaster_ProcessEvent, &ProcessEvent);
        g_update.Attach(addr::kAIMaster_Update, &Update);
        g_clearEvents.Attach(addr::kAIMaster_ClearEvents, &ClearEvents);
    });
}

} // namespace

void Hold(bool held)
{
    Attach();
    const std::lock_guard lock(g_lock);
    if (held == g_held)
    {
        return;
    }
    g_held = held;
    g_holding = held;
    if (held)
    {
        // Held again before the queue's update gave back what the release
        // let go: it waits here as it was.
        for (const auto &[target, left, event] : std::exchange(g_released, {}))
        {
            g_events.Add(target, left, event);
        }
        g_readBefore = true;
        return;
    }
    g_readBefore = false;
    g_before.clear();
    for (const auto &left : g_events.Release())
    {
        g_released.push_back(left);
    }
}

bool Held()
{
    const std::lock_guard lock(g_lock);
    return g_held;
}

void Advance(float seconds)
{
    const std::lock_guard lock(g_lock);
    if (g_held)
    {
        g_events.Advance(seconds);
    }
}

void OwnClock(ObjectId object, bool own)
{
    const std::lock_guard lock(g_lock);
    if (g_held)
    {
        g_events.SetOwn(object, own);
    }
}

void Advance(ObjectId object, float seconds)
{
    const std::lock_guard lock(g_lock);
    if (g_held)
    {
        g_events.Advance(object, seconds);
    }
}

void HoldKind(script::EventType type, bool held)
{
    const std::lock_guard lock(g_kindsLock);
    g_rule.Hold(static_cast<std::int32_t>(type), held);
}

void HoldFrom(float seconds)
{
    const std::lock_guard lock(g_kindsLock);
    g_rule.from = seconds;
}

void Trace(bool on)
{
    Attach();
    g_tracing = on;
    spdlog::info("timers: the trace of delayed script events {}", on ? "begins" : "ends");
}

std::size_t Waiting()
{
    const std::lock_guard lock(g_lock);
    return g_events.Size() + g_released.size();
}

std::size_t Waiting(ObjectId object)
{
    const std::lock_guard lock(g_lock);
    return g_events.Size(object);
}

} // namespace dao::timers
