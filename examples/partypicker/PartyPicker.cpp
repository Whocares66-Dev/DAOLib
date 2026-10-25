// Example: the party picker's choices (dao/PartyPicker.h).
//
// Each time the party picker is up, the creatures it shows are logged with their states and
// places; what it sees goes to example_partypicker.log.
#include "dao/PartyPicker.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/spdlog.h>

namespace
{

namespace api = dao::script::api;
namespace partypicker = dao::partypicker;

bool s_logged = false;

const char *Named(partypicker::State state)
{
    switch (state)
    {
    case partypicker::State::Chosen:
        return "chosen";
    case partypicker::State::Available:
        return "available";
    case partypicker::State::Unavailable:
        return "unavailable";
    case partypicker::State::Suspended:
        return "suspended";
    case partypicker::State::LockedActive:
        return "kept in the party";
    }
    return "unknown";
}

// The picker's entries are there once it is done showing, a few frames after the mode enters.
void LogOnce()
{
    if (s_logged)
    {
        return;
    }
    const auto entries = partypicker::Entries();
    if (entries.empty())
    {
        return;
    }
    s_logged = true;
    for (const partypicker::Entry &entry : entries)
    {
        spdlog::info("{} 0x{:x}: {}{}, slot {}", dao::Utf8(api::GetName(entry.creature)), entry.creature,
                     Named(entry.state), entry.hero ? ", the hero" : "", entry.slot);
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_partypicker");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::modes::OnModeChanged([](dao::GameMode, dao::GameMode now) {
        if (now == dao::GameMode::PartyPicker)
        {
            s_logged = false;
        }
    });
    dao::OnFrame(&LogOnce);
    return true;
}
