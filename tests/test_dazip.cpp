// The add-in package (dao/core/DaZip.h): a zip as SharpZipLib reads it, and
// an add-in's manifest and folder as DAUpdater installs them.
#include "dao/core/DaZip.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace dazip = dao::core::dazip;

namespace
{

std::vector<std::byte> Bytes(const std::string &text)
{
    std::vector<std::byte> out(text.size());
    std::memcpy(out.data(), text.data(), text.size());
    return out;
}

std::uint32_t U32(const std::vector<std::byte> &bytes, std::size_t at)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + at, 4);
    return value;
}

std::uint16_t U16(const std::vector<std::byte> &bytes, std::size_t at)
{
    std::uint16_t value = 0;
    std::memcpy(&value, bytes.data() + at, 2);
    return value;
}

// The zip's files as its central directory lists them: each path, and its bytes from its local header.
std::vector<std::pair<std::string, std::string>> Unzipped(const std::vector<std::byte> &zip)
{
    const std::size_t end = zip.size() - 22;
    REQUIRE(U32(zip, end) == 0x06054B50U);
    const std::uint16_t count = U16(zip, end + 10);
    std::size_t at = U32(zip, end + 16);
    std::vector<std::pair<std::string, std::string>> out;
    for (std::uint16_t i = 0; i < count; ++i)
    {
        REQUIRE(U32(zip, at) == 0x02014B50U);
        CHECK(U16(zip, at + 10) == 0); // stored
        const std::uint32_t size = U32(zip, at + 24);
        const std::uint16_t length = U16(zip, at + 28);
        const std::uint32_t local = U32(zip, at + 42);
        std::string path(length, '\0');
        std::memcpy(path.data(), zip.data() + at + 46, length);
        REQUIRE(U32(zip, local) == 0x04034B50U);
        const std::size_t data = local + 30 + U16(zip, local + 26) + U16(zip, local + 28);
        std::string bytes(size, '\0');
        std::memcpy(bytes.data(), zip.data() + data, size);
        out.emplace_back(path, bytes);
        at += 46 + length;
    }
    return out;
}

} // namespace

TEST_CASE("a zip stores its files with their CRC-32, listed in its central directory", "[dazip]")
{
    const std::vector<dazip::ZipItem> files{{"a/check.txt", Bytes("123456789")}, {"b.bin", Bytes("")}};
    const std::vector<std::byte> zip = dazip::Zip(files);
    REQUIRE_FALSE(zip.empty());
    CHECK(U32(zip, 14) == 0xCBF43926U); // CRC-32 of "123456789", the check value
    const auto unzipped = Unzipped(zip);
    REQUIRE(unzipped.size() == 2);
    CHECK(unzipped[0] == std::pair<std::string, std::string>{"a/check.txt", "123456789"});
    CHECK(unzipped[1].first == "b.bin");
}

TEST_CASE("a zip is not written with a path that leaves its folder or is given twice", "[dazip]")
{
    for (const char *bad : {"", "/root.txt", "a\\b.txt", "../up.txt"})
    {
        const std::vector<dazip::ZipItem> files{{bad, Bytes("x")}};
        CHECK(dazip::Zip(files).empty());
    }
    const std::vector<dazip::ZipItem> twice{{"a.txt", Bytes("x")}, {"a.txt", Bytes("y")}};
    CHECK(dazip::Zip(twice).empty());
}

TEST_CASE("an add-in's package holds its manifest and its files in its folder", "[dazip]")
{
    dazip::AddIn addIn;
    addIn.uid = "Elgara_RedOgre";
    addIn.name = "RedOgre";
    addIn.title = "A red ogre & co.";
    addIn.description = "The ogre, red.";
    addIn.publisher = "Eluvian";
    const std::vector<dazip::AddInFile> files{{"core/override/redogre.erf", Bytes("an archive")},
                                              {"core/data/talktables/Elgara_RedOgre_c_en-us.tlk", Bytes("names")}};
    const auto unzipped = Unzipped(dazip::Package(addIn, files));
    REQUIRE(unzipped.size() == 3);
    CHECK(unzipped[0].first == "Manifest.xml");
    const std::string &manifest = unzipped[0].second;
    CHECK(manifest.find("<Manifest Type=\"AddIn\">") != std::string::npos);
    CHECK(manifest.find("<AddInsList>") != std::string::npos);
    CHECK(manifest.find("<AddInItem UID=\"Elgara_RedOgre\" Name=\"RedOgre\" ExtendedModuleUID=\"Single Player\"") !=
          std::string::npos);
    CHECK(manifest.find("<Title DefaultText=\"A red ogre &amp; co.\">") != std::string::npos);
    CHECK(manifest.find("<Version>1.0</Version>") != std::string::npos);
    CHECK(manifest.find("<en-us><![CDATA[A red ogre & co.]]></en-us>") != std::string::npos);
    CHECK(manifest.find("<Type>1</Type>") != std::string::npos);
    CHECK(manifest.find("<ReleaseDate>") != std::string::npos);
    CHECK(manifest.find("<Size>") != std::string::npos);
    CHECK(manifest.find("<FileList>") != std::string::npos);
    CHECK(manifest.find("<Folder src=\"addins\" Type=\"addin\" />") != std::string::npos);
    CHECK(unzipped[1] == std::pair<std::string, std::string>{"Contents/addins/Elgara_RedOgre/core/override/redogre.erf",
                                                             "an archive"});
    CHECK(unzipped[2].first == "Contents/addins/Elgara_RedOgre/core/data/talktables/Elgara_RedOgre_c_en-us.tlk");
}

TEST_CASE("an add-in is not packed with a uid not of letters, digits and _, or a path that leaves its folder",
          "[dazip]")
{
    dazip::AddIn addIn;
    addIn.uid = "red ogre";
    CHECK(dazip::Package(addIn, {}).empty());
    addIn.uid = "red_ogre";
    for (const char *bad : {"", "/core/a.dds", "core\\a.dds", "../a.dds"})
    {
        const std::vector<dazip::AddInFile> files{{bad, Bytes("x")}};
        CHECK(dazip::Package(addIn, files).empty());
    }
}
