// dao/core/Savegame.h over a small save made here, laid out as the game's are (tools/gff4.py --templates
// on one), and over the player's saves in game/saves when there (DAOLib #202), and its writer (DAOLib #221).
#include "dao/core/Savegame.h"
#include "gff4_builder.h"

#include <windows.h>

#include <shlobj.h>

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
    kEscr,
    kSce1,
    kCqs1NoNumber,
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
        {"CPS1", 12, {{16305, kU32, kL, 0}, {16308, kCqs1, kSL, 4}, {16313, kCqs1NoNumber, kSL, 8}}},
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
        {"ESCR",
         24,
         {{16631, kU32, 0, 0},
          {16632, kU32, 0, 4},
          {16633, kU32, 0, 8},
          {16634, kU32, 0, 12},
          {16635, kU32, 0, 16},
          {16650, kSce1, kS, 20}}},
        {"SCE1", 16, {{16670, kS32, 0, 0}, {16671, kU32, 0, 4}, {16672, kU32, 0, 8}, {16674, kStr, 0, 12}}},
        {"CQS1", 4, {{16311, kU32, 0, 0}}}, // a slot with no number, as no save of the game's has
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
    const std::size_t cps = New(b, 12);
    b.U32(cps, Numbers(b, {1, 2}));
    std::size_t qs = 0;
    b.U32(cps + 4, InPlace(b, 8, 1, qs));
    b.U32(qs, 99);
    b.U8(qs + 4, 3);
    b.U32(cps + 8, InPlace(b, 4, 1, qs));
    b.U32(qs, 98);
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
    const std::size_t sce = New(b, 16);
    b.U32(sce, 4);
    b.U32(sce + 4, 600);
    b.U32(sce + 8, 700);
    Str(b, sce + 12, u"script_a");
    const std::size_t escr = New(b, 24);
    b.U32(escr, 6);
    b.U32(escr + 4, 2000);
    b.U32(escr + 8, 600);
    b.U32(escr + 12, 700);
    b.U32(escr + 16, 10);
    b.U32(escr + 20, Rel(b, sce));
    b.U32(aims, dao::test::Children(b, {{kEbas, event}, {kEscr, escr}}));
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
    REQUIRE(save->hero->stats->quickslots[1].size() == 1);
    CHECK(save->hero->stats->quickslots[1][0].ability == 98);
    CHECK_FALSE(save->hero->stats->quickslots[1][0].number); // not slot 0
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
    REQUIRE(save->state.eventQueue.size() == 2);
    CHECK(save->state.eventQueue[0].label == "EBAS");
    CHECK(save->state.eventQueue[0].KindOf() == sg::QueuedEvent::Kind::Plain); // id 12
    CHECK(save->state.eventQueue[0].day == 5);
    CHECK(save->state.eventQueue[0].time == 1000);
    CHECK(save->state.eventQueue[0].caller == 600);
    CHECK(save->state.eventQueue[0].target == 700);
    CHECK(save->state.eventQueue[0].id == 12);
    CHECK(save->state.eventQueue[1].label == "ESCR");
    CHECK(save->state.eventQueue[1].id == 10);
    REQUIRE(save->state.eventQueue[1].script);
    CHECK(save->state.eventQueue[1].script->scriptName == L"script_a");
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

// -- The writer (DAOLib #221)

namespace
{

// The ranges of bytes that differ between a file and the file written from it, over the length of the first.
std::vector<std::pair<std::size_t, std::size_t>> Changes(const std::vector<std::byte> &a,
                                                         const std::vector<std::byte> &b)
{
    std::vector<std::pair<std::size_t, std::size_t>> out;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i)
    {
        if (a[i] == b[i])
        {
            continue;
        }
        if (!out.empty() && out.back().second == i)
        {
            ++out.back().second;
        }
        else
        {
            out.emplace_back(i, i + 1);
        }
    }
    return out;
}

std::size_t ChangedBytes(const std::vector<std::byte> &a, const std::vector<std::byte> &b)
{
    std::size_t total = 0;
    for (const auto &[from, to] : Changes(a, b))
    {
        total += to - from;
    }
    return total;
}

struct Rewritten
{
    std::vector<std::byte> bytes;
    sg::Save save;
    std::vector<sg::Unwritten> unwritten;
};

// The write of a save, and the read of what was written.
Rewritten Rewrite(const std::vector<std::byte> &original, const sg::Save &save)
{
    const auto out = sg::Write(original, save);
    REQUIRE(out);
    const auto back = sg::Read(out->bytes);
    REQUIRE(back);
    return {out->bytes, *back, out->unwritten};
}

sg::Save ReadOf(const std::vector<std::byte> &bytes)
{
    const auto save = sg::Read(bytes);
    REQUIRE(save);
    return *save;
}

} // namespace

TEST_CASE("a save read and written back unchanged is the same bytes and writes everything", "[savegame][write]")
{
    for (const auto &bytes : {MakeSave(), MakeEngineSave()})
    {
        const auto out = sg::Write(bytes, ReadOf(bytes));
        REQUIRE(out);
        CHECK(out->bytes == bytes);
        CHECK(out->unwritten.empty());
    }
}

TEST_CASE("a file that is no save is not written", "[savegame][write]")
{
    const sg::Save save;
    CHECK_FALSE(sg::Write(MakeSave("SVMT"), save));
    CHECK_FALSE(sg::Write(std::vector<std::byte>(64, std::byte{0x41}), save));
    CHECK_FALSE(sg::Write(std::vector<std::byte>{}, save));
}

TEST_CASE("numbers, floats and flags changed read back changed, and nothing else moved", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    save.areas[0].creatures[0].position = sg::Vec3{5, 6, 7};
    save.areas[0].creatures[0].active = 0;
    save.campaign.variables[0].value = std::int32_t{99};
    save.party.plots[0].flags[0] = 0xF0;
    save.party.money = 5;
    save.timePlayedSeconds = 777;
    save.state.worldTimeMs = 1;

    const auto out = Rewrite(bytes, save);
    CHECK(out.unwritten.empty());
    CHECK(out.bytes.size() == bytes.size()); // everything was set in place
    CHECK(ChangedBytes(bytes, out.bytes) <= 12 + 1 + 4 + 1 + 4 + 4 + 4);
    const auto &wolf = out.save.areas[0].creatures[0];
    CHECK(wolf.position == sg::Vec3{5, 6, 7});
    CHECK(wolf.active == 0);
    CHECK(wolf.id == 600);
    REQUIRE(wolf.stats);
    CHECK(wolf.stats->spells == std::vector<std::uint32_t>{10, 20, 30});
    CHECK(std::get<std::int32_t>(out.save.campaign.variables[0].value) == 99);
    CHECK(out.save.campaign.variables[0].name == L"COUNT");
    CHECK(out.save.party.plots[0].flags[0] == 0xF0);
    CHECK(out.save.party.plots[0].flags[3] == 4);
    CHECK(out.save.party.money == 5);
    CHECK(out.save.timePlayedSeconds == 777);
    CHECK(out.save.state.worldTimeMs == 1);
    CHECK(out.save.state.worldDay == 3);
    CHECK(out.save.version.buildNumber == 13263);
}

