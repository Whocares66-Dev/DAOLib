# The message handler table

Eclipse (the engine) routes input and game events as messages: `UserTogglePauseMessage`, `SetGameModeMessage`, `PerformAbilityMessage`, `RawInputMessage`, and so on. A class that handles one registers a handler object built from a template whose RTTI name spells out the whole binding:

```
Messages::Private::MemberFunctionMessageHandler
    <GameModeController, UserTogglePauseMessage, &GameModeController::HandleMessage>
```

There are 314 of these in the exe, over 50 owner classes (2026-09-29). So the RTTI alone lists every message each class answers. `tools/message_handlers.py` also finds each handler's method. The template's invoke slot is shared code that calls a member function pointer stored in the handler at `+8`, with a `this` adjustment at `+0xC` (0 in every handler). That is the default invoker (0x0060C480). Three others first subtract the owner's `Messages::MessageReceiver` base from the receiver the dispatcher passes: 0x006A4CF0 (−0x18: CCParty, CCCreature, CCPlaceable, CCItem, Store; 38 vftables), 0x0067BC60 (−0xD4: CCArea), 0x004785A0 (−8: OnlineFeatures); a null receiver is passed over. The 13 `GUI::ScaleformManager` handlers use 0x00511350, which calls the function at +4 with no adjustment. So a handler's `this` is always the owner's own address (read 2026-11-01, lib-agent-2). The method's address is therefore where the handler is built, not in any vtable:

```
push  0x10 ; call operator new           ; 0x006407B0, in a GameModeController method
mov   ecx, 0x006403C0                    ; the method
xor   edx, edx                           ; the this-adjustment
mov   dword ptr [eax], 0x00B06648        ; the handler's vftable
mov   dword ptr [eax + 8], ecx
mov   dword ptr [eax + 0xc], edx
push  eax
push  0x00C00B50                         ; the message's type descriptor: its type id
call  0x0063FF40
push  eax
mov   ecx, esi
call  0x004D9190                         ; register (handler, message type) on the owner
```

The script follows that shape for each handler vftable and writes `dev/message_handlers.csv`: `owner,message,method,function_va,this_adjust,vftable_va,built_at_va`. All 314 resolve (DAOLib #188). Twelve are built in a function that follows another with no int3 padding between, so the script reads on past a `ret` from where the padding puts the start. `GUI::ScaleformManager`'s thirteen (its cursor handlers, PlayMovieMessage and StopMovieMessage) are a smaller form, eight bytes with the method at +4 and no adjustment, which their invoke slot 0x00511350 calls on the owner. Each method is named `Owner::Method_Message` in the address library's table (`tools/names.py --give`), and so in Ghidra.

Why it matters:

- **It is the engine's event API.** Hooking a handler is how a plugin sees or intercepts a player action (`GameModeExplore::HandleMessage_PerformAbilityMessage`), a mode change (`GameModeController::HandleMessage_SetGameModeMessage`), or input.
- **It maps the game modes.** `GameModeExplore`, `GameModeDialog`, `GameModeCutscene`, `GameModeDead`, `GameModeCharGen`, ... each with the messages it takes.
- **Sending a message is how a plugin acts as the engine does.** Read 2026-09-30: the handlers are kept by a registry (a singleton, `0x004740B0`), in a table by the receiver's class id and the message's type id, each the object's own vtable slot 0. `Messages::Send(message, receiver)` (`0x004D20C0`, 74 callers) looks the handler up and runs it at once, if the receiver's +4 byte says it takes messages, and returns its answer. `dao::messages::Send` calls it (`dao/Messages.h`); not yet seen in play.

## A message class's type info

Read 2026-11-02 over all 295 classes with an `InitType`, by a script decoding each class's `GetType` and `InitType`; the id's assignment read by lib-agent.

- Each class has a static `s_classInfo` of 8 bytes, {type id, parent}. The id starts as 0xFFFFFFFF in the exe. `GetType` returns its address (`mov eax, S`), and `InitType` writes the parent's address to S+4, behind a one-time guard. All 295 store the same parent in both.
- The roots are `Messages::Message::s_classInfo` 0x00BF0C2C and `Messages::GameMessage::s_classInfo` 0x00BF0C34, whose parent is Message's. 178 classes derive from GameMessage and share one guard for its parent, 0x00C1D264, so the first of them to run sets it.
- `MessageRegistry::AssignClassId` 0x004D7F20 gives a class its id when it is still -1: the registry's counter at +0x10, then incremented. So ids are handed out in the order classes first reach it, and aren't fixed numbers [C].
- `Register` 0x004D9190 indexes an outer vector with a 16-byte row per receiver class, whose inner vector holds a 4-byte handler id per message type. An index out of range gets a shared cell: 0x00C2C0A4 for a row, 0x00C2C0A0 for a slot.
- `Messages::Message::s_instanceCount` 0x00C2BF4C counts live messages (the constructors and destructor, 0x004D2078, 0x004D2088, 0x004D20A6). Nothing reads it.
