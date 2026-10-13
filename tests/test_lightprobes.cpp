#include "dao/core/Dds.h"
#include "dao/core/Erf.h"
#include "dao/core/LightProbes.h"
#include "dao/core/RayCast.h"

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

using Catch::Approx;
using dao::core::Point;
using dao::core::RayHit;
using dao::core::Triangle;
using dao::core::TriangleTree;
namespace probes = dao::core::probes;
namespace archive = dao::core::erf;
namespace dds = dao::core::dds;

namespace
{

template <class T> void Put(std::vector<std::byte> &to, const T &value)
{
    const auto *bytes = reinterpret_cast<const std::byte *>(&value);
    to.insert(to.end(), bytes, bytes + sizeof(T));
}

// A box of two triangles a side, its inside the room: x and y from -half to
// half, z from 0 to `height`. Returned floor first (triangles 0 and 1), then
// the ceiling, then the four walls.
std::vector<Triangle> Box(float half, float height)
{
    const auto quad = [](Point a, Point b, Point c, Point d) {
        return std::array<Triangle, 2>{Triangle{a, b, c}, Triangle{a, c, d}};
    };
    std::vector<Triangle> out;
    const auto add = [&](const std::array<Triangle, 2> &pair) { out.insert(out.end(), pair.begin(), pair.end()); };
    const float h = half;
    add(quad({-h, -h, 0}, {h, -h, 0}, {h, h, 0}, {-h, h, 0}));
    add(quad({-h, -h, height}, {-h, h, height}, {h, h, height}, {h, -h, height}));
    add(quad({-h, -h, 0}, {-h, h, 0}, {-h, h, height}, {-h, -h, height}));
    add(quad({h, -h, 0}, {h, -h, height}, {h, h, height}, {h, h, 0}));
    add(quad({-h, -h, 0}, {-h, -h, height}, {h, -h, height}, {h, -h, 0}));
    add(quad({-h, h, 0}, {h, h, 0}, {h, h, height}, {-h, h, height}));
    return out;
}

// A place baked against one tree, each triangle's colour given by `seen`.
template <class Seen>
std::optional<probes::Coefficients> Baked(const Point &place, const TriangleTree &tree, const Seen &seen)
{
    const std::vector<Point> rays = probes::Directions(384);
    return probes::Bake(rays, [&](const Point &ray) -> std::optional<probes::Landed> {
        const std::optional<RayHit> hit = tree.Cast(place, ray);
        if (!hit)
        {
            return std::nullopt;
        }
        return probes::Landed{seen(*hit), hit->distance};
    });
}

// The light a surface facing `n` receives of a probe, over pi.
float Lit(const probes::Irradiance &light, std::size_t channel, const Point &n)
{
    const std::array<float, 4> v{n[0], n[1], n[2], 1};
    float sum = 0;
    for (std::size_t row = 0; row < 4; ++row)
    {
        for (std::size_t column = 0; column < 4; ++column)
        {
            sum += v.at(row) * light.at((channel * 16) + (row * 4) + column) * v.at(column);
        }
    }
    return sum;
}

} // namespace

TEST_CASE("an archive's directory is read, and no more than the bytes hold", "[erf]")
{
    std::vector<std::byte> file;
    for (const char16_t letter : std::u16string(u"ERF V2.0"))
    {
        Put(file, letter);
    }
    Put(file, std::uint32_t{2});
    file.resize(archive::kHeaderSize);
    for (const auto &[name, offset, size] :
         {std::tuple{std::u16string(u"a.mmh"), 176U, 10U}, std::tuple{std::u16string(u"room_1.rml"), 186U, 20U}})
    {
        std::array<char16_t, 32> padded{};
        std::ranges::copy(name, padded.begin());
        Put(file, padded);
        Put(file, offset);
        Put(file, size);
    }
    REQUIRE(archive::EntryCount(file) == 2);
    const std::vector<archive::Entry> entries = archive::Entries(file);
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].name == L"a.mmh");
    CHECK(entries[1].name == L"room_1.rml");
    CHECK(entries[1].offset == 186);
    CHECK(entries[1].size == 20);

    // Cut in the second entry: the first alone. Not an archive: none.
    CHECK(archive::Entries(std::span(file).first(archive::kHeaderSize + archive::kEntrySize + 10)).size() == 1);
    file[0] = std::byte{'X'};
    CHECK_FALSE(archive::EntryCount(file).has_value());
    CHECK(archive::Entries(file).empty());
}

