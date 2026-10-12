#include "dao/core/Dds.h"

#include "dao/core/Bytes.h"

#include <algorithm>
#include <array>

namespace dao::core::dds
{

namespace
{

constexpr std::uint32_t kMagic = 0x20534444; // "DDS "
constexpr std::size_t kPixels = 128;         // where the first picture starts
constexpr std::uint32_t kSideMost = 16384;   // more is no texture's
constexpr std::uint32_t kDxt1 = 0x31545844;  // "DXT1"
constexpr std::uint32_t kDxt3 = 0x33545844;
constexpr std::uint32_t kDxt5 = 0x35545844;
constexpr std::uint32_t kNotFlat = 0x200 | 0x200000; // the header's second capabilities: a cube map, a volume

std::array<std::uint8_t, 3> Colour565(std::uint16_t packed)
{
    const auto red = static_cast<std::uint8_t>((packed >> 11) & 31);
    const auto green = static_cast<std::uint8_t>((packed >> 5) & 63);
    const auto blue = static_cast<std::uint8_t>(packed & 31);
    return {static_cast<std::uint8_t>((red << 3) | (red >> 2)), static_cast<std::uint8_t>((green << 2) | (green >> 4)),
            static_cast<std::uint8_t>((blue << 3) | (blue >> 2))};
}

// The sixteen colours of a block's 8 colour bytes. With `punch`, DXT1's
// way: the first colour not above the second means three colours and one
// that is nothing.
std::array<std::array<std::uint8_t, 4>, 16> ColourBlock(std::span<const std::byte> block, bool punch)
{
    const std::uint16_t first = ReadAt<std::uint16_t>(block, 0).value_or(0);
    const std::uint16_t second = ReadAt<std::uint16_t>(block, 2).value_or(0);
    const std::uint32_t choices = ReadAt<std::uint32_t>(block, 4).value_or(0);
    const std::array<std::uint8_t, 3> a = Colour565(first);
    const std::array<std::uint8_t, 3> b = Colour565(second);
    std::array<std::array<std::uint8_t, 4>, 4> palette{};
    const bool three = punch && first <= second;
    for (std::size_t channel = 0; channel < 3; ++channel)
    {
        palette[0][channel] = a[channel];
        palette[1][channel] = b[channel];
        palette[2][channel] =
            static_cast<std::uint8_t>(three ? (a[channel] + b[channel]) / 2 : ((2 * a[channel]) + b[channel]) / 3);
        palette[3][channel] = static_cast<std::uint8_t>(three ? 0 : (a[channel] + (2 * b[channel])) / 3);
    }
    palette[0][3] = palette[1][3] = palette[2][3] = 255;
    palette[3][3] = three ? 0 : 255;
    std::array<std::array<std::uint8_t, 4>, 16> pixels{};
    for (std::size_t i = 0; i < 16; ++i)
    {
        pixels.at(i) = palette.at((choices >> (2 * i)) & 3);
    }
    return pixels;
}

// The sixteen alphas of a DXT5 block's 8 alpha bytes.
std::array<std::uint8_t, 16> AlphaBlock(std::span<const std::byte> block)
{
    const std::uint64_t packed = ReadAt<std::uint64_t>(block, 0).value_or(0);
    const auto first = static_cast<std::uint8_t>(packed & 0xFF);
    const auto second = static_cast<std::uint8_t>((packed >> 8) & 0xFF);
    std::array<std::uint8_t, 8> palette{first, second};
    const int steps = first > second ? 7 : 5;
    for (int i = 1; i < steps; ++i)
    {
        palette.at(static_cast<std::size_t>(i) + 1) =
            static_cast<std::uint8_t>((((steps - i) * first) + (i * second)) / steps);
    }
    if (steps == 5)
    {
        palette[6] = 0;
        palette[7] = 255;
    }
    std::array<std::uint8_t, 16> alphas{};
    for (std::size_t i = 0; i < 16; ++i)
    {
        alphas.at(i) = palette.at((packed >> (16 + (3 * i))) & 7);
    }
    return alphas;
}

bool DecodeBlocks(std::span<const std::byte> data, std::uint32_t format, Image &image)
{
    const std::size_t blockSize = format == kDxt1 ? 8 : 16;
    const std::size_t across = (image.width + 3) / 4;
    const std::size_t down = (image.height + 3) / 4;
    if (data.size() / blockSize < across * down)
    {
        return false;
    }
    for (std::size_t by = 0; by < down; ++by)
    {
        for (std::size_t bx = 0; bx < across; ++bx)
        {
            const std::span<const std::byte> block = data.subspan(((by * across) + bx) * blockSize, blockSize);
            const auto colours = ColourBlock(block.subspan(blockSize - 8), format == kDxt1);
            std::array<std::uint8_t, 16> alphas{};
            if (format == kDxt5)
            {
                alphas = AlphaBlock(block);
            }
            for (std::size_t i = 0; i < 16; ++i)
            {
                const std::size_t x = (bx * 4) + (i % 4);
                const std::size_t y = (by * 4) + (i / 4);
                if (x >= image.width || y >= image.height)
                {
                    continue;
                }
                std::array<std::uint8_t, 4> pixel = colours.at(i);
                if (format == kDxt5)
                {
                    pixel[3] = alphas.at(i);
                }
                else if (format == kDxt3)
                {
                    const auto nibble = (std::to_integer<unsigned>(block[i / 2]) >> (4 * (i % 2))) & 15U;
                    pixel[3] = static_cast<std::uint8_t>(nibble * 17);
                }
                std::ranges::copy(pixel, image.rgba.begin() + static_cast<std::ptrdiff_t>(((y * image.width) + x) * 4));
            }
        }
    }
    return true;
}

// A picture that is not packed, of `bytes` a pixel: blue, green, red, and with four an
// alpha; with three every pixel is opaque.
bool DecodePlain(std::span<const std::byte> data, std::size_t bytes, Image &image)
{
    const std::size_t pixels = image.rgba.size() / 4;
    if (data.size() / bytes < pixels)
    {
        return false;
    }
    for (std::size_t pixel = 0; pixel < pixels; ++pixel)
    {
        const std::span<const std::byte> from = data.subspan(pixel * bytes, bytes);
        const std::size_t to = pixel * 4;
        image.rgba[to] = std::to_integer<std::uint8_t>(from[2]);
        image.rgba[to + 1] = std::to_integer<std::uint8_t>(from[1]);
        image.rgba[to + 2] = std::to_integer<std::uint8_t>(from[0]);
        image.rgba[to + 3] = bytes == 4 ? std::to_integer<std::uint8_t>(from[3]) : std::uint8_t{255};
    }
    return true;
}

// How many bytes of the file a picture of this size is.
std::size_t BytesOf(std::uint32_t format, std::size_t plainBytes, std::uint32_t width, std::uint32_t height)
{
    if (plainBytes != 0)
    {
        return std::size_t{width} * height * plainBytes;
    }
    return std::size_t{(width + 3) / 4} * ((height + 3) / 4) * (format == kDxt1 ? 8U : 16U);
}

} // namespace

std::optional<Image> Decode(std::span<const std::byte> file, std::uint32_t sideMost)
{
    if (file.size() < kPixels || ReadAt<std::uint32_t>(file, 0) != kMagic)
    {
        return std::nullopt;
    }
    Image image;
    image.height = ReadAt<std::uint32_t>(file, 12).value_or(0);
    image.width = ReadAt<std::uint32_t>(file, 16).value_or(0);
    const std::uint32_t more = ReadAt<std::uint32_t>(file, 112).value_or(0);
    const std::uint32_t format = ReadAt<std::uint32_t>(file, 84).value_or(0);
    const std::uint32_t bits = ReadAt<std::uint32_t>(file, 88).value_or(0);
    const bool packed = format == kDxt1 || format == kDxt3 || format == kDxt5;
    const std::size_t plainBytes = !packed && format == 0 && (bits == 24 || bits == 32) ? bits / 8 : 0;
    if (image.width == 0 || image.height == 0 || image.width > kSideMost || image.height > kSideMost ||
        (!packed && plainBytes == 0))
    {
        return std::nullopt;
    }
    // Down the file's pictures to the one asked for, by the pictures its bytes hold and not by
    // the count its header gives: the game's own beckmann.dds is marked as having smaller
    // pictures, holds all ten of them, and counts one (DAOLib #96). A cube's or a volume's
    // bytes after its first picture are its other faces or slices, and are not walked.
    std::size_t at = kPixels;
    while (sideMost != 0 && (more & kNotFlat) == 0 && std::max(image.width, image.height) > sideMost)
    {
        const std::size_t next = at + BytesOf(format, plainBytes, image.width, image.height);
        const std::uint32_t width = std::max(image.width / 2, 1U);
        const std::uint32_t height = std::max(image.height / 2, 1U);
        if (next > file.size() || file.size() - next < BytesOf(format, plainBytes, width, height))
        {
            break;
        }
        at = next;
        image.width = width;
        image.height = height;
    }
    image.rgba.resize(static_cast<std::size_t>(image.width) * image.height * 4);
    const std::span<const std::byte> data = file.subspan(at);
    const bool read = packed ? DecodeBlocks(data, format, image) : DecodePlain(data, plainBytes, image);
    return read ? std::optional<Image>(std::move(image)) : std::nullopt;
}

} // namespace dao::core::dds
