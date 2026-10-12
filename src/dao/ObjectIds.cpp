// An object's id chosen by a plugin.
//
// Read 2026-10-12 (DAOLib #86; dao/Addresses.h, "How an object gets its
// id"). The game has the road already: an object asks for an id as it is
// constructed, -1 for a new one and its saved id for one from a save, and
// is given what it asks for if no object has it. The scripts' CreateObject
// constructs with -1 and takes no id, so CreateAs runs that very command
// and, in a detour of the array's AddObject, turns the first -1 asked for
// on this thread into the caller's id. That first one is the creature or
// placeable itself: it registers in CCObject's constructor, before its
// template is loaded and its items made, and nothing else is constructed
// by the command before it. The items after it ask for -1 and get it, so
// they keep ids of the game's own: a guest's and a host's copies of one
// creature need not make the same items (a script's random treasure), and
// an item under the host's next id would be in the way of the host's next
// creature.
//
// An id in use is the engine's to settle: it gives the pool's next and
// returns it, which is what CreateAs returns. No callback as an object is
// registered comes of the detour: the object is then a CCObject half
// constructed, of no kind yet.
//
// The counter is moved by the engine's own TakeId, which is how an object
// from a save moves it; it is read through the layout, there being no
// function that answers it.
#include "dao/ObjectIds.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/generated/ScriptApi.h"

#include <Windows.h>

#include <mutex>

namespace dao::objects
{

namespace
{

// The world database's first pool of ids, the one a new object draws on,
// and what is read of a pool.
constexpr std::uintptr_t kFirstPool = 0x30;
constexpr std::int32_t kFirstPoolIndex = 0;
struct Pool
{
    ObjectId least;
    ObjectId most;
    ObjectId last;
    std::uint8_t wrapped;
};

// The id CreateAs wants for the next object this thread constructs, and the
// thread: another thread's objects are not its.
ObjectId g_wanted = kInvalidObject;
DWORD g_wantedThread = 0;

using AddObjectFn = ObjectId(__fastcall *)(void *array, int edx, ObjectId asked, void *object);
Detour<AddObjectFn> g_addObject;
std::once_flag g_addObjectOnce;

ObjectId __fastcall AddObject(void *array, int edx, ObjectId asked, void *object)
{
    if (asked == kInvalidObject && g_wanted != kInvalidObject && GetCurrentThreadId() == g_wantedThread)
    {
        asked = g_wanted;
        g_wanted = kInvalidObject;
    }
    return g_addObject.Original()(array, edx, asked, object);
}

std::optional<Pool> FirstPool()
{
    const auto database = Global<std::uintptr_t>(addr::kWorldDatabase);
    const auto pool = database != 0 ? TryRead<std::uintptr_t>(database + kFirstPool) : std::nullopt;
    return pool && *pool != 0 ? TryRead<Pool>(*pool) : std::nullopt;
}

} // namespace

ObjectId CreateAs(ObjectId id, std::int32_t type, const Resource &tmpl, const CScriptLocation &location,
                  std::wstring_view overrideScript, bool spawnActive, bool noPermanentDeath)
{
    if (id < kLeastNewId || id > kMostNewId)
    {
        return kInvalidObject;
    }
    std::call_once(g_addObjectOnce, [] {
        if (!g_addObject.Attach(addr::kCGameObjectArray_AddObject, &AddObject))
        {
            spdlog::warn("objects: could not detour the object array; no object is made under a chosen id");
        }
    });
    if (g_addObject.Original() == nullptr)
    {
        return kInvalidObject;
    }
    g_wantedThread = GetCurrentThreadId();
    g_wanted = id;
    const ObjectId made =
        script::api::CreateObject(type, tmpl, location, overrideScript, spawnActive ? 1 : 0, noPermanentDeath ? 1 : 0);
    // Still wanted if nothing was constructed: no such template, a pooled
    // creature, an intercept that made nothing.
    g_wanted = kInvalidObject;
    return made;
}

std::optional<ObjectId> LastNewId()
{
    const auto pool = FirstPool();
    return pool ? std::optional<ObjectId>(pool->last) : std::nullopt;
}

bool NumberNewFrom(ObjectId first)
{
    const auto pool = FirstPool();
    if (!pool || pool->wrapped != 0 || first <= pool->last || first > kMostNewId)
    {
        return false;
    }
    if (first == pool->last + 1)
    {
        return true; // where the numbering stands
    }
    // WorldDatabase::TakeId: __thiscall (id, pool), `ret 8`. An id past the
    // pool's last given becomes the last given, and the next new object
    // gets one more.
    using TakeIdFn = bool(__fastcall *)(std::uintptr_t database, int edx, ObjectId id, std::int32_t pool);
    const auto takeId = reinterpret_cast<TakeIdFn>(Module::Address(addr::kWorldDatabase_TakeId));
    const auto database = Global<std::uintptr_t>(addr::kWorldDatabase);
    return takeId != nullptr && takeId(database, 0, first - 1, kFirstPoolIndex) && LastNewId() == first - 1;
}

} // namespace dao::objects