TEST_CASE("strings and a talk-table string changed are appended and read back", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    save.areas[0].tag = L"ar_renamed";
    save.campaign.variables[1].value = std::wstring(L"a longer value than before");
    save.hero->nameText = L"Zed";
    save.campaign.journal.active[0].titleStrref = 4321;

    const auto out = Rewrite(bytes, save);
    CHECK(out.unwritten.empty());
    CHECK(out.save.areas[0].tag == L"ar_renamed");
    CHECK(out.save.FindArea(L"AR_RENAMED"));
    CHECK(std::get<std::wstring>(out.save.campaign.variables[1].value) == L"a longer value than before");
    CHECK(out.save.hero->nameText == L"Zed");
    CHECK(out.save.hero->nameStrref == 5);
    CHECK(out.save.campaign.journal.active[0].titleStrref == 4321);
    CHECK(out.save.campaign.journal.active[1].titleStrref == 222);
    CHECK(out.save.campaign.journal.active[0].resRef == L"quest_a");
    // Nothing already in the file moved: the old bytes differ only at the slots that now point on.
    CHECK(out.bytes.size() > bytes.size());
    CHECK(ChangedBytes(bytes, out.bytes) <= 4 + 4 + 4 + 4);
}

TEST_CASE("an Opt that is none leaves the field as the file has it", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    save.areas[0].creatures[0].equipment.reset();
    save.areas[0].placeables[0].plotGiver.reset();
    save.areas[0].creatures[0].stats->stats.clear(); // a plain list is written, though
    save.hero->nameStrref.reset();
    save.hero->nameText.reset();

    const auto out = Rewrite(bytes, save);
    REQUIRE(out.save.areas[0].creatures[0].equipment);
    CHECK(out.save.areas[0].creatures[0].equipment->size() == 2);
    CHECK(out.save.areas[0].placeables[0].plotGiver == 1);
    CHECK(out.save.areas[0].creatures[0].stats->stats.empty());
    CHECK(out.save.hero->nameStrref == 5);
    CHECK(out.save.hero->nameText == L"Aria");
}

TEST_CASE("a quickslot read without a number is written back without one", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    save.hero->stats->quickslots[1][0].ability = 97;
    const auto out = Rewrite(bytes, save);
    CHECK(out.unwritten.empty());
    CHECK(out.save.hero->stats->quickslots[1][0].ability == 97);
    CHECK_FALSE(out.save.hero->stats->quickslots[1][0].number);
    CHECK(out.bytes.size() == bytes.size());
}

TEST_CASE("items in a list: added", "[savegame][write]")
{
    const auto bytes = MakeSave();
    const auto original = ReadOf(bytes);

    auto save = original;
    sg::Item helm;
    helm.templateResRef = L"helm";
    helm.position = sg::Vec3{1, 0, 0};
    helm.id = 77;
    helm.stackSize = 2;
    save.areas[0].creatures[0].equipment->push_back(helm);
    const auto out = Rewrite(bytes, save);
    CHECK(out.unwritten.empty());
    const auto &items = *out.save.areas[0].creatures[0].equipment;
    REQUIRE(items.size() == 3);
    CHECK(items[0].templateResRef == L"sword");
    CHECK(items[0].Slot() == 3);
    CHECK(items[1].templateResRef == L"shield");
    CHECK(items[1].Slot() == 4);
    CHECK(items[2].templateResRef == L"helm");
    CHECK(items[2].Slot() == 1);
    CHECK(items[2].id == 77);
    CHECK(items[2].stackSize == 2);
    // The rest of the creature is as it was.
    CHECK(out.save.areas[0].creatures[0].stats->spells == std::vector<std::uint32_t>{10, 20, 30});
    CHECK(out.save.areas[0].creatures[0].stats->stats.size() == 2);
    CHECK(out.save.areas[0].placeables[0].items.size() == 1);
}

TEST_CASE("items in a list: taken out", "[savegame][write]")
{
    const auto bytes = MakeSave();
    const auto original = ReadOf(bytes);

    auto save = original;
    auto &items = *save.areas[0].creatures[0].equipment;
    items.erase(items.begin());
    const auto out = Rewrite(bytes, save);
    const auto &back = *out.save.areas[0].creatures[0].equipment;
    REQUIRE(back.size() == 1);
    CHECK(back[0].templateResRef == L"shield");
    CHECK(back[0].Slot() == 4);
    CHECK(back[0].WeaponSet() == 0);
}

TEST_CASE("items in a list: all taken out", "[savegame][write]")
{
    const auto bytes = MakeSave();
    const auto original = ReadOf(bytes);

    auto save = original;
    save.areas[0].creatures[0].equipment->clear();
    const auto out = Rewrite(bytes, save);
    REQUIRE(out.save.areas[0].creatures[0].equipment);
    CHECK(out.save.areas[0].creatures[0].equipment->empty());
}

TEST_CASE("items in a list: reordered", "[savegame][write]")
{
    const auto bytes = MakeSave();
    const auto original = ReadOf(bytes);

    auto save = original;
    auto &items = *save.areas[0].creatures[0].equipment;
    std::swap(items[0], items[1]);
    const auto out = Rewrite(bytes, save);
    const auto &back = *out.save.areas[0].creatures[0].equipment;
    REQUIRE(back.size() == 2);
    CHECK(back[0].templateResRef == L"shield");
    CHECK(back[0].Slot() == 4);
    CHECK(back[1].templateResRef == L"sword");
    CHECK(back[1].Slot() == 3);
}

TEST_CASE("items in a list: an item in a placeable, with a field changed as well", "[savegame][write]")
{
    const auto bytes = MakeSave();
    const auto original = ReadOf(bytes);

    auto save = original;
    auto &items = save.areas[0].placeables[0].items;
    items[0].stackSize = 9;
    items.push_back(items[0]); // a copy
    items[1].id = 502;
    const auto out = Rewrite(bytes, save);
    const auto &back = out.save.areas[0].placeables[0].items;
    REQUIRE(back.size() == 2);
    CHECK(back[0].id == 501);
    CHECK(back[0].stackSize == 9);
    CHECK(back[1].id == 502);
    CHECK(back[1].stackSize == 9);
    CHECK(back[1].templateResRef == L"gift");
}

