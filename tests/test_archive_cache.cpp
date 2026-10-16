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
