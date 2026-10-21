// dao/core/Savegame.h over a small save made here, laid out as the game's are (tools/gff4.py --templates
// on one), and over the player's saves in game/saves when there (DAOLib #202).
#include "dao/core/Savegame.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <variant>
#include <vector>

namespace gff4 = dao::core::gff4;
namespace sg = dao::core::savegame;
using Catch::Approx;
using dao::test::Builder;

using dao::test::TemplateSpec;

namespace
{

constexpr std::uint16_t kS = gff4::kStruct | gff4::kReference; // a struct held by reference
constexpr std::uint16_t kL = gff4::kList;
constexpr std::uint16_t kSL = gff4::kList | gff4::kStruct;         // a list of structs held in place
constexpr std::uint16_t kGenList = gff4::kList | gff4::kReference; // a generic list of references
constexpr std::uint16_t kU8 = 0;
constexpr std::uint16_t kS8 = 1;
constexpr std::uint16_t kU16 = 2;
constexpr std::uint16_t kU32 = 4;
constexpr std::uint16_t kS32 = 5;
constexpr std::uint16_t kF32 = 8;
constexpr std::uint16_t kVec3 = 10;
constexpr std::uint16_t kQuat = 13;
constexpr std::uint16_t kStr = 14;
constexpr std::uint16_t kTlk = 17;

// The templates, in the order the game's save has the kinds it needs here.
enum Tpl : std::uint16_t
{
    kTop,
    kModu,
    kGmg1,
    kJrnl,
    kQest,
    kPdst,
    kWrld,
    kSvi1,
    kSvs1,
    kSvo1,
    kAre1,
    kPlc1,
    kCre1,
    kItl1,
    kWpt1,
    kCrs1,
    kAttr,
    kDplt,
    kTrgr,
    kTrat,
    kPlyr,
    kCrp1,
    kCps1,
    kCqs1,
    kPrty,
    kCrl1,
    kPlmr,
    kPlen,
    kGmst,
    kAims,
    kEbas,
    kWldt,
    kVers,
    kPtaa,
    kPtai,
    kPtaj,
};

std::vector<TemplateSpec> Templates()
{
    return {
        {"",
         24,
         {{16000, kModu, kS, 0},
          {16001, 0xFFFF, kGenList, 4},
          {16002, kPlyr, kS, 8},
          {16003, kPrty, kS, 12},
          {16004, kVers, kS, 16},
          {16005, kGmst, kS, 20}}},
        {"MODU",
         40,
         {{23, kS32, 0, 0},
          {16014, kStr, 0, 4},
          {16450, kGmg1, kSL, 8},
          {16504, kJrnl, kS, 12},
          {17000, 0xFFFF, kGenList, 16},
          {17101, kStr, 0, 20},
          {17102, kVec3, 0, 24},
          {16780, kWrld, kS, 36},
          {16840, kPtaa, kS, 40}}},
        {"GMG1", 8, {{16451, kU32, 0, 0}, {16452, kU32, kL, 4}}},
        {"JRNL", 16, {{16505, kQest, kSL, 0}, {16506, kQest, kSL, 4}, {16510, kStr, 0, 8}, {16513, kPdst, kSL, 12}}},
        {"QEST",
         24,
         {{16507, kTlk, 0, 0}, {16509, kStr, 0, 8}, {16510, kStr, 0, 12}, {16523, kU8, 0, 16}, {16524, kStr, 0, 20}}},
        {"PDST", 8, {{16514, kStr, 0, 0}, {16515, kStr, kL, 4}}},
        {"WRLD", 4, {{16781, kStr, 0, 0}}},
        {"SVI1", 8, {{17001, kStr, 0, 0}, {17003, kS32, 0, 4}}},
        {"SVS1", 8, {{17001, kStr, 0, 0}, {17003, kStr, 0, 4}}},
        {"SVO1", 8, {{17001, kStr, 0, 0}, {17003, kU32, 0, 4}}},
        {"ARE1",
         32,
         {{1, kStr, 0, 0},
          {23, kU32, 0, 4},
          {16010, kPlc1, kSL, 8},
          {16011, kCre1, kSL, 12},
          {16012, 0xFFFF, kGenList, 16},
          {16015, kWpt1, kSL, 20},
          {16016, kU8, kL, 24},
          {16018, kS32, kL, 28}}},
        {"PLC1",
         44,
         {{3, kStr, 0, 0},
          {4, kVec3, 0, 4},
          {5, kQuat, 0, 16},
          {23, kS32, 0, 32},
          {16455, kU8, 0, 36},
          {16223, kItl1, kSL, 40}}},
        {"CRE1",
         52,
         {{3, kStr, 0, 0},
          {4, kVec3, 0, 4},
          {5, kQuat, 0, 16},
          {23, kS32, 0, 32},
          {16201, kU8, 0, 36},
          {16209, kCrs1, kS, 40},
          {16219, kItl1, kSL, 44},
          {16455, kU8, 0, 48}}},
        {"ITL1", 24, {{3, kStr, 0, 0}, {4, kVec3, 0, 4}, {23, kS32, 0, 16}, {1002, kS32, 0, 20}}},
        {"WPT1",
         44,
         {{4, kVec3, 0, 0},
          {5, kQuat, 0, 12},
          {23, kS32, 0, 28},
          {16710, kU8, 0, 32},
          {16711, kU8, 0, 33},
          {16714, kTlk, 0, 36}}},
        {"CRS1", 8, {{16305, kU32, kL, 0}, {16350, 0xFFFF, kGenList, 4}}},
        {"ATTR", 12, {{16300, kF32, 0, 0}, {16301, kF32, 0, 4}, {16353, kU16, 0, 8}}},
        {"DPLT", 16, {{16300, kF32, 0, 0}, {16301, kF32, 0, 4}, {16302, kF32, 0, 8}, {16353, kU16, 0, 12}}},
        {"TRGR", 16, {{3, kStr, 0, 0}, {23, kS32, 0, 4}, {16101, kVec3, kL, 8}, {16112, kS32, 0, 12}}},
        {"TRAT",
         20,
         {{3, kStr, 0, 0}, {23, kS32, 0, 4}, {16101, kVec3, kL, 8}, {16110, kS32, 0, 12}, {16112, kS32, 0, 16}}},
        {"PLYR", 8, {{16208, kCrp1, kS, 0}, {16298, kS32, 0, 4}}},
        {"CRP1",
         28,
         {{3, kStr, 0, 0},
          {23, kS32, 0, 4},
          {16203, kS8, 0, 8},
          {16209, kCps1, kS, 12},
          {16950, kU8, kL, 16},
          {16255, kTlk, 0, 20}}},
        {"CPS1", 8, {{16305, kU32, kL, 0}, {16308, kCqs1, kSL, 4}}},
        {"CQS1", 8, {{16311, kU32, 0, 0}, {16331, kS8, 0, 4}}},
        {"PRTY",
         20,
         {{16204, kCrl1, kSL, 0},
          {16212, kS32, 0, 4},
          {16278, kU32, 0, 8},
          {16400, kPlmr, kS, 12},
          {16503, kU32, kL, 16}}},
        {"CRL1", 12, {{3, kStr, 0, 0}, {23, kS32, 0, 4}, {16203, kS8, 0, 8}}},
        {"PLMR", 4, {{16401, kPlen, kSL, 0}}},
        {"PLEN",
         20,
         {{16402, kStr, 0, 0}, {16403, kU32, 0, 4}, {16404, kU32, 0, 8}, {16405, kU32, 0, 12}, {16406, kU32, 0, 16}}},
        {"GMST", 12, {{16626, kS32, 0, 0}, {16636, kAims, kS, 4}, {16700, kWldt, kS, 8}}},
        {"AIMS", 4, {{16630, 0xFFFF, kGenList, 0}}},
        {"EBAS",
         20,
         {{16631, kU32, 0, 0}, {16632, kU32, 0, 4}, {16633, kU32, 0, 8}, {16634, kU32, 0, 12}, {16635, kU32, 0, 16}}},
        {"WLDT", 8, {{16701, kS32, 0, 0}, {16702, kS32, 0, 4}}},
        {"VERS", 8, {{16007, kU8, 0, 0}, {16770, kS32, 0, 4}}},
        {"PTAA", 4, {{16843, 0xFFFF, kGenList, 0}}},
        {"PTAI", 8, {{16844, kU16, 0, 0}, {16845, kU16, 0, 2}, {16846, kS32, 0, 4}}},
        {"PTAJ", 4, {{16844, kU16, 0, 0}, {16845, kU16, 0, 2}}}, // no count field
    };
}

// A struct of `size` bytes at the data's end.
std::size_t New(Builder &b, std::size_t size)
{
    return dao::test::Struct(b, size);
}

std::uint32_t Rel(const Builder &b, std::size_t at)
{
    return static_cast<std::uint32_t>(at - b.data);
}

void Str(Builder &b, std::size_t at, std::u16string_view text)
{
    b.U32(at, b.String(text));
}

// A list of `count` structs of `size` bytes held in place: its data offset, and the first struct's place.
std::uint32_t InPlace(Builder &b, std::size_t size, std::uint32_t count, std::size_t &first)
{
    const std::size_t at = b.bytes.size();
    b.U32(at, count);
    b.U8(at + 4 + (size * count) - 1, 0);
    first = at + 4;
    return Rel(b, at);
}

// A list of U32 values.
std::uint32_t Numbers(Builder &b, const std::vector<std::uint32_t> &values)
{
    return b.Numbers(values);
}

void Vec(Builder &b, std::size_t at, const std::vector<float> &v)
{
    dao::test::Floats(b, at, v);
}

// A list of strings: a count and a data offset of each.
std::uint32_t Strings(Builder &b, const std::vector<std::u16string> &texts)
{
    std::vector<std::uint32_t> offsets;
    offsets.reserve(texts.size());
    for (const auto &t : texts)
    {
        offsets.push_back(b.String(t));
    }
    return b.Numbers(offsets);
}

std::vector<std::byte> MakeSave(const char *type = "SAVE")
{
    Builder b = dao::test::Start(type, Templates());
    const std::size_t top = New(b, 24);

    // The campaign
    const std::size_t modu = New(b, 44);
    b.U32(modu, 77);
    Str(b, modu + 4, u"campaign");
    std::size_t first = 0;
    b.U32(modu + 8, InPlace(b, 8, 1, first));
    b.U32(first, 5);
    b.U32(first + 4, Numbers(b, {6, 9}));
    const std::size_t jrnl = New(b, 16);
    b.U32(modu + 12, Rel(b, jrnl));
    b.U32(jrnl, InPlace(b, 24, 2, first));
    b.U32(first, 111); // a quest's title (strref)
    Str(b, first + 8, u"plot_a");
    Str(b, first + 12, u"quest_a");
    b.U8(first + 16, 0);
    Str(b, first + 20, u"grp");
    b.U32(first + 24, 222);
    Str(b, first + 32, u"plot_b");
    Str(b, first + 36, u"quest_b");
    b.U8(first + 40, 0);
    b.U32(jrnl + 4, InPlace(b, 24, 1, first));
    b.U32(first, 333);
    Str(b, first + 12, u"quest_c");
    b.U8(first + 16, 1);
    Str(b, jrnl + 8, u"quest_b");
    b.U32(jrnl + 12, InPlace(b, 8, 1, first));
    Str(b, first, u"dest_tag");
    b.U32(first + 4, Strings(b, {u"AAAA", u"BBBB"}));
    const std::size_t svi = New(b, 8);
    Str(b, svi, u"COUNT");
    b.U32(svi + 4, static_cast<std::uint32_t>(-3));
    const std::size_t svs = New(b, 8);
    Str(b, svs, u"NAME");
    Str(b, svs + 4, u"value");
    const std::size_t svo = New(b, 8);
    Str(b, svo, u"WHO");
    b.U32(svo + 4, 4242);
    b.U32(modu + 16, dao::test::Children(b, {{kSvi1, svi}, {kSvs1, svs}, {kSvo1, svo}}));
    Str(b, modu + 20, u"ar_area");
    Vec(b, modu + 24, {1, 2, 3});
    const std::size_t wrld = New(b, 4);
    Str(b, wrld, u"world_map");
    b.U32(modu + 36, Rel(b, wrld));
    const std::size_t ptaa = New(b, 4);
    b.U32(modu + 40, Rel(b, ptaa));
    const std::size_t counted = New(b, 8);
    b.U8(counted, 7);
    b.U32(counted + 4, 5);
    const std::size_t uncounted = New(b, 4);
    b.U8(uncounted, 8);
    b.U32(ptaa, dao::test::Children(b, {{kPtai, counted}, {kPtaj, uncounted}}));
    b.U32(top, Rel(b, modu));

    // An area: a placeable with an item, a creature with stats, a trigger of two kinds, a waypoint, a map
    const std::size_t area = New(b, 32);
    Str(b, area, u"ar_area");
    b.U32(area + 4, 9001);
    std::size_t plc = 0;
    b.U32(area + 8, InPlace(b, 44, 1, plc));
    Str(b, plc, u"chest");
    Vec(b, plc + 4, {1, 2, 3});
    Vec(b, plc + 16, {0, 0, 0.5F, 0.5F});
    b.U32(plc + 32, 500);
    b.U8(plc + 36, 1);
    std::size_t item = 0;
    b.U32(plc + 40, InPlace(b, 24, 1, item));
    Str(b, item, u"gift");
    Vec(b, item + 4, {4, 1, 0});
    b.U32(item + 16, 501);
    b.U32(item + 20, 3);
    std::size_t cre = 0;
    b.U32(area + 12, InPlace(b, 52, 1, cre));
    Str(b, cre, u"wolf");
    b.U32(cre + 32, 600);
    b.U8(cre + 36, 1);
    b.U8(cre + 48, 1);
    const std::size_t crs = New(b, 8);
    b.U32(crs, Numbers(b, {10, 20, 30}));
    const std::size_t attr = New(b, 12);
    Vec(b, attr, {10.0F, 2.0F});
    b.U8(attr + 8, 3);
    const std::size_t dplt = New(b, 16);
    Vec(b, dplt, {100.0F, 5.0F, 80.0F});
    b.U8(dplt + 12, 7);
    b.U32(crs + 4, dao::test::Children(b, {{kAttr, attr}, {kDplt, dplt}}));
    b.U32(cre + 40, Rel(b, crs));
    b.U32(cre + 44, InPlace(b, 24, 2, item));
    Str(b, item, u"sword");
    Vec(b, item + 4, {3, 1, 0});
    Str(b, item + 24, u"shield");
    Vec(b, item + 28, {4, 0, 0});
    const std::size_t trgr = New(b, 16);
    Str(b, trgr, u"trig");
    b.U32(trgr + 4, 700);
    std::size_t geometry = 0;
    {
        geometry = b.bytes.size();
        b.U32(geometry, 3);
        Vec(b, geometry + 4, {0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0});
        b.U32(trgr + 8, Rel(b, geometry));
    }
    b.U32(trgr + 12, 2);
    const std::size_t trat = New(b, 20);
    Str(b, trat, u"leave");
    b.U32(trat + 4, 701);
    b.U32(trat + 12, 9);
    b.U32(trat + 16, 4);
    b.U32(area + 16, dao::test::Children(b, {{kTrgr, trgr}, {kTrat, trat}}));
    std::size_t wpt = 0;
    b.U32(area + 20, InPlace(b, 44, 1, wpt));
    b.U32(wpt + 28, 800);
    b.U8(wpt + 32, 1);
    b.U8(wpt + 33, 1);
    b.U32(wpt + 36, 4444);
    Str(b, wpt + 40, u"a note");
    b.U32(area + 24, dao::test::Bytes(b, {0x01, 0x80, 0x00}));
    b.U32(area + 28, Numbers(b, {3, 4}));
    b.U32(top + 4, dao::test::Children(b, {{kAre1, area}}));

    // The hero
    const std::size_t plyr = New(b, 8);
    const std::size_t crp = New(b, 28);
    Str(b, crp, u"hero");
    b.U32(crp + 4, 1);
    b.U32(crp + 20, 5);
    Str(b, crp + 24, u"Aria"); // a TlkString's text is its second word
    b.U8(crp + 8, 1);
    b.U32(crp + 16, dao::test::Bytes(b, {'G', 'F', 'F'}));
    const std::size_t cps = New(b, 8);
    b.U32(cps, Numbers(b, {1, 2}));
    std::size_t qs = 0;
    b.U32(cps + 4, InPlace(b, 8, 1, qs));
    b.U32(qs, 99);
    b.U8(qs + 4, 3);
    b.U32(crp + 12, Rel(b, cps));
    b.U32(plyr, Rel(b, crp));
    b.U32(plyr + 4, 555);
    b.U32(top + 8, Rel(b, plyr));

    // The party
    const std::size_t prty = New(b, 20);
    std::size_t crl = 0;
    b.U32(prty, InPlace(b, 12, 1, crl));
    Str(b, crl, u"tamlen");
    b.U32(crl + 4, 269);
    b.U8(crl + 8, 1);
    b.U32(prty + 4, 1234);
    b.U32(prty + 8, 1);
    const std::size_t plmr = New(b, 4);
    std::size_t plen = 0;
    b.U32(plmr, InPlace(b, 20, 1, plen));
    Str(b, plen, u"0123456789ABCDEF0123456789ABCDEF");
    b.U32(plen + 4, 1);
    b.U32(plen + 16, 4);
    b.U32(prty + 12, Rel(b, plmr));
    b.U32(prty + 16, Numbers(b, {7, 8, 9}));
    b.U32(top + 12, Rel(b, prty));

    // The version and the game's state
    const std::size_t vers = New(b, 8);
    b.U8(vers, 1);
    b.U32(vers + 4, 13263);
    b.U32(top + 16, Rel(b, vers));
    const std::size_t gmst = New(b, 12);
    b.U32(gmst, 40);
    const std::size_t aims = New(b, 4);
    const std::size_t event = New(b, 20);
    b.U32(event, 5);
    b.U32(event + 4, 1000);
    b.U32(event + 8, 600);
    b.U32(event + 12, 700);
    b.U32(event + 16, 12);
    b.U32(aims, dao::test::Children(b, {{kEbas, event}}));
    b.U32(gmst + 4, Rel(b, aims));
    const std::size_t wldt = New(b, 8);
    b.U32(wldt, 3);
    b.U32(wldt + 4, 86400);
    b.U32(gmst + 8, Rel(b, wldt));
    b.U32(top + 20, Rel(b, gmst));
    return b.bytes;
}

std::vector<std::byte> FileBytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<char> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::byte> out(raw.size());
    std::memcpy(out.data(), raw.data(), raw.size());
    return out;
}

} // namespace

