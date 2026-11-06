// A creature's walk read live (DAOLib #325, for Eluvian #194 and DAOParagonEngine #429).
//
// The reading is dev/PATHFINDING.md's "A creature in the walker's way" and TURN_BASED.md's
// standing counter. The movement controller is embedded at creature +0xA30, its state at its +0xF0
// (creature +0xB20). The movement record is [creature+0xA40] (CCCreature::Movement), its state at
// +0 and its timer at +4. The move update is handed the sub-action's parameters, the CDataArrays
// at the sub-action's +0x1C (ApproachMelee loads them at 0x007AA59C), and keeps the standing time
// in their int 1 (0x007A97FD: GetInteger with ecx the parameters). Seen in play 2026-11-06: the
// argument's first word is CDataArrays' vtable, 0x00B5951C. The sub-action's type is the running
// sub-action's (dao/SubActions.cpp's list), when its +0x1C is those parameters.
//
// One update is followed by a hook on the update and one on each part of its in-the-way pass. A
// thread's update open at the time is the one a part belongs to. Parts run outside an update (the
// direct-control steering 0x007A8C20 calls HandleCreaturesInTheWay) are not recorded.
#include "dao/Movement.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Guard.h"
#include "dao/Hook.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/Structures.h"
#include "dao/core/ObjectType.h"

#include <atomic>
#include <map>
#include <mutex>
#include <utility>

