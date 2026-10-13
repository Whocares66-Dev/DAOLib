// Example: an object's id chosen by the plugin (dao/ObjectIds.h).
//
// The game numbers its new objects one after the other, and a save keeps
// where it had got to. Two games loaded from one save therefore agree on
// every id until each makes objects of its own. This shows the two calls
// that let one game follow another: a creature made under an id the plugin
// names, and the game's own numbering moved out of the way.
//
// Once, when the player first stands in the world, it makes the same dog
// five ways beside the controlled creature, logs the id each got against
// the id expected, and destroys them again. What it finds goes to
// example_objectids.log. It saves nothing, but a save made after it keeps
// the numbering it moved.
//
// It also follows the game building areas, which is when an area never
// visited numbers everything in it: each build is logged with where the
// numbering stood and how many new objects it made. With the variable
// EXAMPLE_OBJECTIDS_FOLLOW set to such an id (in hex, from another run's
// log), the game's own numbering is moved far ahead after the dogs and the
// next build is numbered on from that id instead: the area's objects then
// have the ids they had in the other run.
#include "dao/ObjectIds.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/Structures.h"
#include "dao/generated/ScriptApi.h"
#include "dao/generated/ScriptConstants.h"

#include <spdlog/spdlog.h>

#include <Windows.h>

#include <array>
#include <cstdint>
#include <cwchar>
#include <optional>
#include <utility>

