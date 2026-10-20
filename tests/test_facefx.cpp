// dao/core/FaceFx.h over an .fxe and an .fxa made here, and over the game's in
// game/facefx when they are there (DAOLib #206).
#include "dao/core/FaceFx.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <numbers>
#include <span>
#include <string>
#include <utility>
#include <vector>

using Catch::Approx;
namespace facefx = dao::core::facefx;

namespace
{

class Bytes
{
  public:
    Bytes &U32(std::uint32_t v)
    {
        return Raw(&v, 4);
    }
    Bytes &U16(std::uint16_t v)
    {
        return Raw(&v, 2);
    }
    Bytes &F32(float v)
    {
        return Raw(&v, 4);
    }
    Bytes &Str(const std::string &s)
    {
        U32(static_cast<std::uint32_t>(s.size()));
        return Raw(s.data(), s.size());
    }
    Bytes &Floats(std::initializer_list<float> v)
    {
        for (const auto f : v)
        {
            F32(f);
        }
        return *this;
    }
    Bytes &Raw(const void *p, std::size_t n)
    {
        const auto *b = static_cast<const std::byte *>(p);
        data.insert(data.end(), b, b + n);
        return *this;
    }
    std::vector<std::byte> data;
};

// The header: magic, version, licensee, the class versions in one bucket each, and the names.
Bytes Header(const std::vector<std::pair<std::string, std::uint16_t>> &classes, const std::vector<std::string> &names)
{
    Bytes b;
    b.Raw("BIOE", 4).U32(1710).Str("Licensee").Str("Project").U32(1000).U16(1);
    b.U32(static_cast<std::uint32_t>(classes.size()));
    std::uint32_t bucket = 0;
    for (const auto &[name, version] : classes)
    {
        b.U32(bucket++).U32(1).Str(name).U16(version);
    }
    b.U32(static_cast<std::uint32_t>(names.size()));
    for (const auto &n : names)
    {
        b.Str(n);
    }
    return b;
}

// An .fxe of one line with two curves: "jaw" (Hermite, 3 keys) and "lip" (linear, 2 keys).
std::vector<std::byte> MakeFxe()
{
    auto b = Header({{"FxAnimSet", 0}, {"FxAnim", 6}, {"FxAnimCurve", 1}, {"FxAnimGroup", 0}, {"FxName", 1}},
                    {"line", "actor", "jaw", "lip"});
    b.U32(0).U32(0); // the set's name, the second slot
    b.U32(1);        // groups
    b.U32(0).U32(1); // group name, one anim
    b.U32(0);        // anim name
    b.U32(2).U32(2).U32(0).U32(3).U32(1);
    b.U32(5); // keys
    b.Floats({-0.5f, 0.0f, 0.0f, 0.0f});
    b.Floats({0.0f, 1.0f, 0.0f, 2.0f});
    b.Floats({1.0f, 0.0f, -1.0f, 0.0f});
    b.Floats({0.0f, 0.0f, 0.0f, 0.0f});
    b.Floats({2.0f, 1.0f, 0.0f, 0.0f});
    b.U32(2).U32(3).U32(2);
    b.F32(0.16f).F32(0.22f);
    b.U32(1).U32(2).F32(0.5f); // a bone weight
    b.Str("").Str("").U32(0xFFFFFFFF);
    return b.data;
}

// An .fxa of one bone with one pose, three nodes (a combiner with a link, a bone target, a material target) and two
// phonemes.
std::vector<std::byte> MakeFxa()
{
    auto b = Header({{"FxActor", 3},
                     {"FxMasterBoneList", 2},
                     {"FxBone", 0},
                     {"FxCompiledFaceGraph", 0},
                     {"FxAnimGroup", 0},
                     {"FxName", 1},
                     {"FxLinkFnParameters", 1}},
                    {"actor", "bone", "sum", "jawOpen", "emo", "Default", "AA", "Material parameter index"});
    b.U32(0).U32(0);
    b.U32(1);                                        // bones
    b.U32(1).Floats({1, 2, 3, 1, 0, 0, 0, 1, 1, 1}); // reference
    b.Floats({1, 0, 0, 0}).U32(35).F32(1.0f);
    b.U32(1).U32(1).U32(1).Floats({0, 0.5f, 0, 0.9f, 0.1f, 0, 0, 0, 0, 0}); // one pose, from node 1
    b.U32(3);                                                               // nodes
    b.U32(0).U32(2).Floats({0, 1, 1, 1}).U32(1);
    b.U32(1).U32(1).U32(1).U32(2).F32(0.25f).F32(0.5f); // a link: from node 1, function 1, two parameters
    b.U32(0);
    b.U32(4).U32(3).Floats({0, 1, 1, 1}).U32(0).U32(0).U32(0);
    b.U32(8).U32(4).Floats({-45, -1.0f / 45, 45, 1.0f / 45}).U32(0).U32(0);
    b.U32(1).U32(7).U32(2).U32(3).F32(1.5f).U32(1).Str("FaceM1");
    b.U32(2);                          // the name map: two buckets
    b.U32(7).U32(1).Str("sum").U32(0); // bucket 7 holds "sum"
    b.U32(9).U32(2).Str("jawOpen").U32(1).Str("emo").U32(2);
    b.U32(5).U32(0); // the actor's group "Default", no anims
    b.U32(0);        // groups
    b.U32(2).U32(10).U32(3).F32(0.75f).U32(11).U32(4).F32(0.5f);
    b.U32(1).U32(6);
    return b.data;
}

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());
    if (!chars.empty())
    {
        std::memcpy(out.data(), chars.data(), chars.size());
    }
    return out;
}

