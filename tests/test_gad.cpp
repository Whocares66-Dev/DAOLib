// dao/core/Gad.h: a clip's root motion read, and sampled as the engine samples it (DAOLib #287).
#include "dao/core/Gad.h"
#include "dao/core/Gff4.h"
#include "dao/core/Install.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

using Catch::Approx;
namespace gff4 = dao::core::gff4;
namespace gad = dao::core::gad;
using dao::test::Builder;
using dao::test::Float;
using dao::test::Floats;
using dao::test::Start;
using dao::test::Struct;

namespace
{

// A quarter turn about z.
const gad::Quaternion kQuarter{0.0F, 0.0F, std::numbers::sqrt2_v<float> / 2, std::numbers::sqrt2_v<float> / 2};

// A list of floats at the data's end: its data offset.
std::uint32_t FloatList(Builder &b, const std::vector<float> &values)
{
    const std::size_t at = b.bytes.size();
    b.U32(at, static_cast<std::uint32_t>(values.size()));
    Floats(b, at + 4, values);
    return static_cast<std::uint32_t>(at - b.data);
}

// A .gad laid out as mh_m.mov_wf_p_lfoot.gad is (its templates read 2026-10-31), "step", 1 s long,
// with its root motion: a translation of three keys, 0.5 m forward and 0.5 m more, and a rotation
// of two, none and a quarter turn about z. The translation's list ends in a part key.
std::vector<std::byte> GadFile(bool hasGobAnim)
{
    Builder b = Start(
        "GAD ", {{"gad ",
                  20,
                  {{4005, 1, gff4::kList | gff4::kStruct, 12},
                   {4007, 14, 0, 0},
                   {4008, 0, 0, 16},
                   {4009, 8, 0, 4},
                   {4010, 8, 0, 8}}},
                 {"node",
                  20,
                  {{4000, 14, 0, 0}, {4001, 4, 0, 4}, {4002, 0, 0, 16}, {4003, 4, 0, 8}, {4004, 8, gff4::kList, 12}}}});
    const std::size_t root = Struct(b, 20);
    b.U32(root, b.String(u"step"));
    Float(b, root + 4, 1.0F);
    b.U8(root + 16, hasGobAnim ? 1 : 0);
    const std::size_t nodes = b.bytes.size();
    b.U32(nodes, 2);
    const std::size_t translation = Struct(b, 20);
    const std::size_t rotation = Struct(b, 20);
    b.U32(root + 12, static_cast<std::uint32_t>(nodes - b.data));

    b.U32(translation, b.String(u"gob_gobtranslation"));
    b.U32(translation + 4, 5);
    b.U32(translation + 8, 3);
    b.U32(translation + 12,
          FloatList(b, {0.0F, 0.0F, 0.0F, 0.0F, 0.5F, 0.0F, 0.5F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 2.0F, 9.0F}));
    b.U32(rotation, b.String(u"gob_gobrotation"));
    b.U32(rotation + 4, 1);
    b.U32(rotation + 8, 4);
    b.U32(rotation + 12,
          FloatList(b, {0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, kQuarter[0], kQuarter[1], kQuarter[2], kQuarter[3]}));
    return b.bytes;
}

std::optional<gad::Gad> Read(const std::vector<std::byte> &bytes)
{
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    return gad::GadOf(*file);
}

// The angle a rotation turns by, in degrees.
float Angle(const gad::Quaternion &q)
{
    return 2.0F * std::acos(std::abs(q[3])) * 180.0F / std::numbers::pi_v<float>;
}

} // namespace

TEST_CASE("a .gad's root motion is read: its name, length, flag and both nodes' keys", "[gad]")
{
    const auto read = Read(GadFile(true));
    REQUIRE(read.has_value());
    CHECK(read->name == L"step");
    CHECK(read->length == Approx(1.0F));
    CHECK(read->hasGobAnim);
    // Four floats a key: the part key at the end is left out, as the engine counts them.
    REQUIRE(read->translation.size() == 3);
    CHECK(read->translation[1].time == Approx(0.5F));
    CHECK(read->translation[1].position == gad::Vector3{0.0F, 0.5F, 0.0F});
    REQUIRE(read->rotation.size() == 2);
    CHECK(read->rotation[1].time == Approx(1.0F));
    CHECK(read->rotation[1].orientation == kQuarter);

    const auto without = Read(GadFile(false));
    REQUIRE(without.has_value());
    CHECK(!without->hasGobAnim);
}

