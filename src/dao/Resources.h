// Reads the game's files by name, found the same way the game finds its own: in the override
// folders, add-ins, patches and archives, in the game's order of precedence. Use it to read what
// is actually installed, so that a mod's version of a file wins over the original.
//
//   const auto script = dao::resources::Read(L"rules_core.ncs");
//   if (script) { /* the compiled script the game runs */ }
//
// A plugin's override\ folder is always among those places. A set of files that goes with a
// setting is kept in a folder of its own beside it, and added and removed as the setting says:
//
//   const auto relit = dao::Module::OwnFolder() / L"relit";
//   if (settings.relit) { dao::resources::AddFolder(relit); }
#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::resources
{

// Returns a copy of a file's bytes.
// @param file the file name with its extension, such as L"rules_core.ncs"
// @return nullopt if the game has no file by that name or cannot read it. The game may write a
// line about it to its own log.
//
// Call it on the game thread, once the game has started. A texture (.dds) is read from its
// archive or folder each time, since the game keeps its own form of a texture it has used and
// not the file; every other file is copied from what the game holds.
std::optional<std::vector<std::byte>> Read(std::wstring_view file);

// Returns whether the game has a file by this name, found the same way as Read finds it. It
// loads the file to find out, so call it on the game thread too.
bool Exists(std::wstring_view file);

// Returns the names of the game's files of one type, each once, in no order: every file the
// game has in its folders and archives as they are now, add-ins' and a plugin's among them.
// @param type the extension, with its dot or without: L"mao", L".dds"
// @return the names in lower case, each with its extension; empty if the game has none, or is
// not a build the library knows
//
// Call it on the game thread, once the game has started. The files of an area's own archives
// are among them only while the area is loaded.
std::vector<std::wstring> Names(std::wstring_view type);

// Adds a folder to the places the game finds its files in, as a plugin's override\ folder is.
// @param folder the folder's full path
// @return false if it is not a folder, the game is not a build the library knows, or the game
//  did not take it
//
// The game finds any file in the folder or its subfolders by name, ahead of its own archives and
// patches and behind the player's override folder. A file that two added folders both have comes
// from either. The folder is read once, when it is added. Adding a folder a second time does
// nothing and returns true.
//
// Call it from DAOLib_Load, where the folder is added as the game starts and true means only that
// it will be, or on the game thread once the game is running. A file the game has loaded already
// stays as it is until the game loads it again.
bool AddFolder(const std::filesystem::path &folder);

// Takes away a folder that AddFolder added, so the game finds the files it had elsewhere, or not
// at all.
// @param folder the path given to AddFolder
// @return false if this plugin has not added the folder, or the game did not have it
//
// Call it where AddFolder is called. A file the game has loaded already stays as it is until the
// game loads it again.
bool RemoveFolder(const std::filesystem::path &folder);

} // namespace dao::resources
