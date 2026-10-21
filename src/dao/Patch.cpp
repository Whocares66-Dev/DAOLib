// dao::Patch: the game's bytes written over where they are as expected, and put back. The
// expected bytes are read with TryCopy, so an address with nothing mapped is refused rather than
// a fault; the write goes through VirtualProtect, since the exe's code and read-only data are
// not writable, and FlushInstructionCache follows it, since the bytes may be code.
#include "dao/Patch.h"

#include "dao/Log.h"
#include "dao/SafeRead.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace dao
{

namespace
{

// Writes `bytes` at `address` under a protection that allows it, then puts the protection back.
bool Write(std::uintptr_t address, std::span<const std::byte> bytes) noexcept
{
    void *const at = reinterpret_cast<void *>(address);
    DWORD was = 0;
    if (VirtualProtect(at, bytes.size(), PAGE_EXECUTE_READWRITE, &was) == FALSE)
    {
        return false;
    }
    std::memcpy(at, bytes.data(), bytes.size());
    VirtualProtect(at, bytes.size(), was, &was);
    FlushInstructionCache(GetCurrentProcess(), at, bytes.size());
    return true;
}

// Whether the bytes at `address` are `bytes`; false where they cannot be read. Read a block at a
// time into the stack, so the destructor can call it without allocating.
bool Holds(std::uintptr_t address, std::span<const std::byte> bytes) noexcept
{
    std::array<std::byte, 64> there{};
    for (std::size_t done = 0; done < bytes.size(); done += there.size())
    {
        const auto block = bytes.subspan(done, std::min(there.size(), bytes.size() - done));
        if (!TryCopy(there.data(), address + done, block.size()) ||
            !std::equal(block.begin(), block.end(), there.begin()))
        {
            return false;
        }
    }
    return true;
}

} // namespace

// No log here: a global patch is destroyed as the process ends, when the log may be gone already,
// and a destructor must not throw, which formatting a message can.
Patch::~Patch()
{
    if (IsApplied() && Holds(address_, replacement_))
    {
        Write(address_, original_);
    }
}

bool Patch::ApplyBytes(Module::Place at, std::span<const std::byte> expected, std::span<const std::byte> replacement)
{
    const std::uintptr_t address = Module::Address(at.id);
    if (address == 0)
    {
        spdlog::warn("patch: the running build has no ID {}, so its bytes at +{:#x} are left as they are",
                     static_cast<std::uint32_t>(at.id), at.offset);
        return false;
    }
    return ApplyBytesAt(address + at.offset, expected, replacement);
}

bool Patch::ApplyBytesAt(std::uintptr_t address, std::span<const std::byte> expected,
                         std::span<const std::byte> replacement)
{
    if (IsApplied())
    {
        spdlog::warn("patch: {:#010x} is already patched, so {:#010x} is left as it is", Module::Preferred(address_),
                     Module::Preferred(address));
        return false;
    }
    if (!Holds(address, expected))
    {
        spdlog::warn("patch: the {} bytes at {:#010x} are not as expected (another build, or another plugin's patch), "
                     "so they are left as they are",
                     expected.size(), Module::Preferred(address));
        return false;
    }
    if (!Write(address, replacement))
    {
        spdlog::warn("patch: the {} bytes at {:#010x} could not be written", replacement.size(),
                     Module::Preferred(address));
        return false;
    }
    address_ = address;
    original_.assign(expected.begin(), expected.end());
    replacement_.assign(replacement.begin(), replacement.end());
    return true;
}

bool Patch::Revert()
{
    if (!IsApplied())
    {
        return true;
    }
    if (!Holds(address_, replacement_))
    {
        // Not this patch's any more: whoever wrote them owns them now.
        spdlog::warn("patch: the {} bytes at {:#010x} are no longer this patch's, so they are left as they are",
                     replacement_.size(), Module::Preferred(address_));
        address_ = 0;
        return false;
    }
    if (!Write(address_, original_))
    {
        spdlog::warn("patch: the {} bytes at {:#010x} could not be put back", original_.size(),
                     Module::Preferred(address_));
        return false;
    }
    address_ = 0;
    return true;
}

} // namespace dao
