// Written by tools/gen_gff3_schema.py from dev/gff3_schema_*.csv (tools/gff3_schema, the GOG
// install's templates scanned) and dev/gff3_references.csv; do not edit.
//
// The toolset's templates, each kind's fields as the install's templates have them: a catalogue
// of what each field is and holds, and a typed view of each struct. A mod's template may have other
// fields: read those through dao/core/Gff3.h as ever.
//
//   if (const auto creature = dao::core::templates::utc::Creature::Of(*file))
//       const std::optional<std::string> tag = creature->Tag();
#pragma once

#include "dao/core/Gff3.h"
#include "dao/core/TemplateView.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core::templates
{

// What a field's values are.
enum class Kind : std::uint8_t
{
    Plain,
    StrRef,   // a talk table's string id
    Resource, // a file's name, without its extension
    Row,      // the ID of a row of a worksheet, Field::target
};

struct Field
{
    std::string_view path;  // the struct it is in: "" the top, "Appearance Data", "ItemList[]"
    std::string_view label; // as the files spell it
    gff3::Type type = gff3::Type::Byte;
    Kind kind = Kind::Plain;
    std::string_view extension; // a Resource's, without its dot
    std::int64_t target = -1;   // a Row's worksheet
    std::size_t files = 0;      // how many of the install's templates of the kind have it
};

// Returns a kind of template's fields, struct by struct; none for a kind with no schema.
// @param type the files' extension, in lower case: "utc"
std::span<const Field> Fields(std::string_view type);
// Returns a field by its struct's path and its label; null for none.
const Field *FieldOf(std::string_view type, std::string_view path, std::string_view label);

namespace utc
{

// Appearance Data/PartList[] of a template.
class AppearanceDataPartListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::uint8_t> PartVariation() const
    {
        return Number<std::uint8_t>("PartVariation");
    }
};

// LvlStatList[]/FeatList[] of a template.
class LvlStatListEntryFeatListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::uint16_t> Feat() const
    {
        return Number<std::uint16_t>("Feat");
    }
};

// LvlStatList[]/KnownList0[] of a template.
class LvlStatListEntryKnownList0Entry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::uint16_t> Spell() const
    {
        return Number<std::uint16_t>("Spell");
    }
};

// LvlStatList[]/KnownList1[] of a template.
class LvlStatListEntryKnownList1Entry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::uint16_t> Spell() const
    {
        return Number<std::uint16_t>("Spell");
    }
};

// LvlStatList[]/SkillList[] of a template.
class LvlStatListEntrySkillListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::uint8_t> Rank() const
    {
        return Number<std::uint8_t>("Rank");
    }
};

// Appearance Data of a template.
class AppearanceData : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::uint8_t> Gender() const
    {
        return Number<std::uint8_t>("Gender");
    }
    std::optional<std::int32_t> BodyTintMask() const
    {
        return Number<std::int32_t>("BodyTintMask");
    }
    std::optional<std::int32_t> BodyTint() const
    {
        return Number<std::int32_t>("BodyTint");
    }
    std::optional<std::int32_t> SkinTint() const
    {
        return Number<std::int32_t>("SkinTint");
    }
    std::optional<std::int32_t> TattooTint() const
    {
        return Number<std::int32_t>("TattooTint");
    }
    std::optional<std::int32_t> EyesColor() const
    {
        return Number<std::int32_t>("EyesColor");
    }
    std::optional<std::int32_t> HairColor() const
    {
        return Number<std::int32_t>("HairColor");
    }
    std::optional<std::int32_t> Appearance_Type() const
    {
        return Number<std::int32_t>("Appearance_Type");
    }
    std::optional<std::uint8_t> Tattoo() const
    {
        return Number<std::uint8_t>("Tattoo");
    }
    std::optional<std::string> HeadMorph() const
    {
        return Text("HeadMorph");
    }
    std::vector<AppearanceDataPartListEntry> PartList() const
    {
        return List<AppearanceDataPartListEntry>("PartList");
    }
    std::optional<std::int32_t> Race() const
    {
        return Number<std::int32_t>("Race");
    }
    std::optional<std::int32_t> BaseMaterial() const
    {
        return Number<std::int32_t>("BaseMaterial");
    }
};

// Equip_ItemList[] of a template.
class EquipItemListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::string> TemplateResRef() const
    {
        return ResRef("TemplateResRef");
    }
    std::optional<std::uint8_t> Stealable() const
    {
        return Number<std::uint8_t>("Stealable");
    }
    std::optional<std::uint8_t> Droppable() const
    {
        return Number<std::uint8_t>("Droppable");
    }
    std::optional<std::int32_t> SetNumber() const
    {
        return Number<std::int32_t>("SetNumber");
    }
    std::optional<std::int32_t> StackSize() const
    {
        return Number<std::int32_t>("StackSize");
    }
};