namespace dao::movement
{

namespace
{

constexpr std::uintptr_t kMotion = 0xB20; // in a creature: its movement controller's state
constexpr std::uintptr_t kRecord = 0xA40; // in a creature: its movement record
constexpr std::uintptr_t kTimer = 4;      // in the record: the timer in ms
constexpr std::uintptr_t kListBegin = 4;  // in the list, a std::vector of creature pointers
constexpr std::uintptr_t kListEnd = 8;
constexpr std::int32_t kStandingIndex = 1; // the sub-action's int that counts the standing time
constexpr std::size_t kMostListed = 64;    // as many entries as are read before the list is taken to be broken
// The running sub-action, as dao/SubActions.cpp reads the list: the creature's command record, its
// sub-actions' std::list at +0xC with its head node at +4, a node's value at +8; and the
// sub-action's parameters at its +0x1C.
constexpr std::uintptr_t kCommandRecord = 0x2DC;
constexpr std::uintptr_t kSubActionList = 0xC;
constexpr std::uintptr_t kListHead = 4;
constexpr std::uintptr_t kNodeValue = 8;
constexpr std::uintptr_t kParameters = 0x1C;

std::uint32_t RunningType(std::uintptr_t creature, std::uintptr_t parameters)
{
    const auto record = TryRead<std::uintptr_t>(creature + kCommandRecord).value_or(0);
    const auto head = record != 0 ? TryRead<std::uintptr_t>(record + kSubActionList + kListHead).value_or(0) : 0;
    const auto first = head != 0 ? TryRead<std::uintptr_t>(head).value_or(0) : 0;
    const auto sub = first != 0 && first != head ? TryRead<std::uintptr_t>(first + kNodeValue).value_or(0) : 0;
    if (sub == 0 || TryRead<std::uintptr_t>(sub + kParameters).value_or(0) != parameters)
    {
        return 0;
    }
    return TryRead<std::uint32_t>(sub).value_or(0);
}

std::optional<Record> RecordAt(std::uintptr_t creature)
{
    const auto at = TryRead<std::uintptr_t>(creature + kRecord).value_or(0);
    if (at == 0)
    {
        return std::nullopt;
    }
    return Record{TryRead<std::int32_t>(at).value_or(0), TryRead<std::int32_t>(at + kTimer).value_or(0)};
}

Walker WalkerAt(std::uintptr_t creature)
{
    return Walker{TryRead<std::int32_t>(creature + kMotion).value_or(motion::kIdle), RecordAt(creature)};
}

std::vector<ObjectId> Listed(std::uintptr_t list)
{
    std::vector<ObjectId> ids;
    const auto begin = TryRead<std::uintptr_t>(list + kListBegin).value_or(0);
    const auto end = TryRead<std::uintptr_t>(list + kListEnd).value_or(0);
    if (begin == 0 || end < begin)
    {
        return ids;
    }
    for (std::uintptr_t at = begin; at < end && ids.size() < kMostListed; at += sizeof(std::uintptr_t))
    {
        if (const auto creature = TryRead<std::uintptr_t>(at).value_or(0); creature != 0)
        {
            ids.push_back(reinterpret_cast<const CCObject *>(creature)->Id());
        }
    }
    return ids;
}

// The update a thread is in, and the creature it moves.
struct Open
{
    std::uintptr_t creature = 0;
    MoveUpdate update;
};
thread_local Open *t_open = nullptr;

InTheWay *PassOf(std::uintptr_t creature, bool combat)
{
    if (t_open == nullptr || t_open->creature != creature)
    {
        return nullptr;
    }
    if (!t_open->update.inTheWay)
    {
        t_open->update.inTheWay.emplace();
        t_open->update.inTheWay->combat = combat;
    }
    return &*t_open->update.inTheWay;
}

// The watches. Never destroyed: a plugin's watch held in a global ends at exit, after this file's
// globals may be gone.
struct Watches
{
    std::mutex mutex;
    std::map<std::uint64_t, std::function<void(const MoveUpdate &)>> observers;
    std::uint64_t next = 1;
};
Watches &TheWatches()
{
    static auto *const watches = new Watches; // NOLINT(cppcoreguidelines-owning-memory): kept to the end
    return *watches;
}
// The watches once the first is made: a live watch reaches them through this, which makes nothing.
std::atomic<Watches *> g_watches{nullptr};
std::atomic<bool> g_any{false}; // whether any watch is alive: with none, an update is passed straight on

using MoveUpdateFn = std::int32_t(__fastcall *)(void *handler, int edx, CCObject *creature, std::uintptr_t parameters,
                                                float stop, std::int32_t flags);
using HandleFn = std::int32_t(__cdecl *)(std::uintptr_t parameters, CCObject *creature, const void *delta,
                                         std::uintptr_t list);
using InCombatFn = std::int32_t(__cdecl *)(CCObject *creature, const void *delta);
using ReactFn = std::int32_t(__cdecl *)(CCObject *creature, std::uintptr_t list);
using ListFn = std::int32_t(__fastcall *)(void *area, int edx, CCObject *mover, const void *probe, std::int32_t zero,
                                          std::uintptr_t list);
Detour<MoveUpdateFn> g_moveUpdate;
Detour<HandleFn> g_handle;
Detour<InCombatFn> g_inCombat;
Detour<ReactFn> g_react;
Detour<ListFn> g_list;

void Fire(const MoveUpdate &update)
{
    std::vector<std::function<void(const MoveUpdate &)>> observers;
    {
        Watches &w = TheWatches();
        const std::lock_guard lock(w.mutex);
        for (const auto &[id, observer] : w.observers)
        {
            observers.push_back(observer);
        }
    }
    for (const auto &observer : observers)
    {
        detail::Guard("a move update callback", [&] { observer(update); });
    }
}

std::int32_t __fastcall OnUpdate(void *handler, int edx, CCObject *creature, std::uintptr_t parameters, float stop,
                                 std::int32_t flags)
{
    if (!g_any.load(std::memory_order_relaxed) || creature == nullptr || parameters == 0)
    {
        return g_moveUpdate.Original()(handler, edx, creature, parameters, stop, flags);
    }
    Open open;
    open.creature = reinterpret_cast<std::uintptr_t>(creature);
    Open *const outer = std::exchange(t_open, &open);
    const std::int32_t result = g_moveUpdate.Original()(handler, edx, creature, parameters, stop, flags);
    t_open = outer;

    MoveUpdate &update = open.update;
    update.creature = creature->Id();
    update.subAction = RunningType(open.creature, parameters);
    update.result = result;
    update.standingMs = reinterpret_cast<const CDataArrays *>(parameters)->Integer(kStandingIndex);
    update.walker = WalkerAt(open.creature);
    Fire(update);
    return result;
}

std::int32_t __cdecl OnHandle(std::uintptr_t parameters, CCObject *creature, const void *delta, std::uintptr_t list)
{
    const std::int32_t answer = g_handle.Original()(parameters, creature, delta, list);
    if (InTheWay *pass = PassOf(reinterpret_cast<std::uintptr_t>(creature), false))
    {
        pass->answer = answer;
    }
    return answer;
}

std::int32_t __cdecl OnInCombat(CCObject *creature, const void *delta)
{
    if (InTheWay *pass = PassOf(reinterpret_cast<std::uintptr_t>(creature), true))
    {
        pass->answer = 1;
    }
    return g_inCombat.Original()(creature, delta);
}

std::int32_t __cdecl OnReact(CCObject *creature, std::uintptr_t list)
{
    InTheWay *pass = PassOf(reinterpret_cast<std::uintptr_t>(creature), false);
    if (pass == nullptr)
    {
        return g_react.Original()(creature, list);
    }
    const auto at = reinterpret_cast<std::uintptr_t>(creature);
    pass->before = RecordAt(at).value_or(Record{});
    const std::int32_t answer = g_react.Original()(creature, list);
    pass->after = RecordAt(at).value_or(Record{});
    pass->reacted = true;
    return answer;
}

std::int32_t __fastcall OnList(void *area, int edx, CCObject *mover, const void *probe, std::int32_t zero,
                               std::uintptr_t list)
{
    const std::int32_t answer = g_list.Original()(area, edx, mover, probe, zero, list);
    if (InTheWay *pass = PassOf(reinterpret_cast<std::uintptr_t>(mover), false); pass != nullptr && list != 0)
    {
        pass->listed = Listed(list);
    }
    return answer;
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] {
        g_list.Attach(addr::kCCArea_CreaturesInTheWay, &OnList);
        g_handle.Attach(addr::kCSSubActionHandler_HandleCreaturesInTheWay, &OnHandle);
        g_inCombat.Attach(addr::kCSSubActionHandler_CreaturesInTheWayInCombat, &OnInCombat);
        g_react.Attach(addr::kCSSubActionHandler_ReactToCreatureInTheWay, &OnReact);
        g_moveUpdate.Attach(addr::kCSSubActionHandler_MoveUpdate, &OnUpdate);
    });
}

} // namespace

