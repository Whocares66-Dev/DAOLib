#include "dao/core/Area.h"

#include <algorithm>
#include <string_view>

namespace dao::core::area
{

namespace
{

std::string Text(const gff3::Struct &in, std::string_view label)
{
    if (auto text = in.String(label))
    {
        return std::move(*text);
    }
    return in.ResRef(label).value_or("");
}

template <class T> T Number(const gff3::Struct &in, std::string_view label, T otherwise)
{
    return in.Get<T>(label).value_or(otherwise);
}

bool Flag(const gff3::Struct &in, std::string_view label, bool otherwise)
{
    if (const auto byte = in.Get<std::uint8_t>(label))
    {
        return *byte != 0;
    }
    if (const auto word = in.Get<std::int32_t>(label))
    {
        return *word != 0;
    }
    return otherwise;
}

std::uint32_t StrRef(const gff3::Struct &in, std::string_view label)
{
    const auto loc = in.Loc(label);
    return loc ? loc->strref : 0xFFFFFFFF;
}

// A variable's Value is a field of the type its Type says: an Int for 1, a
// Float for 2, a String for 3 and 12 (all 297 areas, 2026-10-17). Read by
// the field's own type, so a Type the survey did not see still reads.
std::vector<Variable> Variables(const gff3::Struct &in)
{
    std::vector<Variable> variables;
    for (const auto &entry : in.List("VarTable"))
    {
        Variable variable{.name = Text(entry, "Name"), .type = Number<std::uint8_t>(entry, "Type", 0), .value = 0};
        if (const auto whole = entry.Get<std::int32_t>("Value"))
        {
            variable.value = *whole;
        }
        else if (const auto real = entry.Get<float>("Value"))
        {
            variable.value = *real;
        }
        else if (auto text = entry.String("Value"))
        {
            variable.value = std::move(*text);
        }
        else
        {
            continue;
        }
        variables.push_back(std::move(variable));
    }
    return variables;
}

// Regions of sounds and of scripts have no WOrientation (2026-10-17), nor
// Active or Importance: a missing part is the unturned, active object's.
Placed PlacedOf(const gff3::Struct &in)
{
    return {
        .tag = Text(in, "Tag"),
        .templateResRef = Text(in, "TemplateResRef"),
        .at = {Number(in, "XPosition", 0.0F), Number(in, "YPosition", 0.0F), Number(in, "ZPosition", 0.0F)},
        .turn = {Number(in, "XOrientation", 0.0F), Number(in, "YOrientation", 0.0F), Number(in, "ZOrientation", 0.0F),
                 Number(in, "WOrientation", 1.0F)},
        .active = Number<std::int32_t>(in, "Active", 1),
        .importance = Number<std::int32_t>(in, "Importance", 0),
        .variables = Variables(in),
    };
}

Creature CreatureOf(const gff3::Struct &in)
{
    return {
        .placed = PlacedOf(in),
        .teamId = Number<std::int32_t>(in, "TeamId", -1),
        .rank = Number<std::int32_t>(in, "Rank", 0),
        .treasureCategory = Number<std::int32_t>(in, "TreasureCategory", 0),
        .interactRadius = Number(in, "InteractRadius", 0.0F),
        .isSelectable = Flag(in, "IsSelectable", true),
        .trackable = Number<std::int32_t>(in, "Trackable", 1),
        .weaponsDrawn = Flag(in, "WeaponsDrawn", false),
    };
}

Placeable PlaceableOf(const gff3::Struct &in)
{
    return {
        .placed = PlacedOf(in),
        .teamId = Number<std::int32_t>(in, "TeamId", -1),
        .rank = Number<std::int32_t>(in, "Rank", 0),
        .pickLock = Number<std::int32_t>(in, "PickLock", 0),
        .treasureCategory = Number<std::int32_t>(in, "TreasureCategory", 0),
        .interactRadius = Number(in, "InteractRadius", 0.0F),
        .trapDetection = Number<std::int32_t>(in, "TrapDetection", 0),
        .trapDisarm = Number<std::int32_t>(in, "TrapDisarm", 0),
        .useable = Flag(in, "Useable", true),
        .roomId1 = Number<std::int32_t>(in, "RoomID1", 0),
        .roomId2 = Number<std::int32_t>(in, "RoomID2", 0),
        .initialState = Number<std::int32_t>(in, "InitialState", 0),
    };
}

Sound SoundOf(const gff3::Struct &in)
{
    return {
        .placed = PlacedOf(in),
        .sound = Text(in, "Sound"),
        .volume = Number(in, "Volume", 0.0F),
        .pitch = Number(in, "Pitch", 0.0F),
        .fadeIn = Number<std::int32_t>(in, "FadeIn", 0),
        .fadeOut = Number<std::int32_t>(in, "FadeOut", 0),
        .maxDistanceMult = Number(in, "MaxDistanceMult", 1.0F),
        .coneInside = Number(in, "ConeInside", 360.0F),
        .coneOutside = Number(in, "ConeOutside", 360.0F),
        .coneVolume = Number(in, "ConeVolume", 1.0F),
        .priority = Number<std::int32_t>(in, "Priority", 0),
        .occludable = Flag(in, "Occludable", false),
    };
}

Trigger TriggerOf(const gff3::Struct &in)
{
    Trigger trigger{
        .placed = PlacedOf(in),
        .type = in.Get<std::int32_t>("Type"),
        .geometry = {},
        .reverbPreset = Text(in, "ReverbPreset"),
        .priority = in.Get<std::int32_t>("Priority"),
        .sounds = {},
        .script = Text(in, "ScriptResRefID"),
        .scriptDelay = Number<std::int32_t>(in, "ScriptDelay", 0),
        .enterState = Number<std::int32_t>(in, "EnterState", 0),
        .enterStateDelay = Number<std::int32_t>(in, "EnterStateDelay", 0),
        .exitState = Number<std::int32_t>(in, "ExitState", 0),
        .exitStateDelay = Number<std::int32_t>(in, "ExitStateDelay", 0),
    };
    for (const auto &corner : in.List("Geometry"))
    {
        trigger.geometry.push_back(
            {Number(corner, "XPoint", 0.0F), Number(corner, "YPoint", 0.0F), Number(corner, "ZPoint", 0.0F)});
    }
    for (const auto &sound : in.List("SoundList"))
    {
        trigger.sounds.push_back(SoundOf(sound));
    }
    return trigger;
}

Waypoint WaypointOf(const gff3::Struct &in)
{
    return {
        .placed = PlacedOf(in),
        .group = Number<std::int32_t>(in, "Group", 0),
        .hasMapNote = Flag(in, "HasMapNote", false),
        .mapNote = StrRef(in, "MapNote"),
        .mapNoteType = Number<std::int32_t>(in, "MapNoteType", 0),
        .mapNoteEnabled = Flag(in, "MapNoteEnabled", false),
    };
}

template <class T, class Read> std::vector<T> ListOf(const gff3::Struct &in, std::string_view label, Read read)
{
    std::vector<T> out;
    for (const auto &entry : in.List(label))
    {
        out.push_back(read(entry));
    }
    return out;
}

} // namespace

std::optional<std::int32_t> Area::Int(std::string_view variable) const
{
    const auto found = std::ranges::find_if(variables, [&](const Variable &v) { return v.name == variable; });
    if (found == variables.end() || !std::holds_alternative<std::int32_t>(found->value))
    {
        return std::nullopt;
    }
    return std::get<std::int32_t>(found->value);
}

std::optional<Area> AreaOf(const gff3::File &file)
{
    const auto root = file.Root();
    if (file.FileType() != "ARE" || !root)
    {
        return std::nullopt;
    }
    return Area{
        .name = StrRef(*root, "Name"),
        .tag = Text(*root, "Tag"),
        .templateResRef = Text(*root, "TemplateResRef"),
        .script = Text(*root, "ScriptResRefID"),
        .comments = Text(*root, "Comments"),
        .noTeleport = Flag(*root, "NoTeleport", false),
        .map = Text(*root, "Map"),
        .north = Number<std::int32_t>(*root, "North", 0),
        .layout = Text(*root, "Layout"),
        .reverbPreset = Text(*root, "ReverbPreset"),
        .musicPreset = Text(*root, "MusicPreset"),
        .variables = Variables(*root),
        .creatures = ListOf<Creature>(*root, "CreatureList", CreatureOf),
        .placeables = ListOf<Placeable>(*root, "PlaceableList", PlaceableOf),
        .triggers = ListOf<Trigger>(*root, "TriggerList", TriggerOf),
        .stores = ListOf<Placed>(*root, "StoreList", PlacedOf),
        .stages = ListOf<Placed>(*root, "StageList", PlacedOf),
        .waypoints = ListOf<Waypoint>(*root, "WaypointList", WaypointOf),
        .sounds = ListOf<Sound>(*root, "SoundList", SoundOf),
    };
}

} // namespace dao::core::area