// FeatList[] of a template.
class FeatListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::uint16_t> Feat() const
    {
        return Number<std::uint16_t>("Feat");
    }
    std::optional<std::uint8_t> Uses() const
    {
        return Number<std::uint8_t>("Uses");
    }
};

// ItemList[] of a template.
class ItemListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::string> TemplateResRef() const
    {
        return ResRef("TemplateResRef");
    }
    std::optional<std::uint8_t> Stealable() const
    {
        return Number<std::uint8_t>("Stealable");
    }
    std::optional<std::uint8_t> Droppable() const
    {
        return Number<std::uint8_t>("Droppable");
    }
    std::optional<std::int32_t> StackSize() const
    {
        return Number<std::int32_t>("StackSize");
    }
};

// LvlStatList[] of a template.
class LvlStatListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::uint8_t> LvlStatHitDie() const
    {
        return Number<std::uint8_t>("LvlStatHitDie");
    }
    std::optional<std::int16_t> LvlStatClass() const
    {
        return Number<std::int16_t>("LvlStatClass");
    }
    std::optional<std::uint8_t> EpicLevel() const
    {
        return Number<std::uint8_t>("EpicLevel");
    }
    std::vector<LvlStatListEntryKnownList0Entry> KnownList0() const
    {
        return List<LvlStatListEntryKnownList0Entry>("KnownList0");
    }
    std::vector<LvlStatListEntryKnownList1Entry> KnownList1() const
    {
        return List<LvlStatListEntryKnownList1Entry>("KnownList1");
    }
    std::optional<std::uint16_t> SkillPoints() const
    {
        return Number<std::uint16_t>("SkillPoints");
    }
    std::vector<LvlStatListEntrySkillListEntry> SkillList() const
    {
        return List<LvlStatListEntrySkillListEntry>("SkillList");
    }
    std::vector<LvlStatListEntryFeatListEntry> FeatList() const
    {
        return List<LvlStatListEntryFeatListEntry>("FeatList");
    }
};

// PerceptionList[] of a template.
class PerceptionListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::uint32_t> ObjectId() const
    {
        return Number<std::uint32_t>("ObjectId");
    }
    std::optional<std::uint8_t> PerceptionData() const
    {
        return Number<std::uint8_t>("PerceptionData");
    }
};

// PropertyList[] of a template.
class PropertyListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::int32_t> Index() const
    {
        return Number<std::int32_t>("Index");
    }
    std::optional<std::int32_t> Base() const
    {
        return Number<std::int32_t>("Base");
    }
    std::optional<std::int32_t> Current() const
    {
        return Number<std::int32_t>("Current");
    }
};

// QBList[] of a template.
class QBListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::uint8_t> QBObjectType() const
    {
        return Number<std::uint8_t>("QBObjectType");
    }
    std::optional<std::int32_t> QBINTParam1() const
    {
        return Number<std::int32_t>("QBINTParam1");
    }
    std::optional<std::uint32_t> QBItemInvSlot() const
    {
        return Number<std::uint32_t>("QBItemInvSlot");
    }
    std::optional<std::uint8_t> QBItemReposX() const
    {
        return Number<std::uint8_t>("QBItemReposX");
    }
    std::optional<std::uint8_t> QBItemReposY() const
    {
        return Number<std::uint8_t>("QBItemReposY");
    }
    std::optional<std::uint8_t> QBContReposX() const
    {
        return Number<std::uint8_t>("QBContReposX");
    }
    std::optional<std::uint8_t> QBContReposY() const
    {
        return Number<std::uint8_t>("QBContReposY");
    }
    std::optional<std::uint8_t> QBCastPropIndex() const
    {
        return Number<std::uint8_t>("QBCastPropIndex");
    }
    std::optional<std::uint8_t> QBCastSubPropIdx() const
    {
        return Number<std::uint8_t>("QBCastSubPropIdx");
    }
    std::optional<std::uint8_t> QBMultiClass() const
    {
        return Number<std::uint8_t>("QBMultiClass");
    }
    std::optional<std::uint8_t> QBMetaType() const
    {
        return Number<std::uint8_t>("QBMetaType");
    }
    std::optional<std::uint8_t> QBDomainLevel() const
    {
        return Number<std::uint8_t>("QBDomainLevel");
    }
};

// SkillList[] of a template.
class SkillListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::int32_t> SkillID() const
    {
        return Number<std::int32_t>("SkillID");
    }
    std::optional<std::uint8_t> Rank() const
    {
        return Number<std::uint8_t>("Rank");
    }
};

// SpellList[] of a template.
class SpellListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::int32_t> SpellNumber() const
    {
        return Number<std::int32_t>("SpellNumber");
    }
};

// TalentList[] of a template.
class TalentListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::int32_t> TalentNumber() const
    {
        return Number<std::int32_t>("TalentNumber");
    }
};

