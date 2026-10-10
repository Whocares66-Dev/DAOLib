// Portrait framing: set the camera for a creature's portrait in place of the one the portraits
// table (portraits.gda) gives it. Use it for creatures that the table's Default row does not
// fit, such as an ogre, a shriek or the Broodmother.
//
//   dao::portraits::SetCamera(ogre, {.distance = 1.6f});
//   const std::wstring image = creature->RenderPortrait();   // dao/Objects.h
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <optional>

namespace dao::portraits
{

// A portrait camera, with one value per camera column of the portraits table. A value left
// empty is taken from the table's row for the creature. The camera aims at the model's head
// node, is turned by the rotation and pitch, backed off by the distance, and then shifted by
// the Z and X adjustments.
struct Camera
{
    std::optional<float> pitch;    // CameraPitch, in degrees
    std::optional<float> rotation; // CameraRotation, in degrees
    std::optional<float> distance; // CameraDistance, in metres
    std::optional<float> zAdjust;  // CameraZAdjust, a vertical shift
    std::optional<float> xAdjust;  // CameraXAdjust, a sideways shift
    std::optional<float> lightX;   // LightXAdjust, the light's x offset
    std::optional<float> lightY;   // LightYAdjust, the light's y offset
    std::optional<float> lightZ;   // LightZAdjust, the light's z offset
};

// Frames a creature's portrait with the given camera until ClearCamera is called. Takes
// effect the next time the creature's portrait is rendered. The creature being made in
// character creation keeps the game's own framing.
void SetCamera(ObjectId creature, const Camera &camera);
// Restores the table's framing for a creature's portrait.
void ClearCamera(ObjectId creature);

// Returns the world position of a creature's head: its model's "head" node, which the
// portrait camera aims at.
// @return nullopt if there is no such creature, or its skeleton has no "head" node (the
// deepstalker, the deer, the chicken and a few others)
//
// Call on the game thread. For creatures without a head node, the portrait camera aims at
// the feet, so frame them with Camera's zAdjust and xAdjust.
std::optional<Vector> HeadPosition(ObjectId creature);

} // namespace dao::portraits
