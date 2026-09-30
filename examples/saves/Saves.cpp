// Example: a plugin's own data, kept with the game's saves (dao/Saves.h).
//
// The plugin counts the saves made in a playthrough. The count belongs to
// the playthrough, not to the session: load an older save and the count
// goes back to what it was when that save was made. So it is written
// beside each save and read back from the save being loaded, the way
// SKSE's co-saves keep a plugin's data with a Skyrim save.
//
// - OnSaved runs after the game has written a save, with the slot it
//   wrote. The plugin writes its count to its own file in the slot's
//   folder: <slot folder>\example_saves.daolib.
// - OnLoading runs when the game is about to read a save. The plugin reads
//   its file from that slot's folder. A save made before the plugin was
//   installed has no file, and the count starts from nothing.
//
// Both run on the game's thread, one at a time, so the count needs no
// lock. What it logs goes to example_saves.log in the daolib log folder.
#include "dao/Saves.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <fstream>

namespace
{

// The saves made in the playthrough loaded now.
int g_saves = 0;

void Saved(const dao::saves::Slot &slot)
{
    ++g_saves;
    const auto file = slot.File("example_saves");
    std::ofstream(file) << g_saves << '\n';
    spdlog::info("{}'s save {} ({} save, slot {}): wrote {}", dao::Utf8(slot.character), g_saves,
                 dao::saves::Name(slot.kind), slot.number, dao::Utf8(file.wstring()));
}

void Loading(const dao::saves::Slot &slot)
{
    const auto file = slot.File("example_saves");
    std::ifstream in(file);
    int saves = 0;
    if (in >> saves)
    {
        spdlog::info("loading {}'s {} save, slot {}: {} saves made up to it", dao::Utf8(slot.character),
                     dao::saves::Name(slot.kind), slot.number, saves);
    }
    else
    {
        spdlog::info("loading {}'s {} save, slot {}: no file of ours, a save from before this plugin",
                     dao::Utf8(slot.character), dao::saves::Name(slot.kind), slot.number);
    }
    g_saves = saves;
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_saves");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->expectedExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::saves::OnSaved(&Saved);
    dao::saves::OnLoading(&Loading);
    return true;
}
