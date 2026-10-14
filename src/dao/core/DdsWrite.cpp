// A picture written as the game keeps its textures (DAOLib #120), for a
// texture made in an editor (DAOEluvianMCP #135). Dds.cpp reads them.
//
// Read 2026-10-14 over all 493 of the creatures' textures (cn_*.dds): every
// file holds its picture and each half of it down to one pixel, and its
// header says so; the header's flags are 0xA1007 (with the pictures' count
// and the first picture's size in bytes, its "linear size", even where the
// pixels are not packed), its capabilities 0x401008, its reserved bytes 0.
// Diffuse maps are DXT1 (DXT5 with alpha), normal, specular and emissive
// maps DXT5, tint masks A4R4G4B4 (one A8R8G8B8). A normal map keeps x in
// alpha and y in green, and in red and blue as well: the pixel shader reads
// the sample's alpha and green, times 2 less 1 (DAOElgaraShaders'
// decompiled character shader, `r1.xy = r1.wy * c29.xx + c29.yy`), and the
// maps' red, green and blue are one grey.
//
// DirectXTex packs the blocks; the halving and the header are written here,
// to be the game's and nothing else.
#include "dao/core/Dds.h"

#include <DirectXTex.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>

namespace dao::core::dds
{

namespace
{

constexpr std::uint32_t kFlags = 0xA1007; // caps, height, width, pixel format, picture count, linear size
constexpr std::uint32_t kCaps = 0x401008; // complex, texture, pictures of halving size
constexpr std::uint32_t kFourCc = 0x4;    // the pixel format is a four-letter code
constexpr std::uint32_t kRgbAlpha = 0x41; // the pixel format is red, green, blue and alpha by masks

template <class T> void Put(std::vector<std::byte> &out, std::size_t at, T value)
{
    std::memcpy(out.data() + at, &value, sizeof(T));
}

// The picture half the size of one, each of its pixels the average of the two or four under it.
Image Halved(const Image &from)
{
    Image out;
    out.width = std::max(from.width / 2, 1U);
    out.height = std::max(from.height / 2, 1U);
    out.rgba.resize(std::size_t{out.width} * out.height * 4);
    const std::uint32_t across = from.width > 1 ? 2 : 1;
    const std::uint32_t down = from.height > 1 ? 2 : 1;
    for (std::uint32_t y = 0; y < out.height; ++y)
    {
        for (std::uint32_t x = 0; x < out.width; ++x)
        {
            for (std::size_t channel = 0; channel < 4; ++channel)
            {
                unsigned sum = 0;
                for (std::uint32_t dy = 0; dy < down; ++dy)
                {
                    for (std::uint32_t dx = 0; dx < across; ++dx)
                    {
                        const std::size_t pixel = (std::size_t{(y * down) + dy} * from.width) + (x * across) + dx;
                        const std::size_t at = (pixel * 4) + channel;
                        sum += from.rgba[at];
                    }
                }
                const unsigned count = across * down;
                out.rgba[(((std::size_t{y} * out.width) + x) * 4) + channel] =
                    static_cast<std::uint8_t>((sum + (count / 2)) / count);
            }
        }
    }
    return out;
}

// One picture's bytes in the file's format.
std::vector<std::byte> Packed(const Image &picture, Format format)
{
    const std::size_t pixels = std::size_t{picture.width} * picture.height;
    std::vector<std::byte> out;
    if (format == Format::A4R4G4B4 || format == Format::A8R8G8B8)
    {
        const bool fourBits = format == Format::A4R4G4B4;
        out.resize(pixels * (fourBits ? 2 : 4));
        for (std::size_t p = 0; p < pixels; ++p)
        {
            const std::uint8_t *rgba = picture.rgba.data() + (p * 4);
            if (fourBits)
            {
                const auto four = [](std::uint8_t v) { return static_cast<std::uint16_t>(((v * 15U) + 127U) / 255U); };
                Put(out, p * 2,
                    static_cast<std::uint16_t>((four(rgba[3]) << 12) | (four(rgba[0]) << 8) | (four(rgba[1]) << 4) |
                                               four(rgba[2])));
            }
            else
            {
                Put(out, p * 4, std::array<std::uint8_t, 4>{rgba[2], rgba[1], rgba[0], rgba[3]});
            }
        }
        return out;
    }
    // DirectXTex reads the picture in place; it writes nothing to it.
    DirectX::Image source{};
    source.width = picture.width;
    source.height = picture.height;
    source.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    source.rowPitch = std::size_t{picture.width} * 4;
    source.slicePitch = pixels * 4;
    source.pixels = const_cast<std::uint8_t *>(picture.rgba.data()); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    // A normal map is data, not colour: each channel weighed alike.
    const DirectX::TEX_COMPRESS_FLAGS flags =
        format == Format::Dxt5Normal ? DirectX::TEX_COMPRESS_UNIFORM : DirectX::TEX_COMPRESS_DEFAULT;
    DirectX::ScratchImage blocks;
    if (FAILED(DirectX::Compress(source, format == Format::Dxt1 ? DXGI_FORMAT_BC1_UNORM : DXGI_FORMAT_BC3_UNORM, flags,
                                 DirectX::TEX_THRESHOLD_DEFAULT, blocks)))
    {
        return {};
    }
    const DirectX::Image *packed = blocks.GetImage(0, 0, 0);
    if (packed == nullptr)
    {
        return {};
    }
    out.resize(packed->slicePitch);
    std::memcpy(out.data(), packed->pixels, packed->slicePitch);
    return out;
}

// The header as the game's files have it, after "DDS ".
std::vector<std::byte> HeaderFor(const Image &image, Format format, std::uint32_t pictures, std::size_t first)
{
    std::vector<std::byte> out(128);
    Put(out, 0, 0x20534444U); // "DDS "
    Put(out, 4, 124U);
    Put(out, 8, kFlags);
    Put(out, 12, image.height);
    Put(out, 16, image.width);
    Put(out, 20, static_cast<std::uint32_t>(first));
    Put(out, 28, pictures);
    Put(out, 76, 32U); // the pixel format's size
    switch (format)
    {
    case Format::Dxt1:
        Put(out, 80, kFourCc);
        Put(out, 84, 0x31545844U); // "DXT1"
        break;
    case Format::Dxt5:
    case Format::Dxt5Normal:
        Put(out, 80, kFourCc);
        Put(out, 84, 0x35545844U); // "DXT5"
        break;
    case Format::A4R4G4B4:
        Put(out, 80, kRgbAlpha);
        Put(out, 88, 16U);
        Put(out, 92, std::array<std::uint32_t, 4>{0x0F00, 0x00F0, 0x000F, 0xF000});
        break;
    case Format::A8R8G8B8:
        Put(out, 80, kRgbAlpha);
        Put(out, 88, 32U);
        Put(out, 92, std::array<std::uint32_t, 4>{0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000});
        break;
    }
    Put(out, 108, kCaps);
    return out;
}

} // namespace

std::vector<std::byte> Encode(const Image &image, Format format)
{
    if (!std::has_single_bit(image.width) || !std::has_single_bit(image.height) ||
        image.rgba.size() != std::size_t{image.width} * image.height * 4)
    {
        return {};
    }
    Image picture = image;
    for (std::size_t p = 0; p < picture.rgba.size(); p += 4)
    {
        if (format == Format::Dxt5Normal)
        {
            const std::uint8_t x = picture.rgba[p];
            const std::uint8_t y = picture.rgba[p + 1];
            picture.rgba[p] = picture.rgba[p + 2] = y;
            picture.rgba[p + 3] = x;
        }
        else if (format == Format::Dxt1)
        {
            picture.rgba[p + 3] = 255; // opaque: DXT1's one bit of alpha would cut holes
        }
    }
    std::vector<std::byte> pixels;
    std::size_t first = 0;
    std::uint32_t pictures = 0;
    while (true)
    {
        const std::vector<std::byte> packed = Packed(picture, format);
        if (packed.empty())
        {
            return {};
        }
        first = pictures == 0 ? packed.size() : first;
        pixels.insert(pixels.end(), packed.begin(), packed.end());
        ++pictures;
        if (picture.width == 1 && picture.height == 1)
        {
            break;
        }
        picture = Halved(picture);
    }
    std::vector<std::byte> out = HeaderFor(image, format, pictures, first);
    out.insert(out.end(), pixels.begin(), pixels.end());
    return out;
}

Image Normals(const Image &decoded)
{
    Image out = decoded;
    for (std::size_t p = 0; p + 3 < out.rgba.size(); p += 4)
    {
        const float x = (static_cast<float>(decoded.rgba[p + 3]) / 127.5F) - 1;
        const float y = (static_cast<float>(decoded.rgba[p + 1]) / 127.5F) - 1;
        const float z = std::sqrt(std::max(0.0F, 1 - (x * x) - (y * y)));
        out.rgba[p] = decoded.rgba[p + 3];
        out.rgba[p + 1] = decoded.rgba[p + 1];
        out.rgba[p + 2] = static_cast<std::uint8_t>(std::lround((z + 1) * 127.5F));
        out.rgba[p + 3] = 255;
    }
    return out;
}

} // namespace dao::core::dds
