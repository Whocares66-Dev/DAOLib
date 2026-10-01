#include "dao/core/Inflate.h"

#include <array>

namespace dao::core
{

namespace
{

// The stream's bits, least significant first within a byte.
class Bits
{
  public:
    explicit Bits(std::span<const std::byte> in) : in_(in)
    {
    }

    // The next `count` bits (at most 16) as a number, 0 past the end.
    std::uint32_t Take(int count)
    {
        while (held_ < count)
        {
            if (at_ >= in_.size())
            {
                ended_ = true;
                return 0;
            }
            buffer_ |= std::to_integer<std::uint32_t>(in_[at_++]) << held_;
            held_ += 8;
        }
        const std::uint32_t value = buffer_ & ((1U << count) - 1U);
        buffer_ >>= count;
        held_ -= count;
        return value;
    }

    // To the next whole byte: a stored block starts on one.
    void Align()
    {
        buffer_ = 0;
        held_ = 0;
    }

    // Whole bytes, after Align.
    bool Copy(std::size_t count, std::vector<std::byte> &out)
    {
        if (count > in_.size() - at_)
        {
            ended_ = true;
            return false;
        }
        out.insert(out.end(), in_.begin() + static_cast<std::ptrdiff_t>(at_),
                   in_.begin() + static_cast<std::ptrdiff_t>(at_ + count));
        at_ += count;
        return true;
    }

    bool Ended() const
    {
        return ended_;
    }

    std::size_t At() const
    {
        return at_;
    }

