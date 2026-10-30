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
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// d3d9.h's COM interface, named without its header; Release is how it is freed, not a destructor.
struct IDirect3DBaseTexture9; // NOLINT(cppcoreguidelines-virtual-class-destructor)

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

// What ReadAsync hands back: the file's bytes, or nullopt as Read answers.
using ReadDone = std::function<void(std::optional<std::vector<std::byte>> bytes)>;

// Reads a copy of a file's bytes as Read does, but away from the game thread, and hands them to a
// callback on the game thread when they are read.
// @param file the file name with its extension
// @param done runs once, on the game thread (dao/Frame.h), a frame or more later
//
// For a large file, a texture above all, that would hold the game thread for long enough to be
// seen. The reads go one at a time, in the order asked. Call it once the game has started, from
// any thread.
void ReadAsync(std::wstring_view file, ReadDone done);

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
// @param watched true to have the game find files written into the folder after it is added, as
//  it does for the player's override folder; false to read it once, when it is added
// @return false if it is not a folder, the game is not a build the library knows, or the game
//  did not take it
//
// The game finds any file in the folder or its subfolders by name, ahead of its own archives and
// patches and behind the player's override folder. A file that two added folders both have comes
// from either. A watched folder is listed again every fifth frame, so keep it small. Adding a
// folder a second time does nothing and returns true.
//
// Call it from DAOLib_Load, where the folder is added as the game starts and true means only that
// it will be, or on the game thread once the game is running. A file the game has loaded already
// stays as it is until the game loads it again.
bool AddFolder(const std::filesystem::path &folder, bool watched = false);

// Takes away a folder that AddFolder added, so the game finds the files it had elsewhere, or not
// at all.
// @param folder the path given to AddFolder
// @return false if this plugin has not added the folder, or the game did not have it
//
// Call it where AddFolder is called. A file the game has loaded already stays as it is until the
// game loads it again.
bool RemoveFolder(const std::filesystem::path &folder);

// What kind of place a source is.
enum class SourceKind
{
    Folder,  // loose files, in a folder and its subfolders
    Rim,     // an archive the game reads whole, a level's or an area's
    Archive, // an .erf
    Other,
};

// One of the places the game finds its files in.
struct Source
{
    // The game's name for it: a folder's path, or an alias and an archive, L"DATA:guiexport.erf".
    std::wstring name;
    // Lower is looked in first. Of two with the same, a folder comes before a RIM and a RIM
    // before an .erf, and then the later name in the alphabet first.
    std::int32_t priority = 0;
    SourceKind kind = SourceKind::Other;
};

// Returns the places the game finds its files in, in the order it looks in them: a file comes
// from the first that has it.
// @return empty if the game's sources are not set up yet, or the game is not a build the
// library knows
//
// Call it on the game thread, once the game has started. Loading a level or a campaign adds and
// takes away sources.
std::vector<Source> Sources();

// Returns the place the game takes a file from: the first of Sources() that has it.
// @param file the file name with its extension, such as L"rules_core.ncs"
// @return nullopt if the game has no file by that name
//
// It loads the file to find out, as Exists does, so call it on the game thread.
std::optional<Source> SourceOf(std::wstring_view file);

// Returns the file on disk an engine source name stands for, through the engine's alias table:
// L"WRITE_OVERRIDE:my mod\\x.rim" is the file under the override folder the game writes to. The
// part after the ':' comes back in lower case, as the engine makes it. Seen with ENV:, OVERRIDE:,
// WRITE_OVERRIDE:, ADDINS: and BUILD_ADDINS:.
// @return nullopt before the table is made, for an alias the table has not, and for a name that is
// no path; a full path comes back as it is
//
// Call it on the game thread.
std::optional<std::filesystem::path> PathOf(std::wstring_view sourceName);

// A patch to a file as the game reads it. It is given the file's name, in lower case with its
// extension, and its bytes as the game found them, and returns the changed bytes, or nullopt to
// leave them.
using ResourcePatch =
    std::function<std::optional<std::vector<std::byte>>(std::wstring_view name, std::span<const std::byte> bytes)>;

// Patches every file of a type as the game reads it, whichever copy the game takes: a mod's, an
// add-in's, a campaign's or the core's. Use it to add to the game's files where a whole
// replacement would hide every other version of them.
//
//   dao::resources::OnLoad(L"mao", [](std::wstring_view name, std::span<const std::byte> bytes) {
//       return WithMetalness(name, bytes); // the material with lines of the plugin's own added
//   });
//
// @param extension the type, with its dot or without, in any case: L"mao", L".gda"
//
// The patch runs each time the game reads such a file's bytes: when it first uses the file, and
// again if it has let the file go and uses it again. It runs on the thread that reads, any of the
// game's, so it must not call the game; and while it runs, other reads from the same archive wait
// for it. Patches for one type run in the order they were added, each on the previous one's
// result; other plugins' patches run too, in no guaranteed order. Add patches at plugin load,
// before the game reads anything. Textures (.dds) are not patched: the game turns their bytes into
// its own form within the read. Nothing is written to disk.
void OnLoad(std::wstring_view extension, ResourcePatch patch);

// A Direct3D texture the engine has made from a .dds file.
struct Texture
{
    // The file's name in lower case with its extension, L"lak100d_1_4_100.dds".
    std::wstring_view name;
    // What the game binds to draw with it: an IDirect3DTexture9, IDirect3DCubeTexture9 or
    // IDirect3DVolumeTexture9.
    IDirect3DBaseTexture9 *texture = nullptr;
};

// What OnTexture hands a texture to.
using TextureEvent = std::function<void(const Texture &texture)>;

// Calls back when the engine makes a Direct3D texture from a .dds file, whichever copy of it, and
// again when it frees that texture, so that a plugin can tell what it draws with.
// @param made runs once the texture is made, with every mip the texture quality keeps; once per
//  texture, though a file read again may be given the texture the engine already has for it
// @param released runs with the same name and pointer just before the engine releases it, after
//  which the pointer may be another texture's
//
// Both run on the thread that made or frees the texture, any of the game's, so they must not call
// the game; while made runs, other reads from the file's archive wait for it. Call it at plugin
// load, before the game reads anything: a texture made before is not reported.
void OnTexture(TextureEvent made, TextureEvent released);

} // namespace dao::resources
