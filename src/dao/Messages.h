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

// Notes on PerformAbilityMessage: 0x10 bytes, from its clone (0x0077B210). The quickbar leaves
// the target 0 (seen by the turn-based mod in play, 2026-09-30). The handler (GameModeExplore's,
// 0x00711B50, which combat mode inherits), read 2026-09-30, does not take the target from the
// message. Once the ability may be used (0x0071CE90), it reads the ability's row of abi_base
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
// The handler ignores `target`. It uses the ability on the caster, the selected target or the
// caster's attack target, whichever the ability allows, and otherwise asks the player to pick a
// target.
struct PerformAbilityMessage : Message
{
    DAO_VFTABLE(68725);
    std::int32_t ability; // the ability's ID: its row in the abi_base data table
    ObjectId caster;
    ObjectId target; // 0 from the quickbar; the handler ignores it
};
static_assert(sizeof(PerformAbilityMessage) == 0x10);

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
// the menu slot 2, kind 0.

// Asks the save proxy, the engine object that runs saving and loading, to save the game
// (dao/Saves.h).
struct SaveGameMessage : Message
{
    DAO_VFTABLE(64880);
    std::int32_t slot;    // the save slot; -1 lets the handler choose, as the quicksave key does
    std::int32_t kind;    // 0 manual save, 1 autosave, 2 quicksave; 3 is a fourth, unnamed kind
    std::byte name[0x1C]; // the save's name, an engine std::wstring (WStringRep, dao/core/Msvc8.h)
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
