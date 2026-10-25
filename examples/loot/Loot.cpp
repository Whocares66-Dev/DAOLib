// Example: the loot panel's buttons (dao/Loot.h).
//
// Each time a container's loot panel opens, everything in it is taken as Take All takes it and
// the panel closes, unless the backpack is full; what was done goes to example_loot.log.
#include "dao/Loot.h"
#include "dao/Gui.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

namespace
{

namespace api = dao::script::api;
namespace loot = dao::loot;

void TakeEverything()
{
    const auto container = loot::Container();
    if (!container)
    {
        return;
    }
    const std::string name = dao::Utf8(api::GetName(*container));
    const loot::Result result = loot::TakeAll();
    spdlog::info("{} 0x{:x}: {}", name, *container,
                 result == loot::Result::Closed ? "taken and closed" : "the backpack is full; left open");
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_loot");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::gui::OnFullScreen([](std::wstring_view, bool shown) {
        if (shown)
        {
            TakeEverything();
        }
    });
    return true;
}
