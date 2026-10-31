// dao/core/Character.h over tables made here, and over Alistair's template,
// items and tables in game/characters when ghilana's extract has written
// them there (DAOLib #161).
#include "dao/core/Character.h"
#include "dao/core/GdaFile.h"
#include "dao/core/GdaHash.h"
#include "dao/core/Gff3.h"
#include "dao/core/M2da.h"
#include "files_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace character = dao::core::character;
namespace m2da = dao::core::m2da;
using Catch::Approx;
using dao::core::GdaFile;

namespace
{

// A merged table of named columns, its rows each an ID and its cells.
m2da::Merged Table(std::initializer_list<std::string_view> columns,
                   std::initializer_list<std::pair<std::int64_t, std::vector<GdaFile::Cell>>> rows)
{
    m2da::Merged table;
    for (const auto name : columns)
    {
        table.columns.push_back({.hash = dao::core::GdaColumnHash(name), .type = std::nullopt});
    }
    for (const auto &[id, cells] : rows)
    {
        table.rows.push_back({.part = 0, .cells = cells, .id = id, .conflict = false, .taken = true, .alike = {}});
    }
    m2da::Index(table);
    return table;
}

GdaFile::Cell I(std::int64_t value)
{
    return value;
}

GdaFile::Cell F(double value)
{
    return value;
}

GdaFile::Cell S(const wchar_t *text)
{
    return std::wstring(text);
}

// The game's rows, cut down to Alistair's kit and a helmet or two.
struct Tables
{
    std::map<std::int64_t, m2da::Merged> byId{
        {2, Table({"ID", "ModelType", "ModelRace", "ModelName", "ModelGenderOverride", "WeaponOffset", "Neck_Model",
                   "Head_Worksheet", "Hair_Worksheet"},
                  {{15, {I(15), S(L"P"), S(L"H"), S(L""), S(L""), F(0), S(L"HED_DCPa"), I(79), I(74)}},
                   {40, {I(40), S(L"S"), S(L"O"), S(L"c_ogre"), S(L""), F(0), S(L""), I(0), I(0)}},
                   {50, {I(50), S(L"H"), S(L"C"), S(L"CN_BDY_glka"), S(L"N"), F(0), S(L"HED_DGLa"), I(79), I(0)}}})},
        {74,
         Table({"ID", "ModelType", "ModelSubType", "ModelVariation"}, {{1, {I(1), S(L"har"), S(L"bld"), S(L"a")}}})},
        {79,
         Table({"ID", "ModelType", "ModelSubType", "ModelVariation"}, {{2, {I(2), S(L"uhm"), S(L"bas"), S(L"b")}}})},
        {6, Table({"ID", "Variation_Worksheet", "WearR", "WearL", "WieldR", "WieldL", "WeaponOffset"},
                  {{2, {I(2), I(30), I(5), I(10), I(14), I(15), F(0)}},
                   {6, {I(6), I(39), I(0), I(8), I(0), I(16), F(0)}},
                   {10, {I(10), I(27), I(0), I(0), I(0), I(0), F(0.03)}},
                   {22, {I(22), I(26), I(0), I(0), I(0), I(0), F(0.05)}},
                   {24, {I(24), I(14), I(0), I(0), I(0), I(0), F(0)}},
                   {31, {I(31), I(20), I(0), I(0), I(0), I(0), F(0)}},
                   {38, {I(38), I(46), I(0), I(0), I(0), I(0), F(0.02)}},
                   {45, {I(45), I(24), I(0), I(0), I(0), I(0), F(0)}}})},
        {14,
         Table({"ID", "ModelType", "ModelSubType", "ModelVariation"}, {{4, {I(4), S(L"glv"), S(L"med"), S(L"d")}}})},
        {20,
         Table({"ID", "ModelType", "ModelSubType", "ModelVariation"}, {{4, {I(4), S(L"boo"), S(L"med"), S(L"d")}}})},
        {24,
         Table({"ID", "ModelType", "ModelSubType", "ModelVariation", "FullFace"},
               {{1, {I(1), S(L"hlm"), S(L"med"), S(L"a"), I(0)}}, {2, {I(2), S(L"hlm"), S(L"med"), S(L"b"), I(1)}}})},
        {26,
         Table({"ID", "ModelType", "ModelSubType", "ModelVariation"}, {{1, {I(1), S(L"arm"), S(L"mas"), S(L"a")}}})},
        {27,
         Table({"ID", "ModelType", "ModelSubType", "ModelVariation"}, {{4, {I(4), S(L"ARM"), S(L"MED"), S(L"d")}}})},
        {30,
         Table({"ID", "ModelType", "ModelSubType", "ModelVariation"}, {{3, {I(3), S(L"lsw"), S(L"ls03"), S(L"a")}}})},
        {39,
         Table({"ID", "ModelType", "ModelSubType", "ModelVariation"}, {{1, {I(1), S(L"shd"), S(L"kit"), S(L"a")}}})},
        {46,
         Table({"ID", "ModelType", "ModelSubType", "ModelVariation"}, {{1, {I(1), S(L"cth"), S(L"nob"), S(L"a")}}})},
        {80,
         Table({"ID", "ModelType", "ModelSubType", "ModelVariation"}, {{0, {I(0), S(L"arm"), S(L"nud"), S(L"a")}},
                                                                       {1, {I(1), S(L"glv"), S(L"nud"), S(L"a")}},
                                                                       {2, {I(2), S(L"boo"), S(L"nud"), S(L"a")}}})},
        {91, Table({"ID", "AttachNode", "WeaponOffset"}, {{5, {I(5), S(L"Prop10"), I(1)}},
                                                          {8, {I(8), S(L"Prop11"), I(1)}},
                                                          {10, {I(10), S(L"Prop07"), I(1)}},
                                                          {14, {I(14), S(L"Prop01"), I(0)}},
                                                          {15, {I(15), S(L"Prop02"), I(0)}},
                                                          {16, {I(16), S(L"Prop03"), I(0)}}})},
        {92, Table({"ID", "Gender"}, {{1, {I(1), S(L"m")}}, {2, {I(2), S(L"f")}}})},
        // The tint tables' rows as the game's: 0 INVALID with no file, then a file each.
        {81, Table({"ID", "Label", "MaskFile", "Apr"}, {{0, {I(0), S(L"INVALID"), S(L""), I(0)}},
                                                        {4, {I(4), S(L"Hurlock"), S(L"CN_HED_HLKa_0t.dds"), I(16)}}})},
        {82, Table({"ID", "Label", "MaskFile", "Apr"}, {{3, {I(3), S(L"Hurlock"), S(L"CN_BDY_HLKa_0t.dds"), I(16)}}})},
        {83, Table({"ID", "Label", "TintFile", "Apr"},
                   {{0, {I(0), S(L"INVALID"), S(L""), I(0)}}, {3, {I(3), S(L"Blue"), S(L"Blue.tnt"), I(0)}}})},
        {84, Table({"ID", "Label", "TintFile", "Apr"}, {{1, {I(1), S(L"Red"), S(L"Red.tnt"), I(0)}}})},
        {85, Table({"ID", "Label", "TintFile", "Apr"}, {{2, {I(2), S(L"Green"), S(L"Green.tnt"), I(0)}}})},
        {86, Table({"ID", "Label", "TintFile", "Apr"}, {{13, {I(13), S(L"Gold"), S(L"T3_HLK_H01.tnt"), I(16)}}})},
        {88, Table({"ID", "Label", "TintFile", "Apr"}, {{5, {I(5), S(L"Abomination"), S(L"T3_ABM_B02.tnt"), I(25)}}})},
    };

