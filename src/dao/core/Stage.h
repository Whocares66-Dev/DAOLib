// A stage (.stg) read, over bytes: the places a cutscene's actors are put
// at, and the cameras it cuts to (DAOLib #196).
//
//   const auto stage = dao::core::stage::StageOf(file);
//   const auto *place = dao::core::stage::FindPlace(*stage, L"duncan");
//   const auto *camera = dao::core::stage::FindCamera(*stage, L"group");
//
// The file is GFF4 "STG V1.0", read 2026-10-18 over all 617 of the game's
// (core and add-ins, jowan_attack the Circle's among them): every file has the
// same three templates, STAG (the file's top: StagePlaceList 11000 and
// StageCameraList 11001), PLAC and CAMR. A place and a camera each hold a Tag,
// a Position (a vector) and an Orientation (a quaternion x, y, z, w) in the
// stage's own frame. How a stage is used is in dev/CUTSCENES.md: a line's cut
// names it in CutsceneStageResRef (5017), a conversation line sets it by
// ConversationStageName (12100) from the area's StageList entry with that
// tag, or places it at the speaker (StageAtCurrentLocation, 12104); an
// actor's JumpToPlace action takes a place by tag and a JumpToCamera action a
// camera by tag, each placed by the stage's transform (cutscene::Stage), and
// a camera's field of view through the same divide.
#pragma once

#include "dao/core/Gff4.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::stage
{

// A point in the stage's frame.
using Vector3 = std::array<float, 3>;

// An orientation in the stage's frame as a quaternion (x, y, z, w).
using Quaternion = std::array<float, 4>;

// A spot an actor can be put at.
struct Place
{
    std::wstring tag; // "duncan"
    Vector3 position{};
    Quaternion orientation{0.0F, 0.0F, 0.0F, 1.0F};
    std::wstring defaultCamera; // StagePlaceDefaultCamera (11004): the tag of the camera that frames it
};

// A camera the stage cuts to.
struct Camera
{
    std::wstring tag; // "group", "cam2_3_2"
    Vector3 position{};
    Quaternion orientation{0.0F, 0.0F, 0.0F, 1.0F};
    std::vector<std::wstring> placesInShot; // StagePlacesInShot (11002): tags of places
    float fov = 0.0F;                       // StageCameraFOV (11003), degrees
    std::wstring lookingFrom;               // StageCameraLookingFrom (11006): a tag; empty in jowan_attack
    std::wstring lookingAtPrimary;          // StageCameraLookingAtPrimary (11007): a place's tag
    std::wstring lookingAtSecondary;        // StageCameraLookingAtSecondary (11008)
    std::uint8_t lookingAtType = 0;         // StageCameraLookingAtType (11009), as written
};

struct Stage
{
    std::vector<Place> places;   // in the file's order
    std::vector<Camera> cameras; // in the file's order
};

// Returns a stage file's stage, or nothing for a file that is not one.
std::optional<Stage> StageOf(const gff4::File &file);

// Returns the stage a STAG struct holds.
Stage StageOf(const gff4::Struct &root);

// Returns the place with a tag, or null for none. The first of several, as the file has them.
const Place *FindPlace(const Stage &stage, std::wstring_view tag);

// Returns the camera with a tag, or null for none.
const Camera *FindCamera(const Stage &stage, std::wstring_view tag);

} // namespace dao::core::stage
