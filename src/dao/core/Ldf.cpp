#include "dao/core/Ldf.h"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace dao::core::ldf
{

namespace
{

// A constant named by another is followed no further than this.
constexpr int kMostHops = 8;

std::string_view Trim(std::string_view text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
    {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

// The text with its comments blanked, strings kept whole.
std::string WithoutComments(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == '"')
        {
            const std::size_t end = text.find('"', i + 1);
            const std::size_t stop = end == std::string_view::npos ? text.size() : end + 1;
            out.append(text.substr(i, stop - i));
            i = stop - 1;
        }
        else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/')
        {
            const std::size_t end = text.find('\n', i);
            i = (end == std::string_view::npos ? text.size() : end) - 1;
        }
        else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*')
        {
            const std::size_t end = text.find("*/", i + 2);
            i = (end == std::string_view::npos ? text.size() : end + 2) - 1;
            out.push_back(' ');
        }
        else
        {
            out.push_back(c);
        }
    }
    return out;
}

// The text split at a separator that is in no string and no brackets: a
// vector's default is written [0.0,0.0,0.0] (CommandUseAbility's vTarget).
std::vector<std::string_view> SplitOutsideStrings(std::string_view text, char separator)
{
    std::vector<std::string_view> parts;
    bool quoted = false;
    int depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '"')
        {
            quoted = !quoted;
        }
        else if (!quoted && text[i] == '[')
        {
            ++depth;
        }
        else if (!quoted && text[i] == ']')
        {
            --depth;
        }
        else if (text[i] == separator && !quoted && depth == 0)
        {
            parts.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.push_back(text.substr(start));
    return parts;
}

// "type name" with any space between, the type perhaps an array's.
std::optional<std::pair<std::string, std::string>> TypeAndName(std::string_view text)
{
    text = Trim(text);
    const auto space = text.find_last_of(" \t\r\n");
    if (space == std::string_view::npos)
    {
        return std::nullopt;
    }
    return std::pair{std::string(Trim(text.substr(0, space))), std::string(Trim(text.substr(space + 1)))};
}

std::optional<Function> FunctionOf(std::string_view statement)
{
    const auto open = statement.find('(');
    const auto close = statement.rfind(')');
    const auto equals = statement.rfind('=');
    if (open == std::string_view::npos || close == std::string_view::npos || equals == std::string_view::npos ||
        open >= close || close >= equals)
    {
        return std::nullopt;
    }
    const auto head = TypeAndName(statement.substr(0, open));
    const auto number = Trim(statement.substr(equals + 1));
    Function function;
    if (!head || std::from_chars(number.data(), number.data() + number.size(), function.number).ec != std::errc())
    {
        return std::nullopt;
    }
    function.returns = head->first;
    function.name = head->second;
    const auto list = Trim(statement.substr(open + 1, close - open - 1));
    if (list.empty())
    {
        return function;
    }
    for (const auto part : SplitOutsideStrings(list, ','))
    {
        const auto at = part.find('=');
        const auto declared = TypeAndName(part.substr(0, at));
        if (!declared)
        {
            return std::nullopt;
        }
        function.parameters.push_back(
            {.type = declared->first,
             .name = declared->second,
             .defaultValue = at == std::string_view::npos ? std::string() : std::string(Trim(part.substr(at + 1)))});
    }
    return function;
}

std::optional<Constant> ConstantOf(std::string_view statement)
{
    const auto equals = statement.find('=');
    if (equals == std::string_view::npos || statement.find('(') != std::string_view::npos)
    {
        return std::nullopt;
    }
    const auto declared = TypeAndName(statement.substr(0, equals));
    if (!declared)
    {
        return std::nullopt;
    }
    return Constant{
        .type = declared->first, .name = declared->second, .value = std::string(Trim(statement.substr(equals + 1)))};
}

} // namespace

const Function *Ldf::FunctionOf(std::int32_t number) const
{
    const auto found = std::ranges::find_if(functions, [&](const Function &f) { return f.number == number; });
    return found != functions.end() ? &*found : nullptr;
}

const Constant *Ldf::ConstantOf(std::string_view name) const
{
    const auto found = std::ranges::find_if(constants, [&](const Constant &c) { return c.name == name; });
    return found != constants.end() ? &*found : nullptr;
}

std::optional<std::int32_t> Ldf::IntOf(std::string_view name) const
{
    for (int hop = 0; hop < kMostHops; ++hop)
    {
        const Constant *constant = ConstantOf(name);
        if (constant == nullptr || constant->type != "int")
        {
            return std::nullopt;
        }
        std::int32_t value = 0;
        const auto &text = constant->value;
        const bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
        const char *first = text.data() + (hex ? 2 : 0);
        if (std::from_chars(first, text.data() + text.size(), value, hex ? 16 : 10).ec == std::errc())
        {
            return value;
        }
        name = text;
    }
    return std::nullopt;
}

// The #defines are lines of their own; the rest is statements ending in
// ';', a declaration being the one with a parameter list and a number.
Ldf LdfOf(std::string_view text)
{
    Ldf ldf;
    const std::string code = WithoutComments(text);
    std::string statements;
    statements.reserve(code.size());
    for (const auto line : SplitOutsideStrings(code, '\n'))
    {
        const auto trimmed = Trim(line);
        if (trimmed.starts_with("#define"))
        {
            const auto rest = Trim(trimmed.substr(7));
            const auto space = rest.find_first_of(" \t");
            ldf.defines.emplace_back(std::string(rest.substr(0, space)), space == std::string_view::npos
                                                                             ? std::string()
                                                                             : std::string(Trim(rest.substr(space))));
            continue;
        }
        statements.append(line);
        statements.push_back('\n');
    }
    for (const auto part : SplitOutsideStrings(statements, ';'))
    {
        const auto statement = Trim(part);
        if (statement.empty())
        {
            continue;
        }
        if (auto function = FunctionOf(statement))
        {
            ldf.functions.push_back(std::move(*function));
        }
        else if (auto constant = ConstantOf(statement))
        {
            ldf.constants.push_back(std::move(*constant));
        }
    }
    return ldf;
}

} // namespace dao::core::ldf
