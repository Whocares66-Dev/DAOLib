// The engine's messages: what its classes send each other (a key pressed,
// an ability chosen, the game paused), and the handlers that answer them
// (dev/MESSAGE_HANDLERS.md). A handler is `bool Owner::HandleMessage(const
// Message &)`, __thiscall, `ret 4`; dao/generated/MessageHandlers.h names
// every one as an address to detour:
//
//   using Fn = dao::messages::TypedHandlerFn<dao::messages::PerformAbilityMessage>;
//   dao::Detour<Fn> g_performAbility;
//   bool __fastcall PerformAbility(void *explore, int edx, const dao::messages::PerformAbilityMessage *m)
//   {
//       if (!MyTurn(m->caster))
//           return true; // handled: the order is dropped
//       return g_performAbility.Original()(explore, edx, m);
//   }
//   g_performAbility.Attach(dao::messages::handler::GameModeExplore_PerformAbilityMessage, &PerformAbility);
//
// And a plugin sends one as the engine does, to the object that handles it
// (a game mode, the mode controller):
//
//   auto *modes = dao::CClientExoApp::Get()->GameModes();
//   auto message = dao::messages::Make<dao::messages::CenterOnObjectMessage>();
//   message.object = creature;
//   const bool handled = dao::messages::Send(modes->CurrentModeObject(), message);
//
// A message class derives from Messages::GameMessage and Messages::Message
// (RTTI), polymorphic, with 11 virtual functions; slot 0 answers its type
// id, slot 1 clones it, and the clone copies exactly its fields. The layouts below are read from the
// clone and the handler that uses them; every class's size and field
// offsets are in dev/messages.csv (tools/messages.py), their meanings not
// yet.
#pragma once

#include "dao/Addresses.h"
#include "dao/Module.h"
#include "dao/ObjectId.h"
#include "dao/generated/MessageHandlers.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace dao::messages
{

// Messages::Message: the vtable; a message's own fields follow.
struct Message
{
    const void *vtable;
};

template <class M> using TypedHandlerFn = bool(__fastcall *)(void *owner, int edx, const M *message);

// The message as class M if it is one (its vtable is M's), else null. Each
// typed message below has its class's vtable address as kVftable.
template <class M> const M *As(const Message *message)
{
    return message != nullptr && message->vtable == reinterpret_cast<const void *>(Module::Address(M::kVftable))
               ? static_cast<const M *>(message)
               : nullptr;
}

// A message of class M, ready to send: its vtable set, its fields zero. The
// classes below with a kVftable have only plain fields, so one made here
// needs nothing freed.
template <class M> M Make()
{
    M message{};
    message.vtable = reinterpret_cast<const void *>(Module::Address(M::kVftable));
    return message;
}

// Send a message to the object that handles it, as the engine sends its
// own (dao/Addresses.h, Messages::Send): the receiver's handler for the
// message's class runs now, on this thread, and its answer comes back:
// true if it handled it, false if the receiver has no handler for it or is
// not taking messages. The message stays the caller's. Send from the
// scripts' thread (a frame callback, a command intercept), where the
// engine's own handlers run.
inline bool Send(void *receiver, const Message &message)
{
    using Fn = bool(__cdecl *)(const Message *, void *);
    return reinterpret_cast<Fn>(Module::Address(addr::kMessages_Send))(&message, receiver);
}

// Every message the engine delivers, as it delivers it: a plugin's
// observer sees the receiver and the message just before the receiver's
// handler; an interceptor may take the message instead (return true), and
// then no handler runs and the sender hears it was handled. The engine
// offers one message to several receivers, and only one has a handler for
// it: observers and interceptors see only that delivery, once per message.
//
//   dao::messages::OnMessage<dao::messages::SetGameModeMessage>(
//       [](void *, const dao::messages::SetGameModeMessage &m) { spdlog::info("mode {}", m.mode); });
//   dao::messages::InterceptMessage<dao::messages::PerformAbilityMessage>(
//       [](void *, const dao::messages::PerformAbilityMessage &m) { return !MyTurn(m.caster); });
//
// Read 2026-09-30: every delivery, sent or posted, goes through the
// registry's Dispatch (dao/Addresses.h), which only Send calls; posted
// messages are queued and sent later. This detours Dispatch. It runs for
// every message (the cursor's moves too), so keep observers quick; a
// class without a struct here is subscribed to by its vftable
// (dev/messages.csv), its name by dao::TypeName. Delivery is on the
// engine's thread that runs the game (the scripts'); not yet seen in play.
using MessageObserver = std::function<void(void *receiver, const Message &message)>;
using MessageInterceptor = std::function<bool(void *receiver, const Message &message)>;

void OnAnyMessage(MessageObserver observer);
void OnMessage(std::uintptr_t vftable, MessageObserver observer);
void InterceptMessage(std::uintptr_t vftable, MessageInterceptor interceptor);

template <class M> void OnMessage(std::function<void(void *receiver, const M &message)> observer)
{
    OnMessage(M::kVftable, [observer = std::move(observer)](void *receiver, const Message &message) {
        observer(receiver, static_cast<const M &>(message));
    });
}
template <class M> void InterceptMessage(std::function<bool(void *receiver, const M &message)> interceptor)
{
    InterceptMessage(M::kVftable, [interceptor = std::move(interceptor)](void *receiver, const Message &message) {
        return interceptor(receiver, static_cast<const M &>(message));
    });
}

// All message classes, their sizes and fields, are in dev/messages.csv
// (tools/messages.py, from each class's clone).

// The player orders a creature to use an ability, on a target (the
// quickbar, a radial menu). 0x10 bytes, from its clone (0x0077B210); the
// fields from GameModeExplore's handler (0x00711B50), which looks the
// ability up in the ability table by +4, the caster by +8 with
// GetCreatureById, and the target by +0xC with GetObjectById. Read
// 2026-09-30, not yet seen in play.
struct PerformAbilityMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B1DEE4;
    std::int32_t ability;
    ObjectId caster;
    ObjectId target;
};
static_assert(sizeof(PerformAbilityMessage) == 0x10);