TEST_CASE("a DXT5 block gives its two colours and its alphas", "[dds]")
{
    std::vector<std::byte> file(128);
    const std::uint32_t magic = 0x20534444;
    const std::uint32_t side = 4;
    const std::uint32_t dxt5 = 0x35545844;
    std::memcpy(file.data(), &magic, 4);
    std::memcpy(file.data() + 12, &side, 4);
    std::memcpy(file.data() + 16, &side, 4);
    std::memcpy(file.data() + 84, &dxt5, 4);
    // Alphas 255 and 0, every pixel the first; colours pure red and pure
    // blue, the first row the first, the second row the second, the rest
    // the two made of them.
    Put(file, std::uint8_t{255});
    Put(file, std::uint8_t{0});
    file.insert(file.end(), 6, std::byte{0});
    Put(file, std::uint16_t{0xF800});
    Put(file, std::uint16_t{0x001F});
    Put(file, std::uint8_t{0b00000000});
    Put(file, std::uint8_t{0b01010101});
    Put(file, std::uint8_t{0b10101010});
    Put(file, std::uint8_t{0b11111111});

    const std::optional<dds::Image> image = dds::Decode(file);
    REQUIRE(image.has_value());
    REQUIRE(image->width == 4);
    REQUIRE(image->rgba.size() == 64);
    const auto pixel = [&](std::size_t x, std::size_t y) {
        return std::array<int, 4>{image->rgba[((y * 4) + x) * 4], image->rgba[(((y * 4) + x) * 4) + 1],
                                  image->rgba[(((y * 4) + x) * 4) + 2], image->rgba[(((y * 4) + x) * 4) + 3]};
    };
    CHECK(pixel(0, 0) == std::array<int, 4>{255, 0, 0, 255});
    CHECK(pixel(3, 1) == std::array<int, 4>{0, 0, 255, 255});
    CHECK(pixel(1, 2) == std::array<int, 4>{170, 0, 85, 255});
    CHECK(pixel(2, 3) == std::array<int, 4>{85, 0, 170, 255});

    // A picture the bytes stop short of is none.
    file.pop_back();
    CHECK_FALSE(dds::Decode(file).has_value());
}

namespace
{
// A DDS file's header for pictures that are not packed: so many bits a pixel, the largest
// `side` pixels a side, and `pictures` of them.
std::vector<std::byte> PlainDds(std::uint32_t side, std::uint32_t bits, std::uint32_t pictures)
{
    std::vector<std::byte> file(128);
    const std::uint32_t magic = 0x20534444;
    std::memcpy(file.data(), &magic, 4);
    std::memcpy(file.data() + 12, &side, 4);
    std::memcpy(file.data() + 16, &side, 4);
    std::memcpy(file.data() + 28, &pictures, 4);
    std::memcpy(file.data() + 88, &bits, 4);
    return file;
}
} // namespace

TEST_CASE("a picture of 24 bits is read, blue first, every pixel opaque", "[dds]")
{
    std::vector<std::byte> file = PlainDds(2, 24, 1);
    // Blue, green, red: a red pixel, a green, a blue, and one of 10, 20, 30.
    for (const int byte : {0, 0, 255, 0, 255, 0, 255, 0, 0, 30, 20, 10})
    {
        Put(file, static_cast<std::uint8_t>(byte));
    }
    const std::optional<dds::Image> image = dds::Decode(file);
    REQUIRE(image.has_value());
    REQUIRE(image->width == 2);
    REQUIRE(image->height == 2);
    CHECK(image->rgba == std::vector<std::uint8_t>{255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 10, 20, 30, 255});

    // Bytes that stop short of the picture, and bits this does not read.
    file.pop_back();
    CHECK_FALSE(dds::Decode(file).has_value());
    CHECK_FALSE(dds::Decode(PlainDds(2, 16, 1)).has_value());
}

