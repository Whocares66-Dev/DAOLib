// A GDA table's column, as the game names it: the CRC32 of the column's
// name, lowercased, as UTF-16LE. A table stores only the hash, so a column
// is found by computing it.
//
// Read 2026-09-30: xoreos reads DAO's GDA files so (src/aurora/gdafile.cpp,
// Common::hashStringCRC32 of the lowercase name, UTF-16LE), and its list of
// 2,205 known column names matches the hash for every one; the engine's
// own getters take the same hashes (GameModeExplore's ability handler asks
// the ability table for 0x0F642429, "TargetType", and 0xACEFC4C2,
// "UseType").
#pragma once

#include <cstdint>
#include <string_view>

namespace dao::core
{

constexpr std::uint32_t GdaColumnHash(std::string_view name)
{
    std::uint32_t crc = 0xFFFFFFFFu;
    const auto feed = [&crc](std::uint8_t byte) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    };
    for (const char c : name)
    {
        const auto lower = static_cast<std::uint8_t>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        feed(lower); // UTF-16LE: the character's low byte, then 0 (column names are ASCII)
        feed(0);
    }
    return ~crc;
}

} // namespace dao::core
