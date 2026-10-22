// Example: the Direct3D textures the engine makes from .dds files, by name
// (dao/Resources.h, OnTexture; DAOLib #228).
//
// A plugin that changes what a draw binds needs to know which file a
// texture came from: a level's day lightmaps, say, to be swapped for its
// night's. This keeps its own table of the textures alive, by the pointer
// the game binds, filled as the engine makes them and emptied as it frees
// them. Each texture of a level's lightmaps (a name ending in three numbers,
// lak100d_1_4_100.dds) is logged as it is made, with its kind, size and
// mips as Direct3D has them, to hold against the file and the texture
// quality; and every 600 frames the counts made, freed and alive. What it
// finds goes to example_textures.log.
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/Resources.h"
#include "dao/String.h"

#include <d3d9.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <cwctype>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace
{

std::mutex g_mutex;
std::unordered_map<IDirect3DBaseTexture9 *, std::wstring> g_alive;
std::uint64_t g_made = 0;
std::uint64_t g_freed = 0;
std::uint64_t g_frames = 0;

// A level's lightmap atlas: a name whose last three parts before .dds are numbers.
bool IsLightmap(std::wstring_view name)
{
    if (!name.ends_with(L".dds"))
    {
        return false;
    }
    name.remove_suffix(4);
    for (int part = 0; part < 3; ++part)
    {
        const std::size_t underscore = name.rfind(L'_');
        if (underscore == std::wstring_view::npos || underscore + 1 == name.size())
        {
            return false;
        }
        for (const wchar_t c : name.substr(underscore + 1))
        {
            if (std::iswdigit(c) == 0)
            {
                return false;
            }
        }
        name = name.substr(0, underscore);
    }
    return true;
}

void LogLightmap(const dao::resources::Texture &made)
{
    const D3DRESOURCETYPE type = made.texture->GetType();
    if (type != D3DRTYPE_TEXTURE)
    {
        spdlog::info("lightmap {} made: type {}", dao::Utf8(made.name), static_cast<int>(type));
        return;
    }
    auto *const texture = static_cast<IDirect3DTexture9 *>(made.texture);
    D3DSURFACE_DESC top{.Format = D3DFMT_UNKNOWN,
                        .Type = D3DRTYPE_SURFACE,
                        .Usage = 0,
                        .Pool = D3DPOOL_DEFAULT,
                        .MultiSampleType = D3DMULTISAMPLE_NONE,
                        .MultiSampleQuality = 0,
                        .Width = 0,
                        .Height = 0};
    texture->GetLevelDesc(0, &top);
    spdlog::info("lightmap {} made: {}x{}, {} mips, format {}, pool {}", dao::Utf8(made.name), top.Width, top.Height,
                 texture->GetLevelCount(), static_cast<int>(top.Format), static_cast<int>(top.Pool));
}

void Made(const dao::resources::Texture &made)
{
    {
        const std::lock_guard lock(g_mutex);
        ++g_made;
        g_alive.insert_or_assign(made.texture, std::wstring(made.name));
    }
    if (IsLightmap(made.name))
    {
        LogLightmap(made);
    }
}

void Released(const dao::resources::Texture &released)
{
    const std::lock_guard lock(g_mutex);
    ++g_freed;
    const auto it = g_alive.find(released.texture);
    if (it == g_alive.end() || it->second != released.name)
    {
        spdlog::warn("{} freed, not made by that name", dao::Utf8(released.name));
        return;
    }
    if (IsLightmap(released.name))
    {
        spdlog::info("lightmap {} freed", dao::Utf8(released.name));
    }
    g_alive.erase(it);
}

void Count()
{
    if (++g_frames % 600 != 0)
    {
        return;
    }
    const std::lock_guard lock(g_mutex);
    spdlog::info("textures: {} made, {} freed, {} alive", g_made, g_freed, g_alive.size());
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_textures");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::resources::OnTexture(&Made, &Released);
    dao::OnFrame(&Count);
    return true;
}
