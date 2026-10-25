// Example: a store's prices (dao/Store.h).
//
// Each time a store's screen opens, what it stocks is logged with what each costs, and the
// party's money; what it sees goes to example_store.log.
#include "dao/Store.h"
#include "dao/Gui.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

namespace
{

namespace api = dao::script::api;
namespace store = dao::store;

void Log()
{
    const auto open = store::Current();
    if (!open)
    {
        return;
    }
    spdlog::info("{} 0x{:x}, the party's money {}", dao::Utf8(api::GetName(*open)), *open, store::Money());
    for (const store::Entry &entry : store::Stock())
    {
        spdlog::info("  {} 0x{:x}: {}", dao::Utf8(api::GetName(entry.item)), entry.item, entry.price);
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_store");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::gui::OnFullScreen([](std::wstring_view name, bool shown) {
        if (shown && name == L"Store")
        {
            Log();
        }
    });
    return true;
}