TEST_CASE("a file that is not a .gad is not read as one", "[gad]")
{
    Builder b = Start("ANI ", {{"anim", 4, {{4009, 8, 0, 0}}}});
    Struct(b, 4);
    const std::optional<gff4::File> file = gff4::File::Parse(b.bytes);
    REQUIRE(file.has_value());
    CHECK(!gad::GadOf(*file).has_value());
}

TEST_CASE("a .gad's travel is the keys either side blended, the first before them and the last after", "[gad]")
{
    const auto read = Read(GadFile(true));
    REQUIRE(read.has_value());
    CHECK(gad::PositionAt(read->translation, -1.0F) == gad::Vector3{0.0F, 0.0F, 0.0F});
    const auto between = gad::PositionAt(read->translation, 0.75F);
    REQUIRE(between.has_value());
    CHECK(between->at(1) == Approx(0.75F));
    CHECK(gad::PositionAt(read->translation, 5.0F) == gad::Vector3{0.0F, 1.0F, 0.0F});
    CHECK(!gad::PositionAt({}, 0.0F).has_value());
}

// The engine slerps (0x004CBAA0): a quarter of the way to a quarter turn is a sixteenth of a turn,
// where a blend of the parts normalised would turn about 19.9 degrees.
TEST_CASE("a .gad's turn is slerped the shorter way round, as the engine slerps", "[gad]")
{
    const auto read = Read(GadFile(true));
    REQUIRE(read.has_value());
    const auto quarter = gad::OrientationAt(read->rotation, 0.25F);
    REQUIRE(quarter.has_value());
    CHECK(Angle(*quarter) == Approx(22.5F).margin(1e-3));
    CHECK(gad::OrientationAt(read->rotation, 2.0F) == kQuarter);

    // The same turn written as -q: the blend goes the shorter way, to the same rotation.
    std::vector<gad::OrientationKey> flipped = read->rotation;
    for (float &part : flipped[1].orientation)
    {
        part = -part;
    }
    const auto half = gad::OrientationAt(flipped, 0.5F);
    REQUIRE(half.has_value());
    CHECK(Angle(*half) == Approx(45.0F).margin(1e-3));
    CHECK(std::abs(half->at(2)) == Approx(std::sin(std::numbers::pi_v<float> / 8)).margin(1e-5));
    CHECK(half->at(2) * half->at(3) > 0.0F); // about +z, as the keys turn

    // Keys all but the same are blended straight (the slerp's 1e-6).
    const std::vector<gad::OrientationKey> same{{0.0F, {0.0F, 0.0F, 0.0F, 1.0F}}, {1.0F, {0.0F, 0.0F, 0.0F, 1.0F}}};
    CHECK(gad::OrientationAt(same, 0.5F) == gad::Quaternion{0.0F, 0.0F, 0.0F, 1.0F});
}

// Quaternion::Yaw (0x004EE310): the forward axis, +y, turned a quarter about z faces -x, 90 degrees.
TEST_CASE("a rotation's yaw is the forward axis's heading in degrees, as the engine reckons it", "[gad]")
{
    CHECK(gad::YawDegrees({0.0F, 0.0F, 0.0F, 1.0F}) == Approx(0.0F).margin(1e-5));
    CHECK(gad::YawDegrees(kQuarter) == Approx(90.0F));
    CHECK(gad::YawDegrees({0.0F, 0.0F, -kQuarter[2], kQuarter[3]}) == Approx(-90.0F));
}

