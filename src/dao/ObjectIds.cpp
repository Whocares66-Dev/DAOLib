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
//
// A build of areas (DAOLib #87). The multiplayer mod saw in two games,
// 2026-10-12, that an area the save has never visited numbers all its
// creatures from the counter as it loads: 164 of 164 the same in both with
// the counters equal, most of them nine apart with one counter nine ahead.
// So a game that is to agree with another cannot just number its own
// objects elsewhere; it must number a build as the other did. A list's
// areas are built by CSAreaList::LoadAreas, one a call (dao/Addresses.h,
// "The loading mode's states"), and while a call runs every new object of
// its thread is the build's. Followed, each asks for the next id of the
// numbering given rather than -1, by the same road as CreateAs: the counter
// is never set back, which before it wraps would hand out an id in use
// without a look.
//
// Seen in play 2026-10-12 (examples/objectids, GOG's game, Lyna's manual
// save 342, twice): the save's load is a build, of 110 new objects from
// 0x314EA, the counter at 0x31558 after, 0x314EA + 110; DoAreaTransition
// to cli400ar_city_gates, never visited, built 873 from 0x131567 and left
// the counter at 0x1318D0, that id plus 873, so every new object of the
// build is made inside the calls. The second run moved its own numbering
// 0x1000000 ahead and numbered the build on from 0x131567: 873 objects,
// none refused, its own counter where it was, and of the area's 46
// creatures all 35 the build made had the first run's ids. The two that
// differed were pool creatures (pool_cli400cr_city_gate_defender0 and 1)
// the area's script made after the build, numbered from each run's counter.
#include "dao/ObjectIds.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/generated/ScriptApi.h"

#include <Windows.h>

#include <functional>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

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

// A build of a list's areas, from the first LoadAreas call that finds none
// going to the first that answers other than 0, "more to come".
struct Build
{
    bool going = false;
    DWORD thread = 0;       // inside a LoadAreas call, the thread it is on; else 0
    bool following = false; // numbered on from an id a callback gave
    ObjectId next = 0;      // following: the id the next new object asks for
    AreasBuilt numbered;
};
Build g_build;
std::mutex g_callbacksMutex;
std::vector<std::function<std::optional<ObjectId>(ObjectId)>> g_starting;
std::vector<std::function<void(const AreasBuilt &)>> g_built;

using AddObjectFn = ObjectId(__fastcall *)(void *array, int edx, ObjectId asked, void *object);
Detour<AddObjectFn> g_addObject;
std::once_flag g_addObjectOnce;

ObjectId __fastcall AddObject(void *array, int edx, ObjectId asked, void *object)
{
    if (asked != kInvalidObject || object == nullptr)
    {
        return g_addObject.Original()(array, edx, asked, object);
    }
    const DWORD thread = GetCurrentThreadId();
    if (g_wanted != kInvalidObject && thread == g_wantedThread)
    {
        asked = g_wanted;
        g_wanted = kInvalidObject;
    }
    else if (g_build.thread == thread)
    {
        ++g_build.numbered.objects;
        if (g_build.following)
        {
            // The next id of the numbering followed, asked for as a save's
            // object asks for its own: an id an object has is not given
            // again, and that object gets one of the game's own.
            const ObjectId wanted = g_build.next++;
            const ObjectId got = g_addObject.Original()(array, edx, wanted, object);
            g_build.numbered.refused += got != wanted ? 1 : 0;
            return got;
        }
    }
    return g_addObject.Original()(array, edx, asked, object);
}

bool AttachAddObject()
{
    std::call_once(g_addObjectOnce, [] {
        if (!g_addObject.Attach(addr::kCGameObjectArray_AddObject, &AddObject))
        {
            spdlog::warn("objects: could not detour the object array; no object is given a chosen id");
        }
    });
    return g_addObject.Original() != nullptr;
}

std::optional<Pool> FirstPool()
{
    const auto database = Global<std::uintptr_t>(addr::kWorldDatabase);
    const auto pool = database != 0 ? TryRead<std::uintptr_t>(database + kFirstPool) : std::nullopt;
    return pool && *pool != 0 ? TryRead<Pool>(*pool) : std::nullopt;
}

