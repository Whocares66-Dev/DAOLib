// Example: a follower's tactics (dao/Tactics.h).
//
// Each time the tactics screen opens, the party's tactics are logged: each follower's AI switch
// and slots, and what the screen offers a slot to choose from; what it sees goes to
// example_tactics.log.
#include "dao/Tactics.h"
#include "dao/Gui.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

namespace
{

namespace api = dao::script::api;
namespace tactics = dao::tactics;

void LogFollower(dao::ObjectId who)
{
    const auto slots = tactics::Slots(who);
    spdlog::info("{} 0x{:x}: AI {}, {} slots", dao::Utf8(api::GetName(who)), who,
                 tactics::AiOn(who).value_or(false) ? "on" : "off", slots.size());
    for (const tactics::Slot &slot : slots)
    {
        spdlog::info("  {}{}: target {}, condition {}, command {} {}", slot.index, slot.on ? "" : " (off)", slot.target,
                     slot.condition, slot.command, slot.parameter);
    }
    const auto targets = tactics::Targets(who);
    std::size_t conditions = 0;
    for (const tactics::Target &target : targets)
    {
        for (const std::int32_t base : tactics::BaseConditions(target.type))
        {
            conditions += tactics::Conditions(base, target.type).size();
        }
    }
    spdlog::info("  offered: {} targets, {} conditions, {} sustained abilities", targets.size(), conditions,
                 tactics::ModalAbilities(who).size());
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_tactics");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::gui::OnFullScreen([](std::wstring_view name, bool shown) {
        if (!shown || name != L"Tactics")
        {
            return;
        }
        const auto members = api::GetPartyList(api::GetHero());
        for (std::size_t i = 0; i < members.Size(); ++i)
        {
            LogFollower(members.Object(i));
        }
    });
    return true;
}
