// An example plugin: how a plugin is written against daolib, short enough
// to copy from. It is also a test of the library in play, deployed beside
// the probe and changing nothing.
//
// It intercepts the scripts' GetCombatState: each time a script asks
// whether a creature is in combat, it reads the creature from the call's
// arguments, lets the engine answer (Proceed), and compares the answer with
// what dao::CCCreature::CombatState() reads from the creature itself.
// Every hundredth call it asks the engine something through the generated
// API too, the creature's tag. The counts go to example.log.
//
// The probe also takes GetCombatState's slot in the table, so the two
// chain: whichever installed last runs first and Proceeds into the other.
#include "dao/ClientExoApp.h"
#include "dao/Gda.h"
#include "dao/Log.h"
#include "dao/Messages.h"
#include "dao/Plugin.h"
#include "dao/ScriptHooks.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace
{

std::atomic<std::uint32_t> g_calls{0};
std::atomic<std::uint32_t> g_matched{0};

// A cell of the ability table read two ways: straight from the table
// (dao/Gda.h) and through the scripts' GetM2DAInt (dao::script::api).
void CheckTables()
{
    using dao::script::Table;
    constexpr auto kAbilities = static_cast<std::int32_t>(Table::AbilitiesTalents);
    const std::int32_t row = dao::script::api::GetM2DARowIdFromRowIndex(kAbilities, 0);
    const auto direct = dao::gda::GetInt(Table::AbilitiesTalents, row, "TargetType");
    const std::int32_t script = dao::script::api::GetM2DAInt(kAbilities, L"TargetType", row);
    spdlog::info("ability table, row {} TargetType: {} from the table, {} from GetM2DAInt", row,
                 direct ? std::to_string(*direct) : std::string("nothing"), script);
}

int CheckCombatState(dao::script::CommandCall &call)
{
    const auto creature = call.Object(0);
    const int result = call.Proceed();
    const auto answer = call.IntAnswer();
    const std::uint32_t n = ++g_calls;
    if (result != dao::script::kOk || !creature || !answer)
    {
        return result;
    }
    auto *app = dao::CClientExoApp::Get();
    const dao::CCCreature *object = app != nullptr ? app->GetCreatureById(*creature) : nullptr;
    const bool same = object != nullptr && object->CombatState() == *answer;
    const std::uint32_t matched = same ? ++g_matched : g_matched.load();
    if (n <= 3 || n % 100 == 0)
    {
        // A call into the engine from here, on the scripts' thread, in the
        // middle of the script's own call.
        const std::wstring tag = dao::script::api::GetTag(*creature);
        spdlog::info("GetCombatState({:#010x} \"{}\") = {}, the field says {}; {} of {} calls matched", *creature,
                     dao::Utf8(tag), *answer, object != nullptr ? object->CombatState() : -1, matched, n);
        CheckTables();
    }
    return result;
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->expectedExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::script::InterceptCommand(dao::script::id::GetCombatState, &CheckCombatState);
    spdlog::info("intercepting GetCombatState ({}); installed once the engine has its command table",
                 dao::script::id::GetCombatState);
    dao::script::OnCommandsReady([] { spdlog::info("command table ready; GetCombatState intercepted"); });
    return true;
}
