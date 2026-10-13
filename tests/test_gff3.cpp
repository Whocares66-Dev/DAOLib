#include "dao/core/Gff3.h"
#include "files_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace gff3 = dao::core::gff3;
using dao::test::files::FromText;
using Builder = dao::test::files::Gff3;

namespace
{

// A creature template's shape, as the game's are: the appearance in a struct
// of its own, a tag, a localised name, an inventory list.
std::vector<std::byte> Creature()
{
    Builder b("UTC");
    const auto tag = b.ExoString("ogre");
    const auto name = b.LocString(100);
    const auto res = b.ResRef("gen_im_wep_axe");
    b.Struct(0xFFFFFFFF, {{"Appearance Data", 14, 1},
                          {"Tag", 10, tag},
                          {"LocName", 12, name},
                          {"Items", 15, 0},
                          {"Plot", 0, 1},
                          {"Rank", 5, static_cast<std::uint32_t>(-3)}});
    b.Struct(0, {{"Appearance_Type", 4, 80}, {"Gender", 0, 1}});
    const auto item = b.Struct(7, {{"InventoryRes", 11, res}});
    b.List({item});
    return b.Done();
}

} // namespace

TEST_CASE("a GFF3 template's fields by their labels, of their own types", "[gff3]")
{
    const auto bytes = Creature();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    CHECK(file->FileType() == "UTC");
    CHECK(file->Version() == "V3.2");
    const auto root = file->Root();
    REQUIRE(root);
    CHECK(root->Fields().size() == 6);
    CHECK(root->String("Tag") == "ogre");
    CHECK(root->Get<std::uint8_t>("Plot") == 1);
    CHECK(root->Get<std::int32_t>("Rank") == -3);
    // Another type than the field's gives nothing, even a wider one.
    CHECK_FALSE(root->Get<std::uint32_t>("Plot"));
    CHECK_FALSE(root->String("Plot"));
    const auto loc = root->Loc("LocName");
    REQUIRE(loc);
    CHECK(loc->strref == 100);
    CHECK(loc->strings.empty());
    const auto appearance = root->Child("Appearance Data");
    REQUIRE(appearance);
    CHECK(appearance->Get<std::uint32_t>("Appearance_Type") == 80);
    const auto items = root->List("Items");
    REQUIRE(items.size() == 1);
    CHECK(items[0].Id() == 7);
    CHECK(items[0].ResRef("InventoryRes") == "gen_im_wep_axe");
    CHECK(root->Find("Nothing") == nullptr);
    CHECK(root->List("Tag").empty());
}

TEST_CASE("a GFF3 field whose value runs out of the field data gives nothing", "[gff3]")
{
    Builder b("UTC");
    b.Struct(0xFFFFFFFF, {{"Tag", 10, 1000}, {"Res", 11, 1000}, {"Big", 6, 1000}, {"Self", 14, 9}, {"L", 15, 400}});
    const auto bytes = b.Done();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto root = file->Root();
    CHECK_FALSE(root->String("Tag"));
    CHECK_FALSE(root->ResRef("Res"));
    CHECK_FALSE(root->Get<std::uint64_t>("Big"));
    CHECK_FALSE(root->Child("Self")); // a struct past the file's
    CHECK(root->List("L").empty());
}

TEST_CASE("what is no GFF3, or whose sections lie past its end, is refused", "[gff3]")
{
    CHECK_FALSE(gff3::File::Parse(FromText("UTC V3.2")));
    CHECK_FALSE(gff3::File::Parse(FromText("GFF V4.0PC  xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx")));
    auto bytes = Creature();
    bytes.resize(bytes.size() - 8); // the list indices cut
    CHECK_FALSE(gff3::File::Parse(bytes));
}

TEST_CASE("a GFF3 file's 64-bit numbers, vectors and localised strings with text", "[gff3]")
{
    Builder b("ARE");
    dao::test::files::Bytes wide;
    dao::test::files::Append32(wide, 0x76543210);
    dao::test::files::Append32(wide, 0xFEDCBA98);
    const auto big = b.Raw(wide);
    dao::test::files::Bytes vector;
    for (const float f : {1.0F, -2.5F, 3.0F})
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &f, 4);
        dao::test::files::Append32(vector, bits);
    }
    const auto place = b.Raw(vector);
    dao::test::files::Bytes loc;
    dao::test::files::Append32(loc, 0); // its size, not read
    dao::test::files::Append32(loc, 7);
    dao::test::files::Append32(loc, 1);
    dao::test::files::Append32(loc, 0); // English
    dao::test::files::Append32(loc, 5);
    dao::test::files::AppendText(loc, "Ogres");
    const auto name = b.Raw(loc);
    b.Struct(0xFFFFFFFF, {{"Big", 6, big}, {"Place", 17, place}, {"Orient", 16, name + 20}, {"Name", 12, name}});
    const auto bytes = b.Done();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto root = file->Root();
    CHECK(root->Get<std::uint64_t>("Big") == 0xFEDCBA9876543210ULL);
    CHECK(root->Floats("Place") == std::vector<float>{1.0F, -2.5F, 3.0F});
    CHECK(root->Floats("Orient").empty()); // four floats run out of the field data
    const auto text = root->Loc("Name");
    REQUIRE(text);
    CHECK(text->strref == 7);
    REQUIRE(text->strings.size() == 1);
    CHECK(text->strings[0].first == 0);
    CHECK(text->strings[0].second == "Ogres");
}