namespace
{

sg::Area AreaToAdd()
{
    sg::Area added;
    added.tag = L"ar_new";
    added.id = 9100;
    sg::Placeable barrel;
    barrel.templateResRef = L"barrel";
    barrel.position = sg::Vec3{1, 1, 1};
    barrel.orientation = sg::Quat{0, 0, 0, 1};
    barrel.id = 9101;
    barrel.plotGiver = 1;
    added.placeables.push_back(barrel);
    sg::Creature rat;
    rat.templateResRef = L"rat";
    rat.id = 9102;
    rat.active = 1;
    rat.equipment = std::vector<sg::Item>{};
    added.creatures.push_back(rat);
    added.roomsViewed = {7, 8, 9};
    added.map.bits = {0xFF, 0x01, 0x02};
    sg::Trigger trigger;
    trigger.type = 1; // a transition: its label follows
    trigger.id = 9103;
    trigger.geometry = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}};
    trigger.loadScreen = 12;
    added.triggers.push_back(trigger);
    sg::Waypoint waypoint;
    waypoint.id = 9104;
    waypoint.mapNote = 1;
    waypoint.mapNoteTextOwn = L"here";
    waypoint.mapNoteTextStrref = 12;
    added.waypoints.push_back(waypoint);
    return added;
}

} // namespace

TEST_CASE("areas in the area list: added", "[savegame][write]")
{
    const auto bytes = MakeSave();
    const auto original = ReadOf(bytes);
    const sg::Area added = AreaToAdd();

    auto save = original;
    save.areas.push_back(added);
    const auto out = Rewrite(bytes, save);
    CHECK(out.unwritten.empty());
    REQUIRE(out.save.areas.size() == 2);
    CHECK(out.save.areas[0].tag == L"ar_area");
    CHECK(out.save.areas[0].creatures[0].id == 600);
    CHECK(out.save.areas[0].triggers.size() == 2);
    const sg::Area *area = out.save.FindArea(L"AR_NEW");
    REQUIRE(area);
    CHECK(area->id == 9100);
    CHECK(area->Used());
    REQUIRE(area->placeables.size() == 1);
    CHECK(area->placeables[0].templateResRef == L"barrel");
    CHECK(area->placeables[0].position == sg::Vec3{1, 1, 1});
    CHECK(area->placeables[0].id == 9101);
    CHECK(area->placeables[0].plotGiver == 1);
    CHECK(area->placeables[0].items.empty());
    REQUIRE(area->creatures.size() == 1);
    CHECK(area->creatures[0].templateResRef == L"rat");
    CHECK(area->creatures[0].id == 9102);
    REQUIRE(area->creatures[0].equipment);
    CHECK(area->creatures[0].equipment->empty());
    CHECK_FALSE(area->creatures[0].stats); // a blank's references are null
    REQUIRE(area->triggers.size() == 1);
    CHECK(area->triggers[0].label == "TRAT");
    CHECK(area->triggers[0].KindOf() == sg::Trigger::Kind::Transition);
    CHECK(area->triggers[0].geometry.size() == 3);
    CHECK(area->triggers[0].loadScreen == 12);
    REQUIRE(area->waypoints.size() == 1);
    CHECK(area->waypoints[0].mapNoteTextOwn == L"here");
    CHECK(area->waypoints[0].mapNoteTextStrref == 12);
    CHECK(area->roomsViewed == std::vector<std::int32_t>{7, 8, 9});
    CHECK(area->map.bits == std::vector<std::uint8_t>{0xFF, 0x01, 0x02});
    // Written again unchanged, the grown file is itself.
    const auto again = sg::Write(out.bytes, out.save);
    REQUIRE(again);
    CHECK(again->bytes == out.bytes);
}

TEST_CASE("areas in the area list: taken out", "[savegame][write]")
{
    const auto bytes = MakeSave();
    const auto original = ReadOf(bytes);
    const sg::Area added = AreaToAdd();

    auto save = original;
    save.areas.clear();
    const auto out = Rewrite(bytes, save);
    CHECK(out.save.areas.empty());
    CHECK(out.save.hero);
    CHECK(out.save.campaign.resource == L"campaign");
}

TEST_CASE("areas in the area list: taken out of the front and another added", "[savegame][write]")
{
    const auto bytes = MakeSave();
    const auto original = ReadOf(bytes);
    const sg::Area added = AreaToAdd();

    auto save = original;
    save.areas.erase(save.areas.begin());
    save.areas.push_back(added);
    const auto out = Rewrite(bytes, save);
    REQUIRE(out.save.areas.size() == 1);
    CHECK(out.save.areas[0].tag == L"ar_new");
}

TEST_CASE("areas in the area list: reordered", "[savegame][write]")
{
    const auto bytes = MakeSave();
    const auto original = ReadOf(bytes);
    const sg::Area added = AreaToAdd();

    auto save = original;
    save.areas.push_back(added);
    std::swap(save.areas[0], save.areas[1]);
    const auto out = Rewrite(bytes, save);
    REQUIRE(out.save.areas.size() == 2);
    CHECK(out.save.areas[0].tag == L"ar_new");
    CHECK(out.save.areas[1].tag == L"ar_area");
    CHECK(out.save.areas[1].creatures[0].id == 600);
    REQUIRE(out.save.areas[1].creatures[0].stats);
    CHECK(out.save.areas[1].creatures[0].stats->stats.size() == 2);
}

