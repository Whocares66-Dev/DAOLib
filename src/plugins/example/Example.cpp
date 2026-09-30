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
#include "dao/Events.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Gda.h"
#include "dao/Gui.h"
#include "dao/Log.h"
#include "dao/Messages.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/Plugin.h"
#include "dao/ScriptHooks.h"
#include "dao/String.h"
#include "dao/TypeName.h"
#include "dao/generated/ScriptApi.h"

#include <Windows.h>

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
    constexpr std::int32_t kShieldPummel = 1; // abi_base's row 1 (tools/gda.py --dump abi_base --names)
    const auto type = dao::gda::GetInt(Table::AbilitiesTalents, kShieldPummel, "AbilityType");
    const std::int32_t typeScript = dao::script::api::GetM2DAInt(kAbilities, L"AbilityType", kShieldPummel);
    const auto cost = dao::gda::GetFloat(Table::AbilitiesTalents, kShieldPummel, "Cost");
    const float costScript = dao::script::api::GetM2DAFloat(kAbilities, L"Cost", kShieldPummel);
    spdlog::info("Shield Pummel: AbilityType {} from the table, {} from GetM2DAInt; Cost {} from the table, {} from "
                 "GetM2DAFloat",
                 type ? std::to_string(*type) : std::string("nothing"), typeScript,
                 cost ? std::to_string(*cost) : std::string("nothing"), costScript);

    // A string column, and the table's size, the same way against the
    // scripts' own commands.
    const auto label = dao::gda::GetString(Table::AbilitiesTalents, kShieldPummel, "Label");
    const std::wstring labelScript = dao::script::api::GetM2DAString(kAbilities, L"Label", kShieldPummel);
    const auto rows = dao::gda::Rows(Table::AbilitiesTalents);
    const auto columns = dao::gda::Columns(Table::AbilitiesTalents);
    spdlog::info("abilities: Label \"{}\" from the table, \"{}\" from GetM2DAString; {} rows and {} columns from the "
                 "table, {} and {} from GetM2DARows and GetM2DAColumns",
                 label ? dao::Utf8(*label) : std::string("nothing"), dao::Utf8(labelScript),
                 rows ? std::to_string(*rows) : std::string("nothing"),
                 columns ? std::to_string(*columns) : std::string("nothing"), dao::script::api::GetM2DARows(kAbilities),
                 dao::script::api::GetM2DAColumns(kAbilities));
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

    // Every command a script queues (the AI's choices among them): what it
    // is and what it acts on, and a copy of it, made and destroyed through
    // the engine, as a plugin that holds a creature's actions back would
    // keep one. The command itself goes on.
    dao::script::InterceptCommand(dao::script::id::AddCommand, [](dao::script::CommandCall &call) {
        static std::atomic<std::uint32_t> seen{0};
        const auto creature = call.Object(0);
        const dao::Command *command = call.Command(1);
        if (command != nullptr && seen < 40)
        {
            const dao::script::OwnedCommand copy = dao::script::Copy(*command);
            ++seen;
            const auto show = [](const auto &value) { return value ? fmt::format("{}", *value) : std::string("-"); };
            const auto target = command->Target();
            const auto position = command->Position();
            const bool copyAgrees = copy && copy->Type() == command->Type() && copy->Target() == target &&
                                    copy->Ability() == command->Ability();
            // Not Static(): AddCommand stores its bStatic in the copy it
            // pops, after this runs.
            spdlog::info("AddCommand for {:#010x}: type {}, target {}, position {}, ability {}, wait {} ms, "
                         "player issued {}; copy {}",
                         creature.value_or(0), static_cast<int>(command->Type()),
                         target ? fmt::format("{:#010x}", *target) : std::string("-"),
                         position ? fmt::format("({:.2f}, {:.2f}, {:.2f})", position->x, position->y, position->z)
                                  : std::string("-"),
                         show(command->Ability()), show(command->WaitMilliseconds()), command->PlayerIssued(),
                         copy ? (copyAgrees ? "agrees" : "DIFFERS") : "none");
        }
        return call.Proceed();
    });
    spdlog::info("intercepting GetCombatState ({}); installed once the engine has its command table",
                 dao::script::id::GetCombatState);
    dao::script::OnCommandsReady([] { spdlog::info("command table ready; GetCombatState intercepted"); });

    // Script events as they are delivered: the first few of the kinds a
    // plugin most often wants, with whom each is for.
    using dao::script::EventType;
    for (const EventType type :
         {EventType::ModuleLoad, EventType::GamemodeChange, EventType::Damaged, EventType::PerceptionAppear})
    {
        dao::events::OnScriptEvent(type, [](dao::ObjectId target, const dao::CScriptEvent &event) {
            static std::atomic<std::uint32_t> seen{0};
            if (seen++ < 12)
            {
                spdlog::info("event {} for {:#010x} \"{}\", from {:#010x}", fmt::underlying(event.Type()), target,
                             dao::Utf8(dao::script::api::GetTag(target)), event.Creator());
            }
        });
    }

    // The Flash UI's calls into the game: the first few of each kind, with
    // the screen that made them, the thread they came on, and the object
    // that routes them.
    dao::gui::OnAnyFSCommand([](void *owner, std::string_view name, std::string_view args) {
        static std::atomic<std::uint32_t> seen{0};
        if (seen++ < 40)
        {
            void *const *manager = dao::At<void *>(dao::addr::kGuiManager);
            spdlog::info("FSCommand {}({}) from movie \"{}\" at {}, thread {}, manager {}", name, args,
                         dao::Utf8(dao::gui::NameOf(owner)), owner, GetCurrentThreadId(),
                         *manager != nullptr ? dao::TypeName(*manager) : std::string("none"));
        }
    });
    dao::gui::OnAnyExternalInterface([](const dao::gui::Call &call) {
        static std::atomic<std::uint32_t> seen{0};
        if (seen++ < 40)
        {
            std::string args;
            for (const auto &arg : call.args)
            {
                if (const auto n = arg.AsNumber())
                {
                    args += fmt::format("{} ", *n);
                }
                else if (const auto text = arg.AsString())
                {
                    args += fmt::format("\"{}\" ", *text);
                }
                else
                {
                    args += fmt::format("<type {:#x}> ", arg.type);
                }
            }
            spdlog::info("ExternalInterface {}({}) from movie \"{}\" at {}, thread {}", call.name, args,
                         dao::Utf8(dao::gui::NameOf(call.owner)), call.owner, GetCurrentThreadId());
            // The calling movie, read and left as it is: its root is there,
            // and the FSCommand answer array's size reads.
            if (seen == 1)
            {
                const dao::gui::Movie movie{dao::gui::MovieOf(call.owner)};
                spdlog::info("its movie at {}: _root {}, _root.fsc_retval has {} entries", movie.View(),
                             movie.IsAvailable("_root") ? "available" : "not available",
                             movie.ArraySize("_root.fsc_retval"));
            }
        }
    });

    // Sending a message as the engine does: once the player has explored
    // for ten seconds or so (600 frames), centre the camera on the creature
    // the player controls, and log whether the mode handled it. In combat
    // the same message goes to GameModeCombat, which inherits explore's
    // handlers.
    dao::OnFrame([] {
        static std::uint32_t exploring = 0;
        static bool sent = false;
        auto *app = dao::CClientExoApp::Get();
        auto *modes = app != nullptr ? app->GameModes() : nullptr;
        if (sent || modes == nullptr)
        {
            return;
        }
        exploring = modes->Mode() == dao::GameMode::Explore ? exploring + 1 : 0;
        if (exploring < 600)
        {
            return;
        }
        sent = true;
        auto message = dao::messages::Make<dao::messages::CenterOnObjectMessage>();
        message.object = dao::script::api::GetMainControlled();
        void *explore = modes->CurrentModeObject();
        const bool handled = dao::messages::Send(explore, message);
        spdlog::info("sent CenterOnObjectMessage for {:#010x} to {} at {}: {}", message.object,
                     explore != nullptr ? dao::TypeName(explore) : std::string("no mode"), explore,
                     handled ? "handled" : "not handled");

        // The game's movies by name, once, while exploring.
        std::string names;
        const auto owners = dao::gui::Owners();
        for (void *owner : owners)
        {
            names += fmt::format("{} ({}), ", dao::Utf8(dao::gui::NameOf(owner)),
                                 dao::gui::MovieOf(owner) != nullptr ? "loaded" : "no movie");
        }
        spdlog::info("{} movies: {}", owners.size(), names);
    });
    return true;
}
