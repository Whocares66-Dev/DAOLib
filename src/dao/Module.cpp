#include "dao/Module.h"

#include <Windows.h>

#include <array>
#include <span>

namespace dao::Module
{

std::uintptr_t Base()
{
    // The handle of the process's executable is its base address.
    return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
}

std::filesystem::path Path()
{
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring_view(buffer.data(), length));
}

std::optional<core::Fingerprint> Fingerprint()
{
    // The headers are mapped at the base, readable, and at least a page
    // long; that page holds the DOS header, the NT headers and the section
    // table of any exe this could be running in.
    constexpr std::size_t kHeaderBytes = 0x1000;
    const auto *bytes = reinterpret_cast<const std::byte *>(Base());
    return core::ReadFingerprint(std::span<const std::byte>(bytes, kHeaderBytes));
}

bool IsExpectedExecutable()
{
    const auto have = Fingerprint();
    return have && *have == core::kExpected;
}

} // namespace dao::Module
