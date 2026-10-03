// What a crash log reads as text among the words copied off a thread's
// stack (src/loader/CrashLog.cpp): wide strings, the engine's std::wstring
// lying in a frame, and the start of a GFF4 file. Over copies, never the
// process: the log copies what it reads first.
//
// Written 2026-10-02 for a crash in the engine's parse of a model
// (DAOLib issue #3): the file's buffer was on the stack, and the model's
// name was in an engine std::wstring in a frame beyond the 128 words the log
// shows, so neither said which model it was.
#pragma once

#include "dao/core/Msvc8.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace dao::core
{

// Printable ASCII text in UTF-16 units, as the engine's wide strings hold
// names: the units before the first NUL, if there is one in `units`, there
// are at least `least` of them, and all are printable. Nothing otherwise.
std::optional<std::string> PrintableWide(std::span<const wchar_t> units, std::size_t least = 4);

// An engine std::wstring (WStringRep, 7 words) lying in the stack at
// words[0]: its capacity at least the inline 7 and its length at most that,
// at least three characters, and, when inline, printable with its NUL where
// the length says.
struct StackWString
{
    std::uint32_t size = 0;
    std::string text;          // the text, when it is inline
    std::uint32_t pointer = 0; // where the text is, when it is not
};
std::optional<StackWString> WStringIn(std::span<const std::uint32_t, 7> words);

// Whether bytes begin a GFF4 file: "GFF V4.0" or "GFF V4.1".
bool IsGff4(std::span<const std::byte> head);

} // namespace dao::core