TEST_CASE("a creature copied is built from its members, a new one from what is set", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);

    sg::Creature copy = save.areas[0].creatures[0];
    copy.id = 601;
    save.areas[0].creatures.push_back(copy);

    sg::Creature fresh;
    fresh.templateResRef = L"fresh";
    fresh.id = 602;
    fresh.position = sg::Vec3{9, 8, 7};
    fresh.stats = sg::Stats{};
    fresh.stats->spells = {5, 6};
    sg::Stat attr;
    attr.kind = sg::Stat::Kind::Attribute;
    attr.index = 11;
    attr.base = 4.0F;
    attr.modifier = 1.0F;
    fresh.stats->stats.push_back(attr);
    sg::Item blade;
    blade.templateResRef = L"blade";
    blade.position = sg::Vec3{3, 0, 0};
    fresh.equipment = std::vector<sg::Item>{blade};
    save.areas[0].creatures.push_back(fresh);

    const auto out = Rewrite(bytes, save);
    CHECK(out.unwritten.empty());
    const auto &creatures = out.save.areas[0].creatures;
    REQUIRE(creatures.size() == 3);
    for (const std::size_t i : {std::size_t{0}, std::size_t{1}})
    {
        CHECK(creatures[i].templateResRef == L"wolf");
        CHECK(creatures[i].id == (i == 0 ? 600U : 601U));
        REQUIRE(creatures[i].stats);
        CHECK(creatures[i].stats->spells == std::vector<std::uint32_t>{10, 20, 30});
        REQUIRE(creatures[i].stats->stats.size() == 2);
        CHECK(creatures[i].stats->stats[1].kind == sg::Stat::Kind::Depletable);
        CHECK(*creatures[i].stats->stats[1].current == Approx(80));
        REQUIRE(creatures[i].equipment);
        CHECK(creatures[i].equipment->size() == 2);
    }
    CHECK(creatures[2].templateResRef == L"fresh");
    CHECK(creatures[2].id == 602);
    CHECK(creatures[2].position == sg::Vec3{9, 8, 7});
    REQUIRE(creatures[2].stats);
    CHECK(creatures[2].stats->spells == std::vector<std::uint32_t>{5, 6});
    REQUIRE(creatures[2].stats->stats.size() == 1);
    CHECK(creatures[2].stats->stats[0].kind == sg::Stat::Kind::Attribute);
    CHECK(creatures[2].stats->stats[0].index == 11);
    CHECK(*creatures[2].stats->stats[0].modifier == Approx(1));
    REQUIRE(creatures[2].equipment);
    REQUIRE(creatures[2].equipment->size() == 1);
    CHECK((*creatures[2].equipment)[0].templateResRef == L"blade");
    CHECK((*creatures[2].equipment)[0].Slot() == 3);
}

TEST_CASE("variables are added, taken out and changed in type, each in the template of its type", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    auto &vars = save.campaign.variables;
    REQUIRE(vars.size() == 3);

    vars.erase(vars.begin());         // COUNT
    vars[0].value = std::int32_t{12}; // NAME was a string, now an int
    vars.push_back({.name = L"LATER", .value = sg::ObjectId{777}});
    vars.push_back({.name = L"FLOATY", .value = 1.5F});          // the file has no SVF1 template
    vars.push_back({.name = L"RES", .value = sg::ResRef{L"x"}}); // nor SVR1

    const auto out = Rewrite(bytes, save);
    const auto &back = out.save.campaign.variables;
    REQUIRE(back.size() == 3);
    CHECK(back[0].name == L"NAME");
    CHECK(std::get<std::int32_t>(back[0].value) == 12);
    CHECK(back[1].name == L"WHO");
    CHECK(std::get<sg::ObjectId>(back[1].value).id == 4242);
    CHECK(back[2].name == L"LATER");
    CHECK(std::get<sg::ObjectId>(back[2].value).id == 777);
    REQUIRE(out.unwritten.size() == 1);
    CHECK(out.unwritten[0].structLabel == "MODU");
    CHECK(out.unwritten[0].field == 17000);
    CHECK(out.unwritten[0].count == 2);
}

TEST_CASE("a member the template has no field for is told, and the file keeps what it has", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    auto &wolf = save.areas[0].creatures[0];
    wolf.nameStrref = 4;                               // CRE1 has no name
    wolf.backpack = std::vector<sg::Item>{sg::Item{}}; // nor a backpack
    wolf.tag = L"x";                                   // nor a tag
    const auto out = Rewrite(bytes, save);
    std::vector<std::uint32_t> fields;
    for (const auto &u : out.unwritten)
    {
        CHECK(u.structLabel == "CRE1");
        CHECK(u.count == 1);
        fields.push_back(u.field);
    }
    CHECK(fields == std::vector<std::uint32_t>{16210, 16222, 16255});
    CHECK_FALSE(out.save.areas[0].creatures[0].nameStrref);
    CHECK_FALSE(out.save.areas[0].creatures[0].backpack);
    CHECK(out.save.areas[0].creatures[0].id == 600);
    CHECK(out.bytes == bytes); // nothing else was asked
}

TEST_CASE("a child struct the file lacks is made when it is set, and not for defaults", "[savegame][write]")
{
    auto bytes = MakeSave();
    // The campaign's world map is a null reference (MODU + 36).
    {
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const auto top = file->Root();
        REQUIRE(top);
        const auto modu = top->Child(16000);
        REQUIRE(modu);
        const std::uint32_t none = 0xFFFFFFFF;
        std::memcpy(bytes.data() + modu->Offset() + 36, &none, 4);
    }

    auto save = ReadOf(bytes);
    CHECK(save.campaign.worldMap.primaryMap.empty());
    const auto same = sg::Write(bytes, save);
    REQUIRE(same);
    CHECK(same->bytes == bytes); // defaults make no struct

    save.campaign.worldMap.primaryMap = L"the map";
    const auto out = Rewrite(bytes, save);
    CHECK(out.save.campaign.worldMap.primaryMap == L"the map");
    CHECK(out.save.campaign.entryArea == L"ar_area");
}

TEST_CASE("the game state's event queue and a plot action list grow", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    REQUIRE(save.campaign.plotActions.list.size() == 2);
    save.state.eventQueue.push_back(save.state.eventQueue[0]);
    save.state.eventQueue.back().id = 6;
    sg::QueuedEvent plain;
    plain.id = 10;
    plain.day = 9;
    save.state.eventQueue.push_back(plain);
    auto &actions = save.campaign.plotActions.list;
    actions[0].count = 11;
    actions.push_back(actions[0]);
    actions.back().id = 21;

    const auto out = Rewrite(bytes, save);
    CHECK(out.unwritten.empty());
    REQUIRE(out.save.state.eventQueue.size() == 4);
    CHECK(out.save.state.eventQueue[0].id == 12);
    CHECK(out.save.state.eventQueue[1].id == 10);
    CHECK(out.save.state.eventQueue[2].id == 6);
    CHECK(out.save.state.eventQueue[2].caller == 600);
    CHECK(out.save.state.eventQueue[3].id == 10);
    CHECK(out.save.state.eventQueue[3].day == 9);
    const auto &back = out.save.campaign.plotActions.list;
    REQUIRE(back.size() == 3);
    CHECK(back[0].count == 11);
    CHECK_FALSE(back[1].count); // the second is the PTAJ, with no count field
    CHECK(back[2].id == 21);
    CHECK(back[2].count == 11);
}

TEST_CASE("a damaged save the writer is given is written as far as it reads and never faults", "[savegame][write]")
{
    const auto bytes = MakeSave();
    const auto save = ReadOf(bytes);
    std::size_t written = 0;
    for (std::size_t length = 64; length < bytes.size(); length += 11)
    {
        const std::vector<std::byte> cut(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(length));
        if (sg::Write(cut, save))
        {
            ++written;
        }
    }
    CHECK(written > 0);
}

