// Turns string references into the game's text. A string reference (strref) is the number that
// data tables and scripts use in place of text, for example an ability's NameStrRef. Lookups use
// the talk tables the game has loaded (its own, its DLC's and add-ins') in the player's language,
// and Resolve fills in the game's tokens.
//
//   const std::wstring name = dao::tlk::String(379897); // "Shield Pummel"
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace dao::tlk
{

// Returns the text of a string reference, as the game shows it now: the female form where the
// hero is female and the language has one.
// @param strref the string reference, as a data table or a script holds it
// @return the text, or an empty string for an unknown reference or before the talk tables load
std::wstring String(std::uint32_t strref);

// Replaces the game's tokens in a text, the same way the game's screens do.
// For example, a reply's "<FirstName/>, wait!" becomes "Lyna, wait!", and a tutorial's key tokens
// become the keys the player has bound. The talk tables' markup becomes the Flash HTML the movies
// show: `<emp>` becomes `<b>` and `<desc>` becomes `<i>`. Tokens the game does not know are left
// as they are.
//
//   dao::tlk::Resolve(dao::tlk::String(strref))
//
// @return the resolved text, or the text unchanged before the game is up
//
// Call it on the game thread. Until a game is loaded, the hero's name tokens are left in place.
std::wstring Resolve(std::wstring_view text);

} // namespace dao::tlk
