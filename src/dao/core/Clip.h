// An animation clip (.ani) out of the game's files, over bytes (DAOLib #157):
// its channels, each a node of a model's tree and what of it the channel
// drives, and the keys, decoded as the exe decodes them. For a
// reimplementation that plays the game's animations (DAOParagonEngine);
// dao/core/Model.h reads the models they move.
//
// Read 2026-10-17 over all 4,518 clips the game takes (tools/gff4.py) and in
// the GOG exe. A clip is GFF4 "ANI V0.1": its name, its length in seconds,
// whether it is additive or an override and the override's priority, whether
// it ignores scale, and a list of channels. A channel is a name, "<node>_<what>"
// ("head_rotation", "root_translation"; a node's name in lower case, where the
// model's has capitals), a target byte, and its keys behind a generic
// reference. Each key is a 16-bit time, 0 to 65535 over the clip's length, and
// a value by the target (the loader 0x00815FB0 takes a key's size by it from
// 0x00B267D0):
//
//   2: a rotation in 32 bits (0x00811370): the "smallest three", three 10-bit
//      numbers at bits 22, 12 and 2, each (v - 511.5) * 2 / (sqrt 2 * 1023),
//      the fourth the square root of what they leave of 1, put at the place
//      the low two bits say; 100,114 channels.
//   3: a rotation in three 16-bit numbers (0x00811500): the first two's low
//      bits are the place, the numbers (v - 32767.5) * 2 / (sqrt 2 * 65535);
//      2,212.
//   4: a rotation in 64 bits (0x00811660): the low word's two low bits the
//      place, the high word's top 21 bits, then the next 21 (the low word's top
//      ten and the high word's low eleven), each (v - 1048575.5) * 2 / (sqrt 2
//      * 2097151), then the low word's bits 2 to 21, (v - 524287.5) * 2 /
//      (sqrt 2 * 1048575); 194,365.
//   6: three floats, a translation (and an emitter's bounds); 38,988.
//   8: a float, an emitter's value; 5,420.
//   10: four floats, a colour; 1,655.
//
// A rotation is x, y, z, w, as a model's "rota" nodes are: the largest is w in
// most keys, every decoded key is of length 1, and in c_beara.amb_feed.ani six
// of the nine bones keyed only at its start and end have c_blacbeara_0.mmh's
// own rotations to the third decimal (the root, GOD and the eyes' base are
// turned). Every channel's last key is at 65535, the clip's end.
#pragma once

#include "dao/core/Gff4.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dao::core::clip
{

using Quaternion = std::array<float, 4>; // x, y, z, w

// Returns a rotation key's value as the exe decodes it: 32 bits, three 16-bit numbers, 64 bits.
Quaternion Rotation32(std::uint32_t packed);
Quaternion Rotation48(std::uint16_t first, std::uint16_t second, std::uint16_t third);
Quaternion Rotation64(std::uint64_t packed);

// The target byte's kinds of key.
enum class Target : std::uint8_t
{
    Rotation32 = 2,
    Rotation48 = 3,
    Rotation64 = 4,
    Vector = 6,
    Float = 8,
    Colour = 10,
};

struct Key
{
    float time = 0; // seconds into the clip
    // A rotation's x, y, z, w; a vector's x, y, z and 0; a float and three 0; a colour's r, g, b, a.
    std::array<float, 4> value{};
};

struct Channel
{
    std::wstring name; // as written: "head_rotation"
    std::wstring node; // the name before its last '_': "head"
    std::wstring what; // after it: "rotation", "translation", "birthrate"
    std::uint8_t target = 0;
    std::vector<Key> keys;

    [[nodiscard]] bool IsRotation() const;
};

struct Clip
{
    std::wstring name;
    float length = 0; // seconds
    bool additive = false;
    bool isOverride = false;
    std::uint32_t overridePriority = 0;
    bool ignoreScale = false;
    std::vector<Channel> channels;
};

// Returns a clip, or nothing for a file that is not one. A channel of a target not above keeps
// its name and has no keys.
std::optional<Clip> ClipOf(const gff4::File &animation);

} // namespace dao::core::clip
