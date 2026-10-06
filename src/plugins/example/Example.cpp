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
#include "dao/Data.h"
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
#include "dao/Resources.h"
#include "dao/ScriptHooks.h"
#include "dao/String.h"
#include "dao/Tlk.h"
#include "dao/TypeName.h"
#include "dao/core/Msvc8.h"
#include "dao/generated/ScriptApi.h"

#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <set>
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

    // The ability's name, from its NameStrRef through the talk tables.
    if (const auto strref = dao::gda::GetInt(Table::AbilitiesTalents, kShieldPummel, "NameStrRef"))
    {
        spdlog::info("Shield Pummel's name ({}): \"{}\"", *strref,
                     dao::Utf8(dao::tlk::String(static_cast<std::uint32_t>(*strref))));
    }

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

    // A file as the game finds it (dao/Resources.h), once: the compiled
    // rules script, whose header is "NCS V1.0", and a name it has not.
    static std::once_flag once;
    std::call_once(once, [] {
        const auto script = dao::resources::Read(L"rules_core.ncs");
        std::string header;
        if (script)
        {
            for (std::size_t i = 0; i < script->size() && i < 8; ++i)
            {
                header.push_back(static_cast<char>((*script)[i]));
            }
        }
        spdlog::info("resources: rules_core.ncs {} ({} bytes, header \"{}\"); no_such_file.ncs exists: {}",
                     script ? "read" : "not read", script ? script->size() : 0, header,
                     dao::resources::Exists(L"no_such_file.ncs"));
    });
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

// GetCombatState checked against the field it reads (CheckCombatState).
void WatchCombatState()
{
    dao::script::InterceptCommand(dao::script::id::GetCombatState, &CheckCombatState);
    spdlog::info("intercepting GetCombatState ({}); installed once the engine has its command table",
                 dao::script::id::GetCombatState);
    dao::script::OnCommandsReady([] { spdlog::info("command table ready; GetCombatState intercepted"); });
}

// Every command a script queues (the AI's choices among them): what it
// is and what it acts on, and a copy of it, made and destroyed through
// the engine, as a plugin that holds a creature's actions back would
// keep one. The command itself goes on.
void WatchQueuedCommands()
{
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
            const char *copyWord = "none";
            if (copy)
            {
                copyWord = copyAgrees ? "agrees" : "DIFFERS";
            }
            // Not Static(): AddCommand stores its bStatic in the copy it
            // pops, after this runs.
            spdlog::info("AddCommand for {:#010x}: type {}, target {}, position {}, ability {}, wait {} ms, "
                         "player issued {}; copy {}",
                         creature.value_or(0), static_cast<int>(command->Type()),
                         target ? fmt::format("{:#010x}", *target) : std::string("-"),
                         position ? fmt::format("({:.2f}, {:.2f}, {:.2f})", position->x, position->y, position->z)
                                  : std::string("-"),
                         show(command->Ability()), show(command->WaitMilliseconds()), command->PlayerIssued(),
                         copyWord);
        }
        return call.Proceed();
    });
}

// Script events as they are delivered: the first few of the kinds a
// plugin most often wants, with whom each is for.
void WatchScriptEvents()
{
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
}

// The Flash UI's calls into the game: the first few of each kind, with
// the screen that made them, the thread they came on, and the object
// that routes them.
void WatchGuiCalls()
{
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
}

// The engine's messages as it delivers them: each class the first time
// it comes, with its receiver, and every mode change.
void WatchMessages()
{
    dao::messages::OnAnyMessage([](void *receiver, const dao::messages::Message &message) {
        static std::mutex seen_lock;
        static std::set<const void *> seen;
        const std::lock_guard lock(seen_lock);
        if (seen.size() < 120 && seen.insert(message.vtable).second)
        {
            spdlog::info("message {} to {}", dao::TypeName(&message),
                         receiver != nullptr ? dao::TypeName(receiver) : std::string("nobody"));
        }
    });
    dao::messages::OnMessage<dao::messages::SaveGameMessage>([](void *, const dao::messages::SaveGameMessage &m) {
        const auto &name = *reinterpret_cast<const dao::core::WStringRep *>(m.name);
        spdlog::info("SaveGameMessage: slot {}, kind {}, name \"{}\"", m.slot, m.kind, dao::Utf8(name.View()));
    });
    dao::messages::OnMessage<dao::messages::LoadGameMessage>([](void *, const dao::messages::LoadGameMessage &m) {
        spdlog::info("LoadGameMessage: slot {}, kind {}", m.slot, m.kind);
    });
    dao::messages::OnMessage<dao::messages::SetGameModeMessage>([](void *, const dao::messages::SetGameModeMessage &m) {
        spdlog::info("SetGameModeMessage: mode {}", m.mode);
    });
}