std::filesystem::path Samples()
{
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "facefx";
}

} // namespace

TEST_CASE("an .fxe's header, curves and their keys", "[facefx]")
{
    const auto set = facefx::ReadAnimSet(MakeFxe());
    REQUIRE(set);
    CHECK(set->header.archiveVersion == 1710);
    CHECK(set->header.licensee == "Licensee");
    CHECK(set->header.ClassVersion("FxAnim") == 6);
    CHECK(set->header.names.size() == 4);
    CHECK(set->name == "line");
    REQUIRE(set->groups.size() == 1);
    CHECK(set->groups[0].name == "line");
    REQUIRE(set->groups[0].anims.size() == 1);
    const auto &anim = set->groups[0].anims[0];
    REQUIRE(anim.curves.size() == 2);
    CHECK(anim.curves[0].nodeName == "jaw");
    CHECK(anim.curves[0].interpolation == 0);
    REQUIRE(anim.curves[0].keys.size() == 3);
    CHECK(anim.curves[0].keys[1].value == Approx(1.0f));
    CHECK(anim.curves[0].keys[1].slopeOut == Approx(2.0f));
    CHECK(anim.curves[0].keys[2].slopeIn == Approx(-1.0f));
    CHECK(anim.curves[1].nodeName == "lip");
    CHECK(anim.curves[1].interpolation == 1);
    CHECK(anim.curves[1].keys.size() == 2);
    CHECK(anim.blendIn == Approx(0.16f));
    CHECK(anim.blendOut == Approx(0.22f));
    REQUIRE(anim.boneWeights.size() == 1);
    CHECK(anim.boneWeights[0].bone == "jaw");
    CHECK(anim.boneWeights[0].weight == Approx(0.5f));
    CHECK(anim.tail == 0xFFFFFFFFu);
    CHECK(anim.EndTime() == Approx(2.0f));
}

TEST_CASE("a curve's value: Hermite by the slopes, linear, and held outside its keys", "[facefx]")
{
    const auto set = facefx::ReadAnimSet(MakeFxe());
    REQUIRE(set);
    const auto &hermite = set->groups[0].anims[0].curves[0];
    const auto &linear = set->groups[0].anims[0].curves[1];
    CHECK(hermite.Evaluate(-9) == Approx(0.0f));
    CHECK(hermite.Evaluate(-0.5f) == Approx(0.0f));
    CHECK(hermite.Evaluate(0.0f) == Approx(1.0f));
    CHECK(hermite.Evaluate(1.0f) == Approx(0.0f));
    CHECK(hermite.Evaluate(5) == Approx(0.0f));
    // Between the keys at 0 and 1: v0 = 1, v1 = 0, m0 = 2, m1 = -1, s = 0.5.
    // v = 1 + 0.5 * (2 + 0.5 * ((-3 - 4 + 1) + 0.5 * (2 - 1 + 2))) = 1 + 0.5 * (2 + 0.5 * (-6 + 1.5)) = 0.875
    CHECK(hermite.Evaluate(0.5f) == Approx(0.875f));
    CHECK(linear.Evaluate(1.0f) == Approx(0.5f));
    CHECK(linear.Evaluate(3.0f) == Approx(1.0f));
    CHECK(facefx::AnimCurve{}.Evaluate(1.0f) == 0.0f);
    facefx::AnimCurve one;
    one.keys = {{3.0f, 0.4f, 0, 0}};
    CHECK(one.Evaluate(100.0f) == Approx(0.4f));
}

