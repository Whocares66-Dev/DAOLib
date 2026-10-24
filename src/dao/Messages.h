// Sends, watches and intercepts the engine's messages. The engine's classes talk to each other
// through messages, for example a key press, an ability chosen on the quickbar, or the game being
// paused. Each receiving class has a handler per message class (dev/MESSAGE_HANDLERS.md lists
// them). A handler is the member function `bool Owner::HandleMessage(const Message &)`,
// __thiscall with `ret 4`. dao/generated/MessageHandlers.h gives every handler's address, so you
// can detour one:
//
//   using Fn = dao::messages::TypedHandlerFn<dao::messages::PerformAbilityMessage>;
//   dao::Detour<Fn> g_performAbility;
//   bool __fastcall PerformAbility(void *explore, int edx, const dao::messages::PerformAbilityMessage *m)
//   {
//       if (!MyTurn(m->caster))
//           return true; // report it handled, so the order is dropped
//       return g_performAbility.Original()(explore, edx, m);
//   }
//   g_performAbility.Attach(dao::messages::handler::GameModeExplore_PerformAbilityMessage, &PerformAbility);
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/core/Msvc8.h"
#include "dao/generated/MessageHandlers.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace dao::messages
{

// The base of every engine message (the engine's Messages::Message). A message starts with its
// vtable, and the message's own fields follow.
struct Message
{
    const void *vtable;
};

// The function type of a handler for messages of class M, in the form dao::Detour takes: the
// handler's owner, an unused edx, and the message. A handler returns true if it handled the
// message.
template <class M> using TypedHandlerFn = bool(__fastcall *)(void *owner, int edx, const M *message);

// Returns the message as class M if it is one, checked by comparing its vtable with M's.
// @return the message as M, or null if it is null or of another class
template <class M> const M *As(const Message *message)
{
    return message != nullptr && message->vtable == reinterpret_cast<const void *>(Module::Address(M::kVftable))
               ? static_cast<const M *>(message)
               : nullptr;
}

// Creates a message of class M, ready to send, with its vtable set and its fields zeroed.
// The message classes here hold only plain fields, so nothing needs to be freed afterwards.
template <class M> M Make()
{
    M message{};
    message.vtable = reinterpret_cast<const void *>(Module::Address(M::kVftable));
    return message;
}

// Sends a message to an object, the same way the engine sends its own. The receiver's handler
// for the message's class runs immediately, on the calling thread.
//
//   auto *modes = dao::CClientExoApp::Get()->GameModes();
//   auto message = dao::messages::Make<dao::messages::CenterOnObjectMessage>();
//   message.object = creature;
//   const bool handled = dao::messages::Send(modes->CurrentModeObject(), message);
//
// @param receiver the object that handles the message, such as a game mode or the game mode
// controller
// @param message the message; the caller keeps ownership of it
// @return true if the receiver handled it; false if it has no handler for this message or is not
// accepting messages
//
// Call it on the game thread, for example from a frame callback or a command intercept. That is
// where the engine's own handlers run.
inline bool Send(void *receiver, const Message &message)
{
    using Fn = bool(__cdecl *)(const Message *, void *);
    return reinterpret_cast<Fn>(Module::Address(addr::kMessages_Send))(&message, receiver);
}

// Builds a message from a line of text and sends it to the receiver the text names, the same way
// a button in one of the game's movies sends its callback.
//
//   dao::messages::SendText(L"CClientExoApp.GUIDialogResult 1 0");
//
// @param text "Receiver.Message fields": the receiver's name in the data tree (dao/Data.h), such
// as CClientExoApp or GUIPopupLayer, then the message's class name without "Message", then its
// fields in order, separated by spaces
// @return true if the message was sent, whether or not the receiver handled it; false, with
// nothing sent, if the engine does not know the receiver or the message
//
// The engine parses the fields itself, so you can send any message it can build from text, even
// one with no struct here. Call it on the game thread, like Send.
bool SendText(std::wstring_view text);

// Builds a message from a key binding's text and sends it to the game mode controller, as
// pressing the key does. The controller passes it to the interface first and then to the current
// mode.
//
//   dao::messages::SendBinding(L"SaveGameMessage -1 2"); // what the quicksave key sends
//
// @param text the message's class name and fields as KeyBindings.ini writes them, for example
// "UserTogglePauseMessage" or "SelectPartyMemberIndexMessage 0 1"
// @return true if the message was sent, whether or not anything handled it; false, with nothing
// sent, if the key bindings do not know the message or the game modes do not exist yet
//
// A key only produces its message in the modes its binding is registered for, but SendBinding
// sends it in any mode. A mode with no handler for it ignores it. Call it on the game thread,
// like Send.
bool SendBinding(std::wstring_view text);

// A function called with each message the engine delivers, and its receiver, just before the
// receiver's handler runs.
using MessageObserver = std::function<void(void *receiver, const Message &message)>;
// A function that can take a message before the receiver's handler runs. Return true to take it:
// no handler then runs, and the sender is told the message was handled.
using MessageInterceptor = std::function<bool(void *receiver, const Message &message)>;

// Calls an observer for every message the engine delivers.
// The engine often offers a message to several receivers, or passes it from one handler to
// another. The observer sees it only once, at the first receiver that has a handler for it, and
// never sees a message that no receiver handles. Every message passes through here, including
// mouse moves, so keep the observer fast. Observers run on the game thread.
void OnAnyMessage(MessageObserver observer);
// Calls an observer for every message of one class, like OnAnyMessage.
// Use this overload for a message class with no struct here.
// @param vftable the vftable ID of the message class; dev/messages.csv lists them, and
// dao::TypeName gives a message's class name
void OnMessage(AddressId vftable, MessageObserver observer);
// Lets an interceptor take messages of one class before their handler runs.
// The interceptor sees a message at the first receiver with a handler for it, so taking it there
// keeps it from every later handler too. Once one interceptor takes a message, later
// interceptors are not asked, but observers still see it. Use this overload for a message class
// with no struct here.
// @param vftable the vftable ID of the message class
void InterceptMessage(AddressId vftable, MessageInterceptor interceptor);

// Calls an observer for every message of class M, like OnAnyMessage.
//
//   dao::messages::OnMessage<dao::messages::SetGameModeMessage>(
//       [](void *, const dao::messages::SetGameModeMessage &m) { spdlog::info("mode {}", m.mode); });
template <class M> void OnMessage(std::function<void(void *receiver, const M &message)> observer)
{
    OnMessage(M::kVftable, [observer = std::move(observer)](void *receiver, const Message &message) {
        observer(receiver, static_cast<const M &>(message));
    });
}
// Lets an interceptor take messages of class M before their handler runs. This is the typed
// form of InterceptMessage, with the same rules.
//
//   dao::messages::InterceptMessage<dao::messages::PerformAbilityMessage>(
//       [](void *, const dao::messages::PerformAbilityMessage &m) { return !MyTurn(m.caster); });
template <class M> void InterceptMessage(std::function<bool(void *receiver, const M &message)> interceptor)
{
    InterceptMessage(M::kVftable, [interceptor = std::move(interceptor)](void *receiver, const Message &message) {
        return interceptor(receiver, static_cast<const M &>(message));
    });
}

// All message classes, their sizes and fields, are in dev/messages.csv
// (tools/messages.py, from each class's clone).

// Notes on PerformAbilityMessage: 0x10 bytes, from its clone (0x0077B210). The third word is the
// item the ability comes from, not a target (read 2026-10-24, DAOLib #253, correcting the reading
// of 2026-09-30): the quickbar's icon sends its SourceItem.ID (QuickBarAbilityIcon.OnButtonAction,
// quickbar.gfx), and the handler looks it up (0x00711BF8) and gives the may-use check the item's tag
// (+0xB8) and UseAbility (0x00711680) the id as its item; the targeting state keeps it at +0x48 for
// the click that ends the targeting. In play, 2026-10-24, the quickbar sent 0 for a talent and for
// the Litany of Adralla's slot (its SourceItem.ID read none). The handler (GameModeExplore's,
// 0x00711B50, which combat mode inherits), read 2026-09-30, takes no target from the message.
// Once the ability may be used (0x0071CE90), it reads the ability's row of abi_base
// (TargetType, UseType, AutoTarget) and uses the ability (UseAbility, dao/Addresses.h) on, in
// order:
// - the caster, if the ability targets only its user, or an input (its action 0x2F, taken for
//   self-casting, not checked) is held and it may target its user, or it is an active mode being
//   turned off (UseType 2);
// - the selected target (a global, 0x00C06794, set through 0x0070D030 by the Select Next and
//   Previous CreatureTarget, HostileTarget and UsableObject messages, the keys that cycle
//   targets), if the ability may be used on it;
// - the caster's attack target, if the ability's AutoTarget is set and it may be used on it;
// and failing all of them it asks for a target (targeting mode, 0x0070BA30), which a click then
// answers with PerformAbilityOnTargetUnderCursorMessage.

// The player's order for a creature to use an ability, from the quickbar or a radial menu.
// It names no target: the handler uses the ability on the caster, the selected target or the
// caster's attack target, whichever the ability allows, and otherwise asks the player to pick a
// target.
struct PerformAbilityMessage : Message
{
    DAO_VFTABLE(68725);
    std::int32_t ability; // the ability's ID: its row in the abi_base data table
    ObjectId caster;
    ObjectId item; // the item whose ability it is, as the quickbar sends it; 0 for none
};
static_assert(sizeof(PerformAbilityMessage) == 0x10);

// Notes on the targeting messages: read 2026-10-24 (DAOLib #253) from their handlers, all
// GameModeExplore's (combat mode inherits them). While an ability waits for a target, explore
// mode's targeting state (0x00C35F60) holds it: the ability at +0x40, the item it comes from
// at +0x48, the ability's TargetType at +0x4C, the caster at +0x50. Each of the three below takes
// a target, tests it with ValidTarget (0x0070D550) and, if it passes, uses the pending ability on
// it (0x00711680, which then leaves targeting, 0x007113F0); with no ability pending but a
// script's RequestTarget waiting (+0x54), the target's place goes to the script instead
// (0x0070D8D0). OnTargetUnderCursor's (0x007116C0) target is the object under the cursor
// (0x00C35FF0), else the ground clicked; it is bound to the left button in the targeting
// contexts (the input table, 0x006E01DB, 0x006E67B9). OnSelectedTarget's (0x00711A00) is the
// selected target (0x00C06794), and its one binding is a gamepad button (0x006E69BD).
// OnPlayer's (0x00711940) is the object at +4, looked up by id (0x005D5BE0): no binding and no
// sender in the exe or the movies. CancelAbilityTargeting's handler (0x00711AC0) plays the
// interface's "Click" and leaves targeting; Escape, the right button and a pad's button send it
// in the targeting contexts (0x006E683D, 0x006E68BD, 0x006E6942). ActivateLinearTargetMode's
// (0x0070D650) and ActivateAnalogTargetMode's (0x0070D680) select a target as the target-cycle
// keys do (0x0070D030), the creature explore mode has at +0xBC if usable, else the selected
// one; the pad's bindings send them. RequestTargetMessage (0xC bytes) and IsValidTargetMessage
// (8) have no handler and no sender: the script command RequestTarget calls the mode's
// 0x0070BB70 directly. They are not typed here. In play, 2026-10-24 (examples/messages): Dirty
// Fighting pressed on the quickbar (its movie's ActivateQuickslot), then Escape's binding, logged PerformAbility and
// CancelAbilityTargeting; pressed again with the cursor held on a walking corpse, the left
// click's binding logged OnTargetUnderCursor and Lyna's command queue took the ability;
// OnPlayer sent with the corpse's id arrived with it; the pad's three arrived with no fields.

// Uses the ability waiting for a target, after PerformAbilityMessage asked for one, on the
// object under the cursor, or the ground there. A left click in targeting mode sends it.
struct PerformAbilityOnTargetUnderCursorMessage : Message
{
    DAO_VFTABLE(68786);
};
static_assert(sizeof(PerformAbilityOnTargetUnderCursorMessage) == 4);

// Uses the ability waiting for a target on the selected target, the one the target-cycle keys
// choose. A gamepad's button sends it.
struct PerformAbilityOnSelectedTargetMessage : Message
{
    DAO_VFTABLE(68789);
};
static_assert(sizeof(PerformAbilityOnSelectedTargetMessage) == 4);

// Uses the ability waiting for a target on the object given. Nothing in the game sends it.
struct PerformAbilityOnPlayerMessage : Message
{
    DAO_VFTABLE(68790);
    ObjectId target; // used if the pending ability may be used on it
};
static_assert(sizeof(PerformAbilityOnPlayerMessage) == 8);

// Leaves targeting mode with the ability unused. Escape or the right button sends it there.
struct CancelAbilityTargetingMessage : Message
{
    DAO_VFTABLE(68787);
};
static_assert(sizeof(CancelAbilityTargetingMessage) == 4);

// Selects a target as the target-cycle keys do, for a gamepad's linear targeting.
struct ActivateLinearTargetModeMessage : Message
{
    DAO_VFTABLE(68720);
};
static_assert(sizeof(ActivateLinearTargetModeMessage) == 4);

// Selects a target as the target-cycle keys do, for a gamepad's radial targeting.
struct ActivateAnalogTargetModeMessage : Message
{
    DAO_VFTABLE(68721);
};
static_assert(sizeof(ActivateAnalogTargetModeMessage) == 4);

// Notes on SetQuickslotMessage: 0x48 bytes; its clone (0x0070AF00) copies the three words and
// the two strings (0x004C2510). Read 2026-10-24 (DAOLib #253): explore mode's handler
// (0x0070BE30) looks the creature up and hands the message to the creature's (0x006A74B0), which
// sets the slot of its quickbar (0x0071F230, the creature's stats at +0x7C4; nothing without
// their interface object, +0x1B4) and rebuilds the bar (0x00719AB0). A slot of -1 is the first
// free one (0x0062AD80); otherwise it must be at most 49 (0x0071F360). With a tag the slot is an
// item's, found through the party's inventory (0x005D4FC0); an empty template is the item's own
// (item +0x1CC). The quickbar movie sends it as an icon is dropped on a slot or dragged off
// (QuickBarIconSlot.as, SendMessageArgs(<player token>.SetQuickslot, creature, slot, ability,
// tag, template); a slot cleared sends ability 0 and empty strings). Its "GUI.SelectedChar" is
// the controlled creature, whose handler takes it: in play, 2026-10-24, the movie's SetQuickslot
// on two slots arrived at the CCCreature with slot 0, ability 11 and no strings, and slot 6,
// ability 200203 and cir000im_litany for both strings (examples/messages).

// Puts an ability or an item in a slot of a creature's quickbar, or clears the slot. Sent to the
// creature, as the quickbar does when an icon is dropped, or to the game modes, which pass it on.
struct SetQuickslotMessage : Message
{
    DAO_VFTABLE(67741);
    // Makes the message with the creature, slot and ability 0 and the strings empty.
    SetQuickslotMessage() : Message{}
    {
    }
    ObjectId creature{};
    std::int32_t slot = 0;        // the slot, 0 to 49, or -1 for the first free one
    std::int32_t ability = 0;     // the ability's row in abi_base, or 0 to clear the slot
    core::WStringRep tag;         // an item's tag for an item's slot, else empty
    core::WStringRep refTemplate; // an item's template, or empty for the tagged item's own
};
static_assert(sizeof(SetQuickslotMessage) == 0x48);

// Notes on the ability headers' messages: 8 bytes each, clones 0x0077F98D (Hide) and 0x0077FA69
// (Show). Read 2026-10-24 (DAOLib #253): the creature's handlers keep a set of minimized
// headers, the skills' (creature +0x70C) for a list of 3 and the talents' (+0x728) for any other,
// the saves' MinimizedSkillHeaderList and MinimizedTalentHeaderList (16471, 16472, header ids
// s16, dev/SAVE_WRITER.md). Hide (0x006B75B0) adds the header if it is not there (0x006AE450,
// 0x006B1140); Show (0x006B0980) takes it out (0x006AFF00, 0x006AF840). The message names no
// creature: it goes to the creature's own handler. No movie of the install sends either. In
// play, 2026-10-24, their bindings' text ("ShowAbilityHeaderMessage 7 3", "Hide... -2 1") was
// parsed by the engine into header 7, list 3 and header -2, list 1.

// Expands an ability header in a creature's ability lists: takes it out of the minimized ones.
// Sent to the creature.
struct ShowAbilityHeaderMessage : Message
{
    DAO_VFTABLE(68807);
    std::int16_t header; // the header's id
    std::uint8_t list;   // 3 for the skills' headers, anything else for the talents'
};
static_assert(sizeof(ShowAbilityHeaderMessage) == 8);

// Collapses an ability header in a creature's ability lists: adds it to the minimized ones.
// Sent to the creature.
struct HideAbilityHeaderMessage : Message
{
    DAO_VFTABLE(68806);
    std::int16_t header; // the header's id
    std::uint8_t list;   // 3 for the skills' headers, anything else for the talents'
};
static_assert(sizeof(HideAbilityHeaderMessage) == 8);

// Notes on the ability screens' messages: 0x10 bytes each, three words, as their clones copy.
// Read 2026-10-24 (DAOLib #253), with dev/CHARGEN.md's reading of the scripts' side.
// CharGenAssignAbility: character creation's handler (GUICharGenBase, 0x007FCE60) raises the
// module's EVENT_TYPE_CHARGEN_ASSIGN_ABILITIES (51) with the creature as object 0, the ability as
// int 0 and the kind as int 1, delivered at once (0x0064AD80); the paperdoll's (0x007B8900) adds
// the ability to the abilities it shows picked (+0xF0), or takes -ability out. The level-up and
// creation movies send it as an ability is picked (chargen.gfx, levelup.gfx: GUICharGen.,
// Paperdoll. and GUILevelUp.CharGenAssignAbility). FindAbilityChains: GameModeGUI's (0x00718090)
// and GameModeCharGen's (0x00719450) handlers rebuild the creature's list of ability chains of
// the type (0x0062E9C0: 1 talents, 2 spells, 3 skills, any other none), the mode passed to the
// walk (0x0062DCD0). The movies send their display mode (Ability.DISPLAYMODE_*: 1 character
// creation, 2 level-up, 4 the spellbook, the skills and talents screens). In play, 2026-10-24:
// the skills screen sent type 3, mode 4; the level-up screen types 1, 2 and 3, mode 2; a talent
// taken there sent CharGenAssignAbility to GUICharGen with ability 8, kind 0.
// CharGenSelectSpecialization: character creation's handler (0x00736B80) raises the module's
// event 52 (CHARGEN_SELECT_LEVELUP_CLASS) with the creature and the specialization, if not 0, and with the flag set
// moves the screen on (dev/CHARGEN.md); no movie sends it, and its text is no binding's, so it was not seen in play.

// Picks an ability, or unpicks one, on the level-up or character creation screen. Sent to the
// screen (GUICharGen), as the movie does when an ability is clicked.
struct CharGenAssignAbilityMessage : Message
{
    DAO_VFTABLE(69370);
    ObjectId creature;
    std::int32_t ability; // the ability's row in abi_base; its negative to unpick it
    std::int32_t kind;    // 0 a skill, talent or spell, 1 a specialization
};
static_assert(sizeof(CharGenAssignAbilityMessage) == 0x10);

// Rebuilds a creature's chains of abilities of one type, as the ability screens show them.
// Sent to the game modes, as the movies do after each pick.
struct FindAbilityChainsMessage : Message
{
    DAO_VFTABLE(67880);
    ObjectId creature;
    std::int32_t type; // 1 talents, 2 spells, 3 skills
    std::int32_t mode; // the screen's: 1 character creation, 2 level-up, 4 the spellbook
};
static_assert(sizeof(FindAbilityChainsMessage) == 0x10);

// Chooses a specialization on the character creation screen. Nothing in the game sends it.
struct CharGenSelectSpecializationMessage : Message
{
    DAO_VFTABLE(68334);
    ObjectId creature;
    std::int32_t specialization; // 0 chooses none
    std::int32_t next;           // not 0: the screen moves on to its next step
};
static_assert(sizeof(CharGenSelectSpecializationMessage) == 0x10);

// Notes on the party's messages: read 2026-10-24 (DAOLib #253) from their clones, their text
// readers (each class's slot 5, through Messages::InputStringDataStream's typed readers: +0x20 an
// unsigned word, +0x24 a signed one, +0x28 an unsigned half, +0x30 a byte, +0x34 a bool, +0x14
// a std::wstring) and their handlers. A binding's text (dao::messages::SendBinding) reaches only
// what the current game mode handles, which is why SelectCharacterMessage, CCParty's alone, is
// not one (dev/PARTY.md); a movie's text names its receiver ("Party.HideAlert 1").
// SelectCharacter: CCParty's handler (0x00639F90) selects the creature alone (Party::Select,
// 0x00639E50, add 0), which signals it EVENT_TYPE_ON_SELECT; nothing in the game sends it.
// SelectPartyMemberIndex: CCParty's (0x00639FB0) selects the member at the index of the party's
// list (+0xC8), out of range nothing; explore mode's (0x0070B9E0) passes it on. The keys F1 to F4
// send "SelectPartyMemberIndexMessage 0 0" to "3 0", with Shift "0 1" to "3 1" (the input table,
// 0x006E5300 to 0x006E578F). SelectPartyMemberUnderCursor: explore mode's (0x0070C0B0) selects
// the creature under the cursor (+0xCC), adding with the flag; the left button's release sends
// 0, with Shift 1 (0x006E63C3, 0x006E6447). The party picker's (0x00768AE0) ignores the flag and
// picks by its own ray. UnselectPartyMember: the picker's (0x00767100) takes the member in the
// slot (0 to 3, each entry's +0x24) out of the party being picked, unless it is the hero, which
// plays the picker's error sound; the picker's portraits send it (PartyPickerPortrait.as, partypicker.gfx).
// HideAlert: CCParty's (0x0062F220) dismisses the first alert of the type (the alerts' manager,
// party +0x330, 0x00628970); notifications.gfx's Alert.as sends "Party.HideAlert <type>" as an
// alert fades or is clicked. HideUberAlert (0x0062F240) dismisses the first of the large alerts.
// GoToGuiWithFollower: the GUI system's (0x0060B430) shows the full-screen screen named for the
// follower if it is in the party's pool (0x006316F0); the alerts send it for the tactics,
// ability and approval alerts ("Tactics", "Skills" or "SpellsTalents", "CharacterRecord").
// The fieldless: ApplyPartyPickerChanges (CCParty 0x00636F50) raises the follower events for the
// members whose state changed (dev/PARTY.md), and nothing sends it; ClearPartyPicker (the
// picker's 0x00767230) unselects slots 0 to 3, the picker's Remove All; ExitPartyPicker (0x00765960)
// fades the picker's scene and leaves, sent by nothing (Submit calls it); SubmitPartyChanges
// (0x00767060), the picker's Accept; ShowPartyPicker (0x007183A0) switches to the picker's mode
// (12), from the navigation bar and the script command ShowPartyPickerGUI; ToggleFullPartySelect
// (CCParty 0x00636E40) selects every controllable living member, or the controlled one alone;
// ToggleHoldParty (0x0062F1F0) flips the party's HoldPositions, key H; ToggleRunWalk
// (0x0062EC60) flips party +0x4A8, numpad /; CenterOnMainPartyMember (explore's 0x0070C0E0)
// snaps the camera to the creature it follows, bound in the cursor contexts. In play,
// 2026-10-24 (examples/messages, Leliana's origin with Tug and Sketch): the keys' bindings
// arrived with index 1 add 0, index 0 add 1, index 0 add 0, and the left click's with add 1 and
// 0 over Tug; "Party.SelectCharacterMessage 19" at CCParty with Leliana; the notifications movie
// sent HideAlert type 4 as the level-up alert went; "GUI.GoToGuiWithFollowerMessage Tactics 224"
// arrived with the screen and Tug, and the tactics screen opened for him; ShowPartyPicker opened
// the picker, which took UnselectPartyMember slot 1, ClearPartyPicker, ExitPartyPicker and, on a
// second opening, SubmitPartyChanges, the party unchanged; the fieldless toggles each arrived.

// Selects one creature alone, as if the player picked it. Sent to the party (its receiver,
// "Party"); nothing in the game sends it.
struct SelectCharacterMessage : Message
{
    DAO_VFTABLE(68761);
    ObjectId creature;
};
static_assert(sizeof(SelectCharacterMessage) == 8);

// Selects the party member at a place in the party bar, as F1 to F4 do, or adds it to the
// selection, as Shift with them does. Sent to the game modes, which pass it to the party.
struct SelectPartyMemberIndexMessage : Message
{
    DAO_VFTABLE(67739);
    std::uint16_t index; // the member's place in the bar, from 0
    std::uint8_t add;    // 1 adds it to the selection, 0 selects it alone
};
static_assert(sizeof(SelectPartyMemberIndexMessage) == 8);

// Selects the party member under the cursor, as a left click does, or adds it, as Shift-click
// does. In the party picker it picks the figure under the cursor and the flag is ignored.
struct SelectPartyMemberUnderCursorMessage : Message
{
    DAO_VFTABLE(68766);
    std::uint8_t add; // 1 adds it to the selection, 0 selects it alone
};
static_assert(sizeof(SelectPartyMemberUnderCursorMessage) == 8);

// Takes a member out of the party being picked in the party picker, as a click on its portrait
// does. The hero cannot be taken out: the picker plays its error sound.
struct UnselectPartyMemberMessage : Message
{
    DAO_VFTABLE(68638);
    std::uint8_t slot; // the picked member's portrait, 0 to 3
};
static_assert(sizeof(UnselectPartyMemberMessage) == 8);

// Dismisses the first alert of a type, as the alert area does when one fades or is clicked.
// Sent to the party ("Party.HideAlert").
struct HideAlertMessage : Message
{
    DAO_VFTABLE(65631);
    // The alert's type, the notification movie's Alert.ALERT_*: 0 custom, 1 quest, 2 codex,
    // 3 items, 4 level-up, 5 money, 6 tactics, 7 ability, 8 approval, 9 new content, 10 world
    // map, 11 specialization, 12 connection lost.
    std::int32_t type;
};
static_assert(sizeof(HideAlertMessage) == 8);

// Dismisses the first of the large alerts. Sent to the party ("Party.HideUberAlert").
struct HideUberAlertMessage : Message
{
    DAO_VFTABLE(65632);
};
static_assert(sizeof(HideUberAlertMessage) == 4);

// Opens a full-screen screen showing a follower, as an alert's click does. Sent to the GUI
// system ("GUI.GoToGuiWithFollowerMessage"); the follower must be in the party's pool.
struct GoToGuiWithFollowerMessage : Message
{
    DAO_VFTABLE(65176);
    // Makes the message with no screen and no follower.
    GoToGuiWithFollowerMessage() : Message{}
    {
    }
    core::WStringRep screen; // the screen's name: "Tactics", "Skills", "SpellsTalents", "CharacterRecord"
    ObjectId follower{};
};
static_assert(sizeof(GoToGuiWithFollowerMessage) == 0x24);

// Raises the follower events for the members whose state the party picker changed. Sent to the
// party; nothing in the game sends it.
struct ApplyPartyPickerChangesMessage : Message
{
    DAO_VFTABLE(68773);
};
static_assert(sizeof(ApplyPartyPickerChangesMessage) == 4);

// Takes every member but the hero out of the party being picked: the picker's Remove All.
struct ClearPartyPickerMessage : Message
{
    DAO_VFTABLE(68813);
};
static_assert(sizeof(ClearPartyPickerMessage) == 4);

// Leaves the party picker as it is. The picker's Accept leaves through it.
struct ExitPartyPickerMessage : Message
{
    DAO_VFTABLE(68637);
};
static_assert(sizeof(ExitPartyPickerMessage) == 4);

// Takes the party picked and leaves the picker: its Accept.
struct SubmitPartyChangesMessage : Message
{
    DAO_VFTABLE(68812);
};
static_assert(sizeof(SubmitPartyChangesMessage) == 4);

// Opens the party picker, as its button on the navigation bar does.
struct ShowPartyPickerMessage : Message
{
    DAO_VFTABLE(65854);
};
static_assert(sizeof(ShowPartyPickerMessage) == 4);

// Selects every controllable living party member, or the controlled one alone if all are.
struct ToggleFullPartySelectMessage : Message
{
    DAO_VFTABLE(68764);
};
static_assert(sizeof(ToggleFullPartySelectMessage) == 4);

// Turns the party's Hold Position on or off, as the H key does.
struct ToggleHoldPartyMessage : Message
{
    DAO_VFTABLE(68765);
};
static_assert(sizeof(ToggleHoldPartyMessage) == 4);

// Switches the party between running and walking, as numpad / does.
struct ToggleRunWalkMessage : Message
{
    DAO_VFTABLE(68703);
};
static_assert(sizeof(ToggleRunWalkMessage) == 4);

// Snaps the camera to the creature it follows.
struct CenterOnMainPartyMemberMessage : Message
{
    DAO_VFTABLE(68769);
};
static_assert(sizeof(CenterOnMainPartyMemberMessage) == 4);

// Notes on the tactics screen's messages: read 2026-10-24 (DAOLib #253), each handler
// PartyTactics's (registered in its constructor, 0x007B1120), the screen's object for the
// follower at its +0x58, with its attributes (+0x5C Enabled, +0x6C TacticsList, +0x88
// PresetIdList, +0xA8 the custom slots, +0xC4 PresetIndex, +0xD4 PresetType, +0xE4 Dirty;
// dev/PARTY.md). The tactics movie (tactics.gfx, patch003.erf's) sends all six as
// "GUI.SelectedChar.Tactics.<name> args", and nothing else under that token. DirtyPreset
// (0x007AD100) sets Dirty, so the preset's name shows a "*" and GetTacticPresetID answers 0;
// sent as a row is changed. LoadPreset (0x007AE4B0) clears Dirty and, for type 2, turns the
// tactics on and loads the preset at the index of PresetIdList (SetTacticPresetID, 0x007ADB40;
// event 83 fills the rows), for type 1 the custom slot, for type 0 turns them off
// (PresetPicker.as: "<index> 2" for the presets, "<index> 1" for the custom slots). SavePreset
// (0x007AE780) copies the rows into the custom slot (0x007AE160), out of range nothing, and
// sets PresetIndex, PresetType 1, Dirty 0; Tactics.as sends the slot picked in its Save dialog.
// RefreshTacticsData (0x007AD170) refreshes every row's item (0x007AD070); with the flag set a
// Use Item row with no item finds its name and icon from the saved template. The movie sends 1
// as the list is rebuilt. SetBehaviour (0x007AD120) sets the follower's
// PROPERTY_SIMPLE_AI_BEHAVIOR (18) to the row of aibehaviors (table 187); BehaviourPicker.as
// sends the row picked. ValidateAction (0x007B0170) checks the row's command against its
// condition (ConditionBase, table 137) and changes the command if the condition does not allow
// it (dev/PARTY.md); Tactics.as sends it after a condition is picked. In play, 2026-10-24, on
// Tug's tactics screen, each sent as its movie sends it: the screen sent RefreshTacticsData 1 as
// it opened and after each load; DirtyPreset set Dirty to 1; SetBehaviour 1 set his property 18
// to 1, and 0 back; SavePreset 2 made PresetType 1, PresetIndex 2; LoadPreset 0 2 made them 2
// and 0 and Dirty 0 again; ValidateAction arrived with row 0.

// Marks the follower's tactics changed from their preset. Sent to the follower's tactics
// ("GUI.SelectedChar.Tactics.DirtyPreset").
struct DirtyPresetMessage : Message
{
    DAO_VFTABLE(68963);
};
static_assert(sizeof(DirtyPresetMessage) == 4);

// Loads a preset of tactics, or a custom slot, into the follower's rows, or turns them off.
struct LoadPresetMessage : Message
{
    DAO_VFTABLE(68964);
    std::int32_t index; // a place in PresetIdList for type 2, a custom slot for type 1
    std::int32_t type;  // 0 tactics off, 1 a custom slot, 2 a preset
};
static_assert(sizeof(LoadPresetMessage) == 0xC);

// Saves the follower's rows into one of the custom slots.
struct SavePresetMessage : Message
{
    DAO_VFTABLE(68965);
    std::int32_t slot; // the custom slot, from 0
};
static_assert(sizeof(SavePresetMessage) == 8);

// Refreshes the items the follower's Use Item rows name.
struct RefreshTacticsDataMessage : Message
{
    DAO_VFTABLE(68967);
    std::int32_t fromTemplate; // 1, as the movie sends it: a row with no item finds it by template
};
static_assert(sizeof(RefreshTacticsDataMessage) == 8);

// Sets the follower's behaviour, as the tactics screen's behaviour picker does.
struct SetBehaviourMessage : Message
{
    DAO_VFTABLE(68966);
    std::int32_t behaviour; // a row of the aibehaviors table
};
static_assert(sizeof(SetBehaviourMessage) == 8);

// Changes a tactics row's command if its condition does not allow it.
struct ValidateActionMessage : Message
{
    DAO_VFTABLE(68968);
    std::int32_t row; // the row, from 0
};
static_assert(sizeof(ValidateActionMessage) == 8);

// Notes on the move messages: read 2026-09-30. MoveCharacterToLocationMessage's clone allocates
// 4 bytes, the vtable. Its handler (0x0070C9A0) takes the place from explore mode's own state.
// PreMove's handler (0x0070AAF0) only arms a flag the move needs.

// Sent as the right mouse button goes down, before a move. Its handler sets a flag the move
// needs.
struct PreMoveCharacterToLocationMessage : Message
{
    DAO_VFTABLE(68701);
};
static_assert(sizeof(PreMoveCharacterToLocationMessage) == 4);
// The player's click to move the selected creature, sent as the right mouse button comes up.
// It has no fields: the handler moves to the ground under the cursor (see GameModeExplore in
// dao/GameModes.h).
struct MoveCharacterToLocationMessage : Message
{
    DAO_VFTABLE(68702);
};
static_assert(sizeof(MoveCharacterToLocationMessage) == 4);

// Notes on PerformPrimaryActionOnTargetUnderCursorMessage: the handler is 0x007132B0.

// The player's primary click on what is under the cursor: attack, talk, open or pick up.
// It has no fields: the handler acts on the object explore mode has under the cursor
// (GameModeExplore::CursorObject).
struct PerformPrimaryActionOnTargetUnderCursorMessage : Message
{
    DAO_VFTABLE(67740);
};
static_assert(sizeof(PerformPrimaryActionOnTargetUnderCursorMessage) == 4);

// Notes on CenterOnObjectMessage: 8 bytes, from the clone (0x0077D9A0). Explore mode's handler
// (0x0070C140, combat mode's too), read 2026-09-30, does two things. It selects the object
// (dao/Addresses.h, Party::Select), which for a controllable creature (its +0x7F9, the scripts'
// SetControllable) makes it the one selected and signals it EVENT_TYPE_ON_SELECT, and for
// anything else does nothing. Then it has the camera controller snap to its target
// (CameraControllerExplore, slot 8): the creature it follows, which the selection sets.

// Asks explore or combat mode to centre the camera on a party member.
// If the object is a controllable creature, the handler selects it, which sends the creature
// EVENT_TYPE_ON_SELECT. It then snaps the camera, without panning, to the creature the camera
// follows. So for an enemy, the camera snaps back to whoever it was already following.
struct CenterOnObjectMessage : Message
{
    DAO_VFTABLE(68770);
    ObjectId object;
};
static_assert(sizeof(CenterOnObjectMessage) == 8);

// Notes on SaveGameMessage: SaveLoadProxy's handlers read the slot at +4 and the kind at +8. A
// save's message is 0x28 bytes, a name after (an engine std::wstring, which its clone copies).
// Read 2026-09-30, and seen in play the same day: the quicksave key sent slot -1, kind 2, no
// name, and SaveGame::Write then wrote slot 1; a quickload sent slot 1, kind 2, and a load from
// the menu slot 2, kind 0. The name, where not empty, is a path SaveGame::Write copies the
// written .das to (CopyFileW, 0x0064666E; read 2026-10-15, DAOLib #141).

// Asks the save proxy, the engine object that runs saving and loading, to save the game
// (dao/Saves.h).
struct SaveGameMessage : Message
{
    DAO_VFTABLE(64880);
    std::int32_t slot; // the save slot; -1 lets the handler choose, as the quicksave key does
    std::int32_t kind; // 0 manual save, 1 autosave, 2 quicksave; 3 is a fourth, unnamed kind
    // Not the save's name, which is the save screen's SaveLoad.SaveName: a path the written .das is
    // copied to, relative to the game's folder; leave it empty, as the game does. An engine
    // std::wstring (WStringRep, dao/core/Msvc8.h).
    std::byte name[0x1C];
};
static_assert(sizeof(SaveGameMessage) == 0x28);

// Asks the save proxy to load a save (dao/Saves.h).
struct LoadGameMessage : Message
{
    DAO_VFTABLE(65376);
    std::int32_t slot; // the save slot
    std::int32_t kind; // the same values as SaveGameMessage's kind
};
static_assert(sizeof(LoadGameMessage) == 0xC);

// Notes on GUIDialogResultMessage: read 2026-10-01 from the application's handler, 0x005DAA30,
// which closes the popup and goes by the code alone. For the dialog a load raises over content
// the install lacks ("This saved game cannot be loaded. It requires the following content"), 1
// turns the content on (each name in the application's list at +0x26C) and loads. The 0x1C bytes
// after the code are taken to be a std::wstring, an input box's text, by the size and the one
// call the message's clone makes; the handler's paths read above do not use them.

// A dialog box's answer, sent from the popup to the application: the code of the button pressed.
// When loading a save that needs content the install lacks, the game shows a dialog. There, 1
// turns that content on and loads, 2 opens a link, and 3 is Force Load (see ForceLoad in
// dao/Saves.h).
struct GUIDialogResultMessage : Message
{
    DAO_VFTABLE(64890);
    std::int32_t result;  // the code of the button pressed
    std::byte text[0x1C]; // probably an input box's text, an engine std::wstring; unused there
};
static_assert(sizeof(GUIDialogResultMessage) == 0x24);

// Notes on the save proxy's messages: read 2026-09-30 from its handlers.

// Tells the save proxy to rebuild its list of characters from their save folders (dao/Saves.h).
struct RefreshHeroesMessage : Message
{
    DAO_VFTABLE(68744);
};
static_assert(sizeof(RefreshHeroesMessage) == 4);

// Tells the save proxy to choose a character by its position in its list (dao/Saves.h).
struct SelectHeroMessage : Message
{
    DAO_VFTABLE(68746);
    std::int32_t index; // the character's position in the list
};
static_assert(sizeof(SelectHeroMessage) == 8);

// Tells the save proxy which mode the save screens are in (dao/Saves.h).
struct SetSaveLoadModeMessage : Message
{
    DAO_VFTABLE(68741);
    std::int32_t mode; // 0 for none, 2 for the load screen
};
static_assert(sizeof(SetSaveLoadModeMessage) == 8);

// Notes on DialogBoxClosedMessage: the handler is 0x0060FB50.

// Sent by the popup layer's movie to the GUI system once a message box has closed (see Popup in
// dao/Gui.h). The handler removes the popup layer's screen from the open screens and clears the
// system's flag that says a box is up.
struct DialogBoxClosedMessage : Message
{
    DAO_VFTABLE(62667);
};
static_assert(sizeof(DialogBoxClosedMessage) == 4);

// Notes on the conversation's and cutscene's keys: read 2026-10-01 from their handlers.
// SelectResponseMessage's is the dialogue mode's 0x0068D050, SkipDialogMessage's the dialogue
// mode's 0x0068DC10, and SkipCutsceneMessage's the cutscene mode's 0x00691D60.

// Picks a reply in a conversation, as clicking it or pressing its number key does. The dialogue
// mode handles it (dao/Dialogue.h).
struct SelectResponseMessage : Message
{
    DAO_VFTABLE(68723);
    std::int32_t index; // the reply's position in the list on screen
};
static_assert(sizeof(SelectResponseMessage) == 8);

// The skip key in a conversation. The dialogue mode handles it (dao/Dialogue.h).
struct SkipDialogMessage : Message
{
    DAO_VFTABLE(68722);
};
static_assert(sizeof(SkipDialogMessage) == 4);

// The skip key in a cutscene. The cutscene mode handles it (dao/Dialogue.h).
struct SkipCutsceneMessage : Message
{
    DAO_VFTABLE(68795);
};
static_assert(sizeof(SkipCutsceneMessage) == 4);

// Notes on SkipMovieMessage: read 2026-10-01. GameModeMovie's handler (0x006DC130) stops the
// movie (0x006DC040) unless the application's +0x540 says movies are not to be skipped.

// The skip key while a pre-rendered movie plays. The movie mode stops the movie, unless the game
// has movie skipping turned off.
struct SkipMovieMessage : Message
{
    DAO_VFTABLE(68796);
};
static_assert(sizeof(SkipMovieMessage) == 4);

// Notes on TakeScreenshotMessage: KeyBindings.ini's Screenshot binds TakeScreenshotMessage with
// no arguments; the game mode controller handles it (0x00642050). Read 2026-09-30 from its
// constructor (0x00650CE0): a flag and four engine strings, which the key leaves 0 and empty, as
// Make does. The handler names the file for the local time, to the millisecond, and asks the
// renderer for the picture. What the flag and the strings change is not read (whether the third
// string, its length at +0x54, and the fourth, at +0x70, are empty changes the name). The script
// command Screenshot does nothing in this build (the Eluvian server, 2026-09-30). Seen in play,
// 2026-10-01, through that server: one send wrote
// Screenshots\User\Screenshot20261001012341911.jpg, 512 KB, the call taking 381 ms.

// Takes a screenshot, including the interface, as the screenshot key does. Send it to the game
// mode controller:
//
//   auto shot = dao::messages::Make<dao::messages::TakeScreenshotMessage>();
//   dao::messages::Send(dao::CClientExoApp::Get()->GameModes(), shot);
//
// The picture is saved in Documents\BioWare\Dragon Age\Screenshots\User, named after the local
// time. Send blocks for a few hundred milliseconds while the file is written.
struct TakeScreenshotMessage : Message
{
    DAO_VFTABLE(65864);
    // Makes the message as the key sends it, with the flag 0 and the strings empty.
    TakeScreenshotMessage() : Message{}
    {
    }
    std::uint8_t flag = 0; // 0, as the key sends it; what it changes is unknown
    std::uint8_t padding[3]{};
    core::WStringRep strings[4]; // empty, as the key sends them; what they change is unknown
};
static_assert(sizeof(TakeScreenshotMessage) == 0x78);

// Notes on SetGameModeMessage: 8 bytes; the controller's handler (0x006416E0) passes +4 to
// SetGameMode.

// Switches the game's mode (dao/GameModes.h). The game mode controller handles it.
struct SetGameModeMessage : Message
{
    DAO_VFTABLE(68698);
    std::int32_t mode; // the mode to switch to, numbered as in dao/GameModes.h
};
static_assert(sizeof(SetGameModeMessage) == 8);

// Notes on the pause messages: no fields, 4 bytes each. The controller's handler for the pause
// key toggles the application's pause (dao/Addresses.h).

// The pause key: the game mode controller pauses or unpauses the game.
struct UserTogglePauseMessage : Message
{
    DAO_VFTABLE(68779);
};
// Pauses the game.
struct PauseMessage : Message
{
    DAO_VFTABLE(68777);
};
// Unpauses the game.
struct UnpauseMessage : Message
{
    DAO_VFTABLE(68778);
};

// Notes on the zoom messages: no fields, 4 bytes each; the key bindings ZoomIn and ZoomOut, the
// wheel's notches among them, send them to explore mode (seen 2026-10-15, DAOLib #127).

// Zooms the explore camera in a step.
struct CameraZoomInMessage : Message
{
    DAO_VFTABLE(68784);
};
// Zooms the explore camera out a step.
struct CameraZoomOutMessage : Message
{
    DAO_VFTABLE(68785);
};

} // namespace dao::messages