TEST_CASE("a picture no larger than a side is the one given, of a file's chain", "[dds]")
{
    // Three pictures of 32 bits: 4 by 4 of 1s, 2 by 2 of 2s, 1 by 1 of 3s.
    std::vector<std::byte> file = PlainDds(4, 32, 3);
    file.insert(file.end(), 4 * 4 * 4, std::byte{1});
    file.insert(file.end(), 2 * 2 * 4, std::byte{2});
    file.insert(file.end(), 1 * 1 * 4, std::byte{3});
    const auto sideAndFirst = [&](std::uint32_t sideMost) {
        const std::optional<dds::Image> image = dds::Decode(file, sideMost);
        REQUIRE(image.has_value());
        REQUIRE(image->rgba.size() == std::size_t{image->width} * image->height * 4);
        return std::pair<std::uint32_t, int>(image->width, image->rgba[0]);
    };
    CHECK(sideAndFirst(0) == std::pair<std::uint32_t, int>(4, 1)); // the largest, unasked
    CHECK(sideAndFirst(4) == std::pair<std::uint32_t, int>(4, 1));
    CHECK(sideAndFirst(3) == std::pair<std::uint32_t, int>(2, 2));
    CHECK(sideAndFirst(2) == std::pair<std::uint32_t, int>(2, 2));
    CHECK(sideAndFirst(1) == std::pair<std::uint32_t, int>(1, 3));

    // A file with one picture has no smaller to give: its own, whatever is asked.
    std::vector<std::byte> one = PlainDds(4, 32, 1);
    one.insert(one.end(), 4 * 4 * 4, std::byte{7});
    const std::optional<dds::Image> only = dds::Decode(one, 1);
    REQUIRE(only.has_value());
    CHECK(only->width == 4);

    // A chain that says more pictures than the bytes hold: the smallest that is whole.
    file.resize(128 + (4 * 4 * 4) + 3);
    CHECK(dds::Decode(file, 0).has_value());
    const std::optional<dds::Image> whole = dds::Decode(file, 2);
    REQUIRE(whole.has_value());
    CHECK(whole->width == 4);
}

TEST_CASE("a file's smaller pictures are found by its bytes, whatever its header counts", "[dds]")
{
    // The game's beckmann.dds: marked as having smaller pictures, holding them, counting one.
    std::vector<std::byte> file = PlainDds(4, 24, 1);
    file.insert(file.end(), 4 * 4 * 3, std::byte{1});
    file.insert(file.end(), 2 * 2 * 3, std::byte{2});
    file.insert(file.end(), 1 * 1 * 3, std::byte{3});
    const std::optional<dds::Image> half = dds::Decode(file, 2);
    REQUIRE(half.has_value());
    CHECK(half->width == 2);
    CHECK(half->rgba[0] == 2);
    const std::optional<dds::Image> least = dds::Decode(file, 1);
    REQUIRE(least.has_value());
    CHECK(least->width == 1);
    CHECK(least->rgba[0] == 3);

    // A cube map's bytes after its first picture are its other faces: not a smaller picture.
    const std::uint32_t cube = 0x200;
    std::memcpy(file.data() + 112, &cube, 4);
    const std::optional<dds::Image> face = dds::Decode(file, 1);
    REQUIRE(face.has_value());
    CHECK(face->width == 4);
    CHECK(face->rgba[0] == 1);
}