TEST_CASE("an editor makes blank structs, points at them, and sets talk-table strings and string lists", "[gff4]")
{
    const auto bytes = MakeSave();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto top = file->Root();
    REQUIRE(top);
    gff4::Editor ed(*file);
    CHECK(ed.Bytes() == bytes);
    const std::size_t mark = ed.Size();

    const auto wpt = ed.New(kWpt1);
    REQUIRE(wpt);
    CHECK_FALSE(ed.New(999));
    const auto modu = ed.New(kModu);
    REQUIRE(modu);
    REQUIRE(ed.SetChild(*top, 16000, &*modu));
    CHECK_FALSE(ed.SetChild(*top, 16000, &*wpt));  // another template
    CHECK_FALSE(ed.SetChild(*top, 16001, &*modu)); // a list is no child
    REQUIRE(ed.SetTlk(*wpt, 16714, 321, std::wstring_view(L"own text")));
    REQUIRE(ed.SetNumber(*wpt, 23, 5));
    CHECK_FALSE(ed.SetTlk(*wpt, 16710, 1, std::nullopt));
    const std::vector<std::wstring> guids{L"A", L"BB", L""};
    const auto pdst = ed.New(kPdst);
    REQUIRE(pdst);
    REQUIRE(ed.SetStrings(*pdst, 16515, guids));
    CHECK_FALSE(ed.SetStrings(*pdst, 16514, guids)); // not a list

    const auto after = gff4::File::Parse(ed.Bytes());
    REQUIRE(after);
    const auto top2 = after->Root();
    REQUIRE(top2);
    const auto blank = top2->Child(16000);
    REQUIRE(blank);
    CHECK(blank->Offset() == modu->Offset());
    CHECK_FALSE(blank->Child(16504)); // a blank's reference is null
    CHECK(blank->Structs(16450).empty());
    CHECK(blank->String(16014) == L"");
    const gff4::Struct wpt2(*after, kWpt1, wpt->Offset());
    CHECK(wpt2.Tlk(16714)->strref == 321);
    CHECK(wpt2.TlkText(16714) == L"own text");
    CHECK(wpt2.Get<std::int32_t>(23) == 5);
    CHECK(wpt2.Get<std::uint8_t>(16710) == 0);
    const gff4::Struct pdst2(*after, kPdst, pdst->Offset());
    CHECK(pdst2.Strings(16515) == guids);
    CHECK(pdst2.String(16514) == L""); // null

    CHECK(ed.SetChild(*top, 16000, nullptr)); // none
    CHECK_FALSE(gff4::File::Parse(ed.Bytes())->Root()->Child(16000));

    ed.Truncate(mark);
    CHECK(ed.Bytes().size() == bytes.size());
}

TEST_CASE("an editor sets a generic list to structs of the file and new ones", "[gff4]")
{
    const auto bytes = MakeSave();
    const auto file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const auto top = file->Root();
    REQUIRE(top);
    const auto areas = top->Structs(16001);
    REQUIRE(areas.size() == 1);
    gff4::Editor ed(*file);
    const auto added = ed.New(kAre1);
    REQUIRE(added);
    const std::vector<gff4::Struct> list{*added, areas[0]};
    REQUIRE(ed.SetStructs(*top, 16001, list));
    const auto plc = ed.New(kPlc1);
    REQUIRE(plc);
    const std::vector<gff4::Struct> wrong{*plc};
    CHECK_FALSE(ed.SetStructs(*top, 16010, wrong));   // no such field in the top struct
    CHECK_FALSE(ed.SetStructs(*added, 16010, wrong)); // a list held in place is not a list of references

    const auto after = gff4::File::Parse(ed.Bytes());
    REQUIRE(after);
    const auto back = after->Root()->Structs(16001);
    REQUIRE(back.size() == 2);
    CHECK(back[0].Offset() == added->Offset());
    CHECK(back[0].Label() == "ARE1");
    CHECK(back[0].String(1) == L"");
    CHECK(back[0].Structs(16010).empty());
    CHECK(back[1].Offset() == areas[0].Offset());
    CHECK(back[1].String(1) == L"ar_area");
}

TEST_CASE("a blank struct holds a struct held in place blank in turn", "[gff4]")
{
    // A top struct of 12 bytes that holds an inner one of 8 in place, then a string.
    std::vector<TemplateSpec> templates = {
        {"TOP", 12, {{100, 1, gff4::kStruct, 0}, {103, kStr, 0, 8}}},
        {"INNR", 8, {{101, kStr, 0, 0}, {102, kU32, kL, 4}}},
    };
    Builder b = dao::test::Start("TEST", templates);
    New(b, 12);
    const auto file = gff4::File::Parse(b.bytes);
    REQUIRE(file);
    gff4::Editor ed(*file);
    const auto blank = ed.New(0);
    REQUIRE(blank);
    const auto after = gff4::File::Parse(ed.Bytes());
    REQUIRE(after);
    const gff4::Struct top(*after, 0, blank->Offset());
    const auto inner = top.Child(100);
    REQUIRE(inner);
    CHECK(inner->Offset() == blank->Offset());
    CHECK(inner->String(101) == L"");
    CHECK(inner->Numbers(102).empty());
    CHECK(top.String(103) == L"");
    std::uint32_t word = 0;
    std::memcpy(&word, ed.Bytes().data() + blank->Offset() + 4, 4);
    CHECK(word == 0xFFFFFFFF); // the inner list is null
    std::memcpy(&word, ed.Bytes().data() + blank->Offset() + 8, 4);
    CHECK(word == 0xFFFFFFFF); // so is the string
}

TEST_CASE("an id a save holds twice is found, as a creature copied with its items holds them", "[savegame]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    // The hand-made items have no ids of their own (0 in both); give each one.
    auto &wolf = save.areas[0].creatures[0];
    (*wolf.equipment)[0].id = 1000;
    (*wolf.equipment)[1].id = 1001;
    save.areas[0].placeables[0].items[0].id = 1002;
    CHECK(save.DuplicateIds().empty());

    // The copy of a creature is a creature of another id with the first one's items: seen to crash the game's
    // area list when it is torn down (2026-10-21, DAOLib #221).
    sg::Creature copy = wolf;
    copy.id = 601;
    save.areas[0].creatures.push_back(copy);
    CHECK(save.DuplicateIds() == std::vector<std::uint32_t>{1000, 1001});
    const auto written = sg::Write(bytes, save);
    REQUIRE(written);
    CHECK(written->duplicateIds == std::vector<std::uint32_t>{1000, 1001});

    (*save.areas[0].creatures[1].equipment)[0].id = 1003;
    (*save.areas[0].creatures[1].equipment)[1].id = 1004;
    CHECK(save.DuplicateIds().empty());

    // The same id on a creature and an item, the hero's and the area's, and no id at all.
    save.areas[0].placeables[0].id = 601;
    save.hero->id = sg::kNoObject;
    save.areas[0].waypoints[0].id = sg::kNoObject;
    CHECK(save.DuplicateIds() == std::vector<std::uint32_t>{601});
}

