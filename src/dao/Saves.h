// A plugin's own data with the game's saves, as SKSE's co-saves keep a
// plugin's beside a Skyrim save: the plugin hears when the game has
// written a save and when it is about to load one, with the save's slot,
// and keeps a file of its own in the slot's folder.
//
//   dao::saves::OnSaved([](const dao::saves::Slot &slot) {
//       std::ofstream(slot.File("myplugin")) << state;
//   });
//   dao::saves::OnLoading([](const dao::saves::Slot &slot) {
//       std::ifstream in(slot.File("myplugin"));   // may not exist
//       ...
//   });
//
// Read 2026-09-30 (dev/SAVES.md). A save is written by SaveGame::Write
// (dao/Addresses.h), with the character, the slot and the kind resolved;
// in play the same day a quicksave asked for "any slot" and Write was
// called for slot 1, kind 2, and returned 1. OnSaved runs after Write
// returns and has succeeded, so the slot's folder holds the new save. A
// load looks for its file with SaveGame::FindSave, with the same three, in
// play once per load; OnLoading runs then, before the game reads the save.
// The world is not built yet: read the file there, and apply what it says
// once the module's load event arrives (dao/Events.h,
// EVENT_TYPE_MODULE_LOAD). Both run on the game's thread.
//
// Two gaps. One quicksave in play (the eighth session) wrote its files
// without Write being called, for a reason not read, and such a save
// would not be heard. And a save deleted from the game's menu takes the
// slot's folder with it (a recursive walk over it, 0x004C9B10), a
// plugin's file too, which is as it should be. Not yet seen in play.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

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

    // The slot's folder, as the game makes it (SaveGame::SlotFolder):
    // <documents>\BioWare\Dragon Age\Characters\<character>\Saves\
    // <Slot_|AutoSave_|QuickSave_><n>. The engine's own answer, so it
    // follows where the game keeps its saves.
    std::filesystem::path Folder() const;

    // A plugin's file in it: <folder>\<plugin>.daolib.
    std::filesystem::path File(std::string_view plugin) const;
};

using Observer = std::function<void(const Slot &slot)>;

void OnSaved(Observer observer);
void OnLoading(Observer observer);

} // namespace dao::saves