TEST_CASE("a save's campaign, journal and variables are mapped", "[savegame]")
{
    const auto bytes = MakeSave();
    const auto save = sg::Read(bytes);
    REQUIRE(save);
    const auto &c = save->campaign;
    CHECK(c.id == 77);
    CHECK(c.resource == L"campaign");
    REQUIRE(c.groups.size() == 1);
    CHECK(c.groups[0].id == 5);
    CHECK(c.groups[0].hostiles == std::vector<std::uint32_t>{6, 9});
    REQUIRE(c.journal.active.size() == 2);
    CHECK(c.journal.active[0].titleStrref == 111);
    CHECK(c.journal.active[0].resRef == L"quest_a");
    CHECK(c.journal.active[0].parentPlot == L"plot_a");
    CHECK(c.journal.active[0].group == L"grp");
    CHECK(c.journal.active[1].titleStrref == 222);
    REQUIRE(c.journal.completed.size() == 1);
    CHECK(c.journal.completed[0].completed == 1);
    CHECK(c.journal.activeResRef == L"quest_b");
    REQUIRE(c.journal.plotDestinations.size() == 1);
    CHECK(c.journal.plotDestinations[0].tag == L"dest_tag");
    CHECK(c.journal.plotDestinations[0].guids == std::vector<std::wstring>{L"AAAA", L"BBBB"});
    CHECK(c.worldMap.primaryMap == L"world_map");
    CHECK(c.entryArea == L"ar_area");
    CHECK(c.entryPosition == sg::Vec3{1, 2, 3});
    REQUIRE(c.variables.size() == 3);
    CHECK(c.variables[0].name == L"COUNT");
    CHECK(std::get<std::int32_t>(c.variables[0].value) == -3);
    CHECK(std::get<std::wstring>(c.variables[1].value) == L"value");
    CHECK(std::get<sg::ObjectId>(c.variables[2].value).id == 4242);
}

