// The game's draws as Direct3D 9 receives them, for a tool that records a frame: each draw call with
// its shaders, their constants by name, the textures they sample and the render states asked for,
// handed to a plugin on the render thread while it has the watch armed.
//
//   static const auto watch = dao::render::Watch(
//       [](const dao::render::Draw &draw) { /* copy what is wanted */ },
//       [](std::uint64_t frame) { /* the armed frame's draws are all in */ });
//   watch.Arm(1);   // the next frame, whole
//
// The library places one set of detours on the device for every plugin. With no watch armed, a draw
// costs the test of one counter.
#pragma once

#include "dao/core/Ctab.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

// d3d9.h's COM interfaces, named without its header; Release is how they are freed, not a destructor.
struct IDirect3DDevice9;      // NOLINT(cppcoreguidelines-virtual-class-destructor)
struct IDirect3DBaseTexture9; // NOLINT(cppcoreguidelines-virtual-class-destructor)

namespace dao::render
{

// Which of the device's draw calls it was.
enum class Call : std::uint8_t
{
    Primitive,          // DrawPrimitive
    IndexedPrimitive,   // DrawIndexedPrimitive
    PrimitiveUp,        // DrawPrimitiveUP
    IndexedPrimitiveUp, // DrawIndexedPrimitiveUP
};

// One constant of a shader's table, with what the device holds in its registers.
struct Value
{
    const core::ctab::Constant *constant = nullptr; // its name and type; for the callback only
    std::vector<float> floats;                      // a float4 constant's: four a register
    std::vector<int> ints;                          // an int4 constant's: four a register
    std::vector<bool> bools;                        // a bool constant's: one a register
};

// A texture a shader samples.
struct Sampled
{
    std::uint32_t sampler = 0;                // the sampler's register in the shader
    IDirect3DBaseTexture9 *texture = nullptr; // for the callback only
    std::wstring_view name;                   // the .dds it was made from, empty if not one; for the callback only
};

// A shader bound for the draw: none (empty bytecode) for the fixed pipeline.
struct Shader
{
    std::span<const std::byte> bytecode;      // for the callback only: copy it to keep it
    const core::ctab::Table *table = nullptr; // its constant table, null if it has none; for the callback only
    std::vector<Value> values;                // each constant of the table, in its order, with its registers
    std::vector<Sampled> textures;            // each sampler of the table that has a texture bound
};

// A render state's value.
struct State
{
    std::uint32_t state = 0; // D3DRENDERSTATETYPE
    std::uint32_t value = 0;
};

// One draw call.
struct Draw
{
    std::uint64_t frame = 0; // the frame it is in, counted by the presents
    std::uint32_t index = 0; // its place among the frame's draws, from 0
    IDirect3DDevice9 *device = nullptr;
    Call call = Call::Primitive;
    std::uint32_t type = 0;      // D3DPRIMITIVETYPE
    std::int32_t baseVertex = 0; // DrawIndexedPrimitive's
    std::uint32_t minIndex = 0;  // DrawIndexedPrimitive's and DrawIndexedPrimitiveUP's
    std::uint32_t vertices = 0;  // the indexed calls' vertex count
    std::uint32_t start = 0;     // the first vertex or index
    std::uint32_t primitives = 0;
    Shader vertex;
    Shader pixel;
    std::vector<State> states; // the render states the watch was armed with, in its order
};

// What a watch hands a draw to, on the render thread.
using DrawEvent = std::function<void(const Draw &draw)>;
// What a watch hands the end of an armed frame to, on the render thread, at the frame's present.
using PresentEvent = std::function<void(std::uint64_t frame)>;

// A plugin's watch on the draws: a handle to the library's record of it.
class Watcher
{
  public:
    // Hands the draws to the watch's callbacks from the next frame's first draw, for a number of
    // frames, or with 0 until Disarm.
    // @param states the render states to read at each draw, D3DRENDERSTATETYPE values
    void Arm(std::uint32_t frames, std::vector<std::uint32_t> states = {}) const;
    // Stops handing the draws, at once.
    void Disarm() const;
    // Returns true while it is armed and has frames left.
    bool Armed() const;

    // The library's own; a plugin gets its watcher from Watch.
    explicit Watcher(std::size_t id) : id_(id)
    {
    }

  private:
    std::size_t id_;
};

// Makes a watch on the game's draws, disarmed, and places the library's detours on Direct3D if they
// are not yet. The watch lives as long as the process; the handle may be copied.
//
// Call it at plugin load, before the game makes its device: a device made before is not watched.
Watcher Watch(DrawEvent draw, PresentEvent present = {});

// Returns the number of the frame being drawn, counted by the presents since the device was made.
std::uint64_t Frame();

} // namespace dao::render
