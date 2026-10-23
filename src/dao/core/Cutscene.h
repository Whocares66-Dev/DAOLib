// A cutscene (.cut) read, over bytes: the script it runs at its end, its
// length, the area and stage it plays in, and its actors, each with its
// queue of actions on the cutscene's clock: animations, spoken lines,
// sounds, camera cuts, stage places, drawn weapons, visibility, links to
// another actor, frame-buffer effects, movies and fog, and the curves that
// move them. A script starts one with CS_LoadCutscene; even one the player
// skips runs its end script. A curve is sampled at a time as the engine
// samples it, and a track's actor placed and turned by it (DAOLib #184).
//
//   const auto cutscene = dao::core::cutscene::CutsceneOf(file);
//   cutscene->endScript; // "gen00cs_cutscene_end"
//   dao::core::cutscene::ValueAt(curve, 12.5F);
//
// The file is GFF4 "CUT V1.0" (dev/gff4_fields.csv, 5000 to 5750, with Tag,
// Name, Text, EnvArea and the AtmoFog fields). Read 2026-10-18 over all
// 3,158 of the game's (DAOLib #170): every file has the same 27 templates,
// with the same fields; 39,803 actors and 246,502 actions, an actor's queue
// a generic list of the action templates: 83,424 TRAK (a track, type 17
// position or 18 orientation, three curves each, or 13, 33, 34, 35), 70,110
// AANI (an animation, type 4), 21,493 ACTN (type 38, 39 or 43, no fields of
// its own), 19,384 ACHT (head tracking), 17,582 AJSP (a stage place), 7,372
// AFBE (a frame-buffer effect, 5,572 of them da_dof), 6,301 AACC (the active
// camera), 4,647 ASHK (a shake), 3,555 ASLN (a spoken line, each with its VO
// bank), 3,526 AJSC (a stage camera), 2,572 ADW_, 2,398 ACVI, 1,973 ACSP (a
// sound), 818 ALNK, 781 AGOR, 285 AANF (a line's face alone), 180 ASMK, 94
// APBM (a movie) and 7 AFOG; ACRU and ACTP are templates no action uses.
// Only TRAK, AANI, ACSP, AFBE and ASHK have curves: 372,345 of them, with
// 320,650 vertices and 226,293 transitions. A stop time of 1e10 is "to the
// end". 279 files have an end script (275 gen00cs_cutscene_end), 2,866 a
// stage, none a henchman tag or henchman actions (a conversation line's cut
// can have both: alistair_main's line 1351); 2,396 run at 100 frames a
// second, 762 at 30. The run times are 1 to 213 seconds. Shakes', gore's
// and head tracking's speed and realignment are left unread.
#pragma once

