# The message handler table

Eclipse (the engine) routes input and game events as messages: `UserTogglePauseMessage`, `SetGameModeMessage`, `PerformAbilityMessage`, `RawInputMessage`, and so on. A class that handles one registers a handler object built from a template whose RTTI name spells out the whole binding:

```
Messages::Private::MemberFunctionMessageHandler
    <GameModeController, UserTogglePauseMessage, &GameModeController::HandleMessage>
```

There are 314 of these in the exe, over 50 owner classes (2026-09-29). So the RTTI alone lists every message each class answers. `tools/message_handlers.py` also finds each handler's method. The template's invoke slot is shared code that calls a member function pointer stored in the handler at `+8`, with a `this` adjustment at `+0xC`. The method's address is therefore where the handler is built, not in any vtable:

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
