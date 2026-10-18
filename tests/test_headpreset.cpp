#include "dao/core/Gff4.h"
#include "dao/core/HeadPreset.h"
#include "dao/core/Model.h"
#include "dao/core/Morph.h"
#include "dao/core/MorphConfig.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using Catch::Approx;
namespace gff4 = dao::core::gff4;
namespace preset = dao::core::preset;

namespace
{

std::filesystem::path Heads()
{
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "heads";
}

std::vector<std::byte> FileBytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(chars.size());
    std::memcpy(bytes.data(), chars.data(), chars.size());
    return bytes;
}

// A list of `count` structs of `size` bytes at the data's end, its data offset written at `at`:
// where the first struct is.
std::size_t List(dao::test::Builder &b, std::size_t at, std::uint32_t count, std::size_t size)
{
    const std::size_t list = b.bytes.size();
    b.U32(list, count);
    b.U32(at, static_cast<std::uint32_t>(list - b.data));
    return count == 0 ? list + 4 : dao::test::Struct(b, size * count);
}

// A preset laid out as hm_cps_p01.mop is (tools/gff4.py --templates): parts "Face", "", "Hair";
// tints "skin", "" ; the targets "UH_NOS_30a_0" at 0.5 and its base at -0.5; FaceM1's
// g_vFacialHairWeights[5] = 0.72; HairM1's mml_vTintMaskColours[2] = (0.4, 0.2, 0.1, 1); and
// FaceM1's mml_tDiffuse "uh_hed_dwfa_0d".
std::vector<std::byte> Preset()
{
    dao::test::Builder b = dao::test::Start(
        "MOP ", {{"MOP",
                  0x1C,
                  {{23000, 14, gff4::kList, 4},
                   {23001, 14, gff4::kList, 8},
                   {23008, 14, 0, 0},
                   {23014, 2, gff4::kList | gff4::kStruct, 16},
                   {23015, 3, gff4::kList | gff4::kStruct, 20},
                   {23018, 1, gff4::kList | gff4::kStruct, 12},
                   {23022, 4, gff4::kList | gff4::kStruct, 24}}},
                 {"MOD", 8, {{23016, 14, 0, 0}, {23017, 8, 0, 4}}},
                 {"MAT", 0x10, {{23009, 14, 0, 0}, {23010, 14, 0, 4}, {23011, 5, 0, 8}, {23012, 8, 0, 12}}},
                 {"MAT4", 0x20, {{23009, 14, 0, 16}, {23010, 14, 0, 20}, {23011, 5, 0, 24}, {23013, 12, 0, 0}}},
                 {"TEX", 0xC, {{23019, 14, 0, 0}, {23020, 14, 0, 4}, {23021, 14, 0, 8}}}});
    const std::size_t root = dao::test::Struct(b, 0x1C);
    b.U32(root, b.String(u"hm_cps_p01"));
    b.U32(root + 4, b.Numbers({b.String(u"Face"), 0xFFFFFFFF, b.String(u"Hair")}));
    b.U32(root + 8, b.Numbers({b.String(u"skin"), 0xFFFFFFFF}));
    const std::size_t models = List(b, root + 12, 2, 8);
    b.U32(models, b.String(u"UH_NOS_30a_0"));
    dao::test::Float(b, models + 4, 0.5F);
    b.U32(models + 8, b.String(u"UH_UHM_BASa_0"));
    dao::test::Float(b, models + 12, -0.5F);
    const std::size_t value = List(b, root + 16, 1, 0x10);
    b.U32(value, b.String(u"FaceM1"));
    b.U32(value + 4, b.String(u"g_vFacialHairWeights"));
    b.U32(value + 8, 5);
    dao::test::Float(b, value + 12, 0.72F);
    const std::size_t vector = List(b, root + 20, 1, 0x20);
    dao::test::Floats(b, vector, {0.4F, 0.2F, 0.1F, 1.0F});
    b.U32(vector + 16, b.String(u"HairM1"));
    b.U32(vector + 20, b.String(u"mml_vTintMaskColours"));
    b.U32(vector + 24, 2);
    const std::size_t texture = List(b, root + 24, 1, 0xC);
    b.U32(texture, b.String(u"FaceM1"));
    b.U32(texture + 4, b.String(u"mml_tDiffuse"));
    b.U32(texture + 8, b.String(u"uh_hed_dwfa_0d"));
    return b.bytes;
}

} // namespace