TEST_CASE("an element taken out before another leaves the other with what the file holds of it", "[savegame][write]")
{
    // The second trigger, a transition, gets a geometry of its own whose point has a junk fourth float, which
    // the reader drops. Taking the first trigger out moves the second to its place; paired by place it would
    // be matched with a struct of another template and built afresh, with the junk lost.
    auto bytes = MakeSave();
    float junk = 9.0F;
    {
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const auto triggers = file->Root()->Structs(16001)[0].Structs(16012);
        REQUIRE(triggers.size() == 2);
        const std::size_t list = bytes.size();
        const std::uint32_t one = 1;
        const float point[4] = {1, 2, 3, junk};
        bytes.resize(list + 4 + sizeof point);
        std::memcpy(bytes.data() + list, &one, 4);
        std::memcpy(bytes.data() + list + 4, point, sizeof point);
        const auto offset = static_cast<std::uint32_t>(list - file->DataOffset());
        std::memcpy(bytes.data() + triggers[1].Offset() + 8, &offset, 4);
    }
    auto save = ReadOf(bytes);
    REQUIRE(save.areas[0].triggers.size() == 2);
    REQUIRE(save.areas[0].triggers[1].geometry.size() == 1);
    save.areas[0].triggers.erase(save.areas[0].triggers.begin());

    const auto out = Rewrite(bytes, save);
    REQUIRE(out.save.areas[0].triggers.size() == 1);
    CHECK(out.save.areas[0].triggers[0].label == "TRAT");
    CHECK(out.save.areas[0].triggers[0].loadScreen == 9);
    const auto file = gff4::File::Parse(out.bytes);
    REQUIRE(file);
    const auto kept = file->Root()->Structs(16001)[0].Structs(16012);
    REQUIRE(kept.size() == 1);
    const auto points = kept[0].ListBytes(16101, gff4::Type::Vector3);
    REQUIRE(points.size() == 16);
    float fourth = 0;
    std::memcpy(&fourth, points.data() + 12, 4);
    CHECK(fourth == junk);
}

TEST_CASE("a number is not written into a field of another width", "[savegame][write]")
{
    // An area whose object id is one byte in its template, where the model has four.
    const std::vector<TemplateSpec> templates = {{"", 4, {{16001, 0xFFFF, kGenList, 0}}},
                                                 {"ARE1", 8, {{1, kStr, 0, 0}, {23, kU8, 0, 4}}}};
    Builder b = dao::test::Start("SAVE", templates);
    const std::size_t top = New(b, 4);
    const std::size_t area = New(b, 8);
    Str(b, area, u"a");
    b.U8(area + 4, 7);
    b.U32(top, dao::test::Children(b, {{1, area}}));

    auto save = ReadOf(b.bytes);
    REQUIRE(save.areas.size() == 1);
    CHECK(save.areas[0].id == sg::kNoObject); // not read either
    save.areas[0].id = 0x1234;
    const auto out = sg::Write(b.bytes, save);
    REQUIRE(out);
    CHECK(out->bytes == b.bytes);
}

TEST_CASE("the fourth float of a vector list, which the reader drops, rides along", "[savegame][write]")
{
    auto bytes = MakeSave();
    {
        const auto file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const auto trigger = file->Root()->Structs(16001)[0].Structs(16012)[0];
        const auto points = trigger.ListBytes(16101, gff4::Type::Vector3);
        REQUIRE(points.size() == 48);
        const float junk = 9.0F;
        std::memcpy(bytes.data() + (points.data() - file->Bytes().data()) + 12, &junk, 4);
    }
    auto save = ReadOf(bytes);
    const auto same = sg::Write(bytes, save);
    REQUIRE(same);
    CHECK(same->bytes == bytes);

    save.areas[0].triggers[0].geometry[1] = sg::Vec3{4, 5, 6};
    const auto out = Rewrite(bytes, save);
    REQUIRE(out.save.areas[0].triggers[0].geometry.size() == 3);
    CHECK(out.save.areas[0].triggers[0].geometry[0] == sg::Vec3{0, 0, 0});
    CHECK(out.save.areas[0].triggers[0].geometry[1] == sg::Vec3{4, 5, 6});
    CHECK(out.save.areas[0].triggers[0].geometry[2] == sg::Vec3{1, 1, 0});
}

TEST_CASE("a creature copied does not share its items with the one it was copied from", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    sg::Creature copy = save.areas[0].creatures[0];
    copy.id = 601;
    (*copy.equipment)[0].position = sg::Vec3{8, 1, 0}; // an item changed in place, the list as long as it was
    (*copy.equipment)[1].stackSize = 12;
    save.areas[0].creatures.push_back(copy);
    sg::Creature bare;
    bare.templateResRef = L"bare";
    save.areas[0].creatures.push_back(bare);

    const auto out = Rewrite(bytes, save);
    const auto &creatures = out.save.areas[0].creatures;
    REQUIRE(creatures.size() == 3);
    CHECK((*creatures[0].equipment)[0].Slot() == 3);
    CHECK((*creatures[0].equipment)[1].stackSize == 0);
    CHECK((*creatures[1].equipment)[0].Slot() == 8);
    CHECK((*creatures[1].equipment)[1].stackSize == 12);
    CHECK(creatures[2].id == sg::kNoObject); // the engine's default for an object id, which a caller replaces
}

