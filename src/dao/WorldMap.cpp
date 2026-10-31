// The world map's and the Fade's pins and travel, and a city's maps swapped (dao/WorldMap.h).
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
//
// Notes on the Fade's map and a city's second map: read 2026-10-30 (DAOLib #276) in fademap.gfx,
// worldmap.gfx and the GOG exe, for Eluvian's travel. The Broken Circle's pedestal script
// (cir000ip_fade_pedistal) makes fade_map the primary map (CIR_MapGUI_ActivateFadeMap) and calls
// OpenFadeMap, which sends SetFullScreenGUIMessage "FadeMap" (scene 0x1F, ForScript_OpenFadeMap).
// The movie reads its pins from WorldMap.FadeMap.Locations, an attribute of its own beside
// PrimaryMap (+0x68) and SecondaryMap (+0x78): the WorldMap's +0x88, registered as "FadeMap" at
// 0x007021D6. Its click (SharedLibrary.FadeMapPinIcon.PinClicked) takes a pin of State 2 or 5,
// tests no GUIStatus, sets no closed-by-travel mark, sets WorldMap.LastClickedPinIndex and sends
// "GameModeController.AreaTransition <WorldMap.FadeMap.PlayerLocation.AreaTag> <index>". The
// handler (0x007180E0 -> 0x00700CA0) finds both locations on the primary map (+0x68), which is the
// Fade's while it is up, and raises the module's EVENT_TYPE_BEGIN_TRAVEL; sp_module_travel takes
// WM_FADE's own path. A city's toggle (worldmap.gfx's ToggleMapButton_mc, disabled while
// WorldMap.SecondaryMap reads 0) sends "WorldMap.WorldMapSwapMaps": its handler (0x00701240 ->
// 0x00700E00) does nothing unless both maps are set, and otherwise calls SetWorldMapPrimary with
// the second and SetWorldMapSecondary with the first. The movie watches WorldMap.PrimaryMap.Type
// and loads the new map's picture and pins when it changes.
//
// Seen in play 2026-10-30 (examples/worldmap), Denerim's market in the main campaign: opened for use,
// the map shown was Denerim's (worldmaps row 2, the other 1, 21 pins); SwapMaps went and at once
// showed the world's (row 1, the other 2, 58 pins), and swapped back. With fade_map made primary and
// OpenFadeMap, the Fade's map was shown (row 5, no other), its ten pins read from WorldMap.FadeMap,
// the party's Inner Sanctum; SwapMaps was refused NotShown, a travel to pin 99 NoPin, and a travel to
// The Raw Fade (pin 1, Complete) closed the map and took the party to cir300ar_fade.
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
constexpr std::wstring_view kPrimary = L"WorldMap.PrimaryMap";
constexpr std::wstring_view kFade = L"WorldMap.FadeMap";

std::wstring TextAt(const std::wstring &path)
{
    return data::Text(path).value_or(std::wstring());
}

bool FadeShown()
{
    return gui::ScreenMovie(L"FadeMap").has_value();
}

// The map Pins reads: the Fade's while it is up, which its movie reads, otherwise the primary.
std::wstring_view MapPath()
{
    return FadeShown() ? kFade : kPrimary;
}

std::wstring PinPath(std::wstring_view map, std::int32_t index)
{
    return std::format(L"{}.Locations.{}", map, index);
}

std::int32_t TypeOf(std::wstring_view map)
{
    return static_cast<std::int32_t>(data::Number(std::format(L"{}.Type", map)).value_or(0));
}

// The world map's click (SharedLibrary.MapPinIcon.PinClicked): only an Active pin, and only on a
// map shown for travel that is not already travelling.
Refusal WorldRefusal(const std::wstring &pin)
{
    if (data::Number(L"WorldMap.GUIStatus").value_or(0) != kForUse ||
        data::Number(L"WorldMap.GUITravelStatus").value_or(0) != 0)
    {
        return Refusal::ReadOnly;
    }
    if (data::Find(pin + L".Tag") == nullptr)
    {
        return Refusal::NoPin;
    }
    const double state = data::Number(pin + L".State").value_or(0);
    return state == static_cast<double>(PinState::Active) ? Refusal::None : Refusal::NotActive;
}

// The Fade's click (SharedLibrary.FadeMapPinIcon.PinClicked): an Active or a Complete pin.
Refusal FadeRefusal(const std::wstring &pin)
{
    if (data::Find(pin + L".Tag") == nullptr)
    {
        return Refusal::NoPin;
    }
    const double state = data::Number(pin + L".State").value_or(0);
    return state == static_cast<double>(PinState::Active) || state == static_cast<double>(PinState::Complete)
               ? Refusal::None
               : Refusal::NotActive;
}

} // namespace

Shown ShownMap()
{
    if (FadeShown())
    {
        return {MapKind::Fade, TypeOf(kPrimary), 0};
    }
    if (gui::ScreenMovie(L"WorldMap"))
    {
        return {MapKind::World, TypeOf(kPrimary), TypeOf(L"WorldMap.SecondaryMap")};
    }
    return {};
}

std::vector<Pin> Pins()
{
    const std::wstring_view map = MapPath();
    std::vector<Pin> pins;
    for (std::int32_t index = 0; index < kMostPins; ++index)
    {
        const std::wstring at = PinPath(map, index);
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
    return data::Text(std::format(L"{}.PlayerLocation.AreaTag", MapPath()));
}

Refusal Travel(std::int32_t index)
{
    const bool fade = FadeShown();
    auto movie = fade ? gui::ScreenMovie(L"FadeMap") : gui::ScreenMovie(L"WorldMap");
    if (!movie)
    {
        return Refusal::NotShown;
    }
    if (index < 0)
    {
        return Refusal::NoPin;
    }
    const std::wstring pin = PinPath(fade ? kFade : kPrimary, index);
    if (const Refusal refusal = fade ? FadeRefusal(pin) : WorldRefusal(pin); refusal != Refusal::None)
    {
        return refusal;
    }
    data::Attribute *clicked = data::Find(L"WorldMap.LastClickedPinIndex");
    const auto here = Here();
    if (clicked == nullptr || !here)
    {
        return Refusal::NotShown;
    }
    if (!fade)
    {
        movie->SetVariable("WorldMap.m_bClosedByTravel", gui::Value::Bool(true));
    }
    clicked->SetText(std::to_wstring(index));
    messages::SendText(std::format(L"GameModeController.AreaTransition {} {}", *here, index));
    return Refusal::None;
}

Refusal SwapMaps()
{
    const Shown shown = ShownMap();
    if (shown.kind != MapKind::World)
    {
        return Refusal::NotShown;
    }
    if (shown.other == 0)
    {
        return Refusal::NoOther;
    }
    messages::SendText(L"WorldMap.WorldMapSwapMaps");
    return Refusal::None;
}

} // namespace dao::worldmap
