// Example: the world map's pins (dao/WorldMap.h).
//
// Each time the world map opens, its pins are logged with their states, the party's own marked,
// and a travel to each pin that is not active, which the map refuses as a click would, with its
// refusal. What it sees goes to example_worldmap.log.
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

void Log()
{
    const std::wstring here = worldmap::Here().value_or(std::wstring());
    for (const worldmap::Pin &pin : worldmap::Pins())
    {
        spdlog::info("pin {}: {} ({}), {}{}", pin.index, dao::Utf8(pin.name), dao::Utf8(pin.areaTag), Named(pin.state),
                     pin.areaTag == here ? ", the party's" : "");
        if (pin.state != worldmap::PinState::Active)
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
        if (shown && name == L"WorldMap")
        {
            Log();
        }
    });
    return true;
}