    character::TableOf Of() const
    {
        return [this](std::int64_t id) -> const m2da::Merged * {
            const auto found = byId.find(id);
            return found != byId.end() ? &found->second : nullptr;
        };
    }
};

character::Item Of(std::int32_t base, std::int32_t variation, std::int32_t progress = 0)
{
    character::Item item;
    item.baseItem = base;
    item.variation = variation;
    item.materialProgress = progress;
    return item;
}

// Items by name, as their .uti files would give them.
character::ItemOfName Items()
{
    return [](std::string_view name) -> std::optional<character::Item> {
        static const std::map<std::string, character::Item, std::less<>> items{
            {"sword", Of(2, 3)},  {"shield", Of(6, 1)},    {"chest", Of(10, 4, 46)}, {"gloves", Of(24, 4)},
            {"boots", Of(31, 4)}, {"helmet", Of(45, 1)},   {"visor", Of(45, 2)},     {"robe", Of(38, 1)},
            {"plate", Of(22, 1)}, {"unknown", Of(10, 99)},
        };
        const auto found = items.find(name);
        return found != items.end() ? std::optional(found->second) : std::nullopt;
    };
}

character::Template Human(std::uint8_t gender, std::initializer_list<std::pair<std::int32_t, const char *>> worn)
{
    character::Template creature;
    creature.appearance = 15;
    creature.gender = gender;
    creature.headMorph = "hm_genfl_alistair.mor";
    creature.equipment.reserve(worn.size());
    for (const auto &[slot, item] : worn)
    {
        creature.equipment.push_back({.inventorySlot = slot, .item = item});
    }
    return creature;
}

std::vector<std::wstring> Models(const character::Character &c)
{
    std::vector<std::wstring> models;
    models.reserve(c.body.size());
    for (const auto &part : c.body)
    {
        models.push_back(part.model);
    }
    return models;
}

} // namespace

