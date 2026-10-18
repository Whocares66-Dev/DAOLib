// A creature's models as the engine assembles them, from its template and
// its items through the game's data tables, over bytes (dev/CHARACTERS.md):
// a humanoid's body parts, the bare body where a slot is empty, the helmet
// and what it hides of the head, the weapons and the nodes they hang from,
// or a creature's one model where it is not built from parts.
//
//   const auto creature = dao::core::character::TemplateOf(utc);
//   const auto parts = dao::core::character::Resolve(*creature, itemOf, tableOf);
//   // parts->body[0].model == L"hm_arm_medd"; add "_0" for the model's file
//
// The head's own models (face, eyes, hair, beard, lashes) are its morph's
// (dao/core/Morph.h, the template's HeadMorph); this says which of them a
// helmet leaves. Model names are lower case and without the level of
// detail's "_0".
#pragma once

#include "dao/core/Gff3.h"
#include "dao/core/M2da.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::character
{

// The scripts' INVENTORY_SLOT_* constants, as a template's equipment list numbers its items.
inline constexpr std::int32_t kInventoryMain = 0;
inline constexpr std::int32_t kInventoryOffhand = 1;
inline constexpr std::int32_t kInventoryAmmo = 2;
inline constexpr std::int32_t kInventoryChest = 4;
inline constexpr std::int32_t kInventoryHead = 5;
inline constexpr std::int32_t kInventoryBoots = 6;
inline constexpr std::int32_t kInventoryGloves = 7;
inline constexpr std::int32_t kInventoryCloak = 8;

// The engine's appearance slots, which the parts and weapons below are in.
enum class Slot : std::int32_t
{
    Chest = 0,
    Gloves = 1,
    Boots = 2,
    Main = 3,
    Offhand = 4,
    Helmet = 5,
    Cloak = 6,
};

// Returns the appearance slot an inventory slot's item goes in; nothing for one that is not drawn
// (a ring, a belt).
std::optional<Slot> SlotOf(std::int32_t inventorySlot);

// What a creature template (.utc) says of how the creature looks.
struct Template
{
    std::int32_t appearance = 0; // its row of apr_base (table 2)
    std::uint8_t gender = 1;     // 1 male, 2 female, 3 other
    std::string headMorph;       // a .mor, with its extension; empty for none
    // Its tints and colours as written, rows of the tint tables the morph's own arrays come from.
    std::int32_t hairColor = 0;
    std::int32_t skinTint = 0;
    std::int32_t eyesColor = 0;
    std::int32_t bodyTint = 0;
    std::int32_t bodyTintMask = 0;
    std::int32_t tattooTint = 0;
    std::int32_t tattoo = 0;

    struct Equipped
    {
        std::int32_t inventorySlot = 0; // the list entry's struct id
        std::string item;               // the item template's (.uti) name, without its extension
    };
    std::vector<Equipped> equipment;
};

// Returns what a creature template says of the creature's looks, or nothing for a file that is
// not one.
std::optional<Template> TemplateOf(const gff3::File &utc);

// What an item template (.uti) says of how the item looks.
struct Item
{
    std::int32_t baseItem = 0;  // its row of bitm_base (table 6)
    std::int32_t variation = 0; // its row of its base type's variation worksheet
    std::int32_t material = 0;
    std::int32_t materialProgress = 0;
    std::int32_t bodyTint = 0;
    std::int32_t bodyTintMask = 0;
    std::int32_t tintOverride = 0;
};

// Returns what an item template says of the item's looks, or nothing for a file that is not one.
std::optional<Item> ItemOf(const gff3::File &uti);

// One of a humanoid's body parts: its model, and the item it is of.
struct Part
{
    Slot slot = Slot::Chest;
    std::wstring model;       // "hm_arm_medd"
    std::optional<Item> item; // none for the bare body (naked_variation)
};

// A weapon or a shield, and where on the body it hangs.
struct Weapon
{
    Slot slot = Slot::Main;
    std::wstring model; // "w_lsw_ls03a"
    Item item;
    std::wstring sheathed; // the node it hangs from sheathed, as it is first hung: "Prop10"
    std::wstring drawn;    // and drawn, as an equip event moves it: "Prop01"
    // How far it is moved off the body along the creature's +Y, where the node's weaponparts row
    // asks for it (the back): sheathedOffset and drawnOffset say which; 0 where it is not.
    float offset = 0;
    bool sheathedOffset = false;
    bool drawnOffset = false;
};

// What a helmet leaves of the head.
struct Head
{
    std::wstring neck;     // apr_base's Neck_Model with the prefix; empty for none
    std::wstring helmet;   // empty for none
    bool fullFace = false; // the helmet's worksheet row's FullFace: only the eyes are drawn
    bool face = true;
    bool hair = true; // false under any helmet
    bool beard = true;
    bool lashes = true;
};

struct Character
{
    // True for a humanoid built from parts (apr_base's ModelType starting with P); false for a
    // creature of one model.
    bool parts = false;
    std::wstring model;     // a creature of one model's: apr_base's ModelName
    std::wstring prefix;    // race and gender letters before a part's name: "hm"
    std::vector<Part> body; // chest, gloves and boots, as many as are drawn
    std::vector<Weapon> weapons;
    Head head;
    std::string headMorph; // the template's
};

// Returns a data table by its number in m2da_base (2 apr_base, 6 bitm_base, 80 naked_variation,
// 91 weaponparts, 92 gender and the variation worksheets), merged as the game merges it; null
// for none.
using TableOf = std::function<const m2da::Merged *(std::int64_t table)>;

// Returns an item template by its name; nothing if there is none.
using ItemOfName = std::function<std::optional<Item>(std::string_view item)>;

// Returns a creature's models as the engine assembles them.
// @return nothing if the template's appearance has no row of apr_base
std::optional<Character> Resolve(const Template &creature, const ItemOfName &itemOf, const TableOf &tableOf);

// Returns the eyes' model a full-face helmet swaps in: a 2 before its last letter, cn_eye_glka
// becoming cn_eye_glk2a.
std::wstring FullFaceEyes(std::wstring_view eyes);

} // namespace dao::core::character
