#include "dao/Events.h"

#include "dao/Addresses.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace dao::events
{

namespace
{

// A CAIEvent: vtable, due day and time, caller, target, the queue's id for
// the kind of event, and its data (dao/Addresses.h; probe/Events.cpp reads
// the same). Id 10 is a script event, its data a CScriptEvent.
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

constexpr std::int32_t kScriptEvent = 10;

struct Entry
{
    std::optional<script::EventType> type;
    ScriptEventCallback callback;
};

std::mutex g_mutex;
std::vector<Entry> g_callbacks;

using ProcessEventFn = void(__fastcall *)(void *master, int edx, void *nodeSlot);
Detour<ProcessEventFn> g_processEvent;

void __fastcall ProcessEvent(void *master, int edx, void *nodeSlot)
{
    const auto slot = TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(nodeSlot));
    Node node{};
    if (slot && *slot != 0 && TryCopy(&node, *slot, sizeof node) && node.id == kScriptEvent && node.data != 0 &&
        TryRead<std::uint32_t>(node.data) == static_cast<std::uint32_t>(Module::Address(addr::kCScriptEvent_Vftable)))
    {
        const auto &event = *reinterpret_cast<const CScriptEvent *>(node.data);
        std::vector<Entry> callbacks;
        {
            const std::lock_guard lock(g_mutex);
            callbacks = g_callbacks;
        }
        for (const auto &entry : callbacks)
        {
            if (!entry.type || *entry.type == event.Type())
            {
                detail::Guard("a script event callback", [&] { entry.callback(node.target, event); });
            }
        }
    }
    g_processEvent.Original()(master, edx, nodeSlot);
}

void Add(Entry entry)
{
    const std::lock_guard lock(g_mutex);
    g_callbacks.push_back(std::move(entry));
    if (g_processEvent.Original() == nullptr)
    {
        g_processEvent.Attach(addr::kAIMaster_ProcessEvent, &ProcessEvent);
    }
}

} // namespace

void OnScriptEvent(ScriptEventCallback callback)
{
    Add({std::nullopt, std::move(callback)});
}

void OnScriptEvent(script::EventType type, ScriptEventCallback callback)
{
    Add({type, std::move(callback)});
}

} // namespace dao::events
