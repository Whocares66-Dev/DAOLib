// Reads the class name of a game object from the run-time type information (RTTI) that MSVC
// stores alongside each vtable. Use it to tell what an object is, such as a message, a game mode
// or a creature, before its layout is known, and to find the whole object from a pointer to one
// of its base classes.
#pragma once

#include <string>
#include <string_view>

namespace dao
{

// Returns the decorated class name of an object, such as ".?AVFoo@Bar@@".
// @return an empty view if the pointer is null, or if the object, its vtable or its type
// information lies outside the exe's image, which means the object has no vtable or is not an
// object at all
//
// It is safe to call on any pointer, even one to freed memory: every read is checked.
std::string_view DecoratedTypeName(const void *object);

// Returns the undecorated class name of an object, such as "Bar::Foo". A template's name is left
// decorated rather than guessed at.
// @return an empty string in the same cases as DecoratedTypeName
std::string TypeName(const void *object);

// Returns the start of the whole object, given a pointer to one of its base classes, using the
// offset stored in the vtable's type information. A pointer the game passes around can point at a
// base class inside a larger object, and then the object's fields are not where the pointer
// points.
// @return null if the pointer is null, or the object's vtable or its type information lies
// outside the exe's image
const void *CompleteObject(const void *object);

} // namespace dao