TEST_CASE("a humanoid's body parts, each its item's or the bare body's", "[character]")
{
    const Tables tables;
    const auto full =
        character::Resolve(Human(1, {{4, "chest"}, {7, "gloves"}, {6, "boots"}, {9, "ring"}}), Items(), tables.Of());
    REQUIRE(full);
    CHECK(full->parts);
    CHECK(full->prefix == L"hm");
    CHECK(Models(*full) == std::vector<std::wstring>{L"hm_arm_medd", L"hm_glv_medd", L"hm_boo_medd"});
    REQUIRE(full->body[0].item);
    CHECK(full->body[0].item->materialProgress == 46);
    CHECK(full->headMorph == "hm_genfl_alistair.mor");
    CHECK(full->head.neck == L"hm_hed_dcpa");

    const auto bare = character::Resolve(Human(2, {{4, "unknown"}}), Items(), tables.Of());
    REQUIRE(bare);
    CHECK(bare->prefix == L"hf");
    CHECK(Models(*bare) == std::vector<std::wstring>{L"hf_arm_nuda", L"hf_glv_nuda", L"hf_boo_nuda"});
    CHECK_FALSE(bare->body[0].item); // a variation with no row is the bare body
}

TEST_CASE("clothing is drawn alone, massive armour with no boots without them", "[character]")
{
    const Tables tables;
    const auto robe = character::Resolve(Human(1, {{4, "robe"}, {7, "gloves"}, {6, "boots"}}), Items(), tables.Of());
    REQUIRE(robe);
    CHECK(Models(*robe) == std::vector<std::wstring>{L"hm_cth_noba"});
    const auto plate = character::Resolve(Human(1, {{4, "plate"}, {6, "boots"}}), Items(), tables.Of());
    REQUIRE(plate);
    CHECK(Models(*plate) == std::vector<std::wstring>{L"hm_arm_masa", L"hm_glv_nuda"});
}

