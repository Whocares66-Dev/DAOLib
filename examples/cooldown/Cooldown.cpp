// Example: an ability's cooldown, as the quickbar draws it (dao/Abilities.h).
//
// When the party's controlled creature finishes using an ability, the log
// has the cooldown the game gave it: the seconds left and the seconds it
// was set to, whose ratio is the sweep on the ability's quickbar slot. The
// plugin then sets the cooldown to a third of an hour of which two thirds
// are left, so the slot should show a sweep two thirds full that barely
// moves: what a turn-based mode would show for "two of three turns to go".
// Once a second it logs the cooldown again, so the log has how it ticks.
//
// What it sees goes to example_cooldown.log.
#include "dao/Abilities.h"
#include "dao/CommandEvents.h"
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace
{

constexpr float kTotal = 1200.0F; // seconds: long enough that a fight does not run it down
constexpr float kLeft = 800.0F;   // two thirds of it

struct Watched
{
    dao::ObjectId creature = dao::kInvalidObject;
    std::int32_t ability = 0;
};
std::optional<Watched> g_watched;

std::string AbilityName(std::int32_t ability)
{
    return dao::Utf8(dao::abilities::Name(ability).value_or(L"?"));
}

void Log(const Watched &watched, const char *when)
{
    const auto cooldown = dao::abilities::GetCooldown(watched.creature, watched.ability);
    if (cooldown)
    {
        spdlog::info("{}: {} ({}) of 0x{:x} has {:.1f} s left of {:.1f} s, the sweep at {:.2f}", when,
                     AbilityName(watched.ability), watched.ability, watched.creature, cooldown->remaining,
                     cooldown->total, cooldown->Fraction());
    }
    else
    {
        spdlog::info("{}: {} ({}) of 0x{:x} has no cooldown record", when, AbilityName(watched.ability),
                     watched.ability, watched.creature);
    }
}

void Frame()
{
    using Clock = std::chrono::steady_clock;
    static Clock::time_point last = Clock::now();
    if (!g_watched || Clock::now() - last < std::chrono::seconds(1))
    {
        return;
    }
    last = Clock::now();
    Log(*g_watched, "a second on");
    const auto cooldown = dao::abilities::GetCooldown(g_watched->creature, g_watched->ability);
    if (!cooldown || cooldown->remaining <= 0)
    {
        g_watched.reset();
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_cooldown");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::commands::OnEnd([](dao::ObjectId who, const dao::Command &command, std::int32_t result) {
        const auto ability = command.Ability();
        if (!ability || who != dao::script::api::GetMainControlled() || result != dao::commands::kSuccess)
        {
            return;
        }
        const Watched watched{who, *ability};
        Log(watched, "as the game set it");
        const bool set = dao::abilities::SetCooldown(who, *ability, kLeft, kTotal);
        spdlog::info("set to {:.0f} s left of {:.0f} s: {}", kLeft, kTotal, set ? "taken" : "not taken");
        Log(watched, "after setting");
        g_watched = watched;
    });
    // Every cooldown the scripts set on the controlled creature, as they
    // set it: the moment a cast begins, not when its command ends.
    dao::abilities::OnCooldownSet([](const dao::abilities::CooldownSet &set) {
        if (set.creature == dao::script::api::GetMainControlled())
        {
            spdlog::info("the scripts set ability {}{}'s cooldown to {:.2f} s: the record reads {:.2f} of {:.2f}{}",
                         set.ability, set.itemTag.empty() ? std::string() : " (" + dao::Utf8(set.itemTag) + ")",
                         set.seconds, set.cooldown.remaining, set.cooldown.total, set.took ? "" : ", not taken");
        }
    });
    dao::OnFrame(&Frame);
    return true;
}
