// The templates' catalogue and views (dao/core/generated/Gff3Schema.h, written by
// tools/gen_gff3_schema.py from the install's scan): a field found by its path and label, with what
// it holds; a creature read through its views, a struct field and a list among them, and a field
// the file lacks or gives another type read as none.
#include "dao/core/Gff3.h"
#include "dao/core/generated/Gff3Schema.h"
#include "files_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace gff3 = dao::core::gff3;
namespace templates = dao::core::templates;
using Builder = dao::test::files::Gff3;

TEST_CASE("a template's field is found by its path and label, with what it holds", "[gff3][schema]")
{
    const templates::Field *type = templates::FieldOf("utc", "Appearance Data", "Appearance_Type");
    REQUIRE(type != nullptr);
    CHECK(type->type == gff3::Type::Int);
    CHECK(type->kind == templates::Kind::Row);
    CHECK(type->target == 2); // an Appearances row
    const templates::Field *dialog = templates::FieldOf("utc", "", "DialogResRef");
    REQUIRE(dialog != nullptr);
    CHECK(dialog->kind == templates::Kind::Resource);
    CHECK(dialog->extension == "dlg");
    // One label in three structs, each its own field.
    CHECK(templates::FieldOf("utc", "", "TemplateResRef")->extension == "utc");
    CHECK(templates::FieldOf("utc", "ItemList[]", "TemplateResRef")->extension == "uti");
    CHECK(templates::FieldOf("utc", "", "no such field") == nullptr);
    CHECK(templates::Fields("utc").size() > 100);
    CHECK(templates::Fields("xyz").empty());
}

TEST_CASE("a creature is read through its views, a field it lacks or of another type as none", "[gff3][schema]")
{
    Builder b("UTC");
    const auto tag = b.ExoString("ogre");
    const auto sword = b.ResRef("gen_im_wep_swd");
    b.Struct(0xFFFFFFFF, {{"Tag", 10, tag}, {"Appearance Data", 14, 1}, {"ItemList", 15, 0}, {"TeamId", 0, 3}});
    b.Struct(0, {{"Appearance_Type", 5, 80}});
    const auto item = b.Struct(255, {{"TemplateResRef", 11, sword}});
    b.List({item});
    const auto bytes = b.Done();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto creature = templates::utc::Creature::Of(*file);
    REQUIRE(creature);
    CHECK(creature->Tag() == "ogre");
    const auto look = creature->Appearance_Data();
    REQUIRE(look);
    CHECK(look->Appearance_Type() == 80);
    CHECK_FALSE(look->Gender()); // the file has no such field
    const auto items = creature->ItemList();
    REQUIRE(items.size() == 1);
    CHECK(items[0].TemplateResRef() == "gen_im_wep_swd");
    CHECK(creature->Equip_ItemList().empty());
    CHECK_FALSE(creature->TeamId()); // an Int in the schema, a Byte in this file
}
