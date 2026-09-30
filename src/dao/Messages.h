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
// A message class derives from Messages::GameMessage and Messages::Message
// (RTTI), polymorphic, with 11 virtual functions; slot 1 clones it, and the
// clone copies exactly its fields. The layouts below are read from the
// clone and the handler that uses them; the rest of the ~300 are not yet.
#pragma once

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

// The player orders a creature to use an ability, on a target (the
// quickbar, a radial menu). 0x10 bytes, from its clone (0x0077B210); the
// fields from GameModeExplore's handler (0x00711B50), which looks the
// ability up in the ability table by +4, the caster by +8 with
// GetCreatureById, and the target by +0xC with GetObjectById. Read
// 2026-09-30, not yet seen in play.
struct PerformAbilityMessage : Message
{
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
};
static_assert(sizeof(MoveCharacterToLocationMessage) == 4);

} // namespace dao::messages
