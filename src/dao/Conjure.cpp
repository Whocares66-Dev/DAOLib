#include "dao/Conjure.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Module.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/core/ObjectType.h"

#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace dao::commands
{

namespace
{

// A sub-action's type at +0, its elapsed time at +0xC and its duration at
// +0x14, in milliseconds (dao/Addresses.h, RunCurrentCommand); the conjure
// is type 7 (dev/TURN_BASED.md).
constexpr std::uint32_t kConjure = 7;
constexpr std::uintptr_t kElapsed = 0xC;
constexpr std::uintptr_t kDuration = 0x14;

struct Held
{
    double fed = 0;               // milliseconds given and not yet spent
    double elapsed = 0;           // the held conjure's, in milliseconds
    std::uintptr_t subAction = 0; // the conjure it is holding
};

struct Seen
{
    std::int32_t elapsed = 0;
    std::int32_t duration = 0;
};

std::mutex g_lock;
std::unordered_map<ObjectId, Held> g_held;
std::unordered_map<ObjectId, Seen> g_seen;

// CSSubActionHandler::Execute: __thiscall (object, sub-action), `ret 8`.
using ExecuteFn = std::int32_t(__fastcall *)(void *handler, int edx, CCObject *object, std::uintptr_t subAction);
Detour<ExecuteFn> g_execute;

std::int32_t __fastcall Execute(void *handler, int edx, CCObject *object, std::uintptr_t subAction)
{
    if (object != nullptr && subAction != 0 && object->Type() == core::ObjectType::Creature &&
        TryRead<std::uint32_t>(subAction) == kConjure)
    {
        const auto elapsed = TryRead<std::int32_t>(subAction + kElapsed);
        const auto duration = TryRead<std::int32_t>(subAction + kDuration);
        if (elapsed && duration && *duration > 0)
        {
            const ObjectId id = object->Id();
            const std::lock_guard lock(g_lock);
            std::int32_t now = *elapsed;
            if (const auto held = g_held.find(id); held != g_held.end())
            {
                Held &hold = held->second;
                if (hold.subAction != subAction)
                {
                    // A conjure first seen held: from where it stands, which
                    // is at most this update's real time on.
                    hold.subAction = subAction;
                    hold.elapsed = *elapsed;
                }
                hold.elapsed = std::fmin(hold.elapsed + hold.fed, *duration);
                hold.fed = 0;
                now = static_cast<std::int32_t>(hold.elapsed);
                *reinterpret_cast<std::int32_t *>(subAction + kElapsed) = now;
            }
            g_seen[id] = Seen{now, *duration};
        }
    }
    return g_execute.Original()(handler, edx, object, subAction);
}

void Attach()
{
    static std::once_flag once;
    std::call_once(once, [] { g_execute.Attach(addr::kCSSubActionHandler_Execute, &Execute); });
}

} // namespace

void HoldConjure(ObjectId caster, bool hold)
{
    Attach();
    const std::lock_guard lock(g_lock);
    if (hold)
    {
        g_held.try_emplace(caster);
    }
    else
    {
        g_held.erase(caster);
    }
}

bool ConjureHeld(ObjectId caster)
{
    const std::lock_guard lock(g_lock);
    return g_held.contains(caster);
}

void FeedConjure(ObjectId caster, float seconds)
{
    if (seconds <= 0)
    {
        return;
    }
    const std::lock_guard lock(g_lock);
    if (const auto held = g_held.find(caster); held != g_held.end())
    {
        held->second.fed += static_cast<double>(seconds) * 1000;
    }
}

std::optional<Conjuring> Conjure(ObjectId caster)
{
    Attach();
    CClientExoApp *app = CClientExoApp::Get();
    const CCObject *object = app != nullptr ? app->GetObjectById(caster) : nullptr;
    if (object == nullptr || object->Type() != core::ObjectType::Creature)
    {
        return std::nullopt;
    }
    // CCCreature::ForScript_IsConjuring: __thiscall, no arguments; the
    // running sub-action's type (dev/TURN_BASED.md).
    using IsConjuringFn = std::uint32_t(__fastcall *)(const CCObject *, int);
    if (reinterpret_cast<IsConjuringFn>(Module::Address(addr::kCCCreature_IsConjuring))(object, 0) != kConjure)
    {
        return std::nullopt;
    }
    const std::lock_guard lock(g_lock);
    const auto seen = g_seen.find(caster);
    if (seen == g_seen.end())
    {
        return std::nullopt;
    }
    return Conjuring{static_cast<float>(seen->second.elapsed) / 1000, static_cast<float>(seen->second.duration) / 1000};
}

} // namespace dao::commands