// What is under the cursor as a click arrives, before the mode acts on
// it: the object a primary click would act on, or the ground a move
// would go to, and where the move before this one went.
void WatchClicks()
{
    const auto cursor = [](const char *click) {
        auto *app = dao::CClientExoApp::Get();
        auto *modes = app != nullptr ? app->GameModes() : nullptr;
        const auto *explore = modes != nullptr ? modes->Explore() : nullptr;
        if (explore == nullptr)
        {
            spdlog::info("{}: not in explore or combat mode", click);
            return;
        }
        const auto ground = explore->CursorGround();
        const auto last = explore->LastMoveDestination();
        const auto object = explore->CursorObject();
        spdlog::info("{}: {} hits under the cursor; object {}, ground {} (walkable {}); the move before went to "
                     "({:.2f}, {:.2f}, {:.2f})",
                     click, explore->CursorHits().size(),
                     object ? fmt::format("{:#010x} \"{}\"", *object, dao::Utf8(dao::script::api::GetTag(*object)))
                            : std::string("none"),
                     ground ? fmt::format("({:.2f}, {:.2f}, {:.2f})", ground->x, ground->y, ground->z)
                            : std::string("none"),
                     explore->CursorGroundWalkable(), last.x, last.y, last.z);
    };
    dao::messages::OnMessage<dao::messages::MoveCharacterToLocationMessage>(
        [cursor](void *, const dao::messages::MoveCharacterToLocationMessage &) { cursor("move click"); });
    dao::messages::OnMessage<dao::messages::PerformPrimaryActionOnTargetUnderCursorMessage>(
        [cursor](void *, const dao::messages::PerformPrimaryActionOnTargetUnderCursorMessage &) {
            cursor("primary click");
        });
}

// The camera as the player zooms and turns it (dao/Camera.h), to learn
// which of its numbers are the view, the distance, the yaw and the
// pitch: a line whenever one of them changes, checked every 15 frames.
void WatchCamera()
{
    dao::OnFrame([] {
        static std::uint32_t frame = 0;
        static std::string last;
        if (++frame % 15 != 0)
        {
            return;
        }
        auto *app = dao::CClientExoApp::Get();
        auto *modes = app != nullptr ? app->GameModes() : nullptr;
        auto *camera = modes != nullptr ? modes->Camera() : nullptr;
        if (camera == nullptr)
        {
            return;
        }
        const auto *scene = camera->Scene();
        if (scene == nullptr)
        {
            return;
        }
        const auto offset = camera->FocusOffset();
        const std::string now = fmt::format(
            "target {:#010x}, {} view, zoom {:.2f}; yaw {:.2f}, pitch {:.2f}, scene zoom {:.2f}; focus offset "
            "({:.1f}, {:.1f}, {:.1f})",
            camera->Target(), camera->Tactical() ? "tactical" : "close", camera->Zoom(), scene->Yaw(), scene->Pitch(),
            scene->Zoom(), offset.x, offset.y, offset.z);
        if (now != last)
        {
            last = now;
            const auto focus = camera->Focus();
            const auto at = scene->Position();
            spdlog::debug("camera: {}; focus ({:.1f}, {:.1f}, {:.1f}), camera at ({:.1f}, {:.1f}, {:.1f})", now,
                          focus.x, focus.y, focus.z, at.x, at.y, at.z);
        }
    });
}

// The screens shown, and the full-screen GUI, logged each time they
// change: open the inventory, a store or the map and the log says so.
// Read six times a second, since the walk copies every screen's names.
void WatchScreens()
{
    dao::OnFrame([] {
        static std::uint32_t frames = 0;
        static std::string last;
        if (++frames % 10 != 0)
        {
            return;
        }
        std::string now;
        for (const dao::gui::Screen &screen : dao::gui::ShownScreens())
        {
            now += fmt::format("{} ({:#x}, layer {}), ", dao::Utf8(screen.name), screen.id, screen.layer);
        }
        const auto full = dao::gui::CurrentFullScreen();
        now += fmt::format("full-screen {}", full ? dao::Utf8(full->name) : std::string("none"));
        if (now != last)
        {
            spdlog::info("screens shown: {}", now);
            last = std::move(now);
        }
    });
}

