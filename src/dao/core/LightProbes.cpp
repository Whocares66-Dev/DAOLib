#include "dao/core/LightProbes.h"

#include "dao/core/Bytes.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

namespace dao::core::probes
{

namespace
{

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kNearWall = 0.5F;            // metres: a ray that lands nearer has landed on what the place is in
constexpr float kNearWallShare = 0.3F;       // of the rays: more and the place is closed in
constexpr float kFacesUp = 0.7F;             // of a normal's length upward: a floor
constexpr float kSameFloor = 0.2F;           // metres: surfaces nearer than this in height are one floor
constexpr std::uint32_t kMagic = 0x504F4144; // "DAOP"
constexpr std::uint32_t kVersion = 1;
constexpr std::size_t kLayersMost = 64; // surfaces looked through under one point of the grid

template <class T> void Put(std::vector<std::byte> &to, const T &value)
{
    const auto *bytes = reinterpret_cast<const std::byte *>(&value);
    to.insert(to.end(), bytes, bytes + sizeof(T));
}

} // namespace

std::vector<Point> Directions(std::size_t count)
{
    // A Fibonacci spiral.
    std::vector<Point> directions(count);
    const float turn = kPi * (1 + std::sqrt(5.0F));
    for (std::size_t i = 0; i < count; ++i)
    {
        const float z = 1 - (2 * (static_cast<float>(i) + 0.5F) / static_cast<float>(count));
        const float around = turn * (static_cast<float>(i) + 0.5F);
        const float r = std::sqrt(std::max(0.0F, 1 - (z * z)));
        directions[i] = {r * std::cos(around), r * std::sin(around), z};
    }
    return directions;
}

std::array<float, 9> Harmonics(const Point &d)
{
    const float x = d[0];
    const float y = d[1];
    const float z = d[2];
    return {0.282095F,
            0.488603F * y,
            0.488603F * z,
            0.488603F * x,
            1.092548F * x * y,
            1.092548F * y * z,
            0.315392F * ((3 * z * z) - 1),
            1.092548F * x * z,
            0.546274F * ((x * x) - (y * y))};
}

Irradiance Matrices(const Coefficients &light)
{
    constexpr float c1 = 0.429043F;
    constexpr float c2 = 0.511664F;
    constexpr float c3 = 0.743125F;
    constexpr float c4 = 0.886227F;
    constexpr float c5 = 0.247708F;
    Irradiance out{};
    for (std::size_t channel = 0; channel < 3; ++channel)
    {
        const float l00 = light[0][channel];
        const float l1m1 = light[1][channel];
        const float l10 = light[2][channel];
        const float l11 = light[3][channel];
        const float l2m2 = light[4][channel];
        const float l2m1 = light[5][channel];
        const float l20 = light[6][channel];
        const float l21 = light[7][channel];
        const float l22 = light[8][channel];
        const std::array<float, 16> matrix{
            c1 * l22, c1 * l2m2, c1 * l21, c2 * l11, c1 * l2m2, -c1 * l22, c1 * l2m1, c2 * l1m1,
            c1 * l21, c1 * l2m1, c3 * l20, c2 * l10, c2 * l11,  c2 * l1m1, c2 * l10,  (c4 * l00) - (c5 * l20)};
        for (std::size_t i = 0; i < matrix.size(); ++i)
        {
            out.at((channel * 16) + i) = matrix.at(i) / kPi;
        }
    }
    return out;
}

std::optional<Coefficients> Bake(std::span<const Point> rays,
                                 const std::function<std::optional<Landed>(const Point &ray)> &look)
{
    std::vector<Colour> colours(rays.size());
    std::vector<bool> landed(rays.size());
    Colour mean{};
    std::size_t hits = 0;
    std::size_t near = 0;
    for (std::size_t i = 0; i < rays.size(); ++i)
    {
        const std::optional<Landed> hit = look(rays[i]);
        if (!hit)
        {
            continue;
        }
        landed[i] = true;
        colours[i] = hit->colour;
        ++hits;
        near += hit->distance < kNearWall ? 1 : 0;
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            mean.at(channel) += colours[i].at(channel);
        }
    }
    if (rays.empty() || static_cast<float>(near) > kNearWallShare * static_cast<float>(rays.size()))
    {
        return std::nullopt;
    }
    for (float &value : mean)
    {
        value = hits != 0 ? value / static_cast<float>(hits) : 0;
    }
    Coefficients light{};
    const float each = 4 * kPi / static_cast<float>(rays.size());
    for (std::size_t i = 0; i < rays.size(); ++i)
    {
        // The colour seen along a ray is the light from the ray's other side.
        const std::array<float, 9> harmonics = Harmonics({-rays[i][0], -rays[i][1], -rays[i][2]});
        const Colour &colour = landed[i] ? colours[i] : mean;
        for (std::size_t term = 0; term < light.size(); ++term)
        {
            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                light.at(term).at(channel) += harmonics.at(term) * colour.at(channel) * each;
            }
        }
    }
    return light;
}

namespace
{

// The box the room's triangles are in.
std::pair<Point, Point> BoundsOf(const TriangleTree &room)
{
    Point low = room.At(0).a;
    Point high = low;
    for (std::size_t i = 0; i < room.Size(); ++i)
    {
        const Triangle &t = room.At(i);
        for (const Point *corner : {&t.a, &t.b, &t.c})
        {
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                low.at(axis) = std::min(low.at(axis), (*corner).at(axis));
                high.at(axis) = std::max(high.at(axis), (*corner).at(axis));
            }
        }
    }
    return {low, high};
}

