// Crafting as its screen does it, with or without the screen: the party's recipes of a craft with
// what each needs and has, whether a crafter could make one, and one made, refused where the
// screen greys it out.
//
//   for (const auto &recipe : dao::crafting::Recipes(dao::crafting::Craft::Herbalism, wynne))
//       if (recipe.makeable)
//           dao::crafting::Make(recipe.id, wynne);
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <string>
#include <vector>

namespace dao::crafting
{

// The crafts, as the screen groups their recipes.
enum class Craft : std::uint8_t
{
    Traps,
    Herbalism,
    Poisons,
    Runes,
};

// An ingredient of a recipe, and how many of it the party's backpack holds.
struct Ingredient
{
    std::wstring tag;  // the item's tag
    std::wstring name; // as the screen shows it: the item's own name; empty for a tag no template has
    std::int32_t need = 0;
    std::int32_t have = 0;
};

// A recipe the party knows, as the screen shows it to a crafter.
struct Recipe
{
    std::int32_t id = 0;      // its row of crafting_recipes
    std::int32_t skill = 0;   // the rank of its craft it needs: an ability, 100062 for Improved Herbalism
    std::wstring name;        // as the screen shows it: its text, or "?" and its label where it has none
    std::wstring product;     // the item it makes, its template's name: "gen_im_qck_dogfood_101.uti"
    std::wstring productName; // the item's own name, as the screen shows it
    std::vector<Ingredient> ingredients;
    bool rank = false;            // the crafter has the skill it needs
    bool ingredientsHeld = false; // the backpack holds all it needs
    bool makeable = false;        // both: the screen shows it in black and lets it be made
};

// Returns the recipes of a craft the party knows, in the table's order, as the screen shows them
// to a crafter. Call it, and Make, on the game thread, in a module.
std::vector<Recipe> Recipes(Craft craft, ObjectId crafter);

// Why a recipe was not made, or None if it was.
enum class Refusal : std::uint8_t
{
    None,
    Unknown,       // the party does not know the recipe, or there is no such recipe
    NoRank,        // the crafter has not the skill it needs
    NoIngredients, // the backpack lacks an ingredient
    NotMade,       // the engine made nothing of it
};

// Makes one of a recipe as the screen's Create does: its ingredients leave the party's backpack and
// the item goes in it, and the module counts a thing crafted. Refused where the screen greys it out.
Refusal Make(std::int32_t recipe, ObjectId crafter);

} // namespace dao::crafting
