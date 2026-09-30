#include "dao/core/Exceptions.h"

#include <array>
#include <utility>

namespace dao::core
{

std::string_view ExceptionName(std::uint32_t code)
{
    // The codes of winnt.h's EXCEPTION_* and STATUS_* a game can die of.
    static constexpr std::array<std::pair<std::uint32_t, std::string_view>, 22> kNames{{
        {0xC0000005, "EXCEPTION_ACCESS_VIOLATION"},     {0xC0000006, "EXCEPTION_IN_PAGE_ERROR"},
        {0xC000001D, "EXCEPTION_ILLEGAL_INSTRUCTION"},  {0xC0000025, "EXCEPTION_NONCONTINUABLE_EXCEPTION"},
        {0xC0000026, "EXCEPTION_INVALID_DISPOSITION"},  {0xC000008C, "EXCEPTION_ARRAY_BOUNDS_EXCEEDED"},
        {0xC000008D, "EXCEPTION_FLT_DENORMAL_OPERAND"}, {0xC000008E, "EXCEPTION_FLT_DIVIDE_BY_ZERO"},
        {0xC000008F, "EXCEPTION_FLT_INEXACT_RESULT"},   {0xC0000090, "EXCEPTION_FLT_INVALID_OPERATION"},
        {0xC0000091, "EXCEPTION_FLT_OVERFLOW"},         {0xC0000092, "EXCEPTION_FLT_STACK_CHECK"},
        {0xC0000093, "EXCEPTION_FLT_UNDERFLOW"},        {0xC0000094, "EXCEPTION_INT_DIVIDE_BY_ZERO"},
        {0xC0000095, "EXCEPTION_INT_OVERFLOW"},         {0xC0000096, "EXCEPTION_PRIV_INSTRUCTION"},
        {0xC00000FD, "EXCEPTION_STACK_OVERFLOW"},       {0xC0000374, "STATUS_HEAP_CORRUPTION"},
        {0xC0000409, "STATUS_STACK_BUFFER_OVERRUN"},    {0x80000001, "EXCEPTION_GUARD_PAGE"},
        {0x80000003, "EXCEPTION_BREAKPOINT"},           {0xE06D7363, "C++ exception (throw)"},
    }};
    for (const auto &[value, name] : kNames)
    {
        if (value == code)
        {
            return name;
        }
    }
    return {};
}

std::string_view AccessKind(std::uint32_t operation)
{
    switch (operation)
    {
    case 0:
        return "reading";
    case 1:
        return "writing";
    case 8:
        return "executing";
    default:
        return "accessing";
    }
}

} // namespace dao::core
