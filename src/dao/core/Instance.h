// A second copy of the game beside the first: the names an instance is
// given, the part of loader/Instance.h that needs no process.
//
// The game runs one at a time by a named mutex its WinMain makes and waits
// on with no timeout (dao/Addresses.h has none of it: it is the system's
// CreateMutexW the loader detours, dev/BUILDS.md says what was read), and
// keeps its settings, saves and DAOLib's logs under Documents\BioWare\
// Dragon Age. An instance is a copy of the game started with a name in
// DAOLIB_INSTANCE: its mutex has the name's own, and the Documents folder
// it is given is one of the instance's, under the user's local application
// data, so the BioWare\Dragon Age it makes there is its own.
#pragma once

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace dao::core
{

// The mutex the game makes at start.
inline constexpr std::wstring_view kGameMutex = L"dragonage";

inline constexpr std::size_t kLongestInstanceName = 32;

// Returns true if the text can name an instance: 1 to 32 of ASCII letters, digits, '-' and
// '_'. It becomes part of a folder's name and a kernel object's, so nothing else is taken.
constexpr bool ValidInstanceName(std::string_view name)
{
    const auto allowed = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    };
    return !name.empty() && name.size() <= kLongestInstanceName && std::ranges::all_of(name, allowed);
}

// Returns the instance a text names: the text in lower case, since a folder's name does not
// tell case apart and a mutex's does, and two spellings must not be two games in one folder.
// @return nullopt if the text is not a valid name
inline std::optional<std::string> InstanceOf(std::string_view asked)
{
    if (!ValidInstanceName(asked))
    {
        return std::nullopt;
    }
    std::string name(asked);
    std::ranges::transform(name, name.begin(),
                           [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
    return name;
}

// Returns the name of an instance's mutex, which no other instance and not the game as
// ever shares.
// @param name an instance's name, as InstanceOf gives it
inline std::wstring InstanceMutexName(std::string_view name)
{
    std::wstring mutex(kGameMutex);
    mutex += L'-';
    mutex.append(name.begin(), name.end());
    return mutex;
}

// Returns the folder an instance is given for Documents: DAOLib\instances\<name> under the
// user's local application data, not under the real Documents, which a machine may keep in
// step with a cloud (the user, 2026-10-11: an instance holds a copy of the saves).
// @param localAppData the user's local application data folder
// @param name an instance's name, as InstanceOf gives it
inline std::filesystem::path InstanceDocuments(const std::filesystem::path &localAppData, std::string_view name)
{
    return localAppData / L"DAOLib" / L"instances" / std::wstring(name.begin(), name.end());
}

} // namespace dao::core
