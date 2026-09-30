// The game's text by string reference, from the talk tables the engine
// has loaded (the game's, its DLC's and add-ins'), in the player's
// language: what a data table's NameStrRef or a script's strref names.
//
//   const std::wstring name = dao::tlk::String(379897); // "Shield Pummel"
//
// Read 2026-09-30 from the script command GetTlkTableString: the engine's
// CTlkTable (the object in 0x00C1D08C) answers a string reference with its
// text through GetString, __thiscall (strref, std::wstring &out, flag),
// `ret 0xC`, choosing the male or female form by its +0x1C. tools/tlk.py
// reads the same tables from the files. Not yet seen in play.
#pragma once

#include <cstdint>
#include <string>

namespace dao::tlk
{

// The text of a string reference, the female form if asked and there is
// one; empty for an unknown reference or before the tables are loaded.
std::wstring String(std::uint32_t strref, bool female = false);

} // namespace dao::tlk
