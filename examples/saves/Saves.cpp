// Example: a plugin's own data, kept with the game's saves (dao/Saves.h).
//
// The plugin counts the saves made in a playthrough. The count belongs to
// the playthrough, not to the session: load an older save and the count
// goes back to what it was when that save was made, start a new game and
// it starts again from nothing. So it is written with each save and read
// back from the save being loaded, the way SKSE's co-saves keep a
// plugin's data with a Skyrim save.
//
// Two callbacks do all of it. OnSaved writes the count into the save just
// made (a file of the plugin's own in the save's folder). OnGameLoaded
// reads it back when a playthrough is in the world, from the save that was
// loaded; a new game (no save) or a save from before the plugin (no file)
// starts from nothing. Both run on the game's thread, one at a time, so
// the count needs no lock.
//
// The file is the plugin's own to fill: here the count as text. Parsing it
// with std::from_chars, not std::stoi, since a callback must not throw: an
// exception has no way back through the game's code. What the plugin logs
// goes to example_saves.log.
#include "dao/Saves.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"

#include <spdlog/spdlog.h>

#include <charconv>
#include <optional>
#include <string>

namespace
{

// The saves made in the playthrough loaded now.
int g_saves = 0;

void Loaded(const std::optional<dao::saves::Slot> &save)
{
    g_saves = 0;
    if (!save)
    {
        spdlog::info("a new game: counting from nothing");
        return;
    }
    const auto text = save->Read("example_saves");
    if (text)
    {
        std::from_chars(text->data(), text->data() + text->size(), g_saves);
    }
    spdlog::info("loaded {}'s {} save, slot {}: {} saves made up to it{}", dao::Utf8(save->character),
                 dao::saves::Name(save->kind), save->number, g_saves, text ? "" : " (no file of ours)");
}

void Saved(const dao::saves::Slot &save)
{
    ++g_saves;
    const bool kept = save.Write("example_saves", std::to_string(g_saves));
    spdlog::info("{}'s save {} ({} save, slot {}): {}", dao::Utf8(save.character), g_saves, dao::saves::Name(save.kind),
                 save.number, kept ? "kept with it" : "could not be kept");
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
    dao::saves::OnGameLoaded(&Loaded);
    dao::saves::OnSaved(&Saved);
    return true;
}