// The heights of every surface under (x, y) that faces up, from `top` down, sorted.
std::vector<float> FloorsUnder(const TriangleTree &room, float x, float y, float top)
{
    std::vector<float> floors;
    Point from{x, y, top};
    for (std::size_t layer = 0; layer < kLayersMost; ++layer)
    {
        const std::optional<RayHit> hit = room.Cast(from, {0, 0, -1});
        if (!hit)
        {
            break;
        }
        const Triangle &t = room.At(hit->triangle);
        const Point e1{t.b[0] - t.a[0], t.b[1] - t.a[1], t.b[2] - t.a[2]};
        const Point e2{t.c[0] - t.a[0], t.c[1] - t.a[1], t.c[2] - t.a[2]};
        const Point normal{(e1[1] * e2[2]) - (e1[2] * e2[1]), (e1[2] * e2[0]) - (e1[0] * e2[2]),
                           (e1[0] * e2[1]) - (e1[1] * e2[0])};
        const float length = std::hypot(normal[0], normal[1], normal[2]);
        from[2] -= hit->distance;
        if (normal[2] > kFacesUp * length)
        {
            floors.push_back(from[2]);
        }
    }
    std::ranges::sort(floors);
    return floors;
}

// The lowest of the floors with room to stand over it, taking those a step apart or less for
// one; none if there is no such floor.
std::optional<float> FloorToStandOn(const TriangleTree &room, float x, float y, const std::vector<float> &floors,
                                    float lowest, float headroom)
{
    float last = lowest - (2 * kSameFloor);
    for (const float floor : floors)
    {
        if (floor - last < kSameFloor)
        {
            continue;
        }
        last = floor;
        if (!room.Cast({x, y, floor + 0.1F}, {0, 0, 1}, 1e-3F, headroom))
        {
            return floor;
        }
    }
    return std::nullopt;
}

} // namespace

std::vector<Point> FloorPlaces(const TriangleTree &room, float spacing, float height, float headroom)
{
    std::vector<Point> places;
    if (room.Size() == 0 || spacing <= 0)
    {
        return places;
    }
    const auto [low, high] = BoundsOf(room);
    // A place every `spacing` from half of it in, in x and in y: counted, so that the places
    // are where the count puts them and not where a sum of steps has drifted to.
    const auto along = [&](std::size_t axis, std::size_t index) {
        return low.at(axis) + (spacing * (static_cast<float>(index) + 0.5F));
    };
    for (std::size_t column = 0; along(0, column) < high[0]; ++column)
    {
        for (std::size_t row = 0; along(1, row) < high[1]; ++row)
        {
            const float x = along(0, column);
            const float y = along(1, row);
            const std::vector<float> floors = FloorsUnder(room, x, y, high[2] + 1);
            if (const auto floor = FloorToStandOn(room, x, y, floors, low[2], headroom))
            {
                places.push_back({x, y, *floor + height});
            }
        }
    }
    return places;
}

std::optional<Irradiance> Mix(std::span<Near> probes, float scoreMost)
{
    if (probes.empty())
    {
        return std::nullopt;
    }
    for (Near &probe : probes)
    {
        probe.score = std::min(probe.score, scoreMost);
    }
    std::ranges::sort(probes, std::greater{}, &Near::score);
    const std::size_t count = std::min(probes.size(), kMixed);
    const float beyond = probes.size() > count ? probes[count].score : 0.0F;
    Irradiance mix{};
    float whole = 0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const float weight = (probes[i].score - beyond) * (probes[i].score - beyond);
        for (std::size_t at = 0; at < mix.size(); ++at)
        {
            mix.at(at) += weight * probes[i].light[at];
        }
        whole += weight;
    }
    if (whole <= 0)
    {
        // The nearest few all as far as the next: the nearest alone.
        std::copy_n(probes.front().light, mix.size(), mix.begin());
        return mix;
    }
    for (float &value : mix)
    {
        value /= whole;
    }
    return mix;
}

std::vector<std::byte> Write(std::span<const Room> rooms)
{
    std::vector<std::byte> file;
    Put(file, kMagic);
    Put(file, kVersion);
    Put(file, static_cast<std::uint32_t>(rooms.size()));
    for (const Room &room : rooms)
    {
        Put(file, room.key);
        Put(file, static_cast<std::uint32_t>(room.probes.size()));
        for (const Probe &probe : room.probes)
        {
            Put(file, probe.at);
            Put(file, probe.light);
        }
    }
    return file;
}

std::optional<std::vector<Room>> Read(std::span<const std::byte> file)
{
    if (ReadAt<std::uint32_t>(file, 0) != kMagic || ReadAt<std::uint32_t>(file, 4) != kVersion)
    {
        return std::nullopt;
    }
    const std::uint32_t count = ReadAt<std::uint32_t>(file, 8).value_or(0);
    std::size_t at = 12;
    std::vector<Room> rooms;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        Room room;
        const std::optional<Point> key = ReadAt<Point>(file, at);
        const std::optional<std::uint32_t> probes = ReadAt<std::uint32_t>(file, at + sizeof(Point));
        at += sizeof(Point) + sizeof(std::uint32_t);
        if (!key || !probes || *probes > (file.size() - at) / (sizeof(Point) + sizeof(Irradiance)))
        {
            return std::nullopt;
        }
        room.key = *key;
        room.probes.resize(*probes);
        for (Probe &probe : room.probes)
        {
            probe.at = ReadAt<Point>(file, at).value_or(Point{});
            probe.light = ReadAt<Irradiance>(file, at + sizeof(Point)).value_or(Irradiance{});
            at += sizeof(Point) + sizeof(Irradiance);
        }
        rooms.push_back(std::move(room));
    }
    return rooms;
}

} // namespace dao::core::probes
