// The ambient light the engine gives what it draws, the colour of each point light, the light
// probe's irradiance and the share of the sun it hands each object, read and set. Level meshes, terrain and
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

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

// One point light's colour, as the engine takes it to light an object.
struct PointLight
{
    // The colour the shaders are given, before the light's flicker: vanilla's until a callback sets it.
    Color color{};
    // The light's own colour, as its model has it, before its owner's tint.
    Color own{};
    // The light's name in its model (the "Hotspots" of a level's _ll.mmh), or empty; lasts the call alone.
    std::wstring_view name;
    // The name of the model the light is part of, or empty; lasts the call alone. A room's layout lights are
    // its <level>_<x>_<y>_<d>_ll, the level's lights for figures _lc, the party portraits' portrait_lightrig.
    std::wstring_view model;
    // The light's node, the same each frame for the same light; for telling lights apart.
    std::uintptr_t node = 0;
    // Where the light is in the world.
    Vector place{};
    // How far the light reaches, in metres.
    float reach = 0;
};

// Adds a callback to run each time the engine takes a point light's colour, once for each object
// the light reaches, every frame.
// @param callback may change `color`; the rest is for reading
//
// Nothing is kept: a callback that stops changing the colour leaves the light as vanilla has it.
// Callbacks run on the render thread, many times a frame, so keep them short.
void OnPointLight(std::function<void(PointLight &)> callback);

// Which of the light manager's lists a point light is in. The level's are the lights placed with
// the level and its effects, the figures' those that move with a creature; a flagged light is one
// its model marks (BlueprintFlag), which the engine lights fewer objects with.
enum class LightList : std::uint8_t
{
    LevelsUnflagged,
    LevelsFlagged,
    FiguresFlagged,
    FiguresUnflagged,
};

// A point light of the area's scene, as the engine has it this frame.
struct SceneLight
{
    std::uintptr_t node = 0; // the same each frame for the same light, as PointLight's
    Vector place{};          // where it is in the world
    float reach = 0;         // how far it reaches, in metres
    // Its colour as the shaders are handed it: its own, times its owner's tint where it takes one, times
    // its strength and its flicker; less only the fade each object may give it.
    Color color{};
    Color own{};        // its own colour, as its model has it
    float strength = 0; // what the colour is scaled by
    float flicker = 0;  // the flicker's share this frame, 1 for a steady light
    // The game object whose model the light is part of, or kInvalidObject for one of no game object
    // (a room's layout, a figures' rig, a weapon's part).
    ObjectId owner = kInvalidObject;
    LightList list = LightList::LevelsUnflagged;
    std::wstring model; // the model the light is part of
    std::wstring name;  // the light's name in it
};

// Returns every point light of the current area's scene, from the light manager's lists.
// @return none with no area loaded, or off the game thread
//
// Call it on the game thread, for example from a frame callback (dao/Frame.h). Lights of other scenes
// (a portrait's, an area fast load keeps) are not in it.
std::vector<SceneLight> PointLights();

// Stops, or lets again, the engine giving objects the lights of the two unflagged lists: the lights the
// level and its effects place, and those that move with figures. A plugin that lights every surface with
// them itself (from PointLights) turns them off so they are not added twice.
// @param take false to give objects none of them, true for vanilla's
//
// It lasts for every area loaded after, until changed. Call it on the game thread.
void SetObjectsTakeUnflagged(bool take);

// A light probe of the area's scene.
struct SceneProbe
{
    std::uintptr_t node = 0;            // the probe's node, as ProbeLight's `probe`
    Vector place{};                     // where it is in the world
    std::array<float, 48> irradiance{}; // three 4x4 matrices for red, green and blue, as its blueprint has them
    std::wstring model;                 // the model it is part of: a room's <level>_<x>_<y>_<d>_ll, or a rig's
};

// Returns the probes a light state chose: the object's, from which the engine hands it one.
// @param state a light state, as OnAmbient, OnProbe and OnSunShare give it
// @return none for no state, or off the game and render threads
std::vector<SceneProbe> Probes(std::uintptr_t state);

// Returns every light probe of the current area's scene. Call it on the game thread.
std::vector<SceneProbe> AreaProbes();

// One light state's light probe, as the engine has just handed it the probe's irradiance.
struct ProbeLight
{
    // The probe's irradiance, three 4x4 matrices for red, green and blue, as the shaders are handed
    // them (g_mLightParams). A callback may change them; the engine copies them from the probe afresh
    // for each fill while any OnProbe callback is added, so a change lasts this fill alone.
    std::span<float, 48> irradiance;
    // The name of the model the probe is part of: a room's <level>_<x>_<y>_<d>_ll, or a figure rig's
    // (outside_day_rig). Lasts the call alone.
    std::wstring_view model;
    // Which chooser last chose the state's lights.
    Chosen chosen = Chosen::Unknown;
    // The probe's node, the same each frame for the same probe.
    std::uintptr_t probe = 0;
    // The light state, the same each frame for the same object; for telling objects apart.
    std::uintptr_t state = 0;
};

// Adds a callback to run each time the engine hands an object its light probe's irradiance, at most
// once per object and frame, after the engine's copy and before the object is drawn. Objects with
// no probe chosen are not passed.
// @param callback may change `irradiance`; the rest is for reading
//
// Call it when the plugin loads or later. Callbacks run on the render thread, many times a frame,
// so keep them short and call no script commands from them.
void OnProbe(std::function<void(ProbeLight &)> callback);

// One object's share of the sun, as the engine chooses its lights.
struct SunShare
{
    // The share the object's sun eases toward, which scales the sun's colour for it. Vanilla's until a
    // callback sets it: 1, or 0.1 where the level's walk grid has the object's cell out of the sun.
    float share = 1.0F;
    // Which chooser is choosing the object's lights.
    Chosen chosen = Chosen::Unknown;
    // The object's light state, as OnAmbient and OnProbe have it.
    std::uintptr_t state = 0;
};

// Adds a callback to run each time the engine chooses an object's lights and sets the share of the sun
// it eases the object toward: about once a frame for each object drawn, paused or not.
// @param callback may change `share`
//
// The object's sun reaches a changed share over a moment, as vanilla's does. Call it when the plugin
// loads or later. Callbacks run on the render thread, so keep them short
// and call no script commands from them.
void OnSunShare(std::function<void(SunShare &)> callback);

} // namespace dao::lights