std::optional<Walker> WalkerOf(ObjectId creature)
{
    CClientExoApp *app = CClientExoApp::Get();
    const CCObject *object = app != nullptr ? app->GetObjectById(creature) : nullptr;
    if (object == nullptr || object->Type() != core::ObjectType::Creature)
    {
        return std::nullopt;
    }
    return WalkerAt(reinterpret_cast<std::uintptr_t>(object));
}

Watch OnMoveUpdate(std::function<void(const MoveUpdate &update)> observer)
{
    Attach();
    Watches &w = TheWatches();
    g_watches.store(&w);
    const std::lock_guard lock(w.mutex);
    const std::uint64_t id = w.next++;
    w.observers.emplace(id, std::move(observer));
    g_any.store(true, std::memory_order_relaxed);
    return Watch(id);
}

Watch::Watch(Watch &&other) noexcept : id_(std::exchange(other.id_, 0))
{
}

Watch &Watch::operator=(Watch &&other) noexcept
{
    if (this != &other)
    {
        Stop();
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}

Watch::~Watch()
{
    Stop();
}

void Watch::Stop() noexcept
{
    Watches *const w = g_watches.load();
    if (id_ == 0 || w == nullptr)
    {
        return;
    }
    const std::lock_guard lock(w->mutex);
    w->observers.erase(id_);
    g_any.store(!w->observers.empty(), std::memory_order_relaxed);
    id_ = 0;
}

} // namespace dao::movement
