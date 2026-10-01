// Example: commands starting and ending (dao/CommandEvents.h).
//
// Logs each command the party's controlled creature starts and ends, with
// its type and how it ended, and every command anyone ends in failure: the
// events a turn-based mode would wait on instead of polling whether a
// creature is still busy. What it sees goes to example_commands.log.
#include "dao/CommandEvents.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/core/CommandWord.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

namespace
{

std::string_view TypeName(const dao::Command &command)
{
    const std::string_view name = dao::core::Name(command.Type());
    return name.empty() ? std::string_view("?") : name;
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_commands");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->expectedExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::commands::OnStart([](dao::ObjectId who, const dao::Command &command) {
        if (who == dao::script::api::GetMainControlled())
        {
            spdlog::info("0x{:x} starts {}{}", who, TypeName(command), command.PlayerIssued() ? " (player)" : "");
        }
    });
    dao::commands::OnEnd([](dao::ObjectId who, const dao::Command &command, std::int32_t result) {
        if (who == dao::script::api::GetMainControlled() || result != dao::commands::kSuccess)
        {
            spdlog::info("0x{:x} ends {}: {} ({})", who, TypeName(command), dao::commands::ResultName(result), result);
        }
    });
    return true;
}
