#include "Hold.h"

#include "dao/ClientExoApp.h"
#include "dao/Effects.h"
#include "dao/Frame.h"
#include "dao/ObjectId.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/Structures.h"
#include "dao/SubActions.h"
#include "dao/core/ObjectType.h"
#include "dao/generated/ScriptConstants.h"

#include <spdlog/spdlog.h>

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace probe
{

namespace
{

// The engine's pause for one object, read 2026-10-17 in GOG's exe (DAOLib
// #159). Slot 19 of an object's interface at +0x70 (CCObject's 0x00695380,
// __thiscall (bool), `ret 4`) takes the object's model from the same
// interface's slot 24 and calls 0x00A5D820 on it, which sets the graphics
// object's animation rate (a double at +0x2E8, the frame's time multiplied
// by it in Animation::AnimationTask, 0x008154D0) to 0 when paused and 1 when
// not, and keeps the rate as a float at the model's +0xEC. CCCreature's own
// slot 19 (0x0069F590) does that and then pauses (0x006C9E60) or resumes
// (0x006CAB00) its controller at +0x768. The graphics object is what the
// model's +0x18, then +4, answers from its slot 19. Called through the
// vtable, so no address is needed.
constexpr std::size_t kInterface = 0x70;
constexpr std::size_t kPauseSlot = 19;
constexpr std::size_t kModelSlot = 24;
constexpr std::size_t kModelRate = 0xEC;
constexpr std::size_t kModelGraphics = 0x18;
constexpr std::size_t kGraphicsOwner = 4;
constexpr std::size_t kGraphicsSlot = 19;
constexpr std::size_t kGraphicsRate = 0x2E8;

constexpr ULONGLONG kLogEveryMs = 1000;
constexpr ULONGLONG kWatchAfterMs = 5000;

struct Settings
{
    ULONGLONG holdMs = 0;
    ULONGLONG afterMs = 3000;
    dao::ObjectId victim = dao::kInvalidObject;
};

std::optional<Settings> SettingsFromEnvironment()
{
    std::vector<char> value(64);
    if (GetEnvironmentVariableA("DAOLIB_PROBE_HOLD", value.data(), static_cast<DWORD>(value.size())) == 0)
    {
        return std::nullopt;
    }
    Settings settings;
    settings.holdMs = static_cast<ULONGLONG>(std::strtod(value.data(), nullptr) * 1000.0);
    if (GetEnvironmentVariableA("DAOLIB_PROBE_HOLD_AFTER_MS", value.data(), static_cast<DWORD>(value.size())) != 0)
    {
        settings.afterMs = std::strtoull(value.data(), nullptr, 10);
    }
    if (GetEnvironmentVariableA("DAOLIB_PROBE_HOLD_VICTIM", value.data(), static_cast<DWORD>(value.size())) != 0)
    {
        settings.victim = static_cast<dao::ObjectId>(std::strtoul(value.data(), nullptr, 0));
    }
    return settings;
}

enum class Phase
{
    kWaiting, // for a grabbing effect not held before
    kArmed,   // one seen, its time to the hold running
    kHeld,
    kAfter,
};

std::mutex g_mutex;
Settings g_settings;
struct Event
{
    dao::ObjectId creature = dao::kInvalidObject;
    std::uint32_t type = 0;
};
std::vector<Event> g_events; // the animation events handled since the last frame
Phase g_phase = Phase::kWaiting;
std::uint32_t g_effect = 0; // the grabbing effect armed or held, by its id
std::vector<std::uint32_t> g_heldEffects;
dao::ObjectId g_grabber = dao::kInvalidObject;
ULONGLONG g_armedAt = 0;
ULONGLONG g_heldAt = 0;
ULONGLONG g_releasedAt = 0;
ULONGLONG g_loggedAt = 0;

using SlotFn = void *(__fastcall *)(void *self, int edx);
using PauseFn = void(__fastcall *)(void *self, int edx, bool paused);

void *Interface(dao::CCObject *object)
{
    return reinterpret_cast<char *>(object) + kInterface;
}

void **Vtable(void *self)
{
    return *static_cast<void ***>(self);
}

void Pause(dao::CCObject *object, bool paused)
{
    void *self = Interface(object);
    reinterpret_cast<PauseFn>(Vtable(self)[kPauseSlot])(self, 0, paused);
}

// The model's float and the graphics object's double, as the pause set them.
std::string Rates(dao::CCObject *object)
{
    void *self = Interface(object);
    const auto model = reinterpret_cast<std::uintptr_t>(reinterpret_cast<SlotFn>(Vtable(self)[kModelSlot])(self, 0));
    if (model == 0)
    {
        return "no model";
    }
    const auto modelRate = dao::TryRead<float>(model + kModelRate);
    const auto graphics = dao::TryRead<std::uintptr_t>(model + kModelGraphics);
    const auto owner =
        graphics && *graphics != 0 ? dao::TryRead<std::uintptr_t>(*graphics + kGraphicsOwner) : std::nullopt;
    void *object3d = owner && *owner != 0
                         ? reinterpret_cast<SlotFn>(Vtable(reinterpret_cast<void *>(*owner))[kGraphicsSlot])(
                               reinterpret_cast<void *>(*owner), 0)
                         : nullptr;
    const auto rate = object3d != nullptr
                          ? dao::TryRead<double>(reinterpret_cast<std::uintptr_t>(object3d) + kGraphicsRate)
                          : std::nullopt;
    return fmt::format("model rate {}, graphics rate {}", modelRate ? fmt::format("{}", *modelRate) : "?",
                       rate ? fmt::format("{}", *rate) : "?");
}

std::string Effects(dao::ObjectId id)
{
    std::string text;
    for (const auto &effect : dao::effects::On(id))
    {
        text += fmt::format(" {} {:.2f}/{:.2f}", dao::script::Name(effect.type), effect.remaining, effect.duration);
    }
    return text.empty() ? " none" : text;
}

std::string SubActions(dao::ObjectId id)
{
    std::string text;
    for (const auto &step : dao::commands::SubActions(id))
    {
        text += fmt::format(" [type {} delay {} duration {} elapsed {}]", step.type, step.delayMs, step.durationMs,
                            step.elapsedMs);
    }
    return text.empty() ? " none" : text;
}

void LogState(ULONGLONG now)
{
    auto *app = dao::CClientExoApp::Get();
    auto *grabber = app != nullptr ? app->GetCreatureById(g_grabber) : nullptr;
    const ULONGLONG since = now - g_heldAt;
    if (grabber == nullptr)
    {
        spdlog::info("hold +{} ms: grabber {:#x} is gone", since, g_grabber);
        return;
    }
    spdlog::info("hold +{} ms: grabber {:#x} {}; effects{}; sub-actions{}", since, g_grabber, Rates(grabber),
                 Effects(g_grabber), SubActions(g_grabber));
    if (g_settings.victim != dao::kInvalidObject)
    {
        auto *victim = app->GetCreatureById(g_settings.victim);
        spdlog::info("hold +{} ms: victim {:#x} {}; effects{}; sub-actions{}", since, g_settings.victim,
                     victim != nullptr ? Rates(victim) : "gone", Effects(g_settings.victim),
                     SubActions(g_settings.victim));
    }
}

void LogEvents(ULONGLONG now)
{
    std::vector<Event> events;
    {
        const std::lock_guard lock(g_mutex);
        events.swap(g_events);
    }
    if (g_phase == Phase::kWaiting)
    {
        return;
    }
    const ULONGLONG from = g_phase == Phase::kArmed ? g_armedAt : g_heldAt;
    for (const Event &event : events)
    {
        spdlog::info("hold {}{} ms: animation event {} by {:#x}", g_phase == Phase::kArmed ? "armed +" : "+",
                     now - from, event.type, event.creature);
    }
}

void Hold(ULONGLONG now)
{
    auto *app = dao::CClientExoApp::Get();
    auto *grabber = app != nullptr ? app->GetCreatureById(g_grabber) : nullptr;
    if (grabber == nullptr)
    {
        spdlog::info("hold: grabber {:#x} is gone before its hold", g_grabber);
        g_phase = Phase::kWaiting;
        return;
    }
    auto *victim = g_settings.victim != dao::kInvalidObject ? app->GetCreatureById(g_settings.victim) : nullptr;
    spdlog::info("hold: before the pause, grabber {:#x} {}", g_grabber, Rates(grabber));
    Pause(grabber, true);
    if (victim != nullptr)
    {
        spdlog::info("hold: before the pause, victim {:#x} {}", g_settings.victim, Rates(victim));
        Pause(victim, true);
    }
    g_heldAt = now;
    g_loggedAt = 0;
    g_phase = Phase::kHeld;
    spdlog::info("hold: grabber {:#x}{} paused for {} ms", g_grabber, victim != nullptr ? " and victim" : "",
                 g_settings.holdMs);
}

void Release(ULONGLONG now)
{
    auto *app = dao::CClientExoApp::Get();
    if (auto *grabber = app != nullptr ? app->GetCreatureById(g_grabber) : nullptr)
    {
        Pause(grabber, false);
    }
    if (auto *victim = app != nullptr && g_settings.victim != dao::kInvalidObject
                           ? app->GetCreatureById(g_settings.victim)
                           : nullptr)
    {
        Pause(victim, false);
    }
    g_releasedAt = now;
    g_phase = Phase::kAfter;
    spdlog::info("hold: grabber {:#x} resumed after {} ms", g_grabber, now - g_heldAt);
}

void OnFrame()
{
    const ULONGLONG now = GetTickCount64();
    LogEvents(now);
    if (g_phase == Phase::kWaiting)
    {
        return;
    }
    if (g_phase == Phase::kArmed)
    {
        if (now - g_armedAt >= g_settings.afterMs)
        {
            Hold(now);
        }
        return;
    }
    if (g_phase == Phase::kHeld && now - g_heldAt >= g_settings.holdMs)
    {
        Release(now);
    }
    if (g_loggedAt == 0 || now - g_loggedAt >= kLogEveryMs)
    {
        g_loggedAt = now;
        LogState(now);
    }
    if (g_phase == Phase::kAfter && now - g_releasedAt >= kWatchAfterMs)
    {
        g_phase = Phase::kWaiting;
        spdlog::info("hold: done, waiting for the next grab");
    }
}

// Every temporary effect passes here on every update, on the game thread
// (dao/Effects.h): a grabbing effect not held before arms the hold, its
// creator the grabber (the scripts apply it on the grabber, by the grabber).
std::int32_t OnAgeing(const dao::CGameEffect &effect, std::int32_t ms)
{
    if (g_phase == Phase::kWaiting && effect.Type() == dao::script::EffectType::Grabbing &&
        std::ranges::find(g_heldEffects, effect.Id()) == g_heldEffects.end())
    {
        g_effect = effect.Id();
        g_heldEffects.push_back(g_effect);
        g_grabber = effect.Creator();
        g_armedAt = GetTickCount64();
        g_phase = Phase::kArmed;
        spdlog::info("hold: grabbing effect {} by {:#x} seen, held in {} ms", g_effect, g_grabber, g_settings.afterMs);
    }
    return ms;
}

} // namespace

void AttachHoldWatch()
{
    const auto settings = SettingsFromEnvironment();
    if (!settings)
    {
        return;
    }
    g_settings = *settings;
    spdlog::warn("DAOLIB_PROBE_HOLD is set: a grabber is paused for {} ms, {} ms after its grabbing effect is seen, "
                 "victim {:#x}",
                 g_settings.holdMs, g_settings.afterMs, g_settings.victim);
    dao::OnFrame(&OnFrame);
    dao::effects::OnAgeing(&OnAgeing);
}

void OnAnimationEvent(const void *creature, std::uint32_t type)
{
    if (g_settings.holdMs == 0 || creature == nullptr)
    {
        return;
    }
    const auto *object = static_cast<const dao::CGameObject *>(creature);
    const std::lock_guard lock(g_mutex);
    g_events.push_back({.creature = object->Id(), .type = type});
}

} // namespace probe
