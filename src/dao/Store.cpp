// A store's buying and selling (dao/Store.h).
//
// Notes on the store: read 2026-10-25 (DAOLib #259) in the GOG exe and store.gfx, with
// dev/ITEMS.md's "Who prices what" and dev/script_commands/stores.md, and the Eluvian server's store
// tool (its src/game/Store.cpp), which sent the screen's texts and priced by its own sum. The
// screen sends "Store.BuyItem <buyer> <item> <count>" and "Store.SellItem <seller> <item> <count>",
// the store being the data tree's Store, bound to the store object by OpenStore (0x007EB9D0); it
// refuses nothing itself but greys an entry the party cannot pay for, and offers only the party's
// backpack for sale, never the plot items' list. BuyItem's handler (0x007ED340) checks only the
// price against the party's money (Store::BuyItemFrom, 0x007ECF00) and, with the backpack full,
// keeps the item and flashes the screen's full notice; SellItem's (0x007EC740) only the store's
// own rules (Store::WillBuy, 21206: WillNotBuy and WillOnlyBuy by the item's base row, the store's
// money and MaxBuyPrice against the total). Neither looks at where the item is or its plot flag,
// so those are checked here, as the screen's lists keep them. A price is the store's own
// Store::GetBuyPrice (21193) or GetSellPrice (21192), trunc(pct / 100 x the item's Cost) x count,
// pct MarkUp or MarkDown by whether the item was just traded (+0x5C0): the party's own sale is
// bought back at what it fetched. The screen's close sends CloseStore (0x007EC8B0), which ends
// that. A buy or a sale is known to have gone by the party's money, as the screen tells it.
//
// Seen in play 2026-10-25 (examples/store), Lyna in Redcliffe village with Owen's store opened by
// OpenStore: 20 items stocked at the screen's prices (Plate Armor 6500, a Metal Shard 130); Buy of
// 0 refused BadCount, Sell of the store's own item NotOffered, a plot item PlotItem, 1000 of the
// dearest TooExpensive; the shard bought for 130 (the money 260903 to 260773), sold back for 130
// as just bought (to 260903 again), and Close closed the screen. With the backpack filled by
// script (90 of 90), Buy answered BackpackFull and the money stayed. With the store's
// MaxBuyPrice set to 10 by script, a sale worth 12 was refused StoreWontBuy by WillBuy, the money
// staying; SetStoreMoney does nothing to a store (dev/script_commands/stores.md), so the store's
// own money was not tried.
#include "dao/Store.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Data.h"
#include "dao/Gui.h"
#include "dao/Messages.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/generated/ScriptApi.h"

#include <algorithm>

namespace dao::store
{

namespace
{

namespace api = script::api;

CGameObject *Object(ObjectId id)
{
    CClientExoApp *app = CClientExoApp::Get();
    return app != nullptr ? app->GetGameObjectById(id) : nullptr;
}

CCItem *ItemOf(ObjectId id)
{
    CGameObject *object = Object(id);
    return object != nullptr ? object->AsItem() : nullptr;
}

// The open store's object.
CGameObject *Shown()
{
    const auto id = Current();
    return id ? Object(*id) : nullptr;
}

bool Holds(ObjectId holder, ObjectId item)
{
    const auto items = api::GetItemsInInventory(holder, script::constants::GET_ITEMS_OPTION_BACKPACK);
    for (std::size_t i = 0; i < items.Size(); ++i)
    {
        if (items.Object(i) == item)
        {
            return true;
        }
    }
    return false;
}

std::int32_t Price(AddressId price, CGameObject *store, ObjectId item, std::int32_t count)
{
    CCItem *found = ItemOf(item);
    if (store == nullptr || found == nullptr)
    {
        return 0;
    }
    using PriceFn = std::int32_t(__fastcall *)(CGameObject * store, int edx, CCItem *item, std::int32_t count);
    return reinterpret_cast<PriceFn>(Module::Address(price))(store, 0, found, count);
}

} // namespace

std::optional<ObjectId> Current()
{
    const auto full = gui::CurrentFullScreen();
    if (!full || full->name != L"Store")
    {
        return std::nullopt;
    }
    const auto id = data::Number(L"Store.ID");
    return id ? std::optional<ObjectId>(static_cast<ObjectId>(*id)) : std::nullopt;
}

std::int32_t Money()
{
    return static_cast<std::int32_t>(data::Number(L"Party.Money.Total").value_or(0));
}

std::vector<Entry> Stock()
{
    std::vector<Entry> stock;
    CGameObject *store = Shown();
    const auto id = Current();
    if (store == nullptr || !id)
    {
        return stock;
    }
    const auto items = api::GetItemsInInventory(*id, script::constants::GET_ITEMS_OPTION_BACKPACK);
    for (std::size_t i = 0; i < items.Size(); ++i)
    {
        const ObjectId item = items.Object(i);
        stock.push_back({item, Price(addr::kStore_GetBuyPrice, store, item, 1)});
    }
    return stock;
}

std::int32_t BuyPrice(ObjectId item, std::int32_t count)
{
    return Price(addr::kStore_GetBuyPrice, Shown(), item, count);
}

std::int32_t SellPrice(ObjectId item, std::int32_t count)
{
    return Price(addr::kStore_GetSellPrice, Shown(), item, count);
}

Refusal Buy(ObjectId item, std::int32_t count)
{
    CGameObject *store = Shown();
    const auto id = Current();
    if (store == nullptr || !id)
    {
        return Refusal::NoStore;
    }
    if (count < 1)
    {
        return Refusal::BadCount;
    }
    if (!Holds(*id, item))
    {
        return Refusal::NotOffered;
    }
    const std::int32_t before = Money();
    if (Price(addr::kStore_GetBuyPrice, store, item, count) > before)
    {
        return Refusal::TooExpensive;
    }
    auto message = messages::Make<messages::BuyItemMessage>();
    message.buyer = api::GetHero();
    message.item = item;
    message.quantity = count;
    messages::Send(store, message);
    return Money() != before ? Refusal::None : Refusal::BackpackFull;
}

Refusal Sell(ObjectId item, std::int32_t count)
{
    CGameObject *store = Shown();
    if (store == nullptr)
    {
        return Refusal::NoStore;
    }
    if (count < 1)
    {
        return Refusal::BadCount;
    }
    const ObjectId hero = api::GetHero();
    if (!Holds(hero, item))
    {
        return Refusal::NotOffered;
    }
    if (api::IsPlot(item) != 0)
    {
        return Refusal::PlotItem;
    }
    CCItem *found = ItemOf(item);
    CClientExoApp *app = CClientExoApp::Get();
    CCCreature *seller = app != nullptr ? app->GetCreatureById(hero) : nullptr;
    using WillBuyFn =
        bool(__fastcall *)(CGameObject * store, int edx, CCItem *item, CCCreature *seller, std::int32_t count);
    if (found == nullptr || seller == nullptr ||
        !reinterpret_cast<WillBuyFn>(Module::Address(addr::kStore_WillBuy))(store, 0, found, seller, count))
    {
        return Refusal::StoreWontBuy;
    }
    const std::int32_t before = Money();
    auto message = messages::Make<messages::SellItemMessage>();
    message.seller = hero;
    message.item = item;
    message.quantity = count;
    messages::Send(store, message);
    return Money() != before ? Refusal::None : Refusal::StoreWontBuy;
}

void Close()
{
    if (Current())
    {
        messages::SendText(L"GUI.HideFullScreenGUI");
    }
}

} // namespace dao::store
