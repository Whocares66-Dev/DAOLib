// Example: the engine's messages read by their fields (dao/Messages.h), the abilities' first.
//
// Each typed message of the abilities is logged as it is delivered, whoever sent it: an ability
// pressed on the quickbar, a target clicked or the targeting left, an icon dropped on a slot, an
// ability picked on the level-up screen. A line gives the message, its receiver's class and
// each field. What it finds goes to example_messages.log.
#include "dao/Messages.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/TypeName.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <string>

namespace
{

namespace api = dao::script::api;
namespace messages = dao::messages;

std::string Named(dao::ObjectId object)
{
    return object != dao::kInvalidObject ? fmt::format("{} 0x{:x}", dao::Utf8(api::GetName(object)), object)
                                         : std::string("nobody");
}

// Logs every message of class M as `what` makes its fields into text.
template <class M, class F> void Log(const char *name, F what)
{
    messages::OnMessage<M>([name, what](void *receiver, const M &message) {
        spdlog::info("{} to {}: {}", name, dao::TypeName(receiver), what(message));
    });
}

std::string Empty(const messages::Message & /*message*/)
{
    return "no fields";
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_messages");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    Log<messages::PerformAbilityMessage>("PerformAbility", [](const messages::PerformAbilityMessage &m) {
        return fmt::format("ability {} by {}, item 0x{:x}", m.ability, Named(m.caster), m.item);
    });
    Log<messages::PerformAbilityOnTargetUnderCursorMessage>("PerformAbilityOnTargetUnderCursor", Empty);
    Log<messages::PerformAbilityOnSelectedTargetMessage>("PerformAbilityOnSelectedTarget", Empty);
    Log<messages::PerformAbilityOnPlayerMessage>(
        "PerformAbilityOnPlayer",
        [](const messages::PerformAbilityOnPlayerMessage &m) { return fmt::format("target {}", Named(m.target)); });
    Log<messages::CancelAbilityTargetingMessage>("CancelAbilityTargeting", Empty);
    Log<messages::ActivateLinearTargetModeMessage>("ActivateLinearTargetMode", Empty);
    Log<messages::ActivateAnalogTargetModeMessage>("ActivateAnalogTargetMode", Empty);
    Log<messages::SetQuickslotMessage>("SetQuickslot", [](const messages::SetQuickslotMessage &m) {
        return fmt::format(R"({} slot {} ability {} tag "{}" template "{}")", Named(m.creature), m.slot, m.ability,
                           dao::Utf8(m.tag.View()), dao::Utf8(m.refTemplate.View()));
    });
    Log<messages::ShowAbilityHeaderMessage>("ShowAbilityHeader", [](const messages::ShowAbilityHeaderMessage &m) {
        return fmt::format("header {} list {}", m.header, m.list);
    });
    Log<messages::HideAbilityHeaderMessage>("HideAbilityHeader", [](const messages::HideAbilityHeaderMessage &m) {
        return fmt::format("header {} list {}", m.header, m.list);
    });
    Log<messages::CharGenAssignAbilityMessage>(
        "CharGenAssignAbility", [](const messages::CharGenAssignAbilityMessage &m) {
            return fmt::format("{} ability {} kind {}", Named(m.creature), m.ability, m.kind);
        });
    Log<messages::FindAbilityChainsMessage>("FindAbilityChains", [](const messages::FindAbilityChainsMessage &m) {
        return fmt::format("{} type {} mode {}", Named(m.creature), m.type, m.mode);
    });
    Log<messages::CharGenSelectSpecializationMessage>(
        "CharGenSelectSpecialization", [](const messages::CharGenSelectSpecializationMessage &m) {
            return fmt::format("{} specialization {} next {}", Named(m.creature), m.specialization, m.next);
        });
    return true;
}