TEST_CASE("a helmet hides the hair, a full-face one all but the eyes", "[character]")
{
    const Tables tables;
    const auto open = character::Resolve(Human(1, {{5, "helmet"}}), Items(), tables.Of());
    REQUIRE(open);
    CHECK(open->head.helmet == L"hm_hlm_meda");
    REQUIRE(open->head.helmetItem);
    CHECK(open->head.helmetItem->baseItem == 45);
    CHECK(open->head.helmetItem->variation == 1);
    CHECK_FALSE(open->head.hair);
    CHECK(open->head.face);
    CHECK(open->head.beard);
    CHECK_FALSE(open->head.fullFace);
    const auto visor = character::Resolve(Human(1, {{5, "visor"}}), Items(), tables.Of());
    REQUIRE(visor);
    CHECK(visor->head.fullFace);
    CHECK_FALSE(visor->head.face);
    CHECK_FALSE(visor->head.beard);
    CHECK_FALSE(visor->head.lashes);
    const auto none = character::Resolve(Human(1, {}), Items(), tables.Of());
    REQUIRE(none);
    CHECK(none->head.helmet.empty());
    CHECK_FALSE(none->head.helmetItem);
    CHECK(none->head.hair);
    CHECK(character::FullFaceEyes(L"cn_eye_glka") == L"cn_eye_glk2a");
}

TEST_CASE("weapons hang sheathed and drawn from their base type's nodes, off the back by the offsets", "[character]")
{
    const Tables tables;
    const auto armed = character::Resolve(Human(1, {{0, "sword"}, {1, "shield"}, {4, "chest"}}), Items(), tables.Of());
    REQUIRE(armed);
    REQUIRE(armed->weapons.size() == 2);
    const auto &sword = armed->weapons[0];
    CHECK(sword.slot == character::Slot::Main);
    CHECK(sword.model == L"w_lsw_ls03a");
    CHECK(sword.sheathed == L"Prop10");
    CHECK(sword.drawn == L"Prop01");
    CHECK(sword.sheathedOffset);
    CHECK_FALSE(sword.drawnOffset);
    CHECK(sword.offset == Approx(0.03)); // the medium chest's
    const auto &shield = armed->weapons[1];
    CHECK(shield.slot == character::Slot::Offhand);
    CHECK(shield.model == L"w_shd_kita");
    CHECK(shield.sheathed == L"Prop11"); // the kite shield's WearL 8, the mid right back
    CHECK(shield.drawn == L"Prop03");    // its WieldL 16, the left forearm
    const auto bare = character::Resolve(Human(1, {{0, "sword"}}), Items(), tables.Of());
    REQUIRE(bare);
    CHECK(bare->weapons[0].offset == Approx(0.02)); // no chest counts as 0.02
}

TEST_CASE("a head with no morph is its template's rows of the head worksheets", "[character]")
{
    const Tables tables;
    auto bald = Human(1, {});
    bald.headRows[0] = 2; // the face
    bald.headRows[2] = 1; // the hair
    const auto head = character::Resolve(bald, Items(), tables.Of());
    REQUIRE(head);
    CHECK(head->head.faceModel == L"hm_uhm_basb");
    CHECK(head->head.hairModel == L"hm_har_blda");
    CHECK(head->head.eyesModel.empty());  // apr_base names no eyes worksheet here
    CHECK(head->head.beardModel.empty()); // and the template no beard row
    bald.headRows[0] = 7;                 // a row the worksheet does not have
    CHECK(character::Resolve(bald, Items(), tables.Of())->head.faceModel.empty());
}

