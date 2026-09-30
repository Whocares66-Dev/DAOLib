#include "ScriptChecks.h"

#include "dao/Addresses.h"
#include "dao/ClientExoApp.h"
#include "dao/Hook.h"
#include "dao/Log.h"
#include "dao/ObjectId.h"
#include "dao/Objects.h"
#include "dao/SafeRead.h"
#include "dao/Script.h"
#include "dao/String.h"
#include "dao/Structures.h"
#include "dao/TypeName.h"
#include "dao/core/ObjectType.h"
#include "dao/generated/ObjectCasts.h"
#include "dao/generated/ScriptApi.h"
#include "dao/generated/ScriptFields.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

namespace probe
{

namespace
{

using dao::generated::ObjectClass;
using dao::generated::ScriptGetter;

// Mismatches logged as they happen, per getter; the rest are counted.
constexpr std::uint32_t kMismatchesLogged = 5;
// Values logged as they come, per check, to show what was seen.
constexpr std::uint32_t kValuesLogged = 3;

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
    // The object itself, checked on every call that found one: its id and
    // type byte (dao::CGameObject), and the engine's cast from the object as
    // the array holds it to the getter's class, against the typed lookup.
    std::atomic<std::uint32_t> objectMatched{0};
    std::atomic<std::uint32_t> objectMismatched{0};
    std::atomic<std::uint32_t> failed{0}; // the engine's handler, or a peek, failed
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

struct Expected
{
    dao::generated::CastSlot cast = dao::generated::CastSlot::CCObject;
    std::optional<dao::core::ObjectType> type; // none for CCObject: any of its classes
};

Expected ExpectedOf(ObjectClass objectClass)
{
    using dao::core::ObjectType;
    using dao::generated::CastSlot;
    switch (objectClass)
    {
    case ObjectClass::CCCreature:
        return {CastSlot::CCCreature, ObjectType::Creature};
    case ObjectClass::CCPlaceable:
        return {CastSlot::CCPlaceable, ObjectType::Placeable};
    case ObjectClass::CCWayPoint:
        return {CastSlot::CCWayPoint, ObjectType::Waypoint};
    case ObjectClass::CCObject:
        break;
    }
    return {CastSlot::CCObject, std::nullopt};
}

// -- calling commands from C++ (dao::script::Invoke, the generated API) -----
//
// From inside the shadows, on the scripts' thread and in the middle of a
// script's command, the probe calls other commands through the library
// and checks their answers against what it already knows. None of the
// three is shadowed, so each call goes straight to the engine's handler.
struct InvokeCheck
{
    const char *name;
    std::atomic<std::uint32_t> matched{0};
    std::atomic<std::uint32_t> mismatched{0};
};
InvokeCheck g_objectType{"api::GetObjectType"};
InvokeCheck g_stringLength{"api::GetStringLength"};
InvokeCheck g_positionFromLocation{"api::GetPositionFromLocation"};

void InvokeResult(InvokeCheck &check, bool same, const std::string &shown)
{
    if (same)
    {
        if (check.matched++ < kValuesLogged)
        {
            spdlog::info("invoke check {}: {}", check.name, shown);
        }
    }
    else if (check.mismatched++ < kMismatchesLogged)
    {
        spdlog::warn("invoke check {}: {}", check.name, shown);
    }
}

void CheckObject(Check &check, dao::CClientExoApp *app, dao::ObjectId object, void *target)
{
    const ScriptGetter &getter = *check.getter;
    const Expected expected = ExpectedOf(getter.objectClass);
    const auto at = reinterpret_cast<std::uintptr_t>(target);
    const auto id = dao::TryRead<dao::ObjectId>(at + dao::CGameObject::kId);
    const auto type = dao::TryRead<std::uint8_t>(at + dao::CGameObject::kType);
    dao::CGameObject *game = app->GetGameObjectById(object);
    void *cast = game != nullptr ? game->Cast(expected.cast) : nullptr;
    const bool typeOk = type && (!expected.type || *type == static_cast<std::uint8_t>(*expected.type));
    if (type)
    {
        const std::int32_t answer = dao::script::api::GetObjectType(object);
        const auto fromType =
            static_cast<std::int32_t>(dao::core::ScriptObjectType(static_cast<dao::core::ObjectType>(*type)));
        InvokeResult(
            g_objectType, answer == fromType,
            fmt::format("object {:#010x}: {} from the command, {} from its type byte", object, answer, fromType));
    }
    if (id && *id == object && typeOk && game == target && cast == target)
    {
        ++check.objectMatched;
        return;
    }
    if (check.objectMismatched++ < kMismatchesLogged)
    {
        spdlog::warn("script check {} ({}): object {:#010x} at {}: id at +{:#x} reads {}, type {}, as a game "
                     "object {}, cast {}",
                     getter.name, getter.command, object, target, dao::CGameObject::kId,
                     id ? fmt::format("{:#010x}", *id) : std::string("nothing"),
                     type ? fmt::format("{}", *type) : std::string("nothing"), static_cast<void *>(game), cast);
    }
}

void Compare(Check &check, dao::ObjectId object, std::uint32_t answer)
{
    const ScriptGetter &getter = *check.getter;
    auto *app = dao::CClientExoApp::Get();
    void *target = app != nullptr ? Lookup(app, getter.objectClass, object) : nullptr;
    if (target == nullptr)
    {
        if (check.noObject++ < kMismatchesLogged)
        {
            spdlog::info("script check {} ({}): id {:#010x} names no object of its class", getter.name, getter.command,
                         object);
        }
        return;
    }
    CheckObject(check, app, object, target);
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

// -- the value types (dao/Structures.h, dao/Script.h's Stack) --------------
//
// Commands whose answer is a string, a vector or a location: after the
// engine's handler, the answer is popped with our wrapper, pushed back,
// popped again and pushed back again, and the two pops compared. A round
// trip through our code on live values, which leaves the script's answer
// as the engine made it. For a location, its area must name an area.
//
// Commands whose argument is an effect: the effect is popped into a copy of
// our own and pushed back before the engine's handler runs, and the
// handler's answer compared with our accessor on the copy.

enum class PeekKind : std::uint8_t
{
    StringAnswer,
    VectorAnswer,
    LocationAnswer,
    EffectField,
    EventField,
    EventAnswer,
    TeamArray,   // object[]: every element on the team the script asked for
    EffectArray, // effect[]: every element a CGameEffect
    ObjectArray, // object[]: every element a live object
};

struct PeekCommand
{
    int command = 0;
    const char *name = nullptr;
    PeekKind kind = PeekKind::StringAnswer;
    bool returnsObject = false;                                       // EffectField: the answer is an object id
    std::uint32_t (*field)(const dao::CGameEffect &) = nullptr;       // EffectField: ours
    std::uint32_t (*eventField)(const dao::CScriptEvent &) = nullptr; // EventField: ours
};

constexpr PeekCommand kPeeks[] = {
    {209, "GetTag", PeekKind::StringAnswer},
    {275, "GetName", PeekKind::StringAnswer},
    {21, "GetPosition", PeekKind::VectorAnswer},
    {457, "GetLocation", PeekKind::LocationAnswer},
    {604, "GetEffectType", PeekKind::EffectField, false,
     [](const dao::CGameEffect &e) { return static_cast<std::uint32_t>(e.Type()); }},
    {605, "GetEffectFlags", PeekKind::EffectField, false, [](const dao::CGameEffect &e) { return e.Flags(); }},
    {607, "GetEffectAnimation", PeekKind::EffectField, false,
     [](const dao::CGameEffect &e) { return static_cast<std::uint32_t>(e.Animation()); }},
    {608, "GetEffectDurationType", PeekKind::EffectField, false,
     [](const dao::CGameEffect &e) { return static_cast<std::uint32_t>(e.DurationType()); }},
    {612, "GetEffectCreator", PeekKind::EffectField, true, [](const dao::CGameEffect &e) { return e.Creator(); }},
    {614, "GetEffectID", PeekKind::EffectField, false, [](const dao::CGameEffect &e) { return e.Id(); }},
    {620, "GetEffectAbilityID", PeekKind::EffectField, false,
     [](const dao::CGameEffect &e) { return static_cast<std::uint32_t>(e.AbilityId()); }},
    {111, "GetEventType", PeekKind::EventField, false, nullptr,
     [](const dao::CScriptEvent &e) { return static_cast<std::uint32_t>(e.Type()); }},
    {109, "GetEventCreator", PeekKind::EventField, true, nullptr,
     [](const dao::CScriptEvent &e) { return e.Creator(); }},
    {126, "GetEventTarget", PeekKind::EventField, true, nullptr, [](const dao::CScriptEvent &e) { return e.Target(); }},
    {108, "GetCurrentEvent", PeekKind::EventAnswer},
    {808, "GetTeam", PeekKind::TeamArray},
    {712, "GetEffects", PeekKind::EffectArray},
    {433, "GetNearestObject", PeekKind::ObjectArray},
};

struct PeekState
{
    const PeekCommand *peek = nullptr;
    dao::script::Handler previous = nullptr;
    std::atomic<std::uint32_t> calls{0};
    std::atomic<std::uint32_t> matched{0};
    std::atomic<std::uint32_t> mismatched{0};
    std::atomic<std::uint32_t> failed{0};
};
std::array<PeekState, dao::script::kSlots> g_peeks;

void Matched(PeekState &state, const std::string &value)
{
    if (state.matched++ < kValuesLogged)
    {
        spdlog::info("script check {} ({}): {}", state.peek->name, state.peek->command, value);
    }
}

void Mismatched(PeekState &state, const std::string &what)
{
    if (state.mismatched++ < kMismatchesLogged)
    {
        spdlog::warn("script check {} ({}): {}", state.peek->name, state.peek->command, what);
    }
}

bool SameVector(const dao::Vector &a, const dao::Vector &b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

std::string Show(const dao::Vector &v)
{
    return fmt::format("({:.2f}, {:.2f}, {:.2f})", v.x, v.y, v.z);
}

void PeekString(PeekState &state)
{
    std::wstring first;
    std::wstring second;
    if (!dao::script::Stack::PopString(first) || !dao::script::Stack::PushString(first) ||
        !dao::script::Stack::PopString(second) || !dao::script::Stack::PushString(second))
    {
        ++state.failed;
        return;
    }
    if (first == second)
    {
        Matched(state, fmt::format("\"{}\", {} characters, round trip the same", dao::Utf8(first), first.size()));
        const std::int32_t length = dao::script::api::GetStringLength(first);
        InvokeResult(g_stringLength, length == static_cast<std::int32_t>(first.size()),
                     fmt::format("\"{}\": {} from the command, {} characters", dao::Utf8(first), length, first.size()));
    }
    else
    {
        Mismatched(state,
                   fmt::format("popped \"{}\", then \"{}\" after our push", dao::Utf8(first), dao::Utf8(second)));
    }
}

void PeekVector(PeekState &state)
{
    dao::Vector first;
    dao::Vector second;
    if (!dao::script::Stack::PopVector(first) || !dao::script::Stack::PushVector(first) ||
        !dao::script::Stack::PopVector(second) || !dao::script::Stack::PushVector(second))
    {
        ++state.failed;
        return;
    }
    if (SameVector(first, second))
    {
        Matched(state, Show(first) + ", round trip the same");
    }
    else
    {
        Mismatched(state, "popped " + Show(first) + ", then " + Show(second) + " after our push");
    }
}

void PeekLocation(PeekState &state)
{
    dao::CScriptLocation first;
    dao::CScriptLocation second;
    if (!dao::script::Stack::PopLocation(first) || !dao::script::Stack::PushLocation(first) ||
        !dao::script::Stack::PopLocation(second) || !dao::script::Stack::PushLocation(second))
    {
        ++state.failed;
        return;
    }
    auto *app = dao::CClientExoApp::Get();
    dao::CGameObject *area = app != nullptr ? app->GetGameObjectById(first.area) : nullptr;
    const bool areaOk = area != nullptr && area->Type() == dao::core::ObjectType::Area;
    const std::string shown = fmt::format("position {}, orientation {}, area {:#010x} ({})", Show(first.position),
                                          Show(first.orientation), first.area, areaOk ? "an area" : "not an area");
    if (SameVector(first.position, second.position) && SameVector(first.orientation, second.orientation) &&
        first.area == second.area && areaOk)
    {
        Matched(state, shown + ", round trip the same");
        const dao::Vector position = dao::script::api::GetPositionFromLocation(first);
        InvokeResult(g_positionFromLocation, SameVector(position, first.position),
                     "from the command " + Show(position) + ", in the location " + Show(first.position));
    }
    else
    {
        Mismatched(state, shown + fmt::format("; after our push: position {}, area {:#010x}", Show(second.position),
                                              second.area));
    }
}

void PeekEvent(PeekState &state)
{
    dao::script::OwnedEvent first = dao::script::Stack::PopEvent();
    if (!first || !dao::script::Stack::PushEvent(*first.get()))
    {
        ++state.failed;
        return;
    }
    dao::script::OwnedEvent second = dao::script::Stack::PopEvent();
    if (!second || !dao::script::Stack::PushEvent(*second.get()))
    {
        ++state.failed;
        return;
    }
    const std::string type = dao::TypeName(first.get());
    const std::string shown = fmt::format("{} of type {}, creator {:#010x}, target {:#010x}", type,
                                          fmt::underlying(first->Type()), first->Creator(), first->Target());
    if (type == "CScriptEvent" && first->Type() == second->Type() && first->Creator() == second->Creator() &&
        first->Target() == second->Target())
    {
        Matched(state, shown + ", round trip the same");
    }
    else
    {
        Mismatched(state, shown + fmt::format("; after our push: type {}, creator {:#010x}",
                                              fmt::underlying(second->Type()), second->Creator()));
    }
}

// An array answer: popped with the library's wrapper, its elements checked,
// pushed back (the VM makes its own copy; ours is freed on return).
void PeekArray(PeekState &state, std::optional<std::int32_t> team)
{
    dao::script::Array array = dao::script::Stack::PopArray();
    if (!array || !dao::script::Stack::PushArray(array))
    {
        ++state.failed;
        return;
    }
    const PeekKind kind = state.peek->kind;
    auto *app = dao::CClientExoApp::Get();
    std::size_t good = 0;
    std::string first;
    for (std::size_t i = 0; i < array.Size(); ++i)
    {
        if (kind == PeekKind::EffectArray)
        {
            const std::string type = dao::TypeName(array.Effect(i));
            good += type == "CGameEffect" ? 1 : 0;
            if (i == 0)
            {
                first = fmt::format(", the first {} of type {}", type,
                                    array.Effect(0) != nullptr ? static_cast<int>(array.Effect(0)->Type()) : -1);
            }
            continue;
        }
        const dao::ObjectId id = array.Object(i);
        dao::CGameObject *object = app != nullptr ? app->GetGameObjectById(id) : nullptr;
        dao::CCObject *asObject = object != nullptr ? object->AsObject() : nullptr;
        if (kind == PeekKind::TeamArray)
        {
            good += asObject != nullptr && team && asObject->TeamId() == *team ? 1 : 0;
        }
        else
        {
            good += object != nullptr ? 1 : 0;
        }
        if (i == 0)
        {
            first =
                fmt::format(", the first {:#010x} ({})", id, object != nullptr ? static_cast<int>(object->Type()) : -1);
        }
    }
    const std::string shown = fmt::format("tag {:#x}, {} elements, {} as expected{}{}", array.Tag(), array.Size(), good,
                                          team ? fmt::format(", team {}", *team) : std::string(), first);
    const std::int32_t wantTag = kind == PeekKind::EffectArray ? 0x53 : 0x43;
    if (array.Tag() == wantTag && good == array.Size())
    {
        Matched(state, shown);
    }
    else
    {
        Mismatched(state, shown);
    }
}

int __fastcall PeekShadow(void *commands, int edx, int id, int paramCount)
{
    PeekState &state = g_peeks.at(static_cast<std::size_t>(id));
    const PeekCommand &peek = *state.peek;
    if (peek.kind == PeekKind::EventField)
    {
        dao::script::OwnedEvent event = dao::script::Stack::PopEvent();
        const bool peeked = event && dao::script::Stack::PushEvent(*event.get());
        const int result = state.previous(commands, edx, id, paramCount);
        ++state.calls;
        if (!peeked || result != dao::script::kOk)
        {
            ++state.failed;
            return result;
        }
        const auto answer = PeekAnswer(peek.returnsObject);
        if (!answer)
        {
            ++state.failed;
            return result;
        }
        const std::uint32_t ours = peek.eventField(*event.get());
        if (ours == *answer)
        {
            Matched(state,
                    fmt::format("event of type {}, engine and field both {:#x}", fmt::underlying(event->Type()), ours));
        }
        else
        {
            Mismatched(state, fmt::format("event of type {}: engine said {:#x}, field reads {:#x}",
                                          fmt::underlying(event->Type()), *answer, ours));
        }
        return result;
    }
    std::optional<std::int32_t> team;
    if (peek.kind == PeekKind::TeamArray)
    {
        std::int32_t value = 0;
        if (dao::script::Stack::PopInt(value) && dao::script::Stack::PushInt(value))
        {
            team = value;
        }
    }
    if (peek.kind == PeekKind::EffectField)
    {
        dao::script::OwnedEffect effect = dao::script::Stack::PopEffect();
        const bool peeked = effect && dao::script::Stack::PushEffect(*effect.get());
        const int result = state.previous(commands, edx, id, paramCount);
        ++state.calls;
        if (!peeked || result != dao::script::kOk)
        {
            ++state.failed;
            return result;
        }
        const auto answer = PeekAnswer(peek.returnsObject);
        if (!answer)
        {
            ++state.failed;
            return result;
        }
        const std::uint32_t ours = peek.field(*effect.get());
        if (ours == *answer)
        {
            Matched(state, fmt::format("effect {} of type {}, engine and field both {:#x}", effect->Id(),
                                       fmt::underlying(effect->Type()), ours));
        }
        else
        {
            Mismatched(state, fmt::format("effect {} of type {}: engine said {:#x}, field reads {:#x}", effect->Id(),
                                          fmt::underlying(effect->Type()), *answer, ours));
        }
        return result;
    }
    const int result = state.previous(commands, edx, id, paramCount);
    ++state.calls;
    if (result != dao::script::kOk)
    {
        ++state.failed;
        return result;
    }
    switch (peek.kind)
    {
    case PeekKind::StringAnswer:
        PeekString(state);
        break;
    case PeekKind::VectorAnswer:
        PeekVector(state);
        break;
    case PeekKind::LocationAnswer:
        PeekLocation(state);
        break;
    case PeekKind::EventAnswer:
        PeekEvent(state);
        break;
    case PeekKind::TeamArray:
    case PeekKind::EffectArray:
    case PeekKind::ObjectArray:
        PeekArray(state, team);
        break;
    case PeekKind::EffectField:
    case PeekKind::EventField:
        break;
    }
    return result;
}

std::size_t InstallPeeks()
{
    std::size_t installed = 0;
    for (const auto &peek : kPeeks)
    {
        PeekState &state = g_peeks.at(static_cast<std::size_t>(peek.command));
        dao::script::Handler previous = dao::script::Get(peek.command);
        if (previous == nullptr)
        {
            spdlog::warn("script check {} ({}): no handler in the table", peek.name, peek.command);
            continue;
        }
        state.peek = &peek;
        state.previous = previous;
        dao::script::Set(peek.command, &PeekShadow);
        ++installed;
    }
    return installed;
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
    spdlog::info(
        "script checks: {} of {} value-type commands shadowed (strings, vectors, locations, effects, events, arrays)",
        InstallPeeks(), std::size(kPeeks));
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
                     "object, {} failed; the object's id, type and cast {} matched, {} mismatched",
                     getter.name, getter.command, calls, matched, check.matchedNonZero.load(), mismatched,
                     check.noObject.load(), check.failed.load(), check.objectMatched.load(),
                     check.objectMismatched.load());
    }
    for (const auto &peek : kPeeks)
    {
        const PeekState &state = g_peeks.at(static_cast<std::size_t>(peek.command));
        if (state.peek == nullptr || state.calls == 0)
        {
            continue;
        }
        spdlog::info("script check {} ({}): {} calls, {} matched, {} mismatched, {} failed", peek.name, peek.command,
                     state.calls.load(), state.matched.load(), state.mismatched.load(), state.failed.load());
    }
    for (const InvokeCheck *check : {&g_objectType, &g_stringLength, &g_positionFromLocation})
    {
        if (check->matched + check->mismatched > 0)
        {
            spdlog::info("invoke check {}: {} matched, {} mismatched", check->name, check->matched.load(),
                         check->mismatched.load());
        }
    }
    spdlog::info("script checks: {} of {} getters called, {} of those matched on every object", called,
                 std::size(dao::generated::kScriptGetters), clean);
}

} // namespace probe
