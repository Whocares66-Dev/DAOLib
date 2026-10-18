// The game's script.ldf read: the engine functions a script calls, each by
// its command number with its return type and parameters, the constants the
// scripts share, and the file's #defines (the engine structures' names among
// them). The script compiler takes its declarations from it; a script's
// ACTION names a function by its number (dao/core/Ncs.h).
//
//   const auto ldf = dao::core::ldf::LdfOf(text);
//   const auto *pause = ldf.FunctionOf(188); // void ToggleGamePause(int bPause = TRUE)
//
// The file is in packages/core/data/scripts.erf and the toolset's ldf
// folder: C-like text, a declaration "type Name(type name = default, ...) =
// number;", a constant "type NAME = value;", comments of both kinds. Read
// 2026-10-17 (DAOLib #167): 674 declarations, two of them returning an
// array written with a space ("object []", "int []"), and 481 constants
// (475 int, 5 float, 1 string), two of them named by another (TRUE, FALSE).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dao::core::ldf
{

struct Parameter
{
    std::string type; // "int", "object", "effect[]"
    std::string name;
    std::string defaultValue; // as written, "OBJECT_INVALID", "0.0f"; empty for none
};

struct Function
{
    std::int32_t number = 0; // the command's, as ACTION and dev/SCRIPT_COMMANDS.md number it
    std::string returns;
    std::string name;
    std::vector<Parameter> parameters;
};

struct Constant
{
    std::string type;
    std::string name;
    std::string value; // as written: "-1", "3.141592f", "\"nwscript\"", "TRUE"
};

struct Ldf
{
    std::vector<std::pair<std::string, std::string>> defines; // "ENGINE_STRUCTURE_1", "location"
    std::vector<Constant> constants;
    std::vector<Function> functions;

    // Returns the function of a command number; null for none.
    const Function *FunctionOf(std::int32_t number) const;
    // Returns the constant of a name; null for none.
    const Constant *ConstantOf(std::string_view name) const;
    // Returns an int constant's value, following a constant named by another; nothing for a
    // name that is no int constant.
    std::optional<std::int32_t> IntOf(std::string_view name) const;
};

// Returns what a script.ldf's text declares.
Ldf LdfOf(std::string_view text);

} // namespace dao::core::ldf