TEST_CASE("a template's head rows are its PartList's, each by its element's struct id", "[character]")
{
    // default_hm_noble.utc's shape: Appearance Data a struct, its PartList elements in no order
    // of their own, each its part by its struct id and its row by its PartVariation.
    dao::test::files::Gff3 b("UTC");
    const auto top = b.Struct(0xFFFFFFFF, {});
    const auto hair = b.Struct(2, {{"PartVariation", 0, 3}});
    const auto face = b.Struct(0, {{"PartVariation", 0, 1}});
    const auto looks = b.Struct(1, {{"Appearance_Type", 4, 2},
                                    {"Gender", 0, 1},
                                    {"HeadMorph", 11, b.ResRef("")},
                                    {"PartList", 15, b.List({hair, face})}});
    b.Set(top, {{"Appearance Data", 14, looks}, {"Equip_ItemList", 15, b.List({})}});
    const auto bytes = b.Done();
    const auto file = dao::core::gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto creature = character::TemplateOf(*file);
    REQUIRE(creature);
    CHECK(creature->appearance == 2);
    CHECK(creature->headRows == std::array<std::uint8_t, 6>{1, character::kNoRow, 3, character::kNoRow,
                                                            character::kNoRow, character::kNoRow});
}

TEST_CASE("a template's tint fields are files of their tint tables, none for row 0", "[character]")
{
    const Tables tables;
    auto tinted = Human(1, {});
    tinted.headMorph.clear();
    tinted.skinTint = 2;
    tinted.hairColor = 1;
    tinted.eyesColor = 3;
    tinted.tattooTint = 13;
    tinted.tattoo = 4;
    tinted.bodyTint = 5;
    tinted.bodyTintMask = 3;
    const auto read = character::Resolve(tinted, Items(), tables.Of());
    REQUIRE(read);
    CHECK(read->tints.skin == L"Green.tnt");
    CHECK(read->tints.hair == L"Red.tnt");
    CHECK(read->tints.eyes == L"Blue.tnt");
    CHECK(read->tints.tattoo == L"T3_HLK_H01.tnt");
    CHECK(read->tints.tattooMask == L"CN_HED_HLKa_0t.dds");
    CHECK(read->tints.body == L"T3_ABM_B02.tnt");
    CHECK(read->tints.bodyMask == L"CN_BDY_HLKa_0t.dds");
    tinted.eyesColor = 0; // INVALID
    tinted.tattoo = 0;
    tinted.skinTint = 7; // a row the table has not
    const auto bare = character::Resolve(tinted, Items(), tables.Of());
    REQUIRE(bare);
    CHECK(bare->tints.eyes.empty());
    CHECK(bare->tints.tattooMask.empty());
    CHECK(bare->tints.skin.empty());
    character::Template ogre;
    ogre.appearance = 40;
    ogre.eyesColor = 3;
    CHECK(character::Resolve(ogre, Items(), tables.Of())->tints.eyes == L"Blue.tnt"); // one model too
}

TEST_CASE("a creature not built from parts is its one model", "[character]")
{
    const Tables tables;
    character::Template ogre;
    ogre.appearance = 40;
    const auto one = character::Resolve(ogre, Items(), tables.Of());
    REQUIRE(one);
    CHECK_FALSE(one->parts);
    CHECK(one->model == L"c_ogre");
    CHECK(one->body.empty());
    character::Template nothing;
    nothing.appearance = 999;
    CHECK_FALSE(character::Resolve(nothing, Items(), tables.Of()));
    CHECK(character::SlotOf(character::kInventoryAmmo) == character::Slot::Offhand);
    CHECK_FALSE(character::SlotOf(9)); // a ring is not drawn
}

TEST_CASE("an H creature is its body model whole with a head of parts, weapons hung, no armour or helmet",
          "[character]")
{
    const Tables tables;
    character::Template genlock;
    genlock.appearance = 50;
    genlock.gender = 1;
    genlock.headRows[0] = 2;
    genlock.equipment = {{.inventorySlot = character::kInventoryChest, .item = "chest"},
                         {.inventorySlot = character::kInventoryGloves, .item = "gloves"},
                         {.inventorySlot = character::kInventoryHead, .item = "visor"},
                         {.inventorySlot = character::kInventoryMain, .item = "sword"}};
    const auto resolved = character::Resolve(genlock, Items(), tables.Of());
    REQUIRE(resolved);
    CHECK_FALSE(resolved->parts);
    CHECK(resolved->model == L"cn_bdy_glka"); // the body whole: no chest, gloves or boots parts
    CHECK(resolved->body.empty());
    CHECK(resolved->prefix == L"cn"); // ModelGenderOverride N over the gender's letter
    CHECK(resolved->head.neck == L"cn_hed_dgla");
    CHECK(resolved->head.faceModel == L"cn_uhm_basb");
    CHECK(resolved->head.helmet.empty()); // 0x0074B010 takes a helmet for a P creature only
    CHECK(resolved->head.hair);
    CHECK(resolved->head.face);
    REQUIRE(resolved->weapons.size() == 1);
    CHECK(resolved->weapons[0].model == L"w_lsw_ls03a");
    CHECK(resolved->weapons[0].sheathed == L"Prop10");
}

