// A visual effect out of the game's files, over bytes (DAOLib #181): the
// effects table's row naming it, its model's emitters, lights, crust hooks
// and meshes with every parameter as the file has it, and how its clips name
// them. For a reimplementation that simulates and draws the game's effects
// (DAOParagonEngine #26); dao/Vfx.h is the running game's effects.
//
//   const auto row = dao::core::effect::RowOf(effects, 3006); // the wisp's crust
//   row->ModelFor(0)->File();                                  // L"fxc_wisp_c.mmh"
//   const auto effect = dao::core::effect::EffectOf(model);
//   effect->Find(L"glow")->emitter->birthRate;                 // 7
//
// There is no effect file of its own. An effect is the visual effects
// table's row (table 8, vfx_: vfx_base.gda and the add-ins'), which names a
// model (ModelName1 to 3, an .mmh each, chosen by the size of what the effect is on; the
// ModelName column the engine never reads) and an animation tree (BlendTreeName, an .anb).
// The model is a model hierarchy whose nodes are the effect's parts: emitters
// ("nemt"), point lights ("nplt"), crust hooks ("crst", a place on the
// creature the effect is a crust of), emitter groups ("nemg"), target groups
// ("emtg"), meshes ("mshh") and plain nodes, each with its transform and its
// exports, the names a clip's channels drive its parameters by. The tree's
// nodes vfximpact, vfxduration and vfxcessation are the effect's phases,
// each a clip (.ani, dao/core/Clip.h) whose channels key the parameters over
// the phase, and a clip's events (.evt, clip::EventsOf) name the emitters
// that fire at a time. dev/EFFECTS.md has the reading: 2026-10-18 over all
// 600 effect models the game takes (fx*.mmh), their 549 trees, 1,091 clips
// and 1,070 event files, and vfx_base.gda's 993 rows. How the engine spawns,
// moves and draws a particle with the parameters is lib-agent-2's reading in
// the exe, the same day, in dev/EFFECTS.md's runtime section; the members'
// comments below give its meanings where the files agree.
#pragma once