TEST_CASE("an area's objects, stats, items, map and rooms are mapped", "[savegame]")
{
    const auto bytes = MakeSave();
    const auto save = sg::Read(bytes);
    REQUIRE(save);
    REQUIRE(save->areas.size() == 1);
    const sg::Area *area = save->FindArea(L"ar_area");
    REQUIRE(area);
    CHECK(area->id == 9001);

    REQUIRE(area->placeables.size() == 1);
    const auto &chest = area->placeables[0];
    CHECK(chest.templateResRef == L"chest");
    CHECK(chest.position == sg::Vec3{1, 2, 3});
    CHECK(chest.orientation == sg::Quat{0, 0, 0.5F, 0.5F});
    CHECK(chest.id == 500);
    CHECK(chest.plotGiver == 1);
    REQUIRE(chest.items.size() == 1);
    CHECK(chest.items[0].id == 501);
    CHECK(chest.items[0].stackSize == 3);
    CHECK(chest.items[0].Slot() == 4);
    CHECK(chest.items[0].WeaponSet() == 1);

    REQUIRE(area->creatures.size() == 1);
    const auto &wolf = area->creatures[0];
    CHECK(wolf.kind == sg::Creature::Kind::Area);
    CHECK(wolf.id == 600);
    CHECK(wolf.active == 1);
    CHECK(wolf.plotGiver == 1);
    REQUIRE(wolf.stats);
    CHECK(wolf.stats->spells == std::vector<std::uint32_t>{10, 20, 30});
    REQUIRE(wolf.stats->stats.size() == 2);
    CHECK(wolf.stats->stats[0].kind == sg::Stat::Kind::Attribute);
    CHECK(wolf.stats->stats[0].index == 3);
    CHECK(*wolf.stats->stats[0].base == Approx(10));
    CHECK(*wolf.stats->stats[0].modifier == Approx(2));
    CHECK(wolf.stats->stats[1].kind == sg::Stat::Kind::Depletable);
    CHECK(*wolf.stats->stats[1].current == Approx(80));
    REQUIRE(wolf.stats->Find(7));
    CHECK(wolf.stats->Find(7)->kind == sg::Stat::Kind::Depletable); // by row, the label only says which fields
    CHECK_FALSE(wolf.stats->Find(8));
    REQUIRE(wolf.equipment);
    REQUIRE(wolf.equipment->size() == 2);
    CHECK((*wolf.equipment)[0].templateResRef == L"sword");
    CHECK((*wolf.equipment)[1].Slot() == 4);
    CHECK((*wolf.equipment)[1].WeaponSet() == 0);
    CHECK_FALSE(wolf.backpack); // the creature's struct has no such field: the template's stays
    CHECK_FALSE(wolf.maxItems);

    REQUIRE(area->triggers.size() == 2);
    CHECK(area->triggers[0].label == "TRGR");
    CHECK(area->triggers[0].KindOf() == sg::Trigger::Kind::Trap); // by the type field, 2
    CHECK(area->triggers[0].id == 700);
    CHECK(area->triggers[0].type == 2);
    REQUIRE(area->triggers[0].geometry.size() == 3);
    CHECK(area->triggers[0].geometry[2] == sg::Vec3{1, 1, 0});
    CHECK(area->triggers[1].label == "TRAT");
    CHECK(area->triggers[1].KindOf() == sg::Trigger::Kind::AudioVolume); // 4
    CHECK(area->triggers[1].loadScreen == 9);
    CHECK(area->triggers[1].type == 4);

    REQUIRE(area->waypoints.size() == 1);
    CHECK(area->waypoints[0].mapNote == 1);
    CHECK(area->waypoints[0].mapNoteEnabled == 1);
    CHECK(area->waypoints[0].id == 800);
    CHECK(area->waypoints[0].mapNoteTextStrref == 4444);
    CHECK(area->waypoints[0].mapNoteTextOwn == L"a note");
    CHECK_FALSE(area->waypoints[0].tag); // no field: the template's

    CHECK(area->roomsViewed == std::vector<std::int32_t>{3, 4});
    // Three bytes are 8 * 2 + 1 for a grid of 16 cells: 4 by 4 (bits 0, 15 set, not bit 8 of the last)
    auto map = area->map;
    REQUIRE(map.bits.size() == 3);
    CHECK_FALSE(map.Explored(0, 0)); // no size yet
    CHECK(map.SetSize(4, 4));
    CHECK(map.Explored(0, 0));
    CHECK(map.Explored(3, 3));
    CHECK_FALSE(map.Explored(1, 0));
    CHECK_FALSE(map.Explored(4, 0));
    CHECK(map.ExploredCount() == 2);
    CHECK_FALSE(map.SetSize(5, 5)); // 25 / 8 + 1 = 4 bytes
}

