// FMOD's sample banks (dao/core/Fsb.h): banks made in the test as the game's
// are laid out, IMA ADPCM blocks decoded to values worked by hand from IMA's
// tables, and, where they are, banks of the game's.
#include "dao/core/Fsb.h"
#include "files_builder.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace fsb = dao::core::fsb;
namespace files = dao::test::files;
using files::Bytes;

namespace
{

struct Made
{
    Made(std::string n, std::uint32_t f, std::uint32_t m = fsb::mode::kImaAdpcm | fsb::mode::kMono | fsb::mode::k3D,
         std::uint16_t c = 1, Bytes d = {}, std::vector<std::uint32_t> s = {})
        : name(std::move(n)), frames(f), mode(m), channels(c), data(std::move(d)), sync(std::move(s))
    {
    }

    std::string name;
    std::uint32_t frames;
    std::uint32_t mode;
    std::uint16_t channels;
    Bytes data;
    std::vector<std::uint32_t> sync;
};

void Put16(Bytes &out, std::size_t at, std::uint16_t value)
{
    std::memcpy(out.data() + at, &value, 2);
}

void PutFloat(Bytes &out, std::size_t at, float value)
{
    std::memcpy(out.data() + at, &value, 4);
}

// A sample's header: 80 bytes, and its sync points after them.
Bytes SampleHeader(const Made &made, std::uint32_t size)
{
    Bytes out(fsb::kSampleHeaderSize);
    if (!made.sync.empty())
    {
        files::AppendText(out, "SYNC");
        files::Append32(out, static_cast<std::uint32_t>(made.sync.size()));
        for (const auto point : made.sync)
        {
            files::Append32(out, point);
        }
    }
    Put16(out, 0, static_cast<std::uint16_t>(out.size()));
    std::memcpy(out.data() + 2, made.name.data(), std::min<std::size_t>(made.name.size(), 30));
    files::Put32(out, 32, made.frames);
    files::Put32(out, 36, size);
    files::Put32(out, 40, 0);
    files::Put32(out, 44, made.frames - 1);
    files::Put32(out, 48, made.mode);
    files::Put32(out, 52, 22050);
    Put16(out, 56, 255);
    Put16(out, 58, 128);
    Put16(out, 60, 128);
    Put16(out, 62, made.channels);
    PutFloat(out, 64, 1.0F);
    PutFloat(out, 68, 10000.0F);
    return out;
}

// A bank as the game's: the samples' headers padded so the data starts at a 32-byte boundary,
// each sample's data padded to 32 bytes.
Bytes Bank(const std::vector<Made> &samples, std::uint32_t version = 0x00040000, std::uint32_t mode = 0x20)
{
    Bytes headers;
    Bytes data;
    for (const auto &made : samples)
    {
        Bytes padded = made.data;
        padded.resize((padded.size() + 31) / 32 * 32);
        const Bytes header = SampleHeader(made, static_cast<std::uint32_t>(padded.size()));
        headers.insert(headers.end(), header.begin(), header.end());
        data.insert(data.end(), padded.begin(), padded.end());
    }
    headers.resize(((fsb::kHeaderSize + headers.size() + 31) / 32 * 32) - fsb::kHeaderSize);
    Bytes out = files::FromText("FSB4");
    files::Append32(out, static_cast<std::uint32_t>(samples.size()));
    files::Append32(out, static_cast<std::uint32_t>(headers.size()));
    files::Append32(out, static_cast<std::uint32_t>(data.size()));
    files::Append32(out, version);
    files::Append32(out, mode);
    out.resize(fsb::kHeaderSize);
    out[32] = std::byte{0xAB};
    out[47] = std::byte{0xCD};
    out.insert(out.end(), headers.begin(), headers.end());
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

Bytes Of(std::initializer_list<int> values)
{
    Bytes out;
    for (const int v : values)
    {
        out.push_back(static_cast<std::byte>(v));
    }
    return out;
}

// A channel's block header: the first sample and the step index.
Bytes ImaHeader(std::int16_t sample, std::uint8_t index)
{
    Bytes out(4);
    std::memcpy(out.data(), &sample, 2);
    out[2] = std::byte{index};
    return out;
}

// A mono block: the header, then the nibbles 7, 7, F, 0, 3 and zeros, the last nibble F.
Bytes MonoBlock(std::int16_t sample, std::uint8_t index)
{
    Bytes block = ImaHeader(sample, index);
    const Bytes nibbles = Of({0x77, 0x0F, 0x03});
    block.insert(block.end(), nibbles.begin(), nibbles.end());
    block.resize(36);
    block[35] = std::byte{0xF0};
    return block;
}

std::vector<std::byte> FromDisk(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());
    std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

// The mean step between frames within IMA blocks, and from a block's last frame to the next
// block's first, of one channel.
std::pair<double, double> Steps(const std::vector<std::int16_t> &pcm, std::uint16_t channels, std::uint16_t c)
{
    double within = 0;
    double across = 0;
    std::size_t withins = 0;
    std::size_t acrosses = 0;
    const std::size_t frames = pcm.size() / channels;
    for (std::size_t f = 1; f < frames; ++f)
    {
        const double step = std::abs(pcm[(f * channels) + c] - pcm[((f - 1) * channels) + c]);
        if (f % 64 == 0)
        {
            across += step;
            ++acrosses;
        }
        else
        {
            within += step;
            ++withins;
        }
    }
    return {within / static_cast<double>(std::max<std::size_t>(withins, 1)),
            across / static_cast<double>(std::max<std::size_t>(acrosses, 1))};
}

} // namespace

TEST_CASE("a bank's header and each sample's fields, its data after the padded headers", "[fsb]")
{
    const Bytes bytes = Bank(
        {{"gui_click.wav", 128, fsb::mode::kImaAdpcm | fsb::mode::kMono | fsb::mode::k2D, 1, Bytes(72, std::byte{1})},
         {"36451_m.wav", 1152, fsb::mode::kMpeg | fsb::mode::kMono | fsb::mode::k3D, 1, Bytes(100, std::byte{2}), {}}});
    const auto bank = fsb::Bank::Parse(bytes);
    REQUIRE(bank);
    CHECK(bank->Head().samples == 2);
    CHECK(bank->Head().headersSize == 176); // 160, padded 16 so the data is at 224
    CHECK(bank->Head().dataSize == 96 + 128);
    CHECK(bank->Head().version == 0x00040000);
    CHECK(bank->Head().mode == fsb::bank_mode::kMpegPadded);
    CHECK(bank->Head().hash[0] == 0xAB);
    CHECK(bank->Head().hash[15] == 0xCD);
    REQUIRE(bank->Samples().size() == 2);
    const auto &gui = bank->Samples()[0];
    CHECK(gui.name == "gui_click.wav");
    CHECK(gui.frames == 128);
    CHECK(gui.size == 96);
    CHECK(gui.loopStart == 0);
    CHECK(gui.loopEnd == 127);
    CHECK(gui.rate == 22050);
    CHECK(gui.volume == 255);
    CHECK(gui.pan == 128);
    CHECK(gui.priority == 128);
    CHECK(gui.channels == 1);
    CHECK(gui.minDistance == 1.0F);
    CHECK(gui.maxDistance == 10000.0F);
    CHECK(gui.codec == fsb::Codec::ImaAdpcm);
    CHECK(gui.syncPoints.empty());
    CHECK(gui.offset == 224);
    const auto &voice = bank->Samples()[1];
    CHECK(voice.codec == fsb::Codec::Mpeg);
    CHECK(voice.offset == 224 + 96);
    CHECK(voice.size == 128);
    const auto voiceBytes = fsb::Bank::BytesOf(bytes, voice);
    REQUIRE(voiceBytes.size() == 128);
    CHECK(voiceBytes[99] == std::byte{2});
    CHECK(voiceBytes[100] == std::byte{0});
    CHECK(fsb::Bank::BytesOf(std::span(bytes).first(bytes.size() - 1), voice).empty());
}

TEST_CASE("a sample's sync points follow its 80 bytes, a count and the offsets", "[fsb]")
{
    Made music{"DA_m_theme.wav", 4096,
               fsb::mode::kMpeg | fsb::mode::kStereo | fsb::mode::k2D | fsb::mode::kSyncPoints |
                   fsb::mode::kSyncPointsNoNames};
    music.channels = 2;
    music.data = Bytes(64);
    music.sync = {0, 1000, 3000};
    const Bytes bytes = Bank({music, {"after.wav", 64, fsb::mode::kImaAdpcm, 1, Bytes(36)}});
    const auto bank = fsb::Bank::Parse(bytes);
    REQUIRE(bank);
    CHECK(bank->Samples()[0].syncPoints == std::vector<std::uint32_t>{0, 1000, 3000});
    CHECK(bank->Samples()[0].channels == 2);
    CHECK(bank->Samples()[1].name == "after.wav"); // the next header after the 100 bytes
    CHECK(bank->Samples()[1].syncPoints.empty());

    // A count past the header's own size is a damaged bank.
    Bytes damaged = bytes;
    files::Put32(damaged, fsb::kHeaderSize + 84, 4);
    CHECK_FALSE(fsb::Bank::Parse(damaged));
}

TEST_CASE("a sample found by its name in any case, with or without its extension", "[fsb]")
{
    const Bytes bytes = Bank({{"goat_pain_1.p.wav", 64, fsb::mode::kMpeg, 1, Bytes(32)},
                              {"36451_m.wav", 64, fsb::mode::kMpeg, 1, Bytes(32)}});
    const auto bank = fsb::Bank::Parse(bytes);
    REQUIRE(bank);
    CHECK(bank->Find("36451_M") == &bank->Samples()[1]);
    CHECK(bank->Find("36451_m.WAV") == &bank->Samples()[1]);
    CHECK(bank->Find("goat_pain_1.p") == bank->Samples().data());
    CHECK(bank->Find("goat_pain_1") == nullptr);
    CHECK(bank->Find("36451") == nullptr);
}

TEST_CASE("a bank opened from its file reads its headers, and a sample's bytes when asked", "[fsb]")
{
    Bytes data(70);
    data[0] = std::byte{9};
    data[69] = std::byte{8};
    const Bytes bytes =
        Bank({{"a.wav", 64, fsb::mode::kImaAdpcm, 1, Bytes(36)}, {"b.wav", 128, fsb::mode::kMpeg, 1, data}});
    const files::TempFolder folder;
    folder.Write(L"b.fsb", bytes);
    const auto bank = fsb::Bank::Open(folder.path / L"b.fsb");
    REQUIRE(bank);
    REQUIRE(bank->Samples().size() == 2);
    const auto read = bank->Read(*bank->Find("b"));
    REQUIRE(read);
    REQUIRE(read->size() == 96);
    CHECK((*read)[0] == std::byte{9});
    CHECK((*read)[69] == std::byte{8});
    CHECK_FALSE(fsb::Bank::Parse(bytes)->Read(bank->Samples()[0])); // no file to read from

    // A file cut short of its last sample's data is no bank.
    folder.Write(L"short.fsb", Bytes(bytes.begin(), bytes.end() - 1));
    CHECK_FALSE(fsb::Bank::Open(folder.path / L"short.fsb"));
    CHECK_FALSE(fsb::Bank::Open(folder.path / L"none.fsb"));
}

TEST_CASE("what is no bank this reads: another format or version, basic or enciphered headers, damage", "[fsb]")
{
    const Bytes good = Bank({{"a.wav", 64, fsb::mode::kImaAdpcm, 1, Bytes(36)}});
    REQUIRE(fsb::Bank::Parse(good));
    Bytes fsb3 = good;
    fsb3[3] = std::byte{'3'};
    CHECK_FALSE(fsb::Bank::Parse(fsb3));
    CHECK_FALSE(fsb::Bank::Parse(Bank({{"a.wav", 64, fsb::mode::kImaAdpcm, 1, Bytes(36)}}, 0x00030001)));
    CHECK_FALSE(fsb::Bank::Parse(
        Bank({{"a.wav", 64, fsb::mode::kImaAdpcm, 1, Bytes(36)}}, 0x00040000, fsb::bank_mode::kBasicHeaders)));
    CHECK_FALSE(fsb::Bank::Parse(
        Bank({{"a.wav", 64, fsb::mode::kImaAdpcm, 1, Bytes(36)}}, 0x00040000, fsb::bank_mode::kEncrypted)));
    Bytes smallHeader = good;
    Put16(smallHeader, fsb::kHeaderSize, 79);
    CHECK_FALSE(fsb::Bank::Parse(smallHeader));
    Bytes moreSamples = good;
    files::Put32(moreSamples, 4, 2); // a second header past the table
    CHECK_FALSE(fsb::Bank::Parse(moreSamples));
    Bytes longTable = good;
    files::Put32(longTable, 8, static_cast<std::uint32_t>(good.size()));
    CHECK_FALSE(fsb::Bank::Parse(longTable));
    CHECK_FALSE(fsb::Bank::Parse(std::span(good).first(47)));
    // A name of all 30 bytes has no terminator.
    const auto full = fsb::Bank::Parse(Bank({{std::string(30, 'n'), 64, fsb::mode::kImaAdpcm, 1, Bytes(36)}}));
    REQUIRE(full);
    CHECK(full->Samples()[0].name == std::string(30, 'n'));
}

TEST_CASE("a codec by its mode bits", "[fsb]")
{
    CHECK(fsb::CodecOf(0x00500020) == fsb::Codec::ImaAdpcm);
    CHECK(fsb::CodecOf(0x20402040) == fsb::Codec::ImaAdpcm); // stereo
    CHECK(fsb::CodecOf(0x00100220) == fsb::Codec::Mpeg);
    CHECK(fsb::CodecOf(0x80006240) == fsb::Codec::Mpeg);
    CHECK(fsb::CodecOf(fsb::mode::kMpegLayer3) == fsb::Codec::Mpeg);
    CHECK(fsb::CodecOf(fsb::mode::k16Bits | fsb::mode::kMono) == fsb::Codec::Pcm16);
    CHECK(fsb::CodecOf(fsb::mode::k8Bits) == fsb::Codec::Pcm8);
    CHECK(fsb::CodecOf(fsb::mode::kXma) == fsb::Codec::Other);
}

// Worked by hand from IMA's tables, from 0 at step index 0 (a step of 7):
//   7: 7/8 + 7/4 + 7/2 + 7 = 0 + 1 + 3 + 7 = 11 -> 11, index 8 (16)
//   7: 2 + 4 + 8 + 16 = 30                     -> 41, index 16 (34)
//   F: 4 + 8 + 17 + 34 = 63, less              -> -22, index 24 (73)
//   0: 73/8 = 9                                -> -13, index 23 (66)
//   3: 8 + 16 + 33 = 57                        -> 44, index 22 (60)
//   0: 7 -> 51, index 21 (55); 0: 6 -> 57, index 20 (50); 0: 6 -> 63, index 19 (45); 0: 5 -> 68
TEST_CASE("an IMA block: its header the first frame, then 63 nibbles, the low first", "[fsb]")
{
    const auto pcm = fsb::DecodeIma(MonoBlock(0, 0), 64, 1);
    REQUIRE(pcm);
    REQUIRE(pcm->size() == 64);
    CHECK(std::vector<std::int16_t>(pcm->begin(), pcm->begin() + 10) ==
          std::vector<std::int16_t>{0, 11, 41, -22, -13, 44, 51, 57, 63, 68});

    // The block's last nibble is not played: the same block with it 0 decodes the same.
    Bytes other = MonoBlock(0, 0);
    other[35] = std::byte{0};
    CHECK(fsb::DecodeIma(other, 64, 1) == pcm);

    // Blocks one after another, each from its own header; the last cut to the frames.
    Bytes two = MonoBlock(0, 0);
    const Bytes second = MonoBlock(1000, 0);
    two.insert(two.end(), second.begin(), second.end());
    const auto both = fsb::DecodeIma(two, 64 + 3, 1);
    REQUIRE(both);
    REQUIRE(both->size() == 67);
    CHECK((*both)[63] == (*pcm)[63]);
    CHECK(std::vector<std::int16_t>(both->begin() + 64, both->end()) == std::vector<std::int16_t>{1000, 1011, 1041});
    CHECK_FALSE(fsb::DecodeIma(two, (64 * 2) + 1, 1)); // a third block is not there
}

// From 32000 at the last step index (32767): 7 adds 4095 + 8191 + 16383 + 32767 and clips at
// 32767, the index stays 88; F takes 61436 away, to -28669. A step index past the table's is its
// last.
TEST_CASE("an IMA block's samples clip to 16 bits, its step index to the table", "[fsb]")
{
    const auto pcm = fsb::DecodeIma(MonoBlock(32000, 88), 64, 1);
    REQUIRE(pcm);
    CHECK((*pcm)[1] == 32767);
    CHECK((*pcm)[2] == 32767);
    CHECK((*pcm)[3] == 32767 - 61436);
    CHECK(fsb::DecodeIma(MonoBlock(32000, 200), 64, 1) == pcm);
}

// Both channels' headers, then 4 bytes of the first's nibbles and 4 of the second's in turn.
TEST_CASE("a stereo IMA block: the channels' headers, then their nibbles four bytes each in turn", "[fsb]")
{
    Bytes block = ImaHeader(100, 0);
    const Bytes right = ImaHeader(-100, 0);
    block.insert(block.end(), right.begin(), right.end());
    const Bytes words = Of({0x77, 0x0F, 0x03, 0x00, /* right */ 0, 0, 0, 0, /* left */ 0, 0, 0, 0,
                            /* right */ 0x07});
    block.insert(block.end(), words.begin(), words.end());
    block.resize(72);
    const auto pcm = fsb::DecodeIma(block, 64, 2);
    REQUIRE(pcm);
    REQUIRE(pcm->size() == 128);
    std::vector<std::int16_t> left;
    std::vector<std::int16_t> rightFrames;
    for (std::size_t f = 0; f < 10; ++f)
    {
        left.push_back((*pcm)[2 * f]);
        rightFrames.push_back((*pcm)[(2 * f) + 1]);
    }
    CHECK(left == std::vector<std::int16_t>{100, 111, 141, 78, 87, 144, 151, 157, 163, 168});
    // Zeros keep -100 at a step of 7; its ninth nibble, the second word's first, is 7.
    CHECK(rightFrames == std::vector<std::int16_t>{-100, -100, -100, -100, -100, -100, -100, -100, -100, -89});
    CHECK_FALSE(fsb::DecodeIma(block, 64, 3));
    CHECK_FALSE(fsb::DecodeIma(block, 64, 0));
    CHECK_FALSE(fsb::DecodeIma(std::span(block).first(71), 64, 2));
}

// Copies of the game's banks in game/audio (gui.fsb and glo_fly_mv.fsb of packages\core\audio\sound,
// ss_goat.fsb of its vo\en-us\vo), and two read in place in the GOG install for their headers:
// global_amb_glo.fsb and music_bank_strm_new.fsb, the one with sync points. The counts are the
// survey's of 2026-10-18 (dev/AUDIO.md).
TEST_CASE("the game's banks, where they are: their samples, and IMA decoded continuous across blocks", "[fsb][game]")
{
    const auto folder = std::filesystem::path(__FILE__).parent_path().parent_path() / "game" / "audio";
    if (!std::filesystem::is_directory(folder))
    {
        SKIP("no game/audio (copy gui.fsb, glo_fly_mv.fsb and ss_goat.fsb there from packages\\core\\audio)");
    }
    const auto gui = FromDisk(folder / "gui.fsb");
    const auto bank = fsb::Bank::Parse(gui);
    REQUIRE(bank);
    CHECK(bank->Head().samples == 101);
    CHECK(bank->Head().headersSize == 8080);
    CHECK(bank->Head().dataSize == 2322304);
    const auto *mute = bank->Find("gui_mute_hit2");
    REQUIRE(mute == bank->Samples().data());
    CHECK(mute->frames == 38656);
    CHECK(mute->size == 21760);
    CHECK(mute->rate == 4000);
    CHECK(mute->mode == 0x402020);
    std::size_t stereo = 0;
    for (const auto &sample : bank->Samples())
    {
        INFO(sample.name);
        REQUIRE(sample.codec == fsb::Codec::ImaAdpcm);
        const auto pcm = fsb::DecodeIma(fsb::Bank::BytesOf(gui, sample), sample.frames, sample.channels);
        REQUIRE(pcm);
        stereo += sample.channels == 2 ? 1 : 0;
        if (sample.frames >= 64 * 50)
        {
            for (std::uint16_t c = 0; c < sample.channels; ++c)
            {
                // A frame to the next block's is a step as any other, not a jump; the channels
                // taken in another order would give steps of thousands.
                const auto [within, across] = Steps(*pcm, sample.channels, c);
                CHECK(across < (within * 1.5) + 8);
            }
        }
    }
    CHECK(stereo == 1); // shale_pp.wav

    const auto moves = fsb::Bank::Open(folder / "glo_fly_mv.fsb");
    REQUIRE(moves);
    CHECK(moves->Samples().size() == 288);
    const auto *kneel = moves->Find("kneel_no_armor.wav");
    REQUIRE(kneel != nullptr);
    CHECK(kneel->channels == 2);
    CHECK(kneel->mode == 0x20402040);
    const auto kneelBytes = moves->Read(*kneel);
    REQUIRE(kneelBytes);
    const auto kneelPcm = fsb::DecodeIma(*kneelBytes, kneel->frames, 2);
    REQUIRE(kneelPcm);
    for (std::uint16_t c = 0; c < 2; ++c)
    {
        const auto [within, across] = Steps(*kneelPcm, 2, c);
        CHECK(across < (within * 1.5) + 8);
    }

    const auto goat = fsb::Bank::Open(folder / "ss_goat.fsb");
    REQUIRE(goat);
    CHECK(goat->Samples().size() == 8);
    const auto &pain = goat->Samples()[0];
    CHECK(pain.name == "goat_pain_1.p.wav");
    CHECK(pain.codec == fsb::Codec::Mpeg);
    CHECK(pain.rate == 24000);
    CHECK(pain.frames == 14976);
    const auto mp3 = goat->Read(pain);
    REQUIRE(mp3);
    CHECK(std::to_integer<int>((*mp3)[0]) == 0xFF); // an MPEG frame's sync
    CHECK((std::to_integer<int>((*mp3)[1]) & 0xE0) == 0xE0);

    const std::filesystem::path sound =
        L"C:/Program Files (x86)/GOG Galaxy/Games/Dragon Age Origins/packages/core/audio/sound";
    if (!std::filesystem::is_directory(sound))
    {
        SKIP("no GOG install here for the large banks");
    }
    const auto ambience = fsb::Bank::Open(sound / "global_amb_glo.fsb");
    REQUIRE(ambience);
    CHECK(ambience->Samples().size() == 884);
    CHECK(ambience->Head().dataSize == 68715424);
    const auto music = fsb::Bank::Open(sound / "music_bank_strm_new.fsb");
    REQUIRE(music);
    REQUIRE(music->Samples().size() == 240);
    std::size_t withPoints = 0;
    std::size_t points = 0;
    for (const auto &sample : music->Samples())
    {
        INFO(sample.name);
        withPoints += sample.syncPoints.empty() ? 0 : 1;
        points += sample.syncPoints.size();
        CHECK(std::ranges::is_sorted(sample.syncPoints));
        CHECK((sample.syncPoints.empty() || sample.syncPoints.back() < sample.frames));
    }
    CHECK(withPoints == 150);
    CHECK(points == 3442);
    CHECK(music->Samples()[0].syncPoints.size() == 19);
    CHECK(music->Samples()[0].syncPoints[1] == 210720);
}
