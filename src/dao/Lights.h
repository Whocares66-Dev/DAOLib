// The ambient light the engine gives what it draws, read and set. Level meshes, terrain and
// props take it as g_vLightParams[4], added to the light of the lamps and the sun; it is the
// sum of the ambient lights of the room (the lights in each room's _ll.mmh) that the engine
// chose for the object. Figures take their ambient light from a light probe instead and are
// given none here. SpeedTree's trees light themselves and are not reached.
//
//   dao::lights::OnAmbient([](dao::lights::Ambient &ambient) {
//       if (ambient.chosen == dao::lights::Chosen::ForLevel)
//       {
//           ambient.color = skyLight;
//       }
//   });
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>

namespace dao::lights
{

// A colour: red, green, blue and a fourth.
using Color = std::array<float, 4>;

// Which of the engine's choosers last chose the lights of an object.
enum class Chosen : std::uint8_t
{
    Unknown,   // not seen chosen since the callback was added
    ForLevel,  // a placed piece of the level: a mesh, the terrain, a prop
    ForMoving, // something that moves: a figure, or whatever the engine lights as one
};

// One object's ambient light, as the engine has just worked it out.
struct Ambient
{
    // The colour the shaders are given, vanilla's until a callback sets it; w is 1.
    Color color{};
    // What the ambient lights are added to: zero in every case read so far.
    Color base{};
    // The colour of each ambient light chosen for the object, which vanilla adds to the base.
    std::span<const Color> lights;
    // Which chooser last chose the object's lights.
    Chosen chosen = Chosen::Unknown;
    // The object's light state, the same each frame for the same object; for telling objects apart.
    std::uintptr_t state = 0;
};

// Adds a callback to run each time the engine works out an object's ambient light, at most once
// per object and frame, before the object is drawn.
// @param callback may change `color`; the rest is for reading, and `lights` lasts the call alone
//
// Call it when the plugin loads or later. Callbacks run on the render thread, many times a
// frame, so keep them short and call no script commands from them.
void OnAmbient(std::function<void(Ambient &)> callback);

} // namespace dao::lights
