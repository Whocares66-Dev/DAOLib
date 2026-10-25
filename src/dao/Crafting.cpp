// Crafting's recipes and their making (dao/Crafting.h).
//
// Notes on crafting: read 2026-10-25 (DAOLib #259) in the GOG exe and crafting.gfx, with
// dev/ITEMS.md's recipes and the Eluvian server's craft tool (its src/core/Craft.cpp), which drove
// the screen and checked its two tests by hand. A recipe is a row of crafting_recipes (table 144):
// Skill, the rank of the craft it needs; Ingredient1 to 4, items' tags, with Count1 to 4; and
// ItemCreated. The party knows a recipe when its id is in the party's inventory's list (the
// inventory at CClientExoApp::GetParty's +0x20C, the list below), learned from a recipe's item as
// it reaches the party. The screen lists the known recipes of its craft (the skill's
// group of four, BaseCraftingSkillOf in Crafting.as) and lets one be made only when the crafter
// HasAbility the recipe's Skill and HaveAllIngredients holds (the party's backpack has each
// ingredient's count, Backpack::CountByTag by tag, the stacks summed). Its Create sends
// "GUICrafting.CraftItem <recipe> <crafter>", whose handler (0x00726430) calls
// Inventory::CraftRecipe (17613): known, the ingredients counted, the item made from ItemCreated,
// the ingredients taken and the item added to the backpack, which it does not test for room. The
// handler ignores its answer and raises EVENT_TYPE_CRAFT_ITEM (102) on the module either way, which
// counts a thing crafted for the achievement and the statistics; so the screen's tests are made
// here first, and the making is told by the first ingredient's count falling.
//
// Seen in play 2026-10-25 (examples/crafting), Lyna in Redcliffe village, who has Poison-Making
// (100071) alone, with the screen opened by ShowCraftingGUI: the party's 9 known recipes listed
// with the backpack's counts, the four of traps and three of herbalism not her rank; an unknown
// recipe refused Unknown, Mabari Crunch NoRank, recipe 39 (no flask) NoIngredients; with two
// flasks given by script, recipe 40 makeable and made, the deathroot 22 to 21 and the poison in the
// backpack. Called with the inventory for its this and a bare id, Contains (17611) crashed the
// game twice: its this is the list and its id an attribute, built and destroyed here as
// CraftRecipe does. With the backpack at 90 of 90 and no ingredient's stack emptied, the craft
// still made the poison, the backpack then 91 of 90: the engine adds the product past the limit,
// and the screen tests no room, so Make refuses none and loses nothing.
#include "dao/Crafting.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Gda.h"
#include "dao/Messages.h"
#include "dao/Module.h"
#include "dao/SafeRead.h"
#include "dao/generated/ScriptApi.h"

#include <array>
#include <cstddef>
#include <format>
#include <optional>