TEST_CASE("an .fxa's bones, face graph, name map and phonemes", "[facefx]")
{
    const auto actor = facefx::ReadActor(MakeFxa());
    REQUIRE(actor);
    CHECK(actor->name == "actor");
    REQUIRE(actor->bones.size() == 1);
    const auto &bone = actor->bones[0];
    CHECK(bone.reference.name == "bone");
    CHECK(bone.reference.position[2] == Approx(3.0f));
    CHECK(bone.reference.rotation[0] == Approx(1.0f));
    CHECK(bone.unknownIndex == 35);
    REQUIRE(bone.poses.size() == 1);
    CHECK(bone.poses[0].nodeIndex == 1);
    CHECK(bone.poses[0].pose.name == "bone");
    CHECK(bone.poses[0].pose.position[1] == Approx(0.5f));
    CHECK(bone.poses[0].pose.rotation[0] == Approx(0.9f));
    REQUIRE(actor->nodes.size() == 3);
    const auto &sum = actor->nodes[0];
    CHECK(sum.type == 0);
    CHECK(sum.name == "sum");
    CHECK(sum.inputOperation == 1);
    REQUIRE(sum.links.size() == 1);
    CHECK(sum.links[0].inputNode == 1);
    CHECK(sum.links[0].functionId == 1);
    REQUIRE(sum.links[0].parameters.size() == 2);
    CHECK(sum.links[0].parameters[1] == Approx(0.5f));
    CHECK(actor->nodes[1].type == 4);
    const auto &emo = actor->nodes[2];
    CHECK(emo.type == 8);
    CHECK(emo.min == Approx(-45.0f));
    CHECK(emo.maxReciprocal == Approx(1.0f / 45));
    REQUIRE(emo.userProperties.size() == 1);
    CHECK(emo.userProperties[0].name == "Material parameter index");
    CHECK(emo.userProperties[0].c == Approx(1.5f));
    REQUIRE(emo.userProperties[0].strings.size() == 1);
    CHECK(emo.userProperties[0].strings[0] == "FaceM1");
    CHECK(actor->nodeIndexByName.size() == 3);
    CHECK(actor->NodeIndex("emo") == 2);
    CHECK(actor->NodeIndex("sum") == 0);
    CHECK_FALSE(actor->NodeIndex("nothing"));
    CHECK(actor->defaultGroup.name == "Default");
    CHECK(actor->groups.empty());
    REQUIRE(actor->phonemeMap.size() == 2);
    CHECK(actor->phonemeMap[0].phoneme == 10);
    CHECK(actor->phonemeMap[0].node == "jawOpen");
    CHECK(actor->phonemeMap[1].weight == Approx(0.5f));
    REQUIRE(actor->phonemeNames.size() == 1);
    CHECK(actor->phonemeNames[0] == "AA");
}

TEST_CASE("cut, damaged and foreign bytes are refused without a crash", "[facefx]")
{
    const auto fxe = MakeFxe();
    const auto fxa = MakeFxa();
    // Every cut of either file fails, in either reader.
    for (std::size_t n = 0; n < fxe.size(); ++n)
    {
        CHECK_FALSE(facefx::ReadAnimSet(std::span(fxe).first(n)));
    }
    for (std::size_t n = 0; n < fxa.size(); ++n)
    {
        CHECK_FALSE(facefx::ReadActor(std::span(fxa).first(n)));
    }
    // Each is the other's foreign.
    CHECK_FALSE(facefx::ReadActor(fxe));
    CHECK_FALSE(facefx::ReadAnimSet(fxa));
    // Bytes left over.
    auto longer = fxe;
    longer.push_back(std::byte{0});
    CHECK_FALSE(facefx::ReadAnimSet(longer));
    // A wrong magic, a wrong archive version, a count beyond the bytes.
    auto bad = fxe;
    bad[0] = std::byte{'X'};
    CHECK_FALSE(facefx::ReadAnimSet(bad));
    bad = fxe;
    bad[4] = std::byte{0};
    CHECK_FALSE(facefx::ReadAnimSet(bad));
    CHECK_FALSE(facefx::ReadAnimSet({}));
    // Every byte of the file set to 0xFF in turn: refused or read, never a crash.
    for (std::size_t i = 0; i < fxe.size(); ++i)
    {
        auto damaged = fxe;
        damaged[i] = std::byte{0xFF};
        (void)facefx::ReadAnimSet(damaged);
    }
    for (std::size_t i = 0; i < fxa.size(); ++i)
    {
        auto damaged = fxa;
        damaged[i] = std::byte{0xFF};
        (void)facefx::ReadActor(damaged);
    }
}