#include "dao/core/Gff4.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dao::core::cutscene
{

// A point in the world, or in a stage's or an actor's frame.
using Vector3 = std::array<float, 3>;

// An orientation as a quaternion (x, y, z, w).
using Quaternion = std::array<float, 4>;

// A key of a curve: a value at a time on the cutscene's clock.
struct Vertex
{
    float time = 0.0F;
    float value = 0.0F;
};

// How a curve goes from one vertex to the next, as the engine reads a transition's type.
enum class TransitionType : std::uint8_t
{
    Bezier = 0, // a cubic Bezier curve, its two controls absolute (time, value) points
    Linear = 1, // a straight line from one vertex's value to the next's
    Step = 2,   // the first vertex's value until the next vertex
};

// How a curve goes from one vertex to the next.
struct Transition
{
    std::uint8_t type = 0; // a TransitionType as written; the engine reads any other as the curve's base
    Vertex control1;
    Vertex control2;
};

// One value an action moves over time: a coordinate of a track, a parameter of an effect.
struct Curve
{
    float base = 0.0F; // the value with no vertices
    std::vector<Vertex> vertices;
    std::vector<Transition> transitions; // one between each two vertices
};

// A key of an animation's travel: where the clip's root has gone, cumulative from the clip's
// start, at a time into the clip.
struct PositionKey
{
    float time = 0.0F;
    Vector3 position{};
};

struct OrientationKey
{
    float time = 0.0F;
    Quaternion orientation{0.0F, 0.0F, 0.0F, 1.0F};
};

// A frame-buffer effect's parameter, and the curve of its action that moves it.
struct EffectParameter
{
    std::wstring name;
    float value = 0.0F;
    std::int8_t curve = -1;  // the action's curve; -1 for none
    std::int8_t vector = -1; // the element of a vector parameter
};

struct Fog
{
    bool enabled = false;
    std::array<float, 3> color{};
    float intensity = 0.0F;
    float cap = 0.0F;
    float zenith = 0.0F;
};

// One action of an actor's queue. The fields after the curves are those of the action's own
// template, and left empty, 0 or -1 for an action of another.
struct Action
{
    std::string kind;      // its template: "AANI", "ASLN", "TRAK"
    std::uint8_t type = 0; // CutsceneActionType: 4 an animation, 17 a position track
    float start = 0.0F;    // seconds into the cutscene
    float stop = 0.0F;     // 1e10 for the cutscene's end
    std::uint8_t category = 0;
    std::vector<Curve> curves;

    std::wstring animation; // AANI
    std::wstring blendTree; // AANI
    float speed = 0.0F;     // AANI
    float startOffset = 0.0F;
    bool playGad = false;                        // AANI: the clip's travel (below) is applied
    bool blendGad = false;                       // AANI
    bool extendGad = false;                      // AANI: the last key stays when the clip ends
    bool applyFutureGads = false;                // AANI
    std::vector<PositionKey> gadPositions;       // AANI: GadKeysPos, in the order of the file
    std::vector<OrientationKey> gadOrientations; // AANI: GadKeysOri
    gff4::TlkString text;                        // ASLN's line, AANF's
    std::wstring voBank;                         // ASLN
    std::wstring lipsync;                        // ASLN
    std::wstring sound;                          // ACSP
    std::int32_t actor = -1;          // the other actor's id: AACC's camera, ACHT's look, ALNK's and ACRU's target
    std::int32_t node = -1;           // ALNK: the target's node linked to
    float lookSpeed = 0.0F;           // ACHT: the look-at multiplier set on the action's first frame
    bool realignStart = false;        // ACHT: the creature's facing refreshed on the action's first frame
    bool realignContinue = false;     // ACHT: and on each step of the cut while the action runs
    std::wstring tag;                 // AJSC's and AJSP's stage tag
    std::wstring lookAt;              // AJSP
    bool stageCameraDefault = false;  // AJSC: the tag is a place's, its default camera is the camera
    bool stageCameraHenchman = false; // AJSC: the camera is the henchman's place's default; wins over the above
    bool visible = false;             // ACVI
    bool drawMain = false;            // ADW_: the main hand's weapon drawn
    bool drawOff = false;             // ADW_
    std::wstring effect;              // AFBE: "da_dof", "da_colorfade"
    std::vector<EffectParameter> parameters; // AFBE
    std::wstring movie;                      // APBM
    std::optional<Fog> fog;                  // AFOG
};

struct Actor
{
    std::wstring name;                // the toolset's, "MASTER", "Camera 4"
    std::uint32_t id = 0;             // what other actors' actions name it by
    std::wstring model;               // "tools_null" for a camera or the master
    std::wstring creature;            // the template it is made from; empty for none
    std::wstring mappingTag;          // the tag of the area's creature that plays it; empty for none
    std::uint8_t mappingRequired = 0; // 255 or 0, as written
    std::int32_t cameraTarget = -1;
    std::int32_t pose = -1;
    std::array<float, 3> originPosition{};
    std::array<float, 4> originOrientation{}; // a quaternion
    std::array<float, 3> finalPosition{};
    std::array<float, 4> finalOrientation{};
    std::uint8_t master = 0; // 255 for the master actor
    std::uint8_t inventory = 0;
    float scale = 1.0F;
    std::vector<Action> actions; // in the file's order
};

struct Cutscene
{
    std::wstring area;      // EnvArea: "bhm500ar_tower_harrowing"
    float runTime = 0.0F;   // seconds
    std::wstring endScript; // run when a script's cutscene ends, skipped or not, never for a line's; empty for none
    std::wstring blendTree; // "bhmcs_intro.cub"
    std::uint32_t fps = 0;
    std::wstring stage;                    // CutsceneStageResRef; empty for none
    std::uint8_t playUntilVoCompletes = 0; // 255 or 0, as written; only a line's cut waits on its voices
    std::uint8_t areaRequired = 0;         // 255 or 0, as written
    std::wstring henchmanTag;
    std::vector<std::wstring> resources; // "bhmcs_intro.cub", "da_dof.ffx.xml"
    std::vector<Actor> actors;
};

// Where a conversation line's cut is placed: its stage's position and its heading. A script's
// cutscene has none, and plays in the world's frame, the default.
struct Stage
{
    Vector3 position{};
    Quaternion orientation{0.0F, 0.0F, 0.0F, 1.0F};
};

// Returns the time the cutscene's clock shows after some seconds: the nearest of its frames.
// @param fps the cutscene's frames a second; 0, a file with none, is read as 30.
float FrameTime(float seconds, std::uint32_t fps);

// Returns a curve's value at a time on the cutscene's clock, as the engine samples it.
float ValueAt(const Curve &curve, float time);

// How far a clip's travel has taken its actor: a position and a turn.
struct Travel
{
    Vector3 position{};
    Quaternion orientation{0.0F, 0.0F, 0.0F, 1.0F};
};

// Returns an animation's travel at a time on the cutscene's clock: its GadKeysPos and
// GadKeysOri at the time since its start, the first key before the first and the last after
// the last, between two keys in a line (an orientation's normalised). No keys is no travel.
Travel TravelAt(const Action &animation, float time);

// An actor's place at a time, a stage's frame applied.
struct Placement
{
    Vector3 position{};
    Quaternion orientation{0.0F, 0.0F, 0.0F, 1.0F};
};

// Returns where an actor is at a time: its position and orientation tracks (the last of each
// begun by then) with the travel of its animations added, as the engine's step does. An
// animation with PlayGad is added while it is live, and its last key stays when it has ended
// with ExtendGad. One with BlendGad is left out, which the engine blends by weight.
// @param stage the stage the cutscene plays in; the default for a script's cutscene.
Placement PlacementAt(const Actor &actor, float time, const Stage &stage = {});

// Returns where a position track (an action of type 17) puts its actor in the world at a time.
// A curve the track lacks reads as 0.
Vector3 PositionAt(const Actor &actor, const Action &track, float time, const Stage &stage = {});

// Returns how an orientation track (an action of type 18) turns its actor in the world at a time.
// Its three curves are degrees of yaw, pitch and roll; a curve it lacks reads as 0.
Quaternion OrientationAt(const Actor &actor, const Action &track, float time, const Stage &stage = {});

// Returns a cutscene file's cutscene, or nothing for a file that is not one.
std::optional<Cutscene> CutsceneOf(const gff4::File &file);

// Returns the cutscene a CUT struct holds: a cutscene file's top struct, or
// one a conversation's line holds in its own file (ConversationFile::Cutscene).
Cutscene CutsceneOf(const gff4::Struct &root);

} // namespace dao::core::cutscene