TEST_CASE("a .gad's summary is its length, flag, travel and turn from the first key to the last", "[gad]")
{
    const auto read = Read(GadFile(true));
    REQUIRE(read.has_value());
    const gad::Summary summary = gad::SummaryOf(*read);
    CHECK(summary.length == Approx(1.0F));
    CHECK(summary.hasGobAnim);
    CHECK(summary.translation == gad::Vector3{0.0F, 1.0F, 0.0F});
    CHECK(summary.yawDegrees == Approx(90.0F));

    // From a first key already turned: the turn between the two, not the last key's own.
    gad::Gad turned = *read;
    turned.rotation[0].orientation = kQuarter;
    turned.rotation[1].orientation = {0.0F, 0.0F, 1.0F, 0.0F}; // a half turn
    CHECK(gad::SummaryOf(turned).yawDegrees == Approx(90.0F));

    gad::Gad still;
    still.length = 2.0F;
    const gad::Summary none = gad::SummaryOf(still);
    CHECK(none.translation == gad::Vector3{0.0F, 0.0F, 0.0F});
    CHECK(none.yawDegrees == 0.0F);
}

// The GOG install's walk loop and walk-to-pause clips, as paragon-agent's reading has them
// (DAOParagonEngine #351): the pause carries a man 0.81 m forward over 1.03 s, and the loop has no
// root motion. Hidden, as it reads the install: dao_core_tests "[gad-real]".
TEST_CASE("on the GOG install, the walk-to-pause clip carries 0.81 m and the walk loop none", "[.][gad-real]")
{
    const std::filesystem::path game = L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins";
    if (!std::filesystem::exists(game / L"bin_ship"))
    {
        SKIP("no GOG install here");
    }
    const auto index = dao::core::install::Index::Build(game, {});
    const auto read = [&index](const std::string &name) {
        const auto *copies = index->Find(name);
        REQUIRE(copies != nullptr);
        REQUIRE(!copies->empty());
        const auto bytes = index->Read(copies->front());
        REQUIRE(bytes.has_value());
        const std::optional<gff4::File> file = gff4::File::Parse(*bytes);
        REQUIRE(file.has_value());
        auto clip = gad::GadOf(*file);
        REQUIRE(clip.has_value());
        return *clip;
    };

    const gad::Gad pause = read("mh_m.mov_wf_p_lfoot.gad");
    CHECK(pause.name == L"mov_wf_p_lfoot");
    CHECK(pause.hasGobAnim);
    CHECK(pause.length == Approx(1.0333F).margin(1e-3));
    CHECK(!pause.translation.empty());
    CHECK(!pause.rotation.empty());
    const gad::Summary summary = gad::SummaryOf(pause);
    CHECK(std::hypot(summary.translation[0], summary.translation[1], summary.translation[2]) ==
          Approx(0.81F).margin(0.01));
    CHECK(summary.translation[1] > 0.75F); // forward, along +y
    CHECK(summary.yawDegrees == Approx(0.0F).margin(1.0));

    const gad::Gad loop = read("mh_m.mov_wf.gad");
    CHECK(!loop.hasGobAnim);
    CHECK(loop.translation.empty());
    CHECK(loop.rotation.empty());

    // Every .gad the game takes: each reads, its keys in time order and within its length, its
    // rotations of length 1.
    std::size_t clips = 0;
    std::size_t moving = 0;
    std::size_t outOfOrder = 0;
    std::size_t notUnit = 0;
    for (const auto &[name, copies] : index->Resources())
    {
        if (!name.ends_with(".gad") || copies.empty() || !index->Readable(copies.front()))
        {
            continue;
        }
        const auto bytes = index->Read(copies.front());
        const std::optional<gff4::File> file = bytes ? gff4::File::Parse(*bytes) : std::nullopt;
        const auto clip = file ? gad::GadOf(*file) : std::nullopt;
        INFO(name);
        REQUIRE(clip.has_value());
        ++clips;
        moving += clip->hasGobAnim ? 1 : 0;
        for (std::size_t i = 1; i < clip->translation.size(); ++i)
        {
            outOfOrder += clip->translation[i].time < clip->translation[i - 1].time ? 1 : 0;
        }
        for (const gad::OrientationKey &key : clip->rotation)
        {
            const auto &[x, y, z, w] = key.orientation;
            notUnit += std::abs(std::sqrt((x * x) + (y * y) + (z * z) + (w * w)) - 1.0F) > 0.01F ? 1 : 0;
        }
    }
    INFO("clips " << clips << ", with root motion " << moving);
    CHECK(clips > 1000);
    CHECK(moving > 0);
    CHECK(outOfOrder == 0);
    CHECK(notUnit == 0);
}
