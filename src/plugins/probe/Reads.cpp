// Reads.h: Resource::ReadOnFirstUse watched for the named resources.
#include "Reads.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"
#include "dao/TypeName.h"
#include "dao/core/Msvc8.h"

#include <spdlog/spdlog.h>

#include <intrin.h>

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <cwctype>
#include <string>
#include <vector>

namespace
{

constexpr std::size_t kSource = 0x1C;
constexpr std::size_t kBytes = 0x20;
constexpr std::size_t kStored = 0x2C;
constexpr std::size_t kLength = 0x30;
constexpr std::size_t kState = 0x34;
constexpr std::size_t kBorrowed = 0x3C;
constexpr std::size_t kRead = 0x3D;
constexpr std::size_t kShown = 16;

std::vector<std::wstring> g_wanted;

using ReadFn = void(__fastcall *)(void *resource, int edx);
dao::Detour<ReadFn> g_read;

std::wstring Lower(std::wstring_view text)
{
    std::wstring out(text);
    std::ranges::transform(out, out.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return out;
}

std::string Fields(std::uintptr_t at)
{
    const auto source = dao::TryRead<std::uintptr_t>(at + kSource).value_or(0);
    return fmt::format(
        "source {:#010x} ({}) stored {} length {} bytes {:#010x} state {} borrowed {} read {}", source,
        source != 0 ? dao::TypeName(reinterpret_cast<const void *>(source)) : std::string("none"),
        dao::TryRead<std::uint32_t>(at + kStored).value_or(0), dao::TryRead<std::uint32_t>(at + kLength).value_or(0),
        dao::TryRead<std::uintptr_t>(at + kBytes).value_or(0), dao::TryRead<std::int32_t>(at + kState).value_or(-1),
        dao::TryRead<std::uint8_t>(at + kBorrowed).value_or(0xFF),
        dao::TryRead<std::uint8_t>(at + kRead).value_or(0xFF));
}

std::string Ends(std::uintptr_t at)
{
    const auto bytes = dao::TryRead<std::uintptr_t>(at + kBytes).value_or(0);
    const auto length = dao::TryRead<std::uint32_t>(at + kLength).value_or(0);
    if (bytes == 0 || length < kShown)
    {
        return "-";
    }
    std::array<char, kShown> head{};
    std::array<char, kShown> tail{};
    if (!dao::TryCopy(head.data(), bytes, kShown) || !dao::TryCopy(tail.data(), bytes + length - kShown, kShown))
    {
        return "unreadable";
    }
    const auto shown = [](const std::array<char, kShown> &part) {
        std::string out;
        for (const char c : part)
        {
            out += c >= 0x20 && c < 0x7F ? c : '.';
        }
        return out;
    };
    return fmt::format("[{}] ... [{}]", shown(head), shown(tail));
}

bool Watched(std::uintptr_t resource)
{
    if (resource == 0)
    {
        return false;
    }
    const std::wstring name = Lower(reinterpret_cast<const dao::core::WStringRep *>(resource)->View());
    return std::ranges::any_of(g_wanted, [&](const std::wstring &w) { return name.starts_with(w); });
}

std::string NameOf(std::uintptr_t resource)
{
    return dao::Utf8(reinterpret_cast<const dao::core::WStringRep *>(resource)->View());
}

// Resources::Size and Data, __thiscall on a handle (the resource at its +0), plain `ret`s.
using HandleFn = std::uint32_t(__fastcall *)(void *handle, int edx);
dao::Detour<HandleFn> g_size;
dao::Detour<HandleFn> g_data;

std::uint32_t __fastcall Size(void *handle, int edx)
{
    const auto caller = dao::Module::Preferred(reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    const auto resource = dao::TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(handle)).value_or(0);
    const bool watched = Watched(resource);
    if (watched)
    {
        spdlog::info("reads: {} Size from {:#010x} before: {}", NameOf(resource), caller, Fields(resource));
    }
    const std::uint32_t size = g_size.Original()(handle, edx);
    if (watched)
    {
        spdlog::info("reads: {} Size from {:#010x} answered {}: {}", NameOf(resource), caller, size, Fields(resource));
    }
    return size;
}

std::uint32_t __fastcall Data(void *handle, int edx)
{
    const auto caller = dao::Module::Preferred(reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    const auto resource = dao::TryRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(handle)).value_or(0);
    const bool watched = Watched(resource);
    const std::uint32_t data = g_data.Original()(handle, edx);
    if (watched)
    {
        spdlog::info("reads: {} Data from {:#010x} answered {:#010x}: {}", NameOf(resource), caller, data,
                     Fields(resource));
    }
    return data;
}

void __fastcall Read(void *resource, int edx)
{
    const auto at = reinterpret_cast<std::uintptr_t>(resource);
    const std::wstring name = Lower(reinterpret_cast<const dao::core::WStringRep *>(at)->View());
    const bool watched = std::ranges::any_of(g_wanted, [&](const std::wstring &w) { return name.starts_with(w); });
    if (watched)
    {
        spdlog::info("reads: {} before: {}", dao::Utf8(name), Fields(at));
    }
    g_read.Original()(resource, edx);
    if (watched)
    {
        spdlog::info("reads: {} after: {}; {}", dao::Utf8(name), Fields(at), Ends(at));
    }
}

} // namespace

namespace probe
{

void AttachReadWatch()
{
    std::wstring list;
    std::array<wchar_t, 512> given{};
    const DWORD length = GetEnvironmentVariableW(L"DAOLIB_PROBE_READS", given.data(), static_cast<DWORD>(given.size()));
    if (length > 0 && length < given.size())
    {
        list.assign(given.data(), length);
    }
    for (std::size_t start = 0; start <= list.size();)
    {
        const std::size_t comma = std::min(list.find(L',', start), list.size());
        if (comma > start)
        {
            g_wanted.push_back(Lower(std::wstring_view(list).substr(start, comma - start)));
        }
        start = comma + 1;
    }
    if (g_wanted.empty())
    {
        spdlog::info("reads: off; DAOLIB_PROBE_READS names the resources to watch");
        return;
    }
    spdlog::info("reads: watching {} name prefixes", g_wanted.size());
    if (!g_read.Attach(dao::addr::kResource_ReadOnFirstUse, &Read) ||
        !g_size.Attach(dao::addr::kResources_Size, &Size) || !g_data.Attach(dao::addr::kResources_Data, &Data))
    {
        spdlog::warn("reads: could not detour a resource's read, Size or Data");
    }
}

} // namespace probe
