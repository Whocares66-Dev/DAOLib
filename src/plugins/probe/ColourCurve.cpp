#include "ColourCurve.h"

#include "dao/Frame.h"
#include "dao/Render.h"

#include <Windows.h>
#include <d3d9.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace probe
{

namespace
{

using Clock = std::chrono::steady_clock;

constexpr DWORD kSampler = 1; // hslmatrix.mat.xml's s1
constexpr UINT kTallest = 4;  // a curve is a row, or a few
constexpr auto kLookEvery = std::chrono::seconds(2);
constexpr std::size_t kTexelsALine = 64;

// A curve seen: the texture, what its texels last were, when it was last looked at.
struct Seen
{
    IDirect3DBaseTexture9 *texture = nullptr;
    std::vector<std::uint8_t> bytes;
    Clock::time_point looked;
    bool unreadable = false; // said once
};

std::mutex g_lock;
std::vector<Seen> g_seen;

std::size_t BytesPerTexel(D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
    case D3DFMT_A2R10G10B10:
    case D3DFMT_A2B10G10R10:
    case D3DFMT_G16R16:
    case D3DFMT_R32F:
    case D3DFMT_G16R16F:
        return 4;
    case D3DFMT_A16B16G16R16:
    case D3DFMT_A16B16G16R16F:
    case D3DFMT_G32R32F:
        return 8;
    case D3DFMT_A32B32G32R32F:
        return 16;
    case D3DFMT_L16:
    case D3DFMT_R16F:
    case D3DFMT_R5G6B5:
        return 2;
    case D3DFMT_L8:
    case D3DFMT_A8:
        return 1;
    default:
        return 0;
    }
}

// The level's rows, a row's texels one after another, from a locked rectangle.
std::vector<std::uint8_t> Rows(const D3DLOCKED_RECT &locked, const D3DSURFACE_DESC &desc, std::size_t texel)
{
    std::vector<std::uint8_t> out(static_cast<std::size_t>(desc.Width) * desc.Height * texel);
    const std::size_t row = static_cast<std::size_t>(desc.Width) * texel;
    for (UINT y = 0; y < desc.Height; ++y)
    {
        std::memcpy(out.data() + (y * row), static_cast<const std::uint8_t *>(locked.pBits) + (y * locked.Pitch), row);
    }
    return out;
}

// The first level's texels, or nothing where they cannot be had: locked where the pool allows,
// copied out first where the texture is drawn into.
std::vector<std::uint8_t> Texels(IDirect3DDevice9 *device, IDirect3DTexture9 *texture, const D3DSURFACE_DESC &desc)
{
    const std::size_t texel = BytesPerTexel(desc.Format);
    if (texel == 0)
    {
        return {};
    }
    D3DLOCKED_RECT locked{};
    if ((desc.Usage & D3DUSAGE_RENDERTARGET) == 0)
    {
        if (FAILED(texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)))
        {
            return {};
        }
        std::vector<std::uint8_t> out = Rows(locked, desc, texel);
        texture->UnlockRect(0);
        return out;
    }
    IDirect3DSurface9 *level = nullptr;
    IDirect3DSurface9 *copy = nullptr;
    std::vector<std::uint8_t> out;
    if (SUCCEEDED(texture->GetSurfaceLevel(0, &level)) &&
        SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &copy,
                                                      nullptr)) &&
        SUCCEEDED(device->GetRenderTargetData(level, copy)) &&
        SUCCEEDED(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
    {
        out = Rows(locked, desc, texel);
        copy->UnlockRect();
    }
    if (copy != nullptr)
    {
        copy->Release();
    }
    if (level != nullptr)
    {
        level->Release();
    }
    return out;
}

void LogTexels(const std::vector<std::uint8_t> &bytes, const D3DSURFACE_DESC &desc)
{
    const std::size_t texel = BytesPerTexel(desc.Format);
    const std::size_t count = bytes.size() / texel;
    for (std::size_t from = 0; from < count; from += kTexelsALine)
    {
        std::string line;
        for (std::size_t i = from; i < count && i < from + kTexelsALine; ++i)
        {
            for (std::size_t b = 0; b < texel; ++b)
            {
                line += fmt::format("{:02x}", bytes[(i * texel) + b]);
            }
            line += ' ';
        }
        spdlog::info("colour curve texels {}-{}: {}", from, std::min(count, from + kTexelsALine) - 1, line);
    }
}

void Look(IDirect3DDevice9 *device, IDirect3DBaseTexture9 *base)
{
    if (base->GetType() != D3DRTYPE_TEXTURE)
    {
        return;
    }
    auto *texture = static_cast<IDirect3DTexture9 *>(base);
    D3DSURFACE_DESC desc{.Format = D3DFMT_UNKNOWN,
                         .Type = D3DRTYPE_TEXTURE,
                         .Usage = 0,
                         .Pool = D3DPOOL_DEFAULT,
                         .MultiSampleType = D3DMULTISAMPLE_NONE,
                         .MultiSampleQuality = 0,
                         .Width = 0,
                         .Height = 0};
    if (FAILED(texture->GetLevelDesc(0, &desc)) || desc.Height > kTallest)
    {
        return;
    }
    const std::scoped_lock hold(g_lock);
    Seen *seen = nullptr;
    for (Seen &one : g_seen)
    {
        seen = one.texture == base ? &one : seen;
    }
    const Clock::time_point now = Clock::now();
    if (seen == nullptr)
    {
        g_seen.push_back({.texture = base, .bytes = {}, .looked = {}, .unreadable = false});
        seen = &g_seen.back();
        spdlog::info("colour curve candidate {}: {} x {}, format {}, pool {}, usage {:#x}, levels {}",
                     static_cast<void *>(base), desc.Width, desc.Height, static_cast<int>(desc.Format),
                     static_cast<int>(desc.Pool), desc.Usage, texture->GetLevelCount());
    }
    else if (now - seen->looked < kLookEvery)
    {
        return;
    }
    seen->looked = now;
    std::vector<std::uint8_t> bytes = Texels(device, texture, desc);
    if (bytes.empty())
    {
        if (!seen->unreadable)
        {
            spdlog::warn("colour curve {}: its texels cannot be read", static_cast<void *>(base));
            seen->unreadable = true;
        }
        return;
    }
    if (bytes != seen->bytes)
    {
        spdlog::info("colour curve {}: its texels, {} bytes", static_cast<void *>(base), bytes.size());
        LogTexels(bytes, desc);
        seen->bytes = std::move(bytes);
    }
}

// The textures a draw's pixel shader samples at s1: the curve among them.
void OnDraw(const dao::render::Draw &draw)
{
    for (const dao::render::Sampled &sampled : draw.pixel.textures)
    {
        if (sampled.sampler == kSampler && sampled.texture != nullptr)
        {
            Look(draw.device, sampled.texture);
        }
    }
}

std::optional<dao::render::Watcher> g_watch;
Clock::time_point g_armed;

// Arms the watch for one frame every kLookEvery, from the game's frame.
void Rearm()
{
    const Clock::time_point now = Clock::now();
    if (g_watch && !g_watch->Armed() && now - g_armed >= kLookEvery)
    {
        g_watch->Arm(1);
        g_armed = now;
    }
}

} // namespace

void AttachColourCurveWatch()
{
    g_watch = dao::render::Watch(&OnDraw);
    dao::OnFrame(&Rearm);
}

} // namespace probe
