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
#include <string_view>

namespace dao::tlk
{

// The text of a string reference, the female form if asked and there is
// one; empty for an unknown reference or before the tables are loaded.
std::wstring String(std::uint32_t strref, bool female = false);

// Text with the game's tokens resolved, as its screens show it: a
// conversation's reply "<FirstName/>, wait!" as "Lyna, wait!", the keys a
// tutorial names as the player has bound them. The talk tables' markup comes
// back as the Flash HTML the movies show (<emp> as <b>, <desc> as <i>(); a
// token the game does not know comes back as it was.
//
//   dao::tlk::Resolve(dao::tlk::String(strref))
//
// Read 2026-10-03 (dao/Addresses.h, ReplaceDesignerTags): the engine's one
// replacer, which every movie asks for its bound text. The hero's names
// come from the party's object, so before a game is loaded the name tokens
// stay as they are. The text as given before the game is up. On the game's
// thread. Seen in play 2026-10-03 through the Eluvian server, which passes
// every conversation line and reply through it: the Quartermaster's lines
// read clean, no stray markup; and a reply's <FirstName/> came back as the
// hero's name.
std::wstring Resolve(std::wstring_view text);

} // namespace dao::tlk