TEST_CASE("the hero, the party and the game's state are mapped", "[savegame]")
{
    const auto bytes = MakeSave();
    const auto save = sg::Read(bytes);
    REQUIRE(save);
    REQUIRE(save->hero);
    CHECK(save->hero->kind == sg::Creature::Kind::Hero);
    CHECK(save->hero->templateResRef == L"hero");
    CHECK(save->hero->partyMembers == 1);
    CHECK(save->hero->nameStrref == 5);
    CHECK(save->hero->nameText == L"Aria"); // the typed name, the string's own text
    REQUIRE(save->hero->stats);
    CHECK(save->hero->stats->hasQuickbar);
    CHECK(save->hero->stats->quickslots[0].size() == 1);
    CHECK(save->hero->stats->quickslots[0][0].ability == 99);
    CHECK(save->hero->stats->quickslots[0][0].number == 3);
    CHECK_FALSE(save->hero->stats->lockQbar);
    CHECK(save->hero->playerMorph.size() == 3);
    CHECK(save->timePlayedSeconds == 555);

    REQUIRE(save->party.pool.size() == 1);
    CHECK(save->party.pool[0].kind == sg::Creature::Kind::Pool);
    CHECK(save->party.pool[0].id == 269);
    CHECK(save->party.money == 1234);
    CHECK(save->party.leader == 1);
    CHECK(save->party.seenLines == std::vector<std::uint32_t>{7, 8, 9});
    REQUIRE(save->party.plots.size() == 1);
    const auto *plot = save->party.FindPlot(L"0123456789ABCDEF0123456789ABCDEF");
    REQUIRE(plot);
    CHECK(plot->flags[0] == 1);
    CHECK(plot->flags[3] == 4);

    CHECK(save->version.buildNumber == 13263);
    CHECK(save->version.cheatUsed == 1);
    CHECK(save->state.effectStartingId == 40);
    REQUIRE(save->state.eventQueue.size() == 1);
    CHECK(save->state.eventQueue[0].label == "EBAS");
    CHECK(save->state.eventQueue[0].KindOf() == sg::QueuedEvent::Kind::Plain); // id 12
    CHECK(save->state.eventQueue[0].day == 5);
    CHECK(save->state.eventQueue[0].time == 1000);
    CHECK(save->state.eventQueue[0].caller == 600);
    CHECK(save->state.eventQueue[0].target == 700);
    CHECK(save->state.eventQueue[0].id == 12);
    CHECK(save->state.worldDay == 3);
    CHECK(save->state.worldTimeMs == 86400);

    REQUIRE(save->FindCreature(600));
    REQUIRE(save->FindCreature(269));
    REQUIRE(save->FindCreature(1));
    CHECK_FALSE(save->FindCreature(31337));
}

