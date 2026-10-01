// Script commands described at run time: each command's name, number,
// parameter types and defaults and its answer's type, and values typed at
// run time to call one with. dao/ScriptCall.h calls a command by name with
// these; dao/generated/ScriptTable.h, written by tools/gen_script_api.py
// from script.ldf's signatures, is the table of every command.
//
//   const auto *command = dao::script::table::Find("GetTeamId");
//   const auto answer = dao::script::Call(*command, {Value::Object(id)});
//
// What a call by name moves: int, float, string, object, vector and
// resource arguments, and those answers, and arrays of int, float, string
// and object as lists of values. Locations, effects, events and commands
// are the engine's own structures and stay with the typed wrappers
// (dao/generated/ScriptApi.h). Checking the arguments against a command
// (their count, their types, the defaults for those left off) needs no
// game, and is here, tested.
#pragma once

#include "dao/ObjectId.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dao::core
{

enum class ScriptType : std::uint8_t
{
    Void,
    Int,
    Float,
    String,
    Object,
    Vector,
    Location,
    Effect,
    Event,
    Command,
    Resource,
    Array,
};

// script.ldf's word for a type: "int", "object", "array" for any array.
std::string_view ScriptTypeName(ScriptType type);

// Whether a call by name moves the type, as an argument or an answer.
constexpr bool MovesByName(ScriptType type)
{
    switch (type)
    {
    case ScriptType::Void:
    case ScriptType::Int:
    case ScriptType::Float:
    case ScriptType::String:
    case ScriptType::Object:
    case ScriptType::Vector:
    case ScriptType::Resource:
        return true;
    default:
        return false;
    }
}

struct ScriptValue
{
    ScriptType type = ScriptType::Void;
    std::int32_t integer = 0;
    float number = 0;
    std::wstring text; // a string's, or a resource's name
    ObjectId object = kInvalidObject;
    float x = 0, y = 0, z = 0; // a vector's
    // OBJECT_SELF, to be read when the call is made (a default only).
    bool self = false;
    // An array's elements, each of the array's element type; only int,
    // float, string and object arrays are read.
    std::vector<ScriptValue> elements;

    static ScriptValue Int(std::int32_t value);
    static ScriptValue Float(float value);
    static ScriptValue String(std::wstring value);
    static ScriptValue Object(ObjectId value);
    static ScriptValue Vector(float x, float y, float z);
    static ScriptValue Resource(std::wstring name);
    static ScriptValue Self();
};

enum class DefaultKind : std::uint8_t
{
    None,  // the argument must be given
    Given, // the value below, of the parameter's type
    Self,  // OBJECT_SELF
};

struct ScriptParam
{
    std::string_view name;
    ScriptType type = ScriptType::Void;
    DefaultKind defaultKind = DefaultKind::None;
    // The default, by the parameter's type: an int, or an object's id;
    // a float; a string or a resource's name. A vector's is always zero.
    std::int32_t intDefault = 0;
    float floatDefault = 0;
    std::wstring_view textDefault;

    std::optional<ScriptValue> Default() const;
};

struct ScriptCommandInfo
{
    std::string_view name;
    std::int32_t id = 0;
    ScriptType returns = ScriptType::Void;
    std::span<const ScriptParam> params;
    std::string_view signature; // as script.ldf writes it

    // Whether every parameter's type and the answer's move by name; an
    // array answer does too.
    bool CallableByName() const;
};

// The arguments for a call: those given, in order, then the defaults of
// the rest. Nothing, and `error` says why, if too many are given, one is
// missing that has no default, or one is not of its parameter's type (an
// int is taken for a float, nothing else is converted).
std::optional<std::vector<ScriptValue>> CompleteArguments(const ScriptCommandInfo &command,
                                                          std::span<const ScriptValue> given, std::string &error);

// A command by name, case-insensitively, or by number, in a table.
const ScriptCommandInfo *FindCommand(std::span<const ScriptCommandInfo> table, std::string_view name);
const ScriptCommandInfo *FindCommand(std::span<const ScriptCommandInfo> table, std::int32_t id);

} // namespace dao::core
