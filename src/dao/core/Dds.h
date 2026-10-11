// The first picture of a DDS texture as plain bytes, over bytes: what reads
// a level's lightmaps with no Direct3D. Read 2026-10-11: every lightmap atlas
// of three levels is DXT5, 64 to 1024 pixels a side; DXT1 and DXT3 are read
// too, and 32-bit pictures that are not packed, since a texture a mod makes
// may be any of them.
//
// A DDS file is "DDS ", a header of 124 bytes (its height at byte 12 of the
// file, its width at 16, the pixel format's four-letter code at 84 and its
// bits a pixel at 88), and the pictures, the largest first. A DXT picture is
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

// Returns the largest picture of a DDS file.
// @return nullopt for bytes that are not a DDS file this reads, or that stop
// short of the picture
std::optional<Image> Decode(std::span<const std::byte> file);

} // namespace dao::core::dds
