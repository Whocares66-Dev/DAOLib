// A store, as its screen buys and sells: what it stocks and at what price, what it pays for the
// party's items, a buy or a sale refused where the screen or the store refuses it, and the screen
// closed.
//
//   if (const auto store = dao::store::Current())
//       for (const auto &entry : dao::store::Stock())
//           if (entry.price <= dao::store::Money())
//               dao::store::Buy(entry.item);
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace dao::store
{

// Returns the store whose screen is up.
// @return nullopt unless the store's screen is the full screen shown
//
// Call it, and the rest, on the game thread.
std::optional<ObjectId> Current();

// Returns the party's money, in coppers.
std::int32_t Money();

// An item the store stocks, with what one costs.
struct Entry
{
    ObjectId item = kInvalidObject;
    std::int32_t price = 0; // for one, as the screen shows it
};

// Returns what the open store stocks, in its order.
std::vector<Entry> Stock();

// Returns what the open store charges for some of an item it stocks: more for an item the party
// has just sold it is bought back at what was paid. @return 0 with no store open
std::int32_t BuyPrice(ObjectId item, std::int32_t count = 1);

// Returns what the open store pays for some of an item. @return 0 with no store open
std::int32_t SellPrice(ObjectId item, std::int32_t count = 1);

// Why a buy or a sale was refused, or None if it went.
enum class Refusal : std::uint8_t
{
    None,
    NoStore,      // no store's screen is up
    NotOffered,   // the store does not stock the item, or the party's backpack does not hold it
    BadCount,     // the count is below 1
    PlotItem,     // an item the story needs, which the screen never offers for sale
    TooExpensive, // the party has not the money
    BackpackFull, // the party's backpack had no room, and the store kept the item
    StoreWontBuy, // the store buys no such item, has not the money, or pays no more than that
};

// Buys some of an item the open store stocks into the party's backpack, as the screen does.
Refusal Buy(ObjectId item, std::int32_t count = 1);

// Sells some of an item from the party's backpack to the open store, as the screen does.
Refusal Sell(ObjectId item, std::int32_t count = 1);

// Closes the store's screen as its close does: the store settles, and an item sold can no longer
// be bought back at what was paid. Does nothing with no store's screen up.
void Close();

} // namespace dao::store
