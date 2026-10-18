#include "dao/core/Bytes.h"
#include "dao/core/Dds.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

namespace dds = dao::core::dds;
using dao::core::ReadAt;

namespace
{

// A picture of a size whose every pixel is the same colour.
dds::Image Solid(std::uint32_t width, std::uint32_t height, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                 std::uint8_t a)
{
    dds::Image image{.width = width, .height = height, .rgba = {}};
    for (std::uint32_t i = 0; i < width * height; ++i)
    {
        image.rgba.insert(image.rgba.end(), {r, g, b, a});
    }
    return image;
}

// An 8 by 4 picture: red its x times 30, green its y times 60, blue 7, alpha 200 less its x.
dds::Image Ramp()
{
    dds::Image image{.width = 8, .height = 4, .rgba = {}};
    for (std::uint8_t y = 0; y < 4; ++y)
    {
        for (std::uint8_t x = 0; x < 8; ++x)
        {
            image.rgba.insert(image.rgba.end(), {static_cast<std::uint8_t>(x * 30), static_cast<std::uint8_t>(y * 60),
                                                 std::uint8_t{7}, static_cast<std::uint8_t>(200 - x)});
        }
    }
    return image;
}

bool Near(const std::vector<std::uint8_t> &a, const std::vector<std::uint8_t> &b, int within)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (std::abs(int{a[i]} - int{b[i]}) > within)
        {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("a texture is written with the game's header and every halving down to a pixel", "[dds]")
{
    for (const auto &[format, name, first] :
         {std::tuple{dds::Format::Dxt1, "DXT1", 16U}, std::tuple{dds::Format::Dxt5, "DXT5", 32U},
          std::tuple{dds::Format::Dxt5Normal, "DXT5", 32U}, std::tuple{dds::Format::A4R4G4B4, "A4R4G4B4", 64U},
          std::tuple{dds::Format::A8R8G8B8, "B8G8R8A8", 128U}})
    {
        const std::vector<std::byte> file = dds::Encode(Ramp(), format);
        INFO(name);
        const std::optional<dds::Description> description = dds::Describe(file);
        REQUIRE(description);
        CHECK(description->format == name);
        CHECK(description->width == 8);
        CHECK(description->height == 4);
        CHECK(description->pictures == 4); // 8 by 4, 4 by 2, 2 by 1, 1 by 1
        CHECK(description->headerPictures == 4);
        CHECK(ReadAt<std::uint32_t>(file, 8) == 0xA1007U);
        CHECK(ReadAt<std::uint32_t>(file, 20) == first); // the first picture's bytes
        CHECK(ReadAt<std::uint32_t>(file, 108) == 0x401008U);
    }
}

TEST_CASE("a texture not packed reads back as written, its halvings the averages", "[dds]")
{
    const dds::Image ramp = Ramp();
    const std::optional<dds::Image> back = dds::Decode(dds::Encode(ramp, dds::Format::A8R8G8B8));
    REQUIRE(back);
    CHECK(back->rgba == ramp.rgba);
    const std::optional<dds::Image> half = dds::Decode(dds::Encode(ramp, dds::Format::A8R8G8B8), 4);
    REQUIRE(half);
    REQUIRE(half->width == 4);
    // Its first pixel: red of x 0 and 1 (0, 30), green of y 0 and 1 (0, 60), alpha 200 and 199.
    CHECK(half->rgba[0] == 15);
    CHECK(half->rgba[1] == 30);
    CHECK(half->rgba[2] == 7);
    CHECK(half->rgba[3] == 200); // 199.5 rounded up

    // Four bits a channel: a value of 17's multiples exactly, others to the nearest.
    const dds::Image tint = Solid(4, 4, 255, 136, 0, 17);
    const std::optional<dds::Image> tinted = dds::Decode(dds::Encode(tint, dds::Format::A4R4G4B4));
    REQUIRE(tinted);
    CHECK(tinted->rgba == tint.rgba);
}

TEST_CASE("a packed texture reads back near what was written, DXT1 opaque", "[dds]")
{
    const dds::Image colour = Solid(8, 8, 200, 100, 50, 128);
    const std::optional<dds::Image> five = dds::Decode(dds::Encode(colour, dds::Format::Dxt5));
    REQUIRE(five);
    CHECK(Near(five->rgba, colour.rgba, 4));
    const std::optional<dds::Image> one = dds::Decode(dds::Encode(colour, dds::Format::Dxt1));
    REQUIRE(one);
    CHECK(Near(one->rgba, Solid(8, 8, 200, 100, 50, 255).rgba, 4)); // alpha dropped
}

TEST_CASE("a normal map is kept x in alpha and y in green, and read back to x, y, z", "[dds]")
{
    // Leaning to +x: (0.6, 0, 0.8) as 0 to 255.
    const dds::Image lean = Solid(4, 4, 204, 128, 230, 255);
    const std::vector<std::byte> file = dds::Encode(lean, dds::Format::Dxt5Normal);
    const std::optional<dds::Image> stored = dds::Decode(file);
    REQUIRE(stored);
    CHECK(std::abs(int{stored->rgba[3]} - 204) <= 2);                  // x in alpha
    CHECK(std::abs(int{stored->rgba[1]} - 128) <= 2);                  // y in green
    CHECK(std::abs(int{stored->rgba[0]} - int{stored->rgba[1]}) <= 4); // and red, as the game's are; DXT keeps
                                                                       // red in 5 bits and green in 6
    const dds::Image normals = dds::Normals(*stored);
    CHECK(Near(normals.rgba, lean.rgba, 3));
}

TEST_CASE("the game's textures in game/textures, when there, are written again with their header and size", "[dds]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "textures";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/textures in this checkout (ghilana's extract writes the game's textures there)");
    }
    std::size_t textures = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        std::ifstream in(entry.path(), std::ios::binary);
        const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        std::vector<std::byte> file(chars.size());
        std::memcpy(file.data(), chars.data(), chars.size());
        const std::optional<dds::Description> description = dds::Describe(file);
        const std::optional<dds::Image> image = dds::Decode(file);
        INFO(entry.path().filename().string());
        REQUIRE(description);
        REQUIRE(image);
        // A normal map by its name, "..._0n": the game's own way of telling one.
        const bool normal = entry.path().stem().string().ends_with("n");
        dds::Format format = dds::Format::A8R8G8B8;
        if (description->format == "DXT1")
        {
            format = dds::Format::Dxt1;
        }
        else if (description->format == "DXT5")
        {
            format = normal ? dds::Format::Dxt5Normal : dds::Format::Dxt5;
        }
        else if (description->format == "A4R4G4B4")
        {
            format = dds::Format::A4R4G4B4;
        }
        const std::vector<std::byte> again = dds::Encode(normal ? dds::Normals(*image) : *image, format);
        CHECK(again.size() == file.size());
        CHECK(std::equal(again.begin(), again.begin() + 128, file.begin()));
        ++textures;
    }
    CHECK(textures > 0);
}

