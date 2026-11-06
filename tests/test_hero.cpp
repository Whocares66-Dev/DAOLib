#include "dao/core/Gff4.h"
#include "dao/core/Gff4Writer.h"
#include "dao/core/Hero.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace
{

namespace gff4 = dao::core::gff4;
namespace hero = dao::core::hero;
using gff4::Type;

std::uint32_t U32At(const std::vector<std::byte> &bytes, std::size_t at)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + at, sizeof value);
    return value;
}

std::filesystem::path Heroes()
{
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "heroes";
}

std::vector<std::byte> FileBytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(chars.size());
    std::memcpy(bytes.data(), chars.data(), chars.size());
    return bytes;
}

hero::Hero Sample()
{
    hero::Hero out;
    out.head.parts = {L"HM_UHM_BASa_0", L"", L"hm_har_hb4a_0"};
    out.head.tints = {L"t1_skn_001", L"", L"t3_eye_grn"};
    out.head.models = {{L"uh_nos_30a_0", 0.41F}, {L"uh_uhm_basa_0", -7.5F}};
    out.head.values = {{L"FaceM1", L"mml_fAgeBlendWeight", 11, 0.25F}};
    out.head.vectors = {{L"FaceM1", L"mml_vTintMaskColours", 1, {1, 0.5F, 0.25F, 1}}};
    out.head.textures = {{L"FaceM1", L"mml_tDiffuse", L"uh_hed_fema_0d"}};
    out.preset = L"HM_CPS_P01.mop";
    out.gender = 1;
    out.race = 3;
    out.characterClass = 1;
    out.background = 5;
    out.name = L"Jaden";
    out.attributes = {{2, 3}, {1, 2}};
    out.abilities = {100011, 10001};
    out.portrait = {0, 0, 0.6F, 0, 0, 0, 0};
    return out;
}

} // namespace

TEST_CASE("a template's fields sit by alignment, largest first, and the file lists them by label", "[gff4][writer]")
{
    gff4::Writer out("TEST", "V0.1");
    const gff4::StructAt root = out.Root("TOP ", {{3, Type::U8}, {1, Type::Vector4}, {2, Type::S32}});
    out.U8(root, 3, 7);
    const std::vector<std::byte> bytes = out.Save();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const gff4::Template &shape = file->Templates().at(0);
    CHECK(shape.size == 32); // the vector at 0, the int at 16, the byte at 20, rounded up to 16
    REQUIRE(shape.fields.size() == 3);
    CHECK(shape.fields[0].label == 1);
    CHECK(shape.fields[0].offset == 0);
    CHECK(shape.fields[1].offset == 16);
    CHECK(shape.fields[2].offset == 20);
    CHECK(file->Root()->Get<std::uint8_t>(3) == 7);
    // The data starts at the largest alignment written: 28 + 16 + 3 * 12 = 80, already a multiple of 16.
    CHECK(file->DataOffset() == 80);
    CHECK(U32At(bytes, 80 + 16) == 0xFFFFFFFF); // a field never written
}

TEST_CASE("a list's count sits so its elements land at their alignment, and a string is written once", "[gff4][writer]")
{
    gff4::Writer out("TEST", "V0.1");
    const auto element = out.AddTemplate("VEC ", {{1, Type::Vector4}});
    const gff4::StructAt root = out.Root("TOP ", {{10, Type::String}, {11, element, gff4::kList}, {12, Type::String}});
    out.String(root, 10, L"ab"); // after the 12-byte root: a count of 3 at 12, then a, b and the NUL, to 22
    const auto vectors = out.Structs(root, 11, 2);
    out.String(root, 12, L"ab");
    const std::vector<std::byte> bytes = out.Save();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const std::size_t data = file->DataOffset();
    CHECK(U32At(bytes, data + 0) == 12); // the first string's offset
    CHECK(U32At(bytes, data + 8) == 12); // the same text, written once
    CHECK(U32At(bytes, data + 4) == 28); // the list's count at 28, its elements from 32
    REQUIRE(vectors.size() == 2);
    CHECK(vectors[0].at == 32);
    CHECK(vectors[1].at == 48);
    CHECK(U32At(bytes, data + 28) == 2);
    CHECK(file->Root()->Structs(11).size() == 2);
    CHECK(bytes.size() == data + 64); // nothing after the last element
}

TEST_CASE("a hero written is read back as it was", "[hero]")
{
    const hero::Hero sample = Sample();
    const std::vector<std::byte> bytes = hero::Write(sample);
    CHECK(std::string(reinterpret_cast<const char *>(bytes.data()), 20) == std::string("GFF V4.0PC  CHARV0.1"));
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    CHECK(file->Templates().size() == 8);
    CHECK(file->DataOffset() == 560); // 552 rounded up to 16, for the head's vectors
    const auto read = hero::HeroOf(*file);
    REQUIRE(read);
    CHECK(read->head.name == L"Head_Morph");
    CHECK(read->head.parts == sample.head.parts);
    CHECK(read->head.tints == sample.head.tints);
    REQUIRE(read->head.models.size() == 2);
    CHECK(read->head.models[1].weight == -7.5F);
    REQUIRE(read->head.vectors.size() == 1);
    CHECK(read->head.vectors[0].value == sample.head.vectors[0].value);
    REQUIRE(read->head.textures.size() == 1);
    CHECK(read->head.textures[0].texture == L"uh_hed_fema_0d");
    CHECK(read->preset == sample.preset);
    CHECK(read->gender == 1);
    CHECK(read->race == 3);
    CHECK(read->characterClass == 1);
    CHECK(read->background == 5);
    CHECK(read->name == L"Jaden");
    REQUIRE(read->attributes.size() == 2);
    CHECK(read->attributes[0].id == 1); // in order of id, as the game's map keeps them
    CHECK(read->attributes[0].points == 2);
    CHECK(read->abilities == sample.abilities);
    CHECK(read->portrait == sample.portrait);
    CHECK(hero::Write(*read) == bytes);
}

TEST_CASE("a file that is no hero is not read as one", "[hero]")
{
    gff4::Writer out("MOP ", "V1.0");
    out.Root("MOP ", {{23008, Type::String}});
    const std::vector<std::byte> bytes = out.Save();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    CHECK_FALSE(hero::HeroOf(*file));
}

TEST_CASE("the heroes in game/heroes, when there, each written back byte for byte", "[hero]")
{
    const std::vector<std::filesystem::path> found = hero::HeroesIn(Heroes());
    if (found.empty())
    {
        SKIP("no .chr in game/heroes (character creation's Save button writes one)");
    }
    for (const std::filesystem::path &path : found)
    {
        INFO(path.filename().string());
        const std::vector<std::byte> bytes = FileBytes(path);
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const auto read = hero::HeroOf(*file);
        REQUIRE(read);
        CHECK(hero::Write(*read) == bytes);
    }
}

TEST_CASE("a folder with no heroes lists none", "[hero]")
{
    CHECK(hero::HeroesIn(Heroes() / "no such folder").empty());
}
