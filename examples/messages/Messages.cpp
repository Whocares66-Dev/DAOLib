// Example: the engine's messages read by their fields (dao/Messages.h): the abilities', the party's, the items' and the
// conversations'.
//
// Each typed message is logged as it is delivered, whoever sent it: an ability
// pressed on the quickbar, a target clicked or the targeting left, an icon dropped on a slot, an
// ability picked on the level-up screen; a member selected by key or click, the party picker,
// the alerts and the tactics screen; an item equipped, taken, bought, sold, given or made, and
// the inventory's, store's and crafting screens' own; a reply chosen, a popup answered, the
// journal opened at a quest. A line gives the message, its receiver's class and
// each field. What it finds goes to example_messages.log.
#include "dao/Messages.h"
#include "dao/Log.h"
#include "dao/Plugin.h"
#include "dao/String.h"
#include "dao/TypeName.h"
#include "dao/generated/ScriptApi.h"

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <string>

namespace
{

namespace api = dao::script::api;
namespace messages = dao::messages;

std::string Named(dao::ObjectId object)
{
    return object != dao::kInvalidObject ? fmt::format("{} 0x{:x}", dao::Utf8(api::GetName(object)), object)
                                         : std::string("nobody");
}

// Logs every message of class M as `what` makes its fields into text.
template <class M, class F> void Log(const char *name, F what)
{
    messages::OnMessage<M>([name, what](void *receiver, const M &message) {
        spdlog::info("{} to {}: {}", name, dao::TypeName(receiver), what(message));
    });
}

std::string Empty(const messages::Message & /*message*/)
{
    return "no fields";
}

} // namespace

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_messages");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    Log<messages::PerformAbilityMessage>("PerformAbility", [](const messages::PerformAbilityMessage &m) {
        return fmt::format("ability {} by {}, item 0x{:x}", m.ability, Named(m.caster), m.item);
    });
    Log<messages::PerformAbilityOnTargetUnderCursorMessage>("PerformAbilityOnTargetUnderCursor", Empty);
    Log<messages::PerformAbilityOnSelectedTargetMessage>("PerformAbilityOnSelectedTarget", Empty);
    Log<messages::PerformAbilityOnPlayerMessage>(
        "PerformAbilityOnPlayer",
        [](const messages::PerformAbilityOnPlayerMessage &m) { return fmt::format("target {}", Named(m.target)); });
    Log<messages::CancelAbilityTargetingMessage>("CancelAbilityTargeting", Empty);
    Log<messages::ActivateLinearTargetModeMessage>("ActivateLinearTargetMode", Empty);
    Log<messages::ActivateAnalogTargetModeMessage>("ActivateAnalogTargetMode", Empty);
    Log<messages::SetQuickslotMessage>("SetQuickslot", [](const messages::SetQuickslotMessage &m) {
        return fmt::format(R"({} slot {} ability {} tag "{}" template "{}")", Named(m.creature), m.slot, m.ability,
                           dao::Utf8(m.tag.View()), dao::Utf8(m.refTemplate.View()));
    });
    Log<messages::ShowAbilityHeaderMessage>("ShowAbilityHeader", [](const messages::ShowAbilityHeaderMessage &m) {
        return fmt::format("header {} list {}", m.header, m.list);
    });
    Log<messages::HideAbilityHeaderMessage>("HideAbilityHeader", [](const messages::HideAbilityHeaderMessage &m) {
        return fmt::format("header {} list {}", m.header, m.list);
    });
    Log<messages::CharGenAssignAbilityMessage>(
        "CharGenAssignAbility", [](const messages::CharGenAssignAbilityMessage &m) {
            return fmt::format("{} ability {} kind {}", Named(m.creature), m.ability, m.kind);
        });
    Log<messages::FindAbilityChainsMessage>("FindAbilityChains", [](const messages::FindAbilityChainsMessage &m) {
        return fmt::format("{} type {} mode {}", Named(m.creature), m.type, m.mode);
    });
    Log<messages::CharGenSelectSpecializationMessage>(
        "CharGenSelectSpecialization", [](const messages::CharGenSelectSpecializationMessage &m) {
            return fmt::format("{} specialization {} next {}", Named(m.creature), m.specialization, m.next);
        });
    // The party's.
    Log<messages::SelectCharacterMessage>("SelectCharacter",
                                          [](const messages::SelectCharacterMessage &m) { return Named(m.creature); });
    Log<messages::SelectPartyMemberIndexMessage>("SelectPartyMemberIndex",
                                                 [](const messages::SelectPartyMemberIndexMessage &m) {
                                                     return fmt::format("index {} add {}", m.index, m.add);
                                                 });
    Log<messages::SelectPartyMemberUnderCursorMessage>(
        "SelectPartyMemberUnderCursor",
        [](const messages::SelectPartyMemberUnderCursorMessage &m) { return fmt::format("add {}", m.add); });
    Log<messages::UnselectPartyMemberMessage>("UnselectPartyMember", [](const messages::UnselectPartyMemberMessage &m) {
        return fmt::format("slot {}", m.slot);
    });
    Log<messages::HideAlertMessage>("HideAlert",
                                    [](const messages::HideAlertMessage &m) { return fmt::format("type {}", m.type); });
    Log<messages::HideUberAlertMessage>("HideUberAlert", Empty);
    Log<messages::GoToGuiWithFollowerMessage>("GoToGuiWithFollower", [](const messages::GoToGuiWithFollowerMessage &m) {
        return fmt::format("screen {} follower {}", dao::Utf8(m.screen.View()), Named(m.follower));
    });
    Log<messages::ApplyPartyPickerChangesMessage>("ApplyPartyPickerChanges", Empty);
    Log<messages::ClearPartyPickerMessage>("ClearPartyPicker", Empty);
    Log<messages::ExitPartyPickerMessage>("ExitPartyPicker", Empty);
    Log<messages::SubmitPartyChangesMessage>("SubmitPartyChanges", Empty);
    Log<messages::ShowPartyPickerMessage>("ShowPartyPicker", Empty);
    Log<messages::ToggleFullPartySelectMessage>("ToggleFullPartySelect", Empty);
    Log<messages::ToggleHoldPartyMessage>("ToggleHoldParty", Empty);
    Log<messages::ToggleRunWalkMessage>("ToggleRunWalk", Empty);
    Log<messages::CenterOnMainPartyMemberMessage>("CenterOnMainPartyMember", Empty);
    // The tactics screen's.
    Log<messages::DirtyPresetMessage>("DirtyPreset", Empty);
    Log<messages::LoadPresetMessage>("LoadPreset", [](const messages::LoadPresetMessage &m) {
        return fmt::format("index {} type {}", m.index, m.type);
    });
    Log<messages::SavePresetMessage>(
        "SavePreset", [](const messages::SavePresetMessage &m) { return fmt::format("slot {}", m.slot); });
    Log<messages::RefreshTacticsDataMessage>("RefreshTacticsData", [](const messages::RefreshTacticsDataMessage &m) {
        return fmt::format("from template {}", m.fromTemplate);
    });
    Log<messages::SetBehaviourMessage>("SetBehaviour", [](const messages::SetBehaviourMessage &m) {
        return fmt::format("behaviour {}", m.behaviour);
    });
    Log<messages::ValidateActionMessage>(
        "ValidateAction", [](const messages::ValidateActionMessage &m) { return fmt::format("row {}", m.row); });
    // The items'.
    Log<messages::EquipItemMessage>("EquipItem", [](const messages::EquipItemMessage &m) {
        return fmt::format("{} slot {}", Named(m.item), m.slot);
    });
    Log<messages::UnequipItemMessage>(
        "UnequipItem", [](const messages::UnequipItemMessage &m) { return fmt::format("{}", Named(m.item)); });
    Log<messages::DropItemMessage>("DropItem",
                                   [](const messages::DropItemMessage &m) { return fmt::format("{}", Named(m.item)); });
    Log<messages::AcquireItemMessage>(
        "AcquireItem", [](const messages::AcquireItemMessage &m) { return fmt::format("{}", Named(m.item)); });
    Log<messages::AcquireAllItemsMessage>("AcquireAllItems", [](const messages::AcquireAllItemsMessage &m) {
        return fmt::format("from {}", Named(m.container));
    });
    Log<messages::AttachRuneToItemMessage>("AttachRuneToItem", [](const messages::AttachRuneToItemMessage &m) {
        return fmt::format("{} in slot {}", Named(m.rune), m.slot);
    });
    Log<messages::DetachRuneFromItemMessage>("DetachRuneFromItem", [](const messages::DetachRuneFromItemMessage &m) {
        return fmt::format("slot {}", m.slot);
    });
    Log<messages::GiftItemMessage>("GiftItem", [](const messages::GiftItemMessage &m) {
        return fmt::format("{} to {}", Named(m.item), Named(m.recipient));
    });
    Log<messages::BuyItemMessage>("BuyItem", [](const messages::BuyItemMessage &m) {
        return fmt::format("{} buys {} x{}", Named(m.buyer), Named(m.item), m.quantity);
    });
    Log<messages::SellItemMessage>("SellItem", [](const messages::SellItemMessage &m) {
        return fmt::format("{} sells {} x{}", Named(m.seller), Named(m.item), m.quantity);
    });
    Log<messages::CraftItemMessage>("CraftItem", [](const messages::CraftItemMessage &m) {
        return fmt::format("recipe {} by {}", m.recipe, Named(m.crafter));
    });
    Log<messages::ClearNewlyAcquiredFlagMessage>(
        "ClearNewlyAcquiredFlag",
        [](const messages::ClearNewlyAcquiredFlagMessage &m) { return fmt::format("{}", Named(m.item)); });
    Log<messages::SetBackpackSortMessage>(
        "SetBackpackSort", [](const messages::SetBackpackSortMessage &m) { return fmt::format("sort {}", m.sort); });
    Log<messages::SetItemListFilterMessage>("SetItemListFilter", [](const messages::SetItemListFilterMessage &m) {
        return fmt::format("filter {}", m.filter);
    });
    Log<messages::ShowInventoryGUIMessage>("ShowInventoryGUI", [](const messages::ShowInventoryGUIMessage &m) {
        return fmt::format("with container {} {} can add {}", m.withContainer, Named(m.container), m.canAddItems);
    });
    Log<messages::ShowContainerGUIMessage>("ShowContainerGUI", [](const messages::ShowContainerGUIMessage &m) {
        return fmt::format("{} can add {}", Named(m.container), m.canAddItems);
    });
    Log<messages::GoToItemMessage>("GoToItem",
                                   [](const messages::GoToItemMessage &m) { return fmt::format("{}", Named(m.item)); });
    Log<messages::ShowCraftingGUIMessage>(
        "ShowCraftingGUI", [](const messages::ShowCraftingGUIMessage &m) { return fmt::format("skill {}", m.skill); });
    Log<messages::StartDraggingMessage>(
        "StartDragging", [](const messages::StartDraggingMessage &m) { return fmt::format("at {} {}", m.x, m.y); });
    Log<messages::StopDraggingMessage>(
        "StopDragging", [](const messages::StopDraggingMessage &m) { return fmt::format("aborted {}", m.aborted); });
    Log<messages::ShowItemExamineMessage>("ShowItemExamine", [](const messages::ShowItemExamineMessage &m) {
        return fmt::format("{} right {} requirements {}", dao::Utf8(m.item.View()), m.onRightSide, m.checkRequirements);
    });
    Log<messages::CountIngredientsMessage>("CountIngredients", Empty);
    Log<messages::OpenStoreMessage>("OpenStore", Empty);
    Log<messages::CloseStoreMessage>("CloseStore", Empty);
    Log<messages::InventoryClosedMessage>("InventoryClosed", Empty);
    Log<messages::ContainerChangedMessage>("ContainerChanged", Empty);
    Log<messages::PartyBackpackChangedMessage>("PartyBackpackChanged", Empty);
    Log<messages::PartyPlotItemsChangedMessage>("PartyPlotItemsChanged", Empty);
    Log<messages::EquipmentChangedMessage>("EquipmentChanged", Empty);
    Log<messages::StoreChangedMessage>("StoreChanged", Empty);
    Log<messages::SelectedGUICharacterChangedMessage>("SelectedGUICharacterChanged", Empty);
    Log<messages::ForceInventoryRefreshMessage>("ForceInventoryRefresh", Empty);
    Log<messages::HideContainerGUIMessage>("HideContainerGUI", Empty);
    Log<messages::HideInventoryGUIMessage>("HideInventoryGUI", Empty);
    Log<messages::HideCraftingGUIMessage>("HideCraftingGUI", Empty);
    Log<messages::ShowItemSplitDialogMessage>("ShowItemSplitDialog", Empty);
    // The conversations', popups' and journal's.
    Log<messages::SelectResponseMessage>(
        "SelectResponse", [](const messages::SelectResponseMessage &m) { return fmt::format("reply {}", m.index); });
    Log<messages::ScriptDialogResultMessage>("ScriptDialogResult", [](const messages::ScriptDialogResultMessage &m) {
        return fmt::format("popup {} button {} owner {}", m.popup, m.button, Named(m.owner));
    });
    Log<messages::GUIPRCDialogResultMessage>("GUIPRCDialogResult", [](const messages::GUIPRCDialogResultMessage &m) {
        return fmt::format("button {} content {}", m.button, m.content);
    });
    Log<messages::GoToQuestMessage>("GoToQuest", [](const messages::GoToQuestMessage &m) {
        return fmt::format("{} completed {}", dao::Utf8(m.plot.View()), m.completed);
    });
    Log<messages::GoToCodexMessage>(
        "GoToCodex", [](const messages::GoToCodexMessage &m) { return fmt::format("{}", dao::Utf8(m.entry.View())); });
    Log<messages::AcceptChantersQuestMessage>("AcceptChantersQuest", [](const messages::AcceptChantersQuestMessage &m) {
        return fmt::format("entry {}", m.entry);
    });
    Log<messages::SkipDialogMessage>("SkipDialog", Empty);
    Log<messages::SkipCutsceneMessage>("SkipCutscene", Empty);
    Log<messages::SkipMovieMessage>("SkipMovie", Empty);
    Log<messages::DialogBoxClosedMessage>("DialogBoxClosed", Empty);
    Log<messages::ShowDialogBoxMessage>("ShowDialogBox", Empty);
    Log<messages::ShowJournalGUIMessage>("ShowJournalGUI", Empty);
    Log<messages::HideJournalGUIMessage>("HideJournalGUI", Empty);
    Log<messages::GoToJournalPRCTabMessage>("GoToJournalPRCTab", Empty);
    return true;
}