#include "dao/core/Gff4.h"
#include "dao/core/M2da.h"
#include "dao/core/Model.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace dao::core::effect
{

using Vector = std::array<float, 3>;
using Colour = std::array<float, 4>; // r, g, b, a

// A sound a row plays: its event's name and its volume and pitch.
struct Sound
{
    std::wstring name; // "vfx_spells/vfx_new/monster/fxc_wisp_c_dur_n"
    float volume = 0;
    float pitch = 0;
};

// A model a row takes at a size.
struct Sized
{
    std::wstring model; // ModelNameN trimmed of spaces and tabs, as the engine loads it, without .mmh
    float size = 0;     // SizeN

    // Returns the model's file, "fxc_wisp_c.mmh".
    std::wstring File() const;
};

// The model an effect is drawn with and how much it is scaled.
struct Chosen
{
    Sized model;
    float scale = 1; // the owner's size over the model's: 1 where either is 0
};

// An effect's row of the visual effects table.
struct Row
{
    std::int64_t id = 0;
    std::wstring label;
    std::wstring type; // "D" (601 rows), "F" (381), "T" (9), "C" (1)
    bool fixedOrientation = false;
    bool alignToGround = false;
    // ModelName1 to 3 with Size1 to 3: the leading ones that name a model, and ModelName2 and 3 only
    // when Size1 is above 0, as the engine's row loader reads them. The ModelName column is no
    // part of it: the engine never reads it for this table (it equals ModelName1 in 991 of the
    // vanilla table's 993 rows, and the other two have both empty).
    std::vector<Sized> sizes;
    std::wstring tree;  // BlendTreeName: the .anb without its extension
    bool crust = false; // CrustEffect
    Sound impact;
    Sound duration;
    Sound cessation;
    std::wstring tint;
    std::int64_t loadGlobal = 0;
    bool pauseAnimate = false;
    std::int64_t priority = 0;
    bool showInPaperDoll = false;

    // Returns the model the engine takes for an owner of a size, as it chooses: with a size and
    // Size1 above 0 and several models, the one whose size is nearest the owner's (a tie the
    // smaller, one past the last size the last), else the first; scaled by owner size over its size.
    // @param ownerSize the owning creature's apr_base PerSpace; 0 for a location or anything but a creature
    // @return nothing for a row with no model
    std::optional<Chosen> ModelFor(float ownerSize) const;
    // Returns the tree's file, "fxc_wisp_c.anb"; empty for a row with no tree.
    std::wstring TreeFile() const;
};

// Returns an effect's row: the one the game takes for the ID.
// @param effects the visual effects table merged (table 8)
// @return nothing for an ID the table has no row of
std::optional<Row> RowOf(const m2da::Merged &effects, std::int64_t id);

// The tree's nodes that are an effect's phases, each a clip.
inline constexpr std::wstring_view kImpact = L"vfximpact";
inline constexpr std::wstring_view kDuration = L"vfxduration";
inline constexpr std::wstring_view kCessation = L"vfxcessation";

// A field as the file has it, kept for what the members below do not read.
struct Raw
{
    std::uint32_t label = 0; // dev/gff4_fields.csv names it
    gff4::Type type = gff4::Type::U8;
    // A whole number (a talk table string's strref), the floats of a float or vector, or a string.
    std::variant<std::int64_t, std::vector<float>, std::wstring> value;
};

// A struct of a node that the members below do not read: its label and fields.
struct Part
{
    std::string label;
    std::vector<Raw> fields;
    std::vector<Part> parts;
};

// What a node's export drives: its MMHExportTagVariableType, each the one name ending its exports'
// names in the game's files.
enum class Parameter : std::uint32_t
{
    Rotation = 0,
    Translation = 1,
    GobRotation = 4,
    GobTranslation = 5,
    Life = 6,
    LifeRange = 7,
    BirthRate = 8,
    BirthRateRange = 9,
    InitialSpeed = 10,
    InitialSpeedRange = 11,
    Acceleration = 12,
    GravityMultiplier = 13,
    InitialRotationSpeed = 14,
    InitialRotationSpeedRange = 15,
    SpawnSpreadX = 16,
    SpawnSpreadY = 17,
    MovementSpreadX = 18,
    MovementSpreadY = 19,
    InvMovementSpreadUpdateDelay = 20,
    ScaleRange = 21,
    TargetAttraction = 22,
    TargetRadius = 23,
    Colour = 24, // a light's
    Radius = 25, // a light's
    Scale = 27,
    AgeMapColourMultiplier = 28,
    AgeMapScaleXMultiplier = 29,
    AgeMapScaleYMultiplier = 30,
    RotationalAcceleration = 31, // the exe's emitter has it (lib-agent-2); no export of the game's
    WorldAxisAcceleration = 32,
    RemoteMaterialTintColour = 33,
    RemoteMaterialFresnelFalloff = 34,
    RemoteMaterialAlpha = 35,
    MaterialColour = 36,
    SplatColourMultiplier = 37,
};

// A key of an emitter's age map: a particle's scale and colour at a share of its life.
struct AgeKey
{
    float lifeElapsed = 0; // 0 at its birth, 1 at its death
    float scaleX = 1;
    float scaleY = 1;
    Colour colour{1, 1, 1, 1};
};

// Where an emitter's particles are born.
struct SpawnVolume
{
    // 0 a mesh (its points and indices), 1 a sphere (its radius), 2 a cylinder (its radius, length
    // and axis), 3 a box (its corners); a shape's other fields are NaN in the files.
    std::uint8_t type = 0;
    bool spawnWithinVolume = false;
    bool invertNormals = false;
    bool normalsAsDirection = false;
    float radius = 0;
    float cylinderLength = 0;
    Vector cylinderAxis{};
    Vector boxMin{};
    Vector boxMax{};
    std::vector<Vector> points;         // a mesh's
    std::vector<std::uint16_t> indices; // a mesh's, into its points
};

// What an emitter's particles are born on: an "emat".
struct Attachment
{
    std::uint32_t type = 0; // 1 or 4 in the files
    std::wstring name;      // empty in every one of the game's
    bool spawnOnSurface = false;
    bool useNormalForVelocity = false;
};

// Where a particle lands: an emitter's splat ("spla", the blood pools').
struct Splat
{
    std::wstring material;
    float width = 0;
    float height = 0;
    std::uint32_t samplesWidth = 0;
    std::uint32_t samplesHeight = 0;
    float orientationRange = 0;
    float life = 0;
    std::uint8_t flipbookType = 0;
    float flipbookFramesPerSecond = 0;
    std::uint8_t flipbookRows = 0;
    std::uint8_t flipbookColumns = 0;
    bool flipbookRandomStartFrame = false;
    bool holdLastFrame = false;
    Colour ageMapColourMultiplier{1, 1, 1, 1};
    std::vector<AgeKey> ageMap;
};

// An emitter: a "nemt" node's fields, named as dev/gff4_fields.csv names them. Every emitter of the
// game's has every field.
struct Emitter
{
    std::wstring material;        // MMHMaterialObject: a material of the library
    std::wstring materialLibrary; // MMHMaterialLibrary: "fx_miscmatlib.mal"; 637 of 2,927 without ".mal"
    // 0 emits at its birth rate all the time; 1 is a burst, birthRate particles at once each time
    // an event of its clips (id 16, naming it) arms it. 1,040 of the game's emitters are 1, 1,026
    // of them named by an event.
    std::uint8_t type = 0;
    float birthRate = 0; // particles a second, or a metre with birthRateInParticlesPerMeter
    float birthRateRange = 0;
    float life = 0; // seconds
    float lifeRange = 0;
    float scaleRange = 0;
    float initialSpeed = 0;
    float initialSpeedRange = 0;
    float acceleration = 0;
    float initialRotation = 0;
    float initialRotationRange = 0;
    float initialRotationSpeed = 0; // revolutions a second
    float initialRotationSpeedRange = 0;
    float rotationalAcceleration = 0;
    float invMovementSpreadUpdateDelay = 0;
    float spawnSpreadX = 0; // degrees either way, about the emitter's X; Y about its Y
    float spawnSpreadY = 0;
    float movementSpreadX = 0;
    float movementSpreadY = 0;
    float gravityMultiplier = 0;
    Vector worldAxisAcceleration{};
    bool objectSpaceAcceleration = false;
    bool birthRateInParticlesPerMeter = false;
    bool affectedByWind = false;
    bool followPath = false;
    bool linkParticlesTogether = false;
    bool updateOnlyWhenVisible = false;
    bool enableCollisions = false;
    bool inheritVelocityInsteadOfPos = false;
    float bounciness = 0;
    float friction = 0;
    // How a sprite faces: 0 the screen, 1 flat to the emitter's Z at its birth, 2 flat to the world's
    // Z, 3 along its velocity, 4 along the world's Z.
    std::uint32_t orientationBehavior = 0;
    // The frame its particles move with: 0 none (left where born), 1 the emitter, 2 its parent, 3
    // the model's root.
    std::uint32_t particleInheritance = 0;
    std::uint8_t flipbookType = 0; // 0 to 2; 2 a volume texture
    float flipbookFramesPerSecond = 0;
    std::uint8_t flipbookRows = 0;
    std::uint8_t flipbookColumns = 0;
    bool flipbookRandomStartFrame = false;
    std::wstring targetName;
    float targetAttraction = 0;
    float targetRadius = 0;
    bool spawnDirectionTracksTarget = false;
    bool killParticleWhenTargetHit = false;
    bool isPhysicsEmitter = false;
    bool isPhysicsObjectSpawnEmitter = false;
    std::wstring meshParticleModel; // a model each particle is: "fx_rock"
    Vector meshParticleUpAxis{};
    Vector meshParticleRollAxis{};
    std::uint8_t vertexFormat = 0; // 255 in every one
    std::uint32_t uvDistributionSize = 0;
    bool ignoreDistortion = false;
    bool canParticlesSplat = false;
    std::uint8_t lod = 0;
    std::wstring userParamName; // "color", "fire"
    bool useVariationTint = false;
    // The age map's multipliers, which the clips' channels key.
    Colour ageMapColourMultiplier{1, 1, 1, 1};
    float ageMapScaleXMultiplier = 1;
    float ageMapScaleYMultiplier = 1;
    std::vector<AgeKey> ageMap; // in the file's order, lifeElapsed rising; linear between keys, flat past the ends
    std::optional<SpawnVolume> spawnVolume;
    std::vector<Attachment> attachments;
    std::optional<Splat> splat;
};

// A point light: an "nplt" node's fields.
struct Light
{
    Colour colour{}; // may pass 1: the wisp's is 0.6, 1.4, 2
    float radius = 0;
    bool isStatic = false;
    float intensityVariation = 0;
    float intensityPeriod = 0;
    float intensityPeriodDelta = 0;
    std::uint8_t affectDomain = 0; // 1 or 2
    bool canBeOccluded = false;
    bool useVariationTint = false;
    std::uint8_t version = 0;
};

// One node of the effect's model, and what its kind makes it.
struct Item
{
    model::Node node; // its name, kind ("nemt", "nplt", "crst" ...), place in the tree, transform and exports
    std::optional<Emitter> emitter;                            // a "nemt"
    std::optional<Light> light;                                // an "nplt"
    std::wstring material;                                     // a "nemg"'s or a "mshh"'s MMHMaterialObject
    std::optional<std::array<std::array<float, 4>, 2>> bounds; // its "bbox": the least and most corners
    std::vector<Raw> fields; // every field of its struct that is no list nor struct, as the file has it
    std::vector<Part> parts; // its structs no member above reads
};

// What a clip's channel drives.
struct Driven
{
    std::size_t item = 0;                    // in Effect::items
    const model::Export *exported = nullptr; // in that item's node.exports
    Parameter parameter = Parameter::Rotation;
};

// The remote material an effect puts on what it is a crust of: the model's own fields.
struct RemoteMaterial
{
    std::wstring decal; // MMHRemoteMaterialDecalName: "White.dds"
    float fresnelFalloff = 0;
    bool invertFresnel = false;
    float alpha = 0;
    Colour tint{};
};

struct Effect
{
    std::wstring name;
    float presimulateTime = 0; // MMHEmitterPresimulateTime, seconds
    std::optional<RemoteMaterial> remoteMaterial;
    std::vector<Item> items; // the model's nodes, in model::HierarchyOf's order
    std::uint32_t totalExports = 0;

    // Returns the item of a name, in any case; null for none.
    const Item *Find(std::wstring_view node) const;
    // Returns what a clip's channel drives: the export of its name, in any case. Nothing for a
    // channel no item exports (149 of the game's 6,773 effect channels name none).
    std::optional<Driven> Drives(std::wstring_view channel) const;
};

// Returns an effect's model read as its parts, or nothing for a file that is no model hierarchy.
std::optional<Effect> EffectOf(const gff4::File &model);

} // namespace dao::core::effect
