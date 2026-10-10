// Tells a plugin when any creature or object starts or ends a command from its queue, and how
// the command ended. Use it instead of polling a creature's queue, and instead of
// EVENT_TYPE_COMMAND_COMPLETE, which does not fire for every command that ends.
//
//   dao::commands::OnEnd([](dao::ObjectId who, const dao::Command &command, std::int32_t result) {
//       if (result != dao::commands::kSuccess) spdlog::info("{} failed: {}", who, dao::commands::ResultName(result));
//   });
#pragma once

#include "dao/ObjectId.h"
#include "dao/Objects.h"

#include <cstdint>
#include <functional>
#include <string_view>

namespace dao::commands
{

// Results a command can end with, under their names in script.ldf. The other failures are the
// COMMAND_FAILED_* values of script::CommandFailed.
inline constexpr std::int32_t kSuccess = 1;  // COMMAND_SUCCESSFUL
inline constexpr std::int32_t kLooping = 2;  // COMMAND_LOOPING: a looping animation ended
inline constexpr std::int32_t kFailed = -1;  // COMMAND_FAILED
inline constexpr std::int32_t kCleared = -2; // COMMAND_FAILED_COMMAND_CLEARED: cleared from the queue

// Returns a name for a result, for logging: "success", "looping", "COMMAND_FAILED", or the
// script name of a failure, such as "COMMAND_FAILED_TIMEOUT".
// @return an empty view for a result that has no constant
//
// The name only translates the number; it does not tell you whether the command worked. A
// use-object command that opens its door still ends with COMMAND_FAILED, for example.
std::string_view ResultName(std::int32_t result);

// Calls `observer` each time a creature or object starts a command, before the command runs.
// @param observer receives the object and the command it is starting
//
// The observer runs on the game thread while the object's queue is being processed, so keep it
// short, and don't add or clear commands from it. A command can start and never end; see OnEnd.
void OnStart(std::function<void(ObjectId object, const Command &command)> observer);

// Calls `observer` each time a creature or object ends a command, before the command is freed.
// @param observer receives the object, the command and its result: kSuccess, kLooping, kFailed,
// kCleared or a script::CommandFailed value
//
// The observer runs on the game thread while the object's queue is being processed, so keep it
// short, and don't add or clear commands from it. The ExecuteEffect command that a dying creature
// starts never ends, so don't wait for it.
void OnEnd(std::function<void(ObjectId object, const Command &command, std::int32_t result)> observer);

} // namespace dao::commands