TEST_CASE("a file is described without decoding: format, size, pictures as its bytes hold them, shape", "[dds]")
{
    // The beckmann.dds case: three pictures held, one counted by the header.
    std::vector<std::byte> file = PlainDds(4, 24, 1);
    file.insert(file.end(), (4 * 4 * 3) + (2 * 2 * 3) + (1 * 1 * 3), std::byte{1});
    std::optional<dds::Description> about = dds::Describe(file);
    REQUIRE(about.has_value());
    CHECK(about->format == "B8G8R8");
    CHECK(about->width == 4);
    CHECK(about->height == 4);
    CHECK(about->pictures == 3);
    CHECK(about->headerPictures == 1);
    CHECK(about->shape == dds::Shape::Flat);
    CHECK(about->decodes);

    // Cut into the smallest: two whole pictures.
    file.pop_back();
    about = dds::Describe(file);
    REQUIRE(about.has_value());
    CHECK(about->pictures == 2);

    // A cube map: its first picture only, the rest being faces.
    const std::uint32_t cube = 0x200;
    std::memcpy(file.data() + 112, &cube, 4);
    about = dds::Describe(file);
    REQUIRE(about.has_value());
    CHECK(about->shape == dds::Shape::Cube);
    CHECK(about->pictures == 1);
    const std::uint32_t volume = 0x200000;
    std::memcpy(file.data() + 112, &volume, 4);
    CHECK(dds::Describe(file)->shape == dds::Shape::Volume);

    // DXT5 of one block: its code, one picture.
    std::vector<std::byte> dxt(128 + 16);
    const std::uint32_t magic = 0x20534444;
    const std::uint32_t side = 4;
    const std::uint32_t dxt5 = 0x35545844;
    std::memcpy(dxt.data(), &magic, 4);
    std::memcpy(dxt.data() + 12, &side, 4);
    std::memcpy(dxt.data() + 16, &side, 4);
    std::memcpy(dxt.data() + 84, &dxt5, 4);
    about = dds::Describe(dxt);
    REQUIRE(about.has_value());
    CHECK(about->format == "DXT5");
    CHECK(about->pictures == 1);
    CHECK(about->decodes);

    // A first picture the bytes stop short of: described, none counted, not decoded.
    dxt.pop_back();
    about = dds::Describe(dxt);
    REQUIRE(about.has_value());
    CHECK(about->pictures == 0);
    CHECK_FALSE(dds::Decode(dxt).has_value());

    // A format Decode does not read is still described, by its code or its bits.
    const std::uint32_t ati2 = 0x32495441; // "ATI2"
    std::memcpy(dxt.data() + 84, &ati2, 4);
    about = dds::Describe(dxt);
    REQUIRE(about.has_value());
    CHECK(about->format == "ATI2");
    CHECK_FALSE(about->decodes);
    CHECK(about->pictures == 0);
    about = dds::Describe(PlainDds(2, 16, 1));
    REQUIRE(about.has_value());
    CHECK(about->format == "16 bits");
    CHECK_FALSE(about->decodes);

    // Not a DDS, or one with no size.
    CHECK_FALSE(dds::Describe({}).has_value());
    CHECK_FALSE(dds::Describe(PlainDds(0, 32, 1)).has_value());
}

TEST_CASE("a ray meets the nearest triangle, from either side, and the tree agrees with trying each", "[raycast]")
{
    const std::vector<Triangle> box = Box(5, 4);
    const TriangleTree tree(box);

    // Straight down from the middle: the floor, a metre and a half away.
    const std::optional<RayHit> down = tree.Cast({1, 1, 1.5F}, {0, 0, -1});
    REQUIRE(down.has_value());
    CHECK(down->triangle < 2);
    CHECK(down->distance == Approx(1.5F));
    // From outside, through a wall met from behind, the wall is still met first.
    const std::optional<RayHit> through = tree.Cast({-20, 0, 2}, {1, 0, 0});
    REQUIRE(through.has_value());
    CHECK(through->distance == Approx(15.0F));
    // Away from everything, and no further than asked.
    CHECK_FALSE(tree.Cast({0, 0, 10}, {0, 0, 1}).has_value());
    CHECK_FALSE(tree.Cast({1, 1, 1.5F}, {0, 0, 1}, 1e-3F, 2.0F).has_value());

    // A few thousand triangles scattered about, and rays every way: what the
    // tree answers is what the triangles tried one at a time answer.
    std::vector<Triangle> many;
    std::uint32_t seed = 12345;
    const auto next = [&] {
        seed = (seed * 1664525U) + 1013904223U;
        return static_cast<float>(seed >> 8) / static_cast<float>(1U << 24);
    };
    for (int i = 0; i < 3000; ++i)
    {
        const Point at{(next() * 40) - 20, (next() * 40) - 20, (next() * 40) - 20};
        many.push_back({at,
                        {at[0] + (next() * 2) - 1, at[1] + (next() * 2) - 1, at[2] + (next() * 2) - 1},
                        {at[0] + (next() * 2) - 1, at[1] + (next() * 2) - 1, at[2] + (next() * 2) - 1}});
    }
    const TriangleTree scattered(many);
    for (const Point &ray : probes::Directions(200))
    {
        std::optional<float> nearest;
        for (const Triangle &one : many)
        {
            const std::optional<RayHit> hit = TriangleTree(std::span(&one, 1)).Cast({0.3F, -0.2F, 0.1F}, ray);
            if (hit && (!nearest || hit->distance < *nearest))
            {
                nearest = hit->distance;
            }
        }
        const std::optional<RayHit> found = scattered.Cast({0.3F, -0.2F, 0.1F}, ray);
        REQUIRE(found.has_value() == nearest.has_value());
        if (found)
        {
            CHECK(found->distance == Approx(*nearest));
        }
    }
}

