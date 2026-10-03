// The class of an object of the game's, from MSVC's RTTI at runtime: the
// slot before a vtable holds its complete object locator, which points at
// the type descriptor, whose decorated name follows two pointers. So any
// object with a vtable says what it is, which is how a probe tells a
// message, a game mode or a creature apart before its layout is known.
#pragma once

#include <string>
#include <string_view>

namespace dao
{

// The decorated class name (".?AVFoo@Bar@@") of the vtable an object's
// first word points at, or empty if the object, its vtable or the RTTI
// chain lies outside the exe's image: an object with no vtable, or not an
// object at all.
std::string_view DecoratedTypeName(const void *object);

// The same, undecorated: "Bar::Foo".
std::string TypeName(const void *object);

// The start of the object a base class's part belongs to: a pointer the
// engine hands about may be to a base inside the object, at an offset the
// vtable's complete object locator records (its second word). A creature's
// effects are aged with a pointer to its +0x70 (its vftable there, seen by
// tools/daoexe.py, being the one at offset 112 in every object class but
// the area's, 204), so its id is not at that pointer's +0x10 (dev/
// LIBRARY.md, found 2026-10-02 by the turn-based mod's own clocks).
// Null if the object, its vtable or the locator lies outside the image.
const void *CompleteObject(const void *object);

} // namespace dao