namespace
{

namespace api = dao::script::api;
namespace objects = dao::objects;
using dao::ObjectId;

const dao::Resource kTemplate{L"gen00fl_dog.utc"};
constexpr std::int32_t kCreature = dao::script::constants::OBJECT_TYPE_CREATURE;
constexpr std::int32_t kPlaceable = dao::script::constants::OBJECT_TYPE_PLACEABLE;
// What the scripts leave where a creature died by the party's hand, and what they put in it.
const dao::Resource kBag{L"gen_ip_bodybag.utp"};
const dao::Resource kCoins{L"gen_im_copper.uti"};

void Log(const char *what, ObjectId got, ObjectId expected)
{
    spdlog::info("{}: got 0x{:x}, expected 0x{:x}: {}; \"{}\" with {} item(s); the last new id is 0x{:x}", what, got,
                 expected, got == expected ? "as expected" : "NOT as expected", dao::Utf8(api::GetTag(got)),
                 api::GetItemsInInventory(got).Size(), objects::LastNewId().value_or(0));
}

void Show(ObjectId player)
{
    const std::optional<ObjectId> start = objects::LastNewId();
    if (!start)
    {
        spdlog::error("the game's numbering could not be read");
        return;
    }
    const dao::CScriptLocation where = api::GetLocation(player);
    spdlog::info("the last new id is 0x{:x}", *start);

    // As the scripts make one: the next id.
    const ObjectId plain = api::CreateObject(kCreature, kTemplate, where);
    Log("made by CreateObject", plain, *start + 1);

    // Under an id ahead of the numbering: it gets it, and the numbering goes on from there.
    const ObjectId ahead = *start + 1000;
    const ObjectId chosen = objects::CreateAs(ahead, kCreature, kTemplate, where);
    Log("made under an id ahead", chosen, ahead);

    // Under an id an object has: it gets the next free one, and the caller sees that it did.
    const ObjectId taken = objects::CreateAs(ahead, kCreature, kTemplate, where);
    spdlog::info("made under an id in use: got 0x{:x}, {}", taken,
                 taken != ahead && taken != dao::kInvalidObject ? "another, as expected" : "NOT as expected");

    // Under a free id behind the numbering: it gets it, and the numbering stays.
    const ObjectId behind = *start + 500;
    const ObjectId filled = objects::CreateAs(behind, kCreature, kTemplate, where);
    Log("made under a free id behind", filled, behind);

    // What a kill leaves, as another game made it: the bag under one id and the coins in it
    // under another, then coins asked for under the id the first have.
    const ObjectId bagId = *start + 700;
    const ObjectId bag = objects::CreateAs(bagId, kPlaceable, kBag, where);
    const ObjectId coins = objects::CreateItemAs(bagId + 1, kCoins, bag, 1950);
    spdlog::info("a bag under 0x{:x}: got 0x{:x}; coins in it under 0x{:x}: got 0x{:x}, {}; a stack of {}, {} "
                 "item(s) in the bag",
                 bagId, bag, bagId + 1, coins, coins == bagId + 1 ? "as expected" : "NOT as expected",
                 api::GetItemStackSize(coins), api::GetItemsInInventory(bag).Size());
    const ObjectId more = objects::CreateItemAs(bagId + 1, kCoins, bag, 50);
    spdlog::info("more coins under the id in use: got 0x{:x}, {} an object, a stack of {}; the first coins now a "
                 "stack of {}; {} item(s) in the bag; the last new id is 0x{:x}",
                 more, api::IsObjectValid(more) != 0 ? "still" : "no longer", api::GetItemStackSize(more),
                 api::GetItemStackSize(coins), api::GetItemsInInventory(bag).Size(), objects::LastNewId().value_or(0));
    if (bag != dao::kInvalidObject)
    {
        api::DestroyObject(bag);
    }

    // The game's own numbering moved far ahead: what the scripts make next is there.
    const ObjectId apart = *start + 0x100000;
    spdlog::info("numbering new objects from 0x{:x}: {}; from an id behind: {}", apart, objects::NumberNewFrom(apart),
                 objects::NumberNewFrom(behind));
    const ObjectId moved = api::CreateObject(kCreature, kTemplate, where);
    Log("made by CreateObject after", moved, apart);

    for (const ObjectId made : {plain, chosen, taken, filled, moved})
    {
        if (made != dao::kInvalidObject)
        {
            api::DestroyObject(made);
        }
    }
}

// The id EXAMPLE_OBJECTIDS_FOLLOW names, for the next build after the dogs.
std::optional<ObjectId> g_follow;
bool g_shown = false;

std::optional<ObjectId> FollowAsked()
{
    std::array<wchar_t, 32> text{};
    const DWORD length = GetEnvironmentVariableW(L"EXAMPLE_OBJECTIDS_FOLLOW", text.data(), 32);
    return length != 0 && length < text.size() ? std::optional<ObjectId>(std::wcstoul(text.data(), nullptr, 16))
                                               : std::nullopt;
}

void OnFrame()
{
    if (g_shown || dao::CClientExoApp::CurrentMode() != dao::GameMode::Explore)
    {
        return;
    }
    const ObjectId player = api::GetMainControlled();
    if (player == dao::kInvalidObject)
    {
        return;
    }
    g_shown = true;
    Show(player);
    if (g_follow)
    {
        const ObjectId apart = objects::LastNewId().value_or(0) + 0x1000000;
        spdlog::info("following another run: this game's own numbering moved to 0x{:x}: {}", apart,
                     objects::NumberNewFrom(apart));
    }
}

std::optional<ObjectId> OnAreasStarting(ObjectId last)
{
    spdlog::info("areas begin to be built: the last new id is 0x{:x}", last);
    if (!g_shown || !g_follow)
    {
        return std::nullopt;
    }
    spdlog::info("numbering this build on from 0x{:x}, as asked", *g_follow);
    return std::exchange(g_follow, std::nullopt);
}

void OnAreasBuilt(const objects::AreasBuilt &built)
{
    spdlog::info("areas built: {} new object(s) numbered on from 0x{:x}, {} refused; the last new id is 0x{:x}",
                 built.objects, built.last, built.refused, objects::LastNewId().value_or(0));
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_objectids");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    g_follow = FollowAsked();
    objects::OnAreasBuild(&OnAreasStarting, &OnAreasBuilt);
    dao::OnFrame(&OnFrame);
    return true;
}
