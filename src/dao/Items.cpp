// An item's requirements (dao/Items.h).
//
// Notes on an item's requirements: read 2026-10-25 (DAOLib #259) in the GOG exe and the inventory's
// and tooltip's movies, after the Eluvian server's, which asked a shown movie's
// DAExternalCommands.GetItemRequirements for text. The tooltip's lines are
// GuiFSCommand::GetItemRequirements (16344): unless the item ignores them
// (CCItem::IgnoresRequirements, 18012, its property list holding the flag), one per attribute,
// CCItem::GetRequirementCount (17983) of them (0 to 2, by the Items table's ReqProp1 and ReqProp2),
// each CCItem::GetRequirement (18004): the property and ReqPropNVal plus the material's
// Material Rules modifier, strength turned to magic for a creature with Combat Magic (17023) whose
// magic is the higher; met by CCCreature::MeetsItemRequirement (14705), the creature's total of it,
// bonuses with it. Then the item's own conditions (itemprps rows of IPType 3, each a script) and
// whether the creature has the item's slots. The inventory greys an item out and refuses its Equip
// by GuiFSCommand's CreatureCanUseItem, CCCreature::ForScript_CanUseItem (14746), the scripts'
// CanUseItem: all of these. The engine's EquipItem tests none of them (0x006BE330, 0x006BC7F0).
//
// Seen in play 2026-10-25 (examples/items), Lyna (strength 25, dexterity 44, magic 14,
// constitution 13) with the inventory open: 29 items of the backpack with requirements, the met ones
// and CanUse agreeing on each; a longsword asking strength 25 met and usable and a hammer asking 32
// not, so a requirement equal to the attribute is met; daggers' dexterity 26 met; staves' magic 20
// and 30 and crystals' constitution not. The item's own conditions (itemprps' scripts) are in
// CanUse and not listed.
#include "dao/Items.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/generated/ScriptApi.h"

namespace dao::items
{

namespace
{

CCItem *ItemOf(ObjectId id)
{
    CClientExoApp *app = CClientExoApp::Get();
    CGameObject *object = app != nullptr ? app->GetGameObjectById(id) : nullptr;
    return object != nullptr ? object->AsItem() : nullptr;
}

CCCreature *CreatureOf(ObjectId id)
{
    CClientExoApp *app = CClientExoApp::Get();
    return app != nullptr && id != kInvalidObject ? app->GetCreatureById(id) : nullptr;
}

} // namespace

std::vector<Requirement> Requirements(ObjectId item, ObjectId creature)
{
    std::vector<Requirement> needs;
    CCItem *found = ItemOf(item);
    if (found == nullptr)
    {
        return needs;
    }
    using IgnoresFn = bool(__fastcall *)(CCItem * item, int edx);
    using CountFn = std::int32_t(__fastcall *)(CCItem * item, int edx);
    using GetFn = void(__fastcall *)(CCItem * item, int edx, std::int32_t index, std::int32_t *property, float *value,
                                     CCCreature *creature);
    using MeetsFn = bool(__fastcall *)(CCCreature * creature, int edx, CCItem *item, std::int32_t index);
    if (reinterpret_cast<IgnoresFn>(Module::Address(addr::kCCItem_IgnoresRequirements))(found, 0))
    {
        return needs;
    }
    CCCreature *user = CreatureOf(creature);
    const std::int32_t count = reinterpret_cast<CountFn>(Module::Address(addr::kCCItem_GetRequirementCount))(found, 0);
    for (std::int32_t index = 0; index < count; ++index)
    {
        Requirement need;
        reinterpret_cast<GetFn>(Module::Address(addr::kCCItem_GetRequirement))(found, 0, index, &need.property,
                                                                               &need.value, user);
        if (need.property == 0)
        {
            continue;
        }
        need.met =
            user == nullptr ||
            reinterpret_cast<MeetsFn>(Module::Address(addr::kCCCreature_MeetsItemRequirement))(user, 0, found, index);
        needs.push_back(need);
    }
    return needs;
}

bool CanUse(ObjectId creature, ObjectId item)
{
    return script::api::CanUseItem(creature, item) != 0;
}

} // namespace dao::items
