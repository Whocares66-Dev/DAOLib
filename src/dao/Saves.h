// A plugin's own data with the game's saves, as SKSE's co-saves keep a
// plugin's beside a Skyrim save: a file of the plugin's own in each save's
// folder, written when the game saves and read when a playthrough is
// loaded.
//
//   std::string g_notes;   // the plugin's state, for the playthrough loaded now
//
//   dao::saves::OnGameLoaded([](const std::optional<dao::saves::Slot> &save) {
//       // Nothing for a new game, or a save from before the plugin.
//       g_notes = save ? save->Read("myplugin").value_or("") : "";
//   });
//   dao::saves::OnSaved([](const dao::saves::Slot &save) { save.Write("myplugin", g_notes); });
//
// examples/saves/ is a whole plugin that does this.
//
// And asking the game to save or load, as its menus and keys do, for a
// tool that drives the game (a test harness loading the same save each
// run):
//
//   dao::saves::Load({L"Aedan", 1, dao::saves::Kind::Manual}); // OnGameLoaded says when it is in
//   dao::saves::Save(dao::saves::Kind::Quick);                  // OnSaved says when it is written
//
// What goes in the file is the plugin's own: text, JSON, binary. Each
// plugin has its own file per save, <slot folder>\<plugin>.daolib, so
// there is nothing to share and no framing; a plugin that may change its
// format keeps a version in it. Both callbacks run on the game's thread,
// one at a time, as the game's own saving and loading do, so plugin state
// they share needs no lock. They must not throw: an exception has no way
// back through the game's code.
//
// Read 2026-09-30 (dev/SAVES.md):
//
// - Saving. SaveGame::Write (dao/Addresses.h) writes a save with the
//   character, the slot and the kind resolved; in play a quicksave asked
//   for "any slot" and Write was called for slot 1, kind 2, and returned
//   1. OnSaved runs after Write returns and has succeeded.
// - Loading. A load looks for its file with SaveGame::FindSave, with the
//   same three (in play once per load), before the game reads the save;
//   the world is built after. OnGameLoaded runs at the module's load
//   event (EVENT_TYPE_MODULE_LOAD) that follows, with the save's slot, the
//   world there. The event alone is not enough: BioWare's module_core.nss
//   says it "can fire more than once for a single module", so a load
//   event with no save found before it is not a load and calls nothing.
// - A new game. module_core.nss: EVENT_TYPE_MODULE_START happens "only
//   once for a single game instance". OnGameLoaded runs there with no
//   slot, so a plugin starts a new playthrough from nothing rather than
//   from the one loaded before it.
//
// Seen in play: Write and FindSave with their slots, and the load event
// once per load. Not yet seen: the module's start event for a new game,
// and this file's callbacks themselves. One gap: an early quicksave in
// play wrote its files without Write being called, for a reason not read,
// and such a save would not be heard. A save deleted from the game's menu
// takes its folder with it, and the plugin's file too.
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

// The scripts have no constants for these; the engine's checks and the
// folder names say what they are.
enum class Kind : std::int32_t
{
    Manual = 0, // Slot_<n>, a save the player named or chose
    Auto = 1,   // AutoSave_<n>, 1 to 4
    Quick = 2,  // QuickSave_<n>
    Other = 3,  // a fourth kind the checks allow, not seen
};

// "manual", "auto", "quick" or "other", for logs.
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

struct Slot
{
    std::wstring character; // the character's folder, "Aedan"
    std::int32_t number = 0;
    Kind kind = Kind::Manual;

    // The plugin's data in this save, the whole file; nothing if it has
    // none (a save from before the plugin, or one it never wrote to).
    std::optional<std::string> Read(std::string_view plugin) const;
    // Writes the plugin's data for this save, replacing any. The file is
    // written beside and then moved into place, so a save never holds half
    // of one. False if it could not be written.
    bool Write(std::string_view plugin, std::string_view data) const;

    // The file itself, <slot folder>\<plugin>.daolib, for a plugin that
    // would rather stream it.
    std::filesystem::path File(std::string_view plugin) const;
    // The slot's folder, as the game names it (SaveGame::SlotFolder):
    // <documents>\BioWare\Dragon Age\Characters\<character>\Saves\
    // <Slot_|AutoSave_|QuickSave_><n>. The engine's own answer, so it
    // follows wherever the game keeps its saves.
    std::filesystem::path Folder() const;

    // Whether there is a save in the slot: a .das in its folder.
    bool Exists() const;
    // The save's metadata (its .das.met), what the load screen shows of
    // it: the area, the time played, the character's level and the save's
    // name. Nothing if the slot has none, or it cannot be read.
    std::optional<core::SaveMeta> Meta() const;
};

// After the game has written a save, with its slot.
void OnSaved(std::function<void(const Slot &saved)> callback);

// When a playthrough is in the world: after a save has loaded, with the
// slot it came from, or when a new game starts, with none.
void OnGameLoaded(std::function<void(const std::optional<Slot> &loaded)> callback);

// Asking the game to save or load: the messages its menus and keys send to
// its save proxy (SaveLoadProxy, the application's +0x150; dev/SAVES.md),
// read 2026-09-30. From the main menu or in the world, on the game's thread
// (a frame callback). Not yet seen in play.

// The characters the game has saves for, by their folders' names, as its
// load screen lists them. Read from the save proxy's list, which the game
// fills from the folders when its load screen opens; asking refreshes it
// first if it is empty.
std::vector<std::wstring> Characters();

// The character a save or load means now: the one played, in the world;
// at the main menu, the one the load screen chose last, else the first.
std::wstring Character();

// The slots a character has saves in, from its folders, manual saves
// first, then autosaves and the quicksave, each by number.
std::vector<Slot> List(std::wstring_view character);

// Loads a save, as the load screen does: the load screen's mode set, the
// character chosen, then LoadGameMessage. False, and nothing asked, if the
// slot has no save, the game knows no such character, or the application
// is not up; true if the game was asked, which it may still decline (in a
// cutscene, say). When the world is there, OnGameLoaded is called with the
// slot. A character's own name chooses it, empty the one Character() says.
bool Load(const Slot &save);

// Saves, as the save screen does for a manual save and the quicksave key
// for the quicksave: SaveGameMessage. A slot of -1 lets the game choose (the
// next free manual slot; the quicksave and the autosave have their own),
// and an empty name names the file "savegame". True if the game began the
// save; it is written over the next frames, and OnSaved is called once it
// is. False where the game allows no saving (in combat, in a cutscene) or
// before the application is up.
bool Save(Kind kind, std::int32_t slot = -1, std::wstring_view name = {});

} // namespace dao::saves
