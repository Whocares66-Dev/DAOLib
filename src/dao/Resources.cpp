#include "dao/Resources.h"

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/String.h"

#include <cstdint>

namespace dao::resources
{

namespace
{

// The mode the movie file asks Find with, which has the bytes there on
// return (dao/Addresses.h).
constexpr std::int32_t kLoadNow = 3;
// More than any of the game's files: a length past it is taken as a
// misread.
constexpr std::uint32_t kMostBytes = 512U * 1024 * 1024;

// A resource found by name, released when it goes.
class Found
{
  public:
    explicit Found(std::wstring_view file)
    {
        WString name(file);
        // Resources::Find: __cdecl (out handle, const std::wstring &, mode).
        using FindFn = std::uintptr_t *(__cdecl *)(std::uintptr_t *, const core::WStringRep *, std::int32_t);
        reinterpret_cast<FindFn>(Module::Address(addr::kResources_Find))(&handle_, name.Engine(), kLoadNow);
    }
    ~Found()
    {
        // Resources::Release: __thiscall on the handle.
        using ReleaseFn = void(__fastcall *)(std::uintptr_t *, int);
        reinterpret_cast<ReleaseFn>(Module::Address(addr::kResources_Release))(&handle_, 0);
    }
    Found(const Found &) = delete;
    Found &operator=(const Found &) = delete;
    Found(Found &&) = delete;
    Found &operator=(Found &&) = delete;

    [[nodiscard]] bool Exists() const
    {
        return handle_ != 0;
    }

    std::optional<std::vector<std::byte>> Bytes()
    {
        if (handle_ == 0)
        {
            return std::nullopt;
        }
        // Resources::Data and Size: __thiscall on the handle.
        using DataFn = std::uintptr_t(__fastcall *)(std::uintptr_t *, int);
        using SizeFn = std::uint32_t(__fastcall *)(std::uintptr_t *, int);
        const std::uintptr_t data = reinterpret_cast<DataFn>(Module::Address(addr::kResources_Data))(&handle_, 0);
        const std::uint32_t size = reinterpret_cast<SizeFn>(Module::Address(addr::kResources_Size))(&handle_, 0);
        // No bytes where they should be is a resource not loaded or a lock
        // refused, both of which also answer a size of 0 (dao/Addresses.h),
        // so it is nothing read rather than an empty file.
        if (data == 0 || size > kMostBytes)
        {
            return std::nullopt;
        }
        std::vector<std::byte> bytes(size);
        if (size != 0 && !TryCopy(bytes.data(), data, size))
        {
            return std::nullopt;
        }
        return bytes;
    }

  private:
    std::uintptr_t handle_ = 0;
};

} // namespace

std::optional<std::vector<std::byte>> Read(std::wstring_view file)
{
    if (file.empty())
    {
        return std::nullopt;
    }
    Found found(file);
    return found.Bytes();
}

bool Exists(std::wstring_view file)
{
    return !file.empty() && Found(file).Exists();
}

} // namespace dao::resources