TEST_CASE("an animation whose key counts do not cut its pool is refused", "[facefx]")
{
    auto fxe = MakeFxe();
    // The counts are 2 and 3 of 5 keys; make the second 2 (a pool left over).
    for (std::size_t i = 0; i + 8 <= fxe.size(); ++i)
    {
        std::uint32_t a = 0;
        std::uint32_t b = 0;
        std::memcpy(&a, &fxe[i], 4);
        std::memcpy(&b, &fxe[i + 4], 4);
        if (a == 3 && b == 2 && i > 100)
        {
            const std::uint32_t two = 2;
            std::memcpy(&fxe[i], &two, 4);
            break;
        }
    }
    CHECK_FALSE(facefx::ReadAnimSet(fxe));
}

TEST_CASE("the game's .fxe files in game/facefx, when there, each read and its curves sampled", "[facefx][game]")
{
    if (!std::filesystem::is_directory(Samples()))
    {
        SKIP("no game/facefx (the .fxe of the campaigns' modulefacialanimations.erf and face.erf's humanmale.fxa)");
    }
    std::size_t files = 0;
    for (const auto &entry : std::filesystem::directory_iterator(Samples()))
    {
        if (entry.path().extension() != ".fxe")
        {
            continue;
        }
        const auto set = facefx::ReadAnimSet(FromDisk(entry.path()));
        REQUIRE(set);
        REQUIRE(set->groups.size() == 1);
        REQUIRE(set->groups[0].anims.size() == 1);
        const auto &anim = set->groups[0].anims[0];
        CHECK(anim.curves.size() >= 30);
        CHECK(anim.tail == 0xFFFFFFFFu);
        CHECK(set->name == entry.path().stem().string());
        for (const auto &curve : anim.curves)
        {
            CHECK(curve.interpolation == 0);
            CHECK(std::isfinite(curve.Evaluate(anim.EndTime() / 2)));
        }
        ++files;
    }
    CHECK(files >= 10);
}

TEST_CASE("the game's 100007_m.fxe: 44 curves, 134 keys, jawOpen's first keys", "[facefx][game]")
{
    const auto path = Samples() / "100007_m.fxe";
    if (!std::filesystem::exists(path))
    {
        SKIP("no game/facefx/100007_m.fxe");
    }
    const auto set = facefx::ReadAnimSet(FromDisk(path));
    REQUIRE(set);
    const auto &anim = set->groups[0].anims[0];
    REQUIRE(anim.curves.size() == 44);
    std::size_t keys = 0;
    for (const auto &c : anim.curves)
    {
        keys += c.keys.size();
    }
    CHECK(keys == 134);
    CHECK(anim.curves[0].nodeName == "jawOpen");
    REQUIRE(anim.curves[0].keys.size() == 7);
    CHECK(anim.curves[0].keys[0].time == Approx(-0.2717f).margin(1e-3));
    CHECK(anim.curves[0].keys[0].value == 0.0f);
    CHECK(anim.curves[0].keys[1].time == Approx(-0.0050f).margin(1e-3));
    CHECK(anim.curves[0].keys[1].value == Approx(0.0781f).margin(1e-3));
    CHECK(anim.blendIn == Approx(0.16f));
    CHECK(anim.blendOut == Approx(0.22f));
    CHECK(set->header.ClassVersion("FxAnim") == 6);
}

