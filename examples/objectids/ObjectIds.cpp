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

#include <cstdint>
#include <optional>

namespace
{

namespace api = dao::script::api;
namespace objects = dao::objects;
using dao::ObjectId;

const dao::Resource kTemplate{L"gen00fl_dog.utc"};
constexpr std::int32_t kCreature = dao::script::constants::OBJECT_TYPE_CREATURE;

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

    // The game's own numbering moved far ahead: what the scripts make next is there.
    const ObjectId far = *start + 0x100000;
    spdlog::info("numbering new objects from 0x{:x}: {}; from an id behind: {}", far, objects::NumberNewFrom(far),
                 objects::NumberNewFrom(behind));
    const ObjectId moved = api::CreateObject(kCreature, kTemplate, where);
    Log("made by CreateObject after", moved, far);

    for (const ObjectId made : {plain, chosen, taken, filled, moved})
    {
        if (made != dao::kInvalidObject)
        {
            api::DestroyObject(made);
        }
    }
}

void OnFrame()
{
    static bool done = false;
    if (done || dao::CClientExoApp::CurrentMode() != dao::GameMode::Explore)
    {
        return;
    }
    const ObjectId player = api::GetMainControlled();
    if (player == dao::kInvalidObject)
    {
        return;
    }
    done = true;
    Show(player);
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
    dao::OnFrame(&OnFrame);
    return true;
}
