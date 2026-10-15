// A picture of floats, any number of channels to a pixel, for tools that measure or change a
// texture's pixels in more than eight bits (DAOLib #129, for DAOElgaraShaders' materials table):
// made from a texture's picture (dao/core/Dds.h), and resized by a box filter as Pillow's
// Image.resize(..., Image.BOX) does, so a tool ported from Python reads the same numbers.
#pragma once

#include "dao/core/Dds.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dao::core
{

// Pixels row by row from the top, each `channels` floats.
struct Picture
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t channels = 0;
    std::vector<float> values;

    [[nodiscard]] float At(std::uint32_t x, std::uint32_t y, std::uint32_t channel) const
    {
        return values[(((static_cast<std::size_t>(y) * width) + x) * channels) + channel];
    }
};

// Returns a texture's picture as red, green, blue and alpha from 0 to 1, each byte over 255.
Picture FromRgba8(const dds::Image &image);

// Returns a picture averaged to a size, as Pillow's BOX filter does: each pixel the mean of the
// pixels whose middles fall within its span, across and then down, in doubles rounded to floats
// between the two as Pillow's float pictures are.
// @return the picture itself where the size is its own; an empty one for a size of 0
Picture Box(const Picture &picture, std::uint32_t width, std::uint32_t height);

} // namespace dao::core
