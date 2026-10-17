#include "dao/core/Clip.h"
#include "dao/core/Gff4.h"
#include "gff4_builder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <vector>

using Catch::Approx;
namespace gff4 = dao::core::gff4;
namespace clip = dao::core::clip;
using dao::test::Builder;
using dao::test::Float;
using dao::test::kGeneric;
using dao::test::Start;
using dao::test::Struct;

namespace
{

constexpr float kNear = 0.002F;

float Length(const clip::Quaternion &q)
{
    return std::sqrt((q[0] * q[0]) + (q[1] * q[1]) + (q[2] * q[2]) + (q[3] * q[3]));
}

// A rotation packed in 32 bits as the exe reads it: three 10-bit parts and the largest's place.
std::uint32_t Pack32(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t place)
{
    return (a << 22) | (b << 12) | (c << 2) | place;
}

// A clip as mov.walkf.ani is laid out (tools/gff4.py --templates), "walk", 2 s long: a channel
// "head_rotation" (target 2) with keys at 0 and the end, a quarter turn about z and none, and
// "root_translation" (target 6) with one key at (1, 2, 3).
std::vector<std::byte> ClipFile(std::uint32_t quarter, std::uint32_t none)
{
    Builder b =
        Start("ANI ", {{"anim",
                        20,
                        {{4005, 1, gff4::kList | gff4::kStruct, 12},
                         {4006, 14, 0, 0},
                         {4009, 8, 0, 8},
                         {4011, 0, 0, 17},
                         {4012, 0, 0, 18},
                         {4013, 4, 0, 4},
                         {4040, 0, 0, 16}}},
                       {"node", 16, {{4000, 14, 0, 0}, {4001, 0, 0, 12}, {4004, kGeneric, gff4::kReference, 4}}},
                       {"quat", 6, {{4035, 2, 0, 0}, {4036, 4, 0, 2}}},
                       {"list", 4, {{4004, 2, gff4::kList | gff4::kStruct, 0}}},
                       {"vec3", 14, {{4035, 2, 0, 0}, {4036, 8, 0, 2}, {4037, 8, 0, 6}, {4038, 8, 0, 10}}},
                       {"list", 4, {{4004, 4, gff4::kList | gff4::kStruct, 0}}}});
    const std::size_t root = Struct(b, 20);
    b.U32(root, b.String(u"walk"));
    Float(b, root + 8, 2.0F);
    b.U8(root + 17, 1); // additive
    const std::size_t nodes = b.bytes.size();
    b.U32(nodes, 2);
    const std::size_t head = Struct(b, 16);
    const std::size_t rootNode = Struct(b, 16);
    b.U32(root + 12, static_cast<std::uint32_t>(nodes - b.data));

    b.U32(head, b.String(u"head_rotation"));
    b.U8(head + 12, 2);
    const std::size_t headList = Struct(b, 4);
    b.U32(head + 4, 3U | (std::uint32_t{gff4::kStruct} << 16)); // the generic element: template 3, a struct
    b.U32(head + 8, static_cast<std::uint32_t>(headList - b.data));
    const std::size_t headKeys = b.bytes.size();
    b.U32(headKeys, 2);
    const std::size_t first = Struct(b, 6);
    const std::size_t last = Struct(b, 6);
    b.U32(headList, static_cast<std::uint32_t>(headKeys - b.data));
    b.U8(first + 2, static_cast<std::uint8_t>(quarter & 0xFF));
    b.U8(first + 3, static_cast<std::uint8_t>((quarter >> 8) & 0xFF));
    b.U8(first + 4, static_cast<std::uint8_t>((quarter >> 16) & 0xFF));
    b.U8(first + 5, static_cast<std::uint8_t>(quarter >> 24));
    b.U8(last, 0xFF);
    b.U8(last + 1, 0xFF);
    b.U8(last + 2, static_cast<std::uint8_t>(none & 0xFF));
    b.U8(last + 3, static_cast<std::uint8_t>((none >> 8) & 0xFF));
    b.U8(last + 4, static_cast<std::uint8_t>((none >> 16) & 0xFF));
    b.U8(last + 5, static_cast<std::uint8_t>(none >> 24));

    b.U32(rootNode, b.String(u"root_translation"));
    b.U8(rootNode + 12, 6);
    const std::size_t rootList = Struct(b, 4);
    b.U32(rootNode + 4, 5U | (std::uint32_t{gff4::kStruct} << 16));
    b.U32(rootNode + 8, static_cast<std::uint32_t>(rootList - b.data));
    const std::size_t rootKeys = b.bytes.size();
    b.U32(rootKeys, 1);
    const std::size_t key = Struct(b, 14);
    b.U32(rootList, static_cast<std::uint32_t>(rootKeys - b.data));
    Float(b, key + 2, 1.0F);
    Float(b, key + 6, 2.0F);
    Float(b, key + 10, 3.0F);
    return b.bytes;
}

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());
    std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

} // namespace

