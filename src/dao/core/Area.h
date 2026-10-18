// An area as the toolset wrote it, <area>.are, over bytes: its own fields,
// its variables, and what is placed in it, each object with its kind's own
// fields. No template is read: an object names its template, and the
// template's file (.utc, .utp, ...) is dao/core/Gff3.h's to read.
//
//   const auto file = dao::core::gff3::File::Parse(bytes);
//   const auto area = dao::core::area::AreaOf(*file);
//   const auto row = area->AreaId(); // its row of areadata.gda
//
// The file is GFF3 "ARE V3.2" (dao/core/Gff3.h). Read 2026-10-17 over all
// 297 the game takes (DAOLib #160): every one has the same top fields and
// the seven lists here; the item list is empty in all of them. A trigger is
// one of four shapes, by its Type: none, a trigger placed from a template
// (1,738); 3, a reverb region (437); 4, a reverb region with sounds of its
// own (1,772); 5, a music region, setting the music's state as the
// controlled creature enters and leaves it (283). A variable's
// Type is 1 for an int (its Value an Int), 2 for a float, 3 for a string,
// and 12 for a string too (21, in triggers).
#pragma once

#include "dao/core/Gff3.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace dao::core::area
{

using Point = std::array<float, 3>;
using Quaternion = std::array<float, 4>; // x, y, z, w

// One of an area's or an object's variables (its VarTable).
struct Variable
{
    std::string name;
    std::uint8_t type = 0; // 1 int, 2 float, 3 and 12 string
    std::variant<std::int32_t, float, std::string> value;
};

// Where an object is and what it is, the fields every kind has but where it says otherwise.
struct Placed
{
    std::string tag;
    std::string templateResRef; // empty for a waypoint, a sound and a region with no template
    Point at{};
    Quaternion turn{0, 0, 0, 1};
    std::int32_t active = 1; // 0 for an object placed inactive
    std::int32_t importance = 0;
    std::vector<Variable> variables;
};

struct Creature
{
    Placed placed;
    std::int32_t teamId = -1;
    std::int32_t rank = 0;
    std::int32_t treasureCategory = 0;
    float interactRadius = 0;
    bool isSelectable = true;
    std::int32_t trackable = 1;
    bool weaponsDrawn = false;
};

struct Placeable
{
    Placed placed;
    std::int32_t teamId = -1;
    std::int32_t rank = 0;
    std::int32_t pickLock = 0;
    std::int32_t treasureCategory = 0;
    float interactRadius = 0;
    std::int32_t trapDetection = 0;
    std::int32_t trapDisarm = 0;
    bool useable = true;
    std::int32_t roomId1 = -1; // the rooms it stands between, by the layout's numbers; -1 for none
    std::int32_t roomId2 = -1; // a door's other side; roomId1 again for one in a room
    std::int32_t initialState = 0;
};

// A sound placed in an area, or in a trigger of Type 4.
struct Sound
{
    Placed placed;
    std::string sound; // its event's path: global_amb_glo/exterior_air_wind/amb_ext_rumble1_lp
    float volume = 0;  // decibels, 0 or less
    float pitch = 0;
    std::int32_t fadeIn = 0;
    std::int32_t fadeOut = 0;
    float maxDistanceMult = 1;
    float coneInside = 360; // degrees
    float coneOutside = 360;
    float coneVolume = 1;
    std::int32_t priority = 0;
    bool occludable = false;
};

struct Trigger
{
    Placed placed;
    std::optional<std::int32_t> type;     // 3 reverb, 4 reverb and sounds, 5 music; none for one placed from a template
    std::vector<Point> geometry;          // its outline's corners, from its place, not turned
    std::string reverbPreset;             // a reverb region's, types 3 and 4
    std::optional<std::int32_t> priority; // a type 3's; the engine never reads it
    std::vector<Sound> sounds;            // a type 4's
    std::string script;                   // a music region's ScriptResRefID, empty in every one
    std::int32_t scriptDelay = 0;         // a music region's, 0 in every one
    std::int32_t enterState = 0;          // the music's state set as the region is entered
    std::int32_t enterStateDelay = 0;     // milliseconds
    std::int32_t exitState = 0;           // and as it is left
    std::int32_t exitStateDelay = 0;
};

struct Waypoint
{
    Placed placed;
    std::int32_t group = 0;
    bool hasMapNote = false;
    std::uint32_t mapNote = 0xFFFFFFFF; // a string reference
    std::int32_t mapNoteType = 0;
    bool mapNoteEnabled = false;
};

// An area's file, its own fields as written.
struct Area
{
    std::uint32_t name = 0xFFFFFFFF; // a string reference
    std::string tag;
    std::string templateResRef;
    std::string script; // ScriptResRefID: bhm000ar_generic_area
    std::string comments;
    bool noTeleport = false;
    std::string map;
    std::int32_t north = 0;
    std::string layout; // its level's layout, the .arl's name: lak303d
    std::string reverbPreset;
    std::string musicPreset;
    std::vector<Variable> variables;
    std::vector<Creature> creatures;
    std::vector<Placeable> placeables;
    std::vector<Trigger> triggers;
    std::vector<Placed> stores;
    std::vector<Placed> stages;
    std::vector<Waypoint> waypoints;
    std::vector<Sound> sounds;

    // Returns the int variable of a name; nothing if there is none, or it is not an int.
    std::optional<std::int32_t> Int(std::string_view variable) const;
    // Returns the area's row of areadata.gda, its int variable AREA_ID, which the engine reads
    // as it loads the area.
    std::optional<std::int32_t> AreaId() const
    {
        return Int("AREA_ID");
    }
};

// Returns an area's file, or nothing for a file that is not one.
std::optional<Area> AreaOf(const gff3::File &file);

} // namespace dao::core::area