// VarTable[] of a template.
class VarTableEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::string> Name() const
    {
        return Text("Name");
    }
    std::optional<std::uint8_t> Type() const
    {
        return Number<std::uint8_t>("Type");
    }
    std::optional<std::int32_t> Value() const
    {
        return Number<std::int32_t>("Value");
    }
};

// The top struct of a template.
class Creature : public StructView
{
  public:
    using StructView::StructView;

    // Returns a file's top struct; none for a file with none.
    static std::optional<Creature> Of(const gff3::File &file)
    {
        const auto root = file.Root();
        return root ? std::optional<Creature>(Creature(*root)) : std::nullopt;
    }
    std::optional<gff3::LocString> LocName() const
    {
        return Loc("LocName");
    }
    std::optional<gff3::LocString> Description() const
    {
        return Loc("Description");
    }
    std::optional<std::string> TemplateResRef() const
    {
        return ResRef("TemplateResRef");
    }
    std::optional<std::string> Tag() const
    {
        return Text("Tag");
    }
    std::optional<std::uint32_t> GroupId() const
    {
        return Number<std::uint32_t>("GroupId");
    }
    std::optional<std::int32_t> TeamId() const
    {
        return Number<std::int32_t>("TeamId");
    }
    std::optional<std::uint8_t> IsSelectable() const
    {
        return Number<std::uint8_t>("IsSelectable");
    }
    std::optional<std::uint8_t> IsPlotGiver() const
    {
        return Number<std::uint8_t>("IsPlotGiver");
    }
    std::optional<std::string> DialogResRef() const
    {
        return ResRef("DialogResRef");
    }
    std::optional<std::int32_t> Package() const
    {
        return Number<std::int32_t>("Package");
    }
    std::optional<std::int32_t> PackageAI() const
    {
        return Number<std::int32_t>("PackageAI");
    }
    std::optional<std::string> EventScript() const
    {
        return ResRef("EventScript");
    }
    std::optional<std::uint32_t> Gold() const
    {
        return Number<std::uint32_t>("Gold");
    }
    std::optional<std::uint8_t> Immortal() const
    {
        return Number<std::uint8_t>("Immortal");
    }
    std::optional<std::uint8_t> NoPermDeath() const
    {
        return Number<std::uint8_t>("NoPermDeath");
    }
    std::optional<std::uint8_t> PerceptionClass() const
    {
        return Number<std::uint8_t>("PerceptionClass");
    }
    std::optional<std::uint8_t> Plot() const
    {
        return Number<std::uint8_t>("Plot");
    }
    std::optional<std::int32_t> Combatant() const
    {
        return Number<std::int32_t>("Combatant");
    }
    std::optional<std::int32_t> Rank() const
    {
        return Number<std::int32_t>("Rank");
    }
    std::optional<std::int32_t> TreasureCategory() const
    {
        return Number<std::int32_t>("TreasureCategory");
    }
    std::optional<std::int32_t> MinLevel() const
    {
        return Number<std::int32_t>("MinLevel");
    }
    std::optional<std::int32_t> MaxLevel() const
    {
        return Number<std::int32_t>("MaxLevel");
    }
    std::optional<std::string> SoundSet() const
    {
        return ResRef("SoundSet");
    }
    std::optional<AppearanceData> Appearance_Data() const
    {
        return Child<AppearanceData>("Appearance Data");
    }
    std::optional<std::int32_t> Race() const
    {
        return Number<std::int32_t>("Race");
    }
    std::vector<TalentListEntry> TalentList() const
    {
        return List<TalentListEntry>("TalentList");
    }
    std::vector<SpellListEntry> SpellList() const
    {
        return List<SpellListEntry>("SpellList");
    }
    std::vector<ItemListEntry> ItemList() const
    {
        return List<ItemListEntry>("ItemList");
    }
    std::vector<EquipItemListEntry> Equip_ItemList() const
    {
        return List<EquipItemListEntry>("Equip_ItemList");
    }
    std::vector<VarTableEntry> VarTable() const
    {
        return List<VarTableEntry>("VarTable");
    }
    std::vector<PropertyListEntry> PropertyList() const
    {
        return List<PropertyListEntry>("PropertyList");
    }
    std::vector<SkillListEntry> SkillList() const
    {
        return List<SkillListEntry>("SkillList");
    }
    std::optional<gff3::LocString> FullName() const
    {
        return Loc("FullName");
    }
    std::optional<gff3::LocString> RealName() const
    {
        return Loc("RealName");
    }
    std::optional<std::string> Comment() const
    {
        return Text("Comment");
    }
    std::optional<std::uint8_t> IsPC() const
    {
        return Number<std::uint8_t>("IsPC");
    }
    std::optional<std::uint8_t> IsDM() const
    {
        return Number<std::uint8_t>("IsDM");
    }
    std::optional<std::string> Conversation() const
    {
        return ResRef("Conversation");
    }
    std::optional<std::int32_t> Age() const
    {
        return Number<std::int32_t>("Age");
    }
    std::optional<std::uint8_t> StartingPackage() const
    {
        return Number<std::uint8_t>("StartingPackage");
    }
    std::optional<std::uint8_t> IsUnselectable() const
    {
        return Number<std::uint8_t>("IsUnselectable");
    }
    std::optional<std::string> Deity() const
    {
        return Text("Deity");
    }
    std::optional<std::uint8_t> MClassLevUpIn() const
    {
        return Number<std::uint8_t>("MClassLevUpIn");
    }
    std::optional<std::int16_t> willbonus() const
    {
        return Number<std::int16_t>("willbonus");
    }
    std::optional<std::int16_t> fortbonus() const
    {
        return Number<std::int16_t>("fortbonus");
    }
    std::optional<std::int16_t> refbonus() const
    {
        return Number<std::int16_t>("refbonus");
    }
    std::optional<std::uint8_t> BaseAttackBonus() const
    {
        return Number<std::uint8_t>("BaseAttackBonus");
    }
    std::optional<std::int16_t> ArmorClass() const
    {
        return Number<std::int16_t>("ArmorClass");
    }
    std::optional<std::uint8_t> OverrideBAB() const
    {
        return Number<std::uint8_t>("OverrideBAB");
    }
    std::optional<std::uint8_t> Str() const
    {
        return Number<std::uint8_t>("Str");
    }
    std::optional<std::uint8_t> Dex() const
    {
        return Number<std::uint8_t>("Dex");
    }
    std::optional<std::uint8_t> Int() const
    {
        return Number<std::uint8_t>("Int");
    }
    std::optional<std::uint8_t> Mag() const
    {
        return Number<std::uint8_t>("Mag");
    }
    std::optional<std::uint8_t> Con() const
    {
        return Number<std::uint8_t>("Con");
    }
    std::optional<std::uint8_t> Wil() const
    {
        return Number<std::uint8_t>("Wil");
    }
    std::optional<std::uint8_t> NaturalAC() const
    {
        return Number<std::uint8_t>("NaturalAC");
    }
    std::optional<std::uint16_t> SoundSetFile() const
    {
        return Number<std::uint16_t>("SoundSetFile");
    }
    std::optional<std::uint8_t> Disarmable() const
    {
        return Number<std::uint8_t>("Disarmable");
    }
    std::optional<std::uint32_t> GroupID() const
    {
        return Number<std::uint32_t>("GroupID");
    }
    std::optional<std::uint8_t> BodyBag() const
    {
        return Number<std::uint8_t>("BodyBag");
    }
    std::optional<std::int16_t> HitPoints() const
    {
        return Number<std::int16_t>("HitPoints");
    }
    std::optional<std::int16_t> CurrentHitPoints() const
    {
        return Number<std::int16_t>("CurrentHitPoints");
    }
    std::optional<std::int16_t> MaxHitPoints() const
    {
        return Number<std::int16_t>("MaxHitPoints");
    }
    std::optional<std::int16_t> PregameCurrent() const
    {
        return Number<std::int16_t>("PregameCurrent");
    }
    std::optional<std::uint32_t> Experience() const
    {
        return Number<std::uint32_t>("Experience");
    }
    std::optional<std::uint8_t> MovementRate() const
    {
        return Number<std::uint8_t>("MovementRate");
    }
    std::optional<std::string> Portrait() const
    {
        return ResRef("Portrait");
    }
    std::optional<std::uint8_t> Alignment() const
    {
        return Number<std::uint8_t>("Alignment");
    }
    std::optional<std::int32_t> Power() const
    {
        return Number<std::int32_t>("Power");
    }
    std::vector<LvlStatListEntry> LvlStatList() const
    {
        return List<LvlStatListEntry>("LvlStatList");
    }
    std::optional<std::uint16_t> SkillPoints() const
    {
        return Number<std::uint16_t>("SkillPoints");
    }
    std::vector<FeatListEntry> FeatList() const
    {
        return List<FeatListEntry>("FeatList");
    }
    std::optional<std::uint8_t> DetectMode() const
    {
        return Number<std::uint8_t>("DetectMode");
    }
    std::optional<std::uint8_t> StealthMode() const
    {
        return Number<std::uint8_t>("StealthMode");
    }
    std::optional<std::uint32_t> MasterID() const
    {
        return Number<std::uint32_t>("MasterID");
    }
    std::optional<std::int32_t> CreatureSize() const
    {
        return Number<std::int32_t>("CreatureSize");
    }
    std::optional<std::uint8_t> IsDestroyable() const
    {
        return Number<std::uint8_t>("IsDestroyable");
    }
    std::optional<std::uint8_t> IsRaiseable() const
    {
        return Number<std::uint8_t>("IsRaiseable");
    }
    std::optional<std::uint8_t> DeadSelectable() const
    {
        return Number<std::uint8_t>("DeadSelectable");
    }
    std::optional<std::uint8_t> IsCommandable() const
    {
        return Number<std::uint8_t>("IsCommandable");
    }
    std::optional<std::uint8_t> IsImmortal() const
    {
        return Number<std::uint8_t>("IsImmortal");
    }
    std::optional<std::uint8_t> Lootable() const
    {
        return Number<std::uint8_t>("Lootable");
    }
    std::optional<std::uint32_t> BodyBagId() const
    {
        return Number<std::uint32_t>("BodyBagId");
    }
    std::optional<std::uint32_t> DecayTime() const
    {
        return Number<std::uint32_t>("DecayTime");
    }
    std::vector<PerceptionListEntry> PerceptionList() const
    {
        return List<PerceptionListEntry>("PerceptionList");
    }
    std::optional<std::uint32_t> AreaId() const
    {
        return Number<std::uint32_t>("AreaId");
    }
    std::optional<std::uint8_t> AmbientAnimState() const
    {
        return Number<std::uint8_t>("AmbientAnimState");
    }
    std::optional<std::uint8_t> PM_IsPolymorphed() const
    {
        return Number<std::uint8_t>("PM_IsPolymorphed");
    }
    std::optional<std::uint8_t> Listening() const
    {
        return Number<std::uint8_t>("Listening");
    }
    std::vector<QBListEntry> QBList() const
    {
        return List<QBListEntry>("QBList");
    }
    std::optional<std::uint8_t> CharType() const
    {
        return Number<std::uint8_t>("CharType");
    }
    std::optional<std::int32_t> Class() const
    {
        return Number<std::int32_t>("Class");
    }
    std::optional<std::int16_t> Level() const
    {
        return Number<std::int16_t>("Level");
    }
    std::optional<std::int16_t> Health() const
    {
        return Number<std::int16_t>("Health");
    }
    std::optional<std::int16_t> MaxHealth() const
    {
        return Number<std::int16_t>("MaxHealth");
    }
    std::optional<std::int32_t> GUIBar() const
    {
        return Number<std::int32_t>("GUIBar");
    }
    std::optional<std::int32_t> ResistLightning() const
    {
        return Number<std::int32_t>("ResistLightning");
    }
    std::optional<std::int32_t> ResistFile() const
    {
        return Number<std::int32_t>("ResistFile");
    }
    std::optional<std::int32_t> ResistCold() const
    {
        return Number<std::int32_t>("ResistCold");
    }
    std::optional<std::int32_t> ResistHindrance() const
    {
        return Number<std::int32_t>("ResistHindrance");
    }
    std::optional<std::int32_t> ResistCoercion() const
    {
        return Number<std::int32_t>("ResistCoercion");
    }
    std::optional<std::int32_t> ResistPhysForce() const
    {
        return Number<std::int32_t>("ResistPhysForce");
    }
    std::optional<std::int32_t> ResistDebil() const
    {
        return Number<std::int32_t>("ResistDebil");
    }
    std::optional<std::int32_t> Stamina() const
    {
        return Number<std::int32_t>("Stamina");
    }
    std::optional<std::int32_t> MaxStamina() const
    {
        return Number<std::int32_t>("MaxStamina");
    }
    std::optional<std::int32_t> Mana() const
    {
        return Number<std::int32_t>("Mana");
    }
    std::optional<std::int32_t> MaxMana() const
    {
        return Number<std::int32_t>("MaxMana");
    }
};

} // namespace utc