  private:
    std::span<const std::byte> in_;
    std::size_t at_ = 0;
    std::uint32_t buffer_ = 0;
    int held_ = 0;
    bool ended_ = false;
};

constexpr int kMaxBits = 15;
constexpr int kMaxSymbols = 288;

// A canonical Huffman code: how many codes there are of each length, and
// the symbols in the order of their codes.
struct Huffman
{
    std::array<std::uint16_t, kMaxBits + 1> count{};
    std::array<std::uint16_t, kMaxSymbols> symbol{};
};

// From each symbol's code length (0 for a symbol with no code). False for
// lengths that give more codes than there is room for.
bool Build(Huffman &code, std::span<const std::uint8_t> lengths)
{
    code.count.fill(0);
    for (const std::uint8_t length : lengths)
    {
        ++code.count[length];
    }
    int left = 1;
    for (int length = 1; length <= kMaxBits; ++length)
    {
        left = left * 2 - code.count[static_cast<std::size_t>(length)];
        if (left < 0)
        {
            return false;
        }
    }
    std::array<std::uint16_t, kMaxBits + 2> offset{};
    for (std::size_t length = 1; length <= kMaxBits; ++length)
    {
        offset[length + 1] = static_cast<std::uint16_t>(offset[length] + code.count[length]);
    }
    for (std::size_t s = 0; s < lengths.size(); ++s)
    {
        if (lengths[s] != 0)
        {
            code.symbol[offset[lengths[s]]++] = static_cast<std::uint16_t>(s);
        }
    }
    return true;
}

// The next symbol, or -1 for bits that are no code or a stream cut short.
int Decode(Bits &bits, const Huffman &code)
{
    int value = 0;
    int first = 0;
    int index = 0;
    for (std::size_t length = 1; length <= kMaxBits; ++length)
    {
        value |= static_cast<int>(bits.Take(1));
        if (bits.Ended())
        {
            return -1;
        }
        const int count = code.count[length];
        if (value - count < first)
        {
            return code.symbol[static_cast<std::size_t>(index + (value - first))];
        }
        index += count;
        first += count;
        first <<= 1;
        value <<= 1;
    }
    return -1;
}

// RFC 1951, 3.2.5: what each length and distance symbol stands for.
constexpr std::array<std::uint16_t, 29> kLengthBase = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                                       31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<std::uint8_t, 29> kLengthExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                       2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<std::uint16_t, 30> kDistanceBase = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                                         33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                                         1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<std::uint8_t, 30> kDistanceExtra = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                         6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

// A block's literals and matches, to its end-of-block symbol.
bool Codes(Bits &bits, const Huffman &lengths, const Huffman &distances, std::vector<std::byte> &out, std::size_t limit)
{
    for (;;)
    {
        const int symbol = Decode(bits, lengths);
        if (symbol < 0)
        {
            return false;
        }
        if (symbol < 256)
        {
            if (out.size() >= limit)
            {
                return false;
            }
            out.push_back(static_cast<std::byte>(symbol));
            continue;
        }
        if (symbol == 256)
        {
            return true;
        }
        const std::size_t which = static_cast<std::size_t>(symbol) - 257;
        if (which >= kLengthBase.size())
        {
            return false;
        }
        const std::size_t length = kLengthBase[which] + bits.Take(kLengthExtra[which]);
        const int reach = Decode(bits, distances);
        if (reach < 0 || static_cast<std::size_t>(reach) >= kDistanceBase.size())
        {
            return false;
        }
        const std::size_t distance =
            kDistanceBase[static_cast<std::size_t>(reach)] + bits.Take(kDistanceExtra[static_cast<std::size_t>(reach)]);
        if (bits.Ended() || distance > out.size() || length > limit - out.size())
        {
            return false;
        }
        // A match may run into what it writes, so byte by byte.
        const std::size_t from = out.size() - distance;
        for (std::size_t i = 0; i < length; ++i)
        {
            out.push_back(out[from + i]);
        }
    }
}

bool Stored(Bits &bits, std::vector<std::byte> &out, std::size_t limit)
{
    bits.Align();
    const std::uint32_t length = bits.Take(16);
    const std::uint32_t inverse = bits.Take(16);
    if (bits.Ended() || (length ^ 0xFFFFU) != inverse || length > limit - out.size())
    {
        return false;
    }
    return bits.Copy(length, out);
}

bool Fixed(Bits &bits, std::vector<std::byte> &out, std::size_t limit)
{
    std::array<std::uint8_t, kMaxSymbols> lengths{};
    for (std::size_t s = 0; s < lengths.size(); ++s)
    {
        lengths[s] = s < 144 ? 8 : s < 256 ? 9 : s < 280 ? 7 : 8;
    }
    std::array<std::uint8_t, 30> distances{};
    distances.fill(5);
    Huffman lengthCode;
    Huffman distanceCode;
    Build(lengthCode, lengths);
    Build(distanceCode, distances);
    return Codes(bits, lengthCode, distanceCode, out, limit);
}

bool Dynamic(Bits &bits, std::vector<std::byte> &out, std::size_t limit)
{
    const std::size_t literals = bits.Take(5) + 257;
    const std::size_t distances = bits.Take(5) + 1;
    const std::size_t codeLengths = bits.Take(4) + 4;
    if (bits.Ended() || literals > 286 || distances > 30)
    {
        return false;
    }
    // The code lengths' own code, its lengths sent in this order.
    constexpr std::array<std::uint8_t, 19> kOrder = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    std::array<std::uint8_t, 19> orderLengths{};
    for (std::size_t i = 0; i < codeLengths; ++i)
    {
        orderLengths[kOrder[i]] = static_cast<std::uint8_t>(bits.Take(3));
    }
    Huffman orderCode;
    if (bits.Ended() || !Build(orderCode, orderLengths))
    {
        return false;
    }

    std::array<std::uint8_t, 286 + 30> lengths{};
    std::size_t at = 0;
    while (at < literals + distances)
    {
        const int symbol = Decode(bits, orderCode);
        if (symbol < 0)
        {
            return false;
        }
        if (symbol < 16)
        {
            lengths[at++] = static_cast<std::uint8_t>(symbol);
            continue;
        }
        std::uint8_t repeated = 0;
        std::size_t times = 0;
        if (symbol == 16)
        {
            if (at == 0)
            {
                return false;
            }
            repeated = lengths[at - 1];
            times = 3 + bits.Take(2);
        }
        else if (symbol == 17)
        {
            times = 3 + bits.Take(3);
        }
        else
        {
            times = 11 + bits.Take(7);
        }
        if (bits.Ended() || at + times > literals + distances)
        {
            return false;
        }
        for (std::size_t i = 0; i < times; ++i)
        {
            lengths[at++] = repeated;
        }
    }
    if (lengths[256] == 0)
    {
        return false; // no end-of-block code
    }
    Huffman lengthCode;
    Huffman distanceCode;
    if (!Build(lengthCode, std::span(lengths).first(literals)) ||
        !Build(distanceCode, std::span(lengths).subspan(literals, distances)))
    {
        return false;
    }
    return Codes(bits, lengthCode, distanceCode, out, limit);
}

std::uint32_t BigEndian32(std::span<const std::byte> bytes)
{
    return std::to_integer<std::uint32_t>(bytes[0]) << 24 | std::to_integer<std::uint32_t>(bytes[1]) << 16 |
           std::to_integer<std::uint32_t>(bytes[2]) << 8 | std::to_integer<std::uint32_t>(bytes[3]);
}

} // namespace

std::uint32_t Adler32(std::span<const std::byte> data)
{
    constexpr std::uint32_t kModulus = 65521;
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (const std::byte byte : data)
    {
        a = (a + std::to_integer<std::uint32_t>(byte)) % kModulus;
        b = (b + a) % kModulus;
    }
    return b << 16 | a;
}

std::optional<std::vector<std::byte>> InflateZlib(std::span<const std::byte> stream, std::size_t limit)
{
    // The header: deflate with a window of up to 32K, no preset dictionary,
    // and the two bytes a multiple of 31.
    if (stream.size() < 6)
    {
        return std::nullopt;
    }
    const std::uint32_t method = std::to_integer<std::uint32_t>(stream[0]);
    const std::uint32_t flags = std::to_integer<std::uint32_t>(stream[1]);
    if ((method & 0x0FU) != 8 || (method >> 4) > 7 || (method * 256 + flags) % 31 != 0 || (flags & 0x20U) != 0)
    {
        return std::nullopt;
    }

    const std::span<const std::byte> deflate = stream.subspan(2);
    Bits bits(deflate);
    std::vector<std::byte> out;
    for (;;)
    {
        const bool last = bits.Take(1) != 0;
        const std::uint32_t type = bits.Take(2);
        if (bits.Ended())
        {
            return std::nullopt;
        }
        const bool good = type == 0   ? Stored(bits, out, limit)
                          : type == 1 ? Fixed(bits, out, limit)
                          : type == 2 ? Dynamic(bits, out, limit)
                                      : false;
        if (!good)
        {
            return std::nullopt;
        }
        if (last)
        {
            break;
        }
    }
    // The checksum follows the last block's last byte.
    const std::size_t end = bits.At();
    if (deflate.size() - end < 4 || BigEndian32(deflate.subspan(end, 4)) != Adler32(out))
    {
        return std::nullopt;
    }
    return out;
}

} // namespace dao::core
