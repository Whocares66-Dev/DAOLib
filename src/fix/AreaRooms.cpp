#include "fix/AreaRooms.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "fix/AddressSpace.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

// Everything here runs on the game's update thread: the loading mode's
// update, a transition's step inside it, and the update of the layout the
// party is in. So nothing is locked.
namespace fix
{

namespace
{

// The module's area list (dao/Addresses.h, "The module's area list").
constexpr std::size_t kModuleList = 0x2CC; // in the module: its CSAreaList
constexpr std::size_t kListFirst = 0xB8;   // in the list: its areas' entries, a vector; an entry begins with the id
constexpr std::size_t kListLast = 0xBC;    //
constexpr std::size_t kEntrySize = 0x48;   //

// An area's layout and its rooms (dao/Addresses.h, "Rooms and their streaming").
constexpr std::size_t kLayout = 0x658;       // in the area
constexpr std::size_t kLayoutOutdoor = 0x7C; // in the layout: 1 for an outdoor area
constexpr std::size_t kLayoutReady = 0x510;  // in the layout: a byte, set when it has its scene
constexpr std::size_t kRooms = 0x424;        // in the layout: an array of room pointers
constexpr std::size_t kRoomCount = 0x428;    //
constexpr std::size_t kRoomLoad = 0x264;     // in a room: 2 not loaded, 0 a load asked for, 1 loaded
constexpr std::size_t kRoomState = 0x268;    // in a room: the state last asked of it
constexpr std::size_t kRoomPending = 0x368;  // in a room: its objects are being built, to be taken in
constexpr std::size_t kRoomLoading = 0x371;  // in a room: a load in the background under way
constexpr std::size_t kRoomFinish = 24;      // a room's slot: takes the objects in when they are built
constexpr std::int32_t kNotLoaded = 2;
constexpr std::int32_t kLoadAsked = 0;
constexpr std::int32_t kWanted = 4; // a room's state: load it in the background
constexpr std::uint32_t kMaxRooms = 4096;

constexpr std::int64_t kAfterLoadingMs = 2000;   // the arrival's own loads first
constexpr std::int64_t kBetweenAsksMs = 50;      //
constexpr std::int64_t kGiveUpMs = 10000;        // a room asked for and not in by then is passed over
constexpr std::int64_t kShortAgainMs = 5000;     // how long before the address space is asked about again
constexpr std::int64_t kBetweenScreensMs = 1000; // an update this long after the last begins a loading screen

template <class T> T &At(std::uintptr_t object, std::size_t offset)
{
    return *reinterpret_cast<T *>(object + offset);
}

std::int64_t NowMs()
{
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

// What was done since the last loading screen, for one line in the log at the next.
struct Done
{
    std::uint32_t kept = 0;       // areas left with their rooms loaded
    std::uint32_t unloaded = 0;   // areas left with their rooms unloaded, for want of address space
    std::uint32_t asked = 0;      // rooms asked for ahead
    std::uint32_t takenIn = 0;    // rooms whose objects were taken in here
    std::int64_t longestAsk = 0;  // ms of a frame
    std::int64_t longestTake = 0; //
};
Done g_done;
std::int64_t g_lastLoadingUpdate = 0;
std::int64_t g_lastAsk = 0;

// CCArea::UnloadRooms: __thiscall, no arguments; has the area's layout unload every room. Its
// one caller is a transition's step 10, for the area the party's hero is still in, and takes
// nothing back from it.
using UnloadRoomsFn = void(__fastcall *)(std::uintptr_t area, int edx);
dao::Detour<UnloadRoomsFn> g_unloadRooms;

void __fastcall UnloadRooms(std::uintptr_t area, int edx)
{
    if (AddressSpaceShort())
    {
        ++g_done.unloaded;
        g_unloadRooms.Original()(area, edx);
        return;
    }
    ++g_done.kept;
}

// GameModeLoading::Update: __thiscall (float seconds), `ret 4`. What it leaves in eax is its
// base's, and is passed on unread.
using UpdateFn = std::uintptr_t(__fastcall *)(std::uintptr_t mode, int edx, float seconds);
dao::Detour<UpdateFn> g_update;

std::uintptr_t __fastcall Update(std::uintptr_t mode, int edx, float seconds)
{
    const std::int64_t now = NowMs();
    if (now - g_lastLoadingUpdate > kBetweenScreensMs && (g_done.asked != 0 || g_done.kept + g_done.unloaded != 0))
    {
        spdlog::info("area rooms: since the last loading screen began, {} area(s) left with rooms loaded and {} with "
                     "rooms unloaded; {} room(s) asked for ahead, {} ms of a frame at the most, and {} taken in, {} ms "
                     "at the most",
                     g_done.kept, g_done.unloaded, g_done.asked, g_done.longestAsk, g_done.takenIn, g_done.longestTake);
        g_done = Done{};
    }
    g_lastLoadingUpdate = now;
    return g_update.Original()(mode, edx, seconds);
}

// A layout's rooms as the preload sees them.
struct Rooms
{
    bool loading = false;    // a room's load is asked for and not in
    std::uintptr_t next = 0; // the first room not loaded and not asked for
};

// Looks over a layout the party is not in. A room waiting on its objects has them taken in:
// what AreaLayout::Update does each frame for the layout the party is in, and for no other.
Rooms LookOver(std::uintptr_t layout)
{
    Rooms found;
    const std::uint32_t count = std::min(At<std::uint32_t>(layout, kRoomCount), kMaxRooms);
    const auto rooms = At<std::uintptr_t>(layout, kRooms);
    for (std::uint32_t i = 0; rooms != 0 && i < count; ++i)
    {
        const std::uintptr_t room = At<std::uintptr_t>(rooms, i * sizeof(std::uintptr_t));
        if (room == 0)
        {
            continue;
        }
        if (At<std::uint8_t>(room, kRoomPending) != 0)
        {
            // The room's slot 24: __thiscall (float seconds, layout), `ret 8`. It does nothing
            // until the last of the room's objects is built.
            using FinishFn = void(__fastcall *)(std::uintptr_t room, int edx, float seconds, std::uintptr_t layout);
            const std::int64_t before = NowMs();
            At<FinishFn>(At<std::uintptr_t>(room, 0), kRoomFinish * sizeof(std::uintptr_t))(room, 0, 0.0F, layout);
            if (At<std::uint8_t>(room, kRoomPending) == 0)
            {
                ++g_done.takenIn;
                g_done.longestTake = std::max(g_done.longestTake, NowMs() - before);
            }
        }
        const auto load = At<std::int32_t>(room, kRoomLoad);
        found.loading = found.loading || load == kLoadAsked || At<std::uint8_t>(room, kRoomLoading) != 0;
        // A room already asked to be wanted and still not loaded is one the engine would not
        // load: asking again changes nothing.
        if (found.next == 0 && load == kNotLoaded && At<std::int32_t>(room, kRoomState) != kWanted)
        {
            found.next = room;
        }
    }
    return found;
}

// One step of the preload, after the update of the layout the party is in: over the list's
// other areas that are indoors and ready, at the most one room asked for, and none while one
// is loading.
void Preload(std::uintptr_t current)
{
    dao::CClientExoApp *app = dao::CClientExoApp::Get();
    const auto module = reinterpret_cast<std::uintptr_t>(app != nullptr ? app->GetModule() : nullptr);
    const std::uintptr_t list = module != 0 ? At<std::uintptr_t>(module, kModuleList) : 0;
    if (list == 0)
    {
        return;
    }
    // CClientExoApp::GetAreaById: __thiscall (area id), `ret 4`.
    using AreaFn = std::uintptr_t(__fastcall *)(dao::CClientExoApp * app, int edx, std::uint32_t id);
    const auto areaById = reinterpret_cast<AreaFn>(dao::Module::Address(dao::addr::kCClientExoApp_GetAreaById));
    const std::int64_t now = NowMs();
    const auto first = At<std::uintptr_t>(list, kListFirst);
    const auto last = At<std::uintptr_t>(list, kListLast);
    for (std::uintptr_t entry = first; first != 0 && entry < last; entry += kEntrySize)
    {
        const std::uintptr_t area = areaById(app, 0, At<std::uint32_t>(entry, 0));
        const std::uintptr_t layout = area != 0 ? At<std::uintptr_t>(area, kLayout) : 0;
        // Indoors and ready only: an outdoor area's rooms are its terrain's cells, and a layout
        // without its scene has nowhere to build a room.
        if (layout == 0 || layout == current || At<std::uint8_t>(layout, kLayoutReady) != 1 ||
            At<std::int32_t>(layout, kLayoutOutdoor) == 1)
        {
            continue;
        }
        const Rooms rooms = LookOver(layout);
        if (rooms.loading)
        {
            if (now - g_lastAsk < kGiveUpMs)
            {
                return;
            }
            continue;
        }
        if (rooms.next == 0)
        {
            continue;
        }
        if (now - g_lastLoadingUpdate < kAfterLoadingMs || now - g_lastAsk < kBetweenAsksMs)
        {
            return;
        }
        if (AddressSpaceShort())
        {
            g_lastAsk = now + kShortAgainMs; // nothing more is asked for a while
            return;
        }
        // CRoom::SetState: __thiscall (state, layout, 0), `ret 0xC`.
        using SetStateFn =
            void(__fastcall *)(std::uintptr_t room, int edx, std::int32_t state, std::uintptr_t layout, int);
        reinterpret_cast<SetStateFn>(dao::Module::Address(dao::addr::kCRoom_SetState))(rooms.next, 0, kWanted, layout,
                                                                                       0);
        g_lastAsk = now;
        ++g_done.asked;
        g_done.longestAsk = std::max(g_done.longestAsk, NowMs() - now);
        return;
    }
}

// AreaLayout::Update: __thiscall, no arguments, each frame for the layout the party is in; what
// it leaves in eax is passed on.
using LayoutUpdateFn = std::uintptr_t(__fastcall *)(std::uintptr_t layout, int edx);
dao::Detour<LayoutUpdateFn> g_layoutUpdate;

std::uintptr_t __fastcall LayoutUpdate(std::uintptr_t layout, int edx)
{
    const std::uintptr_t result = g_layoutUpdate.Original()(layout, edx);
    Preload(layout);
    return result;
}

bool TurnedOff()
{
    std::array<char, 4> value{};
    const DWORD length = GetEnvironmentVariableA("DAOLIB_AREA_ROOMS", value.data(), static_cast<DWORD>(value.size()));
    return length == 1 && value[0] == '0';
}

} // namespace

void InstallAreaRoomsFix()
{
    if (TurnedOff())
    {
        spdlog::info("fixes: DAOLIB_AREA_ROOMS=0, so an area's rooms are unloaded when the party leaves and none is "
                     "loaded ahead (issue #72)");
        return;
    }
    // All three or none: a room asked for ahead in an area whose rooms the engine then unloads
    // is work for nothing, and the preload wants to know when a loading screen was.
    if (!g_unloadRooms.Attach(dao::addr::kCCArea_UnloadRooms, &UnloadRooms) ||
        !g_update.Attach(dao::addr::kGameModeLoading_Update, &Update) ||
        !g_layoutUpdate.Attach(dao::addr::kAreaLayout_Update, &LayoutUpdate))
    {
        g_layoutUpdate.Detach();
        g_update.Detach();
        g_unloadRooms.Detach();
        spdlog::warn("fixes: the rooms' detours could not be placed, so an area's rooms are unloaded when the party "
                     "leaves and none is loaded ahead (issue #72)");
        return;
    }
    spdlog::info("fixes: an area's rooms stay loaded when the party leaves it, and the rooms of the indoor areas "
                 "beside it are loaded ahead, while memory allows (issue #72)");
}

} // namespace fix