namespace uti
{

// PropertiesList[] of a template.
class PropertiesListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::int32_t> PropertyID() const
    {
        return Number<std::int32_t>("PropertyID");
    }
    std::optional<std::int32_t> PropertyPower() const
    {
        return Number<std::int32_t>("PropertyPower");
    }
};

// VarTable[] of a template.
class VarTableEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::string> Name() const
    {
        return Text("Name");
    }
    std::optional<std::uint8_t> Type() const
    {
        return Number<std::uint8_t>("Type");
    }
    std::optional<std::int32_t> Value() const
    {
        return Number<std::int32_t>("Value");
    }
};

// The top struct of a template.
class Item : public StructView
{
  public:
    using StructView::StructView;

    // Returns a file's top struct; none for a file with none.
    static std::optional<Item> Of(const gff3::File &file)
    {
        const auto root = file.Root();
        return root ? std::optional<Item>(Item(*root)) : std::nullopt;
    }
    std::optional<std::uint8_t> ModelVariation() const
    {
        return Number<std::uint8_t>("ModelVariation");
    }
    std::optional<std::string> TemplateResRef() const
    {
        return ResRef("TemplateResRef");
    }
    std::optional<gff3::LocString> LocalizedName() const
    {
        return Loc("LocalizedName");
    }
    std::optional<gff3::LocString> Description() const
    {
        return Loc("Description");
    }
    std::optional<std::string> Script() const
    {
        return ResRef("Script");
    }
    std::optional<std::int32_t> AbilityID() const
    {
        return Number<std::int32_t>("AbilityID");
    }
    std::optional<std::int32_t> CraftingRecipeID() const
    {
        return Number<std::int32_t>("CraftingRecipeID");
    }
    std::optional<std::int32_t> OnHitEffectID() const
    {
        return Number<std::int32_t>("OnHitEffectID");
    }
    std::optional<std::int32_t> OnHitPower() const
    {
        return Number<std::int32_t>("OnHitPower");
    }
    std::optional<std::string> Tag() const
    {
        return Text("Tag");
    }
    std::optional<std::int32_t> BaseItem() const
    {
        return Number<std::int32_t>("BaseItem");
    }
    std::optional<std::uint8_t> Plot() const
    {
        return Number<std::uint8_t>("Plot");
    }
    std::optional<std::int32_t> Cost() const
    {
        return Number<std::int32_t>("Cost");
    }
    std::optional<std::string> Icon() const
    {
        return Text("Icon");
    }
    std::optional<std::int32_t> Material() const
    {
        return Number<std::int32_t>("Material");
    }
    std::optional<std::int32_t> MaterialProgress() const
    {
        return Number<std::int32_t>("MaterialProgress");
    }
    std::optional<std::uint8_t> Heraldry() const
    {
        return Number<std::uint8_t>("Heraldry");
    }
    std::optional<std::int32_t> TintOverride() const
    {
        return Number<std::int32_t>("TintOverride");
    }
    std::optional<std::uint8_t> Damaged() const
    {
        return Number<std::uint8_t>("Damaged");
    }
    std::optional<std::uint8_t> Unique() const
    {
        return Number<std::uint8_t>("Unique");
    }
    std::optional<std::int32_t> BodyTintMask() const
    {
        return Number<std::int32_t>("BodyTintMask");
    }
    std::optional<std::int32_t> BodyTint() const
    {
        return Number<std::int32_t>("BodyTint");
    }
    std::optional<std::int32_t> InvSubgroup() const
    {
        return Number<std::int32_t>("InvSubgroup");
    }
    std::vector<PropertiesListEntry> PropertiesList() const
    {
        return List<PropertiesListEntry>("PropertiesList");
    }
    std::vector<VarTableEntry> VarTable() const
    {
        return List<VarTableEntry>("VarTable");
    }
};

} // namespace uti

