// Example: the world map's and the Fade's pins, and a city's maps swapped (dao/WorldMap.h).
//
// Each time the world map or the Fade's opens, the map shown is logged, then its pins with their
// states, the party's own marked, and a travel to each pin the map's click would refuse, with its
// refusal. A city's map is swapped for its second once, and the pins logged again. What it sees
// goes to example_worldmap.log.
#include "dao/WorldMap.h"
#include "dao/Gui.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <string>

namespace
{

namespace worldmap = dao::worldmap;

const char *Named(worldmap::PinState state)
{
    switch (state)
    {
    case worldmap::PinState::Inactive:
        return "inactive";
    case worldmap::PinState::Active:
        return "active";
    case worldmap::PinState::GrayedOut:
        return "grayed out";
    case worldmap::PinState::Unlocked:
        return "unlocked";
    case worldmap::PinState::Complete:
        return "complete";
    case worldmap::PinState::Destroyed:
        return "destroyed";
    }
    return "unknown";
}

bool IsTravelled(worldmap::MapKind kind, worldmap::PinState state)
{
    return state == worldmap::PinState::Active ||
           (kind == worldmap::MapKind::Fade && state == worldmap::PinState::Complete);
}

void Log()
{
    const worldmap::Shown shown = worldmap::ShownMap();
    spdlog::info("{} map shown, type {}, other {}", shown.kind == worldmap::MapKind::Fade ? "the Fade's" : "a world",
                 shown.type, shown.other);
    const std::wstring here = worldmap::Here().value_or(std::wstring());
    for (const worldmap::Pin &pin : worldmap::Pins())
    {
        spdlog::info("pin {}: {} ({}), {}{}", pin.index, dao::Utf8(pin.name), dao::Utf8(pin.areaTag), Named(pin.state),
                     pin.areaTag == here ? ", the party's" : "");
        if (!IsTravelled(shown.kind, pin.state))
        {
            spdlog::info("  travel refused: {}", static_cast<int>(worldmap::Travel(pin.index)));
        }
    }
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_worldmap");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::gui::OnFullScreen([](std::wstring_view name, bool shown) {
        if (!shown || (name != L"WorldMap" && name != L"FadeMap"))
        {
            return;
        }
        Log();
        if (worldmap::ShownMap().other != 0)
        {
            spdlog::info("swap: {}", static_cast<int>(worldmap::SwapMaps()));
            Log();
            spdlog::info("swap back: {}", static_cast<int>(worldmap::SwapMaps()));
        }
    });
    return true;
}