TEST_CASE("the directions are spread evenly and the harmonics are of unit length each", "[lightprobes]")
{
    const std::vector<Point> directions = probes::Directions(2000);
    Point sum{};
    std::array<double, 9> squares{};
    for (const Point &d : directions)
    {
        CHECK(std::hypot(d[0], d[1], d[2]) == Approx(1.0F).margin(1e-5));
        const std::array<float, 9> harmonics = probes::Harmonics(d);
        for (std::size_t i = 0; i < 3; ++i)
        {
            sum.at(i) += d.at(i);
        }
        for (std::size_t i = 0; i < 9; ++i)
        {
            squares.at(i) += static_cast<double>(harmonics.at(i)) * harmonics.at(i);
        }
    }
    CHECK(std::hypot(sum[0], sum[1], sum[2]) < 2.0F);
    for (const double square : squares)
    {
        // Each harmonic squared, over the sphere, is 1.
        CHECK(square * 4 * 3.14159265 / 2000 == Approx(1.0).margin(0.01));
    }
}

TEST_CASE("a room of one colour bakes to that colour on every side", "[lightprobes]")
{
    const TriangleTree tree(Box(5, 4));
    const auto grey = [](const RayHit &) { return probes::Colour{0.25F, 0.5F, 0.75F}; };
    const std::optional<probes::Coefficients> light = Baked({0.5F, -1, 1.5F}, tree, grey);
    REQUIRE(light.has_value());
    const probes::Irradiance matrices = probes::Matrices(*light);
    for (const Point &n : {Point{0, 0, 1}, Point{0, 0, -1}, Point{1, 0, 0}, Point{0.6F, -0.8F, 0}})
    {
        CHECK(Lit(matrices, 0, n) == Approx(0.25F).margin(0.01));
        CHECK(Lit(matrices, 1, n) == Approx(0.5F).margin(0.01));
        CHECK(Lit(matrices, 2, n) == Approx(0.75F).margin(0.01));
    }
}

TEST_CASE("a lit floor lights a figure's top, not its underside", "[lightprobes]")
{
    // The floor's lightmap is bright and all else dark: the light is from above.
    const TriangleTree tree(Box(5, 4));
    const auto floorLit = [](const RayHit &hit) {
        return hit.triangle < 2 ? probes::Colour{1, 1, 1} : probes::Colour{0, 0, 0};
    };
    const std::optional<probes::Coefficients> light = Baked({0, 0, 1.5F}, tree, floorLit);
    REQUIRE(light.has_value());
    const probes::Irradiance matrices = probes::Matrices(*light);
    CHECK(Lit(matrices, 0, {0, 0, 1}) > 0.5F);
    CHECK(Lit(matrices, 0, {0, 0, -1}) < 0.1F);
    CHECK(Lit(matrices, 0, {1, 0, 0}) < Lit(matrices, 0, {0, 0, 1}));
}

