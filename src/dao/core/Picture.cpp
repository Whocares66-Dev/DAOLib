// dao/core/Picture.h. The box filter is Pillow's precompute_coeffs (libImaging/Resample.c) for
// its box, ported by DAOElgaraShaders and checked there against Pillow to 0.0004 in metal share
// over the game's materials (2026-10-15); moved here as DAOLib #129.
#include "dao/core/Picture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dao::core
{

namespace
{

// The input pixels each output pixel of one axis averages: the first, and a weight each.
struct Coefficients
{
    std::vector<std::uint32_t> first;
    std::vector<std::vector<double>> weights;
};

Coefficients BoxCoefficients(std::uint32_t inSize, std::uint32_t outSize)
{
    const double scale = static_cast<double>(inSize) / outSize;
    const double filterScale = std::max(scale, 1.0);
    const double support = 0.5 * filterScale;
    Coefficients out;
    out.first.resize(outSize);
    out.weights.resize(outSize);
    for (std::uint32_t xx = 0; xx < outSize; ++xx)
    {
        const double centre = (xx + 0.5) * scale;
        // Pillow truncates (int)(x + 0.5): the floor here, a start below 0 held to 0 either way.
        const auto xmin = std::max<std::int64_t>(static_cast<std::int64_t>(std::floor(centre - support + 0.5)), 0);
        const std::int64_t xmax =
            std::min<std::int64_t>(static_cast<std::int64_t>(std::floor(centre + support + 0.5)), inSize) - xmin;
        std::vector<double> k(static_cast<std::size_t>(std::max<std::int64_t>(xmax, 0)));
        double total = 0;
        for (std::size_t x = 0; x < k.size(); ++x)
        {
            const double at = (static_cast<double>(x) + static_cast<double>(xmin) - centre + 0.5) / filterScale;
            k[x] = (at > -0.5 && at <= 0.5) ? 1.0 : 0.0;
            total += k[x];
        }
        if (total != 0)
        {
            for (double &weight : k)
            {
                weight /= total;
            }
        }
        out.first[xx] = static_cast<std::uint32_t>(xmin);
        out.weights[xx] = std::move(k);
    }
    return out;
}

// One pass along an axis: across (x) or down (y).
Picture Pass(const Picture &in, std::uint32_t width, std::uint32_t height, bool across)
{
    const Coefficients c = across ? BoxCoefficients(in.width, width) : BoxCoefficients(in.height, height);
    Picture out{width, height, in.channels, {}};
    out.values.resize(static_cast<std::size_t>(width) * height * in.channels);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const std::uint32_t along = across ? x : y;
            for (std::uint32_t channel = 0; channel < in.channels; ++channel)
            {
                double sum = 0;
                const std::vector<double> &weights = c.weights[along];
                for (std::size_t k = 0; k < weights.size(); ++k)
                {
                    const auto from = c.first[along] + static_cast<std::uint32_t>(k);
                    sum += static_cast<double>(across ? in.At(from, y, channel) : in.At(x, from, channel)) * weights[k];
                }
                out.values[(((static_cast<std::size_t>(y) * width) + x) * in.channels) + channel] =
                    static_cast<float>(sum);
            }
        }
    }
    return out;
}

} // namespace

Picture FromRgba8(const dds::Image &image)
{
    constexpr std::uint32_t kChannels = 4;
    Picture out{image.width, image.height, kChannels, {}};
    out.values.resize(image.rgba.size());
    std::ranges::transform(image.rgba, out.values.begin(),
                           [](std::uint8_t byte) { return static_cast<float>(byte) / 255.0F; });
    return out;
}

Picture Box(const Picture &picture, std::uint32_t width, std::uint32_t height)
{
    if (picture.width == width && picture.height == height)
    {
        return picture;
    }
    if (width == 0 || height == 0 || picture.width == 0 || picture.height == 0)
    {
        return Picture{width, height, picture.channels, {}};
    }
    return Pass(Pass(picture, width, picture.height, true), width, height, false);
}

} // namespace dao::core
