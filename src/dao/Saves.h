// Stores a plugin's own data alongside the game's saves, like SKSE's co-saves in Skyrim: each
// plugin gets a file in the save's folder, written when the game saves and read when a save is
// loaded. It can also list the saves and tell the game to save or load, for tools that drive the
// game.
//
//   std::string g_notes;   // the plugin's state for the game loaded now
//
//   dao::saves::OnGameLoaded([](const std::optional<dao::saves::Slot> &save) {
//       // Nothing for a new game, or for a save this plugin never wrote to.
//       g_notes = save ? save->Read("myplugin").value_or("") : "";
//   });
//   dao::saves::OnSaved([](const dao::saves::Slot &save) { save.Write("myplugin", g_notes); });
//
//   dao::saves::Load({L"Aedan", 1, dao::saves::Kind::Manual}); // OnGameLoaded runs once loaded
//   dao::saves::Save(dao::saves::Kind::Quick);                  // OnSaved runs once written
//
// examples/saves/ is a complete plugin that stores its data with the saves.
#pragma once

#include "dao/core/SaveMeta.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::saves
{

// The kinds of save. The numbers are the engine's own; the scripts have no constants for them.
enum class Kind : std::int32_t
{
    Manual = 0, // Slot_<n>: a save the player named, or a slot the player chose
    Auto = 1,   // AutoSave_<n>, numbered 1 to 4
    Quick = 2,  // QuickSave_<n>
    Other = 3,  // a fourth kind the engine accepts; the game does not appear to use it
};

// Returns "manual", "auto", "quick" or "other", for logging.
constexpr std::string_view Name(Kind kind)
{
    switch (kind)
    {
    case Kind::Manual:
        return "manual";
    case Kind::Auto:
        return "auto";
    case Kind::Quick:
        return "quick";
    default:
        return "other";
    }
}

// Notes on MostSlot: read 2026-10-05 (DAOLib #37) in SaveLoadProxy::HandleMessage_SaveGameMessage
// (0x006D1C60), which refuses any other slot. A manual save asked for "a new slot" (below 0)
// takes the next number from the proxy's +0x6C, so a character with slots 1 to 500 taken can make
// no more manual saves, and the game starts none (seen by the turn-based mod's session). The
// numbers are literals in that code, not data.

// Returns the highest slot number the game saves to for a kind. The lowest is 1, though the game
// also accepts a manual save to slot 0.
constexpr std::int32_t MostSlot(Kind kind)
{
    switch (kind)
    {
    case Kind::Manual:
        return 500;
    case Kind::Auto:
        return 4;
    default:
        return 1;
    }
}

// One save slot of one character.
struct Slot
{
    std::wstring character; // the character's folder name, for example "Aedan"
    std::int32_t number = 0;
    Kind kind = Kind::Manual;

    // Reads this plugin's data from the save: the whole of its file.
    // @param plugin the plugin's name, which is also the name of its file
    // @return the data, or nullopt if the save has none, because it is older than the plugin or
    // the plugin never wrote to it
    std::optional<std::string> Read(std::string_view plugin) const;

    // Writes this plugin's data for the save, replacing what was there.
    // @param plugin the plugin's name, which is also the name of its file
    // @param data any bytes: text, JSON or binary
    // @return false if the file could not be written
    //
    // The data goes to a temporary file that is then moved into place, so a save never holds a
    // half-written file. Each plugin has its own file in each save, so the data needs no framing.
    // If your format may change, store a version number in the data.
    bool Write(std::string_view plugin, std::string_view data) const;

    // Returns the path of this plugin's file in the save, <slot folder>\<plugin>.daolib, for a
    // plugin that would rather stream it.
    std::filesystem::path File(std::string_view plugin) const;

    // Returns the slot's folder, for example
    // <documents>\BioWare\Dragon Age\Characters\<character>\Saves\Slot_<n>. The path comes from
    // the game itself, so it is right wherever the game keeps its saves.
    std::filesystem::path Folder() const;

    // Returns whether the slot holds a save, that is, a .das file in its folder.
    bool Exists() const;

    // Reads the save's metadata from its .das.met file: what the load screen shows, such as the
    // area, the time played, the character's level and the save's name.
    // @return the metadata, or nullopt if the slot has none or it cannot be read
    std::optional<core::SaveMeta> Meta() const;
};

// Registers a callback that runs each time the game writes a save, once the save is written.
// @param callback receives the slot that was written
//
// Callbacks run on the game thread, one at a time, so state shared with OnGameLoaded callbacks
// needs no lock. They must not throw.
void OnSaved(std::function<void(const Slot &saved)> callback);

// Registers a callback that runs each time a game starts in the world: a save loaded, or a new
// game.
// @param callback receives the slot the save came from, or nullopt for a new game
//
// Callbacks run on the game thread, one at a time, and must not throw. They run while the game
// mode still reads as loading; if you need the world in play, also wait for explore mode.
void OnGameLoaded(std::function<void(const std::optional<Slot> &loaded)> callback);

// Returns the characters that have saves, by folder name, as the load screen lists them.
// @return the names, or an empty vector before the game has finished starting
//
// Call it on the game thread, at the main menu or in the world.
std::vector<std::wstring> Characters();

// Returns the character that a save or load applies to now. In the world, that is the character
// being played; at the main menu, it is the one last chosen in the load screen, or else the first.
// @return the folder name, or an empty string before the game has finished starting
std::wstring Character();

// Returns a character's save slots: manual saves first, then autosaves, then quicksaves, each in
// order of number.
// @param character the character's folder name
std::vector<Slot> List(std::wstring_view character);

// Loads a save, the same way the load screen does.
// @param save the slot to load; an empty `character` means the one Character() returns
// @return true if the request went to the game, which may still refuse it (in a cutscene, for
// example); false, with no request made, if the slot holds no save, the game knows no such
// character, or the game has not finished starting
//
// Call it on the game thread, at the main menu or in the world. OnGameLoaded runs once the world
// is loaded. If the save needs content the game does not have, loading stops at the game's
// "requires the following content" dialog and goes no further until it is answered; see
// ForceLoad.
bool Load(const Slot &save);

// Answers the game's "requires the following content" dialog with Force Load, so the most
// recently requested save loads without that content.
// @return true if the game accepted the answer; false, with nothing sent, if no load was ever
// requested
//
// Call it only while a load you just requested has not started, since the game loads whatever it
// was last asked to load. A save made after a forced load no longer has the missing content. This
// is meant for tools that drive the game, not for a plugin to decide on the player's behalf.
bool ForceLoad();

// Saves the game, the same way the save screen does for a manual save and the quicksave key does
// for a quicksave.
// @param slot the slot number, or -1 for the next free one; for a manual save, that is the
// current character's lowest slot with no save in it
// @param name the save's name, shown in the load screen and used in its file name; if empty, the
// file is named "savegame"
// @return true if the game started the save; false where the game allows no saving (in combat or
// a cutscene), before the game has finished starting, or when all 500 manual slots are taken
//
// Call it on the game thread. The save is written over the next few frames, and OnSaved runs once
// it is done.
bool Save(Kind kind, std::int32_t slot = -1, std::wstring_view name = {});

} // namespace dao::saves