// The player clicks to move the selected creature. No fields (its clone
// allocates 4 bytes, the vtable): the handler (0x0070C9A0) takes the place
// from explore mode's own state, the ground under the cursor
// (dao/GameModes.h, GameModeExplore). The right button going down sends
// PreMove, whose handler (0x0070AAF0) only arms a flag the move needs;
// going up sends the move. Read 2026-09-30.
struct PreMoveCharacterToLocationMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B1DA64;
};
static_assert(sizeof(PreMoveCharacterToLocationMessage) == 4);
struct MoveCharacterToLocationMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B1DA94;
};
static_assert(sizeof(MoveCharacterToLocationMessage) == 4);

// The player's primary click on what is under the cursor: attack, talk,
// open, pick up. No fields; the handler (0x007132B0) acts on explore
// mode's object under the cursor (GameModeExplore::CursorObject).
struct PerformPrimaryActionOnTargetUnderCursorMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B16190;
};
static_assert(sizeof(PerformPrimaryActionOnTargetUnderCursorMessage) == 4);

// Centre the camera on an object: explore mode's handler (0x0070C140)
// looks the object up by +4 and has the camera frame it. 8 bytes, from the
// clone (0x0077D9A0). Read 2026-09-30.
struct CenterOnObjectMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B1E754;
    ObjectId object;
};
static_assert(sizeof(CenterOnObjectMessage) == 8);

// Asking for a save or a load: SaveLoadProxy's handlers read the slot at
// +4 and the kind at +8 (0 a manual save, 1 an autosave, 2 the quicksave,
// 3 a fourth; a slot of -1 lets the handler choose, as the quicksave's key
// binding "SaveGameMessage -1 2" asks). A save's message is 0x28 bytes, a
// name after (an engine std::wstring, which its clone copies). Read
// 2026-09-30, and seen in play the same day: the quicksave key sent slot
// -1, kind 2, no name, and SaveGame::Write then wrote slot 1; a quickload
// sent slot 1, kind 2, and a load from the menu slot 2, kind 0.
struct SaveGameMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B00098;
    std::int32_t slot;
    std::int32_t kind;
    std::byte name[0x1C]; // an engine std::wstring (dao/core/Msvc8.h, WStringRep)
};
static_assert(sizeof(SaveGameMessage) == 0x28);

struct LoadGameMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B03A14;
    std::int32_t slot;
    std::int32_t kind;
};
static_assert(sizeof(LoadGameMessage) == 0xC);

// Switch the game's mode (dao/GameModes.h). 8 bytes; the controller's
// handler (0x006416E0) passes +4 to SetGameMode.
struct SetGameModeMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B1D9D4;
    std::int32_t mode;
};
static_assert(sizeof(SetGameModeMessage) == 8);

// The pause key (the controller's handler toggles the application's pause,
// dao/Addresses.h), and pause and unpause as such. No fields, 4 bytes each.
struct UserTogglePauseMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B1E904;
};
struct PauseMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B1E8A4;
};
struct UnpauseMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B1E8D4;
};

} // namespace dao::messages
