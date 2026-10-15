// Hud.h: the HUD's movie records compared, dword by dword, every half second.
#include "Hud.h"

#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/Gui.h"
#include "dao/SafeRead.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <map>
#include <string>

namespace
{

constexpr std::size_t kRecordSize = 0x108;
constexpr std::size_t kWords = kRecordSize / 4;
constexpr std::array<std::wstring_view, 6> kHud{L"portraits", L"minimap",  L"armycontrol",
                                                L"navbar",    L"quickbar", L"mainui"};

std::map<std::wstring, std::array<std::uint32_t, kWords>> g_last; // by the movie's name
auto g_since = std::chrono::steady_clock::now();

std::wstring Lower(std::wstring_view text)
{
    std::wstring out(text);
    for (wchar_t &c : out)
    {
        c = static_cast<wchar_t>(std::towlower(c));
    }
    return out;
}

using Words = std::array<std::uint32_t, kWords>;

bool IsHud(const std::wstring &name)
{
    return std::ranges::any_of(kHud, [&](std::wstring_view wanted) { return name == wanted; });
}

// Every dword, the first time a record is seen.
std::string Whole(const Words &words)
{
    std::string all;
    for (std::size_t i = 0; i < kWords; ++i)
    {
        all += fmt::format(" {:03x}={:08x}", i * 4, words.at(i));
    }
    return all;
}

// The dwords that differ, as offset: before -> after.
std::string Differences(const Words &before, const Words &after)
{
    std::string changes;
    for (std::size_t i = 0; i < kWords; ++i)
    {
        if (before.at(i) != after.at(i))
        {
            changes += fmt::format(" {:03x}: {:08x} -> {:08x}", i * 4, before.at(i), after.at(i));
        }
    }
    return changes;
}

void Compare()
{
    const auto now = std::chrono::steady_clock::now();
    if (now - g_since < std::chrono::milliseconds(500))
    {
        return;
    }
    g_since = now;
    auto *app = dao::CClientExoApp::Get();
    const auto mode = dao::CClientExoApp::CurrentMode();
    const int modeNumber = mode ? static_cast<int>(*mode) : -1;
    const char *const state = app != nullptr && app->IsPaused() ? "paused" : "running";
    for (void *owner : dao::gui::Owners())
    {
        const std::wstring name = Lower(dao::gui::NameOf(owner));
        Words words{};
        if (!IsHud(name) || !dao::TryCopy(words.data(), reinterpret_cast<std::uintptr_t>(owner), sizeof words))
        {
            continue;
        }
        const auto found = g_last.find(name);
        if (found == g_last.end())
        {
            spdlog::info("hud: {} first, mode {} {}:{}", dao::Utf8(name), modeNumber, state, Whole(words));
            g_last.emplace(name, words);
        }
        else if (const std::string changes = Differences(found->second, words); !changes.empty())
        {
            spdlog::info("hud: {} mode {} {}:{}", dao::Utf8(name), modeNumber, state, changes);
            found->second = words;
        }
    }
}

} // namespace

namespace probe
{

void AttachHudWatch()
{
    dao::OnFrame(&Compare);
}

} // namespace probe
