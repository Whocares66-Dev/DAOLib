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

} // namespace dao
