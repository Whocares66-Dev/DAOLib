// Example: a folder of the game's files that a plugin adds and takes away
// (dao/Resources.h).
//
// A plugin's override\ folder is always read. Files that go with a setting
// are kept in a folder of their own instead, here extra\ beside the DLL
// with one texture in it that the game has not, and added when they are
// wanted. This adds the folder as the plugin loads, so it is there from the
// game's start, and on the first frame shows the switch working in a
// running game: the texture found, the folder taken away and the texture
// gone, the folder added again and the texture back. A second texture is in
// override\, to show beside it what is there whatever the plugin does. What
// it finds goes to example_resources.log.
#include "dao/Resources.h"
#include "dao/Frame.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>

namespace
{

constexpr const wchar_t *kTexture = L"example_resources_swatch.dds";
constexpr const wchar_t *kAlways = L"example_resources_always.dds";

std::filesystem::path Extra()
{
    return dao::Module::OwnFolder() / L"extra";
}

void LogTexture(const char *when)
{
    const auto bytes = dao::resources::Read(kTexture);
    spdlog::info("{}: the texture is {}, {} bytes; the override folder's is {}", when, bytes ? "found" : "not found",
                 bytes ? bytes->size() : 0, dao::resources::Exists(kAlways) ? "found" : "not found");
}

// A texture read as the file it is, wherever the game has it: loose in a folder, in one of
// the game's archives, or in one the game has already drawn from. A .dds file begins "DDS ".
void LogFile(const wchar_t *file)
{
    const auto bytes = dao::resources::Read(file);
    const bool whole = bytes && bytes->size() >= 4 && std::memcmp(bytes->data(), "DDS ", 4) == 0;
    // FNV-1a over the bytes, to hold against the file's own outside the game.
    std::uint32_t sum = 2166136261U;
    for (const std::byte byte : bytes.value_or(std::vector<std::byte>{}))
    {
        sum = (sum ^ static_cast<std::uint32_t>(byte)) * 16777619U;
    }
    spdlog::info("{}: {}, {} bytes, {}, sum {:#010x}", dao::Utf8(file), bytes ? "read" : "not read",
                 bytes ? bytes->size() : 0, whole ? "a DDS file" : "NOT a DDS file", sum);
}

void OnFirstFrame()
{
    static bool done = false;
    if (done)
    {
        return;
    }
    done = true;
    LogFile(kAlways);                    // loose, in this plugin's override folder
    LogFile(L"01_fire.dds");             // in the game's texture archive
    LogFile(L"default_blackzero.dds");   // in the character creator's, a RIM
    LogFile(L"atl_chargen_dxt1_00.dds"); // and another of its
    LogTexture("added at load");
    spdlog::info("adding it again: {}", dao::resources::AddFolder(Extra()));
    spdlog::info("taking it away: {}", dao::resources::RemoveFolder(Extra()));
    LogTexture("taken away");
    spdlog::info("taking it away again: {}", dao::resources::RemoveFolder(Extra()));
    spdlog::info("adding it: {}", dao::resources::AddFolder(Extra()));
    LogTexture("added in a running game");
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_resources");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    spdlog::info("adding extra\\ as the plugin loads: {}", dao::resources::AddFolder(Extra()));
    dao::OnFrame(&OnFirstFrame);
    return true;
}