namespace utp
{

// ItemList[] of a template.
class ItemListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::string> TemplateResRef() const
    {
        return ResRef("TemplateResRef");
    }
    std::optional<std::int32_t> StackSize() const
    {
        return Number<std::int32_t>("StackSize");
    }
};

// VarTable[] of a template.
class VarTableEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::string> Name() const
    {
        return Text("Name");
    }
    std::optional<std::uint8_t> Type() const
    {
        return Number<std::uint8_t>("Type");
    }
    std::optional<std::int32_t> Value() const
    {
        return Number<std::int32_t>("Value");
    }
};

// The top struct of a template.
class Placeable : public StructView
{
  public:
    using StructView::StructView;

    // Returns a file's top struct; none for a file with none.
    static std::optional<Placeable> Of(const gff3::File &file)
    {
        const auto root = file.Root();
        return root ? std::optional<Placeable>(Placeable(*root)) : std::nullopt;
    }
    std::optional<gff3::LocString> LocName() const
    {
        return Loc("LocName");
    }
    std::optional<gff3::LocString> LocPopupText() const
    {
        return Loc("LocPopupText");
    }
    std::optional<std::string> Tag() const
    {
        return Text("Tag");
    }
    std::optional<std::string> TemplateResRef() const
    {
        return ResRef("TemplateResRef");
    }
    std::optional<std::string> EventScript() const
    {
        return ResRef("EventScript");
    }
    std::optional<std::string> Conversation() const
    {
        return ResRef("Conversation");
    }
    std::optional<std::uint32_t> GroupId() const
    {
        return Number<std::uint32_t>("GroupId");
    }
    std::optional<std::uint32_t> Appearance() const
    {
        return Number<std::uint32_t>("Appearance");
    }
    std::optional<std::uint16_t> AnimContState() const
    {
        return Number<std::uint16_t>("AnimContState");
    }
    std::optional<std::int16_t> Health() const
    {
        return Number<std::int16_t>("Health");
    }
    std::optional<std::int16_t> MaxHealth() const
    {
        return Number<std::int16_t>("MaxHealth");
    }
    std::optional<std::int32_t> PickLock() const
    {
        return Number<std::int32_t>("PickLock");
    }
    std::optional<std::string> KeyName() const
    {
        return Text("KeyName");
    }
    std::optional<std::uint8_t> AutoRemoveKey() const
    {
        return Number<std::uint8_t>("AutoRemoveKey");
    }
    std::optional<std::int32_t> TrapDetection() const
    {
        return Number<std::int32_t>("TrapDetection");
    }
    std::optional<std::int32_t> TrapDisarm() const
    {
        return Number<std::int32_t>("TrapDisarm");
    }
    std::optional<std::uint8_t> KeyRequired() const
    {
        return Number<std::uint8_t>("KeyRequired");
    }
    std::optional<std::uint8_t> Useable() const
    {
        return Number<std::uint8_t>("Useable");
    }
    std::optional<std::uint8_t> Plot() const
    {
        return Number<std::uint8_t>("Plot");
    }
    std::optional<std::int32_t> Rank() const
    {
        return Number<std::int32_t>("Rank");
    }
    std::optional<std::int32_t> TreasureCategory() const
    {
        return Number<std::int32_t>("TreasureCategory");
    }
    std::optional<std::int32_t> TeamId() const
    {
        return Number<std::int32_t>("TeamId");
    }
    std::optional<std::uint8_t> IsBodyBag() const
    {
        return Number<std::uint8_t>("IsBodyBag");
    }
    std::vector<ItemListEntry> ItemList() const
    {
        return List<ItemListEntry>("ItemList");
    }
    std::vector<VarTableEntry> VarTable() const
    {
        return List<VarTableEntry>("VarTable");
    }
    std::optional<std::uint8_t> Ref() const
    {
        return Number<std::uint8_t>("Ref");
    }
    std::optional<std::string> OnUsed() const
    {
        return ResRef("OnUsed");
    }
    std::optional<std::string> Portrait() const
    {
        return ResRef("Portrait");
    }
    std::optional<std::string> OnUserDefined() const
    {
        return ResRef("OnUserDefined");
    }
    std::optional<std::uint8_t> HasInventory() const
    {
        return Number<std::uint8_t>("HasInventory");
    }
    std::optional<std::string> OnDamaged() const
    {
        return ResRef("OnDamaged");
    }
    std::optional<std::uint32_t> Faction() const
    {
        return Number<std::uint32_t>("Faction");
    }
    std::optional<std::uint8_t> Locked() const
    {
        return Number<std::uint8_t>("Locked");
    }
    std::optional<std::uint8_t> CloseLockDC() const
    {
        return Number<std::uint8_t>("CloseLockDC");
    }
    std::optional<std::string> OnOpen() const
    {
        return ResRef("OnOpen");
    }
    std::optional<std::int16_t> HP() const
    {
        return Number<std::int16_t>("HP");
    }
    std::optional<gff3::LocString> Description() const
    {
        return Loc("Description");
    }
    std::optional<std::uint8_t> TrapDetectable() const
    {
        return Number<std::uint8_t>("TrapDetectable");
    }
    std::optional<std::uint8_t> PaletteID() const
    {
        return Number<std::uint8_t>("PaletteID");
    }
    std::optional<std::string> OnClosed() const
    {
        return ResRef("OnClosed");
    }
    std::optional<std::uint8_t> DisarmDC() const
    {
        return Number<std::uint8_t>("DisarmDC");
    }
    std::optional<std::string> OnMeleeAttacked() const
    {
        return ResRef("OnMeleeAttacked");
    }
    std::optional<std::uint8_t> Hardness() const
    {
        return Number<std::uint8_t>("Hardness");
    }
    std::optional<std::uint8_t> AnimationState() const
    {
        return Number<std::uint8_t>("AnimationState");
    }
    std::optional<std::uint8_t> TrapOneShot() const
    {
        return Number<std::uint8_t>("TrapOneShot");
    }
    std::optional<std::uint8_t> Interruptable() const
    {
        return Number<std::uint8_t>("Interruptable");
    }
    std::optional<std::string> OnLock() const
    {
        return ResRef("OnLock");
    }
    std::optional<std::string> OnTrapTriggered() const
    {
        return ResRef("OnTrapTriggered");
    }
    std::optional<std::uint8_t> Type() const
    {
        return Number<std::uint8_t>("Type");
    }
    std::optional<std::uint8_t> TrapType() const
    {
        return Number<std::uint8_t>("TrapType");
    }
    std::optional<std::string> OnDeath() const
    {
        return ResRef("OnDeath");
    }
    std::optional<std::string> OnUnlock() const
    {
        return ResRef("OnUnlock");
    }
    std::optional<std::string> Comment() const
    {
        return Text("Comment");
    }
    std::optional<std::uint8_t> TrapDisarmable() const
    {
        return Number<std::uint8_t>("TrapDisarmable");
    }
    std::optional<std::uint8_t> Lockable() const
    {
        return Number<std::uint8_t>("Lockable");
    }
    std::optional<std::string> OnHeartbeat() const
    {
        return ResRef("OnHeartbeat");
    }
    std::optional<std::uint8_t> OpenLockDC() const
    {
        return Number<std::uint8_t>("OpenLockDC");
    }
    std::optional<std::uint8_t> TrapDetectDC() const
    {
        return Number<std::uint8_t>("TrapDetectDC");
    }
    std::optional<std::string> OnSpellCastAt() const
    {
        return ResRef("OnSpellCastAt");
    }
    std::optional<std::uint8_t> Fort() const
    {
        return Number<std::uint8_t>("Fort");
    }
    std::optional<std::uint8_t> Will() const
    {
        return Number<std::uint8_t>("Will");
    }
    std::optional<std::string> OnInvDisturbed() const
    {
        return ResRef("OnInvDisturbed");
    }
    std::optional<std::uint8_t> TrapFlag() const
    {
        return Number<std::uint8_t>("TrapFlag");
    }
    std::optional<std::string> OnDisarm() const
    {
        return ResRef("OnDisarm");
    }
};

} // namespace utp