namespace
{

// A small save for the places where the engine's reader differs from a plain one: an item's property
// lists behind its ability, an AOE's flags taking its ability, an effect's priority, a list present
// and empty against a list the template lacks, and no GMST.
std::vector<TemplateSpec> EngineTemplates()
{
    return {
        {"", 4, {{16001, 0xFFFF, kGenList, 0}}}, // 0 top
        {"ARE1",
         20,
         {{1, kStr, 0, 0}, {16010, 2, kSL, 4}, {16011, 4, kSL, 8}, {16013, 6, kSL, 12}, {16623, 7, kSL, 16}}},
        {"PLC1", 4, {{16223, 3, kSL, 0}}}, // 2
        {"ITL1",
         16,
         {{3, kStr, 0, 0}, {23, kS32, 0, 4}, {1011, kU32, 0, 8}, {1013, kS32, kL, 12}}}, // 3 with an ability
        {"CRE1", 4, {{16210, 5, kSL, 0}}},                                               // 4
        {"ITL1", 12, {{3, kStr, 0, 0}, {23, kS32, 0, 4}, {1013, kS32, kL, 8}}},          // 5 without
        {"AOE1", 8, {{16750, kS32, 0, 0}, {16751, kS32, 0, 4}}},                         // 6 with flags
        {"GME1", 12, {{16614, kS32, 0, 0}, {16615, kS32, 0, 4}, {16620, kS32, 0, 8}}},   // 7 with a priority
        {"ARE1", 12, {{1, kStr, 0, 0}, {16013, 9, kSL, 4}, {16623, 10, kSL, 8}}},        // 8
        {"AOE1", 4, {{16750, kS32, 0, 0}}},                                              // 9 without flags
        {"GME1", 8, {{16614, kS32, 0, 0}, {16615, kS32, 0, 4}}},                         // 10 without a priority
    };
}

std::vector<std::byte> MakeEngineSave()
{
    Builder b = dao::test::Start("SAVE", EngineTemplates());
    const std::size_t top = New(b, 4);
    const std::size_t a = New(b, 20);
    Str(b, a, u"a");
    std::size_t first = 0;
    std::size_t plc = 0;
    b.U32(a + 4, InPlace(b, 4, 1, plc));
    b.U32(plc, InPlace(b, 16, 1, first));
    Str(b, first, u"with_ability");
    b.U32(first + 4, 1);
    b.U32(first + 8, 7);
    b.U32(first + 12, Numbers(b, {10, 20}));
    std::size_t cre = 0;
    b.U32(a + 8, InPlace(b, 4, 2, cre));
    b.U32(cre, InPlace(b, 12, 1, first));
    Str(b, first, u"no_ability");
    b.U32(first + 4, 2);
    b.U32(first + 8, Numbers(b, {5}));
    b.U32(cre + 4, InPlace(b, 12, 0, first)); // present and empty
    std::size_t aoe = 0;
    b.U32(a + 12, InPlace(b, 8, 1, aoe));
    b.U32(aoe, 33);
    b.U32(aoe + 4, 44);
    std::size_t effect = 0;
    b.U32(a + 16, InPlace(b, 12, 1, effect));
    b.U32(effect, 1);
    b.U32(effect + 4, 6);
    b.U32(effect + 8, 5);
    const std::size_t area2 = New(b, 12);
    Str(b, area2, u"b");
    b.U32(area2 + 4, InPlace(b, 4, 1, aoe));
    b.U32(aoe, 33);
    b.U32(area2 + 8, InPlace(b, 8, 1, effect));
    b.U32(effect, 1);
    b.U32(effect + 4, 6);
    b.U32(top, dao::test::Children(b, {{1, a}, {8, area2}}));
    return b.bytes;
}

} // namespace

