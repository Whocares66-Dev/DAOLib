// The game's files as the game itself finds them: a resource by its file
// name, looked up by the engine's resource system wherever it looks for
// its own (the override folders, add-ins, patches and the game's archives,
// in its own order), so a plugin reads what is installed, a mod's version
// over the game's.
//
//   const auto script = dao::resources::Read(L"rules_core.ncs");
//   if (script) { /* the compiled script the game runs */ }
//
// Read 2026-10-02 (dao/Addresses.h, "A resource's bytes"): what the Flash
// interface's movie file does to open a movie, Resources::Find with the
// name and the mode it asks with, then the resource's Data and Size, and
// Release. Seen in play 2026-10-02 by the turn-based mod: rules_core.ncs
// read as 251,960 bytes, the size of the shipped one in
// designerscripts.rim.
#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace dao::resources
{

// A copy of the resource's bytes; nothing if the game has no file by this
// name (its extension included: L"rules_core.ncs", L"turnorder.gfx") or
// it could not be read, which may put a line in the game's own log. From
// the game's thread, once the game is up.
std::optional<std::vector<std::byte>> Read(std::wstring_view file);

// Whether the game has a file by this name.
bool Exists(std::wstring_view file);

} // namespace dao::resources
