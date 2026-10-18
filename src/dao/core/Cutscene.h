// A cutscene (.cut) read, over bytes: the script it runs at its end, its
// length, the area and stage it plays in, and its actors, each with its
// queue of actions on the cutscene's clock: animations, spoken lines,
// sounds, camera cuts, stage places, drawn weapons, visibility, links to
// another actor, frame-buffer effects, movies and fog, and the curves that
// move them. A script starts one with CS_LoadCutscene; even one the player
// skips runs its end script.
//
//   const auto cutscene = dao::core::cutscene::CutsceneOf(file);
//   cutscene->endScript; // "gen00cs_cutscene_end"
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
// stage, none a henchman tag or henchman actions; 2,396 run at 100 frames a
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

// A key of a curve: a value at a time on the cutscene's clock.
struct Vertex
{
    float time = 0.0F;
    float value = 0.0F;
};

// How a curve goes from one vertex to the next.
struct Transition
{
    std::uint8_t type = 0; // 0 or 1, as written
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
    gff4::TlkString text;    // ASLN's line, AANF's
    std::wstring voBank;     // ASLN
    std::wstring lipsync;    // ASLN
    std::wstring sound;      // ACSP
    std::int32_t actor = -1; // the other actor's id: AACC's camera, ACHT's look, ALNK's and ACRU's target
    std::int32_t node = -1;  // ALNK: the target's node linked to
    std::wstring tag;        // AJSC's and AJSP's stage tag
    std::wstring lookAt;     // AJSP
    bool visible = false;    // ACVI
    bool drawMain = false;   // ADW_: the main hand's weapon drawn
    bool drawOff = false;    // ADW_
    std::wstring effect;     // AFBE: "da_dof", "da_colorfade"
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
    std::wstring endScript; // run when the cutscene ends or is skipped; empty for none
    std::wstring blendTree; // "bhmcs_intro.cub"
    std::uint32_t fps = 0;
    std::wstring stage;                    // CutsceneStageResRef; empty for none
    std::uint8_t playUntilVoCompletes = 0; // 255 or 0, as written
    std::uint8_t areaRequired = 0;         // 255 or 0, as written
    std::wstring henchmanTag;
    std::vector<std::wstring> resources; // "bhmcs_intro.cub", "da_dof.ffx.xml"
    std::vector<Actor> actors;
};

// Returns a cutscene file's cutscene, or nothing for a file that is not one.
std::optional<Cutscene> CutsceneOf(const gff4::File &file);

// Returns the cutscene a CUT struct holds: a cutscene file's top struct, or
// one a conversation's line holds in its own file (ConversationFile::Cutscene).
Cutscene CutsceneOf(const gff4::Struct &root);

} // namespace dao::core::cutscene
