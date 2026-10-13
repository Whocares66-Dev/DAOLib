// A section of a PE image, found by name in its section table: where the
// exe's code (.text) starts and ends, which the probe needs to tell an
// address in the game's code from one in a driver or a system DLL.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace dao::core
{

struct Section
{
    std::uint32_t rva = 0;         // VirtualAddress
    std::uint32_t virtualSize = 0; // VirtualSize, what is mapped

    friend bool operator==(const Section &, const Section &) = default;
};

// The section of that name (".text", at most eight characters) in a PE
// image's headers, as mapped or as on disk: the section table is the same
// in both. Nothing if the bytes are not a PE image or have no such section.
std::optional<Section> FindSection(std::span<const std::byte> image, std::string_view name);

// Where a run of an image's bytes is in the file on disk, from the headers: a section's raw data
// starts at its own offset, not at its RVA.
// @param rva where the run starts, from the image's base
// @param length how many bytes it has
// @return the file offset of its first byte, or nothing if the run is not all in one section's
// raw data (or there are no headers)
std::optional<std::uint32_t> FileOffsetOf(std::span<const std::byte> image, std::uint32_t rva, std::uint32_t length);

} // namespace dao::core
