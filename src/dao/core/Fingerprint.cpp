#include "dao/core/Fingerprint.h"

#include "dao/core/Bytes.h"

namespace dao::core
{

namespace
{

constexpr std::uint16_t kDosMagic = 0x5a4d;        // "MZ"
constexpr std::uint32_t kNtSignature = 0x00004550; // "PE\0\0"
constexpr std::uint16_t kMachineI386 = 0x014c;
constexpr std::uint16_t kOptionalMagicPe32 = 0x10b;

// Offsets within the headers, from the PE/COFF specification.
constexpr std::size_t kLfanewOffset = 0x3c;
constexpr std::size_t kFileHeaderMachine = 4;
constexpr std::size_t kFileHeaderTimeDateStamp = 8;
constexpr std::size_t kOptionalHeader = 24;
constexpr std::size_t kOptionalMagic = 0;
constexpr std::size_t kOptionalEntryPoint = 16;
constexpr std::size_t kOptionalImageBase32 = 28;
constexpr std::size_t kOptionalSizeOfImage = 56;

} // namespace

std::optional<Fingerprint> ReadFingerprint(std::span<const std::byte> image)
{
    const auto dosMagic = ReadAt<std::uint16_t>(image, 0);
    const auto lfanew = ReadAt<std::uint32_t>(image, kLfanewOffset);
    if (!dosMagic || *dosMagic != kDosMagic || !lfanew)
    {
        return std::nullopt;
    }
    const std::size_t nt = *lfanew;
    const auto signature = ReadAt<std::uint32_t>(image, nt);
    if (!signature || *signature != kNtSignature)
    {
        return std::nullopt;
    }
    const auto machine = ReadAt<std::uint16_t>(image, nt + kFileHeaderMachine);
    const auto timeDateStamp = ReadAt<std::uint32_t>(image, nt + kFileHeaderTimeDateStamp);
    const std::size_t optional = nt + kOptionalHeader;
    const auto magic = ReadAt<std::uint16_t>(image, optional + kOptionalMagic);
    const auto entryPoint = ReadAt<std::uint32_t>(image, optional + kOptionalEntryPoint);
    const auto imageBase = ReadAt<std::uint32_t>(image, optional + kOptionalImageBase32);
    const auto sizeOfImage = ReadAt<std::uint32_t>(image, optional + kOptionalSizeOfImage);
    if (!machine || !timeDateStamp || !magic || !entryPoint || !imageBase || !sizeOfImage)
    {
        return std::nullopt;
    }
    // A PE32+ image keeps a 64-bit base where PE32 keeps a 32-bit one, so the
    // fields above only mean what they say for PE32; report the rest as not
    // 32-bit and leave the base at what was read, which a caller comparing
    // against kExpected rejects anyway.
    return Fingerprint{
        .timeDateStamp = *timeDateStamp,
        .sizeOfImage = *sizeOfImage,
        .entryPointRva = *entryPoint,
        .imageBase = *imageBase,
        .is32Bit = *magic == kOptionalMagicPe32 && *machine == kMachineI386,
    };
}

} // namespace dao::core