namespace
{

std::vector<std::byte> FileBytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(chars.size());
    std::memcpy(bytes.data(), chars.data(), chars.size());
    return bytes;
}

// A darkspawn template of the folder resolved, its H checks made; nothing if the file is not there.
std::optional<character::Character> ResolvedIn(const std::filesystem::path &folder, const char *name,
                                               const character::ItemOfName &itemOf, const character::TableOf &tableOf)
{
    if (!std::filesystem::exists(folder / name))
    {
        return std::nullopt;
    }
    const auto bytes = FileBytes(folder / name);
    const auto file = dao::core::gff3::File::Parse(bytes);
    REQUIRE(file);
    const auto creature = character::TemplateOf(*file);
    REQUIRE(creature);
    auto resolved = character::Resolve(*creature, itemOf, tableOf);
    REQUIRE(resolved);
    CHECK_FALSE(resolved->parts);
    CHECK(resolved->body.empty());
    CHECK(resolved->prefix == L"cn");
    CHECK_FALSE(resolved->head.neck.empty());
    CHECK(resolved->head.helmet.empty());
    return resolved;
}

} // namespace

TEST_CASE("Alistair in game/characters, when there, is the parts paragon-agent drew him with",
          "[character][game][folder]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "characters";
    if (!std::filesystem::exists(folder / "gen00fl_alistair.utc") || !std::filesystem::exists(folder / "m2da_base.gda"))
    {
        SKIP("no game/characters (ghilana's extract of Alistair's template, items and tables writes it)");
    }
    // Each table's files merged as the game merges them, by name in their
    // folder's order: enough for the core's own tables here.
    std::map<std::wstring, std::vector<std::byte>> files;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() == ".gda")
        {
            files.emplace(entry.path().stem().wstring(), FileBytes(entry.path()));
        }
    }
    std::map<std::wstring, GdaFile> tables;
    for (const auto &[stem, bytes] : files)
    {
        if (auto table = GdaFile::Parse(bytes))
        {
            tables.emplace(stem, std::move(*table));
        }
    }
    const auto sheets = m2da::Worksheets(tables.at(L"m2da_base"), true);
    std::map<std::int64_t, m2da::Merged> merged;
    for (const auto &sheet : sheets)
    {
        std::vector<const GdaFile *> parts;
        for (const auto &[stem, table] : tables)
        {
            if (m2da::Takes(sheet, stem))
            {
                parts.push_back(&table);
            }
        }
        if (!parts.empty())
        {
            merged.emplace(sheet.id, m2da::Merge(parts));
        }
    }
    const auto tableOf = [&](std::int64_t id) -> const m2da::Merged * {
        const auto found = merged.find(id);
        return found != merged.end() ? &found->second : nullptr;
    };
    const auto itemOf = [&](std::string_view name) -> std::optional<character::Item> {
        const auto bytes = FileBytes(folder / (std::string(name) + ".uti"));
        const auto file = dao::core::gff3::File::Parse(bytes);
        return file ? character::ItemOf(*file) : std::nullopt;
    };
    const auto bytes = FileBytes(folder / "gen00fl_alistair.utc");
    const auto utc = dao::core::gff3::File::Parse(bytes);
    REQUIRE(utc);
    const auto alistair = character::TemplateOf(*utc);
    REQUIRE(alistair);
    CHECK(alistair->appearance == 15);
    CHECK(alistair->headMorph == "hm_genfl_alistair.mor");
    CHECK(alistair->hairColor == 1);
    REQUIRE(alistair->equipment.size() == 6);
    CHECK(alistair->equipment[0].inventorySlot == character::kInventoryOffhand);
    const auto parts = character::Resolve(*alistair, itemOf, tableOf);
    REQUIRE(parts);
    CHECK(Models(*parts) == std::vector<std::wstring>{L"hm_arm_medd", L"hm_glv_medd", L"hm_boo_medd"});
    REQUIRE(parts->weapons.size() == 2);
    CHECK(parts->weapons[0].model == L"w_lsw_ls03a");
    CHECK(parts->weapons[0].sheathed == L"Prop10");
    CHECK(parts->weapons[0].drawn == L"Prop01");
    CHECK(parts->weapons[1].sheathed == L"Prop11");
    CHECK(parts->weapons[1].drawn == L"Prop03"); // a kite shield is drawn onto the left forearm
    CHECK(parts->head.neck == L"hm_hed_dcpa");
    CHECK(parts->head.hair);
    CHECK(parts->head.faceModel.empty()); // his PartList names no part: his morph is his head

    // The elf noble's origin template has no morph: its PartList gives the face, eyes and hair row
    // 1 each by their struct ids 0, 1 and 2, and no beard; elf_hair's row 1 is the bald model.
    const auto nobleBytes = FileBytes(folder / "default_hm_noble.utc");
    const auto nobleFile = dao::core::gff3::File::Parse(nobleBytes);
    REQUIRE(nobleFile);
    const auto noble = character::TemplateOf(*nobleFile);
    REQUIRE(noble);
    CHECK(noble->headMorph.empty());
    CHECK(noble->headRows ==
          std::array<std::uint8_t, 6>{1, 1, 1, character::kNoRow, character::kNoRow, character::kNoRow});
    const auto elf = character::Resolve(*noble, itemOf, tableOf);
    REQUIRE(elf);
    CHECK(elf->prefix == L"em");
    CHECK(elf->head.faceModel == L"em_uhm_basa");
    CHECK(elf->head.hairModel == L"em_har_blda");
    CHECK_FALSE(elf->head.eyesModel.empty());
    CHECK(elf->head.beardModel.empty());
    // Its tint fields are rows 0 but EyesColor 3: eye_tints' Blue.tnt and no other file.
    CHECK(noble->eyesColor == 3);
    CHECK(elf->tints.eyes == L"Blue.tnt");
    CHECK(elf->tints.skin.empty());
    CHECK(elf->tints.hair.empty());
    CHECK(elf->tints.tattooMask.empty());
    CHECK(elf->tints.body.empty());
    CHECK(parts->tints.hair == L"Red.tnt"); // Alistair's HairColor 1, which his morph's tints stand over

    // The Korcari Wilds' darkspawn (DAOLib #216) are ModelType H: their body is ModelName whole
    // and their head is parts, the weapons hang, and no armour part or helmet is drawn.
    if (const auto genlock = ResolvedIn(folder, "pre200cr_genlock_archer.utc", itemOf, tableOf))
    {
        if (tableOf(65) != nullptr) // genlock_heads.gda: PartList's face row 1 is HED_GLK1a
        {
            CHECK(genlock->head.faceModel == L"cn_hed_glk1a");
        }
        CHECK(genlock->model == L"cn_bdy_glka");
        CHECK(genlock->head.neck == L"cn_hed_dgla");
        CHECK_FALSE(genlock->weapons.empty());
    }
    if (const auto hurlock = ResolvedIn(folder, "pre200cr_hurlock_alpha.utc", itemOf, tableOf))
    {
        CHECK(hurlock->model == L"cn_bdy_hlkb");
    }
}
