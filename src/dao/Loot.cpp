// The loot panel's buttons (dao/Loot.h).
//
// Notes on the loot panel: read 2026-10-24 (DAOLib #259), after the Eluvian server's reading of
// inventory.gfx (2026-10-02, its src/game/Gui.cpp), with each handler checked in the GOG exe. The
// panel is up while the data tree's GUIInventory.ShowContainer is set, Container.ID its
// container. Close (the movie's class Container) clears ShowContainer, then sends
// "Container.InventoryClosedMessage", "GameModeController.PerformUseObject <selected> <container>
// 2" and "GUI.HideFullScreenGUI". The first reaches CCPlaceable's handler (0x0074E8D0), which
// leaves a container that takes nothing (+0x618) and is empty no longer usable, a body's bag
// handled and a coin bag destroyed, as each removal already did (dev/ITEMS.md, dev/PLACEABLES.md);
// it raises no script event and plays no animation. PerformUseObject has its one handler in
// GameModeExplore (0x0070BC60), so in the GUI mode the panel is shown in it is dropped, and it is
// left out here. HideFullScreenGUI (0x0060B2E0) ends the GUI mode, which leaves on the controller's
// next update, and pops the screen. Take All sends "GUI.SelectedChar.AcquireAllItems <container>"
// and then closes, unless items are left and Party.BackpackSize has reached
// Party.BackpackMaxSize, when the panel stays with the backpack's full notice. The engine refuses
// none of these; the panel's being shown is the movie's only condition. The container's id is read
// before the first message, which may take it from the tree.
//
// Seen in play 2026-10-24 (examples/loot), Leliana's Song's market in a fight: a crate opened by
// a use order, its panel up in the GUI mode, TakeAll from the panel's first frame took its items
// (GetItemsInInventory then empty) and closed it, the game back in combat 50 ms after the panel
// opened. With the backpack filled by script (96 of 96) and four items in the crate, TakeAll
// answered BackpackFull with the four left and the panel up, and Close then closed it.
#include "dao/Loot.h"

#include "dao/Data.h"
#include "dao/Messages.h"
#include "dao/generated/ScriptApi.h"

#include <format>

namespace dao::loot
{

namespace
{

namespace api = script::api;

void CloseShown()
{
    if (data::Attribute *shown = data::Find(L"GUIInventory.ShowContainer"); shown != nullptr)
    {
        shown->Set(false);
    }
    messages::SendText(L"Container.InventoryClosedMessage");
    messages::SendText(L"GUI.HideFullScreenGUI");
}

} // namespace

std::optional<ObjectId> Container()
{
    if (data::Number(L"GUIInventory.ShowContainer").value_or(0) == 0)
    {
        return std::nullopt;
    }
    const auto id = data::Number(L"Container.ID");
    return id ? std::optional<ObjectId>(static_cast<ObjectId>(*id)) : std::nullopt;
}

Result Close()
{
    if (!Container())
    {
        return Result::NotShown;
    }
    CloseShown();
    return Result::Closed;
}

Result TakeAll()
{
    const auto container = Container();
    if (!container)
    {
        return Result::NotShown;
    }
    messages::SendText(std::format(L"GUI.SelectedChar.AcquireAllItems {}", *container));
    const auto left = api::GetItemsInInventory(*container, script::constants::GET_ITEMS_OPTION_BACKPACK).Size();
    const double size = data::Number(L"Party.BackpackSize").value_or(0);
    const double most = data::Number(L"Party.BackpackMaxSize").value_or(0);
    if (left != 0 && most > 0 && size >= most)
    {
        return Result::BackpackFull;
    }
    CloseShown();
    return Result::Closed;
}

} // namespace dao::loot
