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

TEST_CASE("an item's kind is a row of the items' worksheet, and the item is read through its view", "[gff3][schema]")
{
    const templates::Field *kind = templates::FieldOf("uti", "", "BaseItem");
    REQUIRE(kind != nullptr);
    CHECK(kind->kind == templates::Kind::Row);
    CHECK(kind->target == 6); // an Items row (BITM_base)
    Builder b("UTI");
    const auto tag = b.ExoString("gen_im_wep_swd");
    b.Struct(0xFFFFFFFF, {{"Tag", 10, tag}, {"BaseItem", 5, 2}, {"PropertiesList", 15, 0}});
    const auto power = b.Struct(0, {{"PropertyID", 5, 1001}, {"PropertyPower", 5, 3}});
    b.List({power});
    const auto bytes = b.Done();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto item = templates::uti::Item::Of(*file);
    REQUIRE(item);
    CHECK(item->BaseItem() == 2);
    CHECK(item->Tag() == "gen_im_wep_swd");
    const auto properties = item->PropertiesList();
    REQUIRE(properties.size() == 1);
    CHECK(properties[0].PropertyID() == 1001);
    CHECK(properties[0].PropertyPower() == 3);
    CHECK_FALSE(item->Material());
}

TEST_CASE("a placeable's appearance is a row of the placeables' worksheet, read through its view", "[gff3][schema]")
{
    const templates::Field *look = templates::FieldOf("utp", "", "Appearance");
    REQUIRE(look != nullptr);
    CHECK(look->kind == templates::Kind::Row);
    CHECK(look->target == 93); // a Placeables row (placeable_types)
    Builder b("UTP");
    const auto tag = b.ExoString("genip_chest");
    b.Struct(0xFFFFFFFF, {{"Tag", 10, tag}, {"Appearance", 4, 7}, {"Useable", 0, 1}});
    const auto bytes = b.Done();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto placeable = templates::utp::Placeable::Of(*file);
    REQUIRE(placeable);
    CHECK(placeable->Appearance() == 7u);
    CHECK(placeable->Useable() == 1);
    CHECK(placeable->ItemList().empty());
}

TEST_CASE("a store's stock is read through its views, a list within a list", "[gff3][schema]")
{
    CHECK(templates::FieldOf("utm", "StoreList[]/ItemList[]", "InventoryRes")->extension == "uti");
    Builder b("UTM");
    const auto potion = b.ResRef("gen_im_qck_health_101");
    b.Struct(0xFFFFFFFF, {{"MarkUp", 5, 150}, {"StoreList", 15, 0}});
    const auto stock = b.Struct(0, {{"InventoryRes", 11, potion}, {"Infinite", 0, 1}});
    const auto shelf = b.Struct(0, {{"ItemList", 15, 8}}); // the second list, after the first's 8 bytes
    b.List({shelf});
    b.List({stock});
    const auto bytes = b.Done();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto store = templates::utm::Store::Of(*file);
    REQUIRE(store);
    CHECK(store->MarkUp() == 150);
    const auto shelves = store->StoreList();
    REQUIRE(shelves.size() == 1);
    const auto items = shelves[0].ItemList();
    REQUIRE(items.size() == 1);
    CHECK(items[0].InventoryRes() == "gen_im_qck_health_101");
    CHECK(items[0].Infinite() == 1);
}

TEST_CASE("a trigger's script is a compiled script, and the trigger is read through its view", "[gff3][schema]")
{
    CHECK(templates::FieldOf("utt", "", "EventScript")->extension == "ncs");
    Builder b("UTT");
    const auto script = b.ResRef("trg_ambush");
    b.Struct(0xFFFFFFFF, {{"EventScript", 11, script}, {"GroupId", 4, 2}});
    const auto bytes = b.Done();
    const auto file = gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto trigger = templates::utt::Trigger::Of(*file);
    REQUIRE(trigger);
    CHECK(trigger->EventScript() == "trg_ambush");
    CHECK(trigger->GroupId() == 2u);
    CHECK(trigger->VarTable().empty());
}