namespace
{

// What an object holds that a copy of it must hold too, as one line: for comparing an area written afresh from
// its members with the one the file had.
std::string Line(const sg::Item &i)
{
    std::string s = "item";
    s += i.templateResRef ? " r" + std::to_string(i.templateResRef->size()) : "";
    s += i.position ? " p" + std::to_string(i.position->x) + "," + std::to_string(i.position->y) : "";
    s += i.id ? " i" + std::to_string(*i.id) : "";
    s += i.stackSize ? " s" + std::to_string(*i.stackSize) : "";
    s += " e" + std::to_string(i.effects.size()) + " v" + std::to_string(i.variables.size()) + " p" +
         std::to_string(i.properties.size());
    return s;
}

std::string Count(std::size_t n)
{
    return std::to_string(n) + " ";
}

std::string Digest(const sg::Creature &c)
{
    std::string s =
        "cre " + std::to_string(c.id.value_or(0)) + " " + std::to_string(c.position ? c.position->x : 0) + " ";
    s += c.stats ? Count(c.stats->stats.size()) + Count(c.stats->spells.size()) : "99 99 ";
    s += c.equipment ? Count(c.equipment->size()) : "99 ";
    s += c.backpack ? Count(c.backpack->size()) : "99 ";
    s += Count(c.effects.size()) + Count(c.commands.size()) + Count(c.variables.size());
    s += c.actionQueue ? Count(c.actionQueue->subactions.size()) : "99 ";
    s += c.tactics ? Count(c.tactics->entries.size()) : "99 ";
    s += "\n";
    if (c.stats)
    {
        for (const auto &st : c.stats->stats)
        {
            s += "  stat " + std::to_string(st.index) + " " + std::to_string(st.base.value_or(-1)) + " " +
                 std::to_string(st.current.value_or(-1)) + "\n";
        }
    }
    for (const auto &i : c.equipment.value_or(std::vector<sg::Item>()))
    {
        s += "  " + Line(i) + "\n";
    }
    return s;
}

std::string Digest(const sg::Placeable &p)
{
    std::string s = "plc " + std::to_string(p.id.value_or(0)) + " " + std::to_string(p.state.value_or(0)) + " " +
                    std::to_string(p.health.value_or(0)) + " " + std::to_string(p.position ? p.position->x : 0) + "\n";
    for (const auto &i : p.items)
    {
        s += "  " + Line(i) + "\n";
    }
    return s;
}

std::string Digest(const sg::Area &a)
{
    std::string s = "area " + std::to_string(a.id) + " " + Count(a.placeables.size()) + Count(a.creatures.size()) +
                    Count(a.triggers.size()) + Count(a.aoes.size()) + Count(a.waypoints.size()) +
                    Count(a.stores.size()) + Count(a.sounds.size()) + Count(a.map.bits.size()) +
                    Count(a.roomsViewed.size()) + std::to_string(a.minCreatureImportance) + " " +
                    Count(a.effects.size()) + Count(a.variables.size()) + "\n";
    for (const auto &p : a.placeables)
    {
        s += Digest(p);
    }
    for (const auto &c : a.creatures)
    {
        s += Digest(c);
    }
    for (const auto &t : a.triggers)
    {
        s += "trg " + t.label + " " + Count(t.geometry.size()) + std::to_string(t.type.value_or(-1)) + " " +
             Count(t.sounds.size()) + "\n";
    }
    for (const auto &w : a.waypoints)
    {
        s += "wpt " + std::to_string(w.id.value_or(0)) + " " + std::to_string(w.mapNote.value_or(0)) + "\n";
    }
    for (const auto &st : a.stores)
    {
        s += "sto " + Count(st.items.size()) + std::to_string(st.gold.value_or(0)) + "\n";
    }
    for (const auto &v : a.variables)
    {
        s += "var " + Count(v.name.size()) + std::to_string(v.value.index()) + "\n";
    }
    return s;
}

} // namespace

TEST_CASE("the player's saves in game/saves, when there, are written back unchanged and changed",
          "[savegame][write][game][folder]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "saves";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/saves (a copy of a save's .das)");
    }
    std::size_t seen = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".das")
        {
            continue;
        }
        INFO(entry.path().filename().string());
        const auto bytes = FileBytes(entry.path());
        const auto save = sg::Read(bytes);
        REQUIRE(save);
        ++seen;

        // Unchanged: the same bytes, and nothing the writer could not write.
        const auto same = sg::Write(bytes, *save);
        REQUIRE(same);
        CHECK(same->bytes == bytes);
        CHECK(save->DuplicateIds().empty());
        for (const auto &u : same->unwritten)
        {
            INFO(u.structLabel << " " << u.field << " " << u.reason << " x" << u.count);
            CHECK(false);
        }

        // Changed: a variable and a plot flag set, an item and a creature added, an area copied and one taken out.
        auto changed = *save;
        const std::size_t variables = changed.campaign.variables.size();
        changed.campaign.variables.push_back({.name = L"DAOLIB_WRITE_TEST", .value = std::int32_t{4}});
        REQUIRE_FALSE(changed.party.plots.empty());
        changed.party.plots[0].flags[1] = 0x1234;
        REQUIRE_FALSE(changed.areas.empty());
        changed.areas.push_back(changed.areas[0]);
        changed.areas.back().tag = L"daolib_write_copy";
        const std::size_t creatures = changed.areas[0].creatures.size();
        if (!changed.areas[0].creatures.empty())
        {
            sg::Creature extra = changed.areas[0].creatures[0];
            extra.id = 0x7FFF0001;
            changed.areas[0].creatures.push_back(extra);
        }
        if (changed.areas.size() > 2)
        {
            changed.areas.erase(changed.areas.begin() + 1);
        }
        const auto out = sg::Write(bytes, changed);
        REQUIRE(out);
        const auto back = sg::Read(out->bytes);
        REQUIRE(back);
        CHECK(back->campaign.variables.size() == variables + 1);
        CHECK(std::get<std::int32_t>(back->campaign.variables.back().value) == 4);
        CHECK(back->party.plots.size() == save->party.plots.size());
        CHECK(back->party.plots[0].flags[1] == 0x1234);
        CHECK(back->party.plots[0].guid == save->party.plots[0].guid);
        CHECK(back->areas.size() == changed.areas.size());
        CHECK(back->areas[0].creatures.size() == creatures + (creatures != 0 ? 1 : 0));
        CHECK(back->hero.has_value() == save->hero.has_value());
        CHECK(back->party.pool.size() == save->party.pool.size());
        CHECK(back->state.eventQueue.size() == save->state.eventQueue.size());
        CHECK(back->version.buildNumber == save->version.buildNumber);
        // The area written afresh is the area it was copied from.
        const sg::Area *copy = back->FindArea(L"daolib_write_copy");
        REQUIRE(copy);
        if (creatures != 0)
        {
            CHECK(Digest(*copy) == Digest(save->areas[0]));
        }
        // The areas that were carried over are as they were.
        for (std::size_t i = 1; i < back->areas.size(); ++i)
        {
            if (back->areas[i].tag != L"daolib_write_copy")
            {
                const sg::Area *was = save->FindArea(back->areas[i].tag);
                REQUIRE(was);
                CHECK(Digest(back->areas[i]) == Digest(*was));
            }
        }
        // And what was written is stable: written again, it is itself.
        const auto again = sg::Write(out->bytes, *back);
        REQUIRE(again);
        CHECK(again->bytes == out->bytes);
        CHECK(again->unwritten.empty());
    }
    if (seen == 0)
    {
        SKIP("no .das in game/saves");
    }
}

