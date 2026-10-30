// The game's own string type, for passing text to game functions that take a std::wstring and for
// reading text back from functions that fill one in. If you only need text, use std::wstring
// with PopString and PushString from dao/Script.h, which use this type for you.
#pragma once

#include "dao/core/Msvc8.h"

#include <string>
#include <string_view>

namespace dao
{

// A std::wstring with the memory layout the game uses (MSVC 2005's, see dao/core/Msvc8.h). The
// game's own functions fill it in and free it, because text longer than seven characters is
// stored on the game's heap.
//
// It calls game code, so use it only in a build the library knows (Module::IsKnownExecutable).
class WString
{
  public:
    // Makes an empty string.
    WString() = default;
    // Makes a string holding a copy of `text`.
    explicit WString(std::wstring_view text);
    // Frees the text through the game's own code.
    ~WString();

    WString(const WString &) = delete;
    WString &operator=(const WString &) = delete;

    // Replaces the text with a copy of `text`, using the game's own operator=.
    void Assign(std::wstring_view text);

    // Returns the text. The view is valid until the string is changed or destroyed.
    std::wstring_view View() const
    {
        return rep_.View();
    }

    // Returns the string as the game's std::wstring, to pass to a game function that takes or
    // fills one in.
    core::WStringRep *Engine()
    {
        return &rep_;
    }

  private:
    core::WStringRep rep_;
};
static_assert(sizeof(WString) == sizeof(core::WStringRep), "a WString is the engine's string, nothing more");

// Converts UTF-16 text, which the game uses, to UTF-8, for example for a log.
std::string Utf8(std::wstring_view text);

// Converts UTF-8 text, a Flash movie's for one, to UTF-16.
std::wstring Wide(std::string_view text);

} // namespace dao