TEST_CASE("the game's humanmale.fxa: 38 bones, 201 nodes, and every curve's node in the actor", "[facefx][game]")
{
    const auto path = Samples() / "humanmale.fxa";
    if (!std::filesystem::exists(path))
    {
        SKIP("no game/facefx/humanmale.fxa");
    }
    const auto actor = facefx::ReadActor(FromDisk(path));
    REQUIRE(actor);
    CHECK(actor->name == "humanmale");
    CHECK(actor->bones.size() == 38);
    CHECK(actor->nodes.size() == 201);
    CHECK(actor->nodeIndexByName.size() == 201);
    CHECK(actor->phonemeMap.size() == 357);
    CHECK(actor->phonemeNames.size() == 29);
    CHECK(actor->defaultGroup.name == "Default");
    CHECK(actor->groups.empty());
    CHECK(actor->bones[0].reference.name == "default_brow_left");
    CHECK(actor->bones[0].unknownIndex == 35);
    for (const auto &bone : actor->bones)
    {
        for (const auto &pose : bone.poses)
        {
            CHECK(pose.nodeIndex < actor->nodes.size());
        }
    }
    for (const auto &node : actor->nodes)
    {
        for (const auto &link : node.links)
        {
            CHECK(link.inputNode < actor->nodes.size());
        }
    }
    const auto yaw = actor->NodeIndex("Gaze Eye Yaw");
    REQUIRE(yaw);
    CHECK(actor->nodes[*yaw].min == Approx(-45.0f));
    CHECK(actor->nodes[*yaw].max == Approx(45.0f));
    // The curves of a line name nodes of the actor; a few name none (the engine skips those), 42 of 44 here.
    const auto line = path.parent_path() / "100007_m.fxe";
    if (std::filesystem::exists(line))
    {
        const auto set = facefx::ReadAnimSet(FromDisk(line));
        REQUIRE(set);
        std::size_t found = 0;
        for (const auto &curve : set->groups[0].anims[0].curves)
        {
            found += actor->NodeIndex(curve.nodeName) ? 1 : 0;
        }
        CHECK(found == 42);
    }
}

namespace
{

facefx::FaceGraphNode Node(std::string name, float min = -10, float max = 10, std::uint32_t op = 0)
{
    facefx::FaceGraphNode n;
    n.name = std::move(name);
    n.min = min;
    n.minReciprocal = min == 0 ? 1.0f : 1.0f / min;
    n.max = max;
    n.maxReciprocal = max == 0 ? 1.0f : 1.0f / max;
    n.inputOperation = op;
    return n;
}

facefx::Anim Curves(std::initializer_list<std::pair<const char *, float>> constants)
{
    facefx::Anim anim;
    for (const auto &[name, value] : constants)
    {
        facefx::AnimCurve c;
        c.nodeName = name;
        c.keys = {{0.0f, value, 0, 0}};
        anim.curves.push_back(std::move(c));
    }
    return anim;
}

// A graph of an input "in" and a node "out" linked from it by one function.
float Through(std::uint32_t function, std::vector<float> params, float x)
{
    facefx::Actor actor;
    actor.nodes.push_back(Node("in"));
    auto out = Node("out", -100, 100);
    out.links.push_back({0, function, std::move(params)});
    actor.nodes.push_back(std::move(out));
    return facefx::EvaluateGraph(actor, Curves({{"in", x}}), 0)[1];
}

} // namespace

TEST_CASE("each link function by id, with its defaults", "[facefx]")
{
    CHECK(Through(0, {}, 3) == Approx(0.0f));
    CHECK(Through(1, {2, 1}, 3) == Approx(7.0f));
    CHECK(Through(1, {2}, 3) == Approx(6.0f));
    CHECK(Through(1, {}, 3) == Approx(3.0f));
    CHECK(Through(2, {}, -3) == Approx(-9.0f));
    CHECK(Through(2, {0.5f}, 3) == Approx(4.5f));
    CHECK(Through(3, {}, -2) == Approx(-8.0f));
    CHECK(Through(3, {2}, 2) == Approx(16.0f));
    CHECK(Through(4, {}, 4) == Approx(2.0f));
    CHECK(Through(4, {}, -4) == Approx(-2.0f));
    CHECK(Through(4, {3}, 4) == Approx(6.0f));
    CHECK(Through(4, {}, 0) == Approx(0.0f));
    CHECK(Through(5, {}, 3) == Approx(-3.0f));
    CHECK(Through(6, {}, 4) == Approx(0.25f));
    CHECK(Through(6, {}, 0) == Approx(0.0f));
    CHECK(Through(7, {}, 0.5f) == Approx(1.0f));
    CHECK(Through(7, {}, -3) == Approx(1.0f));
    CHECK(Through(7, {}, 4) == Approx(0.25f));
    CHECK(Through(8, {2.5f}, 9) == Approx(2.5f));
    CHECK(Through(8, {}, 9) == Approx(1.0f));
    // ClampedLinear: m 2, clampx 1, clampy 0.5; the direction above and below.
    CHECK(Through(10, {2, 1, 0.5f, 1}, 3) == Approx(4.5f));
    CHECK(Through(10, {2, 1, 0.5f, 1}, 0) == Approx(0.5f));
    CHECK(Through(10, {2, 1, 0.5f, -1}, 0) == Approx(-1.5f));
    CHECK(Through(10, {2, 1, 0.5f, -1}, 3) == Approx(0.5f));
    CHECK(Through(10, {2}, 3) == Approx(6.0f));
    CHECK(Through(10, {2}, -3) == Approx(0.0f));
    CHECK(Through(10, {}, 3) == Approx(3.0f));
    // An id the game does not use is a Null.
    CHECK(Through(11, {}, 3) == Approx(0.0f));
}

