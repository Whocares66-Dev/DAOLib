// Notes on the banks, read 2026-10-18 over the 9,052 of the GOG install
// (DAOLib #191; dev/AUDIO.md has the format in full):
//
// The bank's header is 48 bytes: "FSB4", the sample count, the size of the
// samples' headers, the size of the data, the version (0x00040000 in all),
// the header mode (0x20 in all: MPEG frames padded), 8 zero bytes and a
// 16-byte hash. The samples' headers follow, each its own size first, and
// the data after them at 48 + their size, which 4,465 banks pad by 16 bytes
// past the last header so the data starts at a 32-byte boundary. Each
// sample's data follows the one before it, its size a multiple of 32: the
// data's size is the sum of them in every bank.
//
// A sample's header is 80 bytes, but 245 of 366,244 are larger, 92 to 284:
// the sync points, "SYNC", a count and that many u32 offsets, so 88 + 4 a
// point. All of them are in music_bank_strm_new.fsb (the core's and the
// Darkspawn Chronicles'), each sample with the mode bits 0x80000000 and
// 0x4000 (sync points, without names), the offsets ascending from 0 and each
// below the sample's length: frames, at a point every four seconds or so of
// the music.
//
// IMA ADPCM, 16,839 samples: a block of 36 bytes a channel, a header of an
// i16 sample, a u8 step index and a byte unused, then 32 bytes of nibbles,
// the low nibble first. The header's sample is the block's first, and 63
// nibbles give the rest: 64 frames a block, the last nibble not played. Read
// by continuity over the ambience of global_amb_fade.fsb: the mean step from
// the last frame of a block so decoded to the next block's header sample is
// the mean step within the blocks (264 to 261, 288 to 283, 99.3 to 99.1),
// where with the header as history alone and 64 nibbles played it is twice
// that (502, 531, 184). vgmstream's decode_xbox_ima does the same ("even
// samples per block, skips last nibble"). A stereo sample (its mode has
// 0x20000000 as well as 0x40, 45 of them) has blocks of 72 bytes: both
// channels' headers, then 4 bytes of the first channel's nibbles and 4 of
// the second's in turn. Read the same way over gui.fsb's shale_pp.wav,
// glo_fly_mv.fsb's kneel_no_armor.wav and three of vfx_spells.fsb's and
// glo_fly_plc.fsb's: the steps across blocks are those within them (429 to
// 427, 18.2 to 19.0), where the two channels' blocks one after the other
// give steps of thousands and step indices past the table's end. A sample's
// stored size rounds its blocks up to 32 bytes.
#include "dao/core/Fsb.h"

#include "dao/core/Bytes.h"

#include <algorithm>
#include <fstream>