namespace utm
{

// StoreList[]/ItemList[] of a template.
class StoreListEntryItemListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::uint8_t> Infinite() const
    {
        return Number<std::uint8_t>("Infinite");
    }
    std::optional<std::int32_t> StackSize() const
    {
        return Number<std::int32_t>("StackSize");
    }
    std::optional<std::string> InventoryRes() const
    {
        return ResRef("InventoryRes");
    }
};

// StoreList[] of a template.
class StoreListEntry : public StructView
{
  public:
    using StructView::StructView;
    std::vector<StoreListEntryItemListEntry> ItemList() const
    {
        return List<StoreListEntryItemListEntry>("ItemList");
    }
};

// VarTable[] of a template.
class VarTableEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::string> Name() const
    {
        return Text("Name");
    }
    std::optional<std::uint8_t> Type() const
    {
        return Number<std::uint8_t>("Type");
    }
    std::optional<std::int32_t> Value() const
    {
        return Number<std::int32_t>("Value");
    }
};

// WillNotBuy[] of a template.
class WillNotBuyEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::int32_t> BaseItem() const
    {
        return Number<std::int32_t>("BaseItem");
    }
};

// WillOnlyBuy[] of a template.
class WillOnlyBuyEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::int32_t> BaseItem() const
    {
        return Number<std::int32_t>("BaseItem");
    }
};