TEST_CASE("a node's input operation, its curve, its clamp and its correctives", "[facefx]")
{
    // Two linked inputs, summed and multiplied; the curve adds to the links' result.
    facefx::Actor actor;
    actor.nodes.push_back(Node("a"));
    actor.nodes.push_back(Node("b"));
    auto sum = Node("sum");
    sum.links = {{0, 1, {}}, {1, 1, {}}};
    auto product = Node("product", -10, 10, 1);
    product.links = {{0, 1, {}}, {1, 1, {}}};
    auto lone = Node("lone", -10, 10, 1); // multiply with no links starts at 0
    auto clamped = Node("clamped", 0, 1);
    clamped.links = {{0, 1, {}}};
    actor.nodes.push_back(sum);
    actor.nodes.push_back(product);
    actor.nodes.push_back(lone);
    actor.nodes.push_back(clamped);
    const auto values = facefx::EvaluateGraph(
        actor, Curves({{"a", 3.0f}, {"b", 4.0f}, {"sum", 0.5f}, {"lone", 2.0f}, {"nothing", 9.0f}}), 0);
    REQUIRE(values.size() == 6);
    CHECK(values[2] == Approx(7.5f));
    CHECK(values[3] == Approx(10.0f)); // 12 clamped to max
    CHECK(values[4] == Approx(2.0f));
    CHECK(values[5] == Approx(1.0f)); // 3 clamped to 1

    // A corrective link attenuates by the input's activity as a part of its range.
    facefx::Actor cor;
    cor.nodes.push_back(Node("base", 0, 1));
    cor.nodes.push_back(Node("damper", 0, 1));
    auto target = Node("target", 0, 10);
    target.links = {{0, 8, {4}}, {1, 9, {0.5f}}};
    cor.nodes.push_back(target);
    const auto damped = Curves({{"damper", 1.0f}});
    CHECK(facefx::EvaluateGraph(cor, damped, 0)[2] == Approx(2.0f)); // 4 * (1 - 0.5 * 1)
    cor.nodes[2].links[1].parameters = {3};
    CHECK(facefx::EvaluateGraph(cor, damped, 0)[2] == Approx(0.0f)); // the accumulator is capped at 1
}

TEST_CASE("bones follow node values, the emotion weights follow the type 8 nodes", "[facefx]")
{
    facefx::Actor actor;
    actor.nodes.push_back(Node("drive", 0, 1));
    auto emo = Node("emo", 0, 1);
    emo.type = 8;
    emo.userProperties.push_back({"Material parameter index", 0, 5, 0, {}});
    actor.nodes.push_back(emo);
    facefx::MasterBone bone;
    bone.reference.position = {1, 2, 3};
    bone.conjugate = {1, 0, 0, 0};
    facefx::BonePose pose;
    pose.nodeIndex = 0;
    pose.pose.position = {10, 20, 30};
    pose.pose.scale = {1, 0, 0};
    const float h = std::sqrt(0.5f); // a quarter turn about x
    pose.pose.rotation = {h, h, 0, 0};
    bone.poses.push_back(pose);
    actor.bones.push_back(bone);

    const auto rest = facefx::Evaluate(actor, Curves({}), 0);
    CHECK(rest.bones[0].position[0] == Approx(1.0f));
    CHECK(rest.bones[0].rotation[0] == Approx(1.0f));
    CHECK(rest.bones[0].rotationDelta[0] == Approx(1.0f));

    const auto full = facefx::Evaluate(actor, Curves({{"drive", 1.0f}, {"emo", 0.75f}}), 0);
    CHECK(full.bones[0].position[1] == Approx(22.0f));
    CHECK(full.bones[0].scale[0] == Approx(2.0f));
    CHECK(full.bones[0].rotation[0] == Approx(h).margin(1e-3));
    CHECK(full.bones[0].rotation[1] == Approx(h).margin(1e-3));
    CHECK(full.bones[0].positionDelta[0] == Approx(0.1f));
    CHECK(full.bones[0].positionDelta[1] == Approx(0.3f)); // z in y's place
    CHECK(full.bones[0].positionDelta[2] == Approx(0.2f));
    CHECK(full.emotionWeights[5] == Approx(0.75f));
    CHECK(full.emotionWeights[0] == 0.0f);

    // Half way is an eighth of a turn about x.
    const auto half = facefx::Evaluate(actor, Curves({{"drive", 0.5f}}), 0);
    CHECK(half.bones[0].rotation[1] == Approx(std::sin(std::numbers::pi_v<float> / 8)).margin(1e-2));
}