TEST_CASE("a place inside a pillar is no place for a probe", "[lightprobes]")
{
    // A pillar 0.4 m across: every ray from within lands at once.
    const TriangleTree pillar(Box(0.2F, 4));
    const auto any = [](const RayHit &) { return probes::Colour{1, 1, 1}; };
    CHECK_FALSE(Baked({0, 0, 1}, pillar, any).has_value());
}

TEST_CASE("probes are placed over the floor, above a table's top, and not where a ceiling is low", "[lightprobes]")
{
    // A room 10 m by 10 m, 4 m high, and in it a slab 1 m up over x < 0 with
    // no room to stand under it: the floor there is passed over for the slab.
    std::vector<Triangle> room = Box(5, 4);
    room.push_back({{-5, -5, 1}, {0, -5, 1}, {0, 5, 1}});
    room.push_back({{-5, -5, 1}, {0, 5, 1}, {-5, 5, 1}});
    const TriangleTree tree(room);
    const std::vector<Point> places = probes::FloorPlaces(tree, 5.0F);
    REQUIRE(places.size() == 4);
    for (const Point &place : places)
    {
        CHECK(place[2] == Approx(place[0] < 0 ? 2.0F : 1.0F).margin(1e-3));
    }
    // A room too low to stand in has none.
    CHECK(probes::FloorPlaces(TriangleTree(Box(5, 1.5F)), 5.0F).empty());
}

TEST_CASE("a mix is the nearest probe's at the probe and never steps as probes come and go", "[lightprobes]")
{
    std::array<probes::Irradiance, 6> lights{};
    for (std::size_t i = 0; i < lights.size(); ++i)
    {
        lights.at(i).fill(static_cast<float>(i + 1));
    }
    const auto mixed = [&](std::array<float, 6> distances) {
        std::vector<probes::Near> near;
        near.reserve(distances.size());
        for (std::size_t i = 0; i < distances.size(); ++i)
        {
            near.push_back({1 / distances.at(i), lights.at(i).data()});
        }
        return probes::Mix(near)->front();
    };
    // Hard by the first probe it is the first's.
    CHECK(mixed({0.01F, 5, 6, 7, 8, 9}) == Approx(1.0F).margin(0.01));
    // The fourth and the fifth nearest change places: nothing steps.
    const float before = mixed({1, 2, 3, 3.99F, 4.01F, 9});
    const float after = mixed({1, 2, 3, 4.01F, 3.99F, 9});
    CHECK(before == Approx(after).margin(0.01));
    // One probe alone is itself; none is nothing.
    std::vector<probes::Near> one{{0.2F, lights[2].data()}};
    CHECK(probes::Mix(one)->front() == Approx(3.0F));
    CHECK_FALSE(probes::Mix({}).has_value());
}

TEST_CASE("a table's file is read back as it was written, and not if cut short", "[lightprobes]")
{
    std::vector<probes::Room> rooms(2);
    rooms[0].key = {1, 2, 3};
    rooms[0].probes.resize(2);
    rooms[0].probes[1].at = {4, 5, 6};
    rooms[0].probes[1].light[47] = 0.5F;
    rooms[1].key = {7, 8, 9};
    const std::vector<std::byte> file = probes::Write(rooms);
    const std::optional<std::vector<probes::Room>> read = probes::Read(file);
    REQUIRE(read.has_value());
    REQUIRE(read->size() == 2);
    CHECK((*read)[0].key == Point{1, 2, 3});
    REQUIRE((*read)[0].probes.size() == 2);
    CHECK((*read)[0].probes[1].at == Point{4, 5, 6});
    CHECK((*read)[0].probes[1].light[47] == 0.5F);
    CHECK((*read)[1].probes.empty());

    CHECK_FALSE(probes::Read(std::span(file).first(file.size() - 30)).has_value());
    std::vector<std::byte> other = file;
    other[0] = std::byte{'X'};
    CHECK_FALSE(probes::Read(other).has_value());
}
