// The engine's string, held by a plugin: its std::wstring (MSVC 2005's,
// dao/core/Msvc8.h), in its layout, filled and freed by the engine's own
// functions, since a string longer than seven characters lives on the
// engine's heap. What a plugin passes to engine code that takes a string,
// and gets back from code that fills one.
//
// A plugin that only wants text uses std::wstring and dao/Script.h's
// PopString/PushString, which go through this.
#pragma once

#include "dao/core/Msvc8.h"

#include <string>
#include <string_view>

namespace dao
{

class WString
{
  public:
    WString() = default;
    explicit WString(std::wstring_view text);
    ~WString();

    WString(const WString &) = delete;
    WString &operator=(const WString &) = delete;

    // The engine's operator=: a copy of the text, on the engine's heap if
    // it is longer than the inline buffer.
    void Assign(std::wstring_view text);

    std::wstring_view View() const
    {
        return rep_.View();
    }

    // For engine functions that take or fill a std::wstring.
    core::WStringRep *Engine()
    {
        return &rep_;
    }

  private:
    core::WStringRep rep_;
};
static_assert(sizeof(WString) == sizeof(core::WStringRep), "a WString is the engine's string, nothing more");

// UTF-8, for logs: the engine's text is UTF-16.
std::string Utf8(std::wstring_view text);

} // namespace dao
