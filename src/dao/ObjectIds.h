// The ids the game gives its objects, chosen by a plugin: a creature made under an id the caller
// names, and where the ids of new objects start. Two games loaded from one save number their new
// objects from the same id and drift apart from there; with these, one game can make a creature
// under the id another gave it.
//
//   using namespace dao;
//   objects::NumberNewFrom(0x20000000);            // this game's own new objects, out of the way
//   const ObjectId made = objects::CreateAs(theirs, script::OBJECT_TYPE_CREATURE, tmpl, where);
//   if (made != theirs) { /* an object here has that id already, or nothing was made */ }
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace dao::objects
{

// The least and the most id a new object gets.
inline constexpr ObjectId kLeastNewId = 1;
inline constexpr ObjectId kMostNewId = 0x3FFFFFFF;

// Creates a creature or a placeable from a template, as the scripts' CreateObject does, under an
// id of the caller's choosing.
// @param id the id to give it, from kLeastNewId to kMostNewId
// @param type OBJECT_TYPE_CREATURE or OBJECT_TYPE_PLACEABLE
// @param overrideScript, spawnActive, noPermanentDeath as CreateObject's arguments
// @return the id the object got: `id`, or another if an object already has `id`, or
//  kInvalidObject if nothing was made
//
// Everything else is CreateObject's: the object's items and equipment are made with it and get
// ids of the game's own choosing, and its scripts run as for any other. A template that has a
// pool (the scripts' CreatePool) gives one of the pool's creatures, which keeps the id it has.
// Any intercept of CreateObject sees this call too. If `id` is above every id given so far, new
// objects are numbered on from it.
//
// Call it on the game thread, in a loaded game.
ObjectId CreateAs(ObjectId id, std::int32_t type, const Resource &tmpl, const CScriptLocation &location,
                  std::wstring_view overrideScript = L"", bool spawnActive = true, bool noPermanentDeath = false);

// Returns the id the last new object got. The next gets one more.
// @return nullopt before a game is loaded, or in a build of the game the library does not know
//
// A save keeps this number, so two games loaded from one save start from the same id.
std::optional<ObjectId> LastNewId();

// Makes the game number its new objects from `first` on, so that the ids below stay free.
// @param first above LastNewId, and at most kMostNewId
// @return false, with nothing changed, if `first` is not in that range or LastNewId is nullopt
//
// The number only goes up, and a save made after keeps it. Call it on the game thread.
bool NumberNewFrom(ObjectId first);

} // namespace dao::objects
