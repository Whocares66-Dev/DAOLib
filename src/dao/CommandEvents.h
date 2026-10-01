// When any creature or object starts a command from its queue, and when it
// ends one, and how: what a plugin otherwise polls for (busy two frames
// running means done), and what EVENT_TYPE_COMMAND_COMPLETE reaches a
// script with only sometimes (dev/LIBRARY.md, the fifth session).
//
//   dao::commands::OnEnd([](dao::ObjectId who, const dao::Command &command, std::int32_t result) {
//       if (result != dao::commands::kSuccess) spdlog::info("{} failed: {}", who, dao::commands::ResultName(result));
//   });
//
// Read 2026-09-30. An object's command step (0x0073D320, each frame) ends
// the command at the head of its queue when it is done (ClearCurrentCommand,
// 0x0073C730, with 1) or failed (with the failure), and starts the next
// (0x0073C7D0); ClearAllCommands and the scripts' ClearCurrentCommand end
// it through ClearCurrentCommand too, with -2. Each tells the commands
// system (the application's +0x1AC): a start through one thunk (0x0064A500,
// __thiscall (object, command), to the system's slot 2), an end through
// another (0x0064A510, __thiscall (object, command, result, flag), to its
// slot 3), and each thunk has that one caller. The hooks are on the thunks:
// a start is seen before the command runs, an end before the command is let
// go, so the Command is whole in both.
//
// Seen in play, 2026-09-30 (the Eluvian server's session of 66 minutes and
// eight fights, 1,699 ends): 1,286 successes, 315 cleared, 29
// COMMAND_FAILED_INVALID_PATH, 5 COMMAND_FAILED_TIMEOUT, 3
// COMMAND_FAILED_PATH_ACTION_REQUIRED, and 61 with result 2, every one a
// PlayAnimation, which no constant names. A start has its end for every
// type but ExecuteEffect: the one a dying creature starts, the frame it
// dies, never ends (40 of 136 starts), so a plugin pairing the two must
// not wait on those. EVENT_TYPE_COMMAND_COMPLETE came 2,094 times in the
// session, so the event and the end are not one to one.
//
// Callbacks run on the game's thread, inside the command step: keep them
// short, and queue or clear no commands from one.
#pragma once

#include "dao/ObjectId.h"
#include "dao/Objects.h"

#include <cstdint>
#include <functional>
#include <string_view>

namespace dao::commands
{

// The results an end carries: success, or a COMMAND_FAILED_* (script.ldf),
// -2 for a command cleared.
inline constexpr std::int32_t kSuccess = 1;
inline constexpr std::int32_t kCleared = -2;

// "success", or the failure's script name (COMMAND_FAILED_TIMEOUT); empty
// for a result no constant names (2, which PlayAnimation ends with).
std::string_view ResultName(std::int32_t result);

void OnStart(std::function<void(ObjectId object, const Command &command)> observer);
void OnEnd(std::function<void(ObjectId object, const Command &command, std::int32_t result)> observer);

} // namespace dao::commands
