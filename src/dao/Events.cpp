// Script events, seen as the engine delivers them.
//
// Events reach an object two ways. Most travel through the engine's event
// queue (dao/Addresses.h, "The engine's event queue"): SignalEvent and the
// engine queue a CScriptEvent, AIMaster::ProcessEvent hands it to the
// target's handler when it is due, and the handler runs the object's
// script for it (CCObject::RunEventScript, 0x00698960). Some never queue:
// engine code calls RunEventScript at once, 18 places in all (a creature's
// death, an order received, a selection, effects, the command queue). So
// OnScriptEvent detours RunEventScript, which sees both, and the queue
// too, for the events it hands an area or the module, which have handlers
// of their own. Read 2026-09-30, after a play test in which four deaths
// and most EVENT_TYPE_COMMAND_COMPLETEs never reached a ProcessEvent hook
// (the Eluvian server's report). Seen in play with the new hook, 2026-09-30
// (20:07 to 20:14, two fights): every death came, each 1.5 to 2.5 s after
// the creature was dead by IsDead, once its death had played out, the
// creator the killer for a death from a blow and null for one by an
// applied effect. Over a longer session the same day, 39 deaths gave 39
// events, a median 2.15 s after IsDead and at most 5.14 s (once 20 ms
// before a frame's IsDead poll). The moment of death itself is
// dao::creatures::OnDeath (DAOLib #255).
//
// EVENT_TYPE_DAMAGED is damage as it is taken: effect_damage_h.nss sends
// it (events_h.nss, SendEventOnDamaged) straight after it lowers a
// creature's health, the one place that does, once mana shields,
// resistances and the friendly-fire scaling have had their say. Float(0)
// is the damage, Int(0) its type (DAMAGE_TYPE_*, physical 1), Int(1) the
// ability, the creator the attacker. Only for a blow that does not kill:
// a fatal one sends the death instead. Read 2026-10-02 from the scripts;
// the event itself not yet seen in play.
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

struct Handled
{
    script::EventType type;
    std::function<void(ObjectId)> callback;
};
std::vector<Handled> g_handled;

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

// After the script: the event's type and target are taken first, as the engine frees or clears
// the event in the call (DAOLib #255's reading of the dispatch, 2026-10-24).
void TellHandled(ObjectId target, script::EventType type)
{
    std::vector<Handled> handled;
    {
        const std::lock_guard lock(g_mutex);
        handled = g_handled;
    }
    for (const auto &entry : handled)
    {
        if (entry.type == type)
        {
            detail::Guard("a handled script event callback", [&] { entry.callback(target); });
        }
    }
}

void __fastcall RunEventScript(CCObject *object, int edx, CScriptEvent **event)
{
    std::optional<script::EventType> type;
    ObjectId target = kInvalidObject;
    if (object != nullptr && event != nullptr && IsScriptEvent(reinterpret_cast<std::uintptr_t>(*event)))
    {
        target = object->Id();
        type = (*event)->Type();
        Tell(target, **event);
    }
    g_runEventScript.Original()(object, edx, event);
    if (type)
    {
        TellHandled(target, *type);
    }
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

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_runEventScript.Attach(addr::kCCObject_RunEventScript, &RunEventScript);
        g_processEvent.Attach(addr::kAIMaster_ProcessEvent, &ProcessEvent);
    });
}

void Add(Entry entry)
{
    {
        const std::lock_guard lock(g_mutex);
        g_callbacks.push_back(std::move(entry));
    }
    Attach();
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

void OnScriptEventHandled(script::EventType type, std::function<void(ObjectId target)> callback)
{
    {
        const std::lock_guard lock(g_mutex);
        g_handled.push_back({type, std::move(callback)});
    }
    Attach();
}

} // namespace dao::events
