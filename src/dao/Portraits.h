// A creature's portrait framed by the plugin: the camera the engine's
// portrait renderer would take from the portraits table (portraits.gda,
// by the creature's tag, else its Default row), given instead. The Default
// row is framed for a person's head; a creature much bigger or smaller
// (an ogre, a shriek, the Broodmother) needs its own, which the table has
// for none of them.
//
//   dao::portraits::SetCamera(ogre, {.distance = 1.6f});
//   const std::wstring image = creature->RenderPortrait();   // dao/Objects.h
//
// Read 2026-10-02 (dao/Addresses.h, PortraitRenderer::PlaceCamera): the
// renderer places its camera through one function given the table's eight
// values, each passed "unset" for an ordinary creature and so read from
// the table; this gives the ones the plugin set, the rest still the
// table's. The camera is set about the model's head node, turned by the
// rotation and pitch (degrees) and backed off by the distance (metres),
// then shifted by the Z and X adjusts; the light's three are the rig's
// offset. Character creation's own creature keeps its own framing. Takes
// effect on the creature's next render. Not yet seen in play.
#pragma once

#include "dao/ObjectId.h"

#include <optional>

namespace dao::portraits
{

// The portraits table's camera columns; one not given is the table's.
struct Camera
{
    std::optional<float> pitch;    // CameraPitch
    std::optional<float> rotation; // CameraRotation
    std::optional<float> distance; // CameraDistance
    std::optional<float> zAdjust;  // CameraZAdjust
    std::optional<float> xAdjust;  // CameraXAdjust
    std::optional<float> lightX;   // LightXAdjust
    std::optional<float> lightY;   // LightYAdjust
    std::optional<float> lightZ;   // LightZAdjust
};

// Frame this creature's portrait with the camera given, until cleared.
void SetCamera(ObjectId creature, const Camera &camera);
// Back to the table's.
void ClearCamera(ObjectId creature);

} // namespace dao::portraits
