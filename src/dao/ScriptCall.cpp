// Calling a script command by its name.
//
// The command comes from dao/generated/ScriptTable.h, which says each one's
// parameter types and defaults; the arguments are checked against it
// first (dao/core/ScriptValue.h, CompleteArguments), and those left off
// take their defaults, OBJECT_SELF being read at the call. Then the call is
// made as dao::script::Invoke makes one. int, float, string, object, vector
// and resource move by name; an array answer of ints, floats, strings or
// objects comes back as its elements. A command with a location, effect,
// event or command anywhere in its signature is refused.
#include "dao/ScriptCall.h"

#include "dao/Script.h"
#include "dao/Structures.h"

#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace dao::script
{

namespace
{

using core::ScriptType;

bool PushValue(const Value &value)
{
    switch (value.type)
    {
    case ScriptType::Int:
        return detail::Push(value.integer);
    case ScriptType::Float:
        return detail::Push(value.number);
    case ScriptType::String:
        return detail::Push(std::wstring_view(value.text));
    case ScriptType::Object:
        return detail::Push(value.self ? Self() : value.object);
    case ScriptType::Vector:
        return detail::Push(Vector{value.x, value.y, value.z});
    case ScriptType::Resource:
        return detail::Push(Resource{value.text});
    default:
        return false;
    }
}

// An array's elements as values, for the element types a call by name
// reads; nothing for an array of engine structures.
std::optional<Value> ArrayValue(const Array &array)
{
    Value out;
    out.type = ScriptType::Array;
    for (std::size_t i = 0; i < array.Size(); ++i)
    {
        switch (static_cast<core::ArrayTag>(array.Tag()))
        {
        case core::ArrayTag::Int:
            out.elements.push_back(Value::Int(array.Int(i)));
            break;
        case core::ArrayTag::Float:
            out.elements.push_back(Value::Float(array.Float(i)));
            break;
        case core::ArrayTag::String:
            out.elements.push_back(Value::String(std::wstring(array.String(i))));
            break;
        case core::ArrayTag::Object:
            out.elements.push_back(Value::Object(array.Object(i)));
            break;
        default:
            return std::nullopt;
        }
    }
    return out;
}

// The answer, popped by its type.
std::optional<Value> PopAnswer(ScriptType type)
{
    switch (type)
    {
    case ScriptType::Void:
        return Value{};
    case ScriptType::Int:
        if (const auto v = detail::Pop<std::int32_t>())
        {
            return Value::Int(*v);
        }
        break;
    case ScriptType::Float:
        if (const auto v = detail::Pop<float>())
        {
            return Value::Float(*v);
        }
        break;
    case ScriptType::String:
        if (auto v = detail::Pop<std::wstring>())
        {
            return Value::String(std::move(*v));
        }
        break;
    case ScriptType::Object:
        if (const auto v = detail::Pop<ObjectId>())
        {
            return Value::Object(*v);
        }
        break;
    case ScriptType::Vector:
        if (const auto v = detail::Pop<Vector>())
        {
            return Value::Vector(v->x, v->y, v->z);
        }
        break;
    case ScriptType::Resource:
        if (auto v = detail::Pop<Resource>())
        {
            return Value::Resource(std::move(v->name));
        }
        break;
    case ScriptType::Array:
        if (const auto v = detail::Pop<Array>())
        {
            return ArrayValue(*v);
        }
        break;
    default:
        break;
    }
    return std::nullopt;
}

} // namespace

CallResult Call(const CommandInfo &command, std::span<const Value> args)
{
    CallResult result;
    if (!command.CallableByName())
    {
        result.error =
            std::string(command.name) + " has a type a call by name does not move: " + std::string(command.signature);
        return result;
    }
    const auto values = core::CompleteArguments(command, args, result.error);
    if (!values)
    {
        return result;
    }
    const std::int32_t depth = StackDepth();
    // Pushed last first, as a script pushes them.
    for (const Value &value : std::views::reverse(*values))
    {
        if (!PushValue(value))
        {
            TruncateStack(depth);
            result.error = std::string(command.name) + ": the VM's stack refused an argument";
            return result;
        }
    }
    const int code = detail::Run(command.id, static_cast<int>(values->size()));
    if (code != kOk)
    {
        TruncateStack(depth);
        result.error = std::string(command.name) + " failed, code " + std::to_string(code);
        return result;
    }
    auto answer = PopAnswer(command.returns);
    if (!answer)
    {
        TruncateStack(depth);
        result.error = std::string(command.name) + ": its answer could not be read";
        return result;
    }
    result.ok = true;
    result.value = std::move(*answer);
    return result;
}

CallResult Call(std::string_view name, std::span<const Value> args)
{
    const CommandInfo *command = table::Find(name);
    if (command == nullptr)
    {
        CallResult result;
        result.error = "no script command " + std::string(name);
        return result;
    }
    return Call(*command, args);
}

} // namespace dao::script
