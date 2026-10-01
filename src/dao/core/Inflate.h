// Inflating a zlib stream: what a compressed Flash movie is after its
// eight-byte header (dao/core/GfxMovie.h).
//
// The formats are RFC 1950 (the zlib wrapper: two header bytes, the deflate
// data, an Adler-32 of what it inflates to) and RFC 1951 (deflate: stored
// blocks, and blocks of Huffman codes, fixed or sent in the block). Written
// here from the RFCs so the library takes on no dependency for one file.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace dao::core
{

// What the stream inflates to. Nothing for a stream that is not zlib's, is
// cut short or malformed, whose checksum disagrees, or that would inflate
// past `limit` bytes. Bytes after the stream's end are ignored.
std::optional<std::vector<std::byte>> InflateZlib(std::span<const std::byte> stream, std::size_t limit);

// The checksum a zlib stream ends with.
std::uint32_t Adler32(std::span<const std::byte> data);

} // namespace dao::core