TEST_CASE("an item's property lists are read only when its ability is present, as the engine does", "[savegame]")
{
    const auto bytes = MakeEngineSave();
    const auto save = sg::Read(bytes);
    REQUIRE(save);
    const sg::Area *a = save->FindArea(L"a");
    REQUIRE(a);
    REQUIRE(a->placeables.size() == 1);
    REQUIRE(a->placeables[0].items.size() == 1);
    const auto &with = a->placeables[0].items[0];
    CHECK(with.abilityId == 7);
    CHECK(with.properties == std::vector<std::int32_t>{10, 20});
    REQUIRE(a->creatures.size() == 2);
    REQUIRE(a->creatures[0].backpack);
    REQUIRE(a->creatures[0].backpack->size() == 1);
    const auto &without = (*a->creatures[0].backpack)[0];
    CHECK_FALSE(without.abilityId);
    CHECK(without.properties.empty()); // the list is in the file; the engine does not read it
}

TEST_CASE("a list present and empty is told from a list the template lacks", "[savegame]")
{
    const auto bytes = MakeEngineSave();
    const auto save = sg::Read(bytes);
    REQUIRE(save);
    const sg::Area *a = save->FindArea(L"a");
    REQUIRE(a);
    REQUIRE(a->creatures.size() == 2);
    REQUIRE(a->creatures[1].backpack); // there, with nothing in it: the engine empties the backpack
    CHECK(a->creatures[1].backpack->empty());
    CHECK_FALSE(a->creatures[0].equipment); // the template lacks it: the creature's own stays
    CHECK_FALSE(a->creatures[0].plotItems);
}

