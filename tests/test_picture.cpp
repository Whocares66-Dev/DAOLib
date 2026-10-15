#include "dao/core/Picture.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

namespace core = dao::core;

namespace
{

core::Picture Row(std::vector<float> values)
{
    return core::Picture{static_cast<std::uint32_t>(values.size()), 1, 1, std::move(values)};
}

} // namespace

// The expected values are Pillow 11's, Image.fromarray(..., 'F').resize(size, Image.BOX),
// 2026-10-15.
TEST_CASE("a row boxed as Pillow boxes it, down and up", "[picture]")
{
    const core::Picture row = Row({0, 10, 20, 30, 40});
    CHECK(core::Box(row, 2, 1).values == std::vector<float>{10, 35});
    CHECK(core::Box(row, 3, 1).values == std::vector<float>{5, 20, 35});
    CHECK(core::Box(row, 4, 1).values == std::vector<float>{0, 15, 30, 40});
    CHECK(core::Box(row, 7, 1).values == std::vector<float>{0, 10, 10, 20, 30, 30, 40});
}

TEST_CASE("a picture boxed across and then down", "[picture]")
{
    core::Picture grid{4, 3, 1, {}};
    for (int i = 0; i < 12; ++i)
    {
        grid.values.push_back(static_cast<float>(i));
    }
    CHECK(core::Box(grid, 2, 2).values == std::vector<float>{2.5F, 4.5F, 8.5F, 10.5F});
    CHECK(core::Box(grid, 3, 5).values ==
          std::vector<float>{0, 1.5F, 3, 0, 1.5F, 3, 4, 5.5F, 7, 8, 9.5F, 11, 8, 9.5F, 11});
}

TEST_CASE("each channel boxed on its own", "[picture]")
{
    const core::Picture two{2, 1, 2, {0, 100, 10, 200}};
    const core::Picture one = core::Box(two, 1, 1);
    CHECK(one.channels == 2);
    CHECK(one.values == std::vector<float>{5, 150});
}

TEST_CASE("a picture of its own size, or of none", "[picture]")
{
    const core::Picture row = Row({1, 2});
    CHECK(core::Box(row, 2, 1).values == row.values);
    CHECK(core::Box(row, 0, 1).values.empty());
}

TEST_CASE("a texture's bytes as floats from 0 to 1", "[picture]")
{
    const core::dds::Image image{1, 1, {0, 51, 255, 128}};
    const core::Picture picture = core::FromRgba8(image);
    CHECK(picture.channels == 4);
    CHECK(picture.At(0, 0, 0) == 0.0F);
    CHECK(picture.At(0, 0, 1) == 0.2F);
    CHECK(picture.At(0, 0, 2) == 1.0F);
    CHECK(picture.At(0, 0, 3) == 128.0F / 255.0F);
}
