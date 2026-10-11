#include "fix/SavedFileName.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/core/Msvc8.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace fix
{

namespace
{

using dao::core::WStringRep;

constexpr std::size_t kCopied = 0x400; // bytes the engine copies from the name's buffer
constexpr std::size_t kCharacters = kCopied / sizeof(wchar_t);

std::atomic<std::uint32_t> g_names{0}; // handed on, for one line in the log at the first

// A name as the engine's copy wants it: a string whose buffer is all of the bytes the copy
// takes, the name's characters at its start, cut to leave a terminator, and nothing after.
struct Whole
{
    std::array<wchar_t, kCharacters> text{};
    WStringRep rep;

    explicit Whole(const WStringRep &name)
    {
        const std::size_t size = std::min<std::size_t>(name.size, kCharacters - 1);
        std::copy_n(name.Characters(), size, text.begin());
        rep.data.pointer = text.data();
        rep.size = static_cast<std::uint32_t>(size);
        rep.capacity = static_cast<std::uint32_t>(kCharacters - 1); // more than fits in the string itself: a pointer
        if (g_names.fetch_add(1, std::memory_order_relaxed) == 0)
        {
            spdlog::info("fixes: a file's name of {} characters was handed to the renderer in a buffer of {} bytes, "
                         "which is what the renderer copies (issue #78)",
                         size, kCopied);
        }
    }
};

// ThreadedGraphicsDriver's slot 84: __thiscall (object, const std::wstring *name, eight more),
// `ret 0x28`. What it leaves in eax, its count of commands, is passed on unread.
using SaveFn = std::uintptr_t(__fastcall *)(std::uintptr_t driver, int edx, std::uintptr_t object,
                                            const WStringRep *name, std::uintptr_t a3, std::uintptr_t a4,
                                            std::uintptr_t a5, std::uintptr_t a6, std::uintptr_t a7, std::uintptr_t a8,
                                            std::uintptr_t a9, std::uintptr_t a10);
dao::Detour<SaveFn> g_save;

std::uintptr_t __fastcall Save(std::uintptr_t driver, int edx, std::uintptr_t object, const WStringRep *name,
                               std::uintptr_t a3, std::uintptr_t a4, std::uintptr_t a5, std::uintptr_t a6,
                               std::uintptr_t a7, std::uintptr_t a8, std::uintptr_t a9, std::uintptr_t a10)
{
    if (name == nullptr)
    {
        return g_save.Original()(driver, edx, object, name, a3, a4, a5, a6, a7, a8, a9, a10);
    }
    const Whole whole(*name);
    return g_save.Original()(driver, edx, object, &whole.rep, a3, a4, a5, a6, a7, a8, a9, a10);
}

// Its slot 85: the same with two arguments fewer, `ret 0x20`.
using SaveOtherFn = std::uintptr_t(__fastcall *)(std::uintptr_t driver, int edx, std::uintptr_t object,
                                                 const WStringRep *name, std::uintptr_t a3, std::uintptr_t a4,
                                                 std::uintptr_t a5, std::uintptr_t a6, std::uintptr_t a7,
                                                 std::uintptr_t a8);
dao::Detour<SaveOtherFn> g_saveOther;

std::uintptr_t __fastcall SaveOther(std::uintptr_t driver, int edx, std::uintptr_t object, const WStringRep *name,
                                    std::uintptr_t a3, std::uintptr_t a4, std::uintptr_t a5, std::uintptr_t a6,
                                    std::uintptr_t a7, std::uintptr_t a8)
{
    if (name == nullptr)
    {
        return g_saveOther.Original()(driver, edx, object, name, a3, a4, a5, a6, a7, a8);
    }
    const Whole whole(*name);
    return g_saveOther.Original()(driver, edx, object, &whole.rep, a3, a4, a5, a6, a7, a8);
}

} // namespace

void InstallSavedFileNameFix()
{
    const bool one = g_save.Attach(dao::addr::kThreadedGraphicsDriver_SaveToFile, &Save);
    const bool other = g_saveOther.Attach(dao::addr::kThreadedGraphicsDriver_SaveToFileOther, &SaveOther);
    if (!one || !other)
    {
        spdlog::warn("fixes: a saved file's name is still copied past its end in {} of the renderer's two commands: "
                     "the detour could not be placed (issue #78)",
                     (one ? 0 : 1) + (other ? 0 : 1));
        return;
    }
    spdlog::info("fixes: a saved file's name reaches the renderer in a buffer of the size the renderer copies "
                 "(issue #78)");
}

} // namespace fix