TEST_CASE("a head preset's parts, tints, targets, material values and textures are read", "[preset]")
{
    const std::vector<std::byte> bytes = Preset();
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const std::optional<preset::Preset> read = preset::PresetOf(*file);
    REQUIRE(read);
    CHECK(read->name == L"hm_cps_p01");
    CHECK(read->parts == std::vector<std::wstring>{L"Face", L"", L"Hair"});
    CHECK(read->tints == std::vector<std::wstring>{L"skin", L""});
    REQUIRE(read->models.size() == 2);
    CHECK(read->models[0].model == L"UH_NOS_30a_0");
    CHECK(read->models[0].weight == Approx(0.5));
    CHECK(read->models[1].model == L"UH_UHM_BASa_0");
    CHECK(read->models[1].weight == Approx(-0.5));
    REQUIRE(read->values.size() == 1);
    CHECK(read->values[0].node == L"FaceM1");
    CHECK(read->values[0].parameter == L"g_vFacialHairWeights");
    CHECK(read->values[0].index == 5);
    CHECK(read->values[0].value == Approx(0.72));
    REQUIRE(read->vectors.size() == 1);
    CHECK(read->vectors[0].node == L"HairM1");
    CHECK(read->vectors[0].parameter == L"mml_vTintMaskColours");
    CHECK(read->vectors[0].index == 2);
    CHECK(read->vectors[0].value == std::array<float, 4>{0.4F, 0.2F, 0.1F, 1.0F});
    REQUIRE(read->textures.size() == 1);
    CHECK(read->textures[0].node == L"FaceM1");
    CHECK(read->textures[0].parameter == L"mml_tDiffuse");
    CHECK(read->textures[0].texture == L"uh_hed_dwfa_0d");
}

TEST_CASE("a file that is no head preset is not read as one", "[preset]")
{
    dao::test::Builder b = dao::test::Start("MORP", {{"morp", 4, {{2, 14, 0, 0}}}});
    dao::test::Struct(b, 4);
    const std::optional<gff4::File> file = gff4::File::Parse(b.bytes);
    REQUIRE(file);
    CHECK_FALSE(preset::PresetOf(*file));
}

TEST_CASE("hm_cps_p01.mop in game/heads, when there, is the human male's first preset", "[preset]")
{
    const std::filesystem::path path = Heads() / "hm_cps_p01.mop";
    if (!std::filesystem::exists(path))
    {
        SKIP("no hm_cps_p01.mop in game/heads (ghilana's extract writes it there)");
    }
    const std::vector<std::byte> bytes = FileBytes(path);
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file);
    const std::optional<preset::Preset> read = preset::PresetOf(*file);
    REQUIRE(read);
    CHECK(read->name == L"hm_cps_p01");
    CHECK(read->parts ==
          std::vector<std::wstring>{L"HM_UHM_BASa_0", L"hm_uem_basa_0", L"hm_har_hb4a_0", L"", L"", L"hm_ulm_basa_0"});
    REQUIRE(read->tints.size() == 12);
    CHECK(read->tints[0] == L"t1_skn_001");
    CHECK(read->tints[3] == L"t3_har_dbr");
    REQUIRE(read->models.size() == 24);
    CHECK(read->models[2].model == L"UH_UHM_BASa_0");
    CHECK(read->models[2].weight == Approx(-7.12));
    CHECK(read->values.size() == 8);
    REQUIRE(read->vectors.size() == 72);
    CHECK(read->vectors[2].node == L"EyesM1");
    CHECK(read->vectors[2].index == 7);
    CHECK(read->vectors[2].value[1] == Approx(0.65));
    REQUIRE(read->textures.size() == 4);
    CHECK(read->textures[1].texture == L"uh_hed_dwfa_0d");
}