// A build begins: where the numbering stands, and whether a callback has
// an id for it to go on from instead. The first that has is followed.
void BuildBegins()
{
    g_build = {};
    g_build.going = true;
    g_build.numbered.last = LastNewId().value_or(kInvalidObject);
    const std::lock_guard lock(g_callbacksMutex);
    for (const auto &starting : g_starting)
    {
        const std::optional<ObjectId> from = starting(g_build.numbered.last);
        if (from && !g_build.following && *from < kMostNewId)
        {
            g_build.following = true;
            g_build.numbered.last = *from;
            g_build.next = *from + 1;
        }
    }
}

void BuildEnds()
{
    g_build.going = false;
    const AreasBuilt numbered = g_build.numbered;
    spdlog::debug("objects: a list's areas built: {} new object(s) numbered on from 0x{:x}{}, {} refused",
                  numbered.objects, numbered.last, g_build.following ? ", as asked" : "", numbered.refused);
    const std::lock_guard lock(g_callbacksMutex);
    for (const auto &built : g_built)
    {
        built(numbered);
    }
}

using LoadAreasFn = int(__fastcall *)(void *list, int edx);
Detour<LoadAreasFn> g_loadAreas;
std::once_flag g_loadAreasOnce;

// CSAreaList::LoadAreas builds one area a call: 0 for more to come, 1 for
// all built, anything else a failure.
int __fastcall LoadAreas(void *list, int edx)
{
    if (!g_build.going)
    {
        BuildBegins();
    }
    g_build.thread = GetCurrentThreadId();
    const int answer = g_loadAreas.Original()(list, edx);
    g_build.thread = 0;
    if (answer != 0)
    {
        BuildEnds();
    }
    return answer;
}

} // namespace

ObjectId CreateAs(ObjectId id, std::int32_t type, const Resource &tmpl, const CScriptLocation &location,
                  std::wstring_view overrideScript, bool spawnActive, bool noPermanentDeath)
{
    if (id < kLeastNewId || id > kMostNewId)
    {
        return kInvalidObject;
    }
    if (!AttachAddObject())
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

// The same road as CreateAs (DAOLib #103): the scripts' CreateItemOnObject (0x00673630) makes
// the items first (0x007595A0, a vector of them, one for each stack the size asks for), each an
// object constructed with -1, and only then puts them in the target's inventory, so the first
// object of the command is the first item.
//
// Seen in play 2026-10-12 (examples/objectids, GOG's game, a Redcliffe autosave): a bag
// (gen_ip_bodybag) made under 0x434C3 got it, and 1,950 coins (gen_im_copper) made in it under
// 0x434C4 got that, a stack of 1,950, one item in the bag. Fifty more asked for under the same
// id got the next free one, 0x435F4: the first stack was then 2,000 and the bag still held one
// item, and 0x435F4 was still an object, a stack of 50 that is in no inventory, which is the
// command's own way with an item that joins a stack.
ObjectId CreateItemAs(ObjectId id, const Resource &tmpl, ObjectId target, std::int32_t stackSize, std::wstring_view tag,
                      bool suppressNotification, bool droppable)
{
    if (id < kLeastNewId || id > kMostNewId)
    {
        return kInvalidObject;
    }
    if (!AttachAddObject())
    {
        return kInvalidObject;
    }
    g_wantedThread = GetCurrentThreadId();
    g_wanted = id;
    const ObjectId made =
        script::api::CreateItemOnObject(tmpl, target, stackSize, tag, suppressNotification ? 1 : 0, droppable ? 1 : 0);
    // Still wanted if nothing was constructed: no such template, no such target.
    g_wanted = kInvalidObject;
    return made;
}

std::optional<ObjectId> LastNewId()
{
    const auto pool = FirstPool();
    return pool ? std::optional<ObjectId>(pool->last) : std::nullopt;
}

void OnAreasBuild(std::function<std::optional<ObjectId>(ObjectId last)> starting,
                  std::function<void(const AreasBuilt &)> built)
{
    {
        const std::lock_guard lock(g_callbacksMutex);
        if (starting)
        {
            g_starting.push_back(std::move(starting));
        }
        if (built)
        {
            g_built.push_back(std::move(built));
        }
    }
    std::call_once(g_loadAreasOnce, [] {
        if (!AttachAddObject() || !g_loadAreas.Attach(addr::kCSAreaList_LoadAreas, &LoadAreas))
        {
            spdlog::warn("objects: could not detour the building of areas; no build is followed");
        }
    });
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
