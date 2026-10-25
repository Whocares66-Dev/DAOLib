// The world map's pins and travel (dao/WorldMap.h).
//
// Notes on the world map: read 2026-10-24 (DAOLib #259) in worldmap.gfx and the GOG exe, after
// Eluvian's travel (src/core/Travel.cpp, turn-based-agent's reading and trials of 2026-10-03,
// Eluvian #24 and #30). The map's movie reads everything from the data tree: the pins are
// WorldMap.PrimaryMap.Locations.<n> with Tag, AreaTag, Name and State (MapLocation +0x88, set by
// SetWorldMapLocationStatus through 0x0066DA00), and the party's pin is
// WorldMap.PrimaryMap.PlayerLocation. The test of a click is the movie's alone
// (SharedLibrary.MapPinIcon.PinClicked): the pin's State is 2 and the map is not read-only, which
// it is unless WorldMap.GUIStatus is 2 (shown for use, not to look at) and from the moment
// WorldMap.GUITravelStatus leaves 0. A click then marks the movie's WorldMap.m_bClosedByTravel,
// plays the pin's sound, sets WorldMap.LastClickedPinIndex and sends
// "GameModeController.AreaTransition <the party's AreaTag> <index>". The engine tests no state:
// the transition's handler (GameModeGUI, 0x007180E0) calls WorldMap::BeginTravelFromAreaTransition
// (0x00700CA0), which raises EVENT_TYPE_BEGIN_TRAVEL (91) on the module, and the travel script
// (sp_module_travel.nss) reads no pin's state either; WorldMapStartTravelling (0x00700990) takes
// the destination from LastClickedPinIndex. So the checks are here, as the movie's. Without the
// mark the map closes saying it was not by travel, which at camp the scripts take for the player
// backing out: the followers stayed behind (Eluvian #30). The click's sound is left out.
//
// Seen in play 2026-10-24 (examples/worldmap), Redcliffe in the main campaign: the map's pins, the party's
// Redcliffe Village; opened by OpenPrimaryWorldMap, GUIStatus 1, every travel refused ReadOnly; with
// SetWorldMapGuiStatus(WM_GUI_STATUS_USE) first, the 42 not active refused NotActive, and a travel
// to the party's own pin raised EVENT_TYPE_BEGIN_TRAVEL on the module, then WORLD_MAP_CLOSED with no
// pretransition, the map closed and the party stayed. A travel from Redcliffe Village to Denerim
// (pin 5) raised BEGIN_TRAVEL, then WORLDMAP_PRETRANSITION and WORLD_MAP_CLOSED, and the party of
// four arrived in the Denerim Market District within a minute. No save was written by it, in the
// instance's own saves or the shared profile, though its AutoSave was 1.
#include "dao/WorldMap.h"

#include "dao/Data.h"
#include "dao/Gui.h"
#include "dao/Messages.h"

#include <format>

namespace dao::worldmap
{

namespace
{

constexpr std::int32_t kMostPins = 256;
constexpr double kForUse = 2; // WorldMap.GUIStatus: shown to travel by

std::wstring TextAt(const std::wstring &path)
{
    return data::Text(path).value_or(std::wstring());
}

std::wstring PinPath(std::int32_t index)
{
    return std::format(L"WorldMap.PrimaryMap.Locations.{}", index);
}

} // namespace

std::vector<Pin> Pins()
{
    std::vector<Pin> pins;
    for (std::int32_t index = 0; index < kMostPins; ++index)
    {
        const std::wstring at = PinPath(index);
        if (data::Find(at + L".Tag") == nullptr)
        {
            break;
        }
        pins.push_back({index, TextAt(at + L".Tag"), TextAt(at + L".AreaTag"), TextAt(at + L".Name"),
                        static_cast<PinState>(data::Number(at + L".State").value_or(0))});
    }
    return pins;
}

std::optional<std::wstring> Here()
{
    return data::Text(L"WorldMap.PrimaryMap.PlayerLocation.AreaTag");
}

Refusal Travel(std::int32_t index)
{
    auto movie = gui::ScreenMovie(L"WorldMap");
    if (!movie)
    {
        return Refusal::NotShown;
    }
    if (data::Number(L"WorldMap.GUIStatus").value_or(0) != kForUse ||
        data::Number(L"WorldMap.GUITravelStatus").value_or(0) != 0)
    {
        return Refusal::ReadOnly;
    }
    const std::wstring at = PinPath(index);
    if (index < 0 || data::Find(at + L".Tag") == nullptr)
    {
        return Refusal::NoPin;
    }
    if (data::Number(at + L".State").value_or(0) != static_cast<double>(PinState::Active))
    {
        return Refusal::NotActive;
    }
    data::Attribute *clicked = data::Find(L"WorldMap.LastClickedPinIndex");
    const auto here = Here();
    if (clicked == nullptr || !here)
    {
        return Refusal::NotShown;
    }
    movie->SetVariable("WorldMap.m_bClosedByTravel", gui::Value::Bool(true));
    clicked->SetText(std::to_wstring(index));
    messages::SendText(std::format(L"GameModeController.AreaTransition {} {}", *here, index));
    return Refusal::None;
}

} // namespace dao::worldmap
