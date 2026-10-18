// An FMOD sample bank (.fsb, FSB4) read: its header, each sample's name,
// length, rate, channels, codec, loop, 3-D distances and sync points, and a
// sample's bytes; and IMA ADPCM, the codec of the game's sound effects and
// ambience, decoded to 16-bit PCM. MPEG, the voice's and the music's, is
// left as its bytes, frames as stored.
//
//   const auto bank = dao::core::fsb::Bank::Open(path); // the headers alone
//   const auto *sample = bank->Find("36451_m");          // or "36451_m.wav"
//   const auto bytes = bank->Read(*sample);
//   if (sample->codec == dao::core::fsb::Codec::ImaAdpcm)
//       const auto pcm = dao::core::fsb::DecodeIma(*bytes, sample->frames, sample->channels);
//
// Read 2026-10-18 over the 9,052 banks of the GOG install (DAOLib #191,
// dev/AUDIO.md): every one FSB4 version 0x00040000, header mode 0x20.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::fsb
{

inline constexpr std::size_t kHeaderSize = 48;       // the bank's header, before the samples' headers
inline constexpr std::size_t kSampleHeaderSize = 80; // a sample's header without its sync points

// A sample's mode bits (FMOD's FSOUND_ flags), those the game's banks have and those a codec is
// told by.
namespace mode
{
inline constexpr std::uint32_t k8Bits = 0x8;
inline constexpr std::uint32_t k16Bits = 0x10;
inline constexpr std::uint32_t kMono = 0x20;
inline constexpr std::uint32_t kStereo = 0x40;
inline constexpr std::uint32_t kMpeg = 0x200;
inline constexpr std::uint32_t k2D = 0x2000;
inline constexpr std::uint32_t kSyncPointsNoNames = 0x4000;
inline constexpr std::uint32_t kMpegLayer2 = 0x40000;
inline constexpr std::uint32_t k3D = 0x100000;
inline constexpr std::uint32_t kImaAdpcm = 0x400000;
inline constexpr std::uint32_t kVag = 0x800000;
inline constexpr std::uint32_t kXma = 0x1000000;
inline constexpr std::uint32_t kGcAdpcm = 0x2000000;
inline constexpr std::uint32_t kOgg = 0x8000000;
inline constexpr std::uint32_t kMpegLayer3 = 0x10000000;
inline constexpr std::uint32_t kImaAdpcmStereo = 0x20000000;
inline constexpr std::uint32_t kSyncPoints = 0x80000000;
} // namespace mode

// The bank's header mode bits this reader cannot read past.
namespace bank_mode
{
inline constexpr std::uint32_t kBasicHeaders = 0x2; // samples after the first with 8-byte headers
inline constexpr std::uint32_t kEncrypted = 0x4;
inline constexpr std::uint32_t kMpegPadded = 0x20; // every bank of the game's
} // namespace bank_mode

enum class Codec : std::uint8_t
{
    Pcm8,
    Pcm16,
    ImaAdpcm,
    Mpeg,
    Other, // a console's codec (VAG, XMA, GameCube ADPCM) or Ogg, none in the game's banks
};

// Returns the codec a sample's mode bits name.
Codec CodecOf(std::uint32_t mode);

struct Header
{
    std::uint32_t samples = 0;
    std::uint32_t headersSize = 0; // the samples' headers, padded so the data starts at a 32-byte boundary
    std::uint32_t dataSize = 0;
    std::uint32_t version = 0; // 0x00040000
    std::uint32_t mode = 0;    // bank_mode bits
    std::array<std::uint8_t, 16> hash{};
};

struct Sample
{
    std::string name;            // "36451_m.wav", as stored
    std::uint32_t frames = 0;    // length in samples a channel
    std::uint32_t size = 0;      // its bytes, a multiple of 32
    std::uint32_t loopStart = 0; // in frames; the game's are 0 to frames - 1
    std::uint32_t loopEnd = 0;
    std::uint32_t mode = 0; // mode bits
    std::int32_t rate = 0;  // default frequency in Hz
    std::uint16_t volume = 0;
    std::int16_t pan = 0;
    std::uint16_t priority = 0;
    std::uint16_t channels = 0;
    float minDistance = 0; // 3-D, 1 and 10000 in every sample of the game's
    float maxDistance = 0;
    std::int32_t varyRate = 0; // the variations of frequency, volume and pan: 0 in the game's
    std::uint16_t varyVolume = 0;
    std::int16_t varyPan = 0;
    std::vector<std::uint32_t> syncPoints; // offsets in frames, ascending; empty for none
    std::uint64_t offset = 0;              // where its bytes start in the bank's file
    Codec codec = Codec::Other;
};

// A bank's header and samples: from its bytes, or from its file, whose samples are read when asked.
class Bank
{
  public:
    // Returns a whole bank's header and samples, or nothing for bytes that are no FSB4 bank this
    // reads or whose samples run past their end.
    static std::optional<Bank> Parse(std::span<const std::byte> bytes);
    // Returns a bank file's header and samples, its headers alone read: the music's bank is 310 MB.
    static std::optional<Bank> Open(const std::filesystem::path &path);

    const Header &Head() const
    {
        return header_;
    }
    const std::vector<Sample> &Samples() const
    {
        return samples_;
    }
    // Returns the sample of a name in any case, with or without its extension; null for none.
    const Sample *Find(std::string_view name) const;
    // Returns a sample's bytes out of the whole bank's bytes; empty if they are not within them.
    static std::span<const std::byte> BytesOf(std::span<const std::byte> bank, const Sample &sample);
    // Returns a sample's bytes from the bank's file. Nothing for a bank given as bytes, or a file
    // that has gone or shrunk since.
    std::optional<std::vector<std::byte>> Read(const Sample &sample) const;

  private:
    static std::optional<Bank> FromHeaders(std::span<const std::byte> headers, std::uint64_t fileSize);

    Header header_;
    std::vector<Sample> samples_;
    std::filesystem::path path_;
};

// FMOD's IMA ADPCM (Xbox IMA as vgmstream names it) decoded to 16-bit PCM, channels interleaved.
// @param frames The sample's length; blocks of 64 frames, the last cut to it.
// @return nothing for channels other than 1 or 2, or bytes short of the frames' blocks.
std::optional<std::vector<std::int16_t>> DecodeIma(std::span<const std::byte> bytes, std::uint32_t frames,
                                                   std::uint16_t channels);

} // namespace dao::core::fsb