TEST_CASE("every preset in game/heads, when there, has six parts, twelve tints, and each mesh's base weighed against "
          "its targets",
          "[preset]")
{
    if (!std::filesystem::exists(Heads()))
    {
        SKIP("no game/heads (ghilana's extract writes the .mop files there)");
    }
    std::size_t presets = 0;
    for (const auto &entry : std::filesystem::directory_iterator(Heads()))
    {
        if (entry.path().extension() != ".mop")
        {
            continue;
        }
        const std::vector<std::byte> bytes = FileBytes(entry.path());
        const std::optional<gff4::File> file = gff4::File::Parse(bytes);
        REQUIRE(file);
        const std::optional<preset::Preset> read = preset::PresetOf(*file);
        REQUIRE(read);
        ++presets;
        CHECK(read->parts.size() == 6);
        CHECK(read->tints.size() == 12);
        // A target's first letters say its mesh; the base takes minus its targets' sum.
        std::map<char, double> sum;
        std::map<char, double> base;
        for (const preset::ModelWeight &model : read->models)
        {
            const char mesh = static_cast<char>(std::towupper(model.model.at(1)));
            const bool isBase = model.model.find(L"_BAS") != std::wstring::npos;
            const char of = isBase ? static_cast<char>(std::towupper(model.model.at(4))) : mesh;
            (isBase ? base : sum)[of] += model.weight;
        }
        for (const auto &[mesh, total] : sum)
        {
            CHECK(std::abs(base[mesh] + total) < 1e-3);
        }
    }
    CHECK(presets == 84);
}

TEST_CASE("a morph config's lists are read by kind and race and gender, a hair's cut with its name", "[preset]")
{
    const preset::Config config = preset::ReadMorphConfig(R"(<?xml version="1.0"?>
<morph_config>
    <heads>
        <human_male>
            <resource name="HM_CPS_P01.mop"/>
            <resource name="HM_PCC_B01.mop"/>
        </human_male>
        <elf_female><resource name="EF_CPS_P01.mop"/></elf_female>
    </heads>
    <!-- <heads><resource name="old"/></heads> -->
    <hairs>
        <human_male>
            <resource name="hm_har_blda_0" cut="0234"/>
        </human_male>     1
    </hairs>
    <hair_colors>
        <resource name ="t3_har_wht"/>
        <resource name=""/>
    </hair_colors>
</morph_config>)");
    REQUIRE(config.lists.size() == 4);
    const preset::List *heads = config.Find("heads", "human_male");
    REQUIRE(heads);
    REQUIRE(heads->resources.size() == 2);
    CHECK(heads->resources[1].name == "HM_PCC_B01.mop");
    CHECK(heads->resources[1].cut.empty());
    REQUIRE(config.Find("heads", "elf_female"));
    CHECK(config.Find("heads", "elf_female")->resources[0].name == "EF_CPS_P01.mop");
    CHECK_FALSE(config.Find("heads"));
    const preset::List *hairs = config.Find("hairs", "human_male");
    REQUIRE(hairs);
    CHECK(hairs->resources[0].cut == "0234");
    const preset::List *colours = config.Find("hair_colors");
    REQUIRE(colours);
    CHECK(colours->group.empty());
    REQUIRE(colours->resources.size() == 2);
    CHECK(colours->resources[0].name == "t3_har_wht");
    CHECK(colours->resources[1].name.empty());
}

TEST_CASE("text with no morph_config has no lists", "[preset]")
{
    CHECK(preset::ReadMorphConfig(R"(<other><heads><resource name="x"/></heads></other>)").lists.empty());
}

TEST_CASE("a hairs list's choices are a hair once per digit of its cut", "[preset]")
{
    const preset::List hairs{
        .kind = "hairs",
        .group = "human_male",
        .resources = {{.name = "bald", .cut = "0234"}, {.name = "long", .cut = "1"}, {.name = "odd", .cut = ""}}};
    const std::vector<preset::HairChoice> choices = preset::HairChoices(hairs);
    REQUIRE(choices.size() == 6);
    CHECK(choices[0].name == "bald");
    CHECK(choices[0].cut == 0);
    CHECK(choices[3].cut == 4);
    CHECK(choices[4].name == "long");
    CHECK(choices[4].cut == 1);
    CHECK(choices[5].name == "odd");
    CHECK(choices[5].cut == 0);
}

