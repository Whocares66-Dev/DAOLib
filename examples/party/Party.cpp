// Example: who the player controls, and what the party puts on and takes
// off (dao/Party.h, dao/Equipment.h).
//
// Each change of the controlled creature is logged, whatever made it: a
// click in the party bar, a key, a script, another plugin's order. Each
// item equipped or taken off is logged too, by whatever path: the
// inventory screen, a script, an equip that displaces another item. A
// line gives the creature, the item, its slot and whether the engine did
// it. Each full screen that opens is logged with the character it acts on.
// What it finds goes to example_party.log.
#include "dao/Party.h"
#include "dao/Equipment.h"
#include "dao/Gui.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <string>

namespace
{

namespace api = dao::script::api;

std::string Named(dao::ObjectId object)
{
    return object != dao::kInvalidObject ? fmt::format("{} 0x{:x}", dao::Utf8(api::GetName(object)), object)
                                         : std::string("nobody");
}

void LogChange(const char *what, const dao::equipment::Change &change)
{
    spdlog::info("{} {} {} in slot {}{}{}", Named(change.creature), what, Named(change.item), change.slot,
                 change.displaced ? ", displaced" : "", change.done ? "" : ": refused");
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_party");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::party::OnControlChanged(
        [](dao::ObjectId before, dao::ObjectId now) { spdlog::info("control: {} to {}", Named(before), Named(now)); });
    dao::equipment::OnEquip([](const dao::equipment::Change &change) { LogChange("put on", change); });
    dao::equipment::OnUnequip([](const dao::equipment::Change &change) { LogChange("took off", change); });
    dao::gui::OnFullScreen([](std::wstring_view name, bool shown) {
        if (shown)
        {
            spdlog::info("{} acts on {}", dao::Utf8(std::wstring(name)), Named(dao::party::ScreenCharacter()));
        }
    });
    return true;
}
