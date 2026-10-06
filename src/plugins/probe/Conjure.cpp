#include "Conjure.h"

#include "dao/Addresses.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/core/CommandWord.h"
#include "dao/core/ObjectType.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace probe
{

namespace
{

// A sub-action's words as RunCurrentCommand reads them (dao/Addresses.h):
// its type at +0, the elapsed time at +0xC, the duration at +0x14 and the
// delay at +0x18, in milliseconds; the rest logged too, unknown.
constexpr std::size_t kSubActionWords = 8;
constexpr std::size_t kType = 0;
constexpr std::size_t kElapsed = 3;
constexpr std::size_t kDuration = 5;
constexpr std::size_t kDelay = 6;
// Log the elapsed time again once it has moved this much.
constexpr std::int32_t kElapsedStep = 250;

struct Seen
{
    std::uintptr_t subAction = 0;
    std::uint32_t type = 0xFFFFFFFF;
    std::int32_t duration = -1;
    std::int32_t delay = -1;
    std::int32_t elapsed = -kElapsedStep;
    std::int32_t conjuring = -1;
    std::int32_t result = -100;
    float mana = -1;
    std::uint16_t command = 0;
};

std::mutex g_mutex;
std::unordered_map<dao::ObjectId, Seen> g_seen;

// CSSubActionHandler::Execute: __thiscall (object, sub-action), `ret 8`.
using ExecuteFn = std::int32_t(__fastcall *)(void *handler, int edx, dao::CCObject *object, std::uintptr_t subAction);
dao::Detour<ExecuteFn> g_execute;

std::int32_t IsConjuring(const dao::CCObject *creature)
{
    // CCCreature::ForScript_IsConjuring: __thiscall, no arguments.
    using Fn = std::int32_t(__fastcall *)(const dao::CCObject *, int);
    return reinterpret_cast<Fn>(dao::Module::Address(dao::addr::kCCCreature_IsConjuring))(creature, 0);
}

std::int32_t __fastcall Execute(void *handler, int edx, dao::CCObject *object, std::uintptr_t subAction)
{
    const dao::Command *command =
        object != nullptr && object->Type() == dao::core::ObjectType::Creature ? object->CurrentCommand() : nullptr;
    const bool watched = command != nullptr && command->Type() == dao::core::CommandType::UseAbility;
    std::array<std::uint32_t, kSubActionWords> words{};
    const bool read = watched && subAction != 0 && dao::TryCopy(words.data(), subAction, sizeof words);
    const std::int32_t result = g_execute.Original()(handler, edx, object, subAction);
    if (!read)
    {
        return result;
    }

    const auto *creature = static_cast<const dao::CCCreature *>(object);
    const std::int32_t conjuring = IsConjuring(object);
    const float mana = creature->ManaStamina().value_or(-1);
    const auto elapsed = static_cast<std::int32_t>(words[kElapsed]);
    const auto duration = static_cast<std::int32_t>(words[kDuration]);
    const auto delay = static_cast<std::int32_t>(words[kDelay]);
    const dao::ObjectId id = object->Id();
    {
        const std::lock_guard lock(g_mutex);
        Seen &seen = g_seen[id];
        const bool changed = seen.subAction != subAction || seen.type != words[kType] || seen.duration != duration ||
                             seen.delay != delay || seen.conjuring != conjuring || seen.result != result ||
                             std::fabs(seen.mana - mana) >= 0.5F || seen.command != command->Sequence() ||
                             elapsed - seen.elapsed >= kElapsedStep || elapsed < seen.elapsed;
        if (!changed)
        {
            return result;
        }
        seen = Seen{subAction, words[kType], duration, delay, elapsed, conjuring, result, mana, command->Sequence()};
    }
    spdlog::debug("conjure: {:#010x} command {} ability {} conjure time {:.2f}; sub-action {:#010x} type {} elapsed "
                  "{} duration {} delay {} words [{:#x} {:#x} {:#x} {:#x} {:#x} {:#x} {:#x} {:#x}]; IsConjuring {}, "
                  "mana {:.1f}; run answered {}",
                  id, command->Sequence(), command->Ability().value_or(-1),
                  command->Parameters() != nullptr ? command->Parameters()->Float(0) : -1.0F, subAction, words[kType],
                  elapsed, duration, delay, words[0], words[1], words[2], words[3], words[4], words[5], words[6],
                  words[7], conjuring, mana, result);
    return result;
}

} // namespace

void AttachConjureWatch()
{
    if (g_execute.Attach(dao::addr::kCSSubActionHandler_Execute, &Execute))
    {
        spdlog::info("conjure watch: detoured CSSubActionHandler::Execute at {:#010x}",
                     dao::Module::Preferred(dao::addr::kCSSubActionHandler_Execute));
    }
    else
    {
        spdlog::error("conjure watch: could not detour CSSubActionHandler::Execute at {:#010x}",
                      dao::Module::Preferred(dao::addr::kCSSubActionHandler_Execute));
    }
}

} // namespace probe
