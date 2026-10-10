// Reads the game's files by name, found the same way the game finds its own: in the override
// folders, add-ins, patches and archives, in the game's order of precedence. Use it to read what
// is actually installed, so that a mod's version of a file wins over the original.
//
//   const auto script = dao::resources::Read(L"rules_core.ncs");
//   if (script) { /* the compiled script the game runs */ }
#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace dao::resources
{

// Returns a copy of a file's bytes.
// @param file the file name with its extension, such as L"rules_core.ncs"
// @return nullopt if the game has no file by that name or cannot read it. The game may write a
// line about it to its own log.
//
// Call it on the game thread, once the game has started.
std::optional<std::vector<std::byte>> Read(std::wstring_view file);

// Returns whether the game has a file by this name, found the same way as Read finds it. It
// loads the file to find out, so call it on the game thread too.
bool Exists(std::wstring_view file);

} // namespace dao::resources