namespace dao::core::fsb
{

namespace
{

constexpr std::uint32_t kVersion = 0x00040000;
constexpr std::size_t kNameSize = 30;
constexpr std::size_t kSyncHeader = 8; // "SYNC" and the count
constexpr std::size_t kImaBlock = 36;  // a channel's block
constexpr std::size_t kImaHeader = 4;
constexpr std::uint32_t kImaFrames = 64; // a block's frames

std::string Lower(std::string_view text)
{
    std::string out(text);
    for (char &c : out)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

template <class T> T At(std::span<const std::byte> bytes, std::size_t offset)
{
    return ReadAt<T>(bytes, offset).value_or(T{});
}

// A sample's sync points, from what its header has past the 80 bytes; nothing if they do not read.
std::optional<std::vector<std::uint32_t>> SyncPoints(std::span<const std::byte> extra)
{
    if (extra.empty())
    {
        return std::vector<std::uint32_t>{};
    }
    constexpr std::uint32_t kSync = 0x434E5953; // "SYNC"
    if (extra.size() < kSyncHeader || At<std::uint32_t>(extra, 0) != kSync)
    {
        return std::vector<std::uint32_t>{}; // something else, which no bank of the game's has
    }
    const auto count = At<std::uint32_t>(extra, 4);
    if (count > (extra.size() - kSyncHeader) / 4)
    {
        return std::nullopt;
    }
    std::vector<std::uint32_t> points(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        points[i] = At<std::uint32_t>(extra, kSyncHeader + (4 * std::size_t{i}));
    }
    return points;
}

std::optional<Sample> SampleOf(std::span<const std::byte> header)
{
    Sample s;
    const auto *name = reinterpret_cast<const char *>(header.data() + 2);
    s.name.assign(name, std::find(name, name + kNameSize, '\0'));
    s.frames = At<std::uint32_t>(header, 32);
    s.size = At<std::uint32_t>(header, 36);
    s.loopStart = At<std::uint32_t>(header, 40);
    s.loopEnd = At<std::uint32_t>(header, 44);
    s.mode = At<std::uint32_t>(header, 48);
    s.rate = At<std::int32_t>(header, 52);
    s.volume = At<std::uint16_t>(header, 56);
    s.pan = At<std::int16_t>(header, 58);
    s.priority = At<std::uint16_t>(header, 60);
    s.channels = At<std::uint16_t>(header, 62);
    s.minDistance = At<float>(header, 64);
    s.maxDistance = At<float>(header, 68);
    s.varyRate = At<std::int32_t>(header, 72);
    s.varyVolume = At<std::uint16_t>(header, 76);
    s.varyPan = At<std::int16_t>(header, 78);
    s.codec = CodecOf(s.mode);
    auto points = SyncPoints(header.subspan(kSampleHeaderSize));
    if (!points)
    {
        return std::nullopt;
    }
    s.syncPoints = std::move(*points);
    return s;
}

constexpr std::array<std::int32_t, 89> kSteps{
    7,    8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,    25,   28,
    31,   34,    37,    41,    45,    50,    55,    60,    66,    73,    80,    88,    97,    107,  118,
    130,  143,   157,   173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,  494,
    544,  598,   658,   724,   796,   876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878, 2066,
    2272, 2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845, 8630,
    9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
constexpr std::array<std::int32_t, 8> kIndexSteps{-1, -1, -1, -1, 2, 4, 6, 8};
constexpr std::int32_t kLastStep = static_cast<std::int32_t>(kSteps.size()) - 1;

// One channel's decoder, as IMA expands a nibble.
struct Ima
{
    std::int32_t sample = 0;
    std::int32_t index = 0;

    std::int16_t Expand(std::uint8_t nibble)
    {
        const std::int32_t step = kSteps[static_cast<std::size_t>(index)];
        std::int32_t delta = step >> 3;
        if ((nibble & 1) != 0)
        {
            delta += step >> 2;
        }
        if ((nibble & 2) != 0)
        {
            delta += step >> 1;
        }
        if ((nibble & 4) != 0)
        {
            delta += step;
        }
        sample = std::clamp((nibble & 8) != 0 ? sample - delta : sample + delta, -32768, 32767);
        index = std::clamp(index + kIndexSteps[nibble & 7], 0, kLastStep);
        return static_cast<std::int16_t>(sample);
    }
};

} // namespace

Codec CodecOf(std::uint32_t mode)
{
    if ((mode & mode::kImaAdpcm) != 0)
    {
        return Codec::ImaAdpcm;
    }
    if ((mode & (mode::kMpeg | mode::kMpegLayer2 | mode::kMpegLayer3)) != 0)
    {
        return Codec::Mpeg;
    }
    if ((mode & (mode::kVag | mode::kXma | mode::kGcAdpcm | mode::kOgg)) != 0)
    {
        return Codec::Other;
    }
    return (mode & mode::k8Bits) != 0 ? Codec::Pcm8 : Codec::Pcm16;
}

std::optional<Bank> Bank::FromHeaders(std::span<const std::byte> headers, std::uint64_t fileSize)
{
    constexpr std::uint32_t kMagic = 0x34425346; // "FSB4"
    if (headers.size() < kHeaderSize || At<std::uint32_t>(headers, 0) != kMagic)
    {
        return std::nullopt;
    }
    Bank bank;
    Header &h = bank.header_;
    h.samples = At<std::uint32_t>(headers, 4);
    h.headersSize = At<std::uint32_t>(headers, 8);
    h.dataSize = At<std::uint32_t>(headers, 12);
    h.version = At<std::uint32_t>(headers, 16);
    h.mode = At<std::uint32_t>(headers, 20);
    for (std::size_t i = 0; i < h.hash.size(); ++i)
    {
        h.hash[i] = std::to_integer<std::uint8_t>(headers[32 + i]);
    }
    if (h.version != kVersion || (h.mode & (bank_mode::kBasicHeaders | bank_mode::kEncrypted)) != 0 ||
        headers.size() - kHeaderSize < h.headersSize)
    {
        return std::nullopt;
    }
    const std::span<const std::byte> table = headers.subspan(kHeaderSize, h.headersSize);
    std::uint64_t at = kHeaderSize + std::uint64_t{h.headersSize};
    std::size_t place = 0;
    bank.samples_.reserve(std::min<std::size_t>(h.samples, h.headersSize / kSampleHeaderSize));
    for (std::uint32_t i = 0; i < h.samples; ++i)
    {
        const auto size = At<std::uint16_t>(table, place);
        if (size < kSampleHeaderSize || table.size() - place < size)
        {
            return std::nullopt;
        }
        auto sample = SampleOf(table.subspan(place, size));
        if (!sample || at > fileSize || fileSize - at < sample->size)
        {
            return std::nullopt;
        }
        sample->offset = at;
        at += sample->size;
        place += size;
        bank.samples_.push_back(std::move(*sample));
    }
    return bank;
}

std::optional<Bank> Bank::Parse(std::span<const std::byte> bytes)
{
    return FromHeaders(bytes, bytes.size());
}

std::optional<Bank> Bank::Open(const std::filesystem::path &path)
{
    std::error_code error;
    const auto fileSize = std::filesystem::file_size(path, error);
    std::ifstream in(path, std::ios::binary);
    if (error || !in || fileSize < kHeaderSize)
    {
        return std::nullopt;
    }
    std::vector<std::byte> headers(kHeaderSize);
    if (!in.read(reinterpret_cast<char *>(headers.data()), kHeaderSize))
    {
        return std::nullopt;
    }
    const auto tableSize = At<std::uint32_t>(headers, 8);
    if (fileSize - kHeaderSize < tableSize)
    {
        return std::nullopt;
    }
    headers.resize(kHeaderSize + tableSize);
    if (!in.read(reinterpret_cast<char *>(headers.data() + kHeaderSize), tableSize))
    {
        return std::nullopt;
    }
    auto bank = FromHeaders(headers, fileSize);
    if (bank)
    {
        bank->path_ = path;
    }
    return bank;
}

const Sample *Bank::Find(std::string_view name) const
{
    const std::string wanted = Lower(name);
    for (const Sample &sample : samples_)
    {
        const std::string own = Lower(sample.name);
        const auto dot = own.rfind('.');
        if (own == wanted || (dot != std::string::npos && std::string_view(own).substr(0, dot) == wanted))
        {
            return &sample;
        }
    }
    return nullptr;
}

std::span<const std::byte> Bank::BytesOf(std::span<const std::byte> bank, const Sample &sample)
{
    if (sample.offset > bank.size() || bank.size() - sample.offset < sample.size)
    {
        return {};
    }
    return bank.subspan(static_cast<std::size_t>(sample.offset), sample.size);
}

std::optional<std::vector<std::byte>> Bank::Read(const Sample &sample) const
{
    if (path_.empty())
    {
        return std::nullopt;
    }
    std::ifstream in(path_, std::ios::binary);
    std::vector<std::byte> bytes(sample.size);
    if (!in.seekg(static_cast<std::streamoff>(sample.offset)) ||
        !in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
    {
        return std::nullopt;
    }
    return bytes;
}

std::optional<std::vector<std::int16_t>> DecodeIma(std::span<const std::byte> bytes, std::uint32_t frames,
                                                   std::uint16_t channels)
{
    if (channels < 1 || channels > 2)
    {
        return std::nullopt;
    }
    const std::size_t blocks = (std::size_t{frames} + kImaFrames - 1) / kImaFrames;
    const std::size_t block = kImaBlock * channels;
    if (bytes.size() / block < blocks)
    {
        return std::nullopt;
    }
    std::vector<std::int16_t> pcm(std::size_t{frames} * channels);
    for (std::size_t b = 0; b < blocks; ++b)
    {
        const std::span<const std::byte> in = bytes.subspan(b * block, block);
        const std::size_t first = b * kImaFrames;
        const std::size_t count = std::min<std::size_t>(kImaFrames, frames - first);
        for (std::size_t c = 0; c < channels; ++c)
        {
            Ima ima{At<std::int16_t>(in, kImaHeader * c),
                    std::min<std::int32_t>(std::to_integer<std::uint8_t>(in[(kImaHeader * c) + 2]), kLastStep)};
            pcm[(first * channels) + c] = static_cast<std::int16_t>(ima.sample);
            // Frame f (1 to 63) is nibble f - 1: in a word of 4 bytes, 8 nibbles, the low first; a
            // channel's words take turns with the other channel's.
            for (std::size_t f = 1; f < count; ++f)
            {
                const std::size_t n = f - 1;
                const std::size_t word = n / 8;
                const std::size_t inWord = n % 8;
                const std::size_t at = (kImaHeader * channels) + (((word * channels) + c) * 4) + (inWord / 2);
                const auto byte = std::to_integer<std::uint8_t>(in[at]);
                const auto nibble = static_cast<std::uint8_t>((inWord % 2) == 0 ? byte & 0xF : byte >> 4);
                pcm[((first + f) * channels) + c] = ima.Expand(nibble);
            }
        }
    }
    return pcm;
}

} // namespace dao::core::fsb