TEST_CASE("an AOE with no flags takes its ability, as the engine's code does", "[savegame]")
{
    const auto bytes = MakeEngineSave();
    const auto save = sg::Read(bytes);
    REQUIRE(save);
    REQUIRE(save->FindArea(L"a")->aoes.size() == 1);
    CHECK(save->FindArea(L"a")->aoes[0].flags == 44);
    REQUIRE(save->FindArea(L"b")->aoes.size() == 1);
    CHECK(save->FindArea(L"b")->aoes[0].abilityId == 33);
    CHECK(save->FindArea(L"b")->aoes[0].flags == 33);
}

TEST_CASE("an effect's priority is 999 for duration type 6 unless the save has one", "[savegame]")
{
    const auto bytes = MakeEngineSave();
    const auto save = sg::Read(bytes);
    REQUIRE(save);
    REQUIRE(save->FindArea(L"a")->effects.size() == 1);
    CHECK(save->FindArea(L"a")->effects[0].Priority() == 5);
    REQUIRE(save->FindArea(L"b")->effects.size() == 1);
    CHECK_FALSE(save->FindArea(L"b")->effects[0].priority);
    CHECK(save->FindArea(L"b")->effects[0].Priority() == 999);
    sg::Effect other;
    other.durationType = 1;
    CHECK(other.Priority() == 0);
}

TEST_CASE("the effect counter is 1 where the save has no game state", "[savegame]")
{
    const auto bytes = MakeEngineSave();
    const auto save = sg::Read(bytes);
    REQUIRE(save);
    CHECK(save->state.effectStartingId == 1);
    CHECK(save->state.eventQueue.empty());
}

TEST_CASE("the engine drops an event by its handler id: 10 and 24 are script, seven are plain, the rest go",
          "[savegame]")
{
    for (std::uint32_t id = 0; id < 40; ++id)
    {
        sg::QueuedEvent event;
        event.id = id;
        sg::QueuedEvent::Kind want = sg::QueuedEvent::Kind::Dropped;
        if (id == 10 || id == 24)
        {
            want = sg::QueuedEvent::Kind::Script;
        }
        else if (id == 6 || id == 7 || id == 11 || id == 12 || id == 13 || id == 16 || id == 27)
        {
            want = sg::QueuedEvent::Kind::Plain;
        }
        CHECK(event.KindOf() == want);
    }
}

