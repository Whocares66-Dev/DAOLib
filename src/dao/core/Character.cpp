#include "dao/core/Character.h"

#include "dao/core/GdaHash.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <variant>

namespace dao::core::character
{

namespace
{

// The tables by their numbers in m2da_base, and the base types the body's
// builder knows by ID (dev/CHARACTERS.md, 0x007480B0 and 0x007480C0).
constexpr std::int64_t kAppearances = 2;
constexpr std::int64_t kItems = 6;
constexpr std::int64_t kNakedVariations = 80;
constexpr std::int64_t kHeadTintMasks = 81;
constexpr std::int64_t kBodyTintMasks = 82;
constexpr std::int64_t kEyeTints = 83;
constexpr std::int64_t kHairTints = 84;
constexpr std::int64_t kSkinTints = 85;
constexpr std::int64_t kTattooTints = 86;
constexpr std::int64_t kBodyTints = 88;
constexpr std::int64_t kWeaponParts = 91;
constexpr std::int64_t kGenders = 92;
constexpr std::int32_t kClothing = 38;
constexpr std::int32_t kMassiveNoBoots = 22;
// A female's own WeaponOffset counts a fifth, and no chest counts as one of
// 0.02 (dev/CHARACTERS.md, 0x00A66B70).
constexpr float kFemaleOffset = 0.2F;
constexpr float kNoChestOffset = 0.02F;
constexpr float kNoOffset = 1e-6F;

const m2da::Row *RowOf(const m2da::Merged *table, std::int64_t id)
{
    if (table == nullptr)
    {
        return nullptr;
    }
    const auto found =
        std::ranges::find_if(table->rows, [&](const m2da::Row &row) { return row.taken && row.id == id; });
    return found != table->rows.end() ? &*found : nullptr;
}

const GdaFile::Cell *CellOf(const m2da::Merged &table, const m2da::Row &row, std::string_view column)
{
    const std::uint32_t hash = GdaColumnHash(column);
    for (std::size_t i = 0; i < table.columns.size() && i < row.cells.size(); ++i)
    {
        if (table.columns[i].hash == hash)
        {
            return &row.cells[i];
        }
    }
    return nullptr;
}

std::wstring Text(const m2da::Merged *table, const m2da::Row *row, std::string_view column)
{
    const GdaFile::Cell *cell = table != nullptr && row != nullptr ? CellOf(*table, *row, column) : nullptr;
    const auto *text = cell != nullptr ? std::get_if<std::wstring>(cell) : nullptr;
    return text != nullptr ? *text : std::wstring();
}

double Number(const m2da::Merged *table, const m2da::Row *row, std::string_view column)
{
    const GdaFile::Cell *cell = table != nullptr && row != nullptr ? CellOf(*table, *row, column) : nullptr;
    if (cell == nullptr)
    {
        return 0;
    }
    if (const auto *whole = std::get_if<std::int64_t>(cell))
    {
        return static_cast<double>(*whole);
    }
    if (const auto *real = std::get_if<double>(cell))
    {
        return *real;
    }
    return 0;
}

std::wstring Lower(std::wstring text)
{
    std::ranges::transform(text, text.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return text;
}

// A part's name from a row of a variation worksheet (0x00749200): its
// ModelType, then "_" and its ModelSubType if it has one, then its
// ModelVariation; empty for a row with no ModelType.
std::wstring NameOf(const m2da::Merged *worksheet, const m2da::Row *row)
{
    std::wstring name = Text(worksheet, row, "ModelType");
    if (name.empty())
    {
        return name;
    }
    if (const auto sub = Text(worksheet, row, "ModelSubType"); !sub.empty())
    {
        name += L"_" + sub;
    }
    return Lower(name + Text(worksheet, row, "ModelVariation"));
}

// A part's name after its prefix and the underscore "%hc%hc_" ends with.
std::wstring Named(const std::wstring &prefix, const std::wstring &name)
{
    std::wstring named = prefix;
    named += L'_';
    named += name;
    return named;
}

struct Context
{
    const TableOf &tableOf;
    const m2da::Merged *items = nullptr;
    const m2da::Merged *appearances = nullptr;
    const m2da::Row *appearance = nullptr;
};

// An item's row of its base type's variation worksheet, with the worksheet.
struct Variation
{
    const m2da::Merged *worksheet = nullptr;
    const m2da::Row *row = nullptr;
};

Variation VariationOf(const Context &context, const Item &item)
{
    const m2da::Row *base = RowOf(context.items, item.baseItem);
    const auto sheet = static_cast<std::int64_t>(Number(context.items, base, "Variation_Worksheet"));
    const m2da::Merged *worksheet = base != nullptr && sheet != 0 ? context.tableOf(sheet) : nullptr;
    return {.worksheet = worksheet, .row = RowOf(worksheet, item.variation)};
}

// The prefix of a part's name (0x00749200): the first letter of apr_base's
// ModelRace, then the first of its ModelGenderOverride if it has one, else
// the gender's letter in gender.gda (m, f, n; 0x00749020); "%hc%hc_", the
// name after an underscore.
std::wstring PrefixOf(const Context &context, std::uint8_t gender)
{
    const auto race = Text(context.appearances, context.appearance, "ModelRace");
    auto letter = Text(context.appearances, context.appearance, "ModelGenderOverride");
    if (letter.empty())
    {
        const m2da::Merged *genders = context.tableOf(kGenders);
        letter = Text(genders, RowOf(genders, gender), "Gender");
    }
    if (race.empty() || letter.empty())
    {
        return {};
    }
    return Lower(std::wstring{race[0], letter[0]});
}

std::optional<Item> EquippedIn(const Template &creature, const ItemOfName &itemOf, Slot slot)
{
    // The off-hand slot is the shield's or the ammunition's (0x007480D0);
    // an off-hand item comes first.
    std::optional<Item> found;
    for (const auto &equipped : creature.equipment)
    {
        if (SlotOf(equipped.inventorySlot) != slot)
        {
            continue;
        }
        auto item = itemOf(equipped.item);
        if (item && (!found || equipped.inventorySlot == kInventoryOffhand))
        {
            found = item;
        }
    }
    return found;
}

// The chest, gloves and boots (0x0074B7E0): each its item's model, or the
// bare body's (naked_variation's row of the slot, 0x0074AB40) where the slot
// is empty or its variation has no row; Clothing draws the chest alone,
// Massive with no boots no boots.
std::vector<Part> BodyOf(const Context &context, const Template &creature, const ItemOfName &itemOf,
                         const std::wstring &prefix)
{
    std::vector<Part> body;
    const auto chest = EquippedIn(creature, itemOf, Slot::Chest);
    const std::int32_t chestBase = chest ? chest->baseItem : 0;
    const m2da::Merged *naked = context.tableOf(kNakedVariations);
    for (const Slot slot : {Slot::Chest, Slot::Gloves, Slot::Boots})
    {
        if ((chestBase == kClothing && slot != Slot::Chest) || (chestBase == kMassiveNoBoots && slot == Slot::Boots))
        {
            continue;
        }
        const auto item = slot == Slot::Chest ? chest : EquippedIn(creature, itemOf, slot);
        const Variation variation = item ? VariationOf(context, *item) : Variation{};
        std::wstring name = NameOf(variation.worksheet, variation.row);
        const bool bare = name.empty();
        if (bare)
        {
            name = NameOf(naked, RowOf(naked, static_cast<std::int64_t>(slot)));
        }
        if (!name.empty())
        {
            body.push_back({.slot = slot, .model = Named(prefix, name), .item = bare ? std::nullopt : item});
        }
    }
    return body;
}

// The neck and the helmet, and what the helmet leaves (0x0074B010).
Head HeadOf(const Context &context, const Template &creature, const ItemOfName &itemOf, const std::wstring &prefix)
{
    Head head;
    if (const auto neck = Text(context.appearances, context.appearance, "Neck_Model"); !neck.empty())
    {
        head.neck = Named(prefix, Lower(neck));
    }
    // A head part's model by its row (0x0074AB40 for a head part, with no
    // morph or past the morph's own names): its worksheet from apr_base
    // (0x00748630), the row the template's PartList set (0x00747E90), named
    // as a body part is (0x00749200); 0xFF none.
    const auto partOf = [&](std::size_t part, std::string_view worksheet) {
        const std::uint8_t row = creature.headRows[part];
        const auto sheet = static_cast<std::int64_t>(Number(context.appearances, context.appearance, worksheet));
        const m2da::Merged *table = row != kNoRow && sheet != 0 ? context.tableOf(sheet) : nullptr;
        const auto name = NameOf(table, RowOf(table, row));
        return name.empty() ? name : Named(prefix, name);
    };
    head.faceModel = partOf(0, "Head_Worksheet");
    head.eyesModel = partOf(1, "Eyes_Worksheet");
    head.hairModel = partOf(2, "Hair_Worksheet");
    head.beardModel = partOf(3, "Beard_Worksheet");
    const auto helmet = EquippedIn(creature, itemOf, Slot::Helmet);
    if (!helmet)
    {
        return head;
    }
    const Variation variation = VariationOf(context, *helmet);
    if (const auto name = NameOf(variation.worksheet, variation.row); !name.empty())
    {
        head.helmet = Named(prefix, name);
    }
    head.hair = false;
    head.fullFace = Number(variation.worksheet, variation.row, "FullFace") != 0;
    if (head.fullFace)
    {
        head.face = false;
        head.beard = false;
        head.lashes = false;
    }
    return head;
}

// A weapon's node, from its base type's column naming a row of weaponparts.
std::pair<std::wstring, bool> NodeOf(const Context &context, const m2da::Row *base, std::string_view column)
{
    const m2da::Merged *parts = context.tableOf(kWeaponParts);
    const m2da::Row *row = RowOf(parts, static_cast<std::int64_t>(Number(context.items, base, column)));
    return {Text(parts, row, "AttachNode"), Number(parts, row, "WeaponOffset") != 0};
}

// The weapons and shields (0x006A6940, 0x00A66B70): "w_" and the
// variation's name, hung sheathed from the right or left hand's Wear node
// and drawn to its Wield node.
std::vector<Weapon> WeaponsOf(const Context &context, const Template &creature, const ItemOfName &itemOf)
{
    std::vector<Weapon> weapons;
    const auto chest = EquippedIn(creature, itemOf, Slot::Chest);
    auto offset = static_cast<float>(Number(context.appearances, context.appearance, "WeaponOffset"));
    if (creature.gender == 2)
    {
        offset *= kFemaleOffset;
    }
    offset += chest ? static_cast<float>(Number(context.items, RowOf(context.items, chest->baseItem), "WeaponOffset"))
                    : kNoChestOffset;
    for (const Slot slot : {Slot::Main, Slot::Offhand})
    {
        const auto item = EquippedIn(creature, itemOf, slot);
        if (!item)
        {
            continue;
        }
        const Variation variation = VariationOf(context, *item);
        const auto name = NameOf(variation.worksheet, variation.row);
        if (name.empty())
        {
            continue;
        }
        const m2da::Row *base = RowOf(context.items, item->baseItem);
        const bool right = slot == Slot::Main;
        auto [sheathed, sheathedOffset] = NodeOf(context, base, right ? "WearR" : "WearL");
        auto [drawn, drawnOffset] = NodeOf(context, base, right ? "WieldR" : "WieldL");
        float sum = offset + static_cast<float>(Number(context.items, base, "WeaponOffset"));
        if (std::fabs(sum) <= kNoOffset)
        {
            sum = 0;
        }
        weapons.push_back({.slot = slot,
                           .model = L"w_" + name,
                           .item = *item,
                           .sheathed = std::move(sheathed),
                           .drawn = std::move(drawn),
                           .offset = sum,
                           .sheathedOffset = sheathedOffset,
                           .drawnOffset = drawnOffset});
    }
    return weapons;
}

template <class T> T Number(const gff3::Struct &in, std::string_view label)
{
    if (const auto value = in.Get<T>(label))
    {
        return *value;
    }
    return T{};
}

// A template's or an item's numbers are of whichever width the toolset wrote.
std::int32_t Whole(const gff3::Struct &in, std::string_view label)
{
    if (const auto v = in.Get<std::int32_t>(label))
    {
        return *v;
    }
    if (const auto v = in.Get<std::uint32_t>(label))
    {
        return static_cast<std::int32_t>(*v);
    }
    if (const auto v = in.Get<std::uint16_t>(label))
    {
        return *v;
    }
    if (const auto v = in.Get<std::int16_t>(label))
    {
        return *v;
    }
    if (const auto v = in.Get<std::uint8_t>(label))
    {
        return *v;
    }
    return 0;
}

} // namespace

// The appearance's slots from the scripts' (0x007480D0).
std::optional<Slot> SlotOf(std::int32_t inventorySlot)
{
    switch (inventorySlot)
    {
    case kInventoryChest:
        return Slot::Chest;
    case kInventoryGloves:
        return Slot::Gloves;
    case kInventoryBoots:
        return Slot::Boots;
    case kInventoryMain:
        return Slot::Main;
    case kInventoryOffhand:
    case kInventoryAmmo:
        return Slot::Offhand;
    case kInventoryHead:
        return Slot::Helmet;
    case kInventoryCloak:
        return Slot::Cloak;
    default:
        return std::nullopt;
    }
}

// A template's equipment list numbers each item by its entry's struct id,
// the scripts' INVENTORY_SLOT_* (gen00fl_alistair.utc: 1 the shield, 0 the
// longsword, 6 the boots, 4 the chest, 7 the gloves, 9 a ring; 2026-10-17).
std::optional<Template> TemplateOf(const gff3::File &utc)
{
    const auto root = utc.Root();
    const auto looks = root ? root->Child("Appearance Data") : std::nullopt;
    if (utc.FileType() != "UTC" || !looks)
    {
        return std::nullopt;
    }
    Template creature{
        .appearance = Whole(*looks, "Appearance_Type"),
        .gender = static_cast<std::uint8_t>(Whole(*looks, "Gender")),
        .headMorph = looks->ResRef("HeadMorph").value_or(looks->String("HeadMorph").value_or("")),
        .hairColor = Whole(*looks, "HairColor"),
        .skinTint = Whole(*looks, "SkinTint"),
        .eyesColor = Whole(*looks, "EyesColor"),
        .bodyTint = Whole(*looks, "BodyTint"),
        .bodyTintMask = Whole(*looks, "BodyTintMask"),
        .tattooTint = Whole(*looks, "TattooTint"),
        .tattoo = Whole(*looks, "Tattoo"),
        .equipment = {},
    };
    // The head's rows (0x00748AE0): PartList's elements, at most six, each
    // its part by its struct id and its row by its PartVariation, into
    // CAppearance+0x1A8 (0x00747EB0); a part none names stays 0xFF, as the
    // constructor leaves it (lib-agent-2's reading, 2026-10-17).
    for (const auto &entry : looks->List("PartList"))
    {
        const std::uint32_t part = entry.Id();
        if (part < creature.headRows.size())
        {
            creature.headRows[part] = static_cast<std::uint8_t>(Whole(entry, "PartVariation"));
        }
    }
    for (const auto &entry : root->List("Equip_ItemList"))
    {
        creature.equipment.push_back({.inventorySlot = static_cast<std::int32_t>(entry.Id()),
                                      .item = entry.ResRef("TemplateResRef").value_or("")});
    }
    return creature;
}

std::optional<Item> ItemOf(const gff3::File &uti)
{
    const auto root = uti.Root();
    if (uti.FileType() != "UTI" || !root)
    {
        return std::nullopt;
    }
    return Item{
        .baseItem = Whole(*root, "BaseItem"),
        .variation = Whole(*root, "ModelVariation"),
        .material = Whole(*root, "Material"),
        .materialProgress = Whole(*root, "MaterialProgress"),
        .bodyTint = Whole(*root, "BodyTint"),
        .bodyTintMask = Whole(*root, "BodyTintMask"),
        .tintOverride = Whole(*root, "TintOverride"),
    };
}

namespace
{

// Notes on the tint tables, read 2026-10-18 (DAOLib #172): m2da_base names 81 head_tint_masks
// ("Tattoos"), 82 body_tint_masks, 83 eye_tints, 84 hair_tints, 85 skin_tints, 86 tattoo_tints
// ("Tattoos"), 87 tint_override (no file in the game) and 88 body_tints. Each has ID, Label, an
// Apr column (the apr_base row it was made for, 0 for any) and the file: TintFile, a .tnt, in
// the tint tables, MaskFile, a texture, in the two mask tables. Row 0 is INVALID with no file in
// all; rows 1 to 3 of the eye, hair, skin and body tints are Red, Green and Blue.tnt; the rest
// are monsters' (hurlocks', genlocks', werewolves', the dog's). Which field indexes which table
// is by the tables' names and labels; the engine's side is not read beyond its reading the
// fields into the appearance (0x00748B00: SkinTint at +0x1C, TattooTint +0x28, HairColor +0x2C,
// EyesColor +0x30). No script reads the tables. So a template with rows 0, default_hm_noble.utc
// but for its EyesColor 3 (Blue.tnt), names no tint; what the engine then draws the head with
// (the material's own values, by the look of the .mao, which names no tint) is not read.
Tints TintsOf(const Template &creature, const TableOf &tableOf)
{
    const auto file = [&tableOf](std::int64_t table, std::int32_t row, std::string_view column) {
        const m2da::Merged *rows = tableOf(table);
        return Text(rows, RowOf(rows, row), column);
    };
    return {.skin = file(kSkinTints, creature.skinTint, "TintFile"),
            .hair = file(kHairTints, creature.hairColor, "TintFile"),
            .eyes = file(kEyeTints, creature.eyesColor, "TintFile"),
            .tattoo = file(kTattooTints, creature.tattooTint, "TintFile"),
            .tattooMask = file(kHeadTintMasks, creature.tattoo, "MaskFile"),
            .body = file(kBodyTints, creature.bodyTint, "TintFile"),
            .bodyMask = file(kBodyTintMasks, creature.bodyTintMask, "MaskFile")};
}

} // namespace

// How a creature is built goes by the first letter of apr_base's ModelType
// (0x007490D0, 0x006C0DE0): P is a humanoid of parts; the rest are
// apr_base's ModelName whole.
std::optional<Character> Resolve(const Template &creature, const ItemOfName &itemOf, const TableOf &tableOf)
{
    Context context{.tableOf = tableOf, .items = tableOf(kItems), .appearances = tableOf(kAppearances)};
    context.appearance = RowOf(context.appearances, creature.appearance);
    if (context.appearance == nullptr)
    {
        return std::nullopt;
    }
    Character character;
    character.headMorph = creature.headMorph;
    character.tints = TintsOf(creature, tableOf);
    const auto type = Text(context.appearances, context.appearance, "ModelType");
    character.parts = !type.empty() && std::towlower(type[0]) == L'p';
    if (!character.parts)
    {
        character.model = Lower(Text(context.appearances, context.appearance, "ModelName"));
        return character;
    }
    character.prefix = PrefixOf(context, creature.gender);
    character.body = BodyOf(context, creature, itemOf, character.prefix);
    character.head = HeadOf(context, creature, itemOf, character.prefix);
    character.weapons = WeaponsOf(context, creature, itemOf);
    return character;
}

std::wstring FullFaceEyes(std::wstring_view eyes)
{
    if (eyes.empty())
    {
        return std::wstring(eyes);
    }
    std::wstring swapped(eyes.substr(0, eyes.size() - 1));
    swapped += L'2';
    swapped += eyes.back();
    return swapped;
}

} // namespace dao::core::character