TEST_CASE("a texture is not written whose sides are not powers of two or whose pixels do not fit", "[dds]")
{
    CHECK(dds::Encode(Solid(6, 4, 0, 0, 0, 0), dds::Format::Dxt1).empty());
    dds::Image short_ = Solid(4, 4, 0, 0, 0, 0);
    short_.rgba.pop_back();
    CHECK(dds::Encode(short_, dds::Format::A8R8G8B8).empty());
}

TEST_CASE("a cube map's six faces in their order, each with its pictures down to a pixel", "[dds]")
{
    // Six faces of 4 x 4, each its own colour, laid one after another as a
    // cube's are: each face Encode's picture and its halvings.
    std::vector<std::byte> cube;
    for (std::uint8_t face = 0; face < 6; ++face)
    {
        const auto file =
            dds::Encode(Solid(4, 4, static_cast<std::uint8_t>(face * 40), 9, 0, 255), dds::Format::A8R8G8B8);
        REQUIRE(file.size() == 128 + ((16 + 4 + 1) * 4));
        cube.insert(cube.end(), face == 0 ? file.begin() : file.begin() + 128, file.end());
    }
    CHECK_FALSE(dds::DecodeCube(cube)); // not marked a cube
    const std::uint32_t faces = 0xFE00;
    std::memcpy(cube.data() + 112, &faces, sizeof faces);
    const auto read = dds::DecodeCube(cube);
    REQUIRE(read);
    for (std::size_t face = 0; face < 6; ++face)
    {
        REQUIRE(read->faces[face].size() == 3);
        CHECK(read->faces[face][0].width == 4);
        CHECK(read->faces[face][2].width == 1);
        CHECK(read->faces[face][0].rgba[0] == face * 40);
        CHECK(read->faces[face][2].rgba[0] == face * 40);
    }
    cube.resize(cube.size() - 4); // the last face's last pixel cut: a pixel less a face
    const auto shorter = dds::DecodeCube(cube);
    REQUIRE(shorter);
    CHECK(shorter->faces[5].size() == 2);
    cube.resize(128 + (16 * 4 * 6) - 1);
    CHECK_FALSE(dds::DecodeCube(cube));
}

TEST_CASE("the probe cubes in game/cubes, when there, are six faces of nine pictures", "[dds][game]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "cubes";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/cubes (ghilana's extract of lak303d_probe_*.dds writes them there)");
    }
    std::size_t cubes = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        std::ifstream in(entry.path(), std::ios::binary);
        const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        std::vector<std::byte> file(chars.size());
        std::memcpy(file.data(), chars.data(), chars.size());
        const auto cube = dds::DecodeCube(file);
        REQUIRE(cube);
        for (const auto &face : cube->faces)
        {
            REQUIRE(face.size() == 9);
            CHECK(face[0].width == 256);
            CHECK(face[8].width == 1);
        }
        ++cubes;
    }
    CHECK(cubes == 8);
}
