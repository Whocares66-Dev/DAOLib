#include "perf/fastload/AreaLists.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/String.h"
#include "dao/core/Msvc8.h"
#include "perf/fastload/AddressSpace.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

// Everything here runs on the game's update thread, inside the loading
// mode's update, so nothing is locked. The update itself is detoured once
// for all of fast load (perf/FastLoad.cpp), which calls BeforeUpdate and
// AfterUpdate around the engine's.
namespace perf::fastload::lists
{

namespace
{

// The loading mode (dao/Addresses.h, "The loading mode's states"). Its state comes with the update.
constexpr std::size_t kData = 0x130;    // in the mode: its data
constexpr std::size_t kSwapStep = 0x6C; // in the data: the step of states 4 and 5
constexpr std::size_t kAreaTag = 0xD4;  // in the mode: the area asked for, a std::wstring
constexpr std::uint16_t kStateStart = 2;
constexpr std::uint16_t kStateListSwap = 4;
constexpr std::uint16_t kStateStartOther = 8;
constexpr std::int32_t kStepDrop = 5;
constexpr std::int32_t kStepMake = 6;
constexpr std::int32_t kStepWait = 8; // the RIM trees switched at 7, waited for here

// The module's area list and the RIM trees (dao/Addresses.h, "The module's
// area list" and "The list's RIM trees").
constexpr std::size_t kModuleList = 0x2CC;  // in the module: its CSAreaList
constexpr std::size_t kListFirst = 0xB8;    // in the list: its areas' entries, a vector
constexpr std::size_t kListLast = 0xBC;     //
constexpr std::size_t kListCursor = 0xC8;   // in the list: the next entry to save or build
constexpr std::size_t kEntrySize = 0x48;    //
constexpr std::size_t kAppResources = 0xC0; // in the application: its ModuleResourceManager
constexpr std::size_t kCurrentTree = 0x7C;  // in the manager: the RIM tree loaded for the list
constexpr std::size_t kNodeTree = 0x58;     // in a tree's node: its tree

// What the loading mode is doing while its update runs; state 0 outside it.
struct Doing
{
    std::uint16_t state = 0;
    std::int32_t step = 0;
};
Doing g_doing;

// A list out of the module, and the RIM tree that was loaded for it.
struct Kept
{
    std::uintptr_t list = 0;
    std::uintptr_t tree = 0;
};
std::vector<Kept> g_kept;                // the lists left before, the newest first
std::size_t g_capacity = 8;              // how many of them at the most: DAOLIB_AREA_LISTS
Kept g_leaving;                          // the list left in this swap, until the new one is made
std::uintptr_t g_handedBack = 0;         // the list put back in this swap, until its read is skipped
std::vector<std::uintptr_t> g_treesGone; // the trees of lists deleted in this swap, to unload after step 7
bool g_swapping = false;                 // a swap has reached its step 5 and not yet its step 6
std::uintptr_t g_mode = 0;               // the loading mode, while its update runs
std::uint32_t g_hits = 0;
std::uint32_t g_misses = 0;

// The application's ModuleResourceManager, or 0.
std::uintptr_t Resources()
{
    const auto app = reinterpret_cast<std::uintptr_t>(dao::CClientExoApp::Get());
    return app != 0 ? At<std::uintptr_t>(app, kAppResources) : 0;
}

std::uint32_t AreaCount(std::uintptr_t list)
{
    const auto first = At<std::uintptr_t>(list, kListFirst);
    const auto last = At<std::uintptr_t>(list, kListLast);
    return first != 0 ? static_cast<std::uint32_t>((last - first) / kEntrySize) : 0;
}

// Whether two lists hold an area of the same ID: an entry's first word. The
// engine's build refuses an area the application already has, and the
// loading fails with it.
bool Overlap(std::uintptr_t one, std::uintptr_t other)
{
    for (std::uint32_t i = 0; i < AreaCount(one); ++i)
    {
        const auto id = At<std::uint32_t>(At<std::uintptr_t>(one, kListFirst), i * kEntrySize);
        for (std::uint32_t j = 0; j < AreaCount(other); ++j)
        {
            if (At<std::uint32_t>(At<std::uintptr_t>(other, kListFirst), j * kEntrySize) == id)
            {
                return true;
            }
        }
    }
    return false;
}

// CSAreaList's deleting destructor, its only virtual: what the module's
// DropAreaList calls. It deletes each of the list's areas. The list's tree
// is left for the swap to unload (g_treesGone); outside a swap it stays
// loaded, and the engine finds it so if it wants it again.
void Delete(Kept &kept, const char *why)
{
    spdlog::info("area lists: deleting the list at {:#x} ({} areas): {}", kept.list, AreaCount(kept.list), why);
    using Fn = void *(__fastcall *)(std::uintptr_t self, int edx, std::uint32_t flags);
    const auto destructor = At<Fn>(At<std::uintptr_t>(kept.list, 0), 0);
    destructor(kept.list, 0, 1);
    if (g_doing.state == kStateListSwap && kept.tree != 0)
    {
        g_treesGone.push_back(kept.tree);
    }
    kept = Kept{};
}

// Whether a tree is that of a list kept or being left: such a tree stays loaded.
bool TreeKept(std::uintptr_t tree)
{
    return tree != 0 && (tree == g_leaving.tree ||
                         std::ranges::any_of(g_kept, [tree](const Kept &kept) { return kept.tree == tree; }));
}

// CSAreaList::FindArea: __thiscall (const std::wstring &tag), `ret 4`; the
// list's area of that tag, or null.
std::uintptr_t FindArea(std::uintptr_t list, std::uintptr_t tag)
{
    using Fn = std::uintptr_t(__fastcall *)(std::uintptr_t self, int edx, std::uintptr_t tag);
    return reinterpret_cast<Fn>(dao::Module::Address(dao::addr::kCSAreaList_FindArea))(list, 0, tag);
}

// Nothing kept outlives a game starting, a save loading or the module
// itself: `why` says which.
void DeleteKept(const char *why)
{
    if (g_leaving.list != 0)
    {
        Delete(g_leaving, why);
    }
    for (Kept &kept : g_kept)
    {
        Delete(kept, why);
    }
    g_kept.clear();
}

// CCModule::DropAreaList: __thiscall, no arguments; deletes the module's
// list and nulls the field. What it leaves in eax is passed on unread. The
// swap's step 5 is the one call answered by keeping the list; its others (a
// load, in state 2's machine at 0x006C80C4; a read that failed; the
// module's own end, from 0x00603F39) delete what is kept too.
using DropFn = std::uintptr_t(__fastcall *)(std::uintptr_t module, int edx);
dao::Detour<DropFn> g_drop;

std::uintptr_t __fastcall DropAreaList(std::uintptr_t module, int edx)
{
    const std::uintptr_t list = At<std::uintptr_t>(module, kModuleList);
    const std::uintptr_t resources = Resources();
    if (On() && g_doing.state == kStateListSwap && g_doing.step == kStepDrop && list != 0 && g_leaving.list == 0 &&
        resources != 0)
    {
        g_leaving = Kept{list, At<std::uintptr_t>(resources, kCurrentTree)};
        At<std::uintptr_t>(module, kModuleList) = 0;
        return 0;
    }
    DeleteKept("the module is dropping its list, and not for a swap");
    return g_drop.Original()(module, edx);
}

// CSAreaList::ReadAreas: __thiscall (int), `ret 4`; empties the list's
// entries and reads them again, answering 1, or 0 if it found none.
using ReadFn = std::int32_t(__fastcall *)(std::uintptr_t list, int edx, std::int32_t flag);
dao::Detour<ReadFn> g_read;

std::int32_t __fastcall ReadAreas(std::uintptr_t list, int edx, std::int32_t flag)
{
    if (list == g_handedBack)
    {
        g_handedBack = 0;
        return 1; // its areas are read, and built
    }
    const std::int32_t result = g_read.Original()(list, edx, flag);
    for (Kept &kept : g_kept)
    {
        if (kept.list != list && Overlap(list, kept.list))
        {
            Delete(kept, "the list being made holds one of its areas");
        }
    }
    std::erase_if(g_kept, [](const Kept &kept) { return kept.list == 0; });
    return result;
}

// RIMTreeManager::Unload: __thiscall (tree), `ret 4`; starts a tree's
// unloading, which the manager's update then waits on, and answers in al
// whether it did. The swap does not read the answer; eax is passed on whole,
// and a tree left loaded answers false.
using UnloadFn = std::uintptr_t(__fastcall *)(std::uintptr_t manager, int edx, std::uintptr_t tree);
dao::Detour<UnloadFn> g_unload;

std::uintptr_t __fastcall UnloadTree(std::uintptr_t manager, int edx, std::uintptr_t tree)
{
    if (TreeKept(tree))
    {
        return 0;
    }
    std::erase(g_treesGone, tree); // the engine is unloading it itself
    return g_unload.Original()(manager, edx, tree);
}

// Before the engine makes the new list: a kept list that holds the area
// asked for goes back into the module; the list just left is kept; and the
// oldest is deleted when there is one too many.
void BeforeMake(std::uintptr_t mode)
{
    dao::CClientExoApp *app = dao::CClientExoApp::Get();
    const auto module = reinterpret_cast<std::uintptr_t>(app != nullptr ? app->GetModule() : nullptr);
    if (module == 0)
    {
        return;
    }
    const auto *tag = reinterpret_cast<const dao::core::WStringRep *>(mode + kAreaTag);
    const auto has =
        std::ranges::find_if(g_kept, [&](const Kept &kept) { return FindArea(kept.list, mode + kAreaTag) != 0; });
    if (has != g_kept.end() && At<std::uintptr_t>(module, kModuleList) == 0)
    {
        ++g_hits;
        At<std::uintptr_t>(module, kModuleList) = has->list;
        At<std::uint32_t>(has->list, kListCursor) = AreaCount(has->list); // every area built
        g_handedBack = has->list;
        g_kept.erase(has);
    }
    else
    {
        ++g_misses;
    }
    if (g_leaving.list != 0)
    {
        g_kept.insert(g_kept.begin(), g_leaving);
        g_leaving = Kept{};
    }
    while (g_kept.size() > g_capacity)
    {
        Delete(g_kept.back(), "it is the oldest, and one more is kept");
        g_kept.pop_back();
    }
    // A kept list is paid for in address space, some 50 to 150 MiB each in play.
    while (!g_kept.empty() && AddressSpaceShort())
    {
        Delete(g_kept.back(), "it is the oldest, and the address space is running short");
        g_kept.pop_back();
    }
    spdlog::info("area lists: \"{}\" is in a list {}; {} kept now, {} handed back and {} made so far",
                 dao::Utf8(tag->View()),
                 g_handedBack != 0 ? "that was kept, and is handed back" : "not kept, to be made", g_kept.size(),
                 g_hits, g_misses);
}

// The engine has switched the RIM trees (step 7), or thinks it need not: it
// does nothing when the tree of the area asked for is loaded already, which
// with lists kept it can be while another is the manager's current one (a
// list handed back; a list whose tree outlived it, deleted at a load). The
// manager's current tree is what the next transition takes for the list the
// party is in (0x005FBBD0, read by 0x006C7080): left stale, a trip back was
// taken for a door within the list, found no such area, and ended at once
// with the party where it stood (in play, 2026-10-10, after a load). So the
// tree of the area asked for is made the current one here, by the engine's
// own lookup. Then the tree of a list deleted in this swap is unloaded,
// unless it is the one in use.
void AfterTrees()
{
    const std::uintptr_t resources = Resources();
    if (resources == 0 || g_mode == 0)
    {
        return;
    }
    // RIMTreeManager::FindNode: __thiscall (const std::wstring &area tag), `ret 4`; the area's
    // node, its tree at +0x58, or null.
    using FindFn = std::uintptr_t(__fastcall *)(std::uintptr_t manager, int edx, std::uintptr_t tag);
    const std::uintptr_t node = reinterpret_cast<FindFn>(dao::Module::Address(dao::addr::kRIMTreeManager_FindNode))(
        resources, 0, g_mode + kAreaTag);
    const std::uintptr_t tree = node != 0 ? At<std::uintptr_t>(node, kNodeTree) : 0;
    auto &current = At<std::uintptr_t>(resources, kCurrentTree);
    if (tree != 0 && current != tree)
    {
        current = tree;
    }
    for (const std::uintptr_t gone : g_treesGone)
    {
        if (gone != current && !TreeKept(gone))
        {
            g_unload.Original()(resources, 0, gone);
        }
    }
    g_treesGone.clear();
}

} // namespace

void Attach()
{
    std::array<char, 8> asked{};
    if (const DWORD length =
            GetEnvironmentVariableA("DAOLIB_AREA_LISTS", asked.data(), static_cast<DWORD>(asked.size()));
        length > 0 && length < asked.size())
    {
        g_capacity = std::strtoul(asked.data(), nullptr, 10);
    }
    if (g_capacity == 0)
    {
        spdlog::info("fast load: DAOLIB_AREA_LISTS=0, so an area list the party leaves is deleted as ever");
        return;
    }
    // All three or none: some of them would leave a list neither kept nor deleted.
    if (!g_read.Attach(dao::addr::kCSAreaList_ReadAreas, &ReadAreas) ||
        !g_drop.Attach(dao::addr::kCCModule_DropAreaList, &DropAreaList) ||
        !g_unload.Attach(dao::addr::kRIMTreeManager_Unload, &UnloadTree))
    {
        g_unload.Detach();
        g_drop.Detach();
        g_read.Detach();
        g_capacity = 0;
        spdlog::warn("fast load: the area lists' detours could not be placed, so a list left is deleted as ever");
        return;
    }
    spdlog::info("fast load: the area lists the party leaves are kept while memory allows, {} at the most, and handed "
                 "back on its return",
                 g_capacity);
}

void BeforeUpdate(const Update &update)
{
    if (g_capacity == 0)
    {
        return;
    }
    const auto data = At<std::uintptr_t>(update.mode, kData);
    g_mode = update.mode;
    g_doing.state = update.state;
    g_doing.step = data != 0 ? At<std::int32_t>(data, kSwapStep) : 0;
    // On or off: a list kept and then deleted leaves its RIM tree loaded, and the engine does
    // not make a tree it finds loaded the current one. In play, with the switch turned off
    // after the castle's list had been kept: the swap to the castle left the village's tree the
    // current one, and every trip back to the village by the map was taken for a door within
    // the list and ended at once, the party where it stood, until a trip elsewhere.
    if (g_doing.state == kStateListSwap && g_doing.step == kStepWait)
    {
        AfterTrees();
    }
    if (!On())
    {
        // Off, nothing more is kept (DropAreaList asks), and what was kept goes at the next
        // loading screen, not in the middle of a swap that has taken a list out of the module.
        if (!g_swapping)
        {
            DeleteKept("fast loading is off");
        }
        return;
    }
    if (g_doing.state == kStateStart || g_doing.state == kStateStartOther)
    {
        DeleteKept("a game is starting or a save loading");
    }
    else if (g_doing.state == kStateListSwap && g_doing.step == kStepDrop)
    {
        g_swapping = true;
    }
    else if (g_doing.state == kStateListSwap && g_doing.step == kStepMake && g_swapping)
    {
        g_swapping = false;
        BeforeMake(update.mode);
    }
}

void AfterUpdate()
{
    g_doing = Doing{};
    g_mode = 0;
}

} // namespace perf::fastload::lists
