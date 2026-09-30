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

#include <cstdint>

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
// from explore mode's own state. Read 2026-09-30.
struct MoveCharacterToLocationMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B1DA94;
};
static_assert(sizeof(MoveCharacterToLocationMessage) == 4);

// Centre the camera on an object: explore mode's handler (0x0070C140)
// looks the object up by +4 and has the camera frame it. 8 bytes, from the
// clone (0x0077D9A0). Read 2026-09-30.
struct CenterOnObjectMessage : Message
{
    static constexpr std::uintptr_t kVftable = 0x00B1E754;
    ObjectId object;
};
static_assert(sizeof(CenterOnObjectMessage) == 8);

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
