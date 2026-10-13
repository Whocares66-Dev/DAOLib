// A picture of a DDS texture as plain bytes, over bytes: what reads a
// level's lightmaps and a material's texture with no Direct3D. Read
// 2026-10-11: every lightmap atlas of three levels is DXT5, 64 to 1024
// pixels a side; DXT1 and DXT3 are read too, and pictures of 32 and of 24
// bits that are not packed, since a texture a mod makes may be any of them.
// Of the textures the game's materials name, 2,145 are DXT1, 1,325 DXT5 and
// 98 of 24 bits (the shaders' session, 2026-10-12, DAOLib #96). A picture of
// 16 bits a pixel is not read.
//
// A DDS file is "DDS ", a header of 124 bytes (its height at byte 12 of the
// file, its width at 16, the pixel format's four-letter code at 84, its
// bits a pixel at 88 and whether it is a cube or a volume at 112), and the
// pictures, the largest first, each half the one before a side and never
// under one pixel. How many there are is taken from the bytes and not from
// the header's count at 28, which the game's own files get wrong
// (DAOLib #96). A DXT picture is
// blocks of four pixels by four: DXT1 a block of 8 bytes, two 5:6:5 colours
// and two bits a pixel that choose between them and two made of them; DXT3
// and DXT5 the same colours after 8 bytes of alpha, DXT3's four bits a
// pixel, DXT5's two alphas and three bits a pixel that choose between them
// and six made of them.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace dao::core::dds
{

// A picture as rows of pixels from its top, each red, green, blue, alpha.
struct Image
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
};

// Returns a picture of a DDS file: its largest, or with `sideMost` the largest that is no
// more than that many pixels a side.
// @param sideMost 0 for the file's largest picture; where every picture the file holds is
// larger, the smallest it holds whole is returned, which for a cube map or a volume is its
// largest
// @return nullopt for bytes that are not a DDS file this reads, or that stop short of the
// picture
std::optional<Image> Decode(std::span<const std::byte> file, std::uint32_t sideMost = 0);

// What a DDS file holds besides its pixels.
enum class Shape : std::uint8_t
{
    Flat,
    Cube,   // a cube map: six faces
    Volume, // slices of a three-dimensional texture
};

struct Description
{
    // "DXT1", "DXT3" or "DXT5"; "B8G8R8" or "B8G8R8A8" for 24 or 32 bits a pixel not packed;
    // another four-letter code as the file has it; or "<n> bits" for another plain format
    std::string format;
    std::uint32_t width = 0; // of the largest picture
    std::uint32_t height = 0;
    // The pictures the bytes hold, the largest first, counted as Decode walks them; 1 for a
    // cube or a volume, whose bytes after the first picture are not walked
    std::uint32_t pictures = 0;
    std::uint32_t headerPictures = 0; // the header's own count, which the game's files get wrong
    Shape shape = Shape::Flat;
    bool decodes = false; // whether Decode reads the file
};

// Returns what a DDS file holds, without decoding its pixels.
// @return nullopt for bytes that are not a DDS file, or whose header gives no size
std::optional<Description> Describe(std::span<const std::byte> file);

} // namespace dao::core::dds
