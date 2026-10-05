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
// the table. This gives all eight, the plugin's and for the rest the
// table's as that function would read them, and has the renderer's row be
// the table's blank one for the call: on the Default row, a creature that
// is not a person is otherwise framed from the table whatever it is given
// (the first try in play gave an ogre 1.6 m and its portrait did not
// change). The camera is set about the model's head node, turned by the
// rotation and pitch (degrees) and backed off by the distance (metres),
// then shifted by the Z and X adjusts; the light's three are the rig's
// offset. Character creation's own creature keeps its own framing. Takes
// effect on the creature's next render. Seen in play the same day by the
// turn-based mod, after the fix: an ogre framed from 2.56 m and 2.47 m
// shows its face, horns and teeth.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

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

// Where a creature's head is: its model's node "head", as the portrait
// camera finds it (CCCreature::HeadTransform; the point comes back with a
// fourth coordinate, which the portrait divides by, and so does this).
// Nothing for no such creature, or a skeleton with no node of that name
// (eleven appearance rows in the turn-based mod's sweep: the deepstalker,
// the deer, the chicken and others), where the portrait camera aims at the
// point PlaceCamera began with, (0, 0, 0) of its clone's scene, the clone's
// feet: seen in play 2026-10-04, the deepstalker framed from there by fixed
// adjusts (Camera's zAdjust and xAdjust). In the world: seen in play
// 2026-10-04 (DAOLib #17), the high dragon's head 6.8 to 7.2 m from its
// position as it stood, 20 m above it as it landed. On the game's thread.
std::optional<Vector> HeadPosition(ObjectId creature);

} // namespace dao::portraits
