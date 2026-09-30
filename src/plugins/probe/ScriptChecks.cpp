#include "ScriptChecks.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/ObjectId.h"
#include "dao/SafeRead.h"
#include "dao/Script.h"
#include "dao/generated/ScriptFields.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>

namespace probe
{

namespace
{

using dao::generated::ObjectClass;
using dao::generated::ScriptGetter;

// Mismatches logged as they happen, per getter; the rest are counted.
constexpr std::uint32_t kMismatchesLogged = 5;

struct Check
{
    const ScriptGetter *getter = nullptr;
    dao::script::Handler previous = nullptr;
    std::atomic<std::uint32_t> calls{0};
    std::atomic<std::uint32_t> matched{0};
    // Of those, on a value other than 0: a field at a wrong offset often
    // reads 0 where the true one is 0 too, so these are the stronger matches.
    std::atomic<std::uint32_t> matchedNonZero{0};
    std::atomic<std::uint32_t> mismatched{0};
    std::atomic<std::uint32_t> noObject{0}; // the id named no object of the getter's class
    std::atomic<std::uint32_t> failed{0};   // the engine's handler, or a peek, failed
};

// By command number. Written once, on the engine's thread, before any
// script runs; read by the shadow and by the report after.
std::array<Check, dao::script::kSlots> g_checks;
std::atomic<bool> g_installed{false};

void *Lookup(dao::CClientExoApp *app, ObjectClass objectClass, dao::ObjectId id)
{
    switch (objectClass)
    {
    case ObjectClass::CCCreature:
        return app->GetCreatureById(id);
    case ObjectClass::CCPlaceable:
        return app->GetPlaceableById(id);
    case ObjectClass::CCWayPoint:
        return app->GetWayPointById(id);
    case ObjectClass::CCObject:
        break;
    }
    return app->GetObjectById(id);
}

// The field as the getter's path and width say, every load guarded:
// zero-extended, and sign-extended, since which the engine does for a
// narrow field is not in the tables.
struct FieldValue
{
    std::uint32_t zeroExtended = 0;
    std::uint32_t signExtended = 0;
};

std::optional<FieldValue> ReadField(const void *object, const ScriptGetter &getter)
{
    auto at = reinterpret_cast<std::uintptr_t>(object);
    for (std::size_t i = 0; i + 1 < getter.depth; ++i)
    {
        const auto next = dao::TryRead<std::uint32_t>(at + getter.offsets.at(i));
        if (!next || *next == 0)
        {
            return std::nullopt;
        }
        at = *next;
    }
    at += getter.offsets.at(getter.depth - 1);
    switch (getter.width)
    {
    case 1:
        if (const auto v = dao::TryRead<std::uint8_t>(at))
        {
            return FieldValue{*v, static_cast<std::uint32_t>(static_cast<std::int8_t>(*v))};
        }
        return std::nullopt;
    case 2:
        if (const auto v = dao::TryRead<std::uint16_t>(at))
        {
            return FieldValue{*v, static_cast<std::uint32_t>(static_cast<std::int16_t>(*v))};
        }
        return std::nullopt;
    default:
        if (const auto v = dao::TryRead<std::uint32_t>(at))
        {
            return FieldValue{*v, *v};
        }
        return std::nullopt;
    }
}

// The answer the engine's handler pushed, read and pushed back.
std::optional<std::uint32_t> PeekAnswer(bool isObject)
{
    if (isObject)
    {
        dao::ObjectId value = 0;
        if (dao::script::Stack::PopObject(value) && dao::script::Stack::PushObject(value))
        {
            return value;
        }
        return std::nullopt;
    }
    std::int32_t value = 0;
    if (dao::script::Stack::PopInt(value) && dao::script::Stack::PushInt(value))
    {
        return static_cast<std::uint32_t>(value);
    }
    return std::nullopt;
}

void Compare(Check &check, dao::ObjectId object, std::uint32_t answer)
{
    const ScriptGetter &getter = *check.getter;
    auto *app = dao::CClientExoApp::Get();
    void *target = app != nullptr ? Lookup(app, getter.objectClass, object) : nullptr;
    if (target == nullptr)
    {
        ++check.noObject;
        return;
    }
    const auto field = ReadField(target, getter);
    if (field && (answer == field->zeroExtended || answer == field->signExtended))
    {
        if (check.matched++ == 0)
        {
            spdlog::info("script check {} ({}): first call, object {:#010x} at {}, engine and field both {:#x}",
                         getter.name, getter.command, object, target, answer);
        }
        if (answer != 0 && check.matchedNonZero++ == 0)
        {
            spdlog::info("script check {} ({}): first match on a value other than 0, object {:#010x}, {:#x}",
                         getter.name, getter.command, object, answer);
        }
        return;
    }
    if (check.mismatched++ < kMismatchesLogged)
    {
        spdlog::warn("script check {} ({}): object {:#010x} at {}, engine said {:#x}, field at +{:#x} depth {} "
                     "reads {}",
                     getter.name, getter.command, object, target, answer, getter.offsets.at(getter.depth - 1),
                     getter.depth, field ? fmt::format("{:#x}", field->zeroExtended) : std::string("nothing"));
    }
}

// Every checked getter's slot holds this. The command's number says which
// check it is; the engine's handler does the work, unchanged.
int __fastcall Shadow(void *commands, int edx, int id, int paramCount)
{
    Check &check = g_checks.at(static_cast<std::size_t>(id));
    dao::ObjectId object = dao::kInvalidObject;
    const bool peeked = dao::script::Stack::PopObject(object) && dao::script::Stack::PushObject(object);
    const int result = check.previous(commands, edx, id, paramCount);
    ++check.calls;
    if (!peeked || result != dao::script::kOk)
    {
        ++check.failed;
        return result;
    }
    const auto answer = PeekAnswer(check.getter->returnsObject);
    if (!answer)
    {
        ++check.failed;
        return result;
    }
    Compare(check, object, *answer);
    return result;
}

void Install(void *commands)
{
    auto *app = dao::CClientExoApp::Get();
    if (app == nullptr || app->Commands() != commands)
    {
        // Another commands object (the client's console commands have a
        // class of their own), or the reading of +0x1E4 is wrong.
        spdlog::warn("script checks: InitializeCommands on {}, the application's commands are {}; not installed",
                     commands, app != nullptr ? app->Commands() : nullptr);
        return;
    }
    if (g_installed.exchange(true))
    {
        spdlog::warn("script checks: InitializeCommands ran again, on {}; the checks stay as they were", commands);
        return;
    }
    std::size_t installed = 0;
    for (const auto &getter : dao::generated::kScriptGetters)
    {
        Check &check = g_checks.at(static_cast<std::size_t>(getter.command));
        dao::script::Handler previous = dao::script::Get(getter.command);
        if (previous == nullptr)
        {
            spdlog::warn("script check {} ({}): no handler in the table", getter.name, getter.command);
            continue;
        }
        check.getter = &getter;
        check.previous = previous;
        dao::script::Set(getter.command, &Shadow);
        ++installed;
    }
    spdlog::info("script checks: {} of {} getters shadowed, table at {}", installed,
                 std::size(dao::generated::kScriptGetters), static_cast<void *>(dao::script::Table()));
}

// CVirtualMachineCommands::InitializeCommands: __thiscall, no arguments,
// `ret`.
using InitializeCommandsFn = void(__fastcall *)(void *self, int edx);
dao::Detour<InitializeCommandsFn> g_initializeCommands;

void __fastcall InitializeCommands(void *self, int edx)
{
    g_initializeCommands.Original()(self, edx);
    Install(self);
}

bool Disabled()
{
    std::array<char, 8> value{};
    std::size_t length = 0;
    return getenv_s(&length, value.data(), value.size(), "DAOLIB_PROBE_SCRIPT_CHECKS") == 0 && length > 1 &&
           value[0] == '0';
}

} // namespace

void AttachScriptChecks()
{
    if (Disabled())
    {
        spdlog::info("script checks: off (DAOLIB_PROBE_SCRIPT_CHECKS=0)");
        return;
    }
    if (g_initializeCommands.Attach(dao::addr::kCVirtualMachineCommands_InitializeCommands, &InitializeCommands))
    {
        spdlog::info("script checks: detoured CVirtualMachineCommands::InitializeCommands at {:#010x}",
                     dao::addr::kCVirtualMachineCommands_InitializeCommands);
    }
    else
    {
        spdlog::error("script checks: could not detour InitializeCommands at {:#010x}",
                      dao::addr::kCVirtualMachineCommands_InitializeCommands);
    }
}

void ReportScriptChecks()
{
    if (!g_installed)
    {
        return;
    }
    std::size_t called = 0;
    std::size_t clean = 0;
    for (const auto &getter : dao::generated::kScriptGetters)
    {
        const Check &check = g_checks.at(static_cast<std::size_t>(getter.command));
        const std::uint32_t calls = check.calls;
        if (check.getter == nullptr || calls == 0)
        {
            continue;
        }
        ++called;
        const std::uint32_t matched = check.matched;
        const std::uint32_t mismatched = check.mismatched;
        if (mismatched == 0 && matched > 0)
        {
            ++clean;
        }
        spdlog::info("script check {} ({}): {} calls, {} matched ({} on a value other than 0), {} mismatched, {} on no "
                     "object, {} failed",
                     getter.name, getter.command, calls, matched, check.matchedNonZero.load(), mismatched,
                     check.noObject.load(), check.failed.load());
    }
    spdlog::info("script checks: {} of {} getters called, {} of those matched on every object", called,
                 std::size(dao::generated::kScriptGetters), clean);
}

} // namespace probe