namespace dao::crafting
{

namespace
{

namespace api = script::api;
using script::Table;

constexpr std::size_t kPartyInventory = 0x20C; // in the party: its Inventory, whose list of recipes known
constexpr std::size_t kRecipesKnown = 0x38;    // in the inventory: the attribute of the recipes it knows
constexpr std::size_t kAttributeValue = 0xC;   // in an attribute: its value
constexpr std::size_t kAttributeSize = 0x10;   // an int attribute: two vtables, a flag, its value
constexpr std::int32_t kMostIngredients = 4;

// The ranks of each craft, as the screen's BaseCraftingSkillOf has them.
constexpr std::array<std::pair<Craft, std::int32_t>, 4> kFirstRank{{
    {Craft::Traps, 100041},
    {Craft::Herbalism, 100061},
    {Craft::Poisons, 100071},
    {Craft::Runes, 410000},
}};
constexpr std::int32_t kRanks = 4;

std::optional<Craft> CraftOf(std::int32_t skill)
{
    for (const auto &[craft, first] : kFirstRank)
    {
        if (skill >= first && skill < first + kRanks)
        {
            return craft;
        }
    }
    return std::nullopt;
}

std::uintptr_t Inventory()
{
    CClientExoApp *app = CClientExoApp::Get();
    if (app == nullptr)
    {
        return 0;
    }
    using PartyFn = std::uintptr_t(__fastcall *)(CClientExoApp * app, int edx);
    const std::uintptr_t party = reinterpret_cast<PartyFn>(Module::Address(addr::kCClientExoApp_GetParty))(app, 0);
    return party != 0 ? TryRead<std::uintptr_t>(party + kPartyInventory).value_or(0) : 0;
}

// Whether the party knows a recipe, by CraftRecipe's own test (0x0073FE45 to 0x0073FE6B): the id
// wrapped as an int attribute by its constructor (119), ECBase::ECDynArray::Contains (17611) on the
// list, the inventory's attribute at +0x38 whose vector is its value at +0xC (0x00403C70), and the
// wrapper's destructor (143). The wrapper holds no memory of its own; its bytes are its whole.
bool Knows(std::int32_t recipe)
{
    const std::uintptr_t inventory = Inventory();
    if (inventory == 0)
    {
        return false;
    }
    using MakeFn = void(__fastcall *)(void *attribute, int edx, const std::int32_t *value);
    using ContainsFn = bool(__fastcall *)(std::uintptr_t list, int edx, const void *attribute);
    using EndFn = void(__fastcall *)(void *attribute, int edx);
    alignas(4) std::array<std::byte, kAttributeSize> wrapped{};
    reinterpret_cast<MakeFn>(Module::Address(addr::kSmartDataAttributeInt_Attribute))(wrapped.data(), 0, &recipe);
    const bool known = reinterpret_cast<ContainsFn>(Module::Address(addr::kECDynArray_Contains))(
        inventory + kRecipesKnown + kAttributeValue, 0, wrapped.data());
    reinterpret_cast<EndFn>(Module::Address(addr::kSmartDataAttributeInt_Destroy))(wrapped.data(), 0);
    return known;
}

// A row as the screen shows it to a crafter; the backpack's counts are the party's.
Recipe Read(std::int32_t row, ObjectId crafter)
{
    Recipe recipe;
    recipe.id = row;
    recipe.skill = gda::GetInt(Table::Crafting, row, "Skill").value_or(0);
    recipe.product = gda::GetString(Table::Crafting, row, "ItemCreated").value_or(L"");
    const ObjectId hero = api::GetHero();
    recipe.ingredientsHeld = true;
    for (std::int32_t i = 1; i <= kMostIngredients; ++i)
    {
        const std::int32_t need = gda::GetInt(Table::Crafting, row, std::format("Count{}", i)).value_or(0);
        const std::wstring tag = gda::GetString(Table::Crafting, row, std::format("Ingredient{}", i)).value_or(L"");
        if (need <= 0 || tag.empty())
        {
            continue;
        }
        const std::int32_t have = api::CountItemsByTag(hero, tag);
        recipe.ingredients.push_back({tag, need, have});
        recipe.ingredientsHeld = recipe.ingredientsHeld && have >= need;
    }
    recipe.rank = api::HasAbility(crafter, recipe.skill) != 0;
    recipe.makeable = recipe.rank && recipe.ingredientsHeld;
    return recipe;
}

} // namespace

std::vector<Recipe> Recipes(Craft craft, ObjectId crafter)
{
    std::vector<Recipe> recipes;
    const auto table = static_cast<std::int32_t>(Table::Crafting);
    const std::int32_t rows = api::GetM2DARows(table);
    for (std::int32_t index = 0; index < rows; ++index)
    {
        const std::int32_t row = api::GetM2DARowIdFromRowIndex(table, index);
        if (CraftOf(gda::GetInt(Table::Crafting, row, "Skill").value_or(0)) == craft && Knows(row))
        {
            recipes.push_back(Read(row, crafter));
        }
    }
    return recipes;
}

Refusal Make(std::int32_t recipe, ObjectId crafter)
{
    if (!Knows(recipe))
    {
        return Refusal::Unknown;
    }
    const Recipe read = Read(recipe, crafter);
    if (!read.rank)
    {
        return Refusal::NoRank;
    }
    if (!read.ingredientsHeld || read.ingredients.empty())
    {
        return Refusal::NoIngredients;
    }
    const ObjectId hero = api::GetHero();
    const std::int32_t before = api::CountItemsByTag(hero, read.ingredients[0].tag);
    messages::SendText(std::format(L"GUICrafting.CraftItem {} {}", recipe, crafter));
    return api::CountItemsByTag(hero, read.ingredients[0].tag) < before ? Refusal::None : Refusal::NotMade;
}

} // namespace dao::crafting
