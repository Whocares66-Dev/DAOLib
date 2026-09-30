#include "dao/core/Pe.h"

#include "dao/core/Bytes.h"

#include <array>

namespace dao::core
{

namespace
{

constexpr std::uint16_t kDosMagic = 0x5a4d;
constexpr std::uint32_t kNtSignature = 0x00004550;
constexpr std::size_t kLfanewOffset = 0x3c;
constexpr std::size_t kFileHeaderSectionCount = 6;
constexpr std::size_t kFileHeaderOptionalSize = 20;
constexpr std::size_t kOptionalHeader = 24;
constexpr std::size_t kSectionHeaderSize = 40;
constexpr std::size_t kSectionName = 0;
constexpr std::size_t kSectionVirtualSize = 8;
constexpr std::size_t kSectionVirtualAddress = 12;

} // namespace

std::optional<Section> FindSection(std::span<const std::byte> image, std::string_view name)
{
    if (name.size() > 8)
    {
        return std::nullopt;
    }
    const auto dosMagic = ReadAt<std::uint16_t>(image, 0);
    const auto lfanew = ReadAt<std::uint32_t>(image, kLfanewOffset);
    if (!dosMagic || *dosMagic != kDosMagic || !lfanew)
    {
        return std::nullopt;
    }
    const std::size_t nt = *lfanew;
    const auto signature = ReadAt<std::uint32_t>(image, nt);
    const auto count = ReadAt<std::uint16_t>(image, nt + kFileHeaderSectionCount);
    const auto optionalSize = ReadAt<std::uint16_t>(image, nt + kFileHeaderOptionalSize);
    if (!signature || *signature != kNtSignature || !count || !optionalSize)
    {
        return std::nullopt;
    }
    const std::size_t table = nt + kOptionalHeader + *optionalSize;
    for (std::size_t i = 0; i < *count; ++i)
    {
        const std::size_t at = table + i * kSectionHeaderSize;
        const auto raw = ReadAt<std::array<char, 8>>(image, at + kSectionName);
        const auto size = ReadAt<std::uint32_t>(image, at + kSectionVirtualSize);
        const auto rva = ReadAt<std::uint32_t>(image, at + kSectionVirtualAddress);
        if (!raw || !size || !rva)
        {
            return std::nullopt;
        }
        // The name is padded with NULs to eight bytes, and a full eight has none.
        std::string_view have(raw->data(), raw->size());
        have = have.substr(0, have.find('\0'));
        if (have == name)
        {
            return Section{.rva = *rva, .virtualSize = *size};
        }
    }
    return std::nullopt;
}

} // namespace dao::core
