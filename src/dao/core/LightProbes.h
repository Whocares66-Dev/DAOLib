// Light probes worked out from a level's own baked light, the mix of them a
// figure is given, and the file they are kept in: the arithmetic of
// examples/graphics's probes (DAOLib #65), with nothing of the game's
// process in it.
//
// A light probe is the light at a place as three 4 by 4 matrices, one each
// for red, green and blue: for a surface there facing n, (n, 1) times the
// matrix times (n, 1) is the light it receives, over pi. They are Ramamoorthi
// and Hanrahan's irradiance matrices of nine spherical-harmonics
// coefficients, over pi, which is what a level's own probes hold (checked
// against the game's files to 1.5e-8, 2026-10-11).
//
// A probe is baked from the lightmaps: a lightmap is the light a surface
// receives, the lamps' own and the bounce both, and a level's shader lays it
// on at its face value. So each side of a figure is lit as the level's
// surfaces behind it are: rays go out from the place, the lightmap is read
// where each lands, and that colour is counted as light arriving from the
// ray's opposite side. A floor lit from above so lights a figure's top.
#pragma once

#include "dao/core/RayCast.h"

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace dao::core::probes
{

using Colour = std::array<float, 3>;
// Nine coefficients, each red, green and blue, in the order 00, 1-1, 10, 11,
// 2-2, 2-1, 20, 21, 22.
using Coefficients = std::array<Colour, 9>;
// Three matrices of sixteen floats: red, green, blue.
using Irradiance = std::array<float, 48>;

// Returns `count` directions spread evenly over the sphere, each of length 1.
std::vector<Point> Directions(std::size_t count);

// Returns the nine real spherical harmonics of a direction of length 1.
std::array<float, 9> Harmonics(const Point &direction);

// Returns a probe's three matrices of its coefficients.
Irradiance Matrices(const Coefficients &light);

// Notes on Bake: the numbers are the Python prototype's that the three test
// levels were baked with and seen in play (2026-10-11): a place is closed in,
// inside a pillar or a wall, when three rays in ten land within half a
// metre; a ray that lands on nothing, through an opening, is given the mean
// of those that land.

// What a ray from a place landed on: the lightmap's colour there, and how far it is.
struct Landed
{
    Colour colour{};
    float distance = 0;
};

// Returns the light at a place as a probe's coefficients, or nothing for a
// place that is closed in.
// @param rays the directions to look in from the place, each of length 1
// @param look says what a ray lands on, or nothing if it lands on nothing
std::optional<Coefficients> Bake(std::span<const Point> rays,
                                 const std::function<std::optional<Landed>(const Point &ray)> &look);

// Returns places for probes over a room's floor: on a grid, `height` above
// the lowest surface at each point that faces up and has `headroom` of free
// air over it.
// @param room the room's own triangles
std::vector<Point> FloorPlaces(const TriangleTree &room, float spacing, float height = 1.0F, float headroom = 2.2F);

// One probe to mix: how near it is, as one over its distance, and its matrices.
struct Near
{
    float score = 0;
    const float *light = nullptr; // forty-eight floats
};

// How many of the nearest probes are mixed.
inline constexpr std::size_t kMixed = 4;

// Returns the mix of the nearest few probes, each by the square of how far
// its score is above the next one's beyond them: a probe has no part as it
// comes into the nearest few or leaves them, so a figure's light never
// steps as it walks.
// @param probes sorted by this, nearest first; a score is taken as at most
// `scoreMost`, so a probe a hair away does not drown the rest
// @return nullopt for no probes
std::optional<Irradiance> Mix(std::span<Near> probes, float scoreMost = 20.0F);

// A probe of the table, and a room's worth: the room is known by the place
// of the probe the level itself has there.
struct Probe
{
    Point at{};
    Irradiance light{};
};
struct Room
{
    Point key{};
    std::vector<Probe> probes;
};

// Notes on the file: "DAOP", a version of 1 and the number of rooms as 32
// bits each; then for each room its key's place as three floats and the
// number of its probes, and for each probe its place and its forty-eight
// floats. All little-endian.

// Returns a table's file.
std::vector<std::byte> Write(std::span<const Room> rooms);

// Returns the rooms of a table's file.
// @return nullopt for bytes that are not such a file or stop short
std::optional<std::vector<Room>> Read(std::span<const std::byte> file);

} // namespace dao::core::probes