// The game's movies by name, once, while exploring.
void LogMovies()
{
    std::string names;
    const auto owners = dao::gui::Owners();
    for (void *owner : owners)
    {
        names += fmt::format("{} ({}), ", dao::Utf8(dao::gui::NameOf(owner)),
                             dao::gui::MovieOf(owner) != nullptr ? "loaded" : "no movie");
    }
    spdlog::info("{} movies: {}", owners.size(), names);
    // The resource lookup a plugin's movie will go through, on the
    // game's own movies' names and on one that is not there.
    if (!owners.empty())
    {
        const std::wstring first(dao::gui::NameOf(owners.front()));
        spdlog::info("resources: {}.gfx {}, {}.swf {}, daolib_example.gfx {}", dao::Utf8(first),
                     dao::gui::HasMovie(first + L".gfx"), dao::Utf8(first), dao::gui::HasMovie(first + L".swf"),
                     dao::gui::HasMovie(L"daolib_example.gfx"));
    }
}

// A screen of the plugin's own, through the game's own registration
// and opening. Its movie is not in the game, so nothing appears:
// this checks the way in, and the probe logs the OpenMovie it makes.
void CheckOwnScreen()
{
    const bool added = dao::gui::AddScreen(L"DAOLibExample", L"daolib_example", dao::gui::kLayerHud);
    const bool shown = dao::gui::ShowScreen(L"DAOLibExample");
    const bool closed = dao::gui::CloseScreen(L"DAOLibExample");
    spdlog::info("screen DAOLibExample: added {}, shown {}, closed {}; its record now {}", added, shown, closed,
                 dao::gui::FindOwner(L"daolib_example"));
}

// The portrait of the creature the player controls, rendered as the
// party bar's are: the URL a movie would load.
void LogControlled(dao::CClientExoApp &app, dao::ObjectId controlled)
{
    if (auto *creature = app.GetCreatureById(controlled))
    {
        spdlog::info("portrait of {:#010x}: \"{}\"", controlled, dao::Utf8(creature->RenderPortrait()));
        const auto show = [](std::optional<float> v) { return v ? fmt::format("{}", *v) : std::string("-"); };
        spdlog::info(
            "its health {} of {}, mana or stamina {} of {}, level {}, strength {} (base {}), against "
            "GetCreatureProperty's {} and {}",
            show(creature->Health()), show(creature->MaxHealth()), show(creature->ManaStamina()),
            show(creature->MaxManaStamina()), show(creature->Level()),
            show(creature->Property(dao::script::PropertyAttribute::Strength)),
            show(creature->Property(dao::script::PropertyAttribute::Strength, dao::script::PropertyValue::Base)),
            dao::script::api::GetCreatureProperty(controlled, dao::script::constants::PROPERTY_DEPLETABLE_HEALTH,
                                                  dao::script::constants::PROPERTY_VALUE_CURRENT),
            dao::script::api::GetCreatureProperty(controlled, dao::script::constants::PROPERTY_DEPLETABLE_HEALTH,
                                                  dao::script::constants::PROPERTY_VALUE_TOTAL));
    }
}

// A few of the data tree's values, as the UI's GetValue and
// GetNumber give them.
void LogDataValues()
{
    for (const wchar_t *path : {L"GUI.SelectedChar", L"Map.ShowAllLabels", L"CClientExoApp.GUIDialogResult"})
    {
        const dao::data::Attribute *attribute = dao::data::Find(path);
        spdlog::info("data {} ({}): {}", dao::Utf8(path),
                     attribute != nullptr ? static_cast<int>(attribute->Kind()) : -1,
                     attribute != nullptr ? fmt::format("\"{}\", {}", dao::Utf8(attribute->Text()), attribute->Number())
                                          : std::string("none"));
    }
}

// Sending a message as the engine does: once the player has explored
// for five seconds or so in all (300 frames), centre the camera on the creature
// the player controls, and log whether the mode handled it. In combat
// the same message goes to GameModeCombat, which inherits explore's
// handlers. Then, once, the checks that want a game running.
void CheckOnceExploring()
{
    dao::OnFrame([] {
        static std::uint32_t exploring = 0;
        static bool sent = false;
        auto *app = dao::CClientExoApp::Get();
        auto *modes = app != nullptr ? app->GameModes() : nullptr;
        if (sent || modes == nullptr)
        {
            return;
        }
        // Explore frames in total, a menu opened between them or not.
        exploring += modes->Mode() == dao::GameMode::Explore ? 1 : 0;
        if (exploring < 300)
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

        LogMovies();
        CheckOwnScreen();
        LogControlled(*app, message.object);
        LogDataValues();
    });
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    WatchCombatState();
    WatchQueuedCommands();
    WatchScriptEvents();
    WatchGuiCalls();
    WatchMessages();
    WatchClicks();
    WatchCamera();
    WatchScreens();
    CheckOnceExploring();
    return true;
}
