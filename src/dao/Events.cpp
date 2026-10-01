#include "dao/Events.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
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

bool IsScriptEvent(std::uintptr_t data)
{
    return data != 0 &&
           TryRead<std::uint32_t>(data) == static_cast<std::uint32_t>(Module::Address(addr::kCScriptEvent_Vftable));
}

void Tell(ObjectId target, const CScriptEvent &event)
{
    std::vector<Entry> callbacks;
    {
        const std::lock_guard lock(g_mutex);
        callbacks = g_callbacks;
    }
    for (const auto &entry : callbacks)
    {
        if (!entry.type || *entry.type == event.Type())
        {
            detail::Guard("a script event callback", [&] { entry.callback(target, event); });
        }
    }
}

// An object's event script run: CCObject::RunEventScript, __thiscall
// (CScriptEvent **), `ret 4`. Every script event an object gets passes
// here, from the queue (through its handler, slot 50) or straight from
// engine code that does not queue (a creature's death, an order received,
// a selection, effects).
using RunEventScriptFn = void(__fastcall *)(CCObject *object, int edx, CScriptEvent **event);
Detour<RunEventScriptFn> g_runEventScript;

void __fastcall RunEventScript(CCObject *object, int edx, CScriptEvent **event)
{
    if (object != nullptr && event != nullptr && IsScriptEvent(reinterpret_cast<std::uintptr_t>(*event)))
    {
        Tell(object->Id(), **event);
    }
    g_runEventScript.Original()(object, edx, event);
}

// The queue, for the events it hands to an area or the module, which have
// handlers of their own (dao/Addresses.h); an object's are seen above.
using ProcessEventFn = void(__fastcall *)(void *master, int edx, void *nodeSlot);
Detour<ProcessEventFn> g_processEvent;

void __fastcall ProcessEvent(void *master, int edx, void *nodeSlot)
{
    const auto slot = TryRead<std::uint32_t>(reinterpret_cast<std::uintptr_t>(nodeSlot));
    Node node{};
    if (slot && *slot != 0 && TryCopy(&node, *slot, sizeof node) && node.id == kScriptEvent && IsScriptEvent(node.data))
    {
        CClientExoApp *app = CClientExoApp::Get();
        if (app != nullptr && app->GetObjectById(node.target) == nullptr)
        {
            Tell(node.target, *reinterpret_cast<const CScriptEvent *>(node.data));
        }
    }
    g_processEvent.Original()(master, edx, nodeSlot);
}

void Add(Entry entry)
{
    {
        const std::lock_guard lock(g_mutex);
        g_callbacks.push_back(std::move(entry));
    }
    static std::once_flag once;
    std::call_once(once, [] {
        g_runEventScript.Attach(addr::kCCObject_RunEventScript, &RunEventScript);
        g_processEvent.Attach(addr::kAIMaster_ProcessEvent, &ProcessEvent);
    });
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