// The top struct of a template.
class Store : public StructView
{
  public:
    using StructView::StructView;

    // Returns a file's top struct; none for a file with none.
    static std::optional<Store> Of(const gff3::File &file)
    {
        const auto root = file.Root();
        return root ? std::optional<Store>(Store(*root)) : std::nullopt;
    }
    std::optional<gff3::LocString> LocName() const
    {
        return Loc("LocName");
    }
    std::optional<std::string> Tag() const
    {
        return Text("Tag");
    }
    std::optional<std::string> EventScript() const
    {
        return ResRef("EventScript");
    }
    std::optional<std::int32_t> MarkDown() const
    {
        return Number<std::int32_t>("MarkDown");
    }
    std::optional<std::int32_t> MarkUp() const
    {
        return Number<std::int32_t>("MarkUp");
    }
    std::vector<WillNotBuyEntry> WillNotBuy() const
    {
        return List<WillNotBuyEntry>("WillNotBuy");
    }
    std::vector<WillOnlyBuyEntry> WillOnlyBuy() const
    {
        return List<WillOnlyBuyEntry>("WillOnlyBuy");
    }
    std::vector<StoreListEntry> StoreList() const
    {
        return List<StoreListEntry>("StoreList");
    }
    std::vector<VarTableEntry> VarTable() const
    {
        return List<VarTableEntry>("VarTable");
    }
};

} // namespace utm

