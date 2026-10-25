// Example: an item's requirements (dao/Items.h).
//
// Each time the inventory opens, every item in the party's backpack that asks something of the
// character the screen shows is logged with what it asks, whether that is met, and whether the
// character can use the item; what it sees goes to example_items.log.
#include "dao/Items.h"
#include "dao/Gui.h"
#include "dao/Log.h"
#include "dao/Party.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <string>

namespace
{

namespace api = dao::script::api;
namespace items = dao::items;

void Log()
{
    const dao::ObjectId who = dao::party::ScreenCharacter();
    const auto held = api::GetItemsInInventory(api::GetHero(), dao::script::constants::GET_ITEMS_OPTION_BACKPACK);
    for (std::size_t i = 0; i < held.Size(); ++i)
    {
        const dao::ObjectId item = held.Object(i);
        const auto needs = items::Requirements(item, who);
        if (needs.empty())
        {
            continue;
        }
        std::string asks;
        for (const items::Requirement &need : needs)
        {
            asks += fmt::format(" property {} {}{}", need.property, need.value, need.met ? "" : " (not met)");
        }
        spdlog::info("{} for {}:{}; {}", dao::Utf8(api::GetName(item)), dao::Utf8(api::GetName(who)), asks,
                     items::CanUse(who, item) ? "usable" : "not usable");
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_items");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::gui::OnFullScreen([](std::wstring_view name, bool shown) {
        if (shown && name == L"Inventory")
        {
            Log();
        }
    });
    return true;
}
