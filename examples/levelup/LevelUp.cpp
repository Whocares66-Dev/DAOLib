// Example: a level-up with no screen (dao/LevelUp.h).
//
// Each time the game takes the player's input again, the controlled creature's points to spend
// are logged, and what the level-up screen would offer it of each kind, learnable first. What it
// sees goes to example_levelup.log.
#include "dao/LevelUp.h"
#include "dao/GameModes.h"
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
namespace levelup = dao::levelup;

std::string Listed(dao::ObjectId creature, levelup::Kind kind)
{
    std::string learnable;
    std::size_t have = 0;
    std::size_t locked = 0;
    for (const levelup::Offer &offer : levelup::Offers(creature, kind))
    {
        if (offer.state == levelup::State::Learnable)
        {
            learnable += fmt::format(" {}", offer.ability);
        }
        else
        {
            ++(offer.state == levelup::State::Have ? have : locked);
        }
    }
    return fmt::format("{} had, {} locked, learnable:{}", have, locked, learnable.empty() ? " none" : learnable);
}

void Log()
{
    const dao::ObjectId who = api::GetMainControlled();
    const auto points = levelup::PointsOf(who);
    if (!points)
    {
        return;
    }
    spdlog::info("{} 0x{:x}: {} attribute, {} skill, {} talent or spell, {} specialization points",
                 dao::Utf8(api::GetName(who)), who, points->attributes, points->skills, points->talentsOrSpells,
                 points->specializations);
    spdlog::info("  talents: {}", Listed(who, levelup::Kind::Talents));
    spdlog::info("  spells: {}", Listed(who, levelup::Kind::Spells));
    spdlog::info("  skills: {}", Listed(who, levelup::Kind::Skills));
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_levelup");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::modes::OnInputBack(&Log);
    return true;
}