TEST_CASE("a bone's turn is from its reference, not on it: a quarter turn about z to a half turn is a quarter turn",
          "[facefx]")
{
    facefx::Actor actor;
    actor.nodes.push_back(Node("drive", 0, 1));
    const float h = std::sqrt(0.5f);
    facefx::MasterBone bone;
    bone.reference.rotation = {h, 0, 0, h}; // a quarter turn about z
    bone.conjugate = {h, 0, 0, -h};
    facefx::BonePose pose;
    pose.nodeIndex = 0;
    pose.pose.rotation = {0, 0, 0, 1}; // a half turn about z
    bone.poses.push_back(pose);
    actor.bones.push_back(bone);

    const auto full = facefx::Evaluate(actor, Curves({{"drive", 1.0f}}), 0);
    const auto &turn = full.bones[0].rotationDelta;
    // The engine's product starts from identity; one that started from the reference gave a half turn here.
    CHECK(std::fabs(turn[0]) == Approx(h).margin(1e-4));
    CHECK(std::fabs(turn[3]) == Approx(h).margin(1e-4));
    CHECK(turn[0] * turn[3] > 0);
    // Handed on as {x, z, y, -w}: z in y's place and w negated.
    CHECK(full.bones[0].nodeRotation[1] == Approx(turn[3]).margin(1e-6));
    CHECK(full.bones[0].nodeRotation[3] == Approx(-turn[0]).margin(1e-6));
    // On the reference, a half turn.
    CHECK(std::fabs(full.bones[0].rotation[3]) == Approx(1.0f).margin(1e-4));
}

TEST_CASE("the game's humanmale.fxa with 100007_m at a few times: finite values within each node's range",
          "[facefx][game]")
{
    const auto actorPath = Samples() / "humanmale.fxa";
    const auto linePath = Samples() / "100007_m.fxe";
    if (!std::filesystem::exists(actorPath) || !std::filesystem::exists(linePath))
    {
        SKIP("no game/facefx/humanmale.fxa and 100007_m.fxe");
    }
    const auto actor = facefx::ReadActor(FromDisk(actorPath));
    const auto set = facefx::ReadAnimSet(FromDisk(linePath));
    REQUIRE(actor);
    REQUIRE(set);
    const auto &anim = set->groups[0].anims[0];
    bool moved = false;
    for (const float t : {-1.0f, 0.0f, 0.4f, anim.EndTime() / 2, anim.EndTime(), anim.EndTime() + 1})
    {
        const auto state = facefx::Evaluate(*actor, anim, t);
        REQUIRE(state.nodeValues.size() == actor->nodes.size());
        REQUIRE(state.bones.size() == actor->bones.size());
        for (std::size_t i = 0; i < state.nodeValues.size(); ++i)
        {
            CHECK(std::isfinite(state.nodeValues[i]));
            CHECK(state.nodeValues[i] >= actor->nodes[i].min);
            CHECK(state.nodeValues[i] <= actor->nodes[i].max);
        }
        for (const auto &bone : state.bones)
        {
            for (const float c : bone.rotation)
            {
                CHECK(std::isfinite(c));
            }
            for (const float c : bone.position)
            {
                CHECK(std::isfinite(c));
            }
            moved = moved || std::fabs(bone.rotationDelta[0] - 1.0f) > 1e-4f;
        }
        for (const float w : state.emotionWeights)
        {
            CHECK(std::isfinite(w));
        }
    }
    CHECK(moved);
}
