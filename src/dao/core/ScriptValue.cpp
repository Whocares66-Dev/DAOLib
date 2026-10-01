#include "dao/core/ScriptValue.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace dao::core
{

std::string_view ScriptTypeName(ScriptType type)
{
    switch (type)
    {
    case ScriptType::Void:
        return "void";
    case ScriptType::Int:
        return "int";
    case ScriptType::Float:
        return "float";
    case ScriptType::String:
        return "string";
    case ScriptType::Object:
        return "object";
    case ScriptType::Vector:
        return "vector";
    case ScriptType::Location:
        return "location";
    case ScriptType::Effect:
        return "effect";
    case ScriptType::Event:
        return "event";
    case ScriptType::Command:
        return "command";
    case ScriptType::Resource:
        return "resource";
    case ScriptType::Array:
        return "array";
    }
    return "?";
}

ScriptValue ScriptValue::Int(std::int32_t value)
{
    ScriptValue v;
    v.type = ScriptType::Int;
    v.integer = value;
    return v;
}

ScriptValue ScriptValue::Float(float value)
{
    ScriptValue v;
    v.type = ScriptType::Float;
    v.number = value;
    return v;
}

ScriptValue ScriptValue::String(std::wstring value)
{
    ScriptValue v;
    v.type = ScriptType::String;
    v.text = std::move(value);
    return v;
}

ScriptValue ScriptValue::Object(ObjectId value)
{
    ScriptValue v;
    v.type = ScriptType::Object;
    v.object = value;
    return v;
}

ScriptValue ScriptValue::Vector(float x, float y, float z)
{
    ScriptValue v;
    v.type = ScriptType::Vector;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

ScriptValue ScriptValue::Resource(std::wstring name)
{
    ScriptValue v;
    v.type = ScriptType::Resource;
    v.text = std::move(name);
    return v;
}

ScriptValue ScriptValue::Self()
{
    ScriptValue v;
    v.type = ScriptType::Object;
    v.self = true;
    return v;
}

std::optional<ScriptValue> ScriptParam::Default() const
{
    switch (defaultKind)
    {
    case DefaultKind::None:
        return std::nullopt;
    case DefaultKind::Self:
        return ScriptValue::Self();
    case DefaultKind::Given:
        break;
    }
    switch (type)
    {
    case ScriptType::Int:
        return ScriptValue::Int(intDefault);
    case ScriptType::Float:
        return ScriptValue::Float(floatDefault);
    case ScriptType::String:
        return ScriptValue::String(std::wstring(textDefault));
    case ScriptType::Object:
        return ScriptValue::Object(static_cast<ObjectId>(intDefault));
    case ScriptType::Vector:
        return ScriptValue::Vector(0, 0, 0);
    case ScriptType::Resource:
        return ScriptValue::Resource(std::wstring(textDefault));
    default:
        return std::nullopt;
    }
}

bool ScriptCommandInfo::CallableByName() const
{
    return (MovesByName(returns) || returns == ScriptType::Array) &&
           std::ranges::all_of(params, [](const ScriptParam &p) { return MovesByName(p.type); });
}

std::optional<std::vector<ScriptValue>> CompleteArguments(const ScriptCommandInfo &command,
                                                          std::span<const ScriptValue> given, std::string &error)
{
    if (given.size() > command.params.size())
    {
        error = std::string(command.name) + " takes " + std::to_string(command.params.size()) + " arguments, " +
                std::to_string(given.size()) + " given";
        return std::nullopt;
    }
    std::vector<ScriptValue> out;
    out.reserve(command.params.size());
    for (std::size_t i = 0; i < command.params.size(); ++i)
    {
        const ScriptParam &param = command.params[i];
        if (i >= given.size())
        {
            auto value = param.Default();
            if (!value)
            {
                error = std::string(command.name) + ": " + std::string(param.name) + " has no default";
                return std::nullopt;
            }
            out.push_back(std::move(*value));
            continue;
        }
        const ScriptValue &value = given[i];
        if (value.type == param.type)
        {
            out.push_back(value);
        }
        else if (value.type == ScriptType::Int && param.type == ScriptType::Float)
        {
            out.push_back(ScriptValue::Float(static_cast<float>(value.integer)));
        }
        else
        {
            error = std::string(command.name) + ": " + std::string(param.name) + " is " +
                    std::string(ScriptTypeName(param.type)) + ", given " + std::string(ScriptTypeName(value.type));
            return std::nullopt;
        }
    }
    return out;
}

namespace
{
bool SameName(std::string_view a, std::string_view b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}
} // namespace

const ScriptCommandInfo *FindCommand(std::span<const ScriptCommandInfo> table, std::string_view name)
{
    const auto it = std::ranges::find_if(table, [&](const ScriptCommandInfo &c) { return SameName(c.name, name); });
    return it != table.end() ? &*it : nullptr;
}

const ScriptCommandInfo *FindCommand(std::span<const ScriptCommandInfo> table, std::int32_t id)
{
    const auto it = std::ranges::find_if(table, [&](const ScriptCommandInfo &c) { return c.id == id; });
    return it != table.end() ? &*it : nullptr;
}

} // namespace dao::core
