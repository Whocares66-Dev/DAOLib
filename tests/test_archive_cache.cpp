// Archives' directories kept between builds of an index (dao/core/ArchiveCache.h): the file's
// bytes, and what does not read as a cache.
#include "dao/core/ArchiveCache.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace install = dao::core::install;
namespace archive = dao::core::erf;

namespace
{

std::vector<install::KeptDirectory> Two()
{
    return {
        {"C:/game/packages/core/data/2da.erf",
         1234,
         133000000000000000,
         archive::Header{archive::Version::V20, 2, 0},
         {{"abi_base.gda", 10}, {"ogre.utc", 4}}},
        {"C:/game/Addins/x/core/data/a.erf", 99, -5, archive::Header{archive::Version::V22, 0, 2U << 4}, {}},
    };
}

} // namespace

TEST_CASE("kept directories come back as they were written", "[archive-cache]")
{
    const auto two = Two();
    const auto read = install::ReadKept(install::WriteKept(two));
    REQUIRE(read);
    CHECK(*read == two);
    CHECK(install::ReadKept(install::WriteKept({})) == std::vector<install::KeptDirectory>{});
}

TEST_CASE("bytes of another version, cut short or with more after are no cache", "[archive-cache]")
{
    const auto bytes = install::WriteKept(Two());
    auto other = bytes;
    other[8] = std::byte{2}; // the format's version
    CHECK_FALSE(install::ReadKept(other));
    for (std::size_t length = 0; length < bytes.size(); ++length)
    {
        CHECK_FALSE(install::ReadKept(std::span(bytes).first(length)));
    }
    auto longer = bytes;
    longer.push_back(std::byte{0});
    CHECK_FALSE(install::ReadKept(longer));
    auto wrongMagic = bytes;
    wrongMagic[0] = std::byte{'X'};
    CHECK_FALSE(install::ReadKept(wrongMagic));
}

TEST_CASE("a header of no version an archive has is no cache", "[archive-cache]")
{
    auto bytes = install::WriteKept(Two());
    // The first directory's header version, after the magic, version, count, the path's
    // length and bytes, the size and the time.
    const std::size_t at = 8 + 4 + 4 + 4 + Two()[0].path.size() + 8 + 8;
    REQUIRE(bytes[at] == std::byte{0});
    bytes[at] = std::byte{7};
    CHECK_FALSE(install::ReadKept(bytes));
}

// Many directories are parsed on several threads (DAOLib #207): they come back in the order written.
TEST_CASE("hundreds of kept directories come back in their order, and one bad among them is no cache",
          "[archive-cache]")
{
    std::vector<install::KeptDirectory> many;
    for (std::uint32_t d = 0; d < 300; ++d)
    {
        install::KeptDirectory kept{"C:/game/a" + std::to_string(d) + ".erf",
                                    d,
                                    -static_cast<std::int64_t>(d),
                                    archive::Header{archive::Version::V22, d, 0},
                                    {}};
        for (std::uint32_t f = 0; f < d % 7; ++f)
        {
            kept.files.emplace_back("file" + std::to_string(d) + "_" + std::to_string(f) + ".utc", f * d);
        }
        many.push_back(std::move(kept));
    }
    auto bytes = install::WriteKept(many);
    CHECK(install::ReadKept(bytes) == many);
    // The version of the 151st directory's header, after the first 150 and its own path, size and time.
    const std::vector<install::KeptDirectory> before(many.begin(), many.begin() + 150);
    const std::size_t at = install::WriteKept(before).size() + 4 + many[150].path.size() + 8 + 8;
    bytes[at] = std::byte{7};
    CHECK_FALSE(install::ReadKept(bytes));
    bytes[at] = std::byte{1};
    CHECK(install::ReadKept(bytes) == many);
    bytes.pop_back();
    CHECK_FALSE(install::ReadKept(bytes));
}