TEST_CASE("chargenmorphcfg.xml in game/heads, when there, offers nine presets a race and gender", "[preset]")
{
    const std::filesystem::path path = Heads() / "chargenmorphcfg.xml";
    if (!std::filesystem::exists(path))
    {
        SKIP("no chargenmorphcfg.xml in game/heads (ghilana's extract writes it there)");
    }
    std::ifstream in(path, std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const preset::Config config = preset::ReadMorphConfig(text);
    CHECK(config.lists.size() == 25); // heads and hairs six each, beards two, eleven for all
    for (std::uint8_t i = 0; i < 6; ++i)
    {
        const auto raceGender = static_cast<preset::RaceGender>(i);
        const preset::List *heads = config.Find("heads", raceGender);
        REQUIRE(heads);
        CHECK(heads->resources.size() == 9);
        REQUIRE(config.Find("hairs", raceGender));
    }
    CHECK(config.Find("heads", preset::RaceGender::HumanMale)->resources[0].name == "HM_CPS_P01.mop");
    CHECK(config.Find("heads", preset::RaceGender::ElfFemale)->resources[0].name == "EF_CPS_P01.mop");
    const preset::List *hairs = config.Find("hairs", preset::RaceGender::HumanMale);
    CHECK(hairs->resources.size() == 16);
    CHECK(hairs->resources[0].cut == "0234");
    CHECK(preset::HairChoices(*hairs).size() == 19); // the bald hair four times
    CHECK(config.Find("hairs", preset::RaceGender::ElfFemale)->resources.size() == 17);
    REQUIRE(config.Find("beards", preset::RaceGender::DwarfMale));
    CHECK(config.Find("beards", preset::RaceGender::DwarfMale)->resources[0].name.empty());
    CHECK_FALSE(config.Find("beards", preset::RaceGender::ElfMale));
    const std::map<std::string, std::size_t> counts{{"hair_colors", 9},
                                                    {"skin_colors", 7},
                                                    {"eyes_colors", 11},
                                                    {"eyes_makeup_colors", 31},
                                                    {"blush_makeup_colors", 25},
                                                    {"lip_makeup_colors", 25},
                                                    {"brow_stubble_colors", 9},
                                                    {"crew_cut_colors", 9},
                                                    {"tattoo_colors", 15},
                                                    {"tattoos", 9},
                                                    {"skins", 6}};
    for (const auto &[kind, count] : counts)
    {
        const preset::List *list = config.Find(kind);
        REQUIRE(list);
        CHECK(list->resources.size() == count);
    }
    CHECK(config.Find("skins")->resources.back().name == "uh_hed_quna_0d");
}

namespace
{

// Meshes by model name, each vertex a place and a texture coordinate.
std::optional<preset::MeshVertices> MadeMesh(std::wstring_view model)
{
    using V = std::vector<std::array<float, 3>>;
    using U = std::vector<std::array<float, 2>>;
    if (model == L"Face")
    {
        return preset::MeshVertices{.positions = V{{0, 0, 0}, {1, 0, 0}, {5, 5, 5}},
                                    .uvs = U{{0, 0}, {1, 0}, {0.5F, 0.5F}}};
    }
    // The base and the targets in another order, (0, 0) twice as a seam's two sides far apart.
    if (model == L"uh_uhm_basa_0")
    {
        return preset::MeshVertices{.positions = V{{1.1F, 0, 0}, {9, 9, 9}, {0.1F, 0, 0}},
                                    .uvs = U{{1, 0}, {0, 0}, {0, 0}}};
    }
    if (model == L"UH_NOS_01a_0")
    {
        return preset::MeshVertices{.positions = V{{0.1F, 0, 1}, {9, 9, 9}, {1.1F, 2, 0}},
                                    .uvs = U{{0, 0}, {0, 0}, {1, 0}}};
    }
    if (model == L"UHM_EYE_01a_0")
    {
        return preset::MeshVertices{.positions = V{{0.1F, 0, 2}, {1.1F, 0, 0}}, .uvs = U{{0, 0}, {1, 0}}};
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("a head composed from a preset is its part moved by each target's difference from the base", "[preset]")
{
    preset::Preset made;
    made.name = L"made";
    made.parts = {L"Face", L"", L"", L"", L"", L"Lashes"};
    made.tints = {L"t1_skn_001"};
    made.models = {{.model = L"UH_NOS_01a_0", .weight = 1.0F},
                   {.model = L"UHM_EYE_01a_0", .weight = 0.5F},
                   {.model = L"UH_UHM_BASa_0", .weight = -1.5F}, // the base's bookkeeping, not summed
                   {.model = L"UH_JAW_99a_0", .weight = 3.0F}};  // no such model: left out
    made.vectors = {
        {.node = L"HairM1", .parameter = L"mml_vTintMaskColours", .index = 2, .value = {0.4F, 0.2F, 0.1F, 1}}};
    made.values = {{.node = L"FaceM1", .parameter = L"g_vFacialHairWeights", .index = 5, .value = 0.72F}};
    made.textures = {{.node = L"FaceM1", .parameter = L"mml_tDiffuse", .texture = L"uh_hed_dwfa_0d"}};
    const dao::core::morph::Morph head = preset::ComposeHead(made, MadeMesh);
    CHECK(head.name == L"made");
    CHECK(head.parts == made.parts);
    CHECK(head.tints == made.tints);
    REQUIRE(head.nodes.size() == 4);
    CHECK(head.nodes[0].name == L"FaceM1");
    REQUIRE(head.nodes[0].places.size() == 3);
    // +1 z by the nose, +1 z by half the eye; the seam's far side is not taken.
    CHECK(head.nodes[0].places[0][2] == Approx(2));
    CHECK(head.nodes[0].places[0][0] == Approx(0));
    CHECK(head.nodes[0].places[1][1] == Approx(2)); // +2 y by the nose
    CHECK(head.nodes[0].places[1][0] == Approx(1));
    CHECK(head.nodes[0].places[1][3] == Approx(1));
    CHECK(head.nodes[0].places[2] == std::array<float, 4>{5, 5, 5, 1}); // a coordinate the base has not
    CHECK(head.nodes[1].name == L"EyesM1");
    CHECK(head.nodes[1].places.empty()); // no part
    CHECK(head.nodes[2].name == L"LashesM1");
    CHECK(head.nodes[2].places.empty()); // a part not found
    REQUIRE(head.nodes[0].vectors.size() == 1);
    CHECK(head.nodes[0].vectors[0].name == L"g_vFacialHairWeights");
    REQUIRE(head.nodes[0].vectors[0].values.size() == 2);
    CHECK(head.nodes[0].vectors[0].values[1] == std::array<float, 4>{0, 0.72F, 0, 0});
    REQUIRE(head.nodes[0].textures.size() == 1);
    CHECK(head.nodes[0].textures[0].texture == L"uh_hed_dwfa_0d");
    CHECK(head.nodes[3].name == L"HairM1");
    REQUIRE(head.nodes[3].vectors.size() == 1);
    REQUIRE(head.nodes[3].vectors[0].values.size() == 3);
    CHECK(head.nodes[3].vectors[0].values[2] == std::array<float, 4>{0.4F, 0.2F, 0.1F, 1});
}

namespace
{

// A model's first mesh chunk from game/heads/targets, by its hierarchy (ghilana's extract of the
// .mmh and its .msh).
std::optional<preset::MeshVertices> TargetMesh(std::wstring_view model)
{
    namespace m = dao::core::model;
    std::wstring name(model);
    for (wchar_t &c : name)
    {
        c = static_cast<wchar_t>(std::towlower(c));
    }
    const std::filesystem::path targets = Heads() / "targets";
    const std::vector<std::byte> hierarchyBytes = FileBytes(targets / (name + L".mmh"));
    const std::optional<gff4::File> hierarchyFile = gff4::File::Parse(hierarchyBytes);
    const std::optional<m::Hierarchy> hierarchy = hierarchyFile ? m::HierarchyOf(*hierarchyFile) : std::nullopt;
    if (!hierarchy)
    {
        return std::nullopt;
    }
    const std::vector<std::byte> meshBytes = FileBytes(targets / hierarchy->meshFile);
    const std::optional<gff4::File> meshFile = gff4::File::Parse(meshBytes);
    const std::optional<m::Mesh> mesh = meshFile ? m::MeshOf(*meshFile) : std::nullopt;
    if (!mesh || mesh->chunks.empty())
    {
        return std::nullopt;
    }
    const m::Chunk &chunk = mesh->chunks.front();
    const m::Element *position = chunk.Find(m::Usage::Position);
    const m::Element *uv = chunk.Find(m::Usage::TexCoord);
    if (position == nullptr || uv == nullptr)
    {
        return std::nullopt;
    }
    preset::MeshVertices out;
    for (const m::Value &p : m::Floats(chunk, *position))
    {
        out.positions.push_back({p[0], p[1], p[2]});
    }
    for (const m::Value &t : m::Floats(chunk, *uv))
    {
        out.uvs.push_back({t[0], t[1]});
    }
    return out;
}

const dao::core::morph::Node *FaceOf(const dao::core::morph::Morph &head)
{
    const auto found = std::ranges::find(head.nodes, std::wstring(L"FaceM1"), &dao::core::morph::Node::name);
    return found == head.nodes.end() ? nullptr : &*found;
}

} // namespace

TEST_CASE("hm_pcc_b02 composed from game/heads, when there, comes near the shipped hm_pcc_b02.mor", "[preset]")
{
    const std::filesystem::path mop = Heads() / "hm_pcc_b02.mop";
    const std::filesystem::path mor = Heads() / "hm_pcc_b02.mor";
    if (!std::filesystem::exists(mop) || !std::filesystem::exists(mor) ||
        !std::filesystem::exists(Heads() / "targets" / "uh_chm_07a_0.mmh"))
    {
        SKIP("no hm_pcc_b02 and its targets in game/heads (ghilana's extract writes them there)");
    }
    const std::vector<std::byte> presetBytes = FileBytes(mop);
    const std::optional<gff4::File> presetFile = gff4::File::Parse(presetBytes);
    REQUIRE(presetFile);
    const std::optional<preset::Preset> read = preset::PresetOf(*presetFile);
    REQUIRE(read);
    const dao::core::morph::Morph head = preset::ComposeHead(*read, TargetMesh);
    const std::vector<std::byte> morphBytes = FileBytes(mor);
    const std::optional<gff4::File> morphFile = gff4::File::Parse(morphBytes);
    REQUIRE(morphFile);
    const std::optional<dao::core::morph::Morph> shipped = dao::core::morph::MorphOf(*morphFile);
    REQUIRE(shipped);
    const dao::core::morph::Node *ours = FaceOf(head);
    const dao::core::morph::Node *theirs = FaceOf(*shipped);
    REQUIRE(ours);
    REQUIRE(theirs);
    REQUIRE(ours->places.size() == 2282);
    REQUIRE(theirs->places.size() == ours->places.size());
    std::size_t close = 0;
    float worst = 0;
    for (std::size_t i = 0; i < ours->places.size(); ++i)
    {
        float off = 0;
        for (std::size_t k = 0; k < 3; ++k)
        {
            off = std::max(off, std::abs(ours->places[i].at(k) - theirs->places[i].at(k)));
        }
        close += off < 1e-4F ? 1 : 0;
        worst = std::max(worst, off);
    }
    // 1,628 within 0.1 mm and none past 1.7 cm, as read 2026-10-18 (HeadPreset.cpp's notes).
    CHECK(close >= 1600);
    CHECK(worst < 0.017F);
}
