// ASI plugins: DLLs named *.asi that an ASI loader puts into a game, DAFix
// among them for this one (DAOLib #145). They ship with Ultimate ASI Loader
// as bin_ship\dinput8.dll, the loader's own name (loader/Proxy.cpp), so the
// two overwrite each other, and UAL cannot stand behind the loader as
// dinput8.chain.dll: it picks what it proxies by its own file name, and
// under any other shows "This library isn't supported." and exits the
// process (UAL's source/loader/proxy/proxy.cpp, read 2026-10-16). So the
// loader loads them itself, where UAL would look with its default settings
// (source/loader/plugins/plugins.cpp): its own folder, then scripts\ and
// plugins\ beside it, not recursively, and InitializeASI called once per
// module after its load. bin_ship\plugins is not daolib\plugins, which holds
// DAOLib's own.
//
// UAL loads them from its DllMain where its ini says DontLoadFromDllMain=0,
// as DAFix's does, and otherwise at the exe's entry point; here they load
// at the entry point, with the loader lock released, before the game's code
// as either way. Nothing they do is checked: an ASI plugin patches the game
// by its own lights, and the log names what was loaded.
#include "loader/Asi.h"

#include "dao/Log.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <string>
#include <system_error>
#include <vector>

namespace
{

bool IsAsi(const std::filesystem::path &path)
{
    auto extension = path.extension().wstring();
    std::ranges::transform(extension, extension.begin(), ::towlower);
    return extension == L".asi";
}

std::vector<std::filesystem::path> AsiFiles(const std::filesystem::path &folder)
{
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(folder, ec))
    {
        if (entry.is_regular_file(ec) && IsAsi(entry.path()))
        {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    return files;
}

} // namespace

namespace loader::asi
{

void LoadAll(const std::filesystem::path &folder)
{
    int count = 0;
    for (const auto &where : std::array{folder, folder / L"scripts", folder / L"plugins"})
    {
        for (const auto &path : AsiFiles(where))
        {
            const auto name = std::filesystem::relative(path, folder).string();
            HMODULE module = LoadLibraryW(path.c_str());
            if (module == nullptr)
            {
                spdlog::error("{}: LoadLibrary failed, error {}", name, GetLastError());
                continue;
            }
            ++count;
            using InitializeFn = void (*)();
            if (const auto initialize = reinterpret_cast<InitializeFn>(GetProcAddress(module, "InitializeASI")))
            {
                initialize();
            }
            spdlog::info("{}: loaded, an ASI plugin", name);
        }
    }
    if (count != 0)
    {
        spdlog::info("{} ASI plugin(s) in {}", count, folder.string());
    }
}

} // namespace loader::asi
