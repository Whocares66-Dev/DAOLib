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
//
// A level's own files are in archives the game loads with the level and
// reads whole (a RIM). To hold any of them against its copy on disk, name
// them in a file read.txt beside the DLL, a line each: once the game is
// first exploring a level, each is read and logged with its length and a
// sum over its bytes.
//
// Where the game takes a file from: every place it finds its files in is
// logged in the order it looks in them, on the first frame and once a level
// is being explored, and for each file named in where.txt (a line each) the
// place it is taken from, and the start of what it holds, so that test files
// of one name put in several places show which the game reads.
#include "dao/Resources.h"
#include "dao/ClientExoApp.h"
#include "dao/Frame.h"
#include "dao/GameModes.h"
#include "dao/Log.h"
#include "dao/Module.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
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

// How many files of a type the game has, and whether one known to be there is among them.
void LogNames(const wchar_t *type, const wchar_t *known)
{
    const std::vector<std::wstring> names = dao::resources::Names(type);
    const bool has = std::ranges::find(names, std::wstring(known)) != names.end();
    std::vector<std::wstring> sorted = names;
    std::ranges::sort(sorted);
    const bool once = std::ranges::adjacent_find(sorted) == sorted.end();
    spdlog::info("{}: {} names, each once: {}, {} among them: {}; the first is {}", dao::Utf8(type), names.size(), once,
                 dao::Utf8(known), has, names.empty() ? std::string("none") : dao::Utf8(names.front()));
}

const char *KindName(dao::resources::SourceKind kind)
{
    switch (kind)
    {
    case dao::resources::SourceKind::Folder:
        return "folder";
    case dao::resources::SourceKind::Rim:
        return "rim";
    case dao::resources::SourceKind::Archive:
        return "erf";
    default:
        return "other";
    }
}

// Every place the game finds its files in, in the order it looks; then where each file
// where.txt names comes from.
void LogSources(const char *when)
{
    const std::vector<dao::resources::Source> sources = dao::resources::Sources();
    spdlog::info("{}: {} sources, in the order the game looks in them", when, sources.size());
    for (std::size_t i = 0; i < sources.size(); ++i)
    {
        spdlog::info("  {:5} {:7} {:6} {}", i, sources[i].priority, KindName(sources[i].kind),
                     dao::Utf8(sources[i].name));
    }
    std::ifstream list(dao::Module::OwnFolder() / L"where.txt");
    for (std::string line; std::getline(list, line);)
    {
        if (line.empty())
        {
            continue;
        }
        const std::wstring file(line.begin(), line.end()); // the game's file names are ASCII
        const auto source = dao::resources::SourceOf(file);
        const auto bytes = dao::resources::Read(file);
        std::string start;
        for (const std::byte byte : bytes.value_or(std::vector<std::byte>{}))
        {
            const auto letter = static_cast<char>(byte);
            if (start.size() == 40 || letter == '\r' || letter == '\n')
            {
                break;
            }
            start.push_back(letter >= ' ' && letter <= '~' ? letter : '.');
        }
        spdlog::info("{}: {} is from {} ({} {}), holding \"{}\"", when, line,
                     source ? dao::Utf8(source->name) : std::string("nowhere"), source ? source->priority : 0,
                     source ? KindName(source->kind) : "-", start);
    }
}

// The files read.txt names, read once the game is first exploring a level.
void InALevel()
{
    static bool done = false;
    if (done || dao::CClientExoApp::CurrentMode() != dao::GameMode::Explore)
    {
        return;
    }
    done = true;
    LogSources("in a level");
    std::ifstream list(dao::Module::OwnFolder() / L"read.txt");
    for (std::string line; std::getline(list, line);)
    {
        if (!line.empty())
        {
            const std::wstring file(line.begin(), line.end()); // the game's file names are ASCII
            spdlog::info("in a level, {} is {}", line, dao::resources::Exists(file) ? "found" : "not found");
            LogFile(file.c_str());
        }
    }
}

void EachFrame()
{
    InALevel();
    static bool done = false;
    if (done)
    {
        return;
    }
    done = true;
    LogNames(L"dds", kAlways);           // this plugin's own, in its override folder
    LogNames(L".dds", L"01_fire.dds");   // the game's, the type given with its dot
    LogNames(L"mao", L"anvil_01.mao");   // a material
    LogNames(L"no_such_type", L"x");     // none
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
    LogSources("on the first frame");
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
    dao::OnFrame(&EachFrame);
    return true;
}