TEST_CASE("a 32-bit rotation is three 10-bit parts and the largest at the place its low bits say", "[clip]")
{
    // The parts' middle is 511.5: 512 is the least step past it, so the largest is all but 1.
    const clip::Quaternion none = clip::Rotation32(Pack32(512, 512, 512, 3));
    CHECK(none[0] == Approx(0.0F).margin(kNear));
    CHECK(none[3] == Approx(1.0F).margin(kNear));
    const clip::Quaternion first = clip::Rotation32(Pack32(512, 512, 512, 0));
    CHECK(first[0] == Approx(1.0F).margin(kNear));
    // A quarter turn about z: z and w both sqrt(1/2), z the largest's place 2, w the third part.
    // sqrt(1/2) is (v - 511.5) * 2 / (sqrt 2 * 1023): v = 511.5 + 511.5 = 1023.
    const clip::Quaternion quarter = clip::Rotation32(Pack32(512, 512, 1023, 2));
    CHECK(quarter[2] == Approx(0.7071F).margin(kNear));
    CHECK(quarter[3] == Approx(0.7071F).margin(kNear));
    CHECK(Length(quarter) == Approx(1.0F).margin(kNear));
}

TEST_CASE("a 48-bit rotation's place is its first two numbers' low bits", "[clip]")
{
    // Place 1 (first's low bit 0, second's 1): the largest goes second.
    const clip::Quaternion q = clip::Rotation48(32768, 32768 | 1, 32768);
    CHECK(q[1] == Approx(1.0F).margin(kNear));
    CHECK(q[0] == Approx(0.0F).margin(kNear));
}

TEST_CASE("a 64-bit rotation is 21, 21 and 20 bits and the low word's place", "[clip]")
{
    // DAOParagonEngine's sample, a key of c_beara: the three parts at their middles, the place 1.
    const clip::Quaternion q = clip::Rotation64(0x80000400001FFFFDULL);
    CHECK(q[0] == Approx(0.0F).margin(kNear));
    CHECK(q[1] == Approx(1.0F).margin(kNear));
    CHECK(q[2] == Approx(0.0F).margin(kNear));
    CHECK(q[3] == Approx(0.0F).margin(kNear));
}

TEST_CASE("a clip has its channels, each its node, what it drives and keys in seconds", "[clip]")
{
    const std::vector<std::byte> bytes = ClipFile(Pack32(512, 512, 1023, 2), Pack32(512, 512, 512, 3));
    const std::optional<gff4::File> file = gff4::File::Parse(bytes);
    REQUIRE(file.has_value());
    const std::optional<clip::Clip> read = clip::ClipOf(*file);
    REQUIRE(read.has_value());
    CHECK(read->name == L"walk");
    CHECK(read->length == 2.0F);
    CHECK(read->additive);
    CHECK(!read->isOverride);
    REQUIRE(read->channels.size() == 2);

    const clip::Channel &head = read->channels[0];
    CHECK(head.node == L"head");
    CHECK(head.what == L"rotation");
    CHECK(head.IsRotation());
    REQUIRE(head.keys.size() == 2);
    CHECK(head.keys[0].time == 0.0F);
    CHECK(head.keys[0].value[2] == Approx(0.7071F).margin(kNear));
    CHECK(head.keys[1].time == Approx(2.0F)); // 65535 is the clip's end
    CHECK(head.keys[1].value[3] == Approx(1.0F).margin(kNear));

    const clip::Channel &translation = read->channels[1];
    CHECK(translation.node == L"root");
    CHECK(translation.what == L"translation");
    CHECK(!translation.IsRotation());
    REQUIRE(translation.keys.size() == 1);
    CHECK(translation.keys[0].value == std::array<float, 4>{1, 2, 3, 0});
}

TEST_CASE("a file that is no clip is none", "[clip]")
{
    Builder b = Start("MMH ", {{"mdlh", 4, {{6000, 14, 0, 0}}}});
    Struct(b, 4);
    const std::optional<gff4::File> file = gff4::File::Parse(b.bytes);
    REQUIRE(file.has_value());
    CHECK(!clip::ClipOf(*file).has_value());
}

TEST_CASE("the game's clips in game/animations, when there, read whole, every rotation of length 1", "[clip]")
{
    const std::filesystem::path folder =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "animations";
    if (!std::filesystem::exists(folder))
    {
        SKIP("no game/animations in this checkout (ghilana's extract of *.ani writes them)");
    }
    std::size_t clips = 0;
    std::size_t notUnit = 0;
    std::size_t endless = 0; // channels whose last key is not at the clip's end
    std::map<std::uint8_t, std::size_t> targets;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".ani")
        {
            continue;
        }
        const std::vector<std::byte> bytes = FromDisk(entry.path());
        const std::optional<gff4::File> file = gff4::File::Parse(bytes);
        INFO(entry.path().filename().string());
        REQUIRE(file.has_value());
        const std::optional<clip::Clip> read = clip::ClipOf(*file);
        REQUIRE(read.has_value());
        ++clips;
        for (const clip::Channel &channel : read->channels)
        {
            ++targets[channel.target];
            CHECK(!channel.keys.empty());
            endless += !channel.keys.empty() && channel.keys.back().time != read->length ? 1 : 0;
            for (const clip::Key &key : channel.keys)
            {
                notUnit += channel.IsRotation() && std::abs(Length(key.value) - 1.0F) > 0.01F ? 1 : 0;
            }
        }
    }
    CHECK(clips > 0);
    CHECK(notUnit == 0);
    CHECK(endless == 0);
    // Every clip the game takes (DAOLib #157): the counts tools/gff4.py read in them.
    if (clips == 4518)
    {
        CHECK(targets[2] == 100114);
        CHECK(targets[3] == 2212);
        CHECK(targets[4] == 194365);
        CHECK(targets[6] == 38988);
        CHECK(targets[8] == 5420);
        CHECK(targets[10] == 1655);
    }
}