TEST_CASE("an area is found by its tag whatever the case, as the engine's area list finds it", "[savegame]")
{
    const auto bytes = MakeSave();
    const auto save = sg::Read(bytes);
    REQUIRE(save);
    CHECK(save->FindArea(L"AR_AREA") == save->FindArea(L"ar_area"));
    CHECK(save->FindArea(L"Ar_Area") != nullptr);
    CHECK(save->FindArea(L"ar_are") == nullptr);
}

TEST_CASE("a plot action's count is null where the save lacks it", "[savegame]")
{
    const auto bytes = MakeSave();
    const auto save = sg::Read(bytes);
    REQUIRE(save);
    const auto &list = save->campaign.plotActions.list;
    REQUIRE(list.size() == 2);
    CHECK(list[0].id == 7);
    REQUIRE(list[0].count);
    CHECK(*list[0].count == 5);
    CHECK(list[1].id == 8);
    CHECK_FALSE(list[1].count);
}

TEST_CASE("a file of another type is no save, and so are bytes that are no GFF4", "[savegame]")
{
    const auto other = MakeSave("SVMT");
    CHECK_FALSE(sg::Read(other));
    const std::vector<std::byte> junk(64, std::byte{0x41});
    CHECK_FALSE(sg::Read(junk));
    CHECK_FALSE(sg::Read(std::vector<std::byte>{}));
}

TEST_CASE("a save cut short gives what it has and never faults", "[savegame]")
{
    const auto bytes = MakeSave();
    std::size_t reads = 0;
    for (std::size_t length = 0; length < bytes.size(); length += 7)
    {
        const std::vector<std::byte> cut(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(length));
        if (const auto save = sg::Read(cut))
        {
            ++reads;
        }
    }
    CHECK(reads > 0); // the top struct survives while the data behind it does not
    // A cut that keeps the campaign but not the party reads the campaign.
    const std::vector<std::byte> most(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(bytes.size() - 40));
    const auto save = sg::Read(most);
    REQUIRE(save);
    CHECK(save->campaign.resource == L"campaign");
}

TEST_CASE("the player's saves in game/saves, when there, read with the counts tools/gff4.py --dump has",
          "[savegame][game]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "saves";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/saves (a copy of a save's .das)");
    }
    // Copies of the player's Lyna: a Dalish start at level 1 (Forest Clearing) and the post-campaign game.
    const auto start = folder / "lyna_level1.das";
    if (std::filesystem::exists(start))
    {
        const auto bytes = FileBytes(start);
        const auto save = sg::Read(bytes);
        REQUIRE(save);
        REQUIRE(save->areas.size() == 1);
        const auto &area = save->areas[0];
        CHECK(area.tag == L"bed100ar_forest_clearing");
        CHECK(area.Used());
        CHECK(area.placeables.size() == 10);
        CHECK(area.creatures.size() == 10);
        CHECK(area.triggers.size() == 13);
        CHECK(area.aoes.empty());
        CHECK(area.waypoints.size() == 9);
        CHECK(area.sounds.size() == 11);
        CHECK(area.stores.empty());
        CHECK_FALSE(area.map.bits.empty());
        CHECK_FALSE(area.roomsViewed.empty());
        CHECK(save->campaign.entryArea == L"bed100ar_forest_clearing");
        CHECK(save->campaign.journal.activeResRef == L"shl000pt_main");
        CHECK(save->campaign.journal.active.size() == 3);
        CHECK(save->campaign.journal.completed.empty());
        REQUIRE(save->hero);
        CHECK(save->hero->kind == sg::Creature::Kind::Hero);
        REQUIRE(save->hero->stats);
        CHECK(save->hero->stats->stats.size() == 32);
        CHECK(save->hero->stats->hasQuickbar);
        CHECK(save->hero->appearance);
        REQUIRE(save->hero->playerMorph.size() > 16);
        CHECK(save->hero->playerMorph[0] == 'G'); // "GFF V4.0 MORP"
        CHECK(save->party.pool.size() == 1);
        CHECK(save->party.plots.size() == 994);
        CHECK(save->version.buildNumber == 13263);
        REQUIRE(save->hero->id);
        CHECK(save->FindCreature(*save->hero->id) == &*save->hero);
        for (const auto &c : area.creatures)
        {
            CHECK(c.id);
            CHECK(c.templateResRef);
        }
        for (const auto &p : save->party.plots)
        {
            CHECK(p.guid.size() == 32); // upper-case hex, no dashes
        }
    }
    const auto late = folder / "lyna_postcampaign.das";
    if (std::filesystem::exists(late))
    {
        const auto bytes = FileBytes(late);
        const auto save = sg::Read(bytes);
        REQUIRE(save);
        CHECK(save->areas.size() == 157);
        CHECK(save->party.pool.size() == 8);
        CHECK(save->campaign.journal.activeResRef == L"gwb000pt_sideplot");
        CHECK(save->campaign.journal.active.size() == 1);
        CHECK(save->campaign.journal.completed.size() == 95);
        REQUIRE(save->hero);
        REQUIRE(save->hero->stats);
        CHECK(save->hero->stats->stats.size() > 32);
        CHECK(save->party.plots.size() == 994);
        CHECK(save->version.buildNumber == 13263);
    }
}
