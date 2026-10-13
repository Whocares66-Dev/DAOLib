// The ids the game gives its objects, chosen by a plugin: a creature made under an id the caller
// names, where the ids of new objects start, and the ids an area's objects get as it is built.
// Two games loaded from one save number their new objects from the same id and drift apart from
// there; with these, one game can give its objects the ids another gave them.
//
//   using namespace dao;
//   objects::NumberNewFrom(0x20000000);            // this game's own new objects, out of the way
//   const ObjectId made = objects::CreateAs(theirs, script::OBJECT_TYPE_CREATURE, tmpl, where);
//   if (made != theirs) { /* an object here has that id already, or nothing was made */ }
//   objects::OnAreasBuild([](ObjectId) { return TheirNumberingForThisBuild(); });
#pragma once

#include "dao/ObjectId.h"
#include "dao/Structures.h"

#include <cstdint>
#include <functional>
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

// Creates an item on a creature or a placeable from a template, as the scripts'
// CreateItemOnObject does, under an id of the caller's choosing.
// @param id the id to give it, from kLeastNewId to kMostNewId
// @param target the creature or placeable whose inventory it goes into
// @param stackSize, tag, suppressNotification, droppable as CreateItemOnObject's arguments
// @return the id of the item made: `id`, or another if an object already has `id`, or
//  kInvalidObject if nothing was made
//
// The first item the command makes takes the id. A stack larger than one item of the template
// holds is made as several items, and the rest get ids of the game's own choosing. Where
// `target` already has a stack the item can join, that stack grows by the amount and keeps its
// own id; the item made is then not in the inventory, though its id is what is returned. Any
// intercept of CreateItemOnObject sees this call too. If `id` is above every id given so far,
// new objects are numbered on from it.
//
// Call it on the game thread, in a loaded game.
ObjectId CreateItemAs(ObjectId id, const Resource &tmpl, ObjectId target, std::int32_t stackSize = 1,
                      std::wstring_view tag = L"", bool suppressNotification = false, bool droppable = true);

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

// What a build of a list's areas numbered.
struct AreasBuilt
{
    ObjectId last = kInvalidObject; // the id its new objects were numbered on from
    std::uint32_t objects = 0;      // how many new objects it made
    std::uint32_t refused = 0;      // of those, how many got another id than the numbering's
};

// Follows the game as it builds the areas of a list it has not in memory, and lets a plugin say
// what ids the new objects of a build get.
// @param starting runs as a build begins, with LastNewId at that moment. It returns the id to
//  number the build's new objects on from, or nullopt to leave them to the game's own numbering
// @param built runs when the build is done, with what it numbered
//
// An area's creatures, placeables and items are made as it is built, and in an area the save has
// never visited every one is new and takes the next id. Two games loaded from one save make the
// same objects in the same order, so a game that numbers a build on from where another's
// numbering stood gives them the same ids. Each id is asked for as a save's object asks for its
// own: one that an object already has is not given twice. That object gets an id of the game's
// own numbering, and `refused` counts it. The game's own numbering stays where it is unless the
// ids given go past it, and then it goes on from the last of them.
//
// A save's load is a build too, makes new objects of its own, and by then numbers from the save's
// counter, so `starting` is how a game keeps a load's new objects clear of ids. What an area's scripts make
// once it is built (CreateObject, CreatePool) is not part of the build, and is numbered by the
// game as ever.
//
// Call it from DAOLib_Load. The callbacks run on the game thread, in the middle of a loading
// screen: keep them short. If several return an id, the first added is followed.
void OnAreasBuild(std::function<std::optional<ObjectId>(ObjectId last)> starting,
                  std::function<void(const AreasBuilt &)> built = {});

} // namespace dao::objects