TEST_CASE("a plain event is taken out of the queue, and the rest stays as it was", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    REQUIRE(save.state.eventQueue.size() == 2);
    std::erase_if(save.state.eventQueue,
                  [](const sg::QueuedEvent &e) { return e.KindOf() == sg::QueuedEvent::Kind::Plain; });
    const auto out = Rewrite(bytes, save);
    CHECK(out.unwritten.empty());
    REQUIRE(out.save.state.eventQueue.size() == 1);
    const auto &kept = out.save.state.eventQueue[0];
    CHECK(kept.id == 10);
    CHECK(kept.day == 6);
    CHECK(kept.time == 2000);
    CHECK(kept.label == "ESCR");
    REQUIRE(kept.script);
    CHECK(kept.script->scriptName == L"script_a");
    CHECK(kept.script->type == 4);

    save.state.eventQueue.clear();
    CHECK(Rewrite(bytes, save).save.state.eventQueue.empty());
}

TEST_CASE("a plain event of each kind is built from its members", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    const std::uint32_t kinds[] = {6, 7, 11, 12, 13, 16, 27};
    std::uint32_t n = 1;
    for (const auto id : kinds)
    {
        save.state.eventQueue.push_back(sg::QueuedEvent::MakePlain(id, n, n * 10, n * 100, n * 1000));
        ++n;
    }
    const auto out = Rewrite(bytes, save);
    CHECK(out.unwritten.empty());
    REQUIRE(out.save.state.eventQueue.size() == 2 + std::size(kinds));
    n = 1;
    for (std::size_t i = 0; i < std::size(kinds); ++i, ++n)
    {
        const auto &e = out.save.state.eventQueue[2 + i];
        CHECK(e.label == "EBAS");
        CHECK(e.KindOf() == sg::QueuedEvent::Kind::Plain);
        CHECK(e.id == kinds[i]);
        CHECK(e.day == n);
        CHECK(e.time == n * 10);
        CHECK(e.caller == n * 100);
        CHECK(e.target == n * 1000);
        CHECK_FALSE(e.script);
    }
    // The file's own two are as they were.
    CHECK(out.save.state.eventQueue[0].id == 12);
    CHECK(out.save.state.eventQueue[1].script->scriptName == L"script_a");
    // And a script event is built with its payload.
    sg::ScriptEvent payload;
    payload.type = 9;
    payload.scriptName = L"script_b";
    save.state.eventQueue.push_back(sg::QueuedEvent::MakeScript(24, 1, 2, 3, 4, payload));
    const auto again = Rewrite(bytes, save);
    const auto &made = again.save.state.eventQueue.back();
    CHECK(made.label == "ESCR");
    CHECK(made.id == 24);
    REQUIRE(made.script);
    CHECK(made.script->scriptName == L"script_b");
    CHECK(made.script->type == 9);
}

TEST_CASE("a script event made plain and a plain one replaced lose no neighbour", "[savegame][write]")
{
    const auto bytes = MakeSave();
    auto save = ReadOf(bytes);
    // The script event's payload taken out and its id made a plain one's: it is written as a plain event.
    save.state.eventQueue[1].script.reset();
    save.state.eventQueue[1].id = 7;
    // The plain event replaced by another at its place.
    save.state.eventQueue[0] = sg::QueuedEvent::MakePlain(27, 8, 9, 10, 11);
    const auto out = Rewrite(bytes, save);
    CHECK(out.unwritten.empty());
    REQUIRE(out.save.state.eventQueue.size() == 2);
    CHECK(out.save.state.eventQueue[0].id == 27);
    CHECK(out.save.state.eventQueue[0].day == 8);
    CHECK(out.save.state.eventQueue[0].target == 11);
    CHECK(out.save.state.eventQueue[1].id == 7);
    CHECK(out.save.state.eventQueue[1].label == "EBAS");
    CHECK_FALSE(out.save.state.eventQueue[1].script);
    CHECK(out.save.state.eventQueue[1].time == 2000);
    CHECK(out.save.state.eventQueue[1].caller == 600);
}

TEST_CASE("a plain event goes on the script template when the file has no plain one", "[savegame][write]")
{
    auto bytes = MakeSave();
    // The EBAS template's label renamed, so the file has none: a save of the engine always has both, and
    // this is the writer's fallback.
    const std::string_view label = "EBAS";
    const auto at = std::search(bytes.begin(), bytes.end(), reinterpret_cast<const std::byte *>(label.data()),
                                reinterpret_cast<const std::byte *>(label.data()) + label.size());
    REQUIRE(at != bytes.end());
    *at = static_cast<std::byte>('X');
    auto save = ReadOf(bytes);
    save.state.eventQueue.push_back(sg::QueuedEvent::MakePlain(16, 3, 4, 5, 6));
    const auto out = Rewrite(bytes, save);
    CHECK(out.unwritten.empty());
    REQUIRE(out.save.state.eventQueue.size() == 3);
    const auto &made = out.save.state.eventQueue[2];
    CHECK(made.id == 16);
    CHECK(made.day == 3);
    CHECK(made.target == 6);
    CHECK(made.label == "ESCR");
    CHECK_FALSE(made.script);
}

// The player's own saves, read only: each read and written back unchanged is its own bytes, its
// string variables among what is written (DAOLib #106 step 6: a save keeps a string variable in an
// SVS1, typed, where a campaign's .cif keeps one in a generic field). Hidden, as the saves are the
// player's: run it by its tag, dao_core_tests "[install-real]".
TEST_CASE("the player's saves, read and written back unchanged, are their own bytes", "[.][install-real]")
{
    PWSTR documents = nullptr;
    std::filesystem::path characters;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documents)))
    {
        characters = std::filesystem::path(documents) / L"BioWare/Dragon Age/Characters";
    }
    CoTaskMemFree(documents);
    if (!std::filesystem::is_directory(characters))
    {
        SKIP("no Documents/BioWare/Dragon Age/Characters here");
    }
    std::size_t saves = 0;
    std::size_t strings = 0;
    for (const auto &item : std::filesystem::recursive_directory_iterator(characters))
    {
        if (!item.is_regular_file() || item.path().extension() != L".das")
        {
            continue;
        }
        INFO(item.path().string());
        const auto bytes = FileBytes(item.path());
        const auto save = sg::Read(bytes);
        REQUIRE(save);
        const auto out = sg::Write(bytes, *save);
        REQUIRE(out);
        CHECK(out->bytes == bytes);
        for (const auto &variable : save->campaign.variables)
        {
            strings += std::holds_alternative<std::wstring>(variable.value) ? 1 : 0;
        }
        ++saves;
    }
    CHECK(saves > 0);
    WARN(saves << " saves written back as they were, " << strings << " string variables among them");
}