namespace utt
{

// VarTable[] of a template.
class VarTableEntry : public StructView
{
  public:
    using StructView::StructView;
    std::optional<std::string> Name() const
    {
        return Text("Name");
    }
    std::optional<std::uint8_t> Type() const
    {
        return Number<std::uint8_t>("Type");
    }
    std::optional<std::int32_t> Value() const
    {
        return Number<std::int32_t>("Value");
    }
};

// The top struct of a template.
class Trigger : public StructView
{
  public:
    using StructView::StructView;

    // Returns a file's top struct; none for a file with none.
    static std::optional<Trigger> Of(const gff3::File &file)
    {
        const auto root = file.Root();
        return root ? std::optional<Trigger>(Trigger(*root)) : std::nullopt;
    }
    std::optional<std::string> Tag() const
    {
        return Text("Tag");
    }
    std::optional<std::string> TemplateResRef() const
    {
        return ResRef("TemplateResRef");
    }
    std::optional<std::string> EventScript() const
    {
        return ResRef("EventScript");
    }
    std::optional<std::uint32_t> GroupId() const
    {
        return Number<std::uint32_t>("GroupId");
    }
    std::vector<VarTableEntry> VarTable() const
    {
        return List<VarTableEntry>("VarTable");
    }
};

} // namespace utt

} // namespace dao::core::templates
