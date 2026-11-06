// Example: every creature's walk traced through its move updates (dao/Movement.h).
//
// A line is logged when something in a creature's walk changes: a creature in its way, the
// movement record's wait or re-path, the standing time starting to count, or the move ending. An
// update that only carries the walk on logs nothing.
//
// What it does goes to example_movement.log.
#include "dao/Movement.h"
#include "dao/Log.h"
#include "dao/Plugin.h"

#include <spdlog/spdlog.h>

#include <string>
#include <unordered_map>

namespace
{

// The game thread's alone: move updates run on it.
std::unordered_map<dao::ObjectId, std::int32_t> g_record; // the record's last state, by creature
std::unordered_map<dao::ObjectId, bool> g_standing;       // whether the standing time was counting
dao::movement::Watch g_watch;                             // for the life of the plugin

std::string Ids(const std::vector<dao::ObjectId> &ids)
{
    std::string text;
    for (const dao::ObjectId id : ids)
    {
        text += fmt::format("{}0x{:x}", text.empty() ? "" : " ", id);
    }
    return text;
}

void Updated(const dao::movement::MoveUpdate &u)
{
    const std::int32_t state = u.walker.record ? u.walker.record->state : -1;
    const bool standing = u.standingMs > 0;
    const bool changed = g_record[u.creature] != state || g_standing[u.creature] != standing;
    g_record[u.creature] = state;
    g_standing[u.creature] = standing;
    const bool blocked = u.inTheWay && !u.inTheWay->listed.empty();
    if (!changed && !blocked && u.result == 0)
    {
        return;
    }
    spdlog::info(
        "0x{:x} sub-action {}: answer {}, standing {} ms ({}, AI {}, {} ms), motion {}, record {} ({} ms), path {}, "
        "point {} of {}",
        u.creature, u.subAction, u.result, u.standingMs, u.counting ? "counting" : "not counting", u.aiLevel,
        u.updateMs, u.walker.motion, state, u.walker.record ? u.walker.record->timerMs : 0,
        u.pathAnswer ? std::to_string(*u.pathAnswer) : std::string("not asked"), u.waypoint, u.waypoints);
    if (u.inTheWay)
    {
        const dao::movement::InTheWay &w = *u.inTheWay;
        spdlog::info("    in the way{}: [{}], answer {}, {}", w.combat ? " (combat)" : "", Ids(w.listed), w.answer,
                     w.reacted ? fmt::format("reacted {} -> {} ({} ms)", w.before.state, w.after.state, w.after.timerMs)
                               : std::string("no reaction"));
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_movement");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    g_watch = dao::movement::OnMoveUpdate(&Updated);
    return true;
}
